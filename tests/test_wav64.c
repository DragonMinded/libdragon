/**
 * @file test_wav64.c
 * @brief Standalone testrom for in-mixer VADPCM + PCM Hermite resampling
 *
 * Exercises MIX_CHANNEL: VADPCM decode into the sample cache followed by the
 * same 4-tap Hermite resampler used for PCM, compared against a C reference.
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

// Residual @p i of a frame, sign extended. It occupies payload bits
// [i*bits, i*bits + bits) counting bit 0 as the most significant bit of the
// first payload byte, which for bits==4 is the classic nibble packing.
static int vadpcm_get_residual(const uint8_t *frame, int i, int bits) {
    int r = 0;
    for (int b = 0; b < bits; b++) {
        int p = i * bits + b;
        r = (r << 1) | ((frame[1 + p/8] >> (7 - (p & 7))) & 1);
    }
    return (r ^ (1 << (bits-1))) - (1 << (bits-1));
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
                           const void *restrict src, int bits) {
    const uint8_t *sptr = src;
    for (size_t frame = 0; frame < frame_count; frame++) {
        const uint8_t *fin = sptr + VADPCM_FRAME_BYTES(bits) * frame;
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
            for (int i = 0; i < 8; i++)
                residuals[i] = vadpcm_get_residual(fin, 8 * vector + i, bits);
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
// channel table (MIX_CHANNEL only carries the sample pointer). The residual
// width rides in the two low bits of the codebook pointer, as in mixer.c.
static void emit_setchannel(int ch, void *codebook, void *state, int bits) {
    rspq_write(__mixer_overlay_id, MIXER_CMD_SETCHANNEL, (uint32_t)ch << 16,
        PhysicalAddr(codebook) | (bits - 2), PhysicalAddr(state), 0);
}

static void emit_channel_lr(int ch, uint32_t flags, int16_t lvol, int16_t rvol,
    uint32_t pos, uint32_t step, uint32_t len, uint32_t loop_len, void *ptr,
    int nsamples)
{
    assert((nsamples & 1) == 0 && nsamples >= 0 && nsamples <= 512);
    rspq_write_t w = rspq_write_begin(__mixer_overlay_id, MIXER_CMD_CHANNEL, 7);
    // a0: ch<<19 | flags<<11 | nsamples/2  (9 bits: nsamples up to 512)
    rspq_write_arg(&w, ((uint32_t)ch << 19) | ((flags & 0xFF) << 11) | ((uint32_t)nsamples >> 1));
    rspq_write_arg(&w, ((uint32_t)(uint16_t)lvol << 16) | (uint16_t)rvol);
    rspq_write_arg(&w, pos);
    rspq_write_arg(&w, step);
    rspq_write_arg(&w, len);
    rspq_write_arg(&w, loop_len);
    rspq_write_arg(&w, PhysicalAddr(ptr));
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

// Every bit pattern is a valid payload whatever the residual width, so the
// frames are just random bytes behind a plausible control byte.
static void gen_frames(uint8_t *frames, int nframes_total, int bits) {
    int fb = VADPCM_FRAME_BYTES(bits);
    for (int f = 0; f < nframes_total; f++) {
        uint8_t *fin = frames + fb * f;
        int scaling = my_rand() % 5;
        int predictor = my_rand() % NPREDICTORS;
        fin[0] = (scaling << 4) | predictor;
        for (int i = 1; i < fb; i++)
            fin[i] = my_rand();
    }
}

//////////////////////////////////////////////////////////////////////////////
// Hermite reference (shared by PCM and VADPCM tests)
//////////////////////////////////////////////////////////////////////////////

// Reference implementation of the RSP mono resampler: 4-tap Catmull-Rom
// evaluated between the second and third tap, in the exact fixed point the
// ucode uses.
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
static int16_t volume_settle(int16_t target)
{
    int32_t x = 0;
    for (int i = 0; i < 1024; i++)
        x = (int32_t)(((int64_t)x * 0xe076 + (int64_t)target * 0x1f8a) >> 16);
    return (int16_t)x;
}

static bool test_vadpcm_inmixer(int nframes, uint32_t step, int nout, int seed, int bits) {
    my_srand(seed);

    // Predictors + Hermite loop taps (zeros: this path does not loop).
    uint8_t *codebook_raw = malloc_uncached(VADPCM_CODEBOOK_STRIDE);
    memset(codebook_raw, 0, VADPCM_CODEBOOK_STRIDE);
    wav64_vadpcm_vector_t *codebook = (wav64_vadpcm_vector_t *)codebook_raw;
    gen_codebook(codebook);

    wav64_vadpcm_vector_t *state_rsp = malloc_uncached(sizeof(wav64_vadpcm_vector_t));
    memset(state_rsp, 0, sizeof(*state_rsp));
    wav64_vadpcm_vector_t state_ref;
    memset(&state_ref, 0, sizeof(state_ref));

    int in_bytes = VADPCM_FRAME_BYTES(bits) * nframes;
    uint8_t *frames = malloc_uncached(in_bytes + 16);
    memset(frames, 0, in_bytes + 16);
    gen_frames(frames, nframes, bits);

    int nsamples = nframes * 16;
    // Pad decoded PCM with zeros for Hermite taps past the last sample.
    int16_t *ref_pcm = malloc((size_t)(nsamples + 4) * sizeof(int16_t));
    memset(ref_pcm, 0, (size_t)(nsamples + 4) * sizeof(int16_t));
    vadpcm_error err = vadpcm_decode(NPREDICTORS, ORDER, codebook, &state_ref,
        nframes, ref_pcm, frames, bits);
    assertf(err == 0, "reference decode error: %d", err);

    int32_t *out = malloc_uncached(nout * 4);
    memset(out, 0, nout * 4);

    int16_t vol = 0x7FFF;
    uint32_t flags = CH_FLAGS_VADPCM | CH_FLAGS_16BIT | CH_FLAGS_CLEAR_ACCUM;
    uint32_t len = (uint32_t)nsamples << MIXER_FX64_FRAC;

    // Warm up the per-channel volume filter (step 0 keeps pos at 0).
    wav64_vadpcm_vector_t *state_warm = malloc_uncached(sizeof(wav64_vadpcm_vector_t));
    memset(state_warm, 0, sizeof(*state_warm));
    int32_t *warm_out = malloc_uncached(WARMUP_SAMPLES * 4);
    for (int i = 0; i < 4; i++) {
        rspq_highpri_begin();
        emit_setchannel(0, codebook, state_warm, bits);
        emit_channel(0, flags, vol, 0, 0, len, 0, frames, WARMUP_SAMPLES);
        emit_flush(WARMUP_SAMPLES, warm_out);
        rspq_highpri_end();
    }
    rspq_wait();
    free_uncached(state_warm);
    free_uncached(warm_out);

    // Fresh state for the measured run.
    memset(state_rsp, 0, sizeof(*state_rsp));
    rspq_highpri_begin();
    emit_setchannel(0, codebook, state_rsp, bits);
    emit_channel(0, flags, vol, 0, step, len, 0, frames, nout);
    emit_flush(nout, out);
    rspq_highpri_end();
    rspq_wait();

    bool ok = true;
    int16_t *stereo = (int16_t*)out;
    int16_t settled = volume_settle(vol);
    for (int i = 0; i < nout; i++) {
        uint32_t pos = step * (uint32_t)i;
        int si = pos >> MIXER_FX64_FRAC;
        uint16_t x = (uint16_t)(pos << 4);
        int16_t y0 = ref_pcm[si], y1 = ref_pcm[si+1], y2 = ref_pcm[si+2], y3 = ref_pcm[si+3];
        int16_t res = hermite_ref(y0, y1, y2, y3, x);
        int32_t v = (int32_t)(((int64_t)res * settled * 2) >> 16);
        int16_t l = stereo[i*2], r = stereo[i*2+1];
        int d0 = l - v, d1 = r - v;
        if (d0 < -2 || d0 > 2 || d1 < -2 || d1 > 2) {
            printf("FAILED vadpcm hermite %dbit: nf=%d step=%#lx seed=%d i=%d: L=%d R=%d ref=%ld\n",
                bits, nframes, (long)step, seed, i, l, r, (long)v);
            printf("  si=%d x=%#x taps=%d %d %d %d\n", si, x, y0, y1, y2, y3);
            ok = false;
            break;
        }
    }

    // State ready for floor(final_pos / 16).
    uint32_t end_pos = step * (uint32_t)nout;
    int end_frame = end_pos >> (MIXER_FX64_FRAC + 4);
    if (end_frame > nframes) end_frame = nframes;
    wav64_vadpcm_vector_t state_end;
    memset(&state_end, 0, sizeof(state_end));
    int16_t *discard = malloc((size_t)nframes * 16 * sizeof(int16_t));
    err = vadpcm_decode(NPREDICTORS, ORDER, codebook, &state_end,
        end_frame, discard, frames, bits);
    assertf(err == 0, "reference end-state decode error: %d", err);
    free(discard);
    if (ok && memcmp(state_rsp, &state_end, sizeof(state_end)) != 0) {
        printf("FAILED vadpcm hermite %dbit: nf=%d step=%#lx seed=%d: state mismatch (end_frame=%d)\n",
            bits, nframes, (long)step, seed, end_frame);
        ok = false;
    }

    free_uncached(codebook_raw);
    free_uncached(state_rsp);
    free_uncached(frames);
    free_uncached(out);
    free(ref_pcm);
    return ok;
}

//////////////////////////////////////////////////////////////////////////////
// Mono PCM resampling
//////////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////////
// Streamed stereo VADPCM through the mixer
//
// These cover the CPU side of the mixer (channel pairing, streamed refills,
// position bookkeeping) rather than the ucode, so the waveform is procedural:
// the reader synthesizes the 9-byte frames from the absolute frame index, and
// the two planes carry the same content. Two invariants follow, and each bug
// fixed in this area used to break at least one of them:
//
//  - the two output channels must stay sample-identical, since the planes are
//    identical and their volumes equal: any drift between the left and right
//    rings is immediately visible;
//  - the mixed output must not depend on where in the stream the content sits,
//    which is what test_mixer_posfold compares.
//////////////////////////////////////////////////////////////////////////////

// Long enough for the fixed point position to cross the 31st bit, which is
// where the position field sent to the RSP ends and the CPU has to start
// folding the excess into the sample pointer.
#define SV_FOLD_FRAME    (1 << (31 - MIXER_FX64_FRAC - 4))
#define SV_FRAMES        (SV_FOLD_FRAME + 4096)
#define SV_FREQ          32000
#define SV_CHANNEL       0

static wav64_vadpcm_vector_t *sv_codebook;
static int sv_shift;      // frame f of the waveform carries content f + sv_shift
static int16_t *sv_out;   // mixing buffer, sized for the longest capture
static int sv_bits = 4;   // residual width of the streamed waveforms

// Frame bytes as a pure function of the content index: any mismatch between
// the frame the mixer meant to play and the one it played changes the audio.
static void sv_gen_frame(uint8_t *out, int idx, int bits)
{
    uint32_t h = (uint32_t)idx * 2654435761u + 1;
    h ^= h >> 15;
    // Large scaling factors: the checks below have to be able to tell a
    // mistake from silence. Narrower residuals are scaled up by as much as
    // they lost, so that every width decodes to the same amplitude.
    out[0] = (uint8_t)(((6 + (4 - bits) + h % 4) << 4) | ((h >> 8) % NPREDICTORS));
    for (int i = 1; i < VADPCM_FRAME_BYTES(bits); i++) {
        h = h * 1103515245 + 12345;
        out[i] = (uint8_t)(h >> 24);
    }
}

static void sv_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    samplebuffer_t *sbuf_r = sbuf + 1;
    wav64_state_vadpcm_t *st = sbuf->state;
    (void)ctx;


    // Same rule as wav64_vadpcm.c: the right plane has no reader of its own,
    // so its ring restarts with the left one, but only when the left one
    // really restarted. A loop wrap also arrives as a seek, and there the left
    // ring keeps its live window: restarting the right one would leave the
    // planes at different stream positions.
    if (seeking && sbuf->widx == 0) {
        // Every frame here decodes from a zero predictor state, so a restart
        // is just a matter of clearing it (a real codec restores the state it
        // saved for that frame). Only on a restart: the RSP runs behind the
        // CPU, and rewriting the state under it would corrupt the frames it
        // has not decoded yet.
        memset(st->state, 0, sizeof(st->state));
        samplebuffer_flush(sbuf_r);
        sbuf_r->wpos = wpos;
        sbuf_r->head = sbuf->head;
    }

    int fb = VADPCM_FRAME_BYTES(sv_bits);
    while (wlen > 0) {
        int n = wlen < SAMPLEBUFFER_MARGIN_UNITS ? wlen : SAMPLEBUFFER_MARGIN_UNITS;
        uint8_t *dl = samplebuffer_append(sbuf, n);
        uint8_t *dr = samplebuffer_append(sbuf_r, n);
        for (int i = 0; i < n; i++) {
            sv_gen_frame(dl + i*fb, wpos + i + sv_shift, sv_bits);
            memcpy(dr + i*fb, dl + i*fb, fb);
        }
        sbuf_r->wnext = sbuf_r->wpos + sbuf_r->widx;
        wlen -= n;
        wpos += n;
    }
}

static waveform_vadpcm_t sv_codec;

static waveform_t sv_wave = {
    .name = "sv-stereo", .bits = 16, .channels = 2, .frequency = SV_FREQ,
    .len = SV_FRAMES * 16, .read = sv_read, .format = WAVEFORM_FORMAT_VADPCM,
    .codec = &sv_codec, .state_size = sizeof(wav64_state_vadpcm_t),
};

// Same stream, distinct waveform object: playing this over the other one makes
// the mixer reconfigure a channel that already holds a stereo VADPCM pair.
static waveform_t sv_wave2 = {
    .name = "sv-stereo-2", .bits = 16, .channels = 2, .frequency = SV_FREQ,
    .len = SV_FRAMES * 16, .read = sv_read, .format = WAVEFORM_FORMAT_VADPCM,
    .codec = &sv_codec, .state_size = sizeof(wav64_state_vadpcm_t),
};

// Short enough to wrap a few times over a test, long enough that the mixer
// keeps streaming it instead of pinning it into the samplebuffer.
#define SV_LOOP_FRAMES   1024

static waveform_t sv_wave_loop = {
    .name = "sv-stereo-loop", .bits = 16, .channels = 2, .frequency = SV_FREQ,
    .len = SV_LOOP_FRAMES * 16, .loop_len = SV_LOOP_FRAMES * 16,
    .read = sv_read, .format = WAVEFORM_FORMAT_VADPCM,
    .codec = &sv_codec, .state_size = sizeof(wav64_state_vadpcm_t),
};

static void sv_init(void)
{
    my_srand(4321);
    // Two codebooks, one per plane, with the same content: identical frames
    // must decode to identical samples on both sides. Each plane is
    // VADPCM_CODEBOOK_STRIDE bytes (predictors + zeroed loop taps).
    sv_codebook = malloc_uncached(2 * VADPCM_CODEBOOK_STRIDE);
    memset(sv_codebook, 0, 2 * VADPCM_CODEBOOK_STRIDE);
    gen_codebook(sv_codebook);
    memcpy((uint8_t *)sv_codebook + VADPCM_CODEBOOK_STRIDE, sv_codebook,
        8 * sizeof(wav64_vadpcm_vector_t));
    sv_codec.codebook = sv_codebook;
    sv_codec.loop_state = NULL;
    sv_codec.bits = sv_bits;
    sv_out = malloc_uncached(8192 * 4);
    // The sweep goes above the output rate, so the buffers must be sized for
    // the fastest playback the channel will ever see.
    mixer_ch_set_limits(SV_CHANNEL, 0, 48000, 0);
}

// The frames are generated on the fly, so switching the residual width of the
// streamed waveforms is just a matter of telling generator and mixer about it.
static void sv_set_bits(int bits)
{
    sv_bits = bits;
    sv_codec.bits = bits;
}

// Samples per #mixer_poll: one call enqueues that many samples worth of mix
// rounds before syncing the RSP, so this is how far the CPU is allowed to run
// ahead of it. Games poll one audio buffer at a time (~1000 samples).
static int sv_chunk = 1024;

static void sv_mix(int nsamples)
{
    int16_t *out = sv_out;
    while (nsamples > 0) {
        int n = nsamples < sv_chunk ? nsamples : sv_chunk;
        mixer_poll(out, n);
        out += n * 2;
        nsamples -= n;
    }
}

// Silence everything and let the volume ramps decay, so that each test starts
// from the same state.
static void sv_silence(void)
{
    mixer_ch_stop(SV_CHANNEL);
    sv_mix(2048);
}

// Start the waveform, then mix (and discard) enough samples for the volume
// ramps of both channels of the pair to settle on their target.
static void sv_start(waveform_t *wave, int startsample, float freq)
{
    sv_silence();
    mixer_ch_play(SV_CHANNEL, wave);
    mixer_ch_set_freq(SV_CHANNEL, freq);
    mixer_ch_set_vol(SV_CHANNEL, 0.5f, 0.5f);
    if (startsample)
        mixer_ch_set_pos(SV_CHANNEL, startsample);
    sv_mix(2048);
}

// The planes are identical and the volumes equal, so the two output channels
// must carry the same audio: they do not if the rings drift apart.
static bool sv_check_lr(const char *tag, int nsamples)
{
    // Not bit-identical though: each channel runs its own volume ramp, and
    // integer truncation gives that filter a small band of fixed points rather
    // than a single one, so two ramps that started from different values can
    // settle a few units apart and stay there.
    int peak = 0, nbad = 0, first = -1, maxdiff = 0;
    for (int i = 0; i < nsamples; i++) {
        int16_t l = sv_out[i*2], r = sv_out[i*2+1];
        int d = l - r; if (d < 0) d = -d;
        if (d > 32) {
            nbad++;
            if (first < 0) first = i;
            if (d > maxdiff) maxdiff = d;
        }
        int a = l < 0 ? -l : l;
        if (a > peak) peak = a;
    }
    if (nbad) {
        printf("FAILED %s: L/R differ on %d/%d samples (max %d) from %d\n",
            tag, nbad, nsamples, maxdiff, first);
        for (int i = first; i < first + 6 && i < nsamples; i++)
            printf("   [%d] %d / %d\n", i, sv_out[i*2], sv_out[i*2+1]);
        return false;
    }
    // A silent output would satisfy every check in this file.
    if (peak < 4096) {
        printf("FAILED %s: output is silent (peak %d)\n", tag, peak);
        return false;
    }
    return true;
}

// Stream at several playback rates and from several start positions, including
// ones that are not frame aligned: the size of a refill depends on both, and
// underestimating it by a single frame trips an assertion in samplebuffer_get.
static bool test_mixer_stream(float freq, int startsample)
{
    sv_shift = 0;
    sv_start(&sv_wave, startsample, freq);

    for (int i = 0; i < 8; i++) {
        sv_mix(4096);
        if (!sv_check_lr("mixer stream", 4096)) {
            printf("  freq=%d startsample=%d iter=%d\n", (int)freq, startsample, i);
            return false;
        }
    }
    return true;
}

// The position the RSP receives is 31 bits wide, so past 2^31 the CPU has to
// carry the excess in the sample pointer instead. Play the same content twice,
// once from the start of the stream and once from around the boundary: the
// mixed output must be identical.
static bool test_mixer_posfold(void)
{
    int startframe = SV_FOLD_FRAME - 16;
    int nsamples = 4096;
    int16_t *ref = malloc(nsamples * 4);

    // Reference: the same frames, but early enough in the stream that the
    // position never reaches the boundary.
    sv_shift = startframe;
    sv_start(&sv_wave, 0, SV_FREQ);
    sv_mix(nsamples);
    memcpy(ref, sv_out, nsamples * 4);
    bool ok = sv_check_lr("mixer posfold (reference)", nsamples);

    // Same content, now sitting across the 2^31 boundary.
    sv_shift = 0;
    sv_start(&sv_wave, startframe * 16, SV_FREQ);
    sv_mix(nsamples);
    ok = ok && sv_check_lr("mixer posfold", nsamples);

    // Same tolerance as sv_check_lr: the two runs settle their volume ramps
    // independently.
    for (int i = 0; ok && i < nsamples * 2; i++) {
        int d = ref[i] - sv_out[i]; if (d < 0) d = -d;
        if (d > 32) {
            printf("FAILED mixer posfold: sample %d: %d/%d, expected %d/%d\n",
                i/2, sv_out[(i/2)*2], sv_out[(i/2)*2+1], ref[(i/2)*2], ref[(i/2)*2+1]);
            ok = false;
        }
    }

    free(ref);
    return ok;
}

// Stopping a stereo channel releases the secondary one. If it is left armed,
// the mixer keeps it as an independent channel: it advances it and tries to
// refill its samplebuffer, which has no reader of its own.
static bool test_mixer_stop_releases_sub(void)
{
    sv_shift = 0;
    sv_start(&sv_wave, 0, SV_FREQ);
    sv_mix(4096);
    if (!sv_check_lr("mixer stop", 4096))
        return false;

    mixer_ch_stop(SV_CHANNEL);
    if (mixer_ch_playing(SV_CHANNEL + 1)) {
        printf("FAILED mixer stop: channel %d still playing after stopping %d\n",
            SV_CHANNEL + 1, SV_CHANNEL);
        return false;
    }

    // Mix well past the silence ramp: this is what used to run the orphaned
    // channel off the end of its buffered window.
    for (int i = 0; i < 8; i++)
        sv_mix(4096);
    for (int i = 0; i < 4096; i++) {
        if (sv_out[i*2] || sv_out[i*2+1]) {
            printf("FAILED mixer stop: still audible at sample %d: %d/%d\n",
                i, sv_out[i*2], sv_out[i*2+1]);
            return false;
        }
    }
    return true;
}

// Replacing the waveform of a channel that holds a stereo VADPCM pair has to
// reconfigure both rings, including the secondary one that only the owner ever
// fills.
static bool test_mixer_switch_waveform(void)
{
    sv_shift = 0;
    sv_start(&sv_wave, 0, SV_FREQ);
    sv_mix(4096);

    waveform_t *waves[] = { &sv_wave2, &sv_wave, &sv_wave2 };
    for (int i = 0; i < 3; i++) {
        mixer_ch_play(SV_CHANNEL, waves[i]);
        mixer_ch_set_vol(SV_CHANNEL, 0.5f, 0.5f);
        sv_mix(2048);
        sv_mix(4096);
        if (!sv_check_lr("mixer switch waveform", 4096)) {
            printf("  after switching to %s\n", waves[i]->name);
            return false;
        }
    }
    return true;
}

// A loop wrap refills the ring from the loop start without restarting it: the
// two planes must go through that together.
//
// The wrap also re-seeds the decoder, which is where the state the RSP reads
// and the one the CPU writes have to be kept apart: the epilog of the rounds
// still queued saves its own state for each plane at a different moment, and
// used to overwrite the seed of one of them. This waveform declares no
// loop_state, so nothing else covers up a seed that gets lost.
static bool test_mixer_loop(void)
{
    sv_shift = 0;
    sv_start(&sv_wave_loop, (SV_LOOP_FRAMES - 512) * 16, SV_FREQ);

    // Enough to go around the loop a few times.
    for (int i = 0; i < 16; i++) {
        sv_mix(4096);
        if (!sv_check_lr("mixer loop", 4096)) {
            printf("  iter=%d bits=%d\n", i, sv_bits);
            return false;
        }
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Streamed mono PCM through the mixer
//
// Covers two bugs that only show up with uncompressed PCM (the path XM takes
// with --xm-compress 0), neither of which the VADPCM stream tests reach:
//
//  - mixer_refresh_max_ns must reserve one unit for the inclusive span that
//    mixer_channel_window counts (last-pos+1). Without it, a step below 1.0
//    asks samplebuffer_get for SAMPLEBUFFER_MARGIN_UNITS+1 and asserts.
//  - 8-bit mono has unit_bytes 1, so an odd-length waveform leaves the ring
//    write cursor on an odd index after a discard; samplebuffer_append must
//    accept that state as long as head and wpos keep the same byte parity.
//////////////////////////////////////////////////////////////////////////////

#define SP_FREQ     44100
#define SP_LEN      10001   // odd on purpose
#define SP_CHANNEL  2

static void sp_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    (void)ctx; (void)seeking;
    while (wlen > 0) {
        int n = wlen < SAMPLEBUFFER_MARGIN_UNITS ? wlen : SAMPLEBUFFER_MARGIN_UNITS;
        int8_t *d = samplebuffer_append(sbuf, n);
        for (int i = 0; i < n; i++)
            d[i] = (int8_t)((wpos + i) * 17 + 3);
        wlen -= n;
        wpos += n;
    }
}

static waveform_t sp_wave = {
    .name = "sp-mono8", .bits = 8, .channels = 1, .frequency = SP_FREQ,
    .len = SP_LEN, .loop_len = SP_LEN, .read = sp_read,
};

// Stream 8-bit mono PCM below the sample rate, so the resampler step is less
// than one and the inclusive window is one unit larger than the position
// advance. Crossing the loop point also forces odd-sized fragments through
// waveform_read, which is what leaves the ring with an odd wpos.
static bool test_mixer_stream_pcm8(float freq, int startsample)
{
    mixer_ch_stop(SP_CHANNEL);
    sv_mix(2048);

    mixer_ch_set_limits(SP_CHANNEL, 8, 48000, 0);
    mixer_ch_play(SP_CHANNEL, &sp_wave);
    mixer_ch_set_freq(SP_CHANNEL, freq);
    mixer_ch_set_vol(SP_CHANNEL, 0.5f, 0.5f);
    if (startsample)
        mixer_ch_set_pos(SP_CHANNEL, startsample);
    sv_mix(2048);

    int peak = 0;
    for (int i = 0; i < 16; i++) {
        sv_mix(4096);
        for (int j = 0; j < 4096 * 2; j++) {
            int a = sv_out[j] < 0 ? -sv_out[j] : sv_out[j];
            if (a > peak) peak = a;
        }
    }
    mixer_ch_stop(SP_CHANNEL);
    sv_mix(2048);

    if (peak < 256) {
        printf("FAILED mixer stream pcm8: silent (peak %d) freq=%d start=%d\n",
            peak, (int)freq, startsample);
        return false;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Partial VADPCM frames
//
// A waveform whose length is not a multiple of 16 ends with a partial frame,
// and audioconv64 pads the encoded stream past it. The samples in that frame
// are as playable as any other, so the mixer has to fetch it: dropping it
// leaves a hole right before the loop point, filled with whatever the pinned
// copy holds there (the loop start) and played as noise on every wrap.
//
// Sizes taken from instrument 1 of Arcade_S900.xm, where this showed up.
//////////////////////////////////////////////////////////////////////////////

#define LV_LEN        1045
#define LV_LOOP_LEN   277

static int lv_frames;    // frames the mixer asked for, as a one-past-the-end index
static bool lv_oob;      // ...and whether it went past what a file would hold

static void lv_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    wav64_state_vadpcm_t *st = sbuf->state;
    (void)ctx;

    if (seeking && sbuf->widx == 0)
        memset(st->state, 0, sizeof(st->state));
    if (wpos + wlen > WAV64_VADPCM_FILE_FRAMES(LV_LEN))
        lv_oob = true;
    if (wpos + wlen > lv_frames)
        lv_frames = wpos + wlen;

    int fb = VADPCM_FRAME_BYTES(sv_bits);
    while (wlen > 0) {
        int n = wlen < SAMPLEBUFFER_MARGIN_UNITS ? wlen : SAMPLEBUFFER_MARGIN_UNITS;
        uint8_t *dst = samplebuffer_append(sbuf, n);
        for (int i = 0; i < n; i++)
            sv_gen_frame(dst + i*fb, wpos + i, sv_bits);
        wlen -= n;
        wpos += n;
    }
}

static waveform_t lv_wave = {
    .name = "lv-partial", .bits = 16, .channels = 1, .frequency = SV_FREQ,
    .len = LV_LEN, .loop_len = LV_LOOP_LEN, .read = lv_read,
    .format = WAVEFORM_FORMAT_VADPCM,
    .codec = &sv_codec, .state_size = sizeof(wav64_state_vadpcm_t),
};

static bool test_mixer_vadpcm_partial_frame(float freq)
{
    lv_frames = 0;
    lv_oob = false;
    sv_start(&lv_wave, 0, freq);
    sv_mix(4096);

    int need = WAV64_VADPCM_FRAMES(LV_LEN);
    if (lv_frames < need) {
        printf("FAILED vadpcm partial frame: fetched %d frames out of %d (freq=%d)\n",
            lv_frames, need, (int)freq);
        return false;
    }
    if (lv_oob) {
        printf("FAILED vadpcm partial frame: read past the padding (freq=%d)\n", (int)freq);
        return false;
    }
    int peak = 0;
    for (int i = 0; i < 4096 * 2; i++) {
        int a = sv_out[i] < 0 ? -sv_out[i] : sv_out[i];
        if (a > peak) peak = a;
    }
    // Just a sanity check: silence would satisfy everything above.
    if (peak < 1024) {
        printf("FAILED vadpcm partial frame: output is silent (freq=%d peak=%d)\n", (int)freq, peak);
        return false;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Resampling across a loop point
//
// The resampler reads up to three taps past the sample it interpolates, so at
// the end of a waveform it needs whatever follows it: for a loop, the samples
// at the loop start. The VADPCM path used to hand it four zeroes there, which
// on a short loop is a dip to silence a few hundred times a second.
//
// A constant waveform makes any such tap visible: Hermite reproduces a
// constant exactly, so the output must be flat everywhere.
//////////////////////////////////////////////////////////////////////////////

#define LP_SHIFT     12
#define LP_RESIDUAL  4

static wav64_vadpcm_vector_t *lp_codebook;   // zeroed: sample = residual << shift
static waveform_vadpcm_t lp_codec;

static void lp_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    (void)ctx; (void)wpos; (void)seeking;
    while (wlen > 0) {
        int n = wlen < SAMPLEBUFFER_MARGIN_UNITS ? wlen : SAMPLEBUFFER_MARGIN_UNITS;
        uint8_t *dst = samplebuffer_append(sbuf, n);
        for (int i = 0; i < n; i++) {
            dst[i*9] = LP_SHIFT << 4;
            memset(dst + i*9 + 1, (LP_RESIDUAL << 4) | LP_RESIDUAL, 8);
        }
        wlen -= n;
        wpos += n;
    }
}

static waveform_t lp_wave = {
    .name = "lp-flat", .bits = 16, .channels = 1, .frequency = SV_FREQ,
    .len = LV_LEN, .loop_len = LV_LOOP_LEN, .read = lp_read,
    .format = WAVEFORM_FORMAT_VADPCM,
    .codec = &lp_codec, .state_size = sizeof(wav64_state_vadpcm_t),
};

// A loop too long to be pinned into the samplebuffer is wrapped by the CPU
// between rounds, with a seek back to the loop point: sizes and loop point of
// instrument 5 of Arcade_S900, which a tracker plays three octaves up (eight
// input samples per output one).
static waveform_t lp_wave_big = {
    .name = "lp-flat-bigloop", .bits = 16, .channels = 1, .frequency = SV_FREQ,
    .len = 57872, .loop_len = 8752, .read = lp_read,
    .format = WAVEFORM_FORMAT_VADPCM,
    .codec = &lp_codec, .state_size = sizeof(wav64_state_vadpcm_t),
};

static bool test_mixer_loop_taps(waveform_t *wave, float freq, int startsample)
{
    if (!lp_codebook) {
        // Zero predictors: sample = residual << shift. Loop taps match the
        // constant the frames decode to so Hermite stays flat across wraps.
        int16_t flat = (int16_t)(LP_RESIDUAL << LP_SHIFT);
        lp_codebook = malloc_uncached(VADPCM_CODEBOOK_STRIDE);
        memset(lp_codebook, 0, VADPCM_CODEBOOK_STRIDE);
        int16_t *taps = (int16_t *)((uint8_t *)lp_codebook + 128);
        taps[0] = taps[1] = taps[2] = flat;
        lp_codec.codebook = lp_codebook;
        lp_codec.bits = 4;
        // A non-NULL loop_state arms the tap path in the ucode (the vector
        // itself is unused for this constant waveform).
        wav64_vadpcm_vector_t *lp_loop_state = malloc_uncached(sizeof(*lp_loop_state));
        memset(lp_loop_state, 0, sizeof(*lp_loop_state));
        lp_codec.loop_state = lp_loop_state;
    }
    sv_silence();
    mixer_ch_set_limits(SV_CHANNEL, 0, freq > 48000 ? freq : 48000, 0);
    sv_start(wave, startsample, freq);
    sv_mix(4096);

    int lo = INT_MAX, hi = INT_MIN, first = -1;
    for (int i = 0; i < 4096 * 2; i++) {
        if (sv_out[i] < lo) lo = sv_out[i];
        if (sv_out[i] > hi) { hi = sv_out[i]; }
    }
    for (int i = 0; i < 4096 * 2 && first < 0; i++)
        if (hi - sv_out[i] > 64) first = i;
    if (hi - lo > 64) {
        printf("FAILED loop taps %s: output swings %d..%d (freq=%d), first dip at %d\n",
            wave->name, lo, hi, (int)freq, first/2);
        for (int i = first - 2; i < first + 4 && i < 4096*2; i++)
            if (i >= 0) printf("   [%d] %d\n", i/2, sv_out[i]);
        return false;
    }
    if (hi < 1024) {
        printf("FAILED loop taps %s: output is silent (freq=%d)\n", wave->name, (int)freq);
        return false;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Looping VADPCM, sample by sample
//
// The tests above play a constant, which shows samples that are dropped or
// zeroed but not samples played at the wrong position, twice, or not at all.
// Playing at the output rate turns the resampler into an identity (Hermite
// with a null fraction returns the sample it interpolates from), so every
// output sample can be matched against the waveform decoded on the CPU, with
// the loop unrolled by hand.
//////////////////////////////////////////////////////////////////////////////

// Instrument 5 of Arcade_S900.xm: only its loop fits the samplebuffer, so it
// is streamed until playback reaches the loop point and pinned from there, and
// neither the length nor the loop length is a whole number of frames.
#define RF_LEN        10377
#define RF_LOOP_LEN   1513
#define RF_FRAMES     ((RF_LEN + 15) / 16)
#define RF_LOOP_START (RF_LEN - RF_LOOP_LEN)

static wav64_vadpcm_vector_t *rf_codebook;
static wav64_vadpcm_vector_t *rf_loop_state;
static uint8_t *rf_frames;    // the waveform, encoded...
static int16_t *rf_ref;       // ...and the samples it decodes to
static bool rf_oob;           // reader was asked for frames the file has not
static waveform_vadpcm_t rf_codec;
static int rf_bits = 4;       // residual width of the encoded waveform

static void rf_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    (void)ctx;
    if (wpos < 0 || wpos + wlen > RF_FRAMES)
        rf_oob = true;
    if (seeking) {
        // The state a frame decodes from is just the eight samples before it.
        wav64_state_vadpcm_t *st = sbuf->state;
        for (int i = 0; i < 8; i++) {
            int p = wpos * 16 - 8 + i;
            st->state[0].v[i] = p >= 0 ? rf_ref[p] : 0;
        }
    }
    int fb = VADPCM_FRAME_BYTES(rf_bits);
    while (wlen > 0) {
        int n = wlen < SAMPLEBUFFER_MARGIN_UNITS ? wlen : SAMPLEBUFFER_MARGIN_UNITS;
        uint8_t *dst = samplebuffer_append(sbuf, n);
        memcpy(dst, rf_frames + wpos * fb, n * fb);
        wlen -= n;
        wpos += n;
    }
}

static waveform_t rf_wave = {
    .name = "rf-loop", .bits = 16, .channels = 1, .frequency = 44100,
    .len = RF_LEN, .loop_len = RF_LOOP_LEN, .read = rf_read,
    .format = WAVEFORM_FORMAT_VADPCM,
    .codec = &rf_codec, .state_size = sizeof(wav64_state_vadpcm_t),
};

// Same stream without the loop: what happens between two rounds does not
// depend on it.
static waveform_t rf_wave_plain = {
    .name = "rf-plain", .bits = 16, .channels = 1, .frequency = 44100,
    .len = RF_FRAMES * 16, .read = rf_read,
    .format = WAVEFORM_FORMAT_VADPCM,
    .codec = &rf_codec, .state_size = sizeof(wav64_state_vadpcm_t),
};

static void rf_init(int bits)
{
    my_srand(20250802);
    rf_bits = bits;
    int fb = VADPCM_FRAME_BYTES(bits);
    if (!rf_codebook)
        rf_codebook = malloc_uncached(VADPCM_CODEBOOK_STRIDE);
    memset(rf_codebook, 0, VADPCM_CODEBOOK_STRIDE);
    gen_codebook(rf_codebook);
    free(rf_frames);
    rf_frames = malloc(RF_FRAMES * fb);
    for (int f = 0; f < RF_FRAMES; f++)
        sv_gen_frame(rf_frames + f * fb, f, bits);
    free(rf_ref);
    rf_ref = malloc(RF_FRAMES * 16 * 2);
    wav64_vadpcm_vector_t state = {0};
    vadpcm_decode(NPREDICTORS, ORDER, rf_codebook, &state, RF_FRAMES, rf_ref, rf_frames, bits);
    // The loop start is frame aligned (as audioconv64 makes sure it is), so
    // the state to resume it from is the one of that frame. The first three
    // samples of that frame ride after the codebook, as in a real WAV64.
    if (!rf_loop_state)
        rf_loop_state = malloc_uncached(sizeof(*rf_loop_state));
    for (int i = 0; i < 8; i++)
        rf_loop_state->v[i] = rf_ref[RF_LOOP_START - 8 + i];
    int16_t *taps = (int16_t *)((uint8_t *)rf_codebook + 128);
    taps[0] = rf_ref[RF_LOOP_START + 0];
    taps[1] = rf_ref[RF_LOOP_START + 1];
    taps[2] = rf_ref[RF_LOOP_START + 2];
    rf_codec.codebook = rf_codebook;
    rf_codec.loop_state = rf_loop_state;
    rf_codec.bits = bits;
}

// Sample played when the position is @p pos, with the loop unrolled. The
// resampler interpolates between its second and third tap, so a position sits
// one sample before what it plays.
static int rf_len, rf_loop_len;   // waveform being played

static int rf_play(int pos)
{
    pos++;
    if (rf_loop_len && pos >= rf_len)
        pos = rf_len - rf_loop_len + (pos - rf_len) % rf_loop_len;
    return pos;
}

static int rf_expect(int pos)
{
    return rf_ref[rf_play(pos)];
}

static bool test_mixer_loop_exact(waveform_t *wave, int startsample)
{
    const int nsamples = 4096;
    rf_len = wave->len;
    rf_loop_len = wave->loop_len;
    sv_silence();
    rf_oob = false;
    sv_start(wave, startsample, 44100);

    // The warm-up mixed an unknown number of samples: ask the mixer where the
    // capture starts from.
    int pos = (int)mixer_ch_get_pos(SV_CHANNEL);
    sv_mix(nsamples);

    int nbad = 0, first = -1, maxdiff = 0, peak = 0;
    for (int i = 0; i < nsamples; i++) {
        // Half volume, and the volume filter settles a couple of units short
        // of its target.
        int want = rf_expect(pos + i) / 2;
        int got = sv_out[i*2];
        int d = got - want; if (d < 0) d = -d;
        if (d > 64) {
            nbad++;
            if (first < 0) first = i;
            if (d > maxdiff) maxdiff = d;
        }
        if (got > peak) peak = got; else if (-got > peak) peak = -got;
    }
    if (nbad) {
        printf("FAILED exact %s (start=%d): %d/%d samples off (max %d)\n",
            wave->name, startsample, nbad, nsamples, maxdiff);
        // Where each burst of wrong samples begins, and how the position it
        // plays sits inside its frame.
        for (int i = 0, nrun = 0; i < nsamples && nrun < 5; i++) {
            int d = sv_out[i*2] - rf_expect(pos+i)/2; if (d < 0) d = -d;
            if (d <= 64) continue;
            int j = i + 1, quiet = 0;
            while (j < nsamples && quiet < 16) {
                int d2 = sv_out[j*2] - rf_expect(pos+j)/2; if (d2 < 0) d2 = -d2;
                quiet = d2 > 64 ? 0 : quiet + 1;
                j++;
            }
            printf("   [%d..%d] play=%d (frame %d + %d)\n", i, j-1-quiet,
                rf_play(pos+i), rf_play(pos+i) / 16, rf_play(pos+i) % 16);
            nrun++;
            i = j;
        }
        for (int i = first - 2; i < first + 6 && i < nsamples; i++)
            if (i >= 0) printf("   [%d] play=%d got %d want %d\n",
                i, rf_play(pos+i), sv_out[i*2], rf_expect(pos+i) / 2);
        return false;
    }
    if (rf_oob) {
        printf("FAILED exact %s (start=%d): read past the end of the waveform\n",
            wave->name, startsample);
        return false;
    }
    if (peak < 4096) {
        printf("FAILED exact %s (start=%d): output is silent (peak %d)\n",
            wave->name, startsample, peak);
        return false;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Tracker-style retriggering
//
// A tracker restarts a channel from its playback callback several times a
// second, usually on a different instrument: a different codebook, a different
// stream, and a decoder state that has to go back to zero — all of it while
// the rounds of the previous note are still queued for the RSP.
//
// Each waveform here carries a codebook of its own, unlike the pair the stereo
// tests switch between: pointers that reach the ucode out of order decode the
// new frames into garbage that two identical planes would agree on.
//////////////////////////////////////////////////////////////////////////////

#define IS_NWAVES    3
#define IS_FRAMES    768        // 12288 samples: longer than any note played
#define IS_ROW       1024       // output samples between two retriggers
#define IS_NROWS     8

static wav64_vadpcm_vector_t *is_codebook[IS_NWAVES];
static uint8_t *is_frames[IS_NWAVES];
static int16_t *is_ref[IS_NWAVES];
static waveform_vadpcm_t is_codec[IS_NWAVES];
static waveform_t is_wave[IS_NWAVES];
static bool is_oob;             // reader was asked for frames outside the waveform

static void is_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    int w = sbuf->wave - is_wave;
    (void)ctx;

    if (wpos < 0 || wpos + wlen > IS_FRAMES)
        is_oob = true;
    if (seeking) {
        // The state a frame decodes from is just the eight samples before it.
        wav64_state_vadpcm_t *st = sbuf->state;
        for (int i = 0; i < 8; i++) {
            int p = wpos * 16 - 8 + i;
            st->state[0].v[i] = p >= 0 ? is_ref[w][p] : 0;
        }
    }
    while (wlen > 0) {
        int n = wlen < SAMPLEBUFFER_MARGIN_UNITS ? wlen : SAMPLEBUFFER_MARGIN_UNITS;
        uint8_t *dst = samplebuffer_append(sbuf, n);
        memcpy(dst, is_frames[w] + wpos * 9, n * 9);
        wlen -= n;
        wpos += n;
    }
}

static void is_init(void)
{
    static const char *names[IS_NWAVES] = { "is-note0", "is-note1", "is-note2" };
    for (int w = 0; w < IS_NWAVES; w++) {
        my_srand(0x51D0 + w * 7717);
        is_codebook[w] = malloc_uncached(VADPCM_CODEBOOK_STRIDE);
        memset(is_codebook[w], 0, VADPCM_CODEBOOK_STRIDE);
        gen_codebook(is_codebook[w]);
        is_frames[w] = malloc(IS_FRAMES * 9);
        for (int f = 0; f < IS_FRAMES; f++)
            sv_gen_frame(is_frames[w] + f * 9, w * 100000 + f, 4);
        is_ref[w] = malloc(IS_FRAMES * 16 * 2);
        wav64_vadpcm_vector_t state = {0};
        vadpcm_decode(NPREDICTORS, ORDER, is_codebook[w], &state, IS_FRAMES,
            is_ref[w], is_frames[w], 4);
        is_codec[w] = (waveform_vadpcm_t){
            .codebook = is_codebook[w], .loop_state = NULL, .bits = 4,
        };
        is_wave[w] = (waveform_t){
            .name = names[w], .bits = 16, .channels = 1, .frequency = 44100,
            .len = IS_FRAMES * 16, .read = is_read,
            .format = WAVEFORM_FORMAT_VADPCM,
            .codec = &is_codec[w], .state_size = sizeof(wav64_state_vadpcm_t),
        };
    }
}

static int is_cur;              // waveform the tick is playing
static bool is_same_wave;       // retrigger the same one instead of switching
static float is_freq;           // playback rate of every note
static uint8_t is_row_wave[IS_NROWS + 2];
static int is_nrows;

static int is_tick(void *ctx)
{
    (void)ctx;
    if (!is_same_wave)
        is_cur = (is_cur + 1) % IS_NWAVES;
    // Same shape as the xm64 tick: play the waveform only if it is not the one
    // on the channel already, then restart it from the top.
    if (mixer_ch_playing_waveform(SV_CHANNEL) != &is_wave[is_cur])
        mixer_ch_play(SV_CHANNEL, &is_wave[is_cur]);
    mixer_ch_set_pos(SV_CHANNEL, 0);
    mixer_ch_set_freq(SV_CHANNEL, is_freq);
    mixer_ch_set_vol(SV_CHANNEL, 0.5f, 0.5f);
    if (is_nrows < (int)sizeof(is_row_wave))
        is_row_wave[is_nrows++] = is_cur;
    return IS_ROW;
}

// Output sample @p j of a note, at half volume: the same step the mixer
// derives from the playback rate, and the resampler of the ucode.
static int is_expect(int w, int j, float freq)
{
    uint32_t step = (uint32_t)(int64_t)((freq / 44100.0f) * (1 << MIXER_FX64_FRAC));
    uint32_t pos = step * (uint32_t)j;
    const int16_t *r = is_ref[w] + (pos >> MIXER_FX64_FRAC);
    return hermite_ref(r[0], r[1], r[2], r[3], (uint16_t)(pos << 4)) / 2;
}

static bool test_mixer_retrigger(bool same_wave, float freq)
{
    const int nsamples = IS_ROW * IS_NROWS;
    const char *tag = same_wave ? "retrigger" : "instrument switch";

    sv_silence();
    is_oob = false;
    is_cur = 0;
    is_same_wave = same_wave;
    is_freq = freq;
    is_nrows = 0;
    mixer_ch_play(SV_CHANNEL, &is_wave[0]);
    mixer_ch_set_freq(SV_CHANNEL, freq);
    mixer_ch_set_vol(SV_CHANNEL, 0.5f, 0.5f);
    sv_mix(2048);                     // settle the volume ramp

    mixer_add_event(IS_ROW, is_tick, NULL);
    sv_mix(nsamples);
    mixer_remove_event(is_tick, NULL);

    int peak = 0;
    for (int i = 0; i < nsamples * 2; i++) {
        int a = sv_out[i] < 0 ? -sv_out[i] : sv_out[i];
        if (a > peak) peak = a;
    }

    // A retrigger can be served up to one sample late (a round is an even
    // number of samples), so line each row up with the note it plays before
    // comparing it.
    for (int row = 1; row < IS_NROWS; row++) {
        int w = is_row_wave[row-1];
        int base = row * IS_ROW;
        int bestbad = INT_MAX, bestd = 0;
        for (int d = -2; d <= 2; d++) {
            int bad = 0;
            for (int j = 8; j < IS_ROW - 8; j++) {
                int e = sv_out[(base + j) * 2] - is_expect(w, j + d, freq);
                if (e < -64 || e > 64) bad++;
            }
            if (bad < bestbad) { bestbad = bad; bestd = d; }
        }
        if (bestbad) {
            printf("FAILED mixer %s (freq=%d chunk=%d): row %d (%s): %d/%d samples off (d=%d)\n",
                tag, (int)freq, sv_chunk, row, is_wave[w].name, bestbad, IS_ROW - 16, bestd);
            int first = 8;
            while (first < IS_ROW - 8) {
                int e = sv_out[(base + first) * 2] - is_expect(w, first + bestd, freq);
                if (e < -64 || e > 64) break;
                first++;
            }
            for (int j = first - 4; j < first + 12 && j < IS_ROW - 8; j++)
                printf("   [%d] pos=%d got %6d want %6d\n", base + j, j,
                    sv_out[(base + j) * 2], is_expect(w, j + bestd, freq));
            return false;
        }
    }
    if (is_oob) {
        printf("FAILED mixer %s (freq=%d): read outside the waveform\n", tag, (int)freq);
        return false;
    }
    if (peak < 4096) {
        printf("FAILED mixer %s (freq=%d): output is silent (peak %d)\n",
            tag, (int)freq, peak);
        return false;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Pinned loops
//
// A loop short enough to fit the samplebuffer is copied there once and left
// pinned, so that the RSP can wrap it by itself. The copy must be followed by
// the samples the resampler reads past the loop end, and those are the ones at
// the loop start: taking them from past the end of the waveform instead turns
// every wrap into a burst of noise.
//
// The reference is the same signal with the loop already unrolled, which is
// long enough that the mixer streams it normally instead of pinning it.
//////////////////////////////////////////////////////////////////////////////

static int lc_len, lc_loop_len;   // loop being played, in samples
static bool lc_oob;               // reader was asked for samples past the end

// Noise-like, so that a tap taken from the wrong place cannot pass for a
// plausible sample.
static int16_t lc_sample(int pos)
{
    uint32_t h = (uint32_t)pos * 2654435761u + 1;
    h ^= h >> 13;
    return (int16_t)(h >> 8);
}

// Position folded into the waveform: what an ideal looping playback reads.
static int lc_wrap(int pos)
{
    if (pos >= lc_len)
        pos = lc_len - lc_loop_len + (pos - lc_len) % lc_loop_len;
    return pos;
}

static void lc_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    waveform_t *wave = sbuf->wave;
    bool unrolled = (wave->loop_len == 0);
    (void)ctx; (void)seeking;

    // The mixer clamps every read to [0, len): a codec reads its own samples
    // and has no way to produce anything outside of them.
    if (wpos < 0 || wpos + wlen > wave->len)
        lc_oob = true;

    while (wlen > 0) {
        int n = wlen < SAMPLEBUFFER_MARGIN_UNITS ? wlen : SAMPLEBUFFER_MARGIN_UNITS;
        int16_t *dst = samplebuffer_append(sbuf, n);
        for (int i = 0; i < n; i++)
            dst[i] = lc_sample(unrolled ? lc_wrap(wpos + i) : wpos + i);
        wlen -= n;
        wpos += n;
    }
}

// The case that turned up the bug: instrument 1 of Arcade_S900.xm, a 16-bit
// sample of about a thousand samples with a short forward loop at the end.
// Only the loop fits the samplebuffer, so the pin happens on the fly, when
// playback first reaches the loop point.
static waveform_t lc_wave_loop = {
    .name = "loopcache", .bits = 16, .channels = 1, .frequency = 32000,
    .len = 1066, .loop_len = 277, .read = lc_read,
};

// Short enough for the whole waveform to be pinned, attack included, which the
// mixer does upfront rather than at the loop point.
static waveform_t lc_wave_whole = {
    .name = "loopcache-whole", .bits = 16, .channels = 1, .frequency = 32000,
    .len = 400, .loop_len = 100, .read = lc_read,
};

static waveform_t lc_wave_unrolled = {
    .name = "loopcache-unrolled", .bits = 16, .channels = 1, .frequency = 32000,
    .len = 1 << 20, .read = lc_read,
};

static bool test_mixer_loop_cache(waveform_t *wave, float freq)
{
    int nsamples = 4096;
    int16_t *ref = malloc(nsamples * 4);
    lc_len = wave->len;
    lc_loop_len = wave->loop_len;
    lc_oob = false;

    sv_start(&lc_wave_unrolled, 0, freq);
    sv_mix(nsamples);
    memcpy(ref, sv_out, nsamples * 4);

    sv_start(wave, 0, freq);
    sv_mix(nsamples);

    bool ok = true;
    if (lc_oob) {
        printf("FAILED %s: read past the end of the waveform\n", wave->name);
        ok = false;
    }
    // Same tolerance as sv_check_lr: the two runs settle their volume ramps
    // independently.
    for (int i = 0, bad = 0; i < nsamples * 2 && bad < 4; i++) {
        int d = ref[i] - sv_out[i]; if (d < 0) d = -d;
        if (d > 32) {
            printf("FAILED %s: sample %d: %d, expected %d (freq=%d)\n",
                wave->name, i/2, sv_out[i], ref[i], (int)freq);
            ok = false;
            bad++;
        }
    }

    free(ref);
    return ok;
}

int main(void)
{
    debug_init_emulog();
    debug_init_usblog();
    emux_ioctl_fast();

    console_init();

    printf("WAV64 VADPCM Hermite tests\n\n");

    audio_init(44100, 4);
    mixer_init(8);

    int total = 0, failed = 0;
    int inmix_frames[] = { 1, 2, 8, 16 };
    const double vad_ratios[] = { 0.5, 1.0, 1.2891, 1.5 };
    // 4-bit residuals keep the classic nibble packing, 3 and 2 bit pack the
    // payload densely: same framing, same codebook, shorter frames.
    const int vad_bits[] = { 4, 3, 2 };
    for (int b = 0; b < 3; b++) {
        for (int s = 0; s < 4; s++) {
            for (int fc = 0; fc < 4; fc++) {
                for (int r = 0; r < 4; r++) {
                    uint32_t step = (uint32_t)(vad_ratios[r] * (1 << MIXER_FX64_FRAC));
                    int nwave = inmix_frames[fc] * 16;
                    // Stay inside the waveform including Hermite overread.
                    int nout = nwave / 2;
                    if (nout > 96) nout = 96;
                    if (nout < 8) nout = nwave > 8 ? 8 : nwave;
                    total++;
                    if (!test_vadpcm_inmixer(inmix_frames[fc], step, nout, s + 1, vad_bits[b]))
                        failed++;
                }
            }
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

    printf("Streamed mono PCM tests\n");
    fflush(stdout);
    sv_init();
    // Below the sample rate: step < 1 makes the inclusive window one unit
    // larger than the position advance, which is what used to overrun the
    // samplebuffer margin. Odd starts exercise the 8-bit ring parity path.
    const float sp_freqs[] = { 22050, 11025, 16000, 32000 };
    const int sp_starts[] = { 0, 1, 5779, SP_LEN - 64 };
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            total++;
            if (!test_mixer_stream_pcm8(sp_freqs[i], sp_starts[j]))
                failed++;
        }
    }

    printf("Streamed stereo VADPCM tests\n");
    fflush(stdout);
    const float sv_freqs[] = { 32000, 44100, 22050, 33333, 48000 };
    const int sv_starts[] = { 0, 16, 7, 12345, 65537 };
    for (int c = 0; c < 3; c++) {
        sv_chunk = 512 << c;
        for (int i = 0; i < 5; i++) {
            total++;
            if (!test_mixer_stream(sv_freqs[i], sv_starts[i]))
                failed++;
        }
    }
    sv_chunk = 1024;
    total++; if (!test_mixer_posfold()) failed++;
    total++; if (!test_mixer_stop_releases_sub()) failed++;
    total++; if (!test_mixer_switch_waveform()) failed++;
    total++; if (!test_mixer_loop()) failed++;

    // The same stream at the narrower widths. Frames get shorter, so every
    // size and offset the mixer derives from them changes with the width.
    for (int b = 3; b >= 2; b--) {
        printf("Streamed stereo VADPCM tests (%d bit)\n", b);
        fflush(stdout);
        sv_set_bits(b);
        for (int i = 0; i < 5; i++) {
            total++;
            if (!test_mixer_stream(sv_freqs[i], sv_starts[i]))
                failed++;
        }
        total++; if (!test_mixer_posfold()) failed++;
        total++; if (!test_mixer_loop()) failed++;
    }
    sv_set_bits(4);

    printf("Retrigger tests\n");
    fflush(stdout);
    is_init();
    // A tracker plays these an octave or two below the output rate (Amiga
    // periods), so a note advances slower than the mix and its window covers
    // fewer frames than the output samples it feeds.
    const float is_freqs[] = { 44100, 28125, 14063, 8363 };
    for (int c = 0; c < 3; c++) {
        sv_chunk = 512 << c;
        for (int i = 0; i < 4; i++) {
            total++; if (!test_mixer_retrigger(false, is_freqs[i])) failed++;
            total++; if (!test_mixer_retrigger(true, is_freqs[i])) failed++;
        }
    }
    sv_chunk = 1024;

    printf("Pinned loop tests\n");
    fflush(stdout);
    const float lc_freqs[] = { 32000, 44100, 21237 };
    for (int i = 0; i < 3; i++) {
        total++; if (!test_mixer_loop_cache(&lc_wave_loop, lc_freqs[i])) failed++;
        total++; if (!test_mixer_loop_cache(&lc_wave_whole, lc_freqs[i])) failed++;
        total++; if (!test_mixer_vadpcm_partial_frame(lc_freqs[i])) failed++;
        total++; if (!test_mixer_loop_taps(&lp_wave, lc_freqs[i], 0)) failed++;
        // Large loop, wrapped by the CPU: start just before the loop point so
        // that the mix crosses it several times, and go up to three octaves
        // above the sample rate, where a round covers thousands of samples of
        // the stream.
        total++; if (!test_mixer_loop_taps(&lp_wave_big, lc_freqs[i], 48000)) failed++;
        total++; if (!test_mixer_loop_taps(&lp_wave_big, lc_freqs[i] * 8, 48000)) failed++;
    }
    // Start before the loop point (so that the pin happens on the fly) and
    // inside it (so that it happens right away). Every residual width is
    // compared sample by sample against the C decoder, which is where a
    // mistake in the unpacking of a frame turns into audible noise.
    static const int rf_starts[] = { -700, -1, 64, 777 };
    for (int b = 4; b >= 2; b--) {
        printf("Exact loop tests (%d bit)\n", b);
        fflush(stdout);
        rf_init(b);
        // A round is a whole number of frames, so the start decides where every
        // round boundary of the test falls inside a frame, and the last samples
        // of a frame are where the resampler needs the one after it.
        for (int i = 0; i < (b == 4 ? 16 : 4); i++) {
            total++; if (!test_mixer_loop_exact(&rf_wave_plain, 16 + i)) failed++;
        }
        // Reaching the loop point while streaming, and starting inside a loop
        // that is pinned right away. The wrap moves across the rounds as it
        // repeats, so a single run covers several alignments between the two.
        for (int i = 0; i < 4; i++) {
            total++;
            if (!test_mixer_loop_exact(&rf_wave, RF_LOOP_START + rf_starts[i])) failed++;
        }
    }
    sv_silence();
    mixer_ch_set_limits(SV_CHANNEL, 0, 48000, 0);

    if (failed) {
        printf("\n%d/%d TESTS FAILED\n", failed, total);
        abort();
    }
    printf("\nALL TESTS PASSED (%d)\n", total);
    return 0;
}
