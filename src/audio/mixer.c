/**
 * @file mixer.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief RSP Audio mixer 
 * @ingroup mixer
 */

#include "mixer.h"
#include "mixer_internal.h"
#include "regsinternal.h"
#include "utils.h"
#include "rsp.h"
#include "rspq.h"
#include "debug.h"
#include "samplebuffer.h"
#include "audio.h"
#include "n64sys.h"
#include "interrupt.h"
#include "accounting_internal.h"
#include "../rspq/rspq_internal.h"
#include <memory.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>

/** @brief Set to 1 to activate debug logs */
#define MIXER_TRACE   0

#if MIXER_TRACE
/** @brief like debugf(), but writes only if #MIXER_TRACE is not 0 */
#define tracef(fmt, ...)  debugf(fmt, ##__VA_ARGS__)
#else
/** @brief like debugf(), but writes only if #MIXER_TRACE is not 0 */
#define tracef(fmt, ...)  ({ })
#endif

/**
 * @name AI Status Register Values
 * @{
 */
/** @brief Bit representing that the AI is busy */
#define AI_STATUS_BUSY  ( 1 << 30 )
/** @brief Bit representing that the AI is full */
#define AI_STATUS_FULL  ( 1 << 31 )
/** @} */

/** @brief Maximum number of mixer events */
#define MAX_EVENTS              32
/** @brief Number of expected #mixer_poll calls per second 
 *
 * This is used to allocate memory for the sample buffers
 * according to the expected number of samples that must
 * be calculated and held in memory.
 */
#define MIXER_POLL_PER_SECOND   8

/**
 * RSP mixer ucode (rsp_mixer.S)
 */
DEFINE_RSP_UCODE(rsp_mixer);

/** @brief Size of the ucode state that is automatically persisted by rspq.
 * Layout must match RSPQ_BeginSavedState in rsp_mixer.S:
 *   XVOL_L[32] + XVOL_R[32] (128 bytes) + COUNTER_RDRAM + pad (16 bytes).
 * ACCUM lives in .bss (not saved state) so the VADPCM bssovl1 bank still fits.
 */
#define MIXER_STATE_SIZE 144

/** @brief Max output samples per mix round (must match rsp_mixer.S). */
#define MIXER_MAX_SAMPLES_PER_ROUND  256

// NOTE: keep these in sync with rsp_mixer.S
#define CH_FLAGS_BPS_SHIFT  	(3<<0)   ///< BPS shift value
#define CH_FLAGS_16BIT      	(1<<2)   ///< Set if the channel is 16 bit
#define CH_FLAGS_STEREO     	(1<<3)   ///< Set if the channel is stereo (left)
#define CH_FLAGS_STEREO_SUB 	(1<<4)   ///< The channel is the second half of a stereo (right)
#define CH_FLAGS_VADPCM     	(1<<5)   ///< In-mixer VADPCM mono (wire + CPU)
#define CH_FLAGS_FORCE_MONO  	(1<<6)   ///< Fold this channel's output to both buses (mono downmix). CPU-side only; RSP ucode ignores this bit.
#define CH_FLAGS_CLEAR_ACCUM 	(1<<7)   ///< Zero ACCUM before mixing (first MIX_CHANNEL of a round)
#define CH_FLAGS_STEREO_ALLOC	(1<<9)   ///< The channel has a buffer sized for stereo (CPU-side only)

/** @brief rspq command IDs for rsp_mixer.S */
#define MIXER_CMD_CHANNEL     0x0
#define MIXER_CMD_VADPCM      0x1
#define MIXER_CMD_SETSTATE    0x2
#define MIXER_CMD_MEMMOVE     0x3
#define MIXER_CMD_FLUSH       0x4

/// @brief Fixed point value used in waveform position calculations.
/// This is a signed 64-bit integer with the fractional part using
/// #MIXER_FX64_FRAC bits. You can use #MIXER_FX64 to convert from float.
typedef uint64_t mixer_fx64_t;

/// @brief Fixed point value used for volume and panning calculations.
/// You can use #MIXER_FX15 to convert from float.
typedef int16_t mixer_fx15_t;

/// Number of fractional bits in #mixer_fx64_t
#define MIXER_FX64_FRAC    12    // NOTE: this must be the same of WAVERFORM_POS_FRAC_BITS in rsp_mixer.S
/// Convert a floating point value to #mixer_fx64_t
#define MIXER_FX64(f)      (int64_t)((f) * (1<<MIXER_FX64_FRAC))

/// Number of fractional bits in #mixer_fx15_t
#define MIXER_FX15_FRAC    15
/// Convert a floating point value to #mixer_fx15_t
#define MIXER_FX15(f)      (int16_t)((f) * ((1<<MIXER_FX15_FRAC)-1))

/// @brief Fixed point 16.16 value, used for the global volume.
/// You can use #MIXER_FX16 to convert from float.
typedef int32_t mixer_fx16_t;

/// Number of fractional bits for a fixed 16.16 value
#define MIXER_FX16_FRAC    16
/// Convert a floating point value to a fixed 16.16 value
#define MIXER_FX16(f)      (mixer_fx16_t)((f) * (1<<MIXER_FX16_FRAC))

/** @brief Mixer channel state - CPU side */
typedef struct mixer_channel_s {
	mixer_fx64_t pos;      ///< Position (bytes for PCM, samples for VADPCM)
	mixer_fx64_t step;     ///< Step per output sample (same units as pos)
	mixer_fx64_t len;      ///< Waveform length (same units as pos)
	mixer_fx64_t loop_len; ///< Loop length (same units as pos)
	void *ptr;             ///< Waveform data base (PCM samples or VADPCM frames)
	void *codec_state;     ///< Per-channel codec state (VADPCM decoder state)
	void *codebook;        ///< VADPCM codebook (NULL for PCM)
	void *loop_state;      ///< VADPCM state at loop start (NULL if none)
	uint32_t flags;        ///< Misc flags (see CH_FLAGS_*)
	waveform_t *wave;      ///< Waveform being played back on this channel
} mixer_channel_t;

/** @brief RDRAM completion counter published by the mixer ucode */
typedef struct {
	uint32_t rounds_done;
	uint32_t pad[3];
} mixer_rdram_state_t;

/** @brief Overlay saved-state layout (must match rsp_mixer.S) */
typedef struct {
	uint16_t xvol_l[MIXER_MAX_CHANNELS];
	uint16_t xvol_r[MIXER_MAX_CHANNELS];
	uint32_t counter_rdram;
	uint32_t pad[3];
} mixer_overlay_state_t;

_Static_assert(sizeof(mixer_overlay_state_t) == MIXER_STATE_SIZE, "mixer overlay state size mismatch");

/** @brief Configured limits of a mixer channel.
 *
 * This structure describes the playback limits for a mixer channel. The limits
 * are used to avoid over-allocating memory via sample buffers.
 */
typedef struct {
	int max_bits;           ///< Maximum number of bits per channel
	float max_frequency;    ///< Maximum frequency
	int max_buf_sz;         ///< Maximum sample buffer size (bytes)
} channel_limit_t;

/** @brief A mixer event (synchronized with sample playback) */
typedef struct {
	int64_t ticks;          ///< Absolute time at which the event will trigger (ticks = output samples)
	MixerEvent cb;          ///< Callback for the event
	void *ctx;              ///< Opaque context pointer to pass to the callback
} mixer_event_t;

static struct {
	uint32_t sample_rate;
	int num_channels;
	float vol;
	float max_samples;
	bool throttled;
	uint32_t uuid_counter;

	int64_t ticks;
	int num_events;
	mixer_event_t events[MAX_EVENTS];

	samplebuffer_t ch_buf[MIXER_MAX_CHANNELS];
	channel_limit_t limits[MIXER_MAX_CHANNELS];

	mixer_channel_t channels[MIXER_MAX_CHANNELS];
	mixer_fx15_t lvol[MIXER_MAX_CHANNELS];
	mixer_fx15_t rvol[MIXER_MAX_CHANNELS];

	volatile mixer_rdram_state_t *rstate;
	uint32_t round_next;
	uint32_t round_enqueued;

} Mixer;

/** @brief Count of ticks spent waiting on mixer rounds (rare fallback path). */
int64_t __mixer_profile_rsp = 0;

uint32_t __mixer_overlay_id;

static inline int mixer_initialized(void) { return Mixer.num_channels != 0; }

bool __mixer_round_done(uint32_t round_id) {
	// Before mixer_init (e.g. wav64 preload), nothing is in flight.
	if (!Mixer.rstate)
		return true;
	return (int32_t)(Mixer.rstate->rounds_done - round_id) >= 0;
}

void __mixer_round_wait(uint32_t round_id) {
	if (__mixer_round_done(round_id))
		return;
	// Waiting on a round whose mix command is not in the queue yet would
	// deadlock: the counter is published by the mix command itself.
	assertf((int32_t)(round_id - Mixer.round_enqueued) <= 0,
		"mixer: wait on round %lx not yet enqueued (last enqueued: %lx)",
		round_id, Mixer.round_enqueued);
	rspq_flush();
	uint32_t t0 = TICKS_READ();
	ACCT_SCOPE(ACCT_CAT_RSP) RSP_WAIT_LOOP(500) {
		if (__mixer_round_done(round_id))
			break;
	}
	__mixer_profile_rsp += TICKS_READ() - t0;
}

uint32_t __mixer_round_producer(void) {
	// ID of the mix round that will publish completion of highpri commands
	// enqueued right now: the round being prepared (if inside mixer_exec),
	// or the next one that will be issued. Note that the latter may never be
	// issued: __mixer_dirty_wait handles that case by draining the queue.
	if ((int32_t)(Mixer.round_next - Mixer.round_enqueued) > 0)
		return Mixer.round_next;
	return Mixer.round_next + 1;
}

bool __mixer_memmove_async(void *dst, void *src, int len) {
	if (!mixer_initialized())
		return false;
	assert(((uint32_t)dst & 7) == 0 && ((uint32_t)src & 7) == 0 && (len & 7) == 0);
	bool highpri = rspq_in_highpri();
	if (!highpri) rspq_highpri_begin();
	rspq_write(__mixer_overlay_id, MIXER_CMD_MEMMOVE,
		PhysicalAddr(dst), PhysicalAddr(src), len);
	if (!highpri) rspq_highpri_end();
	return true;
}

void __mixer_dirty_wait(uint32_t round_id) {
	if (__mixer_round_done(round_id))
		return;
	if ((int32_t)(round_id - Mixer.round_enqueued) > 0) {
		// The round that owns the dirty tail is the one currently being
		// prepared: its mix command (which publishes the counter) is not in
		// the queue yet, so waiting on the counter would deadlock. The
		// producer commands (eg: VADPCM decodes) are already enqueued
		// though, so drain the highpri queue instead. This happens only at
		// a streamed-loop wrap, when waveform_read chains a second decode
		// right after the undo, within the same mix round.
		uint32_t t0 = TICKS_READ();
		bool highpri = rspq_in_highpri();
		if (highpri) rspq_highpri_end();
		rspq_highpri_sync();
		if (highpri) rspq_highpri_begin();
		__mixer_profile_rsp += TICKS_READ() - t0;
	} else {
		__mixer_round_wait(round_id);
	}
}

void __mixer_wait_idle(void) {
	if (!mixer_initialized())
		return;
	if (Mixer.round_enqueued)
		__mixer_round_wait(Mixer.round_enqueued);
	// Compaction memmoves can be enqueued after the last mix round (e.g. by
	// mixer_reclaim at frame start): the round counter does not cover them,
	// so drain the highpri queue before the caller frees/reuses memory.
	rspq_highpri_sync();
}

static void mixer_reclaim(void) {
	for (int i = 0; i < Mixer.num_channels; i++) {
		samplebuffer_t *sbuf = &Mixer.ch_buf[i];
		mixer_channel_t *ch = &Mixer.channels[i];
		if (!samplebuffer_is_inited(sbuf) || !ch->ptr)
			continue;
		// Fully-cachable loop: the loop must stay resident in the buffer
		// forever (the RSP plays it straight from the cache, and wrapped
		// positions jump backwards to the loop start). There is nothing to
		// reclaim: compaction (if any) is handled by the loop-align discard
		// in mixer_exec, which runs at most once per waveform.
		int bps_fx64 = (ch->flags & CH_FLAGS_BPS_SHIFT) + MIXER_FX64_FRAC;
		int loop_len = ch->loop_len >> bps_fx64;
		// VADPCM samplebuffers are in frames; loop_len above is in samples.
		if (ch->flags & CH_FLAGS_VADPCM)
			loop_len /= 16;
		if (loop_len && loop_len < sbuf->size)
			continue;
		if (!__mixer_round_done(sbuf->last_round))
			continue;
		if (sbuf->wdirty && __mixer_round_done(sbuf->dirty_round))
			sbuf->wdirty = 0;
		// Free consumed samples now so mid-frame appends rarely need to
		// compact while rounds are still in flight.
		samplebuffer_discard(sbuf, sbuf->wpos + sbuf->ridx);
	}
}

void mixer_init(int num_channels) {
	memset(&Mixer, 0, sizeof(Mixer));

	Mixer.num_channels = num_channels;
	Mixer.sample_rate = audio_get_frequency();  // actual sample rate obtained via DAC clock
	assertf(Mixer.sample_rate > 0, "audio_init() must be called before mixer_init()");
	Mixer.vol = 1.0f;

	for (int ch=0;ch<MIXER_MAX_CHANNELS;ch++) {
		mixer_ch_set_vol(ch, 1.0f, 1.0f);
		mixer_ch_set_limits(ch, 16, Mixer.sample_rate, 0);
	}

	Mixer.rstate = malloc_uncached(sizeof(*Mixer.rstate));
	assertf(Mixer.rstate, "Out of memory");
	memset((void*)Mixer.rstate, 0, sizeof(*Mixer.rstate));

	rspq_init();
	__mixer_overlay_id = rspq_overlay_register(&rsp_mixer);

	mixer_overlay_state_t *mixer_state = rspq_overlay_get_state(&rsp_mixer);
	memset(mixer_state, 0, sizeof(*mixer_state));
	mixer_state->counter_rdram = PhysicalAddr((void*)Mixer.rstate);
	data_cache_hit_writeback(mixer_state, sizeof(*mixer_state));
}

static int mixer_calc_buffer_size(int ch, waveform_t *wave)
{
	int64_t nsamples = Mixer.limits[ch].max_frequency;
	int64_t size;

	if (wave->format == WAVEFORM_FORMAT_VADPCM) {
		// Samplebuffer stores 9-byte frames (16 samples each).
		int64_t nframes = (int64_t)ceilf((float)nsamples / (float)MIXER_POLL_PER_SECOND);
		nframes = (nframes + 15) / 16;
		size = ROUND_UP(nframes * 9, 8);
	} else {
		nsamples *= Mixer.limits[ch].max_bits / 8;
		nsamples *= wave->channels;
		size = ROUND_UP((int64_t)ceilf((float)nsamples / (float)MIXER_POLL_PER_SECOND), 8);
	}

	if (Mixer.limits[ch].max_buf_sz && size > Mixer.limits[ch].max_buf_sz)
		size = Mixer.limits[ch].max_buf_sz;

	assert((size % 8) == 0);
	assert((int32_t)size == size);
	
	return size;
}

void mixer_set_vol(float vol) {
	Mixer.vol = vol;
}

void mixer_close(void) {
	assert(mixer_initialized());

	__mixer_wait_idle();

	rspq_overlay_unregister(__mixer_overlay_id);
	__mixer_overlay_id = 0;

	for (int i=0; i<Mixer.num_channels; i++)
	{
		if (samplebuffer_is_inited(&Mixer.ch_buf[i]))
			samplebuffer_close(&Mixer.ch_buf[i]);
	}

	if (Mixer.rstate) {
		free_uncached((void*)Mixer.rstate);
		Mixer.rstate = NULL;
	}

	Mixer.num_channels = 0;
}

void mixer_ch_set_freq(int ch, float frequency) {
	mixer_channel_t *c = &Mixer.channels[ch];
	assertf(!(c->flags & CH_FLAGS_STEREO_SUB), "cannot call on secondary stereo channel %d", ch);
	assertf(frequency >= 0, "cannot set negative frequency on channel %d: %f", ch, frequency);
	// Check if the frequency is within the configured limit. Allow for a 1% margin because of rounding errors
	// for default maximum frequency being the output sample rate converted from fixed point.
	assertf(frequency <= Mixer.limits[ch].max_frequency*1.01f, "frequency %.1f exceeds configured limit %.1f on channel %d; use mixer_ch_set_limit to change the limit for this channel", frequency, Mixer.limits[ch].max_frequency, ch);
	mixer_fx64_t step = MIXER_FX64(frequency / (float)Mixer.sample_rate);
	if (!(c->flags & CH_FLAGS_VADPCM))
		step <<= (c->flags & CH_FLAGS_BPS_SHIFT);
	c->step = step;
}

void mixer_ch_set_vol(int ch, float lvol, float rvol) {
	mixer_channel_t *c = &Mixer.channels[ch];
	assertf(!(c->flags & CH_FLAGS_STEREO_SUB), "mixer_ch_set_vol: cannot call on secondary stereo channel %d", ch);
	Mixer.lvol[ch] = MIXER_FX15(lvol);
	Mixer.rvol[ch] = MIXER_FX15(rvol);
}

void mixer_ch_set_vol_pan(int ch, float vol, float pan) {
	mixer_ch_set_vol(ch, vol * (1.f - pan), vol * pan);
}

void mixer_ch_set_force_mono(int ch, bool enable) {
	assert(ch < Mixer.num_channels);
	if (enable) Mixer.channels[ch].flags |=  CH_FLAGS_FORCE_MONO;
	else        Mixer.channels[ch].flags &= ~CH_FLAGS_FORCE_MONO;
}

bool mixer_ch_get_force_mono(int ch) {
	assert(ch < Mixer.num_channels);
	return (Mixer.channels[ch].flags & CH_FLAGS_FORCE_MONO) != 0;
}

void mixer_set_force_mono(bool enable) {
	for (int i = 0; i < Mixer.num_channels; i++) {
		mixer_ch_set_force_mono(i, enable);
	}
}

void mixer_ch_set_vol_dolby(int ch, float fl, float fr,
	float c, float sl, float sr) {

	/// @cond
	#define SQRT_05   0.7071067811865476f
	#define SQRT_075  0.8660254037844386f
	#define SQRT_025  0.5f

	#define KF        1.0f
	#define KC        SQRT_05
	#define KA        SQRT_075
	#define KB        SQRT_025

	#define KTOT      (KF+KC+KA+KB)
	#define KFn       (KF/KTOT)
	#define KCn       (KC/KTOT)
	#define KAn       (KA/KTOT)
	#define KBn       (KB/KTOT)
	/// @endcond

	mixer_ch_set_vol(ch,
		fl*KFn + c*KCn - sl*KAn - sr*KBn,
		fr*KFn + c*KCn + sl*KBn + sr*KAn
	);
}

// Given a position within a looping waveform, calculate its wrapped position
// in the range [0, len], according to loop definition.
// NOTE: this function should only be called on looping waveforms.
static int waveform_wrap_wpos(int wpos, int len, int loop_len) {
	assert(loop_len != 0);
	assert(wpos >= len);
	return ((wpos - len) % loop_len) + (len - loop_len);
}

// A wrapper for a waveform's read function that handles loops.
// Sample buffers are not aware of loops. The way the mixer handles
// loops is by unrolling them in the sample buffer: that is, the sample
// buffer is called with an unlimited growing wpos, and the
// WaveformRead callback is expected to unroll the loop as wpos
// grows. To alleviate all waveforms implementations to handle loop
// unrolling, this simple wrapper performs the wpos wrapping calculations
// and convert it in a sequence of calls to read callbacks using only positions
// in the range [0, len].
static void waveform_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
	waveform_t *wave = sbuf->wave;
	// Samplebuffer units: PCM samples, or VADPCM frames. Wave metadata is
	// always in samples; convert bounds when the buffer stores frames.
	int wave_len = wave->len;
	int wave_loop = wave->loop_len;
	if (wave->format == WAVEFORM_FORMAT_VADPCM) {
		wave_len /= 16;
		wave_loop /= 16;
	}
	int ub = samplebuffer_unit_bytes(sbuf);

	if (!wave_loop) {
		int len1 = wlen;
		if (wpos + wlen > wave_len)
			len1 = MAX(wave_len - wpos, 0);
		int len2 = wlen-len1;

		if (len1 > 0)
			wave->read(ctx, sbuf, wpos, len1, seeking);
		if (len2 > 0) {
			void *dest = samplebuffer_append(sbuf, len2);
			memset(dest, 0, len2 * ub);
		}
	} else {
		if (wpos >= wave_len)
			wpos = waveform_wrap_wpos(wpos, wave_len, wave_loop);

		if (wpos == wave_len - wave_loop)
			seeking = true;

		int len1 = wlen;
		if (wpos + wlen > wave_len)
			len1 = wave_len - wpos;
		int len2 = wlen-len1;

		int overread = (MIXER_LOOP_OVERREAD + ub - 1) / ub;
		assertf(len2 <= wave_loop + overread,
			"waveform %s: logic error: double loop in single read\n"
			"wpos:%x, wlen:%x, len:%x loop_len:%x",
			wave->name, wpos, wlen, wave_len, wave_loop);

		wave->read(ctx, sbuf, wpos, len1, seeking);

		while (len2 > 0) {
			int loop_start = wave_len - wave_loop;
			int ns = MIN(len2, wave_loop);
			wave->read(ctx, sbuf, loop_start, ns, true);
			len2 -= ns;
		}
	}
}

void mixer_ch_play(int ch, waveform_t *wave)
{
	assert(ch < Mixer.num_channels);
	samplebuffer_t *sbuf = &Mixer.ch_buf[ch];
	mixer_channel_t *c = &Mixer.channels[ch];
	assertf(!(c->flags & CH_FLAGS_STEREO_SUB), "mixer_ch_play: cannot call on secondary stereo channel %d", ch);

	// Initialize uuid for this waveform if it wasn't already
	if (wave->__uuid == 0) 
		wave->__uuid = ++Mixer.uuid_counter;

	// If we're going to play a stereo waveform on a channel that was allocated
	// for mono, we need to reallocate the buffer.
	if (wave->channels == 2 && !(c->flags & CH_FLAGS_STEREO_ALLOC)) {
		__mixer_wait_idle();
		samplebuffer_close(sbuf);
	}
	// Check if the state buffer is big enough, otherwise we need to reallocate
	if (sbuf->state_size < wave->state_size) {
		__mixer_wait_idle();
		samplebuffer_close(sbuf);
	}

	if (!samplebuffer_is_inited(sbuf)) {
		// If we have not yet allocated the memory for the sample buffers,
		// this is a good moment to do so, as we might need the configure
		// the samplebuffer in a moment.
		int size = ROUND_UP(mixer_calc_buffer_size(ch, wave), 16);
		int state_size = ROUND_UP(wave->state_size, 16);
		void *ptr = malloc_uncached(size + state_size);
		assertf(ptr, "Out of memory");
		samplebuffer_init(sbuf, ptr, size, state_size);
		if (wave->channels == 2) c->flags |= CH_FLAGS_STEREO_ALLOC;
	}

	// Configure the waveform on this channel, if we have not
	// already. This optimization is useful in case the caller
	// wants to play the same waveform on the same channel multiple
	// times, and the waveform has been already decoded and cached
	// in the sample buffer.
	// We compare:
	//  - The UUID of the waveform (not the wave pointer, as that
	//    could have been even recycled by the caller)
	//  - The context pointer: if that changes, probably the internal
	//    state of the callback is also different (eg: compression state),
	//    so the next (not already buffered) sample could cause
	//    an error because it'd be decompressed with the wrong state.
	if (wave->__uuid != sbuf->wave_uuid) {
		samplebuffer_flush(sbuf);

		// If this channel is playing something else, stop it
		if (mixer_ch_playing(ch))
			mixer_ch_stop(ch);

		// Configure the sample buffer for this waveform
		assert(wave->channels == 1 || wave->channels == 2);
		assert(wave->bits == 8 || wave->bits == 16);
		assertf(wave->format != WAVEFORM_FORMAT_VADPCM || wave->channels == 1,
			"waveform %s: in-mixer VADPCM requires mono", wave->name);
		if (wave->format == WAVEFORM_FORMAT_VADPCM) {
			samplebuffer_set_unit_bytes(sbuf, 9);
		} else {
			samplebuffer_set_bps(sbuf, wave->bits*wave->channels);
		}
		samplebuffer_set_waveform(sbuf, wave, wave->read ? waveform_read : NULL);

		// Configure the mixer channel structured used by the RSP ucode
		assertf(wave->len >= 0 && wave->len <= WAVEFORM_MAX_LEN, "waveform %s: invalid length %x", wave->name, wave->len);
		assertf(wave->len != WAVEFORM_UNKNOWN_LEN || wave->loop_len == 0, "waveform %s with unknown length cannot loop", wave->name);
		c->flags &= ~(CH_FLAGS_BPS_SHIFT | CH_FLAGS_16BIT | CH_FLAGS_STEREO | CH_FLAGS_VADPCM);
		c->codebook = NULL;
		c->loop_state = NULL;
		c->codec_state = sbuf->state;
		if (wave->format == WAVEFORM_FORMAT_VADPCM) {
			waveform_vadpcm_t *vc = wave->codec;
			assertf(vc && vc->codebook, "waveform %s: VADPCM missing codec/codebook", wave->name);
			c->flags |= CH_FLAGS_VADPCM | CH_FLAGS_16BIT;
			c->codebook = vc->codebook;
			c->loop_state = vc->loop_state;
			// Positions are in samples (no bps shift).
			c->len = MIXER_FX64((int64_t)wave->len);
			c->loop_len = MIXER_FX64((int64_t)wave->loop_len);
		} else {
			int bps = SAMPLES_BPS_SHIFT(sbuf);
			c->flags |= bps | (wave->channels == 2 ? CH_FLAGS_STEREO : 0) | (wave->bits == 16 ? CH_FLAGS_16BIT : 0);
			c->len = MIXER_FX64((int64_t)wave->len) << bps;
			c->loop_len = MIXER_FX64((int64_t)wave->loop_len) << bps;
		}
		mixer_ch_set_freq(ch, wave->frequency);

		// Invoke start callback if specified. This is only done when the
		// waveform is assigned to this channel, and not at the start of all
		// subsequent playbacks.
		if (wave->start)
			wave->start(wave->ctx, sbuf);

		tracef("mixer_ch_play[new]: ch=%d len=%llx loop_len=%llx wave=%s\n", ch, c->len >> (MIXER_FX64_FRAC+SAMPLES_BPS_SHIFT(sbuf)), c->loop_len >> (MIXER_FX64_FRAC+SAMPLES_BPS_SHIFT(sbuf)), wave->name);
	} else {
		tracef("mixer_ch_play[old]: ch=%d len=%llx loop_len=%llx wave=%s\n", ch, c->len >> (MIXER_FX64_FRAC+SAMPLES_BPS_SHIFT(sbuf)), c->loop_len >> (MIXER_FX64_FRAC+SAMPLES_BPS_SHIFT(sbuf)), wave->name);

		// There is a UUID match. There must also be a pointer match then.
		assertf(sbuf->wave == wave, "%s: uuid match (%ld) but pointer mismatch: %p != %p", wave->name, wave->__uuid, sbuf->wave, wave);
	}

	// Restart from the beginning of the waveform
	c->wave = wave;
	c->ptr = SAMPLES_PTR(sbuf);
	c->pos = 0;

	// If we start playing a stereo waveform, configure channel ch+1 as stereo sub,
	// to help catching errors where it is used as a separate channel.
	if (c->flags & CH_FLAGS_STEREO) {
		assertf(ch != Mixer.num_channels-1, "cannot configure last channel (%d) as stereo", ch);
		assertf(!mixer_ch_playing(ch+1), "cannot play stereo waveform on channel %d because channel %d is active", ch, ch+1);
		Mixer.channels[ch+1].flags |= CH_FLAGS_STEREO_SUB;
	} else if (ch != Mixer.num_channels-1) {
		Mixer.channels[ch+1].flags &= ~CH_FLAGS_STEREO_SUB;
	}
}

void mixer_ch_set_pos(int ch, double pos) {
	mixer_channel_t *c = &Mixer.channels[ch];
	assertf(!(c->flags & CH_FLAGS_STEREO_SUB), "mixer_ch_set_pos: cannot call on secondary stereo channel %d", ch);
	mixer_fx64_t p = MIXER_FX64(pos);
	if (!(c->flags & CH_FLAGS_VADPCM))
		p <<= (c->flags & CH_FLAGS_BPS_SHIFT);
	c->pos = p;
	tracef("mixer_ch_set_pos: ch=%d pos=%.32g(%llx)\n", ch, pos, c->pos);
}

double mixer_ch_get_pos(int ch) {
	mixer_channel_t *c = &Mixer.channels[ch];
	assertf(!(c->flags & CH_FLAGS_STEREO_SUB), "mixer_ch_get_pos: cannot call on secondary stereo channel %d", ch);
	uint64_t pos = c->pos;
	if (!(c->flags & CH_FLAGS_VADPCM))
		pos >>= (c->flags & CH_FLAGS_BPS_SHIFT);
	return (double)pos / (double)(1<<MIXER_FX64_FRAC);
}

void mixer_ch_stop(int ch) {
	mixer_channel_t *c = &Mixer.channels[ch];

	tracef("mixer_ch_stop: ch=%d\n", ch);

	if (c->flags & CH_FLAGS_STEREO)
		c[1].flags &= ~CH_FLAGS_STEREO_SUB;

	c->ptr = 0;
	c->pos = 0;

	// Invalidate the wave pointer, as it might become dangling
	// anyway, as the user can free the waveform memory at any time after stop.
	// Keep the uuid valid instead. This allows
	// for an optimization: if mixer_ch_play is called again on the same
	// waveform, we will realize that by the uuid, and reuse the same
	// samplebuffer contents.
	c->wave = NULL;
}

waveform_t *mixer_ch_playing_waveform(int ch) {
	mixer_channel_t *c = &Mixer.channels[ch];
	if (c->flags & CH_FLAGS_STEREO_SUB) {
		assert(ch > 0);
		c--;
	}
	return c->ptr != 0 ? c->wave : NULL;
}

bool mixer_ch_playing(int ch) {
	return mixer_ch_playing_waveform(ch) != NULL;
}

void mixer_ch_set_limits(int ch, int max_bits, float max_frequency, int max_buf_sz) {
	assert(max_bits == 0 || max_bits == 8 || max_bits == 16);
	assert(max_frequency >= 0);
	assert(max_buf_sz >= 0 && max_buf_sz % 8 == 0);
	assert(ch >= 0 && ch < MIXER_MAX_CHANNELS);
	assert(!mixer_ch_playing(ch));
	tracef("mixer_ch_set_limits: ch=%d bits=%d maxfreq:%.2f bufsz:%d\n", ch, max_bits, max_frequency, max_buf_sz);

	channel_limit_t newlimits = {
		.max_bits = max_bits ? max_bits : 16,
		.max_frequency = max_frequency ? max_frequency : Mixer.sample_rate,
		.max_buf_sz = max_buf_sz,
	};

	// No-op if the limits are unchanged: the sample buffer is sized from them,
	// so it's still valid and we keep it. Freeing it here (to reallocate lazily)
	// would churn the uncached buffer on every playback for callers that
	// re-assert the same limit, fragmenting the heap.
	if (newlimits.max_bits == Mixer.limits[ch].max_bits &&
	    newlimits.max_frequency == Mixer.limits[ch].max_frequency &&
	    newlimits.max_buf_sz == Mixer.limits[ch].max_buf_sz)
		return;

	Mixer.limits[ch] = newlimits;

	// Free the memory immediately, as it doesn't match the new limits anymore.
	// We will reallocate it later lazily if needed.
	if (samplebuffer_is_inited(&Mixer.ch_buf[ch])) {
		__mixer_wait_idle();
		samplebuffer_close(&Mixer.ch_buf[ch]);
		Mixer.channels[ch].flags &= ~CH_FLAGS_STEREO_ALLOC;
	}
}

/** @brief Apply global volume (FX16) to a channel volume (FX15). */
static inline mixer_fx15_t mixer_apply_gvol(mixer_fx15_t vol, mixer_fx16_t gvol_fx16) {
	int32_t v = (int32_t)(((int64_t)vol * gvol_fx16) >> 16);
	if (v > 0x7FFF) v = 0x7FFF;
	if (v < -0x8000) v = -0x8000;
	return (mixer_fx15_t)v;
}

/** @brief Emit a MIX_CHANNEL rspq command for one channel. */
static void mixer_emit_channel(int ch, uint32_t flags, mixer_fx15_t lvol, mixer_fx15_t rvol,
	uint32_t pos, uint32_t step, uint32_t len, uint32_t loop_len, void *ptr,
	int nsamples, int acc_offset, void *codebook, void *state, void *loop_state)
{
	rspq_write_t w = rspq_write_begin(__mixer_overlay_id, MIXER_CMD_CHANNEL, 12);
	// a0 payload (24 bits): ch_idx<<16 | flags<<8
	rspq_write_arg(&w, ((uint32_t)ch << 16) | ((flags & 0xFF) << 8));
	rspq_write_arg(&w, ((uint32_t)(uint16_t)lvol << 16) | (uint16_t)rvol);
	rspq_write_arg(&w, pos);
	rspq_write_arg(&w, step);
	rspq_write_arg(&w, len);
	rspq_write_arg(&w, loop_len);
	rspq_write_arg(&w, ptr ? PhysicalAddr(ptr) : 0);
	rspq_write_arg(&w, ((uint32_t)(uint16_t)acc_offset << 16) | (uint16_t)nsamples);
	rspq_write_arg(&w, codebook ? PhysicalAddr(codebook) : 0);
	rspq_write_arg(&w, state ? PhysicalAddr(state) : 0);
	rspq_write_arg(&w, loop_state ? PhysicalAddr(loop_state) : 0);
	rspq_write_arg(&w, 0);
	rspq_write_end(&w);
}

/** @brief Emit VADPCM_SetState (copy src→dst, or clear if src is NULL). */
static void mixer_emit_setstate(void *dst, void *src)
{
	rspq_write(__mixer_overlay_id, MIXER_CMD_SETSTATE,
		PhysicalAddr(dst), src ? PhysicalAddr(src) : 0);
}

static void mixer_exec(int32_t *out, int num_samples) {
	tracef("mixer_exec: 0x%x samples\n", num_samples);

	uint32_t fake_loop = 0;
	uint32_t last_round_id = Mixer.round_next + 1;

	// ---- Phase 1: ensure samplebuffers have data for the full span ----
	for (int i=0; i<Mixer.num_channels; i++) {
		samplebuffer_t *sbuf = &Mixer.ch_buf[i];
		mixer_channel_t *ch = &Mixer.channels[i];
		bool vadpcm = (ch->flags & CH_FLAGS_VADPCM) != 0;
		int bps = vadpcm ? 0 : (ch->flags & CH_FLAGS_BPS_SHIFT);
		int bps_fx64 = bps + MIXER_FX64_FRAC;
		int ub = samplebuffer_unit_bytes(sbuf);

		if (ch->ptr) {
			int len = ch->len >> bps_fx64;
			int loop_len = ch->loop_len >> bps_fx64;
			int wpos = ch->pos >> bps_fx64;
			int wlast = (ch->pos + ch->step*(num_samples-1)) >> bps_fx64;
			int wnext = (ch->pos + ch->step*num_samples) >> bps_fx64;
			int wlen = MAX(wlast-wpos+1, wnext-wpos);

			assertf(wlen >= 0, "channel %d: wpos overflow", i);
			tracef("ch:%d wpos:%x wlen:%x len:%x loop_len:%x sbuf_size:%x\n", i, wpos, wlen, len, loop_len, sbuf->size);

			// VADPCM samplebuffers are addressed in frames.
			int spos = wpos, slen = len, sloop = loop_len, swlen = wlen;
			if (vadpcm) {
				spos = wpos / 16;
				slen = len / 16;
				sloop = loop_len / 16;
				swlen = (wpos + wlen + 15) / 16 - spos;
			}

			if (!loop_len) {
				if (wpos >= len) {
					mixer_ch_stop(i);
					continue;
				}
				if (wpos+wlen > len)
					wlen = len-wpos;
				if (vadpcm) {
					swlen = (wpos + wlen + 15) / 16 - spos;
					swlen += (MIXER_LOOP_OVERREAD + ub - 1) / ub;
				} else {
					swlen = wlen + (MIXER_LOOP_OVERREAD >> bps);
				}
				assert(swlen >= 0);
			} else if ((vadpcm ? sloop : loop_len) < sbuf->size) {
				int loop_pos = len - loop_len;
				int sloop_pos = vadpcm ? loop_pos / 16 : loop_pos;
				if (sbuf->size < (vadpcm ? slen : len) && wpos >= loop_pos && sbuf->wpos != sloop_pos) {
					tracef("ch:%d discard to align loop wpos:%x loop_pos:%x\n", i, wpos, loop_pos);
					samplebuffer_discard(sbuf, sloop_pos);
				}
				while (wpos >= len)
					wpos -= loop_len;
				if (wpos+wlen > len)
					wlen = len-wpos;
				if (vadpcm) {
					spos = wpos / 16;
					swlen = (wpos + wlen + 15) / 16 - spos;
					swlen += (MIXER_LOOP_OVERREAD + ub - 1) / ub;
				} else {
					swlen = wlen + (MIXER_LOOP_OVERREAD >> bps);
				}
				assertf(swlen >= 0, "ch:%d wlen=%x wpos=%x len=%x\n", i, swlen, wpos, len);
			} else {
				int sbuf_len = vadpcm ? slen : len;
				int sbuf_loop = vadpcm ? sloop : loop_len;
				if (sbuf->wpos > sbuf_len && wpos > len) {
					tracef("mixer_poll: wrapping sample buffer loop: sbuf->wpos:%x len:%x\n", sbuf->wpos, sbuf_len);
					int wrap_pos = vadpcm ? wpos / 16 : wpos;
					samplebuffer_discard(sbuf, wrap_pos);
					sbuf->wpos = waveform_wrap_wpos(sbuf->wpos, sbuf_len, sbuf_loop);
					if (sbuf->wnext >= 0)
						sbuf->wnext = sbuf->wpos + sbuf->widx;
					int wpos2 = waveform_wrap_wpos(wpos, len, loop_len);
					ch->pos -= (int64_t)(wpos-wpos2) << bps_fx64;
					wpos = wpos2;
					if (vadpcm)
						spos = wpos / 16;
				}
				fake_loop |= 1<<i;
				if (vadpcm) {
					spos = wpos / 16;
					swlen = (wlen + 15) / 16 + (MIXER_LOOP_OVERREAD + ub - 1) / ub;
				} else {
					swlen = wlen;
				}
			}

			void* ptr = samplebuffer_get(sbuf, vadpcm ? spos : wpos, &swlen);
			assert(ptr);
			if (vadpcm)
				ch->ptr = (uint8_t*)ptr - spos * 9;
			else
				ch->ptr = (uint8_t*)ptr - (wpos<<bps);
		}
	}

	float gvol = Mixer.vol;
	uint32_t reset_time = exception_reset_time();
	if (reset_time) {
		const float FADE_OUT_TIME = (float)RESET_TIME_LENGTH / TICKS_PER_SECOND;
		float elapsed = (float)reset_time / TICKS_PER_SECOND;
		gvol *= (FADE_OUT_TIME - MIN(elapsed, FADE_OUT_TIME)) / FADE_OUT_TIME;
	}
	mixer_fx16_t gvol_fx16 = MIXER_FX16(gvol);

	// ---- Phase 2: emit per-channel mix rounds of <= MAX_SAMPLES_PER_ROUND ----
	for (int offset = 0; offset < num_samples; offset += MIXER_MAX_SAMPLES_PER_ROUND) {
		int ns = MIN(num_samples - offset, MIXER_MAX_SAMPLES_PER_ROUND);
		uint32_t round_id = ++Mixer.round_next;
		last_round_id = round_id;
		bool clear_accum = true;

		rspq_highpri_begin();

		for (int ch = 0; ch < Mixer.num_channels; ch++) {
			mixer_channel_t *c = &Mixer.channels[ch];
			mixer_fx15_t lvol, rvol;
			uint32_t flags = c->flags & (CH_FLAGS_BPS_SHIFT | CH_FLAGS_16BIT |
				CH_FLAGS_STEREO | CH_FLAGS_STEREO_SUB | CH_FLAGS_VADPCM);
			void *ptr = NULL;
			void *codebook = NULL, *state = NULL, *loop_state = NULL;
			uint32_t pos = 0, step = 0, len = 0xFFFFFFFF, loop_len = 0;
			int ns0 = ns, ns1 = 0;
			uint32_t pos1 = 0;

			if (c->flags & CH_FLAGS_STEREO_SUB) {
				mixer_channel_t *owner = &Mixer.channels[ch-1];
				if (!owner->ptr)
					continue;
				if (owner->flags & CH_FLAGS_FORCE_MONO) {
					mixer_fx15_t v = mixer_apply_gvol(Mixer.rvol[ch-1] >> 1, gvol_fx16);
					lvol = rvol = v;
				} else {
					lvol = 0;
					rvol = mixer_apply_gvol(Mixer.rvol[ch-1], gvol_fx16);
				}
				mixer_fx64_t abs_pos = owner->pos + owner->step * (uint64_t)offset;
				pos = (uint32_t)abs_pos & 0x7FFFFFFF;
				step = (uint32_t)owner->step & 0x7FFFFFFF;
				ptr = (uint8_t*)owner->ptr + ((abs_pos & ~0x7FFFFFFF) >> MIXER_FX64_FRAC);
				flags = (owner->flags & (CH_FLAGS_BPS_SHIFT | CH_FLAGS_16BIT | CH_FLAGS_STEREO)) | CH_FLAGS_STEREO_SUB;
				if ((fake_loop & (1<<(ch-1))) || owner->pos>>31 != owner->len>>31) {
					len = 0xFFFFFFFF;
					loop_len = 0;
				} else {
					len = (uint32_t)owner->len & 0x7FFFFFFF;
					loop_len = (uint32_t)owner->loop_len & 0x7FFFFFFF;
				}
			} else if (!c->ptr) {
				lvol = rvol = 0;
				ptr = NULL;
			} else {
				mixer_fx64_t abs_pos = c->pos + c->step * (uint64_t)offset;
				pos = (uint32_t)abs_pos & 0x7FFFFFFF;
				step = (uint32_t)c->step & 0x7FFFFFFF;
				if (c->flags & CH_FLAGS_VADPCM) {
					// ptr is the frame base; high bits of pos are sample index bits
					// above 31 — not used for byte offset (samples, not bytes).
					ptr = c->ptr;
					codebook = c->codebook;
					state = c->codec_state;
					loop_state = c->loop_state;
				} else {
					ptr = (uint8_t*)c->ptr + ((abs_pos & ~0x7FFFFFFF) >> MIXER_FX64_FRAC);
				}
				if ((fake_loop & (1<<ch)) || c->pos>>31 != c->len>>31) {
					len = 0xFFFFFFFF;
					loop_len = 0;
				} else {
					len = (uint32_t)c->len & 0x7FFFFFFF;
					assert(c->loop_len <= 0x7FFFFFFF);
					loop_len = (uint32_t)c->loop_len & 0x7FFFFFFF;
				}

				// Unrolled VADPCM loop: split this channel at the wrap tick and
				// insert SetState(loop_state) so the second half continues with
				// the correct predictor state.
				if ((c->flags & CH_FLAGS_VADPCM) && (fake_loop & (1<<ch)) &&
					c->loop_state && c->step)
				{
					mixer_fx64_t wlen = c->len;
					mixer_fx64_t wpos = abs_pos;
					mixer_fx64_t end = wpos + c->step * (uint64_t)ns;
					if (wpos < wlen && end >= wlen) {
						int64_t dist = (int64_t)(wlen - wpos);
						ns0 = (int)((dist + (int64_t)c->step - 1) / (int64_t)c->step);
						if (ns0 < 1) ns0 = 1;
						if (ns0 > ns) ns0 = ns;
						ns1 = ns - ns0;
						if (ns1 > 0) {
							mixer_fx64_t p1 = wpos + c->step * (uint64_t)ns0;
							while (p1 >= c->len)
								p1 -= c->loop_len;
							pos1 = (uint32_t)p1 & 0x7FFFFFFF;
						}
					}
				}

				if (c->flags & CH_FLAGS_STEREO) {
					if (c->flags & CH_FLAGS_FORCE_MONO) {
						mixer_fx15_t v = mixer_apply_gvol(Mixer.lvol[ch] >> 1, gvol_fx16);
						lvol = rvol = v;
					} else {
						lvol = mixer_apply_gvol(Mixer.lvol[ch], gvol_fx16);
						rvol = 0;
					}
				} else if (c->flags & CH_FLAGS_FORCE_MONO) {
					mixer_fx15_t v = mixer_apply_gvol(
						(Mixer.lvol[ch] + Mixer.rvol[ch]) >> 1, gvol_fx16);
					lvol = rvol = v;
				} else {
					lvol = mixer_apply_gvol(Mixer.lvol[ch], gvol_fx16);
					rvol = mixer_apply_gvol(Mixer.rvol[ch], gvol_fx16);
				}
			}

			if (clear_accum) {
				flags |= CH_FLAGS_CLEAR_ACCUM;
				clear_accum = false;
			}

			mixer_emit_channel(ch, flags, lvol, rvol, pos, step, len, loop_len, ptr,
				ns0, 0, codebook, state, loop_state);
			if (ns1 > 0) {
				mixer_emit_setstate(state, loop_state);
				mixer_emit_channel(ch, flags & ~CH_FLAGS_CLEAR_ACCUM, lvol, rvol,
					pos1, step, len, loop_len, ptr,
					ns1, ns0, codebook, state, loop_state);
			}
		}

		if (clear_accum) {
			mixer_emit_channel(0, CH_FLAGS_CLEAR_ACCUM, 0, 0, 0, 0, 0, 0, NULL,
				ns, 0, NULL, NULL, NULL);
		}

		rspq_write(__mixer_overlay_id, MIXER_CMD_FLUSH,
			(uint32_t)ns, PhysicalAddr(out + offset), round_id);
		rspq_highpri_end();
		Mixer.round_enqueued = round_id;
	}

	for (int i = 0; i < Mixer.num_channels; i++) {
		if (Mixer.channels[i].ptr)
			Mixer.ch_buf[i].last_round = last_round_id;
	}

	for (int i=0;i<Mixer.num_channels;i++) {
		mixer_channel_t *ch = &Mixer.channels[i];
		if (!ch->ptr || (ch->flags & CH_FLAGS_STEREO_SUB))
			continue;

		ch->pos += ch->step * (uint64_t)num_samples;

		if (ch->loop_len && !(fake_loop & (1<<i))) {
			while (ch->pos >= ch->len)
				ch->pos -= ch->loop_len;
		}
	}

	Mixer.ticks += num_samples;
}

static mixer_event_t* mixer_next_event(void) {
	mixer_event_t *e = NULL;
	for (int i=0;i<Mixer.num_events;i++) {
		if (!e || Mixer.events[i].ticks < e->ticks)
			e = &Mixer.events[i];
	}
	return e;
}

void mixer_add_event(int64_t delay, MixerEvent cb, void *ctx) {
	Mixer.events[Mixer.num_events++] = (mixer_event_t){
		.cb = cb,
		.ctx = ctx,
		.ticks = Mixer.ticks + delay
	};
}

void mixer_remove_event(MixerEvent cb, void *ctx) {
	for (int i=0;i<Mixer.num_events;i++) {
		if (Mixer.events[i].cb == cb && Mixer.events[i].ctx == ctx) {
			memmove(&Mixer.events[i], &Mixer.events[i+1], sizeof(mixer_event_t) * (Mixer.num_events-i-1));
			Mixer.num_events--;
			return;
		}
	}
	assertf("mixer_remove_event: specified event does not exist\ncb:%p ctx:%p", (void*)cb, ctx);
}

void mixer_throttle(float num_samples) {
	Mixer.max_samples += num_samples;
	Mixer.throttled = true;
}

void mixer_unthrottle(void) {
	Mixer.max_samples = 0;
	Mixer.throttled = false;
}

static void mixer_poll_async(int16_t *out16, int num_samples) {
	int32_t *out = (int32_t*)out16;

	mixer_reclaim();

	// Since the AI can only play an even number of samples,
	// it's not possible to call this function with an odd number,
	// otherwise buffering might become complicated / impossible.
	assert(num_samples % 2 == 0);

	// Check if the mixer is throttled. If so, do not produce more
	// than the allowance (with a small extra equal to a full audio buffer,
	// to avoid issues with fixed-size buffers like those provided by audio.c),
	// and silence after it.
	if (Mixer.throttled) {
		int extra = Mixer.sample_rate / MIXER_POLL_PER_SECOND;
		int total = num_samples;
		num_samples = MIN(num_samples, Mixer.max_samples+extra);
		Mixer.max_samples -= num_samples;
		memset(out + num_samples, 0, (total - num_samples) * sizeof(int32_t));
	}

	while (num_samples > 0) {
		mixer_event_t *e = mixer_next_event();

		int ns = MIN(num_samples, e ? e->ticks - Mixer.ticks : num_samples);
		if (ns > 0) {
			mixer_exec(out, ns);
			out += ns;
			num_samples -= ns;
		}
		if (e && Mixer.ticks == e->ticks) {
			int64_t repeat = e->cb(e->ctx);
			if (repeat)
				e->ticks += repeat;
			else
				mixer_remove_event(e->cb, e->ctx);
		}
	}
}

void mixer_poll(int16_t *out16, int num_samples) {
	mixer_poll_async(out16, num_samples);
	// Preserve the synchronous contract for direct callers (user-owned
	// buffer / legacy audio_set_buffer_callback). One sync per poll instead
	// of one per mixer_exec.
	rspq_highpri_sync();
}

void mixer_try_play(void)
{
	// To smooth out the pacing for mixer and wav64 decodes, we fill buffers
	// to at least audio_get_num_buffers()-1, but fill completely, if there is
	// only one free one left.
	int free_buffers = audio_get_num_buffers() - audio_get_queued_buffers();
	if (free_buffers <= 0) {
		mixer_reclaim();
		return;
	}

	int buffers_to_fill = MAX(free_buffers - 1, 1);
	while (buffers_to_fill-- > 0) {
		short *buf = audio_write_begin();
		mixer_poll_async(buf, audio_get_buffer_length());
		audio_write_end();
	}
	rspq_flush();
}
