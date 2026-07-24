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

#define CH_FLAGS_16BIT       (1<<2)
#define CH_FLAGS_VADPCM      (1<<5)
#define CH_FLAGS_CLEAR_ACCUM (1<<7)
#define MIXER_FX64_FRAC      12

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
        rspq_write_t w = rspq_write_begin(__mixer_overlay_id, 0x0, 12);
        rspq_write_arg(&w, (0u << 16) | ((flags & 0xFF) << 8));
        rspq_write_arg(&w, ((uint32_t)(uint16_t)vol << 16) | (uint16_t)vol);
        rspq_write_arg(&w, 0);
        rspq_write_arg(&w, step);
        rspq_write_arg(&w, (uint32_t)nsamples << MIXER_FX64_FRAC);
        rspq_write_arg(&w, (uint32_t)nsamples << MIXER_FX64_FRAC);
        rspq_write_arg(&w, PhysicalAddr(frames));
        rspq_write_arg(&w, 256);
        rspq_write_arg(&w, PhysicalAddr(codebook));
        rspq_write_arg(&w, PhysicalAddr(state_warm));
        rspq_write_arg(&w, 0);
        rspq_write_arg(&w, 0);
        rspq_write_end(&w);
        rspq_write(__mixer_overlay_id, 0x4, 256, PhysicalAddr(warm_out), 1);
        rspq_highpri_end();
    }
    rspq_wait();
    free_uncached(state_warm);
    free_uncached(warm_out);

    rspq_highpri_begin();
    rspq_write_t w = rspq_write_begin(__mixer_overlay_id, 0x0, 12);
    rspq_write_arg(&w, (0u << 16) | ((flags & 0xFF) << 8));
    rspq_write_arg(&w, ((uint32_t)(uint16_t)vol << 16) | (uint16_t)vol);
    rspq_write_arg(&w, 0);
    rspq_write_arg(&w, step);
    rspq_write_arg(&w, 0xFFFFFFFFu);
    rspq_write_arg(&w, 0);
    rspq_write_arg(&w, PhysicalAddr(frames));
    rspq_write_arg(&w, (uint32_t)nsamples);
    rspq_write_arg(&w, PhysicalAddr(codebook));
    rspq_write_arg(&w, PhysicalAddr(state_rsp));
    rspq_write_arg(&w, 0);
    rspq_write_arg(&w, 0);
    rspq_write_end(&w);
    rspq_write(__mixer_overlay_id, 0x4, (uint32_t)nsamples, PhysicalAddr(out), 1);
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

    if (failed) {
        printf("\n%d/%d TESTS FAILED\n", failed, total);
        abort();
    }
    printf("\nALL TESTS PASSED (%d)\n", total);
    return 0;
}
