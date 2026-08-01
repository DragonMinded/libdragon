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
#include "profile.h"
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
 *   XVOL_L[32] + XVOL_R[32] (128 bytes) +
 *   the per-channel VADPCM pointer table (384 bytes).
 * ACCUM lives in .bss (not saved state) so the VADPCM bssovl1 bank still fits.
 */
#define MIXER_STATE_SIZE 512

/** @brief Max output samples per mix round (must match rsp_mixer.S). */
#define MIXER_MAX_SAMPLES_PER_ROUND  512

/**
 * @brief Samples for which a stopped channel keeps being emitted to the RSP.
 *
 * A channel with no data only runs the RSP volume ramp (it mixes nothing), so
 * once the ramp has decayed we stop emitting it altogether. The ramp is a
 * one-pole filter with coefficient 0.9837^8 per 8 samples, which reaches the
 * 16-bit noise floor in ~640 samples.
 */
#define MIXER_SILENCE_RAMP_SAMPLES   1024

// NOTE: keep these in sync with rsp_mixer.S
#define CH_FLAGS_BPS_SHIFT  	(3<<0)   ///< BPS shift value
#define CH_FLAGS_16BIT      	(1<<2)   ///< Set if the channel is 16 bit
#define CH_FLAGS_STEREO     	(1<<3)   ///< Set if the channel is stereo (left)
#define CH_FLAGS_STEREO_SUB 	(1<<4)   ///< The channel is the second half of a stereo (right)
#define CH_FLAGS_VADPCM     	(1<<5)   ///< In-mixer VADPCM mono (wire + CPU)
#define CH_FLAGS_FORCE_MONO  	(1<<6)   ///< Fold this channel's output to both buses (mono downmix). CPU-side only; RSP ucode ignores this bit.
#define CH_FLAGS_CLEAR_ACCUM 	(1<<7)   ///< Zero ACCUM before mixing (first MIX_CHANNEL of a round)
#define CH_FLAGS_RESIDENT       (1<<8)   ///< Channel plays from waveform->mem (no samplebuffer)
#define CH_FLAGS_LOOP_CACHED    (1<<10)  ///< Streamed loop pinned in the samplebuffer; RSP wraps
#define CH_FLAGS_STEREO_ALLOC	(1<<9)   ///< The channel has a buffer sized for stereo (CPU-side only)

#define MIXER_CMD_CHANNEL     0x0        ///< rspq command ID for channel setup
#define MIXER_CMD_SETCHANNEL  0x1        ///< rspq command ID for setting a channel
#define MIXER_CMD_FLUSH       0x2        ///< rspq command ID for flushing the mixer

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
	uint32_t wave_uuid;    ///< UUID of last configured waveform (survives stop)
	int silence_ns;        ///< Samples still to be emitted while silent (volume ramp)
	int vframe;            ///< VADPCM frame the decoder state in RDRAM refers to
	int max_round_ns;      ///< Max round length from step + samplebuffer margin (streamed)
} mixer_channel_t;

/** @brief Overlay saved-state layout (must match rsp_mixer.S) */
typedef struct {
	uint32_t xvol[MIXER_MAX_CHANNELS];      ///< [left:16][right:16]
	uint32_t codebook[MIXER_MAX_CHANNELS];
	uint32_t state[MIXER_MAX_CHANNELS];
	uint32_t loop_state[MIXER_MAX_CHANNELS];
} mixer_overlay_state_t;

_Static_assert(sizeof(mixer_overlay_state_t) == MIXER_STATE_SIZE, "mixer overlay state size mismatch");

/** @brief Shadow of the VADPCM pointers currently held by the ucode table. */
typedef struct {
	void *codebook;
	void *state;
	void *loop_state;
} mixer_chtbl_t;

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
	mixer_chtbl_t chtbl[MIXER_MAX_CHANNELS];
	mixer_fx15_t lvol[MIXER_MAX_CHANNELS];
	mixer_fx15_t rvol[MIXER_MAX_CHANNELS];

	uint32_t chtbl_dirty;   ///< VADPCM channels whose SETCHANNEL is out of date
	int hi_ch;              ///< Exclusive upper bound of channels to scan

} Mixer;

uint32_t __mixer_overlay_id;

void __mixer_profile_init(void) {
	profile_register(PS_MIXER,        "mixer_try_play", 0);
	profile_register(PS_XM_TICK,      "xm_tick", 1);
	profile_register(PS_XM_GETPOS,    "xm_getpos", 2);
	profile_register(PS_XM_LIBXM,     "xm_libxm", 2);
	profile_register(PS_XM_SYNC,      "xm_sync", 2);
	profile_register(PS_MIXER_EXEC,   "mixer_exec", 1);
	profile_register(PS_MIXER_PREP,   "prep", 2);
	profile_register(PS_MIXER_EMIT,   "emit", 2);
	profile_register(PS_SBUF_GET,     "sbuf_get", 3);
	profile_register(PS_VADPCM_READ,  "vadpcm_read", 4);
	profile_register(PS_VADPCM_HUFF,  "vadpcm_huff", 5);
	profile_register(PS_VADPCM_IO,    "vadpcm_io", 5);
	profile_register(PS_MIXER_ADVANCE,"advance", 2);
	profile_register(PS_MIXER_SEEK,   "ch_seek", 3);
}

static inline uint32_t mixer_bit(int ch) { return 1u << ch; }

/** Grow the exclusive scan limit so channel @p ch is included. */
static inline void mixer_touch_ch(int ch) {
	if (ch + 1 > Mixer.hi_ch)
		Mixer.hi_ch = ch + 1;
}

/** Recompute #mixer_channel_t.max_round_ns from step and samplebuffer margin. */
static void mixer_refresh_max_ns(int ch);

static inline int mixer_initialized(void) { return Mixer.num_channels != 0; }

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

	rspq_init();
	__mixer_overlay_id = rspq_overlay_register(&rsp_mixer);

	mixer_overlay_state_t *mixer_state = rspq_overlay_get_state(&rsp_mixer);
	memset(mixer_state, 0, sizeof(*mixer_state));
	data_cache_hit_writeback(mixer_state, sizeof(*mixer_state));
}

static int mixer_calc_buffer_size(int ch, waveform_t *wave)
{
	int64_t nsamples = Mixer.limits[ch].max_frequency;
	int64_t size;

	if (wave->format == WAVEFORM_FORMAT_VADPCM) {
		// Live window is SAMPLEBUFFER_MARGIN_UNITS; size for ~1/16 s so
		// typical streamed loops still fit the loop-cache pin.
		int64_t nframes = (int64_t)ceilf((float)nsamples / (float)(MIXER_POLL_PER_SECOND * 2));
		nframes = (nframes + 15) / 16;
		if (nframes < SAMPLEBUFFER_MARGIN_UNITS * 2)
			nframes = SAMPLEBUFFER_MARGIN_UNITS * 2;
		size = ROUND_UP(nframes * 9, 8);
	} else {
		nsamples *= Mixer.limits[ch].max_bits / 8;
		nsamples *= wave->channels;
		int64_t nbytes = (int64_t)ceilf((float)nsamples / (float)(MIXER_POLL_PER_SECOND * 2));
		int min_bytes = SAMPLEBUFFER_MARGIN_UNITS * 2 * (Mixer.limits[ch].max_bits / 8) * wave->channels;
		if (nbytes < min_bytes) nbytes = min_bytes;
		size = ROUND_UP(nbytes, 8);
	}

	if (Mixer.limits[ch].max_buf_sz && size > Mixer.limits[ch].max_buf_sz)
		size = Mixer.limits[ch].max_buf_sz;

	// samplebuffer needs ≥MARGIN units of usable space plus the mirrored tail.
	int ub = (wave && wave->format == WAVEFORM_FORMAT_VADPCM) ? 9
		: (Mixer.limits[ch].max_bits / 8) * (wave ? wave->channels : 1);
	int min_bytes = SAMPLEBUFFER_MARGIN_UNITS * 2 * ub;
	if (size < min_bytes)
		size = min_bytes;

	assert((size % 8) == 0);
	assert((int32_t)size == size);
	
	return size;
}

void mixer_set_vol(float vol) {
	Mixer.vol = vol;
}

void mixer_close(void) {
	assert(mixer_initialized());

	rspq_highpri_sync();

	rspq_overlay_unregister(__mixer_overlay_id);
	__mixer_overlay_id = 0;

	for (int i=0; i<Mixer.num_channels; i++)
	{
		mixer_channel_t *c = &Mixer.channels[i];
		if ((c->flags & CH_FLAGS_RESIDENT) && c->codec_state) {
			free_uncached(c->codec_state);
			c->codec_state = NULL;
		}
		if (samplebuffer_is_inited(&Mixer.ch_buf[i]))
			samplebuffer_close(&Mixer.ch_buf[i]);
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
	if (c->step == step)
		return;
	c->step = step;
	mixer_refresh_max_ns(ch);
}

void mixer_ch_set_vol(int ch, float lvol, float rvol) {
	mixer_channel_t *c = &Mixer.channels[ch];
	assertf(!(c->flags & CH_FLAGS_STEREO_SUB), "mixer_ch_set_vol: cannot call on secondary stereo channel %d", ch);
	mixer_fx15_t l = MIXER_FX15(lvol);
	mixer_fx15_t r = MIXER_FX15(rvol);
	if (Mixer.lvol[ch] == l && Mixer.rvol[ch] == r)
		return;
	Mixer.lvol[ch] = l;
	Mixer.rvol[ch] = r;
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

// Clamp a WaveformRead so codecs only ever see positions in [0, len).
//
// Samplebuffers and codecs are not loop-aware. The mixer stops rounds at
// loop boundaries (or pins small loops for the RSP to wrap), but a fetch
// window still includes #MIXER_LOOP_OVERREAD past the last useful sample,
// and a streamed wpos can sit past len while the samplebuffer keeps a
// contiguous unrolled view of the stream. This wrapper maps those cases
// onto reads in [0, len) — silence for one-shots, samples from loop_start
// for looping waveforms — without forcing a seek on a contiguous wrap.
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
	int ub = sbuf->unit_bytes;

	if (wpos >= wave_len) {
		if (!wave_loop) {
			void *dest = samplebuffer_append(sbuf, wlen);
			memset(dest, 0, wlen * ub);
			return;
		}
		// Keep seeking as-is: a contiguous fetch past len is still sequential
		// for the codec (it just continues); only force a seek when the
		// wrapped position lands exactly on the loop point.
		wpos = waveform_wrap_wpos(wpos, wave_len, wave_loop);
	}
	if (wave_loop && wpos == wave_len - wave_loop)
		seeking = true;

	int len1 = wlen;
	if (wpos + wlen > wave_len)
		len1 = wave_len - wpos;
	int len2 = wlen - len1;

	if (len1 > 0)
		wave->read(ctx, sbuf, wpos, len1, seeking);
	if (len2 <= 0)
		return;

	if (!wave_loop) {
		void *dest = samplebuffer_append(sbuf, len2);
		memset(dest, 0, len2 * ub);
		return;
	}

	// Overread past the end: refill from the loop start. A single fragment
	// covers MIXER_LOOP_OVERREAD; tiny loops may need more than one.
	int loop_start = wave_len - wave_loop;
	while (len2 > 0) {
		int ns = MIN(len2, wave_loop);
		wave->read(ctx, sbuf, loop_start, ns, true);
		len2 -= ns;
	}
}

static bool mixer_wave_fits(int ch);

/** @brief Apply a VADPCM seek to the channel's codec state via WaveformRead. */
static void mixer_vadpcm_seek(mixer_channel_t *c, int sample_pos) {
	assert(c->flags & CH_FLAGS_VADPCM);
	assert(c->wave && c->wave->read && c->codec_state);
	// Two buffers: a stereo read also touches the right-plane one, even when
	// seeking with no data to fetch.
	samplebuffer_t tmp[2] = {{0}};
	tmp[0].wave = c->wave;
	tmp[0].state = c->codec_state;
	tmp[0].state_size = c->wave->state_size;
	c->wave->read(c->wave->ctx, tmp, sample_pos / 16, 0, true);
	c->vframe = sample_pos / 16;
	if (c->flags & CH_FLAGS_STEREO)
		c[1].vframe = c->vframe;
}

/**
 * @brief Re-establish the playback state after the CPU moved a position.
 *
 * The RSP always resumes a channel from where the CPU says it is, assuming
 * that what it needs is already in place: the samples of that position in the
 * samplebuffer, and (for VADPCM) a decoder state in RDRAM saved exactly for the frame
 * containing it. Both assumptions only hold while playback advances
 * sequentially, so any jump made by the CPU must be fixed up here.
 */
static void mixer_ch_seek(int ch) {
	mixer_channel_t *c = &Mixer.channels[ch];
	samplebuffer_t *sbuf = &Mixer.ch_buf[ch];
	bool vadpcm = (c->flags & CH_FLAGS_VADPCM) != 0;
	int bps_fx64 = (vadpcm ? 0 : (c->flags & CH_FLAGS_BPS_SHIFT)) + MIXER_FX64_FRAC;
	int frame = (int)(c->pos >> (MIXER_FX64_FRAC + 4));

	if (!c->wave)
		return;

	// Resident waveforms are always fully available: only the decoder state
	// has to be moved.
	if (c->flags & CH_FLAGS_RESIDENT) {
		if (vadpcm && frame != c->vframe)
			mixer_vadpcm_seek(c, frame * 16);
		return;
	}

	if (c->flags & CH_FLAGS_LOOP_CACHED) {
		int len = c->len >> bps_fx64;
		int loop_len = c->loop_len >> bps_fx64;
		int cache_start = mixer_wave_fits(ch) ? 0 : len - loop_len;
		bool have_samples = (c->pos >> bps_fx64) >= cache_start;
		if (have_samples) {
			// The samples are pinned already: just move the decoder.
			if (vadpcm && frame != c->vframe)
				mixer_vadpcm_seek(c, frame * 16);
			return;
		}
		// Seeking before the pinned region: go back to streaming, and let the
		// next round pin the loop again (re-seeding the decoder while at it).
		c->flags &= ~CH_FLAGS_LOOP_CACHED;
		if (vadpcm && (c->flags & CH_FLAGS_STEREO)) {
			Mixer.channels[ch+1].flags &= ~CH_FLAGS_LOOP_CACHED;
			samplebuffer_flush(&Mixer.ch_buf[ch+1]);
		}
		samplebuffer_flush(sbuf);
		mixer_refresh_max_ns(ch);
		return;
	}

	// Streamed: flushing the samplebuffer forces the next fetch to seek, which
	// is what re-seeds the decoder state.
	if (vadpcm && frame != c->vframe)
		samplebuffer_flush(sbuf);
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

	bool resident = wave->mem != NULL;
	bool vadpcm = wave->format == WAVEFORM_FORMAT_VADPCM;
	bool stereo_vadpcm = vadpcm && wave->channels == 2;

	// Stereo VADPCM uses two mono rings (ch and ch+1). PCM stereo uses one
	// interleaved buffer on the owner (STEREO_ALLOC). The ring on ch+1 belongs
	// to this channel for as long as it keeps playing stereo VADPCM: ch+1
	// cannot be played on its own while STEREO_SUB is set.
	bool was_stereo_vadpcm = !(c->flags & CH_FLAGS_RESIDENT) &&
		(c->flags & (CH_FLAGS_VADPCM|CH_FLAGS_STEREO)) == (CH_FLAGS_VADPCM|CH_FLAGS_STEREO);
	if (!resident && stereo_vadpcm) {
		// Only reuse the pair if it was allocated as a pair and is still intact
		// (mixer_ch_set_limits frees just the primary ring).
		if (!was_stereo_vadpcm || !samplebuffer_is_inited(sbuf)) {
			rspq_highpri_sync();
			samplebuffer_close(sbuf);
			samplebuffer_close(&Mixer.ch_buf[ch+1]);
		}
	} else if (!resident) {
		// Leaving stereo VADPCM: release the secondary ring, or the next stereo
		// VADPCM would find it allocated but still holding the previous stream.
		if (was_stereo_vadpcm) {
			rspq_highpri_sync();
			samplebuffer_close(&Mixer.ch_buf[ch+1]);
		}
		if (wave->channels == 2 && !(c->flags & CH_FLAGS_STEREO_ALLOC)) {
			rspq_highpri_sync();
			samplebuffer_close(sbuf);
		}
	}
	// Check if the state buffer is big enough, otherwise we need to reallocate
	if (!resident && sbuf->state_size < wave->state_size) {
		rspq_highpri_sync();
		samplebuffer_close(sbuf);
		if (stereo_vadpcm) samplebuffer_close(&Mixer.ch_buf[ch+1]);
	}

	if (!resident && !samplebuffer_is_inited(sbuf)) {
		int size = ROUND_UP(mixer_calc_buffer_size(ch, wave), 16);
		int ub = vadpcm ? 9 : ((wave->bits / 8) * (stereo_vadpcm ? 1 : wave->channels));
		size += SAMPLEBUFFER_MARGIN_UNITS * ub;
		size = ROUND_UP(size, 16);
		int state_size = ROUND_UP(wave->state_size, 16);
		void *ptr = malloc_uncached(size + state_size);
		assertf(ptr, "Out of memory");
		samplebuffer_init(sbuf, ptr, size, state_size);
		if (stereo_vadpcm) {
			assertf(ch != Mixer.num_channels-1, "cannot play stereo VADPCM on last channel");
			void *ptr_r = malloc_uncached(size); // R samplebuffer: no second state (shared)
			assertf(ptr_r, "Out of memory");
			samplebuffer_init(&Mixer.ch_buf[ch+1], ptr_r, size, 0);
			c->flags |= CH_FLAGS_STEREO_ALLOC;
		} else if (wave->channels == 2) {
			c->flags |= CH_FLAGS_STEREO_ALLOC;
		}
	}

	// Configure the waveform on this channel, if we have not already.
	if (wave->__uuid != c->wave_uuid || (c->flags & CH_FLAGS_RESIDENT) != (resident ? CH_FLAGS_RESIDENT : 0)) {
		if (!resident) {
			samplebuffer_flush(sbuf);
			// Stereo VADPCM keeps a second ring on ch+1, which is reconfigured
			// below as well: it must be emptied too.
			if (stereo_vadpcm)
				samplebuffer_flush(&Mixer.ch_buf[ch+1]);
		}

		// If this channel is playing something else, stop it
		if (mixer_ch_playing(ch))
			mixer_ch_stop(ch);

		// Free a previous resident-only codec state if switching away.
		if ((c->flags & CH_FLAGS_RESIDENT) && c->codec_state) {
			free_uncached(c->codec_state);
			c->codec_state = NULL;
		}

		assert(wave->channels == 1 || wave->channels == 2);
		assert(wave->bits == 8 || wave->bits == 16);
		assertf(wave->len >= 0 && wave->len <= WAVEFORM_MAX_LEN, "waveform %s: invalid length %x", wave->name, wave->len);
		assertf(wave->len != WAVEFORM_UNKNOWN_LEN || wave->loop_len == 0, "waveform %s with unknown length cannot loop", wave->name);

		c->flags &= ~(CH_FLAGS_BPS_SHIFT | CH_FLAGS_16BIT | CH_FLAGS_STEREO | CH_FLAGS_VADPCM | CH_FLAGS_RESIDENT | CH_FLAGS_LOOP_CACHED);
		c->codebook = NULL;
		c->loop_state = NULL;
		c->codec_state = NULL;

		if (resident) {
			c->flags |= CH_FLAGS_RESIDENT;
			if (wave->state_size) {
				c->codec_state = malloc_uncached(ROUND_UP(wave->state_size, 16));
				assertf(c->codec_state, "Out of memory");
				memset(c->codec_state, 0, wave->state_size);
			}
		} else {
			if (wave->format == WAVEFORM_FORMAT_VADPCM) {
				samplebuffer_set_unit_bytes(sbuf, 9);
				if (stereo_vadpcm)
					samplebuffer_set_unit_bytes(&Mixer.ch_buf[ch+1], 9);
			} else {
				samplebuffer_set_bps(sbuf, wave->bits*wave->channels);
			}
			samplebuffer_set_waveform(sbuf, wave, wave->read ? waveform_read : NULL);
			c->codec_state = sbuf->state;
		}

		if (wave->format == WAVEFORM_FORMAT_VADPCM) {
			waveform_vadpcm_t *vc = wave->codec;
			assertf(vc && vc->codebook, "waveform %s: VADPCM missing codec/codebook", wave->name);
			c->flags |= CH_FLAGS_VADPCM | CH_FLAGS_16BIT;
			if (wave->channels == 2) c->flags |= CH_FLAGS_STEREO;
			c->codebook = vc->codebook;
			c->loop_state = vc->loop_state;
			c->len = MIXER_FX64((int64_t)wave->len);
			c->loop_len = MIXER_FX64((int64_t)wave->loop_len);
			if (stereo_vadpcm) {
				mixer_channel_t *r = &Mixer.channels[ch+1];
				r->flags = (r->flags & CH_FLAGS_FORCE_MONO) | CH_FLAGS_STEREO_SUB | CH_FLAGS_VADPCM | CH_FLAGS_16BIT;
				r->codebook = (uint8_t*)vc->codebook + 128; // 8 vectors × 16 bytes
				r->codec_state = (uint8_t*)c->codec_state + 16;
				r->loop_state = vc->loop_state ? (uint8_t*)vc->loop_state + 16 : NULL;
				r->len = c->len;
				r->loop_len = c->loop_len;
				r->wave = wave;
				r->wave_uuid = wave->__uuid;
			}
		} else {
			int bps = (wave->bits == 16 ? 1 : 0) + (wave->channels == 2 ? 1 : 0);
			c->flags |= bps | (wave->channels == 2 ? CH_FLAGS_STEREO : 0) | (wave->bits == 16 ? CH_FLAGS_16BIT : 0);
			c->len = MIXER_FX64((int64_t)wave->len) << bps;
			c->loop_len = MIXER_FX64((int64_t)wave->loop_len) << bps;
		}
		mixer_ch_set_freq(ch, wave->frequency);

		if (!resident && wave->start)
			wave->start(wave->ctx, sbuf);

		c->wave_uuid = wave->__uuid;
		tracef("mixer_ch_play[new]: ch=%d len=%llx loop_len=%llx wave=%s%s\n",
			ch, (uint64_t)wave->len, (uint64_t)wave->loop_len, wave->name,
			resident ? " [resident]" : "");
	} else if (!resident) {
		tracef("mixer_ch_play[old]: ch=%d wave=%s\n", ch, wave->name);
		assertf(sbuf->wave == wave, "%s: uuid match (%ld) but pointer mismatch: %p != %p",
			wave->name, wave->__uuid, sbuf->wave, wave);
	}

	// Restart from the beginning of the waveform
	c->wave = wave;
	if (resident && stereo_vadpcm) {
		int nframes = wave->len / 16;
		c->ptr = (void*)wave->mem;
		Mixer.channels[ch+1].ptr = (uint8_t*)wave->mem + nframes * 9;
		Mixer.channels[ch+1].pos = 0;
	} else {
		c->ptr = resident ? (void*)wave->mem : SAMPLES_PTR(sbuf);
	}
	c->pos = 0;
	PROFILE_START(PS_MIXER_SEEK);
	mixer_ch_seek(ch);
	PROFILE_STOP(PS_MIXER_SEEK);

	// Mark ch+1 as stereo sub for PCM interleaved or VADPCM dual-mono.
	if (c->flags & CH_FLAGS_STEREO) {
		assertf(ch != Mixer.num_channels-1, "cannot configure last channel (%d) as stereo", ch);
		assertf(!mixer_ch_playing(ch+1) || (Mixer.channels[ch+1].flags & CH_FLAGS_STEREO_SUB),
			"cannot play stereo waveform on channel %d because channel %d is active", ch, ch+1);
		if (!(c->flags & CH_FLAGS_VADPCM)) {
			// PCM interleaved: R has no samplebuffer of its own. Replace flags
			// so a previous mono/VADPCM occupant cannot leave VADPCM set and
			// make Phase A try to fetch from a null-wave samplebuffer.
			Mixer.channels[ch+1].flags = (Mixer.channels[ch+1].flags & CH_FLAGS_FORCE_MONO) | CH_FLAGS_STEREO_SUB;
			Mixer.channels[ch+1].ptr = NULL;
		} else {
			Mixer.channels[ch+1].flags |= CH_FLAGS_STEREO_SUB;
		}
	} else if (ch != Mixer.num_channels-1) {
		Mixer.channels[ch+1].flags &= ~CH_FLAGS_STEREO_SUB;
	}

	mixer_touch_ch(ch);
	if (c->flags & CH_FLAGS_VADPCM)
		Mixer.chtbl_dirty |= mixer_bit(ch);
	if (c->flags & CH_FLAGS_STEREO) {
		mixer_touch_ch(ch+1);
		if (c->flags & CH_FLAGS_VADPCM)
			Mixer.chtbl_dirty |= mixer_bit(ch+1);
	}
	mixer_refresh_max_ns(ch);
}

void mixer_ch_set_pos(int ch, double pos) {
	mixer_channel_t *c = &Mixer.channels[ch];
	assertf(!(c->flags & CH_FLAGS_STEREO_SUB), "mixer_ch_set_pos: cannot call on secondary stereo channel %d", ch);
	mixer_fx64_t p = MIXER_FX64(pos);
	if (!(c->flags & CH_FLAGS_VADPCM))
		p <<= (c->flags & CH_FLAGS_BPS_SHIFT);
	c->pos = p;
	PROFILE_START(PS_MIXER_SEEK);
	mixer_ch_seek(ch);
	PROFILE_STOP(PS_MIXER_SEEK);
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

	// Already stopped: do not re-arm the silence ramp. The XM tick calls stop
	// every tick for idle channels; re-arming would keep emitting silent
	// MIX_CHANNEL commands forever.
	if (!c->ptr)
		return;

	tracef("mixer_ch_stop: ch=%d\n", ch);

	bool stereo = (c->flags & CH_FLAGS_STEREO) != 0;
	if (stereo) {
		// Dropping STEREO_SUB releases ch+1, so it must be disarmed like the
		// owner: a stereo VADPCM sub keeps its own ptr/pos, and would
		// otherwise look like an independent channel and keep advancing and
		// fetching from a samplebuffer that only its owner can fill.
		c[1].flags &= ~CH_FLAGS_STEREO_SUB;
		c[1].ptr = 0;
		c[1].pos = 0;
		c[1].silence_ns = MIXER_SILENCE_RAMP_SAMPLES;
		Mixer.chtbl_dirty &= ~mixer_bit(ch+1);
	}

	c->ptr = 0;
	c->pos = 0;
	c->silence_ns = MIXER_SILENCE_RAMP_SAMPLES;
	Mixer.chtbl_dirty &= ~mixer_bit(ch);

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
		rspq_highpri_sync();
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
	int nsamples, int acc_offset)
{
	rspq_write_t w = rspq_write_begin(__mixer_overlay_id, MIXER_CMD_CHANNEL, 8);
	// a0 payload (24 bits): ch_idx<<16 | flags<<8
	rspq_write_arg(&w, ((uint32_t)ch << 16) | ((flags & 0xFF) << 8));
	rspq_write_arg(&w, ((uint32_t)(uint16_t)lvol << 16) | (uint16_t)rvol);
	rspq_write_arg(&w, pos);
	rspq_write_arg(&w, step);
	rspq_write_arg(&w, len);
	rspq_write_arg(&w, loop_len);
	rspq_write_arg(&w, ptr ? PhysicalAddr(ptr) : 0);
	rspq_write_arg(&w, ((uint32_t)(uint16_t)acc_offset << 16) | (uint16_t)nsamples);
	rspq_write_end(&w);
}

/** @brief Record a channel's VADPCM pointers in the ucode channel table. */
static void mixer_emit_setchannel(int ch, void *codebook, void *state, void *loop_state)
{
	rspq_write(__mixer_overlay_id, MIXER_CMD_SETCHANNEL, (uint32_t)ch << 16,
		codebook ? PhysicalAddr(codebook) : 0,
		state ? PhysicalAddr(state) : 0,
		loop_state ? PhysicalAddr(loop_state) : 0);
}

// True if the whole waveform fits in the samplebuffer, so that it can be pinned
// as a unit instead of just its loop region.
static bool mixer_wave_fits(int i) {
	mixer_channel_t *ch = &Mixer.channels[i];
	samplebuffer_t *sbuf = &Mixer.ch_buf[i];
	if (!samplebuffer_is_inited(sbuf))
		return false;
	bool vadpcm = (ch->flags & CH_FLAGS_VADPCM) != 0;
	int bps = vadpcm ? 0 : (ch->flags & CH_FLAGS_BPS_SHIFT);
	int len = ch->len >> (bps + MIXER_FX64_FRAC);
	int slen = vadpcm ? (len + 15) / 16 : len;
	return slen > 0 &&
		slen + (MIXER_LOOP_OVERREAD / (vadpcm ? 9 : (1<<bps)) + 1) <= sbuf->size;
}

/** @brief Pin a streamed loop into the samplebuffer (one-shot). RSP wraps after. */
static void mixer_fill_loop_cache(int ch) {
	mixer_channel_t *c = &Mixer.channels[ch];
	samplebuffer_t *sbuf = &Mixer.ch_buf[ch];
	waveform_t *wave = c->wave;
	bool vadpcm = (c->flags & CH_FLAGS_VADPCM) != 0;
	bool stereo_vadpcm = vadpcm && (c->flags & CH_FLAGS_STEREO);
	int bps = vadpcm ? 0 : (c->flags & CH_FLAGS_BPS_SHIFT);
	int bps_fx64 = bps + MIXER_FX64_FRAC;
	int ub = sbuf->unit_bytes;
	int len = c->len >> bps_fx64;
	int loop_len = c->loop_len >> bps_fx64;
	// A waveform that fits entirely is pinned from its start, so that its
	// attack stays available and the RSP can wrap it right away.
	bool whole = mixer_wave_fits(ch);
	int cache_start = whole ? 0 : len - loop_len;
	int cache_len = whole ? len : loop_len;
	int sloop_start = vadpcm ? cache_start / 16 : cache_start;
	int sloop_len = vadpcm ? (cache_len + 15) / 16 : cache_len;
	int overread = (MIXER_LOOP_OVERREAD + ub - 1) / ub;
	int fill = sloop_len + overread;
	assertf(fill <= sbuf->size, "ch:%d loop cache %x > samplebuffer %x", ch, fill, sbuf->size);
	assert(wave && wave->read);

	samplebuffer_flush(sbuf);
	sbuf->wpos = 0;
	sbuf->wnext = 0;
	sbuf->head = 0;
	if (stereo_vadpcm) {
		samplebuffer_t *sbuf_r = &Mixer.ch_buf[ch+1];
		assertf(fill <= sbuf_r->size, "ch:%d R loop cache %x > samplebuffer %x", ch+1, fill, sbuf_r->size);
		samplebuffer_flush(sbuf_r);
		sbuf_r->wpos = 0;
		sbuf_r->wnext = 0;
		sbuf_r->head = 0;
	}
	wave->read(wave->ctx, sbuf, sloop_start, sloop_len + overread, true);
	sbuf->wpos = sloop_start;
	sbuf->wnext = sloop_start + sbuf->widx;
	if (stereo_vadpcm) {
		samplebuffer_t *sbuf_r = &Mixer.ch_buf[ch+1];
		sbuf_r->wpos = sloop_start;
		sbuf_r->wnext = sloop_start + sbuf_r->widx;
		Mixer.channels[ch+1].ptr = (uint8_t*)SAMPLES_PTR(sbuf_r) - sloop_start * 9;
		Mixer.channels[ch+1].flags |= CH_FLAGS_LOOP_CACHED;
	}

	if (vadpcm)
		c->ptr = (uint8_t*)SAMPLES_PTR(sbuf) - sloop_start * 9;
	else
		c->ptr = (uint8_t*)SAMPLES_PTR(sbuf) - (cache_start << bps);
	c->flags |= CH_FLAGS_LOOP_CACHED;
	if (vadpcm) {
		c->vframe = sloop_start;
		if (stereo_vadpcm) Mixer.channels[ch+1].vframe = sloop_start;
	}
	mixer_refresh_max_ns(ch);
	tracef("ch:%d loop cached at %x len=%x\n", ch, sloop_start, sloop_len);
}

// True if the channel's loop is small enough to be pinned into the samplebuffer,
// so that the RSP can wrap it by itself. Large loops are handled via wrap=seek.
static bool mixer_loop_fits(int i) {
	mixer_channel_t *ch = &Mixer.channels[i];
	samplebuffer_t *sbuf = &Mixer.ch_buf[i];
	if (!samplebuffer_is_inited(sbuf))
		return false;
	bool vadpcm = (ch->flags & CH_FLAGS_VADPCM) != 0;
	int bps = vadpcm ? 0 : (ch->flags & CH_FLAGS_BPS_SHIFT);
	int loop_len = ch->loop_len >> (bps + MIXER_FX64_FRAC);
	int sloop = vadpcm ? loop_len / 16 : loop_len;
	return sloop > 0 &&
		sloop + (MIXER_LOOP_OVERREAD / (vadpcm ? 9 : (1<<bps)) + 1) <= sbuf->size;
}

// Wrap a large loop: rebase the position within the loop and restart the
// stream there. Must be done between rounds, as the RSP never wraps these.
static void mixer_large_loop_wrap(int i) {
	mixer_channel_t *ch = &Mixer.channels[i];
	bool vadpcm = (ch->flags & CH_FLAGS_VADPCM) != 0;
	int bps_fx64 = (vadpcm ? 0 : (ch->flags & CH_FLAGS_BPS_SHIFT)) + MIXER_FX64_FRAC;
	int wpos = ch->pos >> bps_fx64;
	int wpos2 = waveform_wrap_wpos(wpos, ch->len >> bps_fx64, ch->loop_len >> bps_fx64);
	ch->pos -= (int64_t)(wpos - wpos2) << bps_fx64;
	samplebuffer_flush(&Mixer.ch_buf[i]);
	if (vadpcm)
		mixer_vadpcm_seek(ch, wpos2);
	tracef("ch:%d large-loop seek %x -> %x\n", i, wpos, wpos2);
}

// End-of-sample, loop-cache transition and large-loop seek. Runs before each
// round: a short loop can be reached in the middle of a mixer_exec, and from
// that point on the RSP must be the one wrapping it.
static void mixer_update_loops(void) {
	for (int i = 0; i < Mixer.hi_ch; i++) {
		mixer_channel_t *ch = &Mixer.channels[i];
		bool vadpcm = (ch->flags & CH_FLAGS_VADPCM) != 0;
		int bps_fx64 = (vadpcm ? 0 : (ch->flags & CH_FLAGS_BPS_SHIFT)) + MIXER_FX64_FRAC;

		if (!ch->ptr || (ch->flags & CH_FLAGS_STEREO_SUB))
			continue;

		int len = ch->len >> bps_fx64;
		int loop_len = ch->loop_len >> bps_fx64;
		int wpos = ch->pos >> bps_fx64;

		if (!loop_len) {
			if (wpos >= len)
				mixer_ch_stop(i);
			continue;
		}
		if (ch->flags & (CH_FLAGS_RESIDENT | CH_FLAGS_LOOP_CACHED))
			continue;

		if (mixer_loop_fits(i)) {
			if (wpos >= len - loop_len || mixer_wave_fits(i))
				mixer_fill_loop_cache(i);
		} else if (wpos >= len) {
			mixer_large_loop_wrap(i);
		}
	}
}

// Refresh the ucode per-channel VADPCM table. The pointers only change when a
// new waveform starts playing, so in steady state this emits nothing.
static void mixer_refresh_chtbl(void) {
	if (!Mixer.chtbl_dirty)
		return;
	for (int ch = 0; ch < Mixer.hi_ch; ch++) {
		if (!(Mixer.chtbl_dirty & mixer_bit(ch)))
			continue;
		mixer_channel_t *c = &Mixer.channels[ch];
		mixer_chtbl_t *sh = &Mixer.chtbl[ch];
		mixer_channel_t *src = (c->flags & CH_FLAGS_STEREO_SUB) ? c-1 : c;
		Mixer.chtbl_dirty &= ~mixer_bit(ch);
		if (!src->ptr || !(c->flags & CH_FLAGS_VADPCM))
			continue;
		*sh = (mixer_chtbl_t){ c->codebook, c->codec_state, c->loop_state };
		mixer_emit_setchannel(ch, c->codebook, c->codec_state, c->loop_state);
	}
}

// Global volume to apply this call, including the fade-out on reset.
static mixer_fx16_t mixer_global_volume(void) {
	float gvol = Mixer.vol;
	uint32_t reset_time = exception_reset_time();
	if (reset_time) {
		const float FADE_OUT_TIME = (float)RESET_TIME_LENGTH / TICKS_PER_SECOND;
		float elapsed = (float)reset_time / TICKS_PER_SECOND;
		gvol *= (FADE_OUT_TIME - MIN(elapsed, FADE_OUT_TIME)) / FADE_OUT_TIME;
	}
	return MIXER_FX16(gvol);
}

/** Cap of output samples a streamed channel can take in one round.
 *
 * A streamed channel can only be given one contiguous window of the
 * samplebuffer, that is #SAMPLEBUFFER_MARGIN_UNITS minus the units the RSP may
 * overread past its end. Convert that window (16 samples per unit for VADPCM,
 * one otherwise) into output samples through the resampling step: past this
 * point the RSP would outrun what the CPU can hand it in a single piece.
 * Channels reading straight from RDRAM (resident, pinned loop, stereo sub)
 * have no such limit.
 *
 * VADPCM also reserves one frame for the ceiling in #mixer_channel_window:
 * a span of N samples starting mid-frame covers ceil((pos%16+N)/16) frames,
 * which is one more than N/16 when pos is not frame-aligned.
 */
static void mixer_refresh_max_ns(int ch) {
	mixer_channel_t *c = &Mixer.channels[ch];
	if (!c->ptr || !c->step ||
		(c->flags & (CH_FLAGS_RESIDENT | CH_FLAGS_LOOP_CACHED | CH_FLAGS_STEREO_SUB))) {
		c->max_round_ns = MIXER_MAX_SAMPLES_PER_ROUND;
		return;
	}
	bool vadpcm = (c->flags & CH_FLAGS_VADPCM) != 0;
	int bps_fx64 = (vadpcm ? 0 : (c->flags & CH_FLAGS_BPS_SHIFT)) + MIXER_FX64_FRAC;
	int ub = Mixer.ch_buf[ch].unit_bytes;
	int overread = (MIXER_LOOP_OVERREAD + ub - 1) / ub;
	int max_wlen = MAX(SAMPLEBUFFER_MARGIN_UNITS - overread - (vadpcm ? 1 : 0), 1);
	uint64_t units = vadpcm ? (uint64_t)max_wlen * 16 : (uint64_t)max_wlen;
	int ns = (int)((units << bps_fx64) / c->step);
	c->max_round_ns = MIN(MAX(ns, 1), MIXER_MAX_SAMPLES_PER_ROUND);
}

// Number of samples the next round can mix: a full round, unless a streamed
// channel would outrun what the CPU is able to feed it in one go.
//
// The result is always even, so that each round advances the output pointer by
// a multiple of 8 bytes and MIX_FLUSH can DMA the accumulator out as-is
// (see #mixer_poll_async).
static int mixer_round_length(int max_ns) {
	int ns = MIN(max_ns, MIXER_MAX_SAMPLES_PER_ROUND);

	for (int ch = 0; ch < Mixer.hi_ch; ch++) {
		mixer_channel_t *c = &Mixer.channels[ch];
		if (!c->ptr || !c->step ||
			(c->flags & (CH_FLAGS_RESIDENT | CH_FLAGS_LOOP_CACHED | CH_FLAGS_STEREO_SUB)))
			continue;
		// max_round_ns is 0 until the first set_freq/play refresh; treat as
		// unlimited so a stale zero cannot collapse the round to empty (which
		// would spin forever in mixer_exec).
		if (c->max_round_ns > 0)
			ns = MIN(ns, c->max_round_ns);

		// Stop where the CPU has to step in: the start of a small loop (pinned
		// by mixer_update_loops before the next round), or len for a large one
		// (rebased by a seek).
		if (c->loop_len) {
			mixer_fx64_t limit = mixer_loop_fits(ch) ? c->len - c->loop_len : c->len;
			if (c->pos < limit && c->pos + c->step * (uint64_t)ns >= limit) {
				int64_t dist = (int64_t)(limit - c->pos);
				int ns2 = (int)((dist + (int64_t)c->step - 1) / (int64_t)c->step);
				ns = MIN(ns, MAX(ns2, 1));
			}
		}
	}

	// Truncate to an even number of samples. Stopping one sample before a
	// constraint is always safe (the next round resumes there); the only
	// exception is a limit exactly one sample away, where we mix one sample
	// too many. That sample is still valid data: past a small loop point it
	// is the loop body the CPU is about to pin, past the end of a large loop
	// it is covered by #MIXER_LOOP_OVERREAD, and in both cases the CPU takes
	// over from the next round.
	return MAX(ns & ~1, 2);
}

// Volumes to send for a channel: a stereo owner only carries the L plane and
// its sub channel only the R one, and FORCE_MONO folds the surviving plane
// onto both outputs. @p flags are the owner's flags.
static void mixer_channel_volumes(int ch, uint32_t flags, bool sub,
	mixer_fx16_t gvol, mixer_fx15_t *lvol, mixer_fx15_t *rvol)
{
	bool mono = (flags & CH_FLAGS_FORCE_MONO) != 0;
	if (sub) {
		mixer_fx15_t v = Mixer.rvol[ch-1];
		*lvol = mono ? mixer_apply_gvol(v >> 1, gvol) : 0;
		*rvol = mixer_apply_gvol(mono ? v >> 1 : v, gvol);
	} else if (flags & CH_FLAGS_STEREO) {
		mixer_fx15_t v = Mixer.lvol[ch];
		*lvol = mixer_apply_gvol(mono ? v >> 1 : v, gvol);
		*rvol = mono ? *lvol : 0;
	} else if (mono) {
		*lvol = *rvol = mixer_apply_gvol((Mixer.lvol[ch] + Mixer.rvol[ch]) >> 1, gvol);
	} else {
		*lvol = mixer_apply_gvol(Mixer.lvol[ch], gvol);
		*rvol = mixer_apply_gvol(Mixer.rvol[ch], gvol);
	}
}

// Waveform bounds to send to the RSP. The top bit of pos and len is a wrap
// flag: when the two differ the position is past the end of the waveform, and
// the RSP must not compare against len. @p wrap is false for the loops that
// the CPU wraps by itself between rounds.
static void mixer_rsp_bounds(mixer_channel_t *c, bool wrap, uint32_t *len, uint32_t *loop_len) {
	if (c->pos>>31 != c->len>>31) {
		*len = 0xFFFFFFFF;
		*loop_len = 0;
	} else {
		*len = (uint32_t)c->len & 0x7FFFFFFF;
		*loop_len = wrap ? (uint32_t)c->loop_len & 0x7FFFFFFF : 0;
	}
}

// samplebuffer window that @p ns output samples read from a channel, in units
// (VADPCM frames or PCM samples), including the overread the RSP does at loops.
static void mixer_channel_window(int ch, int ns, int *wpos, int *wlen) {
	mixer_channel_t *c = &Mixer.channels[ch];
	samplebuffer_t *sbuf = &Mixer.ch_buf[ch];
	bool vadpcm = (c->flags & CH_FLAGS_VADPCM) != 0;
	int bps = vadpcm ? 0 : (c->flags & CH_FLAGS_BPS_SHIFT);
	int bps_fx64 = bps + MIXER_FX64_FRAC;
	int ub = sbuf->unit_bytes;

	int pos = c->pos >> bps_fx64;
	int last = (c->pos + c->step * (uint64_t)(ns > 0 ? ns-1 : 0)) >> bps_fx64;
	int next = (c->pos + c->step * (uint64_t)ns) >> bps_fx64;
	int len = MAX(last - pos + 1, next - pos);

	if (vadpcm) {
		*wpos = pos / 16;
		*wlen = (pos + len + 15) / 16 - *wpos + (MIXER_LOOP_OVERREAD + ub - 1) / ub;
	} else {
		*wpos = pos;
		*wlen = len + (MIXER_LOOP_OVERREAD >> bps);
	}
}

// Fetch from the samplebuffer the input window this round is going to read.
// Updates c->ptr to the waveform base; the RSP pointer for PCM may need
// the high-bit pos offset applied by the caller.
static void mixer_fetch_window(int ch, int ns) {
	mixer_channel_t *c = &Mixer.channels[ch];
	samplebuffer_t *sbuf = &Mixer.ch_buf[ch];
	int wpos, wlen;
	mixer_channel_window(ch, ns, &wpos, &wlen);

	if (c->flags & CH_FLAGS_VADPCM) {
		if (sbuf->widx == 0 || wpos < sbuf->wpos || wpos > sbuf->wpos + sbuf->widx)
			c->vframe = wpos;
		void *p = samplebuffer_get(sbuf, wpos, &wlen);
		assert(p);
		c->ptr = (uint8_t*)p - wpos * 9;
	} else {
		void *p = samplebuffer_get(sbuf, wpos, &wlen);
		assert(p);
		c->ptr = (uint8_t*)p - (wpos << (c->flags & CH_FLAGS_BPS_SHIFT));
	}
}

// Kick the fetches the next mixer_exec will need, so their PI DMA flies while
// the CPU sequences the song and the RSP mixes the rounds just enqueued.
static void mixer_prefetch_next(int num_samples) {
	for (int ch = 0; ch < Mixer.hi_ch; ch++) {
		mixer_channel_t *c = &Mixer.channels[ch];
		mixer_channel_t *owner = (c->flags & CH_FLAGS_STEREO_SUB) ? c-1 : c;
		if (!owner->ptr || !c->step ||
			(owner->flags & (CH_FLAGS_RESIDENT | CH_FLAGS_LOOP_CACHED)))
			continue;
		// A stereo pair streams through the owner's read, which fills both rings.
		if (c->flags & CH_FLAGS_STEREO_SUB)
			continue;
		// Nothing to overlap with if the waveform is produced synchronously.
		waveform_t *wave = Mixer.ch_buf[ch].wave;
		if (!wave || !wave->async_read)
			continue;
		int wpos, wlen;
		mixer_channel_window(ch, num_samples, &wpos, &wlen);
		samplebuffer_prefetch(&Mixer.ch_buf[ch], wpos, wlen);
	}
}

/** Wait for any in-flight PI DMA into the samplebuffer this channel reads. */
static void mixer_wait_channel_dma(int ch) {
	mixer_channel_t *c = &Mixer.channels[ch];
	if (c->flags & CH_FLAGS_RESIDENT)
		return;
	// PCM stereo R shares the owner's interleaved buffer.
	int bufch = ((c->flags & CH_FLAGS_STEREO_SUB) && !(c->flags & CH_FLAGS_VADPCM)) ? ch - 1 : ch;
	if (samplebuffer_is_inited(&Mixer.ch_buf[bufch]))
		samplebuffer_dma_wait(&Mixer.ch_buf[bufch]);
}

// Emit one mix round: a MIX_CHANNEL per active channel, plus the flush that
// writes the accumulator out to @p out.
//
// Phase A kicks all streamed fetches so PI DMAs pipeline across channels;
// Phase B waits for each channel's DMA only when about to emit its command.
static void mixer_emit_round(int32_t *out, int ns, mixer_fx16_t gvol) {
	bool clear_accum = true;

	// Phase A: enqueue all PI DMAs back-to-back.
	for (int ch = 0; ch < Mixer.hi_ch; ch++) {
		mixer_channel_t *c = &Mixer.channels[ch];
		mixer_channel_t *owner = (c->flags & CH_FLAGS_STEREO_SUB) ? c-1 : c;
		if (!owner->ptr || (owner->flags & (CH_FLAGS_RESIDENT | CH_FLAGS_LOOP_CACHED)))
			continue;
		if (c->flags & CH_FLAGS_STEREO_SUB) {
			if (!(owner->flags & CH_FLAGS_VADPCM))
				continue; // PCM R shares the owner's fetch
			c->pos = owner->pos;
			c->step = owner->step;
			c->len = owner->len;
			c->loop_len = owner->loop_len;
		}
		mixer_fetch_window(ch, ns);
	}

	// Phase B: emit, syncing each channel's DMA at the last moment.
	for (int ch = 0; ch < Mixer.hi_ch; ch++) {
		mixer_channel_t *c = &Mixer.channels[ch];
		mixer_channel_t *owner = (c->flags & CH_FLAGS_STEREO_SUB) ? c-1 : c;
		uint32_t flags = c->flags & (CH_FLAGS_BPS_SHIFT | CH_FLAGS_16BIT |
			CH_FLAGS_STEREO | CH_FLAGS_STEREO_SUB | CH_FLAGS_VADPCM);
		mixer_fx15_t lvol = 0, rvol = 0;
		void *ptr = NULL;
		uint32_t pos = 0, step = 0, len = 0xFFFFFFFF, loop_len = 0;

		if (!owner->ptr) {
			// A channel with no data mixes nothing: it only runs the RSP
			// volume ramp. Keep emitting it until the ramp has decayed, then
			// drop it from the round entirely.
			if (c->silence_ns <= 0)
				continue;
			c->silence_ns -= ns;
		} else if (c->flags & CH_FLAGS_STEREO_SUB) {
			mixer_channel_volumes(ch, owner->flags, true, gvol, &lvol, &rvol);

			if (!(owner->flags & CH_FLAGS_VADPCM)) {
				// PCM interleaved: extract R from the owner's sample stream.
				flags = (owner->flags & (CH_FLAGS_BPS_SHIFT | CH_FLAGS_16BIT | CH_FLAGS_STEREO)) | CH_FLAGS_STEREO_SUB;
				pos = (uint32_t)owner->pos & 0x7FFFFFFF;
				step = (uint32_t)owner->step & 0x7FFFFFFF;
				ptr = (uint8_t*)owner->ptr + ((owner->pos & ~0x7FFFFFFF) >> MIXER_FX64_FRAC);
				mixer_rsp_bounds(owner, true, &len, &loop_len);
			} else {
				// Stereo VADPCM R: timing already synced in Phase A when streamed.
				c->pos = owner->pos;
				c->step = owner->step;
				c->len = owner->len;
				c->loop_len = owner->loop_len;
				flags = CH_FLAGS_VADPCM | CH_FLAGS_16BIT;
				pos = (uint32_t)c->pos & 0x7FFFFFFF;
				step = (uint32_t)c->step & 0x7FFFFFFF;
				ptr = c->ptr;
				if (owner->flags & (CH_FLAGS_RESIDENT | CH_FLAGS_LOOP_CACHED))
					mixer_rsp_bounds(c, true, &len, &loop_len);
			}
		} else {
			mixer_channel_volumes(ch, c->flags, false, gvol, &lvol, &rvol);
			pos = (uint32_t)c->pos & 0x7FFFFFFF;
			step = (uint32_t)c->step & 0x7FFFFFFF;

			if (c->flags & (CH_FLAGS_RESIDENT | CH_FLAGS_LOOP_CACHED)) {
				ptr = c->ptr;
				mixer_rsp_bounds(c, true, &len, &loop_len);
			} else {
				ptr = (c->flags & CH_FLAGS_VADPCM) ? c->ptr :
					(void*)((uint8_t*)c->ptr + ((c->pos & ~0x7FFFFFFF) >> MIXER_FX64_FRAC));
				// Small loops are pinned by mixer_update_loops, large ones are
				// wrapped by the CPU between rounds: either way the RSP is
				// never allowed to wrap a streamed channel.
				if (!c->loop_len)
					mixer_rsp_bounds(c, false, &len, &loop_len);
			}
		}

		if (owner->ptr)
			mixer_wait_channel_dma(ch);

		if (clear_accum) {
			flags |= CH_FLAGS_CLEAR_ACCUM;
			clear_accum = false;
		}

		mixer_emit_channel(ch, flags, lvol, rvol, pos, step, len, loop_len, ptr, ns, 0);
	}

	// All channels were silent: the accumulator must still be cleared before
	// being flushed out.
	if (clear_accum)
		mixer_emit_channel(0, CH_FLAGS_CLEAR_ACCUM, 0, 0, 0, 0, 0, 0, NULL, ns, 0);

	rspq_write(__mixer_overlay_id, MIXER_CMD_FLUSH,
		(uint32_t)ns, PhysicalAddr(out));
}

// Advance all channels by the samples just mixed, wrapping the loops that the
// RSP does not wrap by itself.
static void mixer_advance(int ns) {
	for (int i = 0; i < Mixer.hi_ch; i++) {
		mixer_channel_t *ch = &Mixer.channels[i];
		if (!ch->ptr || (ch->flags & CH_FLAGS_STEREO_SUB))
			continue;
		ch->pos += ch->step * (uint64_t)ns;
		if (ch->loop_len && ch->pos >= ch->len) {
			if (ch->flags & (CH_FLAGS_RESIDENT | CH_FLAGS_LOOP_CACHED)) {
				while (ch->pos >= ch->len)
					ch->pos -= ch->loop_len;
			} else if (!mixer_loop_fits(i)) {
				// Large loop crossing len between two rounds of the same call.
				mixer_large_loop_wrap(i);
			}
		}
		if (ch->flags & CH_FLAGS_VADPCM) {
			ch->vframe = (int)(ch->pos >> (MIXER_FX64_FRAC+4));
			if (ch->flags & CH_FLAGS_STEREO) ch[1].vframe = ch->vframe;
		}
	}
}

static void mixer_exec(int32_t *out, int num_samples) {
	PROFILE_SCOPE(PS_MIXER_EXEC) {
	tracef("mixer_exec: 0x%x samples\n", num_samples);

	mixer_fx16_t gvol = mixer_global_volume();

	rspq_highpri_begin();
	for (int offset = 0; offset < num_samples; ) {
		int ns;
		PROFILE_SCOPE(PS_MIXER_PREP) {
			mixer_update_loops();
			mixer_refresh_chtbl();
			ns = mixer_round_length(num_samples - offset);
		}
		PROFILE_SCOPE(PS_MIXER_EMIT) {
			mixer_emit_round(out + offset, ns, gvol);
		}
		PROFILE_SCOPE(PS_MIXER_ADVANCE) {
			mixer_advance(ns);
		}
		offset += ns;
	}
	rspq_highpri_end();

	mixer_prefetch_next(num_samples);

	Mixer.ticks += num_samples;
	}
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

	// Since the AI can only play an even number of samples,
	// it's not possible to call this function with an odd number,
	// otherwise buffering might become complicated / impossible.
	assert(num_samples % 2 == 0);

	// MIX_FLUSH DMAs the accumulator straight out of DMEM, so every output
	// address the RSP is given must be 8-byte aligned. That holds as long as
	// the buffer itself is aligned and we only ever advance it by an even
	// number of stereo samples, which is what the rest of this function and
	// #mixer_round_length guarantee.
	assertf(((uint32_t)out16 & 7) == 0,
		"mixer output buffer must be 8-byte aligned: %p", out16);

	// Check if the mixer is throttled. If so, do not produce more
	// than the allowance (with a small extra equal to a full audio buffer,
	// to avoid issues with fixed-size buffers like those provided by audio.c),
	// and silence after it.
	if (Mixer.throttled) {
		int extra = Mixer.sample_rate / MIXER_POLL_PER_SECOND;
		int total = num_samples;
		num_samples = (int)MIN(num_samples, Mixer.max_samples+extra) & ~1;
		Mixer.max_samples -= num_samples;
		memset(out + num_samples, 0, (total - num_samples) * sizeof(int32_t));
	}

	while (num_samples > 0) {
		mixer_event_t *e = mixer_next_event();

		// Stop at the next event, rounding the split up to an even number of
		// samples. The event then fires up to one sample late, but without
		// drifting: its schedule stays anchored to absolute ticks.
		int ns = num_samples;
		if (e) {
			int64_t delay = e->ticks - Mixer.ticks;
			ns = delay > 0 ? MIN(ns, (int)ROUND_UP(delay, 2)) : 0;
		}
		if (ns > 0) {
			mixer_exec(out, ns);
			out += ns;
			num_samples -= ns;
		}
		if (e && Mixer.ticks >= e->ticks) {
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
	PROFILE_SCOPE(PS_MIXER) {
	// To smooth out the pacing for mixer and wav64 decodes, we fill buffers
	// to at least audio_get_num_buffers()-1, but fill completely, if there is
	// only one free one left.
	int free_buffers = audio_get_num_buffers() - audio_get_queued_buffers();
	if (free_buffers <= 0)
		goto done;

	int buffers_to_fill = MAX(free_buffers - 1, 1);
	while (buffers_to_fill-- > 0) {
		short *buf = audio_write_begin();
		mixer_poll_async(buf, audio_get_buffer_length());
		audio_write_end();
	}
	rspq_flush();
done: ;
	}
}
