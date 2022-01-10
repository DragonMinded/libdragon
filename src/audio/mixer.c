#include "libdragon.h"
#include "regsinternal.h"
#include "mixer.h"
#include "utils.h"
#include <memory.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define MIXER_TRACE   0

#if MIXER_TRACE
#define tracef(fmt, ...)  debugf(fmt, ##__VA_ARGS__)
#else
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

#define MAX_EVENTS              32
#define MIXER_POLL_PER_SECOND   8

/**
 * RSP mixer ucode (rsp_mixer.S)
 */
DEFINE_RSP_UCODE(rsp_mixer);

// NOTE: keep these in sync with rsp_mixer.S
#define CH_FLAGS_BPS_SHIFT  (3<<0)   // BPS shift value
#define CH_FLAGS_16BIT      (1<<2)   // Set if the channel is 16 bit
#define CH_FLAGS_STEREO     (1<<3)   // Set if the channel is stereo (left)
#define CH_FLAGS_STEREO_SUB (1<<4)   // The channel is the second half of a stereo (right)

// Fixed point value used in waveform position calculations. This is a signed
// 64-bit integer with the fractional part using MIXER_FX64_FRAC bits.
// You can use MIXER_FX64() to convert from float.
typedef uint64_t mixer_fx64_t;

// Fixed point value used for volume and panning calculations.
// You can use MIXER_FX15() to convert from float.
typedef int16_t mixer_fx15_t;


#define MIXER_FX64_FRAC    12    // NOTE: this must be the same of WAVERFORM_POS_FRAC_BITS in rsp_mixer.S
#define MIXER_FX64(f)      (int64_t)((f) * (1<<MIXER_FX64_FRAC))

#define MIXER_FX15_FRAC    15
#define MIXER_FX15(f)      (int16_t)((f) * ((1<<MIXER_FX15_FRAC)-1))

#define MIXER_FX16_FRAC    16
#define MIXER_FX16(f)      (int16_t)((f) * ((1<<MIXER_FX16_FRAC)-1))

typedef struct mixer_channel_s {
	/* Current position within the waveform (in bytes) */
	mixer_fx64_t pos;
	/* Step between samples (in bytes) to playback at the correct frequency */
	mixer_fx64_t step;
	/* Length of the waveform (in bytes) */
	mixer_fx64_t len;
	/* Length of the loop in the waveform (in bytes) */
	mixer_fx64_t loop_len;
	/* Pointer to the waveform */
	void *ptr;
	/* Misc flags */
	uint32_t flags;
} mixer_channel_t;

typedef struct rsp_mixer_channel_s {
	uint32_t pos;
	uint32_t step;
	uint32_t len;
	uint32_t loop_len;
	void *ptr;
	uint32_t flags;
} __attribute__((packed)) rsp_mixer_channel_t;

_Static_assert(sizeof(rsp_mixer_channel_t) == 6*4);

typedef struct {
	int max_bits;
	float max_frequency;
	int max_buf_sz;
} channel_limit_t;

typedef struct {
	int64_t ticks;
	MixerEvent cb;
	void *ctx;
} mixer_event_t;

struct {
	uint32_t sample_rate;
	int num_channels;
	float divider;
	float vol;

	int64_t ticks;
	int num_events;
	mixer_event_t events[MAX_EVENTS];

	uint8_t *ch_buf_mem;
	samplebuffer_t ch_buf[MIXER_MAX_CHANNELS];
	channel_limit_t limits[MIXER_MAX_CHANNELS];

	mixer_channel_t channels[MIXER_MAX_CHANNELS];
	mixer_fx15_t lvol[MIXER_MAX_CHANNELS];
	mixer_fx15_t rvol[MIXER_MAX_CHANNELS];

	// Permanent state of the ucode across different executions
	uint8_t ucode_state[128] __attribute__((aligned(8)));

} Mixer;

/** @brief Count of ticks spent in mixer RSP, used for debugging purposes. */
int64_t __mixer_profile_rsp = 0;

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
}

static void mixer_init_samplebuffers(void) {
	// Initialize the samplebuffers. This is done lazily so to allow the
	// client to configure the limits of the channels.
	int totsize = 0;
	int bufsize[MIXER_MAX_CHANNELS];

	for (int i=0;i<Mixer.num_channels;i++) {
		// Get maximum frequency for this channel
		int nsamples = Mixer.limits[i].max_frequency;

		// Multiple by maximum byte per sample
		nsamples *= Mixer.limits[i].max_bits / 8;

		// Calculate buffer size according to number of expected polls per second.
		bufsize[i] = ROUND_UP((int)ceilf((float)nsamples / (float)MIXER_POLL_PER_SECOND), 8);

		// If we're over the allowed maximum, clamp to it
		if (Mixer.limits[i].max_buf_sz && bufsize[i] > Mixer.limits[i].max_buf_sz)
			bufsize[i] = Mixer.limits[i].max_buf_sz;

		assert((bufsize[i] % 8) == 0);
		totsize += bufsize[i];
	}

	// Do one large allocations for all sample buffers
	assert(Mixer.ch_buf_mem == NULL);
	Mixer.ch_buf_mem = malloc_uncached(totsize);
	assert(Mixer.ch_buf_mem != NULL);
	uint8_t *cur = Mixer.ch_buf_mem;

	// Initialize the sample buffers
	for (int i=0;i<Mixer.num_channels;i++) {
		samplebuffer_init(&Mixer.ch_buf[i], cur, bufsize[i]);
		cur += bufsize[i];
	}

	assert(cur == Mixer.ch_buf_mem+totsize);
}

void mixer_set_vol(float vol) {
	Mixer.vol = vol;
}

void mixer_close(void) {
	assert(mixer_initialized());

	if (Mixer.ch_buf_mem) {
		free_uncached(Mixer.ch_buf_mem);
		Mixer.ch_buf_mem = NULL;
	}

	Mixer.num_channels = 0;
}

void mixer_ch_set_freq(int ch, float frequency) {
	mixer_channel_t *c = &Mixer.channels[ch];
	assertf(!(c->flags & CH_FLAGS_STEREO_SUB), "mixer_ch_set_freq: cannot call on secondary stereo channel %d", ch);
	c->step = MIXER_FX64(frequency / (float)Mixer.sample_rate) << (c->flags & CH_FLAGS_BPS_SHIFT);
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

void mixer_ch_set_vol_dolby(int ch, float fl, float fr,
	float c, float sl, float sr) {

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

	mixer_ch_set_vol(ch,
		fl*KFn + c*KCn - sl*KBn - sr*KBn,
		fr*KFn + c*KCn + sl*KBn + sr*KBn
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
	waveform_t *wave = (waveform_t*)ctx;

	if (!wave->loop_len) {
		// No loop defined: just call the waveform's read function.
		wave->read(wave->ctx, sbuf, wpos, wlen, seeking);
	} else {
		// Calculate wrapped position
		if (wpos >= wave->len)
			wpos = waveform_wrap_wpos(wpos, wave->len, wave->loop_len);

		// The read might cross the end point of the waveform
		// and continue at the loop point. We would need to handle
		// this case by performing two reads with a seek inbetween.

		// Split the length into two segments: before loop and loop.
		int len1 = wlen;
		if (wpos + wlen > wave->len)
			len1 = wave->len - wpos;
		int len2 = wlen-len1;

		// Logic check: the second segment (loop) shouldn't be longer
		// than the loop length plus the loop overread. Otherwise, it means
		// that we've been requested a single read that spans more than two
		// full loops, but that's impossible! In fact, a single request must fit
		// a sample buffer, and if a whole loop fits the sample buffer,
		// we wouldn't get here: the mixer handles fully-cachable loops
		// without unrolling them (see mixer_poll).
		assertf(len2 <= wave->loop_len + (MIXER_LOOP_OVERREAD >> SAMPLES_BPS_SHIFT(sbuf)),
			"waveform %s: logic error: double loop in single read\n"
			"wpos:%x, wlen:%x, len:%x loop_len:%x",
			wave->name, wpos, wlen, wave->len, wave->loop_len);

		// Perform the first read
		wave->read(wave->ctx, sbuf, wpos, len1, seeking);

		// See if we need to perform a second read for the loop. Because of
		// overread, we need to read the loop as many times as necessary
		// (though technically, once would be sufficient without overread).
		while (len2 > 0) {
			int loop_start = wave->len - wave->loop_len;
			int ns = MIN(len2, wave->loop_len);
			wave->read(wave->ctx, sbuf, loop_start, ns, true);
			len2 -= ns;
		}
	}
}

void mixer_ch_play(int ch, waveform_t *wave) {
	samplebuffer_t *sbuf = &Mixer.ch_buf[ch];
	mixer_channel_t *c = &Mixer.channels[ch];

	if (!Mixer.ch_buf_mem) {
		// If we have not yet allocated the memory for the sample buffers,
		// this is a good moment to do so, as we might need the configure
		// the samplebuffer in a moment.
		mixer_init_samplebuffers();
	}

	// Configure the waveform on this channel, if we have not
	// already. This optimization is useful in case the caller
	// wants to play the same waveform on the same channel multiple
	// times, and the waveform has been already decoded and cached
	// in the sample buffer.
	if (wave != sbuf->wv_ctx) {
		samplebuffer_flush(sbuf);

		// Configure the sample buffer for this waveform
		assert(wave->channels == 1 || wave->channels == 2);
		assert(wave->bits == 8 || wave->bits == 16);
		samplebuffer_set_bps(sbuf, wave->bits*wave->channels);
		samplebuffer_set_waveform(sbuf, wave->read ? waveform_read : NULL, wave);

		// Configure the mixer channel structured used by the RSP ucode
		assertf(wave->len >= 0 && wave->len <= WAVEFORM_MAX_LEN, "waveform %s: invalid length %x", wave->name, wave->len);
		assertf(wave->len != WAVEFORM_UNKNOWN_LEN || wave->loop_len == 0, "waveform %s with unknown length cannot loop", wave->name);
		int bps = SAMPLES_BPS_SHIFT(sbuf);
		c->flags = bps | (wave->channels == 2 ? CH_FLAGS_STEREO : 0) | (wave->bits == 16 ? CH_FLAGS_16BIT : 0);
		c->len = MIXER_FX64((int64_t)wave->len) << bps;
		c->loop_len = MIXER_FX64((int64_t)wave->loop_len) << bps;
		mixer_ch_set_freq(ch, wave->frequency);

		if (wave->channels == 2) {
			assertf(ch != Mixer.num_channels-1, "cannot configure last channel (%d) as stereo", ch);
			Mixer.channels[ch+1].flags |= CH_FLAGS_STEREO_SUB;
		} else if (ch != Mixer.num_channels-1) {
			Mixer.channels[ch+1].flags &= ~CH_FLAGS_STEREO_SUB;
		}

		tracef("mixer_ch_play: ch=%d len=%llx loop_len=%llx wave=%s\n", ch, c->len >> (MIXER_FX64_FRAC+bps), c->loop_len >> (MIXER_FX64_FRAC+bps), wave->name);
	}

	// Restart from the beginning of the waveform
	c->ptr = SAMPLES_PTR(sbuf);
	c->pos = 0;
}

void mixer_ch_set_pos(int ch, float pos) {
	mixer_channel_t *c = &Mixer.channels[ch];
	assertf(!(c->flags & CH_FLAGS_STEREO_SUB), "mixer_ch_set_pos: cannot call on secondary stereo channel %d", ch);
	c->pos = MIXER_FX64(pos) << (c->flags & CH_FLAGS_BPS_SHIFT);
}

float mixer_ch_get_pos(int ch) {
	mixer_channel_t *c = &Mixer.channels[ch];
	assertf(!(c->flags & CH_FLAGS_STEREO_SUB), "mixer_ch_get_pos: cannot call on secondary stereo channel %d", ch);
	uint32_t pos = c->pos >> (c->flags & CH_FLAGS_BPS_SHIFT);
	return (float)pos / (float)(1<<MIXER_FX64_FRAC);
}

void mixer_ch_stop(int ch) {
	mixer_channel_t *c = &Mixer.channels[ch];
	c->ptr = 0;
	if (c->flags & CH_FLAGS_STEREO)
		c[1].flags &= ~CH_FLAGS_STEREO_SUB;

	// Restart caching if played again. We need this guarantee
	// because after calling stop(), the caller must be able
	// to free waveform, and thus this pointer might become invalid.
	Mixer.ch_buf[ch].wv_ctx = NULL;

}

bool mixer_ch_playing(int ch) {
	mixer_channel_t *c = &Mixer.channels[ch];
	assertf(!(c->flags & CH_FLAGS_STEREO_SUB), "mixer_ch_playing: cannot call on secondary stereo channel %d", ch);
	return c->ptr != 0;
}

void mixer_ch_set_limits(int ch, int max_bits, float max_frequency, int max_buf_sz) {
	assert(max_bits == 0 || max_bits == 8 || max_bits == 16);
	assert(max_frequency >= 0);
	assert(max_buf_sz >= 0 && max_buf_sz % 8 == 0);
	tracef("mixer_ch_set_limits: ch=%d bits=%d maxfreq:%.2f bufsz:%d\n", ch, max_bits, max_frequency, max_buf_sz);

	Mixer.limits[ch] = (channel_limit_t){
		.max_bits = max_bits ? max_bits : 16,
		.max_frequency = max_frequency ? max_frequency : Mixer.sample_rate,
		.max_buf_sz = max_buf_sz,
	};

	// Changing the limits will invalidate the whole sample buffer
	// memory area. Invalidate all sample buffers.
	if (Mixer.ch_buf_mem) {
		for (int i=0;i<Mixer.num_channels;i++)
			samplebuffer_close(&Mixer.ch_buf[i]);
		free_uncached(Mixer.ch_buf_mem);
		Mixer.ch_buf_mem = NULL;
	}
}

void mixer_exec(int32_t *out, int num_samples) {
	if (!Mixer.ch_buf_mem) {
		// If we have not yet allocated the memory for the sample buffers,
		// this is a good moment to do so.
		mixer_init_samplebuffers();
	}

	tracef("mixer_exec: 0x%x samples\n", num_samples);

	uint32_t fake_loop = 0;

	for (int i=0; i<Mixer.num_channels; i++) {
		samplebuffer_t *sbuf = &Mixer.ch_buf[i];
		mixer_channel_t *ch = &Mixer.channels[i];
		int bps = ch->flags & CH_FLAGS_BPS_SHIFT;
		int bps_fx64 = bps + MIXER_FX64_FRAC;

		if (ch->ptr) {
			int len = ch->len >> bps_fx64;
			int loop_len = ch->loop_len >> bps_fx64;
			int wpos = ch->pos >> bps_fx64;
			int wlast = (ch->pos + ch->step*(num_samples-1)) >> bps_fx64;
			int wlen = wlast-wpos+1;
			assertf(wlen >= 0, "channel %d: wpos overflow", i);
			tracef("ch:%d wpos:%x wlen:%x len:%x loop_len:%x sbuf_size:%x\n", i, wpos, wlen, len, loop_len, sbuf->size);

			if (!loop_len) {
				// If we reached the end of the waveform, stop the channel
				// by NULL-ing the buffer pointer.
				if (wpos >= len) {
					ch->ptr = 0;
					if (ch->flags & CH_FLAGS_STEREO)
						ch[1].flags &= ~CH_FLAGS_STEREO_SUB;
					continue;
				}
				// When there's no loop, do not ask for more samples then
				// actually present in the waveform.
				if (wpos+wlen > len)
					wlen = len-wpos;
				assert(wlen >= 0);
			} else if (loop_len < sbuf->size) {
				// If the whole loop fits the sample buffer, we just need to
				// make sure that it is aligned at the start of the buffer, so
				// that it can be fully cached.
				// To do so, we discard everything that comes before the loop 
				// (once we enter the loop).
				int loop_pos = len - loop_len;
				if (wpos >= loop_pos) {
					tracef("ch:%d discard to align loop wpos:%x loop_pos:%x\n", i, wpos, loop_pos);
					samplebuffer_discard(sbuf, loop_pos);
				}

				// Do not ask more samples than the end of waveform. When we
				// get there, the loop has been already fully cached. The RSP
				// will correctly follow the loop.
				while (wpos >= len)
					wpos -= loop_len;
				if (wpos+wlen > len)
					wlen = len-wpos;

				// FIXME: due to a limit in the RSP ucode, we need to overread
				// more data past the loop end.
				wlen += MIXER_LOOP_OVERREAD >> bps;
				assertf(wlen >= 0, "ch:%d wlen=%x wpos=%x len=%x\n", i, wlen, wpos, len);
			} else {
				// The loop is larger than the sample buffer. We cannot fully
				// cache it, so we will have to unroll it in the sample buffer.
				// This happens by default without doing anything: wpos will
				// increase, and the actual unrolling logic will be performed
				// by waveform_read() (see above).

				// To avoid having wpos growing indefinitely (and overflowing),
				// let's force a manual wrapping of the coordinates. Check if
				// this is a good moment to do it.
				if (sbuf->wpos > len && wpos > len) {
					tracef("mixer_poll: wrapping sample buffer loop: sbuf->wpos:%x len:%x\n", sbuf->wpos, len);
					samplebuffer_discard(sbuf, wpos);
					sbuf->wpos = waveform_wrap_wpos(sbuf->wpos, len, loop_len);
					int wpos2 = waveform_wrap_wpos(wpos, len, loop_len);
					ch->pos -= (int64_t)(wpos-wpos2) << bps_fx64;
					wpos = wpos2;
				}

				// We will also lie to the RSP ucode telling it that there is
				// no loop in this waveform, since the RSP will always see
				// the loop unrolled in the buffer, so it doesn't need to
				// do anything.
				fake_loop |= 1<<i;
			}

			void* ptr = samplebuffer_get(sbuf, wpos, &wlen);
			assert(ptr);
			ch->ptr = (uint8_t*)ptr - (wpos<<bps);
		}
	}

	rsp_wait();
	rsp_load(&rsp_mixer);

	volatile rsp_mixer_channel_t *rsp_wv = (volatile rsp_mixer_channel_t *)&SP_DMEM[36];
	mixer_fx15_t lvol[MIXER_MAX_CHANNELS] __attribute__((aligned(8))) = {0};
	mixer_fx15_t rvol[MIXER_MAX_CHANNELS] __attribute__((aligned(8))) = {0};

	for (int ch=0;ch<Mixer.num_channels;ch++) {
		mixer_channel_t *c = &Mixer.channels[ch];

		// Stereo sub-channel. Will be ignored by RSP but we need to configure
		// volume correctly.
		if (c->flags & CH_FLAGS_STEREO_SUB) {
			rsp_wv[ch].ptr = 0;
			lvol[ch] = 0;
			rvol[ch] = Mixer.rvol[ch-1];
			continue;
		}

		// Check if the channel is stopped
		if (!c->ptr) {
			rsp_wv[ch].ptr = 0;
			// Configure the volume to 0 when the channel is keyed off. This
			// makes sure that we smooth volume correctly even for waveforms
			// where the sequencer creates an a attack ramp (which would nullify
			// the one-tap volume filter if the volume started from max).
			lvol[ch] = 0;
			rvol[ch] = 0;
			continue;
		}

		// Convert to RSP mixer channel structure truncating 64-bit values to 32-bit.
		// We don't need full absolute position on the RSP, so 32-bit is more
		// than enough. In fact, we only expose 31 bits, so that we can use the
		// 32nd bit later to correctly update the position without overflow bugs.
		rsp_wv[ch].pos = (uint32_t)c->pos & 0x7FFFFFFF;
		rsp_wv[ch].step = (uint32_t)c->step & 0x7FFFFFFF;
		rsp_wv[ch].ptr = c->ptr + ((c->pos & ~0x7FFFFFFF) >> MIXER_FX64_FRAC);
		rsp_wv[ch].flags = c->flags;

		// If the loop is fake (i.e. we are unrolling it), or the current
		// position has been truncated but it's far from the end of the waveform,
		// just tell the RSP that there is no loop.
		if (fake_loop & (1<<ch) || c->pos>>31 != c->len>>31) {
			rsp_wv[ch].len = 0xFFFFFFFF;
			rsp_wv[ch].loop_len = 0;
		} else {
			rsp_wv[ch].len = (uint32_t)c->len & 0x7FFFFFFF;
			// We can't represent a very long loop in RSP. But those loops
			// should be unrolled anyway (and thus be a fake_loop), so we
			// should not get here.
			assert(c->loop_len <= 0x7FFFFFFF);
			rsp_wv[ch].loop_len = (uint32_t)c->loop_len & 0x7FFFFFFF;
		}

		if (c->flags & CH_FLAGS_STEREO) {
			lvol[ch] = Mixer.lvol[ch];
			rvol[ch] = 0;
		} else {
			lvol[ch] = Mixer.lvol[ch];
			rvol[ch] = Mixer.rvol[ch];
		}
	}

	// Copy the volumes into DMEM. TODO: check if should change this loop into
	// a DMA copy, or fold it into the above loop.
	uint32_t *lvol32 = (uint32_t*)lvol;
	uint32_t *rvol32 = (uint32_t*)rvol;
	for (int ch=0;ch<MIXER_MAX_CHANNELS/2;ch++)  {
		SP_DMEM[4+0*16+ch] = lvol32[ch];
		SP_DMEM[4+1*16+ch] = rvol32[ch];
	}

	SP_DMEM[0] = MIXER_FX16(Mixer.vol);
	SP_DMEM[1] = (num_samples << 16) | Mixer.num_channels;
	SP_DMEM[2] = (uint32_t)out;
	SP_DMEM[3] = (uint32_t)Mixer.ucode_state;

	uint32_t t0 = TICKS_READ();
	rsp_run();
	__mixer_profile_rsp += TICKS_READ() - t0;

	for (int i=0;i<Mixer.num_channels;i++) {
		mixer_channel_t *ch = &Mixer.channels[i];
		if (ch->ptr)
			ch->pos += (uint64_t)rsp_wv[i].pos - (uint64_t)(ch->pos & 0x7FFFFFFF);
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
		if (Mixer.events[i].cb == cb) {
			memmove(&Mixer.events[i], &Mixer.events[i+1], sizeof(mixer_event_t) * (Mixer.num_events-i-1));
			Mixer.num_events--;
			return;
		}
	}
	assertf("mixer_remove_event: specified event does not exist\ncb:%p ctx:%p", (void*)cb, ctx);
}

void mixer_poll(int16_t *out16, int num_samples) {
	int32_t *out = (int32_t*)out16;

	// Since the AI can only play an even number of samples,
	// it's not possible to call this function with an odd number,
	// otherwise buffering might become complicated / impossible.
	assert(num_samples % 2 == 0);

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
