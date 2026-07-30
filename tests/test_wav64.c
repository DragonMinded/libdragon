/**
 * @file test_wav64.c
 * @brief Standalone testrom for in-mixer VADPCM (MIX_CHANNEL)
 *
 * Exercises the VADPCM path inside MIX_CHANNEL and compares against the
 * reference C decoder. The legacy VADPCM_Decompress command has been removed.
 */
#include <libdragon.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "../src/audio/mixer_internal.h"
#include "../src/audio/wav64_vadpcm_internal.h"

//////////////////////////////////////////////////////////////////////////////
// Reference VADPCM decoder
//////////////////////////////////////////////////////////////////////////////

typedef enum {
    kVADPCMErrNone,
    kVADPCMErrInvalidData,
} vadpcm_error;

static int vadpcm_ext4(int x) {
    return x > 7 ? x - 16 : x;
}

static int vadpcm_clamp16(int x) {
    if (x < -0x8000 || 0x7fff < x) {
        return (x >> (sizeof(int) * CHAR_BIT - 1)) ^ 0x7fff;
    }
    return x;
}

static vadpcm_error vadpcm_decode(int predictor_count, int order,
                           const wav64_vadpcm_vector_t *restrict codebook,
                           wav64_vadpcm_vector_t *restrict state,
                           size_t frame_count, int16_t *restrict dest,
                           const void *restrict src) {
    const uint8_t *sptr = src;
    for (size_t frame = 0; frame < frame_count; frame++) {
        const uint8_t *fin = sptr + 9 * frame;
        int control = fin[0];
        int scaling = control >> 4;
        int predictor_index = control & 15;
        if (predictor_index >= predictor_count) {
            return kVADPCMErrInvalidData;
        }
        const wav64_vadpcm_vector_t *predictor =
            codebook + order * predictor_index;
        for (int vector = 0; vector < 2; vector++) {
            int32_t accumulator[8];
            for (int i = 0; i < 8; i++) {
                accumulator[i] = 0;
            }
            for (int k = 0; k < order; k++) {
                int sample = state->v[8 - order + k];
                for (int i = 0; i < 8; i++) {
                    accumulator[i] += sample * predictor[k].v[i];
                }
            }
            int residuals[8];
            for (int i = 0; i < 4; i++) {
                int byte = fin[1 + 4 * vector + i];
                residuals[2 * i] = vadpcm_ext4(byte >> 4);
                residuals[2 * i + 1] = vadpcm_ext4(byte & 15);
            }
            const wav64_vadpcm_vector_t *v = &predictor[order - 1];
            for (int k = 0; k < 8; k++) {
                int residual = residuals[k] << scaling;
                accumulator[k] += residual << 11;
                for (int i = 0; i < 7 - k; i++) {
                    accumulator[k + 1 + i] += residual * v->v[i];
                }
            }
            for (int i = 0; i < 8; i++) {
                int sample = vadpcm_clamp16(accumulator[i] >> 11);
                dest[16 * frame + 8 * vector + i] = sample;
                state->v[i] = sample;
            }
        }
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
// Test scaffolding
//////////////////////////////////////////////////////////////////////////////

static uint32_t rand_state = 1;
static void my_srand(uint32_t v) { rand_state = v ? v : 1; }
static uint32_t my_rand(void) {
    uint32_t x = rand_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return rand_state = x;
}

#define NPREDICTORS   4
#define ORDER         2

#define CH_FLAGS_BPS_SHIFT   (3<<0)
#define CH_FLAGS_16BIT       (1<<2)
#define CH_FLAGS_STEREO      (1<<3)
#define CH_FLAGS_STEREO_SUB  (1<<4)
#define CH_FLAGS_VADPCM      (1<<5)
#define CH_FLAGS_CLEAR_ACCUM (1<<7)
#define MIXER_FX64_FRAC      12

// Mirrors the private command ids/layouts in src/audio/mixer.c.
#define MIXER_CMD_CHANNEL     0x0
#define MIXER_CMD_SETCHANNEL  0x1
#define MIXER_CMD_FLUSH       0x2

// Register the VADPCM codebook and predictor state of a channel in the ucode
// channel table (MIX_CHANNEL only carries the sample pointer).
static void emit_setchannel(int ch, void *codebook, void *state) {
    rspq_write(__mixer_overlay_id, MIXER_CMD_SETCHANNEL, (uint32_t)ch << 16,
        PhysicalAddr(codebook), PhysicalAddr(state), 0);
}

static void emit_channel_lr(int ch, uint32_t flags, int16_t lvol, int16_t rvol,
    uint32_t pos, uint32_t step, uint32_t len, uint32_t loop_len, void *ptr,
    int nsamples)
{
    rspq_write_t w = rspq_write_begin(__mixer_overlay_id, MIXER_CMD_CHANNEL, 8);
    rspq_write_arg(&w, ((uint32_t)ch << 16) | ((flags & 0xFF) << 8));
    rspq_write_arg(&w, ((uint32_t)(uint16_t)lvol << 16) | (uint16_t)rvol);
    rspq_write_arg(&w, pos);
    rspq_write_arg(&w, step);
    rspq_write_arg(&w, len);
    rspq_write_arg(&w, loop_len);
    rspq_write_arg(&w, PhysicalAddr(ptr));
    rspq_write_arg(&w, (uint32_t)nsamples);   // acc_offset 0
    rspq_write_end(&w);
}

static void emit_channel(int ch, uint32_t flags, int16_t vol, uint32_t pos,
    uint32_t step, uint32_t len, uint32_t loop_len, void *ptr, int nsamples)
{
    emit_channel_lr(ch, flags, vol, vol, pos, step, len, loop_len, ptr, nsamples);
}

static void emit_flush(int nsamples, void *out) {
    rspq_write(__mixer_overlay_id, MIXER_CMD_FLUSH, (uint32_t)nsamples,
        PhysicalAddr(out));
}

static void gen_codebook(wav64_vadpcm_vector_t *cb) {
    for (int v = 0; v < 8; v++)
        for (int i = 0; i < 8; i++)
            cb[v].v[i] = (int16_t)((my_rand() % 4096) - 2048);
}

static void gen_frames(uint8_t *frames, int nframes_total) {
    for (int f = 0; f < nframes_total; f++) {
        uint8_t *fin = frames + 9 * f;
        int scaling = my_rand() % 5;
        int predictor = my_rand() % NPREDICTORS;
        fin[0] = (scaling << 4) | predictor;
        for (int i = 1; i < 9; i++)
            fin[i] = my_rand();
    }
}

static bool test_vadpcm_inmixer(int nframes, int seed) {
    my_srand(seed);

    wav64_vadpcm_vector_t *codebook = malloc_uncached(8 * sizeof(wav64_vadpcm_vector_t));
    gen_codebook(codebook);

    wav64_vadpcm_vector_t *state_rsp = malloc_uncached(sizeof(wav64_vadpcm_vector_t));
    memset(state_rsp, 0, sizeof(*state_rsp));
    wav64_vadpcm_vector_t state_ref;
    memset(&state_ref, 0, sizeof(state_ref));

    int in_bytes = 9 * nframes;
    uint8_t *frames = malloc_uncached(in_bytes + 16);
    gen_frames(frames, nframes);

    int nsamples = nframes * 16;
    int16_t *ref_out = malloc((size_t)nsamples * sizeof(int16_t));
    vadpcm_error err = vadpcm_decode(NPREDICTORS, ORDER, codebook, &state_ref,
        nframes, ref_out, frames);
    assertf(err == 0, "reference decode error: %d", err);

    int32_t *out = malloc_uncached(nsamples * 4);
    memset(out, 0, nsamples * 4);

    uint32_t step = 1u << MIXER_FX64_FRAC;
    int16_t vol = 0x7FFF;
    uint32_t flags = CH_FLAGS_VADPCM | CH_FLAGS_16BIT | CH_FLAGS_CLEAR_ACCUM;

    // Warm up the per-channel volume filter.
    wav64_vadpcm_vector_t *state_warm = malloc_uncached(sizeof(wav64_vadpcm_vector_t));
    memset(state_warm, 0, sizeof(*state_warm));
    int32_t *warm_out = malloc_uncached(256 * 4);
    for (int i = 0; i < 4; i++) {
        rspq_highpri_begin();
        emit_setchannel(0, codebook, state_warm);
        emit_channel(0, flags, vol, 0, step,
            (uint32_t)nsamples << MIXER_FX64_FRAC,
            (uint32_t)nsamples << MIXER_FX64_FRAC,
            frames, 256);
        emit_flush(256, warm_out);
        rspq_highpri_end();
    }
    rspq_wait();
    free_uncached(state_warm);
    free_uncached(warm_out);

    rspq_highpri_begin();
    emit_setchannel(0, codebook, state_rsp);
    emit_channel(0, flags, vol, 0, step, 0xFFFFFFFFu, 0, frames, nsamples);
    emit_flush(nsamples, out);
    rspq_highpri_end();
    rspq_wait();

    // vmulf volume path: allow ±2 LSB vs full-scale reference.
    bool ok = true;
    int16_t *stereo = (int16_t*)out;
    for (int i = 0; i < nsamples; i++) {
        int16_t l = stereo[i*2], r = stereo[i*2+1];
        int d0 = l - ref_out[i], d1 = r - ref_out[i];
        if (d0 < -2 || d0 > 2 || d1 < -2 || d1 > 2) {
            printf("FAILED in-mixer: nframes=%d seed=%d sample %d: L=%d R=%d ref=%d\n",
                nframes, seed, i, l, r, ref_out[i]);
            ok = false;
            break;
        }
    }
    if (ok && memcmp(state_rsp, &state_ref, sizeof(state_ref)) != 0) {
        printf("FAILED in-mixer: nframes=%d seed=%d: state mismatch\n", nframes, seed);
        ok = false;
    }

    free_uncached(codebook);
    free_uncached(state_rsp);
    free_uncached(frames);
    free_uncached(out);
    free(ref_out);
    return ok;
}

//////////////////////////////////////////////////////////////////////////////
// Mono PCM resampling
//////////////////////////////////////////////////////////////////////////////

// Reference implementation of the RSP mono resampler: 4-tap Catmull-Rom
// evaluated between the second and third tap, in the exact fixed point the
// ucode uses.
//
//   x    = fraction of the sample position, 0.16 unsigned
//   y0..3= the four taps around it, int16 (8-bit data is scaled by 256)
//   out  = y1
//        + x  *(y2 - y0)/2
//        + x^2*(y0 - 5*y1/2 + 2*y2 - y3/2)
//        + x^3*(-y0/2 + 3*y1/2 - 3*y2/2 + y3/2)
//
// Powers of x are truncated back to 0.16 after each product (vmudl), and every
// term is summed in the 48-bit accumulator; only the final read clamps.
static int16_t hermite_ref(int16_t y0, int16_t y1, int16_t y2, int16_t y3, uint16_t x)
{
    uint16_t x2   = (uint16_t)(((uint32_t)x  * x) >> 16);
    uint16_t x05  = (uint16_t)(((uint32_t)x  * 0x8000) >> 16);
    uint16_t x205 = (uint16_t)(((uint32_t)x2 * 0x8000) >> 16);
    uint16_t x3   = (uint16_t)(((uint32_t)x2 * x) >> 16);
    uint16_t x305 = (uint16_t)(((uint32_t)x3 * 0x8000) >> 16);

    // vsub against zero saturates, so -32768 negates to 32767.
    #define NEG(v) ((int16_t)((v) == INT16_MIN ? INT16_MAX : -(v)))
    int16_t y0n = NEG(y0), y1n = NEG(y1), y2n = NEG(y2), y3n = NEG(y3);
    #undef NEG

    int64_t acc = (int64_t)y1 << 16;
    acc += (int64_t)y2  * x05;
    acc += (int64_t)y0n * x05;
    acc += (int64_t)y0  * x2;
    acc += (int64_t)y1n * x2;
    acc += (int64_t)y1n * x2;
    acc += (int64_t)y1n * x205;
    acc += (int64_t)y2  * x2;
    acc += (int64_t)y2  * x2;
    acc += (int64_t)y3n * x205;
    acc += (int64_t)y0n * x305;
    acc += (int64_t)y1  * x3;
    acc += (int64_t)y1  * x305;
    acc += (int64_t)y2n * x3;
    acc += (int64_t)y2n * x305;
    acc += (int64_t)y3  * x305;

    int64_t v = acc >> 16;
    if (v > INT16_MAX) v = INT16_MAX;
    if (v < INT16_MIN) v = INT16_MIN;
    return (int16_t)v;
}

#define WARMUP_SAMPLES  256

// Value the one-tap volume filter of the ucode settles on for a given target.
// It is not the target itself: the accumulator is read back truncated, so the
// recurrence has a whole range of fixed points and, coming from zero, it stops
// a few LSB short. Mirrors the vmudm/vmadm pair in the ucode.
static int16_t volume_settle(int16_t target)
{
    int32_t x = 0;
    for (int i = 0; i < 1024; i++)
        x = (int32_t)(((int64_t)x * 0xe076 + (int64_t)target * 0x1f8a) >> 16);
    return (int16_t)x;
}

static bool test_pcm_resample(bool is16, uint32_t step, int nsamples, int seed)
{
    my_srand(seed);

    // Waveform, padded so that the tap window and the RSP overread past the
    // last played sample always land on readable (zero) memory.
    int bps = is16 ? 1 : 0;
    int nwave = 512;
    int wave_bytes = (nwave << bps) + 256;
    uint8_t *wave = malloc_uncached(wave_bytes);
    memset(wave, 0, wave_bytes);
    for (int i = 0; i < nwave; i++) {
        if (is16) ((int16_t*)wave)[i] = (int16_t)my_rand();
        else      ((int8_t*)wave)[i]  = (int8_t)my_rand();
    }

    int32_t *out = malloc_uncached(nsamples * 4);
    memset(out, 0, nsamples * 4);

    int16_t vol = 0x7FFF;
    uint32_t flags = CH_FLAGS_CLEAR_ACCUM | (is16 ? (CH_FLAGS_16BIT | 1) : 0);
    uint32_t len = (uint32_t)(nwave << bps) << MIXER_FX64_FRAC;

    // The volume filter converges on the target geometrically, one step every
    // 8 output samples. Run it on a zero step (which keeps replaying sample 0,
    // so it cannot run off the waveform) until it has settled.
    int32_t *warm = malloc_uncached(WARMUP_SAMPLES * 4);
    for (int i = 0; i < 4; i++) {
        rspq_highpri_begin();
        emit_channel(0, flags, vol, 0, 0, len, 0, wave, WARMUP_SAMPLES);
        emit_flush(WARMUP_SAMPLES, warm);
        rspq_highpri_end();
    }
    rspq_wait();
    free_uncached(warm);

    rspq_highpri_begin();
    emit_channel(0, flags, vol, 0, step, len, 0, wave, nsamples);
    emit_flush(nsamples, out);
    rspq_highpri_end();
    rspq_wait();

    bool ok = true;
    int16_t *stereo = (int16_t*)out;
    for (int i = 0; i < nsamples; i++) {
        uint32_t pos = step * (uint32_t)i;
        int si = pos >> (MIXER_FX64_FRAC + bps);
        uint16_t x = (uint16_t)(pos << (is16 ? 3 : 4));

        int16_t y[4];
        for (int t = 0; t < 4; t++)
            y[t] = is16 ? ((int16_t*)wave)[si+t] : (int16_t)(((int8_t*)wave)[si+t] << 8);
        int16_t res = hermite_ref(y[0], y[1], y[2], y[3], x);

        int32_t v = (int32_t)(((int64_t)res * volume_settle(vol) * 2) >> 16);
        int16_t l = stereo[i*2], r = stereo[i*2+1];
        int d0 = l - v, d1 = r - v;
        if (d0 < -2 || d0 > 2 || d1 < -2 || d1 > 2) {
            printf("FAILED resample %dbit step=%#lx: sample %d: L=%d R=%d ref=%ld\n",
                is16 ? 16 : 8, step, i, l, r, v);
            printf("  si=%d x=%#x taps=%d %d %d %d\n", si, x, y[0], y[1], y[2], y[3]);
            ok = false;
            break;
        }
    }

    free_uncached(wave);
    free_uncached(out);
    return ok;
}

// Stereo PCM: interleaved L/R in the waveform, two MIX_CHANNELs (owner=L,
// STEREO_SUB=R) that share the Hermite loop with mono.
static bool test_pcm_resample_stereo(bool is16, uint32_t step, int nsamples, int seed)
{
    my_srand(seed);

    int bps = (is16 ? 1 : 0) + 1; // stereo
    int nframes = 512;
    int wave_bytes = (nframes << bps) + 256;
    uint8_t *wave = malloc_uncached(wave_bytes);
    memset(wave, 0, wave_bytes);
    for (int i = 0; i < nframes * 2; i++) {
        if (is16) ((int16_t*)wave)[i] = (int16_t)my_rand();
        else      ((int8_t*)wave)[i]  = (int8_t)my_rand();
    }

    int32_t *out = malloc_uncached(nsamples * 4);
    memset(out, 0, nsamples * 4);

    int16_t vol = 0x7FFF;
    uint32_t flags_l = CH_FLAGS_CLEAR_ACCUM | CH_FLAGS_STEREO |
        (is16 ? (CH_FLAGS_16BIT | 2) : 1);
    uint32_t flags_r = CH_FLAGS_STEREO | CH_FLAGS_STEREO_SUB |
        (is16 ? (CH_FLAGS_16BIT | 2) : 1);
    uint32_t len = (uint32_t)(nframes << bps) << MIXER_FX64_FRAC;

    int32_t *warm = malloc_uncached(WARMUP_SAMPLES * 4);
    for (int i = 0; i < 4; i++) {
        rspq_highpri_begin();
        emit_channel_lr(0, flags_l, vol, 0, 0, 0, len, 0, wave, WARMUP_SAMPLES);
        emit_channel_lr(1, flags_r, 0, vol, 0, 0, len, 0, wave, WARMUP_SAMPLES);
        emit_flush(WARMUP_SAMPLES, warm);
        rspq_highpri_end();
    }
    rspq_wait();
    free_uncached(warm);

    rspq_highpri_begin();
    emit_channel_lr(0, flags_l, vol, 0, 0, step, len, 0, wave, nsamples);
    emit_channel_lr(1, flags_r, 0, vol, 0, step, len, 0, wave, nsamples);
    emit_flush(nsamples, out);
    rspq_highpri_end();
    rspq_wait();

    bool ok = true;
    int16_t *stereo = (int16_t*)out;
    int16_t settled = volume_settle(vol);
    for (int i = 0; i < nsamples; i++) {
        uint32_t pos = step * (uint32_t)i;
        int fi = pos >> (MIXER_FX64_FRAC + bps);
        uint16_t x = (uint16_t)(pos << (4 - bps));

        int16_t yl[4], yr[4];
        for (int t = 0; t < 4; t++) {
            if (is16) {
                yl[t] = ((int16_t*)wave)[(fi + t) * 2];
                yr[t] = ((int16_t*)wave)[(fi + t) * 2 + 1];
            } else {
                yl[t] = (int16_t)(((int8_t*)wave)[(fi + t) * 2] << 8);
                yr[t] = (int16_t)(((int8_t*)wave)[(fi + t) * 2 + 1] << 8);
            }
        }
        int16_t res_l = hermite_ref(yl[0], yl[1], yl[2], yl[3], x);
        int16_t res_r = hermite_ref(yr[0], yr[1], yr[2], yr[3], x);
        int32_t vl = (int32_t)(((int64_t)res_l * settled * 2) >> 16);
        int32_t vr = (int32_t)(((int64_t)res_r * settled * 2) >> 16);

        int16_t l = stereo[i*2], r = stereo[i*2+1];
        int d0 = l - vl, d1 = r - vr;
        if (d0 < -2 || d0 > 2 || d1 < -2 || d1 > 2) {
            printf("FAILED resample stereo %dbit step=%#lx: sample %d: L=%d/%ld R=%d/%ld\n",
                is16 ? 16 : 8, step, i, l, vl, r, vr);
            printf("  fi=%d x=%#x\n", fi, x);
            ok = false;
            break;
        }
    }

    free_uncached(wave);
    free_uncached(out);
    return ok;
}

int main(void)
{
    debug_init_emulog();
    debug_init_usblog();
    emux_ioctl_fast();

    console_init();

    printf("WAV64 VADPCM in-mixer tests\n\n");

    audio_init(44100, 4);
    mixer_init(8);

    int total = 0, failed = 0;
    int inmix_frames[] = { 1, 2, 8, 16 };
    for (int s = 0; s < 8; s++) {
        for (int fc = 0; fc < 4; fc++) {
            total++;
            if (!test_vadpcm_inmixer(inmix_frames[fc], s + 1))
                failed++;
        }
    }

    printf("Mono PCM resampling tests\n");
    fflush(stdout);
    // Enough output samples to force several cache refills, but few enough
    // that even the fastest ratio stays inside the 512-sample waveform.
    const double ratios[] = { 0.5, 1.0, 1.2891, 1.5, 2.0, 3.0, 3.7 };
    for (int b = 0; b < 2; b++) {
        bool is16 = b != 0;
        for (int r = 0; r < 7; r++) {
            uint32_t step = (uint32_t)(ratios[r] * (1 << (MIXER_FX64_FRAC + (is16 ? 1 : 0))));
            total++;
            if (!test_pcm_resample(is16, step, 96, r + 1))
                failed++;
        }
    }

    printf("Stereo PCM resampling tests\n");
    fflush(stdout);
    for (int b = 0; b < 2; b++) {
        bool is16 = b != 0;
        int bps = (is16 ? 1 : 0) + 1;
        for (int r = 0; r < 7; r++) {
            uint32_t step = (uint32_t)(ratios[r] * (1 << (MIXER_FX64_FRAC + bps)));
            total++;
            if (!test_pcm_resample_stereo(is16, step, 96, r + 10))
                failed++;
        }
    }

    if (failed) {
        printf("\n%d/%d TESTS FAILED\n", failed, total);
        abort();
    }
    printf("\nALL TESTS PASSED (%d)\n", total);
    return 0;
}
