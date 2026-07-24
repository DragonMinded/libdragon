/**
 * @file test_wav64.c
 * @brief Standalone testrom for the VADPCM RSP decompressor
 *
 * This test exercises the VADPCM decompression microcode in rsp_mixer.S by
 * decompressing a sequence of frames (so that the inter-frame context update
 * is exercised too) and comparing the RSP output, bit by bit, against the
 * reference C decoder taken from src/audio/wav64_vadpcm.c.
 *
 * The frames and codebook are synthesized in this file using a deterministic
 * PRNG, using value ranges that match what a real VADPCM encoder produces
 * (small codebook coefficients, limited scaling), so that RSP and reference
 * agree bit-exactly without hitting unrealistic intermediate overflows.
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
// Reference VADPCM decoder (copied verbatim from src/audio/wav64_vadpcm.c)
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

// Number of predictors in the synthesized codebook. Must be <= 4 so that the
// per-channel codebook (8 vectors = 4 predictors of order 2) fits in the
// 128-byte half used by the RSP for each stereo channel.
#define NPREDICTORS   4
#define ORDER         2

// RSP decompression: replicates rsp_vadpcm_decompress() from wav64_vadpcm.c.
static void rsp_vadpcm_decompress(void *input, int16_t *output, bool stereo,
    int nframes, wav64_vadpcm_vector_t *state, wav64_vadpcm_vector_t *codebook)
{
    rspq_write(__mixer_overlay_id, 0x1,
        PhysicalAddr(input),
        PhysicalAddr(output) | (nframes-1) << 24,
        PhysicalAddr(state)  | (stereo ? 1 : 0) << 31,
        PhysicalAddr(codebook));
    rspq_wait();
}

// Synthesize a realistic codebook (16 vectors = 256 bytes: 8 vectors per
// channel). Coefficients are kept small to mimic a real VADPCM encoder.
static void gen_codebook(wav64_vadpcm_vector_t *cb) {
    for (int v = 0; v < 16; v++)
        for (int i = 0; i < 8; i++)
            cb[v].v[i] = (int16_t)((my_rand() % 4096) - 2048);
}

// Synthesize a sequence of VADPCM frames (9 bytes each).
static void gen_frames(uint8_t *frames, int nframes_total) {
    for (int f = 0; f < nframes_total; f++) {
        uint8_t *fin = frames + 9 * f;
        int scaling = my_rand() % 5;            // 0..4: keeps accumulator bounded
        int predictor = my_rand() % NPREDICTORS;
        fin[0] = (scaling << 4) | predictor;
        for (int i = 1; i < 9; i++)
            fin[i] = my_rand();                 // 8 residual bytes (random nibbles)
    }
}

// out_misalign: byte offset (0/2/4/6) applied to the RDRAM output buffer to
// exercise the misaligned-output path (rdram_output & 7 != 0), which triggers
// the read-modify-write merge loop (VADPCM_OutputMergeLoop) in rsp_mixer.S.
static bool test_vadpcm(int nframes, bool stereo, int seed, int out_misalign) {
    int channels = stereo ? 2 : 1;
    my_srand(seed);

    wav64_vadpcm_vector_t *codebook = malloc_uncached(16 * sizeof(wav64_vadpcm_vector_t));
    gen_codebook(codebook);

    wav64_vadpcm_vector_t *state_rsp = malloc_uncached(2 * sizeof(wav64_vadpcm_vector_t));
    memset(state_rsp, 0, 2 * sizeof(wav64_vadpcm_vector_t));
    wav64_vadpcm_vector_t state_ref[2];
    memset(state_ref, 0, sizeof(state_ref));

    int nframes_total = nframes * channels;
    int in_bytes = 9 * nframes_total;
    uint8_t *frames = malloc(in_bytes);     // pristine copy (reference reads this)
    gen_frames(frames, nframes_total);

    // Output buffer. The RSP decoder works in-place: the compressed input is
    // placed at the tail of the output buffer (exactly like waveform_vadpcm_read).
    // To exercise the misaligned-output path we allocate a 16-byte aligned base
    // (malloc_uncached) and offset the output pointer by out_misalign bytes; the
    // decoded sample values are independent of where the buffer lands.
    int out_samples = nframes * 16 * channels;
    int out_bytes = out_samples * 2;
    int buf_bytes = (out_bytes + 15) & ~15;
    uint8_t *rsp_buf = malloc_uncached(buf_bytes + 32);
    // Fill with a sentinel so a buggy merge that corrupts neighbouring bytes
    // (before the output or in the trailing word) is more likely to be noticed.
    memset(rsp_buf, 0xA5, buf_bytes + 32);
    int16_t *rsp_out = (int16_t*)(rsp_buf + out_misalign);
    uint8_t *rsp_in = (uint8_t*)rsp_out + out_bytes - in_bytes;
    memcpy(rsp_in, frames, in_bytes);

    rsp_vadpcm_decompress(rsp_in, rsp_out, stereo, nframes, state_rsp, codebook);

    // Reference decode
    int16_t *ref_out = malloc(out_bytes);
    if (!stereo) {
        vadpcm_error err = vadpcm_decode(NPREDICTORS, ORDER, codebook, state_ref,
            nframes, ref_out, frames);
        assertf(err == 0, "reference decode error: %d", err);
    } else {
        int16_t uncomp[2][16];
        int16_t *dst = ref_out;
        uint8_t *src = frames;
        for (int i = 0; i < nframes; i++) {
            for (int j = 0; j < 2; j++) {
                vadpcm_error err = vadpcm_decode(NPREDICTORS, ORDER, codebook + 8*j,
                    &state_ref[j], 1, uncomp[j], src);
                assertf(err == 0, "reference decode error: %d", err);
                src += 9;
            }
            for (int j = 0; j < 16; j++) {
                *dst++ = uncomp[0][j];
                *dst++ = uncomp[1][j];
            }
        }
    }

    // Compare
    bool ok = true;
    for (int i = 0; i < out_samples; i++) {
        if (rsp_out[i] != ref_out[i]) {
            printf("FAILED: %s nframes=%d seed=%d misalign=%d: sample %d: rsp=%d ref=%d\n",
                stereo ? "stereo" : "mono", nframes, seed, out_misalign, i, rsp_out[i], ref_out[i]);
            ok = false;
            break;
        }
    }

    free_uncached(codebook);
    free_uncached(state_rsp);
    free_uncached(rsp_buf);
    free(frames);
    free(ref_out);
    return ok;
}

// ---------------------------------------------------------------------------
// In-mixer VADPCM path (MIX_CHANNEL + CH_FLAGS_VADPCM)
// ---------------------------------------------------------------------------

#define CH_FLAGS_16BIT       (1<<2)
#define CH_FLAGS_VADPCM      (1<<5)
#define CH_FLAGS_CLEAR_ACCUM (1<<7)
#define MIXER_FX64_FRAC      12

static bool test_vadpcm_inmixer(int nframes, int seed) {
    my_srand(seed);

    wav64_vadpcm_vector_t *codebook = malloc_uncached(16 * sizeof(wav64_vadpcm_vector_t));
    gen_codebook(codebook);

    wav64_vadpcm_vector_t *state_rsp = malloc_uncached(sizeof(wav64_vadpcm_vector_t));
    memset(state_rsp, 0, sizeof(*state_rsp));
    wav64_vadpcm_vector_t state_ref;
    memset(&state_ref, 0, sizeof(state_ref));

    int in_bytes = 9 * nframes;
    uint8_t *frames = malloc_uncached(in_bytes + 16);
    gen_frames(frames, nframes);

    int nsamples = nframes * 16;
    int16_t *ref_out = malloc(nsamples * 2);
    vadpcm_error err = vadpcm_decode(NPREDICTORS, ORDER, codebook, &state_ref,
        nframes, ref_out, frames);
    assertf(err == 0, "reference decode error: %d", err);

    int32_t *out = malloc_uncached(nsamples * 4);
    memset(out, 0, nsamples * 4);

    uint32_t step = 1u << MIXER_FX64_FRAC;
    int16_t vol = 0x7FFF;
    uint32_t flags = CH_FLAGS_VADPCM | CH_FLAGS_16BIT | CH_FLAGS_CLEAR_ACCUM;

    // Warm up the per-channel volume filter: XVOL starts at 0 in the overlay
    // saved state and converges to the channel volume via the one-tap filter
    // (one step every 8 samples), so run a few full discarded rounds first.
    // The warm-up loops over the same frames with a throwaway decoder state.
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

    // The in-mixer path multiplies by the filtered volume with vmulf, so it
    // cannot be bit-exact: XVOL settles at 0x7FFE/0x7FFF, which can shift
    // full-scale samples by up to 2 LSB.
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

    printf("WAV64 VADPCM RSP decoder tests\n\n");

    audio_init(44100, 4);
    mixer_init(8);

    int total = 0, failed = 0;
#if 0
    // TRACE MODE: single run so the RSP trace (xtracestart/xtracestop around the
    // decode loop) is short and readable; flip stereo to measure each path.
    total++;
    if (!test_vadpcm(32, true, 1, 0))
        failed++;
#else
    int frame_counts[] = { 1, 2, 16, 32 };

    // Aligned output buffer (out_misalign = 0).
    for (int s = 0; s < 8; s++) {
        for (int fc = 0; fc < 4; fc++) {
            for (int stereo = 0; stereo < 2; stereo++) {
                total++;
                if (!test_vadpcm(frame_counts[fc], stereo, s + 1, 0))
                    failed++;
            }
        }
    }

    // Misaligned output buffer (out_misalign = 2/4/6): exercises the
    // rdram_output & 7 != 0 merge path (VADPCM_OutputMergeLoop).
    for (int s = 0; s < 2; s++) {
        for (int fc = 0; fc < 4; fc++) {
            for (int stereo = 0; stereo < 2; stereo++) {
                for (int m = 2; m <= 6; m += 2) {
                    total++;
                    if (!test_vadpcm(frame_counts[fc], stereo, s + 1, m))
                        failed++;
                }
            }
        }
    }

    // In-mixer mono VADPCM (MIX_CHANNEL path). Cap at 16 frames (256 samples).
    int inmix_frames[] = { 1, 2, 8, 16 };
    for (int s = 0; s < 4; s++) {
        for (int fc = 0; fc < 4; fc++) {
            total++;
            if (!test_vadpcm_inmixer(inmix_frames[fc], s + 1))
                failed++;
        }
    }
#endif

    if (failed) {
        printf("\n%d/%d TESTS FAILED\n", failed, total);
        abort();
    }
    printf("\nALL TESTS PASSED (%d)\n", total);
    return 0;
}
