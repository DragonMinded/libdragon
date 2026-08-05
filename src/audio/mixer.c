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
/** @brief Fallback samplebuffer depth when #audio_init has not been called yet.
 *
 * Sizing normally follows the AI queue (#audio_get_num_buffers ×
 * #audio_get_buffer_length). This rate is only the fallback and the
 * #mixer_throttle extra, so it stays conservative.
 */
#define MIXER_POLL_PER_SECOND   8

/** @brief Polls the sample buffers are sized to hold at once.
 *
 * One is the minimum: a single #mixer_poll_async must never have to wait for
 * the RSP, whatever the number of rounds it splits into. The second one is
 * what keeps the CPU from waiting in practice, since by the time it starts a
 * poll the RSP is normally done with the one before the last (see
 * #__mixer_inflight_samples).
 */
#define MIXER_POLL_LOOKAHEAD    2

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
#define CH_FLAGS_VLOOP_STATE 	(1<<6)   ///< VADPCM: decode from the loop-start state (wire only, see mixer_emit_channel)
#define CH_FLAGS_CLEAR_ACCUM 	(1<<7)   ///< Zero ACCUM before mixing (first MIX_CHANNEL of a round)
#define CH_FLAGS_RESIDENT       (1<<8)   ///< Channel plays from waveform->mem (no samplebuffer)
#define CH_FLAGS_LOOP_CACHED    (1<<10)  ///< Streamed loop pinned in the samplebuffer; RSP wraps
#define CH_FLAGS_STEREO_ALLOC	(1<<9)   ///< The channel has a buffer sized for stereo (CPU-side only)
#define CH_FLAGS_FORCE_MONO  	(1<<11)  ///< Fold this channel's output to both buses (mono downmix). CPU-side only; RSP ucode ignores this bit.

#define MIXER_CMD_CHANNEL     0x0        ///< rspq command ID for channel setup
#define MIXER_CMD_SETCHANNEL  0x1        ///< rspq command ID for setting a channel
#define MIXER_CMD_FLUSH       0x2        ///< rspq command ID for flushing the mixer
#define MIXER_CMD_SETSTATE    0x3        ///< rspq command ID for seeding a VADPCM state
#define MIXER_CMD_COPY        0x4        ///< rspq command ID for an RDRAM to RDRAM copy

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
	mixer_fx64_t len;      ///< Active end: loop end while looping, waveform length otherwise
	mixer_fx64_t loop_len; ///< Loop length, 0 if not looping (see #mixer_ch_set_loop)
	void *ptr;             ///< Waveform data base (PCM samples or VADPCM frames)
	void *codec_state;     ///< Per-channel codec state, CPU side
	void *codebook;        ///< VADPCM codebook (NULL for PCM)
	void *loop_state;      ///< VADPCM state at loop start (NULL if none)
	uint32_t flags;        ///< Misc flags (see CH_FLAGS_*)
	waveform_t *wave;      ///< Waveform being played back on this channel
	uint32_t wave_uuid;    ///< UUID of last configured waveform (survives stop)
	int silence_ns;        ///< Samples still to be emitted while silent (volume ramp)
	int vframe;            ///< VADPCM frame the decoder state in RDRAM refers to
	uint8_t vbits;         ///< VADPCM residual width (see #VADPCM_FRAME_BYTES)
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
	uint32_t vstate_dirty;  ///< VADPCM channels the CPU has re-seeded (see #mixer_vstate)
	void *vstates;          ///< Decoder states owned by the RSP, 16 bytes per channel
	int hi_ch;              ///< Exclusive upper bound of channels to scan

	uint32_t round_id;      ///< Id of the last round emitted
	volatile uint32_t *round_done;  ///< Last round the RSP finished, written by MIX_FLUSH
	uint32_t poll_round[MIXER_POLL_LOOKAHEAD];  ///< Last round of each recent poll
	uint32_t poll_count;    ///< Polls done so far (indexes #poll_round)

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

	Mixer.vstates = malloc_uncached(num_channels * 16);
	assertf(Mixer.vstates, "Out of memory");
	memset(Mixer.vstates, 0, num_channels * 16);

	// Where the RSP publishes the last round it has run (see MIX_FLUSH). Two
	// words because that is the smallest transfer the RSP can do.
	Mixer.round_done = malloc_uncached(8);
	assertf(Mixer.round_done, "Out of memory");
	Mixer.round_done[0] = 0;
	Mixer.round_done[1] = 0;

	rspq_init();
	__mixer_overlay_id = rspq_overlay_register(&rsp_mixer);

	mixer_overlay_state_t *mixer_state = rspq_overlay_get_state(&rsp_mixer);
	memset(mixer_state, 0, sizeof(*mixer_state));
	data_cache_hit_writeback_invalidate(mixer_state, sizeof(*mixer_state));
}

/**
 * @brief The VADPCM decoder state the RSP owns for a channel (16 bytes).
 *
 * The ucode saves into it the state of the frame each round ended on, so that
 * the next one resumes from there, and it does so whenever it gets to the
 * command — which can be several rounds behind the CPU, and at a different
 * moment for each plane of a stereo pair. That makes the buffer unusable for
 * the seeds the CPU produces, so those live in a buffer of their own
 * (#mixer_channel_t.codec_state, written by the codec) and reach the RSP
 * through #mixer_emit_setstate, which orders them against those saves.
 */
static inline void *mixer_vstate(int ch)
{
	return (uint8_t*)Mixer.vstates + ch * 16;
}

/** @brief Compressed frame size of a VADPCM waveform, in bytes. */
static inline int mixer_vadpcm_frame_bytes(const waveform_t *wave)
{
	assert(wave->format == WAVEFORM_FORMAT_VADPCM && wave->codec);
	return VADPCM_FRAME_BYTES(((const waveform_vadpcm_t*)wave->codec)->bits);
}

/**
 * @brief Output samples the CPU may mix without ever waiting for the RSP.
 *
 * A poll enqueues its rounds and returns, so the input windows it handed out
 * stay live until the RSP gets to them: sample buffers are sized to span that
 * much input (#MIXER_POLL_LOOKAHEAD polls of it). The next poll waits for the
 * oldest of those before starting (see #mixer_poll_async).
 */
int __mixer_inflight_samples(void)
{
	int blen = audio_get_buffer_length();
	if (blen > 0)
		return blen * MIXER_POLL_LOOKAHEAD;
	int rate = Mixer.sample_rate ? (int)Mixer.sample_rate : audio_get_frequency();
	return rate / MIXER_POLL_PER_SECOND;
}

bool __mixer_round_done(uint32_t id)
{
	if (!Mixer.round_done)
		return true;
	return (int32_t)(*Mixer.round_done - id) >= 0;
}

void __mixer_round_wait(uint32_t id)
{
	if (__mixer_round_done(id))
		return;
	rspq_flush();
	ACCT_SCOPE(ACCT_CAT_RSPQ) RSP_WAIT_LOOP(200) {
		if (__mixer_round_done(id))
			break;
	}
}

/** True if starting another poll would have the CPU wait for the RSP. */
static bool mixer_poll_would_wait(void)
{
	if (Mixer.poll_count < MIXER_POLL_LOOKAHEAD)
		return false;
	return !__mixer_round_done(Mixer.poll_round[Mixer.poll_count % MIXER_POLL_LOOKAHEAD]);
}

/** Wait until starting another poll would not overwrite live sample windows. */
static void mixer_poll_barrier(void)
{
	if (Mixer.poll_count < MIXER_POLL_LOOKAHEAD)
		return;
	__mixer_round_wait(Mixer.poll_round[Mixer.poll_count % MIXER_POLL_LOOKAHEAD]);
}

static int mixer_calc_buffer_size(int ch, waveform_t *wave)
{
	bool vadpcm = wave->format == WAVEFORM_FORMAT_VADPCM;
	int ub = vadpcm ? mixer_vadpcm_frame_bytes(wave)
		: (Mixer.limits[ch].max_bits / 8) * wave->channels;

	int64_t out_samples = __mixer_inflight_samples();
	int64_t nsamples = (int64_t)ceilf((float)out_samples *
		(Mixer.limits[ch].max_frequency / (float)Mixer.sample_rate));

	int64_t nunits = vadpcm ? DIVIDE_CEIL(nsamples, 16) : nsamples;

	// Plus what sits between the oldest window the RSP still has to read and
	// the write cursor: the window of the round being emitted now, and the
	// units #samplebuffer_prefetch has already pulled in past it. Both are
	// capped at a margin.
	nunits += SAMPLEBUFFER_MARGIN_UNITS * 2;

	// Block codecs (Opus, ULC) append one whole frame at a time, however few
	// samples were asked for, so the ring needs room for one more of those on
	// top of everything above. The mirrored tail is also enlarged to cover a
	// whole frame (see #samplebuffer_t::margin_units); that extra is allocated
	// by the caller (#mixer_ch_play), not here.
	int f = wave->append_units;
	if (f > SAMPLEBUFFER_MARGIN_UNITS)
		nunits += f;

	if (nunits < SAMPLEBUFFER_MARGIN_UNITS * 2)
		nunits = SAMPLEBUFFER_MARGIN_UNITS * 2;

	int64_t size = ROUND_UP(nunits * ub, 8);

	if (Mixer.limits[ch].max_buf_sz && size > Mixer.limits[ch].max_buf_sz)
		size = Mixer.limits[ch].max_buf_sz;

	// samplebuffer needs ≥MARGIN units of usable space plus the mirrored tail.
	// The mirrored tail itself is allocated by the caller (#mixer_ch_play).
	int min_bytes = SAMPLEBUFFER_MARGIN_UNITS * 2 * ub;
	if (size < min_bytes)
		size = min_bytes;

	assert((size % 8) == 0);
	assert((int32_t)size == size);

	return size;
}

/** Mirrored-tail size in bytes for a waveform (see #samplebuffer_t::margin_units). */
static int mixer_margin_bytes(waveform_t *wave, int ub)
{
	return samplebuffer_margin_units(wave->append_units) * ub;
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

	if (Mixer.vstates) {
		free_uncached(Mixer.vstates);
		Mixer.vstates = NULL;
	}

	if (Mixer.round_done) {
		free_uncached((void*)Mixer.round_done);
		Mixer.round_done = NULL;
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

/** Exclusive end of the loop region of a waveform (0 means terminal loop). */
static int waveform_loop_end(const waveform_t *wave) {
	return wave->loop_end ? wave->loop_end : wave->len;
}

/**
 * @brief Set the bounds a channel plays within: the loop region, or all of it.
 *
 * While looping, the channel ends at the loop end and wraps back by loop_len;
 * otherwise it runs to the physical end of the waveform (which includes the
 * release tail of a waveform whose loop ends before it).
 */
static void mixer_ch_bounds(mixer_channel_t *c, const waveform_t *wave, bool loop) {
	int bps = (c->flags & CH_FLAGS_VADPCM) ? 0 : (c->flags & CH_FLAGS_BPS_SHIFT);
	loop = loop && wave->loop_len;
	c->len = MIXER_FX64((int64_t)(loop ? waveform_loop_end(wave) : wave->len)) << bps;
	c->loop_len = loop ? MIXER_FX64((int64_t)wave->loop_len) << bps : 0;
	// The secondary channel of a stereo pair is played with the same bounds.
	if (c->flags & CH_FLAGS_STEREO) {
		c[1].len = c->len;
		c[1].loop_len = c->loop_len;
	}
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
	// A seek that restarts the ring is the CPU moving playback where the
	// stream does not go on its own, so the state the codec writes for it has
	// to reach the RSP (see #mixer_emit_setstate). The refill that follows the
	// end of a looping waveform arrives as a seek too, but there the ring
	// keeps its live window: the RSP is still decoding the frames before the
	// loop, and the state of the loop start only becomes the one it needs once
	// the CPU wraps the position.
	if (seeking && sbuf->widx == 0 && wave->format == WAVEFORM_FORMAT_VADPCM) {
		int ch = sbuf - Mixer.ch_buf;
		Mixer.vstate_dirty |= mixer_bit(ch);
		if (Mixer.channels[ch].flags & CH_FLAGS_STEREO)
			Mixer.vstate_dirty |= mixer_bit(ch+1);
	}

	// Bounds come from the channel, not from the waveform: they follow its
	// loop state (see #mixer_ch_set_loop), so a voice in release reads through
	// to the sample end while another one on the same waveform still loops.
	mixer_channel_t *c = &Mixer.channels[sbuf - Mixer.ch_buf];
	int bps_fx64 = ((c->flags & CH_FLAGS_VADPCM) ? 0 : (c->flags & CH_FLAGS_BPS_SHIFT)) + MIXER_FX64_FRAC;
	int wave_len = c->len >> bps_fx64;
	int wave_loop = c->loop_len >> bps_fx64;

	// Samplebuffer units: PCM samples, or VADPCM frames. Wave metadata is
	// always in samples; convert bounds when the buffer stores frames.
	if (wave->format == WAVEFORM_FORMAT_VADPCM) {
		// The last frame of a waveform is partial whenever its length is not a
		// multiple of 16, and the loop point can sit anywhere inside a frame:
		// both bounds have to become the frame the sample belongs to, or the
		// samples living in those partial frames would never be fetched.
		int loop_start = (wave_len - wave_loop) / 16;
		wave_len = DIVIDE_CEIL(wave_len, 16);
		wave_loop = wave_loop ? wave_len - loop_start : 0;
	}
	int ub = sbuf->unit_bytes;
	// Silence past the end: the codec read fills both VADPCM planes, so the
	// padding must too (and at the same wpos), or the right ring falls short
	// / misaligns and samplebuffer_get tries to extend it without a reader.
	bool stereo_vadpcm = (c->flags & (CH_FLAGS_VADPCM | CH_FLAGS_STEREO))
		== (CH_FLAGS_VADPCM | CH_FLAGS_STEREO);

	if (wpos >= wave_len) {
		if (!wave_loop) {
			memset(samplebuffer_append(sbuf, wlen), 0, wlen * ub);
			if (stereo_vadpcm) {
				samplebuffer_t *r = sbuf + 1;
				if (r->widx == 0) r->wpos = sbuf->wpos;
				memset(samplebuffer_append(r, wlen), 0, wlen * ub);
				r->wnext = r->wpos + r->widx;
			}
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
		memset(samplebuffer_append(sbuf, len2), 0, len2 * ub);
		if (stereo_vadpcm) {
			samplebuffer_t *r = sbuf + 1;
			if (r->widx == 0) r->wpos = sbuf->wpos;
			memset(samplebuffer_append(r, len2), 0, len2 * ub);
			r->wnext = r->wpos + r->widx;
		}
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

/**
 * @brief Go back to streaming a loop that was pinned in the samplebuffer.
 *
 * The pinned copy is addressed relative to the loop region and is wrapped by
 * the RSP, so it only survives while playback stays inside it. Dropping it
 * empties the ring, which makes the next fetch a seek: that is also what
 * re-seeds the decoder state.
 */
static void mixer_ch_unpin_loop(int ch) {
	mixer_channel_t *c = &Mixer.channels[ch];
	c->flags &= ~CH_FLAGS_LOOP_CACHED;
	if ((c->flags & CH_FLAGS_VADPCM) && (c->flags & CH_FLAGS_STEREO)) {
		Mixer.channels[ch+1].flags &= ~CH_FLAGS_LOOP_CACHED;
		samplebuffer_flush(&Mixer.ch_buf[ch+1]);
	}
	samplebuffer_flush(&Mixer.ch_buf[ch]);
	mixer_refresh_max_ns(ch);
}

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
	int ch = c - Mixer.channels;
	Mixer.vstate_dirty |= mixer_bit(ch);
	if (c->flags & CH_FLAGS_STEREO) {
		c[1].vframe = c->vframe;
		Mixer.vstate_dirty |= mixer_bit(ch+1);
	}
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
		mixer_ch_unpin_loop(ch);
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
	// Check if the sample / state buffer is big enough for this waveform.
	// A channel that first played a raw/VADPCM SFX may hold a ring sized
	// without Opus/ULC append headroom; keep that from wrapping onto RSP-live
	// samples when a block codec is played later.
	if (!resident && samplebuffer_is_inited(sbuf)) {
		int need = ROUND_UP(mixer_calc_buffer_size(ch, wave), 16);
		int ub = vadpcm ? mixer_vadpcm_frame_bytes(wave)
			: ((wave->bits / 8) * (stereo_vadpcm ? 1 : wave->channels));
		need += mixer_margin_bytes(wave, ub);
		need = ROUND_UP(need, 16);
		if (sbuf->capacity_bytes < need || sbuf->state_size < wave->state_size) {
			rspq_highpri_sync();
			samplebuffer_close(sbuf);
			if (stereo_vadpcm) samplebuffer_close(&Mixer.ch_buf[ch+1]);
		}
	}

	if (!resident && !samplebuffer_is_inited(sbuf)) {
		int size = ROUND_UP(mixer_calc_buffer_size(ch, wave), 16);
		int ub = vadpcm ? mixer_vadpcm_frame_bytes(wave) : ((wave->bits / 8) * (stereo_vadpcm ? 1 : wave->channels));
		size += mixer_margin_bytes(wave, ub);
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
		assertf(wave->loop_len >= 0 && wave->loop_len <= waveform_loop_end(wave) && waveform_loop_end(wave) <= wave->len,
			"waveform %s: invalid loop of %d samples ending at %d (len %d)",
			wave->name, wave->loop_len, waveform_loop_end(wave), wave->len);

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
				samplebuffer_set_unit_bytes(sbuf, mixer_vadpcm_frame_bytes(wave));
				if (stereo_vadpcm)
					samplebuffer_set_unit_bytes(&Mixer.ch_buf[ch+1], mixer_vadpcm_frame_bytes(wave));
			} else {
				samplebuffer_set_bps(sbuf, wave->bits*wave->channels);
			}
			samplebuffer_set_waveform(sbuf, wave, wave->read ? waveform_read : NULL);
			c->codec_state = sbuf->state;
		}

		if (wave->format == WAVEFORM_FORMAT_VADPCM) {
			waveform_vadpcm_t *vc = wave->codec;
			assertf(vc && vc->codebook, "waveform %s: VADPCM missing codec/codebook", wave->name);
			assertf(vc->bits >= 2 && vc->bits <= 4, "waveform %s: invalid VADPCM residual width %d", wave->name, vc->bits);
			c->flags |= CH_FLAGS_VADPCM | CH_FLAGS_16BIT;
			if (wave->channels == 2) c->flags |= CH_FLAGS_STEREO;
			c->codebook = vc->codebook;
			c->loop_state = vc->loop_state;
			c->vbits = vc->bits;
			if (stereo_vadpcm) {
				mixer_channel_t *r = &Mixer.channels[ch+1];
				r->flags = (r->flags & CH_FLAGS_FORCE_MONO) | CH_FLAGS_STEREO_SUB | CH_FLAGS_VADPCM | CH_FLAGS_16BIT;
				r->codebook = (uint8_t*)vc->codebook + VADPCM_CODEBOOK_STRIDE;
				r->vbits = vc->bits;
				r->codec_state = (uint8_t*)c->codec_state + 16;
				r->loop_state = vc->loop_state ? (uint8_t*)vc->loop_state + 16 : NULL;
				r->wave = wave;
				r->wave_uuid = wave->__uuid;
			}
		} else {
			int bps = (wave->bits == 16 ? 1 : 0) + (wave->channels == 2 ? 1 : 0);
			c->flags |= bps | (wave->channels == 2 ? CH_FLAGS_STEREO : 0) | (wave->bits == 16 ? CH_FLAGS_16BIT : 0);
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

	// Restart from the beginning of the waveform, with the loop armed again:
	// a #mixer_ch_set_loop only applies to the note that was playing.
	c->wave = wave;
	mixer_ch_bounds(c, wave, true);
	if (resident && stereo_vadpcm) {
		// Planes are laid out back to back, each holding every frame that
		// carries a sample (the last one is partial if len is not a multiple
		// of 16).
		int nframes = DIVIDE_CEIL(wave->len, 16);
		c->ptr = (void*)wave->mem;
		Mixer.channels[ch+1].ptr = (uint8_t*)wave->mem + nframes * mixer_vadpcm_frame_bytes(wave);
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
	// The state buffer of a channel outlives the waveforms played on it, so a
	// new one always has to seed it, even when it starts where the previous
	// one happened to be.
	if (c->flags & CH_FLAGS_VADPCM) {
		Mixer.chtbl_dirty |= mixer_bit(ch);
		Mixer.vstate_dirty |= mixer_bit(ch);
	}
	if (c->flags & CH_FLAGS_STEREO) {
		mixer_touch_ch(ch+1);
		if (c->flags & CH_FLAGS_VADPCM) {
			Mixer.chtbl_dirty |= mixer_bit(ch+1);
			Mixer.vstate_dirty |= mixer_bit(ch+1);
		}
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

void mixer_ch_set_loop(int ch, bool enable) {
	mixer_channel_t *c = &Mixer.channels[ch];
	assertf(!(c->flags & CH_FLAGS_STEREO_SUB), "mixer_ch_set_loop: cannot call on secondary stereo channel %d", ch);
	assertf(c->wave, "mixer_ch_set_loop: channel %d is not playing", ch);
	assertf(!enable || c->wave->loop_len, "mixer_ch_set_loop: waveform %s does not loop", c->wave->name);
	if (enable == (c->loop_len != 0))
		return;

	mixer_ch_bounds(c, c->wave, enable);
	// The ring holds the loop unrolled: whatever it cached past the loop end
	// was taken from the loop start, and a pinned loop is not even addressed
	// linearly. Drop it and resume streaming from the current position.
	if (!(c->flags & CH_FLAGS_RESIDENT))
		mixer_ch_unpin_loop(ch);
	tracef("mixer_ch_set_loop: ch=%d enable=%d len=%llx\n", ch, enable, (uint64_t)c->len);
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
		Mixer.vstate_dirty &= ~mixer_bit(ch+1);
	}

	c->ptr = 0;
	c->pos = 0;
	c->silence_ns = MIXER_SILENCE_RAMP_SAMPLES;
	Mixer.chtbl_dirty &= ~mixer_bit(ch);
	Mixer.vstate_dirty &= ~mixer_bit(ch);

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
		// The waveform that was playing has to be configured again on the new
		// buffer, so forget it: mixer_ch_play skips that work when it sees the
		// same waveform it already has.
		Mixer.channels[ch].wave_uuid = 0;
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

/** @brief Emit a MIX_CHANNEL rspq command for one channel.
 *
 * @p nsamples is latched by the ucode into DMEM (same value for every channel
 * of a round). It must be even and at most #MIXER_MAX_SAMPLES_PER_ROUND so that
 * nsamples/2 fits the 9-bit field in a0.
 */
static void mixer_emit_channel(int ch, uint32_t flags, mixer_fx15_t lvol, mixer_fx15_t rvol,
	uint32_t pos, uint32_t step, uint32_t len, uint32_t loop_len, void *ptr,
	int nsamples)
{
	assert(ch >= 0 && ch < MIXER_MAX_CHANNELS);
	assert((nsamples & 1) == 0 && nsamples >= 0 && nsamples <= MIXER_MAX_SAMPLES_PER_ROUND);
	rspq_write_t w = rspq_write_begin(__mixer_overlay_id, MIXER_CMD_CHANNEL, 7);
	// a0: ch<<19 | flags<<11 | (nsamples/2)   [5|8|9 bits in the 24-bit payload]
	rspq_write_arg(&w, ((uint32_t)ch << 19) | ((flags & 0xFF) << 11) | ((uint32_t)nsamples >> 1));
	rspq_write_arg(&w, ((uint32_t)(uint16_t)lvol << 16) | (uint16_t)rvol);
	rspq_write_arg(&w, pos);
	rspq_write_arg(&w, step);
	rspq_write_arg(&w, len);
	rspq_write_arg(&w, loop_len);
	rspq_write_arg(&w, ptr ? PhysicalAddr(ptr) : 0);
	rspq_write_end(&w);
}

/**
 * @brief Record a channel's VADPCM pointers in the ucode channel table.
 *
 * The codebook is 8-byte aligned, so the residual width travels in its two low
 * bits (as `bits-2`) instead of costing a command word: the ucode needs it to
 * build the unpacking constants, and it never changes without the codebook
 * changing too.
 */
static void mixer_emit_setchannel(int ch, void *codebook, void *state, void *loop_state, int bits)
{
	assert(!codebook || ((PhysicalAddr(codebook) & 7) == 0 && bits >= 2 && bits <= 4));
	rspq_write(__mixer_overlay_id, MIXER_CMD_SETCHANNEL, (uint32_t)ch << 16,
		codebook ? PhysicalAddr(codebook) | (bits - 2) : 0,
		state ? PhysicalAddr(state) : 0,
		loop_state ? PhysicalAddr(loop_state) : 0);
}

/**
 * @brief Hand a decoder state the CPU re-seeded over to the RSP.
 *
 * Sent as a command rather than written to #mixer_vstate directly, so that it
 * lands after the rounds already enqueued (whose epilog writes that same
 * buffer) and before the one that decodes from it.
 */
static void mixer_emit_setstate(int ch)
{
	const uint32_t *src = Mixer.channels[ch].codec_state;
	rspq_write(__mixer_overlay_id, MIXER_CMD_SETSTATE, 0,
		PhysicalAddr(mixer_vstate(ch)), src[0], src[1], src[2], src[3]);
}

/** @brief Largest block MIX_COPY can move at once (the ucode's sample cache). */
#define MIXER_COPY_MAX_BYTES  320

bool __mixer_rdram_copy(void *dst, void *src, int nbytes)
{
	if (!__mixer_overlay_id)
		return false;
	assert(((PhysicalAddr(dst) | PhysicalAddr(src) | nbytes) & 7) == 0);

	rspq_highpri_begin();
	while (nbytes > 0) {
		int n = MIN(nbytes, MIXER_COPY_MAX_BYTES);
		rspq_write(__mixer_overlay_id, MIXER_CMD_COPY, n,
			PhysicalAddr(dst), PhysicalAddr(src));
		dst = (uint8_t*)dst + n;
		src = (uint8_t*)src + n;
		nbytes -= n;
	}
	rspq_highpri_end();
	return true;
}

// Ring space, in units, that pinning a region of "units" samples actually
// consumes: the region itself plus the overread past its end, both rounded to
// whole appends because a block codec (Opus, ULC) always writes full frames.
// Notably the overread is only a handful of samples but still costs a whole
// frame, so it cannot be folded into the region's own rounding.
static int mixer_pin_span(const samplebuffer_t *sbuf, int units) {
	int overread = DIVIDE_CEIL(MIXER_LOOP_OVERREAD, sbuf->unit_bytes) + 1;
	int f = sbuf->append_units;
	if (f > SAMPLEBUFFER_MARGIN_UNITS)
		return ROUND_UP(units, f) + ROUND_UP(overread, f);
	return units + overread;
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
	int slen = vadpcm ? DIVIDE_CEIL(len, 16) : len;
	return slen > 0 && mixer_pin_span(sbuf, slen) <= sbuf->size;
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
	// In frames, the pinned region spans from the frame holding its first
	// sample to the one holding its last: a loop point is not necessarily
	// aligned to a frame, so this is not just the length rounded up.
	int sloop_start = vadpcm ? cache_start / 16 : cache_start;
	int sloop_len = vadpcm ? DIVIDE_CEIL(cache_start + cache_len, 16) - sloop_start : cache_len;
	int overread = (MIXER_LOOP_OVERREAD + ub - 1) / ub;
	// What the producer will really write, which for a block codec is more
	// than what is asked for here.
	int fill = mixer_pin_span(sbuf, sloop_len);
	assertf(fill <= sbuf->size, "ch:%d loop cache %x > samplebuffer %x", ch, fill, sbuf->size);
	assert(wave && wave->read);

	// The pinned copy has to be one contiguous run, which only the top of the
	// ring can guarantee: this is the one flush that cannot restart forward,
	// so the rounds that are still reading the ring have to be drained first.
	// It only happens when a channel starts looping.
	rspq_highpri_sync();
	samplebuffer_flush(sbuf);
	sbuf->wpos = 0;
	sbuf->wnext = 0;
	sbuf->head = 0;
	if (stereo_vadpcm) {
		samplebuffer_t *sbuf_r = &Mixer.ch_buf[ch+1];
		int fill_r = mixer_pin_span(sbuf_r, sloop_len);
		assertf(fill_r <= sbuf_r->size, "ch:%d R loop cache %x > samplebuffer %x", ch+1, fill_r, sbuf_r->size);
		samplebuffer_flush(sbuf_r);
		sbuf_r->wpos = 0;
		sbuf_r->wnext = 0;
		sbuf_r->head = 0;
	}
	// Go through the wrapper: the overread past the loop end must be filled
	// with the samples at the loop start, which is exactly what the RSP reads
	// there once it starts wrapping the pinned copy on its own.
	waveform_read(wave->ctx, sbuf, sloop_start, sloop_len + overread, true);
	sbuf->wpos = sloop_start;
	sbuf->wnext = sloop_start + sbuf->widx;
	if (stereo_vadpcm) {
		samplebuffer_t *sbuf_r = &Mixer.ch_buf[ch+1];
		sbuf_r->wpos = sloop_start;
		sbuf_r->wnext = sloop_start + sbuf_r->widx;
		Mixer.channels[ch+1].ptr = (uint8_t*)SAMPLES_PTR(sbuf_r) - sloop_start * VADPCM_FRAME_BYTES(c->vbits);
		Mixer.channels[ch+1].flags |= CH_FLAGS_LOOP_CACHED;
	}

	if (vadpcm)
		c->ptr = (uint8_t*)SAMPLES_PTR(sbuf) - sloop_start * VADPCM_FRAME_BYTES(c->vbits);
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
	int sloop = vadpcm ? DIVIDE_CEIL(loop_len, 16) : loop_len;
	return sloop > 0 && mixer_pin_span(sbuf, sloop) <= sbuf->size;
}

// Wrap a large loop: rebase the position within the loop and restart the
// stream there. Must be done between rounds, as the RSP never wraps these.
static void mixer_large_loop_wrap(int i) {
	mixer_channel_t *ch = &Mixer.channels[i];
	bool vadpcm = (ch->flags & CH_FLAGS_VADPCM) != 0;
	int bps_fx64 = (vadpcm ? 0 : (ch->flags & CH_FLAGS_BPS_SHIFT)) + MIXER_FX64_FRAC;
	int wpos = ch->pos >> bps_fx64;
	// A round can stop a few samples past the loop point (see
	// #mixer_round_length), and those samples have already been mixed, out of
	// the overread that #waveform_read fills from the loop start. Resuming
	// right there is what keeps the loop period exact, which short tracker
	// loops need to stay in tune. A codec that only restarts on its seek
	// points cannot do that, so for those we go back to the loop start and
	// let the handful of samples play twice.
	waveform_t *wave = Mixer.ch_buf[i].wave;
	int wpos2 = (wave && wave->loop_restart_only)
		? (ch->len - ch->loop_len) >> bps_fx64
		: waveform_wrap_wpos(wpos, ch->len >> bps_fx64, ch->loop_len >> bps_fx64);
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
		void *state = mixer_vstate(ch);
		*sh = (mixer_chtbl_t){ c->codebook, state, c->loop_state };
		mixer_emit_setchannel(ch, c->codebook, state, c->loop_state, c->vbits);
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
 * One unit is reserved on top of the overread because #mixer_channel_window
 * counts an inclusive span (last-pos+1), which can be one past the position
 * advance that this cap is derived from when the fractional part of pos is
 * large relative to step. For VADPCM that same reserve also absorbs the
 * ceiling to frames: a span of N samples starting mid-frame covers
 * ceil((pos%16+N)/16) frames, one more than N/16 when pos is not aligned.
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
	int max_wlen = MAX(SAMPLEBUFFER_MARGIN_UNITS - overread - 1, 1);
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

// Byte offset that the CPU folds into the pointer sent to the RSP. The ucode
// only receives the low 31 bits of the position, so a streamed channel (whose
// position grows with the stream instead of wrapping) must carry everything
// above that bit in the base pointer. VADPCM positions count samples, and the
// ucode addresses one compressed frame every 16 of them.
static int32_t mixer_pos_fold(const mixer_channel_t *c) {
	int64_t high = c->pos & ~(int64_t)0x7FFFFFFF;
	if (c->flags & CH_FLAGS_VADPCM)
		return (high >> (MIXER_FX64_FRAC+4)) * VADPCM_FRAME_BYTES(c->vbits);
	return high >> MIXER_FX64_FRAC;
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

#ifndef NDEBUG
	// Every window handed to a round stays live until the RSP runs it. A ring
	// that does not span a whole poll would overwrite its own windows: the
	// poll barrier (see #mixer_poll_barrier) only frees space from older polls.
	// Waveforms that end up entirely in the ring never refill: exempt.
	if (!mixer_wave_fits(ch) && !mixer_loop_fits(ch)) {
		int poll_ns = __mixer_inflight_samples() / MIXER_POLL_LOOKAHEAD;
		bool vadpcm = (c->flags & CH_FLAGS_VADPCM) != 0;
		int bps_fx64 = (vadpcm ? 0 : (c->flags & CH_FLAGS_BPS_SHIFT)) + MIXER_FX64_FRAC;
		uint64_t span = ((uint64_t)poll_ns * c->step) >> bps_fx64;
		int need = (vadpcm ? (int)DIVIDE_CEIL(span, 16) : (int)span)
			+ SAMPLEBUFFER_PREFETCH_UNITS
			+ DIVIDE_CEIL(MIXER_LOOP_OVERREAD, sbuf->unit_bytes)
			+ sbuf->append_units;
		assertf(sbuf->size >= need,
			"ch%d: sample buffer holds %d units, %d needed to play at %.0f Hz "
			"for the %d samples of a poll; raise max_buf_sz in "
			"mixer_ch_set_limits (or convert the asset again)",
			ch, sbuf->size, need,
			(double)c->step * Mixer.sample_rate / (double)(1ull << bps_fx64),
			poll_ns);
	}
#endif

	if (c->flags & CH_FLAGS_VADPCM) {
		if (sbuf->widx == 0 || wpos < sbuf->wpos || wpos > sbuf->wpos + sbuf->widx)
			c->vframe = wpos;
		void *p = samplebuffer_get(sbuf, wpos, &wlen);
		assert(p);
		c->ptr = (uint8_t*)p - wpos * VADPCM_FRAME_BYTES(c->vbits);
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

	// The windows fetched below belong to this round, and stay live until the
	// RSP publishes its id back (see MIX_FLUSH / #__mixer_round_done).
	uint32_t round_id = ++Mixer.round_id;

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
				ptr = (uint8_t*)owner->ptr + mixer_pos_fold(owner);
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
				if (owner->flags & (CH_FLAGS_RESIDENT | CH_FLAGS_LOOP_CACHED)) {
					ptr = c->ptr;
					mixer_rsp_bounds(c, true, &len, &loop_len);
				} else {
					ptr = (uint8_t*)c->ptr + mixer_pos_fold(c);
				}
			}
		} else {
			mixer_channel_volumes(ch, c->flags, false, gvol, &lvol, &rvol);
			pos = (uint32_t)c->pos & 0x7FFFFFFF;
			step = (uint32_t)c->step & 0x7FFFFFFF;

			if (c->flags & (CH_FLAGS_RESIDENT | CH_FLAGS_LOOP_CACHED)) {
				ptr = c->ptr;
				mixer_rsp_bounds(c, true, &len, &loop_len);
			} else {
				ptr = (uint8_t*)c->ptr + mixer_pos_fold(c);
				// Small loops are pinned by mixer_update_loops, large ones are
				// wrapped by the CPU between rounds: either way the RSP is
				// never allowed to wrap a streamed channel.
				if (!c->loop_len)
					mixer_rsp_bounds(c, false, &len, &loop_len);
			}
		}

		if (owner->ptr)
			mixer_wait_channel_dma(ch);

		// A state the CPU re-seeded rides with the first round that needs it,
		// which is where it belongs in the queue.
		if (Mixer.vstate_dirty & mixer_bit(ch)) {
			Mixer.vstate_dirty &= ~mixer_bit(ch);
			if ((c->flags & CH_FLAGS_VADPCM) && c->codec_state)
				mixer_emit_setstate(ch);
		}

		// A command that starts on the frame the loop starts on must decode
		// from the state saved for that frame: the one the stream left behind
		// belongs to the end of the waveform, since it is the CPU that wraps
		// the position between rounds. The ucode has no cheap way to tell,
		// while here it is a comparison on values already at hand.
		if ((flags & CH_FLAGS_VADPCM) && loop_len && Mixer.chtbl[ch].loop_state &&
			(pos >> (MIXER_FX64_FRAC+4)) == ((len - loop_len) >> (MIXER_FX64_FRAC+4)))
			flags |= CH_FLAGS_VLOOP_STATE;

		if (clear_accum) {
			flags |= CH_FLAGS_CLEAR_ACCUM;
			clear_accum = false;
		}

		mixer_emit_channel(ch, flags, lvol, rvol, pos, step, len, loop_len, ptr, ns);
	}

	// All channels were silent: the accumulator must still be cleared before
	// being flushed out.
	if (clear_accum)
		mixer_emit_channel(0, CH_FLAGS_CLEAR_ACCUM, 0, 0, 0, 0, 0, 0, NULL, ns);

	rspq_write(__mixer_overlay_id, MIXER_CMD_FLUSH,
		(uint32_t)ns, PhysicalAddr(out), round_id,
		PhysicalAddr((void*)Mixer.round_done));
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

/** @brief Poll the mixer asynchronously. */
void mixer_poll_async(int16_t *out16, int num_samples) {
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

	// Sample buffers span #MIXER_POLL_LOOKAHEAD polls. Wait for the oldest
	// still-live one before starting, so a refill cannot overwrite windows
	// the RSP has not read yet. The first LOOKAHEAD polls never wait.
	mixer_poll_barrier();

	assertf(num_samples <= __mixer_inflight_samples(),
		"cannot mix %d samples in one go: sample buffers are sized for %d",
		num_samples, __mixer_inflight_samples());

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

	// Remember where this poll ended, so that #mixer_poll_would_wait /
	// #mixer_poll_barrier can tell whether the ring space of the polls
	// before it has been freed.
	Mixer.poll_round[Mixer.poll_count % MIXER_POLL_LOOKAHEAD] = Mixer.round_id;
	Mixer.poll_count++;
}

void mixer_poll(int16_t *out16, int num_samples) {
	// Sample buffers only span what a poll can queue (see
	// #__mixer_inflight_samples), so a caller asking for more than that in one
	// go would wrap onto windows still live from earlier in the same call.
	// Sync along the way instead, which costs nothing for the usual sizes.
	int budget = __mixer_inflight_samples() & ~1;
	while (num_samples > budget) {
		mixer_poll_async(out16, budget);
		rspq_highpri_sync();
		out16 += budget * 2;
		num_samples -= budget;
	}
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

	int queued = audio_get_queued_buffers();
	int buffers_to_fill = MAX(free_buffers - 1, 1);
	while (buffers_to_fill-- > 0) {
		// Filling one more buffer now would mean waiting for the RSP to free
		// the ring space of an older poll. There is already mixed audio
		// queued, so leave the rest for the next frame rather than spending
		// it here. With nothing queued the wait is unavoidable: the AI needs
		// samples now.
		if (queued > 1 && mixer_poll_would_wait())
			break;
		short *buf = audio_write_begin();
		mixer_poll_async(buf, audio_get_buffer_length());
		audio_write_end();
		queued++;
	}
	rspq_flush();
done: ;
	}
}
