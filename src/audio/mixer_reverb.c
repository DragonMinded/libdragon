/**
 * @file mixer_reverb.c
 * @brief Global Schroeder reverb post-process for the mixer
 *
 * Implementation notes:
 *
 *   - Topology: 4 parallel comb filters (with one-pole low-pass damping in
 *     each feedback path) summed into 2 series allpasses. Mono reverb bus,
 *     mixed back into both stereo channels at independent wet levels.
 *
 *   - Sample-rate independent: the preset table specifies comb/allpass tap
 *     lengths in samples-at-a-reference-rate and damping as a cutoff in Hz.
 *     At init time the runtime allocates delay lines sized for the actual
 *     mixer rate, and at preset-apply time the damping cutoff is converted to
 *     a one-pole coefficient using the actual rate, so the same preset keeps
 *     the same RT60 (in seconds) and the same brightness (in Hz) at any rate.
 *
 *   - DSP body lives in __mixer_reverb_process(), called from mixer_poll()
 *     after mixer_exec() writes the int32-packed stereo output buffer.
 *     The function will later be swapped for an RSP overlay without changing
 *     the API in mixer_reverb.h.
 */

#include "mixer_reverb.h"
#include "mixer_internal.h"
#include "debug.h"
#include "n64sys.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Cycle-counter profiling of __mixer_reverb_process. Logs a summary every
 * ~2 s of audio to debugf so the cost can be compared on/off in a real scene. */
#define REVERB_PROFILE 0

/* ------------------------------------------------------------------------- */
/* Preset table                                                              */
/* ------------------------------------------------------------------------- */

#define REVERB_NUM_COMBS     4
#define REVERB_NUM_ALLPASSES 2
#define REVERB_NUM_PRESETS   10

/* Tap lengths in the preset table are at this reference rate. They get
 * linearly rescaled to the actual mixer rate at init / set_type time. */
#define REVERB_REF_RATE      32000

typedef struct {
    uint16_t comb_len[REVERB_NUM_COMBS];        /* samples at REVERB_REF_RATE */
    uint16_t allpass_len[REVERB_NUM_ALLPASSES]; /* samples at REVERB_REF_RATE */
    float    feedback;     /* comb feedback gain, ~0..0.99 */
    float    damping_hz;   /* feedback-path LP cutoff in Hz */
    bool     off;          /* true on mode 0 to short-circuit DSP */
} reverb_preset_t;

/* Allpass diffusion coefficient (Freeverb convention). */
static const float ALLPASS_FEEDBACK = 0.5f;

/* damping_hz values are the LP cutoff applied to each comb's feedback path.
 * Lower = darker tail, higher = brighter. They were derived from the original
 * 32 kHz-tuned coefficients via fc = -fs * ln(a) / (2π) and rounded. */
static const reverb_preset_t presets[REVERB_NUM_PRESETS] = {
    /* 0: off — DSP is bypassed. */
    { .off = true },

    /* 1: short, bright room, low decay. */
    { .comb_len = {610, 656, 703, 740}, .allpass_len = {310, 240},
      .feedback = 0.70f, .damping_hz = 8200.0f },

    /* 2: small/medium studio, light damping. */
    { .comb_len = {730, 780, 840, 890}, .allpass_len = {360, 290},
      .feedback = 0.78f, .damping_hz = 6100.0f },

    /* 3: medium studio, balanced. */
    { .comb_len = {810, 862, 927, 984}, .allpass_len = {403, 320},
      .feedback = 0.84f, .damping_hz = 4700.0f },

    /* 4: medium studio, slightly darker. */
    { .comb_len = {860, 920, 980, 1050}, .allpass_len = {410, 330},
      .feedback = 0.86f, .damping_hz = 3000.0f },

    /* 5: long hall, lush. */
    { .comb_len = {1130, 1190, 1260, 1330}, .allpass_len = {450, 360},
      .feedback = 0.92f, .damping_hz = 5300.0f },

    /* 6: large space, very long, very wet. */
    { .comb_len = {1400, 1480, 1560, 1660}, .allpass_len = {500, 410},
      .feedback = 0.95f, .damping_hz = 7100.0f },

    /* 7: echo — medium delay, distinct repeats, sparse. */
    { .comb_len = {1200, 1280, 1370, 1460}, .allpass_len = {200, 140},
      .feedback = 0.80f, .damping_hz = 6100.0f },

    /* 8: pre-delay-leaning, low feedback. */
    { .comb_len = {800, 850, 900, 950}, .allpass_len = {120, 90},
      .feedback = 0.50f, .damping_hz = 8200.0f },

    /* 9: half-length echo, partial feedback. */
    { .comb_len = {600, 660, 720, 780}, .allpass_len = {180, 130},
      .feedback = 0.65f, .damping_hz = 7100.0f },
};

/* ------------------------------------------------------------------------- */
/* Runtime state                                                             */
/* ------------------------------------------------------------------------- */

typedef struct {
    int16_t *buf;        /* circular delay line, sized for the longest preset */
    int      buf_len;    /* allocated capacity (samples)                      */
    int      len;        /* active length from preset (scaled to rate)        */
    int      idx;        /* write/read cursor                                 */
    float    damp_state; /* one-pole LP state on the feedback path            */
} comb_t;

typedef struct {
    int16_t *buf;
    int      buf_len;
    int      len;
    int      idx;
} allpass_t;

typedef struct {
    bool      initialized;
    bool      enabled;
    int       sample_rate;
    int       current_type;
    float     wet_l;
    float     wet_r;
    float     damp_coeff;  /* one-pole LP coefficient at sample_rate (preset Hz pre-warped) */
    float     input_gain;  /* per-preset wet-bus scale that normalizes comb amplification:
                              prevents the wet output from running louder than the dry
                              signal at depth=1, and stops the s16 comb buffers from
                              clipping internally. Computed in apply_preset(). */

    comb_t    combs[REVERB_NUM_COMBS];
    allpass_t allpasses[REVERB_NUM_ALLPASSES];
} reverb_state_t;

static reverb_state_t state;

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static int scale_len(int ref_len, int sample_rate)
{
    if (ref_len <= 0) return 1;
    int v = (int)(((int64_t)ref_len * sample_rate + REVERB_REF_RATE / 2)
                  / REVERB_REF_RATE);
    if (v < 1) v = 1;
    return v;
}

static void zero_delay_lines(void)
{
    for (int i = 0; i < REVERB_NUM_COMBS; i++) {
        if (state.combs[i].buf)
            memset(state.combs[i].buf, 0,
                   state.combs[i].buf_len * sizeof(int16_t));
        state.combs[i].damp_state = 0.0f;
        state.combs[i].idx = 0;
    }
    for (int j = 0; j < REVERB_NUM_ALLPASSES; j++) {
        if (state.allpasses[j].buf)
            memset(state.allpasses[j].buf, 0,
                   state.allpasses[j].buf_len * sizeof(int16_t));
        state.allpasses[j].idx = 0;
    }
}

static void apply_preset(int type)
{
    if (type < 0 || type >= REVERB_NUM_PRESETS) type = 0;
    const reverb_preset_t *p = &presets[type];
    state.current_type = type;

    if (p->off) {
        for (int i = 0; i < REVERB_NUM_COMBS; i++)
            state.combs[i].len = 1;
        for (int j = 0; j < REVERB_NUM_ALLPASSES; j++)
            state.allpasses[j].len = 1;
        state.damp_coeff = 0.0f;
        state.input_gain = 0.0f;
    } else {
        for (int i = 0; i < REVERB_NUM_COMBS; i++) {
            int len = scale_len(p->comb_len[i], state.sample_rate);
            assertf(len <= state.combs[i].buf_len,
                    "preset %d comb %d: scaled len %d > capacity %d "
                    "(longest-preset / rate mismatch in mixer_reverb_init)",
                    type, i, len, state.combs[i].buf_len);
            state.combs[i].len = len;
        }
        for (int j = 0; j < REVERB_NUM_ALLPASSES; j++) {
            int len = scale_len(p->allpass_len[j], state.sample_rate);
            assertf(len <= state.allpasses[j].buf_len,
                    "preset %d allpass %d: scaled len %d > capacity %d",
                    type, j, len, state.allpasses[j].buf_len);
            state.allpasses[j].len = len;
        }
        /* Pre-warp the cutoff to a one-pole coefficient at the actual rate
         * so the LP frequency response in Hz stays constant across rates. */
        state.damp_coeff =
            expf(-2.0f * (float)M_PI * p->damping_hz
                 / (float)state.sample_rate);

        /* Each comb's steady-state DC gain is 1/(1-feedback), so without
         * compensation a high-feedback preset produces a wet bus that's tens
         * of times louder than the dry input. Scaling the input by (1-fb)
         * caps each comb's internal state at ~0.5× the input amplitude (so
         * the s16 delay-line buffers don't clip on loud sources) while still
         * leaving an audible wet level on transients. The 4-comb sum then
         * runs roughly 2× the dry signal at DC steady-state; the user
         * dials the perceived mix down with set_depth(). */
        state.input_gain = 1.0f - p->feedback;
    }
    zero_delay_lines();
}

static inline bool dsp_is_silent(void)
{
    if (!state.initialized || !state.enabled) return true;
    if (presets[state.current_type].off) return true;
    if (state.wet_l <= 0.0f && state.wet_r <= 0.0f) return true;
    return false;
}

static inline int16_t sat16(float v)
{
    if (v > 32767.0f)  return 32767;
    if (v < -32768.0f) return -32768;
    return (int16_t)v;
}

/* ------------------------------------------------------------------------- */
/* Profiling                                                                 */
/* ------------------------------------------------------------------------- */

#if REVERB_PROFILE
static uint64_t prof_active_ticks = 0;
static uint32_t prof_active_samples = 0;
static uint32_t prof_active_calls = 0;
static uint32_t prof_active_max_ticks = 0;
static uint64_t prof_silent_ticks = 0;
static uint32_t prof_silent_samples = 0;
static uint32_t prof_silent_calls = 0;
static uint32_t prof_report_sample_threshold = 64000;  /* ~2 s @ 32 kHz */

static void prof_maybe_report(void)
{
    uint32_t total_samples = prof_active_samples + prof_silent_samples;
    if (total_samples < prof_report_sample_threshold) return;

    /* CPU share over the audio-time window. N samples at rate sr = N/sr
     * seconds of audio. Reverb consumed `ticks` ticks of CPU in that window;
     * share = ticks * sr / (TICKS_PER_SECOND * N). Express as ppm (×1e6) so
     * the silent fast-path stays measurable. */
    extern int __mixer_reverb_sample_rate(void);
    int sr = __mixer_reverb_sample_rate();
    uint32_t active_ppm = (sr && prof_active_samples)
        ? (uint32_t)((prof_active_ticks * 1000000ULL * (uint64_t)sr)
                     / ((uint64_t)TICKS_PER_SECOND * prof_active_samples)) : 0;
    uint32_t silent_ppm = (sr && prof_silent_samples)
        ? (uint32_t)((prof_silent_ticks * 1000000ULL * (uint64_t)sr)
                     / ((uint64_t)TICKS_PER_SECOND * prof_silent_samples)) : 0;

    debugf("[reverb-prof] active: %lu calls %lu samples %lu total-ticks max %lu ticks/call ~%lu ppm CPU (%lu.%02lu%%)\n",
        prof_active_calls, prof_active_samples, (uint32_t)prof_active_ticks,
        prof_active_max_ticks, active_ppm, active_ppm / 10000, (active_ppm / 100) % 100);
    debugf("[reverb-prof] silent: %lu calls %lu samples %lu total-ticks ~%lu ppm CPU\n",
        prof_silent_calls, prof_silent_samples, (uint32_t)prof_silent_ticks, silent_ppm);

    prof_active_ticks = prof_active_samples = prof_active_calls = prof_active_max_ticks = 0;
    prof_silent_ticks = prof_silent_samples = prof_silent_calls = 0;
}

int __mixer_reverb_sample_rate(void) { return state.sample_rate; }
#endif

/* ------------------------------------------------------------------------- */
/* DSP entry (called from mixer.c)                                           */
/* ------------------------------------------------------------------------- */

static void __mixer_reverb_process(int32_t *out, int num_samples)
{
#if REVERB_PROFILE
    uint32_t t0 = TICKS_READ();
#endif
    if (dsp_is_silent()) {
#if REVERB_PROFILE
        uint32_t dt = TICKS_SINCE(t0);
        prof_silent_ticks   += dt;
        prof_silent_samples += num_samples;
        prof_silent_calls++;
        prof_maybe_report();
#endif
        return;
    }

    /* Mixer output is stereo interleaved int16 (big-endian): each int32
     * holds L in the upper half and R in the lower half. Process as int16
     * pairs in place. */
    int16_t *p = (int16_t *)out;

    const reverb_preset_t *pr = &presets[state.current_type];
    const float feedback   = pr->feedback;
    const float damping    = state.damp_coeff;     /* pre-warped at preset apply */
    const float one_m_damping = 1.0f - damping;
    const float input_gain = state.input_gain;     /* wet-bus normalization */
    const float wet_l = state.wet_l;
    const float wet_r = state.wet_r;

    comb_t    *c0 = &state.combs[0];
    comb_t    *c1 = &state.combs[1];
    comb_t    *c2 = &state.combs[2];
    comb_t    *c3 = &state.combs[3];
    allpass_t *a0 = &state.allpasses[0];
    allpass_t *a1 = &state.allpasses[1];

    for (int n = 0; n < num_samples; n++) {
        int16_t l_in = p[0];
        int16_t r_in = p[1];

        /* Mono reverb bus, halved (L+R)/2, then attenuated to compensate for
         * the combs' feedback amplification. See state.input_gain above. */
        float in = ((float)l_in + (float)r_in) * 0.5f * input_gain;

        /* Four parallel combs, summed. */
        float sum = 0.0f;
        {
            int16_t r = c0->buf[c0->idx];
            float damped = c0->damp_state * damping + (float)r * one_m_damping;
            c0->damp_state = damped;
            c0->buf[c0->idx] = sat16(in + damped * feedback);
            if (++c0->idx >= c0->len) c0->idx = 0;
            sum += (float)r;
        }
        {
            int16_t r = c1->buf[c1->idx];
            float damped = c1->damp_state * damping + (float)r * one_m_damping;
            c1->damp_state = damped;
            c1->buf[c1->idx] = sat16(in + damped * feedback);
            if (++c1->idx >= c1->len) c1->idx = 0;
            sum += (float)r;
        }
        {
            int16_t r = c2->buf[c2->idx];
            float damped = c2->damp_state * damping + (float)r * one_m_damping;
            c2->damp_state = damped;
            c2->buf[c2->idx] = sat16(in + damped * feedback);
            if (++c2->idx >= c2->len) c2->idx = 0;
            sum += (float)r;
        }
        {
            int16_t r = c3->buf[c3->idx];
            float damped = c3->damp_state * damping + (float)r * one_m_damping;
            c3->damp_state = damped;
            c3->buf[c3->idx] = sat16(in + damped * feedback);
            if (++c3->idx >= c3->len) c3->idx = 0;
            sum += (float)r;
        }

        /* Two series allpasses for diffusion. */
        {
            float bufout = (float)a0->buf[a0->idx];
            float out_ap = -sum + bufout;
            a0->buf[a0->idx] = sat16(sum + bufout * ALLPASS_FEEDBACK);
            if (++a0->idx >= a0->len) a0->idx = 0;
            sum = out_ap;
        }
        {
            float bufout = (float)a1->buf[a1->idx];
            float out_ap = -sum + bufout;
            a1->buf[a1->idx] = sat16(sum + bufout * ALLPASS_FEEDBACK);
            if (++a1->idx >= a1->len) a1->idx = 0;
            sum = out_ap;
        }

        /* Mix wet into dry and write back. */
        p[0] = sat16((float)l_in + sum * wet_l);
        p[1] = sat16((float)r_in + sum * wet_r);
        p += 2;
    }

#if REVERB_PROFILE
    uint32_t dt = TICKS_SINCE(t0);
    prof_active_ticks   += dt;
    prof_active_samples += num_samples;
    prof_active_calls++;
    if (dt > prof_active_max_ticks) prof_active_max_ticks = dt;
    prof_maybe_report();
#endif
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

void mixer_reverb_init(int sample_rate)
{
    assertf(sample_rate > 0, "mixer_reverb_init: invalid sample_rate %d",
            sample_rate);

    if (state.initialized) mixer_reverb_close();

    state.sample_rate = sample_rate;
    state.enabled = false;
    state.current_type = 0;
    state.wet_l = 0.0f;
    state.wet_r = 0.0f;

    /* Find the longest comb and allpass tap across all presets so the
     * delay-line buffers can hold any preset at the current rate. The
     * extra "+ 1" guards against rounding when a preset's scaled length
     * uses the very top of the range. */
    uint16_t max_comb_ref = 0, max_allpass_ref = 0;
    for (int p = 0; p < REVERB_NUM_PRESETS; p++) {
        if (presets[p].off) continue;
        for (int i = 0; i < REVERB_NUM_COMBS; i++)
            if (presets[p].comb_len[i] > max_comb_ref)
                max_comb_ref = presets[p].comb_len[i];
        for (int j = 0; j < REVERB_NUM_ALLPASSES; j++)
            if (presets[p].allpass_len[j] > max_allpass_ref)
                max_allpass_ref = presets[p].allpass_len[j];
    }
    int max_comb_len    = scale_len(max_comb_ref,    sample_rate) + 1;
    int max_allpass_len = scale_len(max_allpass_ref, sample_rate) + 1;

    for (int i = 0; i < REVERB_NUM_COMBS; i++) {
        state.combs[i].buf =
            (int16_t *)calloc(max_comb_len, sizeof(int16_t));
        assertf(state.combs[i].buf, "mixer_reverb_init: OOM comb %d", i);
        state.combs[i].buf_len = max_comb_len;
    }
    for (int j = 0; j < REVERB_NUM_ALLPASSES; j++) {
        state.allpasses[j].buf =
            (int16_t *)calloc(max_allpass_len, sizeof(int16_t));
        assertf(state.allpasses[j].buf, "mixer_reverb_init: OOM ap %d", j);
        state.allpasses[j].buf_len = max_allpass_len;
    }

    state.initialized = true;
    apply_preset(0);
    mixer_register_global_effect(__mixer_reverb_process);
}

void mixer_reverb_close(void)
{
    if (!state.initialized) return;
    mixer_unregister_global_effect(__mixer_reverb_process);
    for (int i = 0; i < REVERB_NUM_COMBS; i++) {
        free(state.combs[i].buf);
        state.combs[i].buf = NULL;
    }
    for (int j = 0; j < REVERB_NUM_ALLPASSES; j++) {
        free(state.allpasses[j].buf);
        state.allpasses[j].buf = NULL;
    }
    memset(&state, 0, sizeof(state));
}

void mixer_reverb_set_enabled(bool enable)
{
    state.enabled = enable;
}

void mixer_reverb_set_type(int preset)
{
    if (!state.initialized) return;
    apply_preset(preset);
}

void mixer_reverb_set_depth(float left, float right)
{
    if (left  < 0.0f) left  = 0.0f;
    else if (left  > 1.0f) left  = 1.0f;
    if (right < 0.0f) right = 0.0f;
    else if (right > 1.0f) right = 1.0f;
    state.wet_l = left;
    state.wet_r = right;
}

void mixer_reverb_clear_work_area(void)
{
    if (!state.initialized) return;
    zero_delay_lines();
}
