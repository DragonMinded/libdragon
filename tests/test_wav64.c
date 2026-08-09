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

// @p dlvol / @p drvol are the volume ramp increments applied by the ucode every
// 4 output samples; 0 keeps the volume constant for the whole command. @p tlvol
// / @p trvol are where the ramp stops: the ucode clamps between them and the
// starting volume, so passing the starting volume itself pins the volume.
static void emit_channel_target(int ch, uint32_t flags, int16_t lvol, int16_t rvol,
    int16_t dlvol, int16_t drvol, int16_t tlvol, int16_t trvol, uint32_t pos,
    uint32_t step, uint32_t len, uint32_t loop_len, void *ptr, int nsamples)
{
    assert((nsamples & 1) == 0 && nsamples >= 0 && nsamples <= 512);
    rspq_write_t w = rspq_write_begin(__mixer_overlay_id, MIXER_CMD_CHANNEL, 9);
    // a0: ch<<19 | flags<<11 | nsamples/2  (9 bits: nsamples up to 512)
    rspq_write_arg(&w, ((uint32_t)ch << 19) | ((flags & 0xFF) << 11) | ((uint32_t)nsamples >> 1));
    rspq_write_arg(&w, ((uint32_t)(uint16_t)lvol << 16) | (uint16_t)rvol);
    rspq_write_arg(&w, pos);
    rspq_write_arg(&w, step);
    rspq_write_arg(&w, len);
    rspq_write_arg(&w, loop_len);
    rspq_write_arg(&w, PhysicalAddr(ptr));
    rspq_write_arg(&w, ((uint32_t)(uint16_t)dlvol << 16) | (uint16_t)drvol);
    rspq_write_arg(&w, ((uint32_t)(uint16_t)tlvol << 16) | (uint16_t)trvol);
    rspq_write_end(&w);
}

static void emit_channel_lr(int ch, uint32_t flags, int16_t lvol, int16_t rvol,
    uint32_t pos, uint32_t step, uint32_t len, uint32_t loop_len, void *ptr,
    int nsamples)
{
    emit_channel_target(ch, flags, lvol, rvol, 0, 0, lvol, rvol, pos, step, len,
        loop_len, ptr, nsamples);
}

static void emit_channel(int ch, uint32_t flags, int16_t vol, uint32_t pos,
    uint32_t step, uint32_t len, uint32_t loop_len, void *ptr, int nsamples)
{
    emit_channel_lr(ch, flags, vol, vol, pos, step, len, loop_len, ptr, nsamples);
}

static void emit_flush(int nsamples, void *out) {
    // MIX_FLUSH publishes the id of the round it ends (the mixer uses it to
    // know what the RSP has read); these tests drive the ucode by hand and
    // have nowhere to track it, so it goes to a scratch word.
    static uint32_t *round_marker = NULL;
    if (!round_marker)
        round_marker = malloc_uncached(8);
    rspq_write(__mixer_overlay_id, MIXER_CMD_FLUSH, (uint32_t)nsamples,
        PhysicalAddr(out), 0, PhysicalAddr(round_marker));
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

    memset(state_rsp, 0, sizeof(*state_rsp));
    rspq_highpri_begin();
    emit_setchannel(0, codebook, state_rsp, bits);
    emit_channel(0, flags, vol, 0, step, len, 0, frames, nout);
    emit_flush(nout, out);
    rspq_highpri_end();
    rspq_wait();

    bool ok = true;
    int16_t *stereo = (int16_t*)out;
    for (int i = 0; i < nout; i++) {
        uint32_t pos = step * (uint32_t)i;
        int si = pos >> MIXER_FX64_FRAC;
        uint16_t x = (uint16_t)(pos << 4);
        int16_t y0 = ref_pcm[si], y1 = ref_pcm[si+1], y2 = ref_pcm[si+2], y3 = ref_pcm[si+3];
        int16_t res = hermite_ref(y0, y1, y2, y3, x);
        int32_t v = (int32_t)(((int64_t)res * vol * 2) >> 16);
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

        int32_t v = (int32_t)(((int64_t)res * vol * 2) >> 16);
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

    rspq_highpri_begin();
    emit_channel_lr(0, flags_l, vol, 0, 0, step, len, 0, wave, nsamples);
    emit_channel_lr(1, flags_r, 0, vol, 0, step, len, 0, wave, nsamples);
    emit_flush(nsamples, out);
    rspq_highpri_end();
    rspq_wait();

    bool ok = true;
    int16_t *stereo = (int16_t*)out;
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
        int32_t vl = (int32_t)(((int64_t)res_l * vol * 2) >> 16);
        int32_t vr = (int32_t)(((int64_t)res_r * vol * 2) >> 16);

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

// Every mix round the CPU enqueues keeps its window of the samplebuffer live
// until the RSP runs it, and the ring is only sized for the rounds of a couple
// of polls: past that, mixer_poll_async waits for the oldest poll before
// starting. Play the same stream three times — syncing after every short
// chunk, in poll-sized chunks, and finally queueing polls back-to-back without
// syncing, as mixer_try_play does. All three must produce the same audio: a
// refill landing on a window the RSP had still to read makes the runs diverge.
extern void mixer_poll_async(int16_t *out, int nsamples);

static bool test_mixer_queue_depth(float freq)
{
    int nsamples = 8192;
    int16_t *ref = malloc(nsamples * 4);
    int saved = sv_chunk;

    sv_shift = 0;
    sv_chunk = 64;
    sv_start(&sv_wave, 0, freq);
    sv_mix(nsamples);
    memcpy(ref, sv_out, nsamples * 4);
    bool ok = sv_check_lr("mixer queue depth (reference)", nsamples);

    // Same tolerance as sv_check_lr: each run settles its volume ramps
    // independently.
    #define CHECK_AGAINST_REF(tag) ({ \
        for (int i = 0; ok && i < nsamples * 2; i++) { \
            int d = ref[i] - sv_out[i]; if (d < 0) d = -d; \
            if (d > 32) { \
                printf("FAILED %s: freq=%d: sample %d: %d/%d, expected %d/%d\n", \
                    tag, (int)freq, i/2, sv_out[(i/2)*2], sv_out[(i/2)*2+1], \
                    ref[(i/2)*2], ref[(i/2)*2+1]); \
                ok = false; \
            } \
        } \
    })

    int blen = audio_get_buffer_length() & ~1;

    sv_chunk = blen;
    sv_start(&sv_wave, 0, freq);
    sv_mix(nsamples);
    ok = ok && sv_check_lr("mixer queue depth", nsamples);
    CHECK_AGAINST_REF("mixer queue depth");

    sv_start(&sv_wave, 0, freq);
    for (int done = 0; done < nsamples; done += blen) {
        int n = nsamples - done < blen ? nsamples - done : blen;
        mixer_poll_async(sv_out + done * 2, n);
    }
    rspq_highpri_sync();
    ok = ok && sv_check_lr("mixer queue depth (async)", nsamples);
    CHECK_AGAINST_REF("mixer queue depth (async)");

    #undef CHECK_AGAINST_REF
    sv_chunk = saved;
    free(ref);
    return ok;
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
// Block codec at a loop point
//
// A block codec (ULC, Opus) decodes whole blocks and undoes whatever crosses
// the end of the waveform; the mixer then appends the loop overread right
// after, in the same fetch and with no flush in between. Both writes reach
// RDRAM through SP DMA, which needs an 8-byte aligned destination, so the trim
// cannot stop at an arbitrary sample: it rounds up and keeps a few of the
// samples it already decoded past the end.
//////////////////////////////////////////////////////////////////////////////

#define BC_LEN      4099    // not a multiple of the 4 samples of an 8-byte slot
#define BC_BLOCK    1024
#define BC_CHANNEL  2

// Triangle over the whole loop, so the waveform is continuous across the wrap
// and any jump in the output is a defect rather than the signal.
static int16_t bc_sample(int pos)
{
    int half = BC_LEN / 2;
    int v = pos < half ? pos : BC_LEN - pos;
    return (int16_t)(v * 12000 / half);
}

static int bc_calls, bc_units;

static void bc_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    (void)ctx; (void)seeking;
    int n = ROUND_UP(wlen, BC_BLOCK);
    bc_calls++; bc_units += n;
    int16_t *out = samplebuffer_append(sbuf, n);
    for (int i = 0; i < n; i++)
        out[i] = bc_sample((wpos + i) % BC_LEN);

    // Keep the samples the block already holds up to the next 8-byte boundary,
    // which is what leaves the write cursor where the next block can land.
    int valid = ROUND_UP(BC_LEN - wpos, 4);
    if (n > valid)
        samplebuffer_undo(sbuf, n - valid);
}

static waveform_t bc_wave = {
    .name = "bc-block", .bits = 16, .channels = 1, .frequency = 44100,
    .len = BC_LEN, .loop_len = BC_LEN, .read = bc_read,
    .append_units = BC_BLOCK, .rsp_written = true, .loop_restart_only = true,
};

static bool test_mixer_block_codec_loop(float freq)
{
    mixer_ch_stop(BC_CHANNEL);
    sv_mix(2048);

    mixer_ch_set_limits(BC_CHANNEL, 16, 48000, 0);
    mixer_ch_play(BC_CHANNEL, &bc_wave);
    mixer_ch_set_freq(BC_CHANNEL, freq);
    mixer_ch_set_vol(BC_CHANNEL, 0.5f, 0.5f);
    sv_mix(2048);

    // Several laps of the loop, so the trim and the refill happen many times.
    bc_calls = bc_units = 0;
    int peak = 0, maxjump = 0, worst = -1, prev = 0;
    for (int i = 0; i < 4; i++) {
        sv_mix(4096);
        for (int j = 0; j < 4096; j++) {
            int s = sv_out[j*2];
            int a = s < 0 ? -s : s;
            int d = s - prev; if (d < 0) d = -d;
            if (a > peak) peak = a;
            if ((i || j) && d > maxjump) { maxjump = d; worst = i*4096 + j; }
            prev = s;
        }
    }
    mixer_ch_stop(BC_CHANNEL);
    sv_mix(2048);

    if (peak < 1024) {
        printf("FAILED block codec loop: silent (peak %d) freq=%d\n", peak, (int)freq);
        return false;
    }
    // The slope is a handful of units per sample at most, so anything beyond
    // this is the ring handing out samples from the wrong place.
    if (maxjump > 200) {
        printf("FAILED block codec loop: discontinuity %d at %d freq=%d (reads=%d units=%d)\n",
            maxjump, worst, (int)freq, bc_calls, bc_units);
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

//////////////////////////////////////////////////////////////////////////////
// Baseline mixer scenarios + mixer_ch_set_loop matrix
//
// Synthetic asset: rising-ramp intro + square-wave loop + descending release.
// Variants cover PCM/VADPCM × resident/streamed × mono/stereo × cached/tiny,
// and note-off before / inside / near loop_end / just after a wrap.
//////////////////////////////////////////////////////////////////////////////

// Longer than sv_start's volume-ramp warmup (2048), so a oneshot is still
// playing when the measured capture begins.
#define BL_INTRO     2048
#define BL_LOOP      512
#define BL_RELEASE   256             // shorter than MIXER_MAX_SAMPLES_PER_ROUND
#define BL_LOOP_END  (BL_INTRO + BL_LOOP)
#define BL_LEN       (BL_LOOP_END + BL_RELEASE)
#define BL_TINY_LOOP 32              // shorter than MIXER_LOOP_OVERREAD
#define BL_PIN_INTRO 200
#define BL_PIN_LOOP  100             // whole waveform pins in the samplebuffer
#define BL_PIN_REL   80
#define BL_PIN_END   (BL_PIN_INTRO + BL_PIN_LOOP)
#define BL_PIN_LEN   (BL_PIN_END + BL_PIN_REL)
#define BL_AMP       16000
#define BL_SQ        16
#define BL_FREQ      44100

static int16_t bl_sample_geom(int pos, int intro, int loop, int release)
{
    int loop_end = intro + loop, len = loop_end + release;
    if (pos < intro)
        return (int16_t)(-BL_AMP + (int32_t)(2 * BL_AMP) * pos / intro);
    if (pos < loop_end) {
        int i = pos - intro;
        return (i % BL_SQ < BL_SQ / 2) ? BL_AMP : (int16_t)-BL_AMP;
    }
    if (pos < len) {
        int i = pos - loop_end;
        return (int16_t)(BL_AMP - (int32_t)(2 * BL_AMP) * i / release);
    }
    return 0;
}

static int16_t bl_sample(int pos) {
    return bl_sample_geom(pos, BL_INTRO, BL_LOOP, BL_RELEASE);
}
static int16_t bl_tiny_sample(int pos) {
    return bl_sample_geom(pos, BL_INTRO, BL_TINY_LOOP, BL_RELEASE);
}
static int16_t bl_pin_sample(int pos) {
    return bl_sample_geom(pos, BL_PIN_INTRO, BL_PIN_LOOP, BL_PIN_REL);
}

static int bl_play(int pos, int loop_end, int loop_len)
{
    // Hermite with a null fraction returns the second tap, one past pos.
    pos++;
    if (loop_len && pos >= loop_end)
        pos = loop_end - loop_len + (pos - loop_end) % loop_len;
    return pos;
}

static void bl_read_with(samplebuffer_t *sbuf, int wpos, int wlen,
    int16_t (*sample)(int), int ch)
{
    while (wlen > 0) {
        int n = wlen < SAMPLEBUFFER_MARGIN_UNITS ? wlen : SAMPLEBUFFER_MARGIN_UNITS;
        int16_t *dst = samplebuffer_append(sbuf, n);
        for (int i = 0; i < n; i++) {
            int16_t s = sample(wpos + i);
            if (ch == 2) dst[2*i] = dst[2*i+1] = s;
            else         dst[i] = s;
        }
        wlen -= n;
        wpos += n;
    }
}

static void bl_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    (void)ctx; (void)seeking;
    bl_read_with(sbuf, wpos, wlen, bl_sample, 1);
}

static void bl_tiny_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    (void)ctx; (void)seeking;
    bl_read_with(sbuf, wpos, wlen, bl_tiny_sample, 1);
}

static void bl_read_stereo(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    (void)ctx; (void)seeking;
    bl_read_with(sbuf, wpos, wlen, bl_sample, 2);
}

static void bl_pin_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    (void)ctx; (void)seeking;
    bl_read_with(sbuf, wpos, wlen, bl_pin_sample, 1);
}

static waveform_t bl_wave_oneshot_res = {
    .name = "bl-oneshot-res", .bits = 16, .channels = 1, .frequency = BL_FREQ,
    .len = BL_LEN,
};

// Terminal loop (loop_end == 0 ⇒ len). Resident overread past len is padded
// with loop-start samples, as wav64 preload does.
static waveform_t bl_wave_loop_res = {
    .name = "bl-loop-res", .bits = 16, .channels = 1, .frequency = BL_FREQ,
    .len = BL_LOOP_END, .loop_len = BL_LOOP,
};

// Resident sustain + release. Overread past loop_end bleeds into the release
// while looping (WAV64 preload of the rotated prefix is a later PR); this
// asset is only used for a mid-loop note-off that never wraps after disable.
static waveform_t bl_wave_loop_res_rel = {
    .name = "bl-loop-res-rel", .bits = 16, .channels = 1, .frequency = BL_FREQ,
    .len = BL_LEN, .loop_len = BL_LOOP, .loop_end = BL_LOOP_END,
};

static waveform_t bl_wave_oneshot_stream = {
    .name = "bl-oneshot-stream", .bits = 16, .channels = 1, .frequency = BL_FREQ,
    .len = BL_LEN, .read = bl_read,
};

// Sustain loop + release tail (streamed): loop_end < len. Loop is large enough
// that the mixer keeps streaming it (non-cached).
static waveform_t bl_wave_loop_stream = {
    .name = "bl-loop-stream", .bits = 16, .channels = 1, .frequency = BL_FREQ,
    .len = BL_LEN, .loop_len = BL_LOOP, .loop_end = BL_LOOP_END, .read = bl_read,
};

static waveform_t bl_wave_loop_stereo = {
    .name = "bl-loop-stereo", .bits = 16, .channels = 2, .frequency = BL_FREQ,
    .len = BL_LEN, .loop_len = BL_LOOP, .loop_end = BL_LOOP_END, .read = bl_read_stereo,
};

// Loop shorter than MIXER_LOOP_OVERREAD: overread refill wraps the tiny loop
// more than once per fetch.
static waveform_t bl_wave_tiny = {
    .name = "bl-tiny-loop", .bits = 16, .channels = 1, .frequency = BL_FREQ,
    .len = BL_INTRO + BL_TINY_LOOP + BL_RELEASE,
    .loop_len = BL_TINY_LOOP, .loop_end = BL_INTRO + BL_TINY_LOOP, .read = bl_tiny_read,
};

// Short enough that the mixer pins the whole waveform in the samplebuffer.
static waveform_t bl_wave_pin = {
    .name = "bl-pin-loop", .bits = 16, .channels = 1, .frequency = BL_FREQ,
    .len = BL_PIN_LEN, .loop_len = BL_PIN_LOOP, .loop_end = BL_PIN_END,
    .read = bl_pin_read,
};

// Resident copy of the first @p len samples. The RSP overreads
// MIXER_LOOP_OVERREAD past the end, and for a looping waveform those samples
// must be the ones at the loop start, as a wav64 preload lays them out.
static int16_t *bl_resident(int len, int loop_len)
{
    int16_t *mem = malloc_uncached((len + MIXER_LOOP_OVERREAD) * 2);
    for (int i = 0; i < len + MIXER_LOOP_OVERREAD; i++)
        mem[i] = bl_sample(i < len || !loop_len ? i
            : len - loop_len + (i - len) % loop_len);
    return mem;
}

static void bl_init(void)
{
    if (bl_wave_oneshot_res.mem)
        return;
    bl_wave_oneshot_res.mem = bl_resident(BL_LEN, 0);
    bl_wave_loop_res.mem = bl_resident(BL_LOOP_END, BL_LOOP);
    // Full asset including release; no loop-start pad at loop_end (see above).
    bl_wave_loop_res_rel.mem = bl_resident(BL_LEN, 0);
}

// Sample-by-sample check at the output rate: Hermite is an identity there.
// @p loop_end doubles as the physical end when there is no loop.
// @p sample is NULL for the default bl_sample shape.
static bool bl_check_exact(const char *tag, int start_pos, int nsamples,
    int loop_end, int loop_len, int16_t (*sample)(int))
{
    if (!sample) sample = bl_sample;
    int nbad = 0, first = -1, maxdiff = 0, peak = 0;
    for (int i = 0; i < nsamples; i++) {
        int p = bl_play(start_pos + i, loop_end, loop_len);
        if (!loop_len && p >= loop_end)
            break;
        int want = sample(p) / 2;
        int got = sv_out[i * 2];
        int d = got - want; if (d < 0) d = -d;
        if (d > 64) {
            nbad++;
            if (first < 0) first = i;
            if (d > maxdiff) maxdiff = d;
        }
        int a = got < 0 ? -got : got;
        if (a > peak) peak = a;
    }
    if (nbad) {
        printf("FAILED %s: %d/%d samples off (max %d) from %d\n",
            tag, nbad, nsamples, maxdiff, first);
        for (int i = first; i < first + 6 && i < nsamples; i++)
            printf("   [%d] play=%d got %d want %d\n", i,
                bl_play(start_pos + i, loop_end, loop_len),
                sv_out[i * 2],
                sample(bl_play(start_pos + i, loop_end, loop_len)) / 2);
        return false;
    }
    if (peak < 4096) {
        printf("FAILED %s: output is silent (peak %d)\n", tag, peak);
        return false;
    }
    return true;
}

static int bl_wave_loop_end(const waveform_t *wave) {
    return wave->loop_end ? wave->loop_end : wave->len;
}

static bool test_mixer_baseline_pcm(waveform_t *wave, int startsample, int nsamples)
{
    bl_init();
    sv_silence();
    mixer_ch_set_limits(SV_CHANNEL, 0, 48000, 0);
    sv_start(wave, startsample, BL_FREQ);
    int pos = (int)mixer_ch_get_pos(SV_CHANNEL);
    sv_mix(nsamples);
    return bl_check_exact(wave->name, pos, nsamples,
        bl_wave_loop_end(wave), wave->loop_len, NULL);
}

// Frame a test explicitly asked for with #mixer_ch_set_pos, which is the one
// case where the caller owns the restriction (and a real asset would have to
// be built with a seek point there).
static int slv_seek_asked = -1;

// Where the note-off (#mixer_ch_set_loop false) lands relative to the loop.
typedef enum {
    SL_BEFORE,       // still in the intro
    SL_INSIDE,       // mid-loop
    SL_NEAR_END,     // a few samples before loop_end
    SL_AFTER_WRAP,   // just after wrapping back to loop_start
} sl_when_t;

// Disable the sustain loop at @p when, then verify a seamless one-shot through
// the release tail and a silent stop. PCM exact match when @p sample is set;
// otherwise only L/R identity + audible + idle (VADPCM / stereo stress).
static bool test_mixer_set_loop(waveform_t *wave, sl_when_t when,
    int16_t (*sample)(int))
{
    int loop_end = bl_wave_loop_end(wave);
    int loop_start = loop_end - wave->loop_len;
    int start, target;
    char tag[64];
    const char *wname[] = { "before", "inside", "near_end", "after_wrap" };

    switch (when) {
    case SL_BEFORE:
        start = loop_start > 64 ? loop_start - 64 : 0;
        target = start + 16;
        break;
    case SL_INSIDE:
        start = loop_start + 8;
        target = loop_start + wave->loop_len / 2;
        break;
    case SL_NEAR_END:
        // Land a few samples before loop_end; mix in steps of 2 so a round
        // cannot jump past the window and wrap.
        start = loop_end - 8;
        if (start < loop_start) start = loop_start;
        target = loop_end - 4;
        break;
    case SL_AFTER_WRAP:
        start = loop_end - 6;
        if (start < loop_start) start = loop_start;
        target = loop_start + 16;   // after the wrap
        break;
    }
    snprintf(tag, sizeof(tag), "set_loop %s/%s", wave->name, wname[when]);

    bl_init();
    sv_silence();
    mixer_ch_set_limits(SV_CHANNEL, 0, 48000, 0);
    // Settle the volume ramp at step 0 so the playhead stays on @p start
    // (sv_start's 2048-sample warmup would walk past the intro / near-end).
    mixer_ch_play(SV_CHANNEL, wave);
    mixer_ch_set_vol(SV_CHANNEL, 0.5f, 0.5f);
    slv_seek_asked = start / 16;
    mixer_ch_set_pos(SV_CHANNEL, start);
    mixer_ch_set_freq(SV_CHANNEL, 0);
    sv_mix(2048);
    mixer_ch_set_freq(SV_CHANNEL, BL_FREQ);

    int prev = start;
    for (int guard = 0; guard < 256; guard++) {
        int pos = (int)mixer_ch_get_pos(SV_CHANNEL);
        if (when == SL_AFTER_WRAP) {
            if (pos < prev && pos >= loop_start && pos < loop_start + 64)
                break;
        } else if (when == SL_NEAR_END) {
            if (pos >= target && pos < loop_end) break;
        } else if (when == SL_BEFORE) {
            break; // already parked in the intro
        } else if (pos >= target && pos < loop_end) {
            break;
        }
        prev = pos;
        // Steps of 2: mixer_poll requires an even count, and near loop_end a
        // larger step would wrap past the NEAR_END window.
        sv_mix(when == SL_INSIDE ? 32 : 2);
    }

    int pos = (int)mixer_ch_get_pos(SV_CHANNEL);
    if (when == SL_BEFORE && !(pos < loop_start)) {
        printf("FAILED %s: pos %d not in intro [0,%d)\n", tag, pos, loop_start);
        return false;
    }
    if (when == SL_INSIDE && !(pos >= loop_start && pos < loop_end)) {
        printf("FAILED %s: pos %d not inside the loop\n", tag, pos);
        return false;
    }
    if (when == SL_NEAR_END && !(pos >= loop_end - 16 && pos < loop_end)) {
        printf("FAILED %s: pos %d not near loop_end=%d\n", tag, pos, loop_end);
        return false;
    }
    if (when == SL_AFTER_WRAP && !(pos >= loop_start && pos < loop_start + 64)) {
        printf("FAILED %s: pos %d not just after wrap (loop_start=%d)\n",
            tag, pos, loop_start);
        return false;
    }

    // From here on nothing asked for a jump: every seek the mixer makes has to
    // be one the asset could serve.
    slv_seek_asked = -1;
    mixer_ch_set_loop(SV_CHANNEL, false);
    pos = (int)mixer_ch_get_pos(SV_CHANNEL);
    int remain = wave->len - pos;
    if (remain < 8) {
        printf("FAILED %s: only %d samples left after disable\n", tag, remain);
        return false;
    }
    // mixer_poll rejects odd counts; a release of 37 samples is the point.
    sv_mix((remain + 65) & ~1);

    if (sample) {
        char rtag[80];
        snprintf(rtag, sizeof(rtag), "%s (release)", tag);
        int ncheck = remain < 4096 ? remain : 4096;
        if (!bl_check_exact(rtag, pos, ncheck, wave->len, 0, sample))
            return false;
    } else {
        // Audible release; stereo planes must stay together.
        int peak = 0, n = remain < 512 ? remain : 512;
        for (int i = 0; i < n * 2; i++) {
            int a = sv_out[i] < 0 ? -sv_out[i] : sv_out[i];
            if (a > peak) peak = a;
        }
        if (peak < 1024) {
            printf("FAILED %s: silent release (peak %d)\n", tag, peak);
            return false;
        }
    }
    if (wave->channels == 2) {
        // Skip the last overread past sample_end: silence padding of the two
        // VADPCM planes can settle one sample apart there.
        int n = remain < 512 ? remain : 512;
        if (n > 8) n -= 8;
        if (!sv_check_lr(tag, n))
            return false;
    }

    sv_mix(2048);
    if (mixer_ch_playing(SV_CHANNEL)) {
        printf("FAILED %s: still playing after release (pos=%.1f len=%d)\n",
            tag, mixer_ch_get_pos(SV_CHANNEL), wave->len);
        return false;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////////
// VADPCM sustain + release (streamed / resident)
//
// Release length is deliberately not a multiple of 16, so the last frame is
// partial — the case WAV64 v10 has to get right at sample_end.
//////////////////////////////////////////////////////////////////////////////

#define SLV_INTRO     128
#define SLV_LOOP      64
#define SLV_RELEASE   37
#define SLV_LOOP_END  (SLV_INTRO + SLV_LOOP)
#define SLV_LEN       (SLV_LOOP_END + SLV_RELEASE)
#define SLV_FRAMES    ((SLV_LEN + 15) / 16)

static uint8_t *slv_frames;
static int16_t *slv_ref;
static wav64_vadpcm_vector_t *slv_codebook;
static wav64_vadpcm_vector_t *slv_loop_state;
static waveform_vadpcm_t slv_codec;
static waveform_vadpcm_t slv_codec_stereo;

// A real VADPCM asset can only re-seed its decoder on the points audioconv64
// wrote into the file: the start of the waveform and its loop point. Deriving
// the state from the reference PCM would let the mixer seek anywhere, and hide
// the seeks that a wav64 could not serve.
static void slv_check_seek(int wpos)
{
    assertf(wpos == 0 || wpos == SLV_INTRO / 16 || wpos == slv_seek_asked,
        "VADPCM seek to frame %d, which is not a seek point", wpos);
}

static void slv_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    (void)ctx;
    if (seeking) {
        slv_check_seek(wpos);
        wav64_state_vadpcm_t *st = sbuf->state;
        for (int i = 0; i < 8; i++) {
            int p = wpos * 16 - 8 + i;
            st->state[0].v[i] = p >= 0 ? slv_ref[p] : 0;
        }
    }
    int fb = VADPCM_FRAME_BYTES(4);
    while (wlen > 0) {
        int n = wlen < SAMPLEBUFFER_MARGIN_UNITS ? wlen : SAMPLEBUFFER_MARGIN_UNITS;
        memcpy(samplebuffer_append(sbuf, n), slv_frames + wpos * fb, n * fb);
        wlen -= n;
        wpos += n;
    }
}

// Stereo VADPCM with identical planes: L/R must stay together across a note-off.
static void slv_read_stereo(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    samplebuffer_t *sbuf_r = sbuf + 1;
    (void)ctx;
    if (seeking && sbuf->widx == 0) {
        slv_check_seek(wpos);
        wav64_state_vadpcm_t *st = sbuf->state;
        for (int i = 0; i < 8; i++) {
            int p = wpos * 16 - 8 + i;
            st->state[0].v[i] = st->state[1].v[i] = p >= 0 ? slv_ref[p] : 0;
        }
        samplebuffer_flush(sbuf_r);
        sbuf_r->wpos = wpos;
        sbuf_r->head = sbuf->head;
    }
    int fb = VADPCM_FRAME_BYTES(4);
    while (wlen > 0) {
        int n = wlen < SAMPLEBUFFER_MARGIN_UNITS ? wlen : SAMPLEBUFFER_MARGIN_UNITS;
        uint8_t *dl = samplebuffer_append(sbuf, n);
        uint8_t *dr = samplebuffer_append(sbuf_r, n);
        memcpy(dl, slv_frames + wpos * fb, n * fb);
        memcpy(dr, dl, n * fb);
        sbuf_r->wnext = sbuf_r->wpos + sbuf_r->widx;
        wlen -= n;
        wpos += n;
    }
}

static waveform_t slv_wave_stream = {
    .name = "slv-stream", .bits = 16, .channels = 1, .frequency = BL_FREQ,
    .len = SLV_LEN, .loop_len = SLV_LOOP, .loop_end = SLV_LOOP_END,
    .read = slv_read, .format = WAVEFORM_FORMAT_VADPCM,
    .codec = &slv_codec, .state_size = sizeof(wav64_state_vadpcm_t),
};

static waveform_t slv_wave_stereo = {
    .name = "slv-stereo", .bits = 16, .channels = 2, .frequency = BL_FREQ,
    .len = SLV_LEN, .loop_len = SLV_LOOP, .loop_end = SLV_LOOP_END,
    .read = slv_read_stereo, .format = WAVEFORM_FORMAT_VADPCM,
    .codec = &slv_codec_stereo, .state_size = sizeof(wav64_state_vadpcm_t),
};

static waveform_t slv_wave_res;
static uint8_t *slv_mem_res;

static void slv_init(void)
{
    if (slv_frames)
        return;
    int fb = VADPCM_FRAME_BYTES(4);
    slv_codebook = malloc_uncached(2 * VADPCM_CODEBOOK_STRIDE);
    memset(slv_codebook, 0, 2 * VADPCM_CODEBOOK_STRIDE);
    gen_codebook(slv_codebook);
    memcpy((uint8_t *)slv_codebook + VADPCM_CODEBOOK_STRIDE, slv_codebook,
        8 * sizeof(wav64_vadpcm_vector_t));
    slv_frames = malloc(SLV_FRAMES * fb);
    for (int f = 0; f < SLV_FRAMES; f++)
        sv_gen_frame(slv_frames + f * fb, f + 9000, 4);
    slv_ref = malloc(SLV_FRAMES * 16 * 2);
    wav64_vadpcm_vector_t state = {0};
    vadpcm_decode(NPREDICTORS, ORDER, slv_codebook, &state,
        SLV_FRAMES, slv_ref, slv_frames, 4);
    slv_loop_state = malloc_uncached(2 * sizeof(*slv_loop_state));
    for (int i = 0; i < 8; i++)
        slv_loop_state[0].v[i] = slv_loop_state[1].v[i] = slv_ref[SLV_INTRO - 8 + i];
    int16_t *taps = (int16_t *)((uint8_t *)slv_codebook + 128);
    taps[0] = slv_ref[SLV_INTRO + 0];
    taps[1] = slv_ref[SLV_INTRO + 1];
    taps[2] = slv_ref[SLV_INTRO + 2];
    memcpy((uint8_t *)slv_codebook + VADPCM_CODEBOOK_STRIDE + 128, taps, 6);
    slv_codec.codebook = slv_codebook;
    slv_codec.loop_state = slv_loop_state;
    slv_codec.bits = 4;
    slv_codec_stereo = slv_codec;

    slv_mem_res = malloc_uncached(SLV_FRAMES * fb + 64);
    memcpy(slv_mem_res, slv_frames, SLV_FRAMES * fb);
    slv_wave_res = slv_wave_stream;
    slv_wave_res.name = "slv-res";
    slv_wave_res.mem = slv_mem_res;
    slv_wave_res.__uuid = 0;
}

static int16_t slv_sample(int pos)
{
    if (pos < 0 || pos >= SLV_FRAMES * 16) return 0;
    return slv_ref[pos];
}

// Switch the playback rate mid-note: the playhead must advance at the rate
// that is in force for each half, and both halves must stay audible.
// Use a oneshot so a full-rate block of N samples cannot land on the same
// wrapped position as a looping waveform would after exactly one loop_len.
static bool test_mixer_freq_change(void)
{
    const int n1 = 512, n2 = 512;
    bl_init();
    sv_silence();
    mixer_ch_set_limits(SV_CHANNEL, 0, 48000, 0);
    // Settle the volume filter at step 0 so warmup does not consume the oneshot
    // (sv_start's 2048-sample warmup would land in the quiet middle of the ramp).
    mixer_ch_play(SV_CHANNEL, &bl_wave_oneshot_res);
    mixer_ch_set_vol(SV_CHANNEL, 0.5f, 0.5f);
    mixer_ch_set_pos(SV_CHANNEL, BL_INTRO - 64);
    mixer_ch_set_freq(SV_CHANNEL, 0);
    sv_mix(2048);
    mixer_ch_set_freq(SV_CHANNEL, 22050);

    double pos0 = mixer_ch_get_pos(SV_CHANNEL);
    sv_mix(n1);
    int peak = 0;
    for (int i = 0; i < n1 * 2; i++) {
        int a = sv_out[i] < 0 ? -sv_out[i] : sv_out[i];
        if (a > peak) peak = a;
    }
    if (peak < 4096) {
        printf("FAILED freq change: silent at half rate (peak %d)\n", peak);
        return false;
    }

    double pos1 = mixer_ch_get_pos(SV_CHANNEL);
    double advanced = pos1 - pos0;
    if (advanced < n1 / 2.0 - 2 || advanced > n1 / 2.0 + 2) {
        printf("FAILED freq change: half-rate advance %.2f, expected ~%d\n",
            advanced, n1 / 2);
        return false;
    }

    mixer_ch_set_freq(SV_CHANNEL, 44100);
    double pos2 = mixer_ch_get_pos(SV_CHANNEL);
    sv_mix(n2);
    peak = 0;
    for (int i = 0; i < n2 * 2; i++) {
        int a = sv_out[i] < 0 ? -sv_out[i] : sv_out[i];
        if (a > peak) peak = a;
    }
    if (peak < 4096) {
        printf("FAILED freq change: silent at full rate (peak %d)\n", peak);
        return false;
    }

    advanced = mixer_ch_get_pos(SV_CHANNEL) - pos2;
    if (advanced < n2 - 2 || advanced > n2 + 2) {
        printf("FAILED freq change: full-rate advance %.2f, expected ~%d\n",
            advanced, n2);
        return false;
    }
    return true;
}

// Stop while the playhead is still inside the waveform, not at the end.
static bool test_mixer_stop_mid(void)
{
    bl_init();
    sv_silence();
    mixer_ch_set_limits(SV_CHANNEL, 0, 48000, 0);
    sv_start(&bl_wave_oneshot_stream, 0, BL_FREQ);
    sv_mix(128);

    double pos = mixer_ch_get_pos(SV_CHANNEL);
    if (pos < 32 || pos >= BL_LEN - 32) {
        printf("FAILED stop mid: position %.1f not inside the waveform\n", pos);
        return false;
    }
    if (!mixer_ch_playing(SV_CHANNEL)) {
        printf("FAILED stop mid: channel already idle at position %.1f\n", pos);
        return false;
    }

    mixer_ch_stop(SV_CHANNEL);
    if (mixer_ch_playing(SV_CHANNEL)) {
        printf("FAILED stop mid: channel still playing after stop\n");
        return false;
    }

    // Let the volume ramp decay, then the output must be silent.
    sv_mix(2048);
    for (int i = 0; i < 8; i++)
        sv_mix(4096);
    for (int i = 0; i < 4096; i++) {
        if (sv_out[i * 2] || sv_out[i * 2 + 1]) {
            printf("FAILED stop mid: still audible at sample %d: %d/%d\n",
                i, sv_out[i * 2], sv_out[i * 2 + 1]);
            return false;
        }
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Volume ramps
//
// The loop of the baseline asset is a square wave of constant absolute
// amplitude, so playing it at the output rate makes the envelope of the
// capture the channel volume itself, sample by sample.
//////////////////////////////////////////////////////////////////////////////

// Deviation of the envelope from a straight line, and from the volume that was
// asked for. The ucode walks the volume one output sample at a time, in steps
// it recomputes every four of them, so the envelope follows the line it is
// meant to within the rounding of those steps.
#define VR_TOL_LINE   64
#define VR_TOL_LEVEL  128

static int vr_amp(int i)   { int a = sv_out[i*2];   return a < 0 ? -a : a; }
static int vr_amp_r(int i) { int a = sv_out[i*2+1]; return a < 0 ? -a : a; }

static void vr_start(waveform_t *wave, float lvol, float rvol)
{
    bl_init();
    sv_silence();
    mixer_ch_set_limits(SV_CHANNEL, 0, 48000, 0);
    sv_start(wave, BL_INTRO, BL_FREQ);
    mixer_ch_set_vol(SV_CHANNEL, lvol, rvol);
    sv_mix(2048);                     // let the declick ramp settle
}

// A linear ramp comes out as a straight line whatever the staircase of the
// ucode does to it, so the capture is checked against the line through two of
// its own interior points: a ramp that moves once per mix round (or once per
// poll) is caught this way. The endpoints of that line are then checked
// against the volumes that were asked for.
static bool vr_check_linear(const char *tag, int nramp, float v0, float v1,
    int (*amp)(int))
{
    int i1 = nramp/4, i2 = nramp*3/4;
    int a1 = amp(i1), a2 = amp(i2);
    int worst = 0, wi = -1;

    for (int i = nramp/8; i <= nramp*7/8; i++) {
        int want = a1 + (int)((int64_t)(a2 - a1) * (i - i1) / (i2 - i1));
        int d = amp(i) - want; if (d < 0) d = -d;
        if (d > worst) { worst = d; wi = i; }
    }
    if (worst > VR_TOL_LINE) {
        printf("FAILED %s: envelope off its own line by %d at %d\n", tag, worst, wi);
        printf("   [%d]=%d [%d]=%d [%d]=%d\n", i1, a1, wi, amp(wi), i2, a2);
        return false;
    }
    for (int k = 0; k < 2; k++) {
        int i = k ? i2 : i1, a = k ? a2 : a1;
        int want = (int)(BL_AMP * (v0 + (v1 - v0) * i / (float)nramp));
        int d = a - want; if (d < 0) d = -d;
        if (d > VR_TOL_LEVEL) {
            printf("FAILED %s: envelope is %d at %d, expected %d\n", tag, a, i, want);
            return false;
        }
    }
    return true;
}

static bool vr_check_level(const char *tag, int from, int to, float v,
    int (*amp)(int))
{
    int want = (int)(BL_AMP * v), worst = 0, wi = -1;
    for (int i = from; i < to; i++) {
        int d = amp(i) - want; if (d < 0) d = -d;
        if (d > worst) { worst = d; wi = i; }
    }
    if (worst > VR_TOL_LEVEL) {
        printf("FAILED %s: volume %d at %d, expected %d\n", tag, amp(wi), wi, want);
        return false;
    }
    return true;
}

// A ramp spanning several rounds and several polls must come out as one
// straight line, with no step at the boundaries.
static bool test_mixer_vol_ramp(waveform_t *wave)
{
    const int nramp = 4096, ntail = 2048;

    vr_start(wave, 0.5f, 0.5f);
    mixer_ch_set_vol_ramp(SV_CHANNEL, 0.0f, 0.0f, nramp);
    sv_mix(nramp + ntail);
    if (!vr_check_linear("vol ramp down", nramp, 0.5f, 0.0f, vr_amp))
        return false;
    if (!vr_check_level("vol ramp down", nramp + 512, nramp + ntail, 0.0f, vr_amp))
        return false;

    vr_start(wave, 0.0f, 0.0f);
    mixer_ch_set_vol_ramp(SV_CHANNEL, 0.75f, 0.75f, nramp);
    sv_mix(nramp + ntail);
    if (!vr_check_linear("vol ramp up", nramp, 0.0f, 0.75f, vr_amp))
        return false;
    if (!vr_check_level("vol ramp up", nramp + 512, nramp + ntail, 0.75f, vr_amp))
        return false;

    // The two sides ramp independently, and on a stereo waveform they are two
    // separate MIX_CHANNELs that must stay in step.
    vr_start(wave, 0.5f, 0.5f);
    mixer_ch_set_vol_ramp(SV_CHANNEL, 0.0f, 1.0f, nramp);
    sv_mix(nramp + ntail);
    if (!vr_check_linear("vol ramp pan L", nramp, 0.5f, 0.0f, vr_amp))
        return false;
    if (!vr_check_linear("vol ramp pan R", nramp, 0.5f, 1.0f, vr_amp_r))
        return false;
    return true;
}

// A ramp shorter than a mix round still has to run: what the ucode is given
// is an increment per block of 4 samples, not one per round.
static bool test_mixer_vol_ramp_short(void)
{
    vr_start(&bl_wave_loop_res, 0.5f, 0.5f);
    mixer_ch_set_vol_ramp(SV_CHANNEL, 0.0f, 0.0f, 64);
    sv_mix(1024);
    // Well before the round the ramp lives in is over.
    if (vr_amp(200) > BL_AMP / 8) {
        printf("FAILED vol ramp short: still at %d after 200 samples\n", vr_amp(200));
        return false;
    }
    return vr_check_level("vol ramp short", 512, 1024, 0.0f, vr_amp);
}

// A new ramp starts from the volume the channel has now, not from the target
// of the ramp it replaces; #mixer_ch_set_vol cancels it outright.
static bool test_mixer_vol_ramp_replace(void)
{
    const int nramp = 8192;

    vr_start(&bl_wave_loop_res, 1.0f, 1.0f);
    mixer_ch_set_vol_ramp(SV_CHANNEL, 0.0f, 0.0f, nramp);
    sv_mix(nramp/2);
    if (!vr_check_level("vol ramp replace", nramp/2 - 64, nramp/2, 0.5f, vr_amp))
        return false;
    mixer_ch_set_vol_ramp(SV_CHANNEL, 1.0f, 1.0f, 4096);
    sv_mix(4096 + 1024);
    if (!vr_check_linear("vol ramp replace", 4096, 0.5f, 1.0f, vr_amp))
        return false;
    if (!vr_check_level("vol ramp replace", 4096 + 512, 4096 + 1024, 1.0f, vr_amp))
        return false;

    vr_start(&bl_wave_loop_res, 1.0f, 1.0f);
    mixer_ch_set_vol_ramp(SV_CHANNEL, 0.0f, 0.0f, nramp);
    sv_mix(nramp/2);
    mixer_ch_set_vol(SV_CHANNEL, 0.25f, 0.25f);
    sv_mix(4096);
    // The cancelled ramp must not resume, now or later.
    if (!vr_check_level("vol ramp cancel", 512, 4096, 0.25f, vr_amp))
        return false;
    sv_mix(4096);
    return vr_check_level("vol ramp cancel", 0, 4096, 0.25f, vr_amp);
}

// The ucode is given a slope and the value it is heading for, and clamps to
// it: a ramp may therefore end anywhere inside a round, at a sample that is
// not even a multiple of the 4 the slope is recomputed at. What this looks for
// is the volume sailing past its target, which is what would happen if the
// slope kept being applied to the end of the round.
static bool test_mixer_vol_ramp_land(void)
{
    static const int durations[] = { 13, 77, 300, 1001 };

    for (int i = 0; i < 4; i++) {
        int n = durations[i];

        vr_start(&bl_wave_loop_res, 1.0f, 1.0f);
        mixer_ch_set_vol_ramp(SV_CHANNEL, 0.0f, 0.0f, n);
        sv_mix(2048);
        if (!vr_check_level("vol ramp land down", n + 8, 2048, 0.0f, vr_amp))
            return false;

        // Upwards, from and to a volume that leaves room on both sides, so
        // that an overshoot shows up instead of saturating out of sight.
        vr_start(&bl_wave_loop_res, 0.25f, 0.25f);
        mixer_ch_set_vol_ramp(SV_CHANNEL, 0.5f, 0.5f, n);
        sv_mix(2048);
        if (!vr_check_level("vol ramp land up", n + 8, 2048, 0.5f, vr_amp))
            return false;
    }
    return true;
}

// The ramp moves at every output sample, and not once per group of eight: a
// steep ramp comes out as a line and not as a staircase. What this looks for
// is the jump between two neighbouring samples, which stepping once per group
// would make eight times the slope at every group boundary.
static bool test_mixer_vol_ramp_smooth(void)
{
    const int nramp = 256;
    const int slope = BL_AMP / nramp;     // envelope units per output sample

    vr_start(&bl_wave_loop_res, 0.0f, 0.0f);
    mixer_ch_set_vol_ramp(SV_CHANNEL, 1.0f, 1.0f, nramp);
    sv_mix(1024);

    int worst = 0, wi = -1;
    for (int i = 1; i < nramp - 1; i++) {
        int d = vr_amp(i) - vr_amp(i - 1); if (d < 0) d = -d;
        if (d > worst) { worst = d; wi = i; }
    }
    if (worst > slope * 2) {
        printf("FAILED vol ramp smooth: envelope jumps by %d at %d, slope is %d\n",
            worst, wi, slope);
        return false;
    }
    // ...and it is a ramp at all: an envelope that never moves would sail
    // through the test above.
    int mid = vr_amp(nramp/2);
    if (mid < BL_AMP/4 || mid > BL_AMP*3/4) {
        printf("FAILED vol ramp smooth: %d halfway, expected around %d\n",
            mid, BL_AMP/2);
        return false;
    }
    return true;
}

// mixer_ch_set_vol walks to the new volume instead of stepping to it, which is
// what keeps a music player from clicking on every tick.
static bool test_mixer_declick(void)
{
    const int nd = 128;               // MIXER_DECLICK_SAMPLES

    vr_start(&bl_wave_loop_res, 0.0f, 0.0f);
    mixer_ch_set_vol(SV_CHANNEL, 1.0f, 1.0f);
    sv_mix(1024);

    // Not a step: a quarter into the ramp the volume is still around a quarter.
    int a = vr_amp(nd/4);
    if (a > BL_AMP/2) {
        printf("FAILED declick: %d at sample %d, expected around %d\n",
            a, nd/4, BL_AMP/4);
        return false;
    }
    // ...but over and done with well within a mix round.
    return vr_check_level("declick", nd + 8, 1024, 1.0f, vr_amp);
}

// Resident VADPCM loop: same content as rf_wave, but addressed from RDRAM.
static uint8_t *rf_mem_res;
static waveform_t rf_wave_resident;

static bool test_mixer_resident_vadpcm_loop(void)
{
    if (!rf_mem_res) {
        int fb = VADPCM_FRAME_BYTES(rf_bits);
        rf_mem_res = malloc_uncached(RF_FRAMES * fb + 64);
        memcpy(rf_mem_res, rf_frames, RF_FRAMES * fb);
        rf_wave_resident = rf_wave;
        rf_wave_resident.name = "rf-loop-res";
        rf_wave_resident.mem = rf_mem_res;
        // Keep read for seek side-effects (decoder state).
        rf_wave_resident.__uuid = 0;
    }
    return test_mixer_loop_exact(&rf_wave_resident, RF_LOOP_START - 64);
}

//////////////////////////////////////////////////////////////////////////////
// Channel allocation / mixer_play
//////////////////////////////////////////////////////////////////////////////

static void alloc_silence(void)
{
	for (int i = 0; i < 8; i++)
		mixer_ch_stop(i);
	sv_mix(256);
}

/** Free preferred over steal; quieter over louder; older over newer. */
static bool test_mixer_alloc_order(void)
{
	alloc_silence();
	int out[4];

	// Fill ch0-2; ch3 free → alloc prefers the free channel.
	mixer_ch_play(0, &bl_wave_loop_res);
	mixer_ch_set_priority(0, MIXER_PRIORITY_SFX);
	mixer_ch_play(1, &bl_wave_loop_res);
	mixer_ch_set_priority(1, MIXER_PRIORITY_SFX);
	mixer_ch_play(2, &bl_wave_loop_res);
	mixer_ch_set_priority(2, MIXER_PRIORITY_SFX);
	if (mixer_ch_alloc(0, 4, 1, false, MIXER_PRIORITY_SFX, NULL, out) != 1 ||
		out[0] != 3) {
		printf("FAILED alloc order: free channel not preferred (got %d)\n",
			out[0]);
		return false;
	}

	// All busy, same prio: quieter wins.
	mixer_ch_play(3, &bl_wave_loop_res);
	mixer_ch_set_priority(3, MIXER_PRIORITY_SFX);
	mixer_ch_set_vol_ramp(0, 1.0f, 1.0f, 0);
	mixer_ch_set_vol_ramp(1, 0.25f, 0.25f, 0);
	mixer_ch_set_vol_ramp(2, 0.75f, 0.75f, 0);
	mixer_ch_set_vol_ramp(3, 0.5f, 0.5f, 0);
	if (mixer_ch_alloc(0, 4, 1, false, MIXER_PRIORITY_SFX, NULL, out) != 1 ||
		out[0] != 1) {
		printf("FAILED alloc order: quietest not preferred (got %d)\n", out[0]);
		return false;
	}

	// Equal volume: oldest wins. Restart 1/2/3 after aging ch0.
	alloc_silence();
	mixer_ch_play(0, &bl_wave_loop_res);
	mixer_ch_set_priority(0, MIXER_PRIORITY_SFX);
	mixer_ch_set_vol_ramp(0, 0.5f, 0.5f, 0);
	sv_mix(512);
	mixer_ch_play(1, &bl_wave_loop_res);
	mixer_ch_set_priority(1, MIXER_PRIORITY_SFX);
	mixer_ch_set_vol_ramp(1, 0.5f, 0.5f, 0);
	mixer_ch_play(2, &bl_wave_loop_res);
	mixer_ch_set_priority(2, MIXER_PRIORITY_SFX);
	mixer_ch_set_vol_ramp(2, 0.5f, 0.5f, 0);
	if (mixer_ch_alloc(0, 3, 1, false, MIXER_PRIORITY_SFX, NULL, out) != 1 ||
		out[0] != 0) {
		printf("FAILED alloc order: oldest not preferred (got %d)\n", out[0]);
		return false;
	}

	return true;
}

/** Victims with prio > request are not stealable. */
static bool test_mixer_alloc_priority(void)
{
	alloc_silence();
	int out[2];

	for (int i = 0; i < 4; i++) {
		mixer_ch_play(i, &bl_wave_loop_res);
		// Leave default MIXER_PRIORITY_MAX from mixer_ch_play.
	}
	if (mixer_ch_alloc(0, 4, 1, false, MIXER_PRIORITY_SFX, NULL, out) != 0) {
		printf("FAILED alloc prio: stole a MAX-priority channel\n");
		return false;
	}

	mixer_ch_set_priority(2, MIXER_PRIORITY_SFX);
	if (mixer_ch_alloc(0, 4, 1, false, MIXER_PRIORITY_SFX, NULL, out) != 1 ||
		out[0] != 2) {
		printf("FAILED alloc prio: expected ch2 (got n=%d ch=%d)\n",
			out[0] >= 0 ? 1 : 0, out[0]);
		return false;
	}

	// Equal priority is stealable.
	mixer_ch_set_priority(0, 100);
	mixer_ch_set_priority(1, 100);
	mixer_ch_set_priority(2, 100);
	mixer_ch_set_priority(3, 100);
	if (mixer_ch_alloc(0, 4, 1, false, 100, NULL, out) != 1) {
		printf("FAILED alloc prio: equal priority should be stealable\n");
		return false;
	}
	return true;
}

/** count larger than availability returns a partial plan. */
static bool test_mixer_alloc_partial(void)
{
	alloc_silence();
	int out[8];
	mixer_ch_play(0, &bl_wave_loop_res);
	mixer_ch_set_priority(0, MIXER_PRIORITY_SFX);
	mixer_ch_play(1, &bl_wave_loop_res);
	mixer_ch_set_priority(1, MIXER_PRIORITY_SFX);
	// Only 2 free in a window of 4.
	int n = mixer_ch_alloc(0, 4, 4, false, MIXER_PRIORITY_MIN, NULL, out);
	if (n != 2 || out[0] != 2 || out[1] != 3) {
		printf("FAILED alloc partial: n=%d out=%d,%d (want 2: 2,3)\n",
			n, out[0], out[1]);
		return false;
	}
	return true;
}

/** Plan-only: channels keep playing; repeated calls are stable. */
static bool test_mixer_alloc_plan_only(void)
{
	alloc_silence();
	int out1[2], out2[2];
	mixer_ch_play(0, &bl_wave_loop_res);
	mixer_ch_set_priority(0, MIXER_PRIORITY_SFX);
	mixer_ch_set_vol_ramp(0, 0.5f, 0.5f, 0);
	mixer_ch_play(1, &bl_wave_loop_res);
	mixer_ch_set_priority(1, MIXER_PRIORITY_SFX);
	mixer_ch_set_vol_ramp(1, 0.5f, 0.5f, 0);

	if (mixer_ch_alloc(0, 2, 1, false, MIXER_PRIORITY_SFX, NULL, out1) != 1) {
		printf("FAILED alloc plan-only: first plan empty\n");
		return false;
	}
	if (!mixer_ch_playing(0) || !mixer_ch_playing(1)) {
		printf("FAILED alloc plan-only: plan stopped a channel\n");
		return false;
	}
	if (mixer_ch_alloc(0, 2, 1, false, MIXER_PRIORITY_SFX, NULL, out2) != 1 ||
		out2[0] != out1[0]) {
		printf("FAILED alloc plan-only: second plan differs (%d vs %d)\n",
			out1[0], out2[0]);
		return false;
	}
	return true;
}

/** Stereo pairs: contiguous primary, never last channel, never scattered. */
static bool test_mixer_alloc_stereo(void)
{
	alloc_silence();
	int out[4];

	// Free window: lowest contiguous pair.
	if (mixer_ch_alloc(0, 8, 1, true, MIXER_PRIORITY_SFX, NULL, out) != 1 ||
		out[0] != 0) {
		printf("FAILED alloc stereo: expected primary 0, got %d\n", out[0]);
		return false;
	}

	// Occupy 0; next pair is 1-2? Actually 0-1 is busy if 0 plays stereo.
	mixer_ch_play(0, &bl_wave_loop_stereo);
	mixer_ch_set_priority(0, MIXER_PRIORITY_SFX);
	if (mixer_ch_alloc(0, 8, 1, true, MIXER_PRIORITY_MIN, NULL, out) != 1 ||
		out[0] != 2) {
		printf("FAILED alloc stereo: expected next free pair at 2, got %d\n",
			out[0]);
		return false;
	}

	// Never return the stereo sub as a primary (mono alloc over a stereo pair).
	if (mixer_ch_alloc(0, 8, 1, false, MIXER_PRIORITY_MIN, NULL, out) != 1 ||
		out[0] == 1) {
		printf("FAILED alloc stereo: returned STEREO_SUB as primary (%d)\n",
			out[0]);
		return false;
	}

	// Window with no contiguous free pair even if scattered frees exist.
	alloc_silence();
	mixer_ch_play(1, &bl_wave_loop_res);
	mixer_ch_play(3, &bl_wave_loop_res);
	// Free: 0,2,4,5,6,7 — but in window [0,4) free are 0 and 2 (not a pair).
	if (mixer_ch_alloc(0, 4, 1, true, MIXER_PRIORITY_MIN, NULL, out) != 0) {
		printf("FAILED alloc stereo: found pair in scattered window (ch=%d)\n",
			out[0]);
		return false;
	}

	// Last mixer channel cannot be a stereo primary.
	alloc_silence();
	for (int i = 0; i < 7; i++)
		mixer_ch_play(i, &bl_wave_loop_res);
	if (mixer_ch_alloc(0, 8, 1, true, MIXER_PRIORITY_MIN, NULL, out) != 0) {
		printf("FAILED alloc stereo: planned last channel as stereo primary\n");
		return false;
	}
	return true;
}

/** Channels whose limits reject the waveform are skipped. */
static bool test_mixer_alloc_limits(void)
{
	alloc_silence();
	int out[2];

	mixer_ch_set_limits(0, 8, BL_FREQ, 0);
	mixer_ch_set_limits(1, 16, 8000, 0);
	// ch2 keeps defaults (16-bit, full rate) and is free.
	if (mixer_ch_alloc(0, 3, 1, false, MIXER_PRIORITY_SFX,
		&bl_wave_loop_res, out) != 1 || out[0] != 2) {
		printf("FAILED alloc limits: expected ch2, got %d\n", out[0]);
		mixer_ch_set_limits(0, 16, BL_FREQ, 0);
		mixer_ch_set_limits(1, 16, BL_FREQ, 0);
		return false;
	}
	mixer_ch_set_limits(0, 16, BL_FREQ, 0);
	mixer_ch_set_limits(1, 16, BL_FREQ, 0);
	return true;
}

/** mixer_play: free channel, steal SFX, refuse music, stereo, state reset. */
static bool test_mixer_play(void)
{
	alloc_silence();

	int ch = mixer_play(&bl_wave_oneshot_res, MIXER_PRIORITY_SFX);
	if (ch < 0 || !mixer_ch_playing(ch)) {
		printf("FAILED mixer_play: free-channel play failed (ch=%d)\n", ch);
		return false;
	}

	// Saturate with SFX; next play steals one of them.
	alloc_silence();
	int occupied[8];
	for (int i = 0; i < 8; i++) {
		occupied[i] = mixer_play(&bl_wave_loop_res, MIXER_PRIORITY_SFX);
		if (occupied[i] < 0) {
			printf("FAILED mixer_play: could not fill channel %d\n", i);
			return false;
		}
	}
	int stolen = mixer_play(&bl_wave_oneshot_res, MIXER_PRIORITY_SFX);
	if (stolen < 0) {
		printf("FAILED mixer_play: failed to steal another SFX\n");
		return false;
	}

	// Music-priority channels are not stolen by SFX.
	alloc_silence();
	for (int i = 0; i < 8; i++) {
		mixer_ch_play(i, &bl_wave_loop_res);
		mixer_ch_set_priority(i, MIXER_PRIORITY_MUSIC);
	}
	if (mixer_play(&bl_wave_oneshot_res, MIXER_PRIORITY_SFX) != -1) {
		printf("FAILED mixer_play: stole a MUSIC-priority channel\n");
		return false;
	}

	// Stereo picks a contiguous pair.
	alloc_silence();
	mixer_ch_play(0, &bl_wave_loop_res);
	mixer_ch_set_priority(0, MIXER_PRIORITY_SFX);
	ch = mixer_play(&bl_wave_loop_stereo, MIXER_PRIORITY_SFX);
	if (ch != 1) {
		printf("FAILED mixer_play: stereo expected ch1, got %d\n", ch);
		return false;
	}
	if (!mixer_ch_playing(1) || !mixer_ch_playing(2)) {
		printf("FAILED mixer_play: stereo pair not both playing\n");
		return false;
	}

	// Volume/pan and force_mono are reset before play.
	alloc_silence();
	mixer_ch_play(0, &bl_wave_loop_res);
	mixer_ch_set_vol_pan(0, 0.25f, 1.0f);
	mixer_ch_set_force_mono(0, true);
	mixer_ch_stop(0);
	ch = mixer_play(&bl_wave_oneshot_res, MIXER_PRIORITY_SFX);
	if (ch != 0) {
		printf("FAILED mixer_play: reset test expected ch0, got %d\n", ch);
		return false;
	}
	if (mixer_ch_get_force_mono(0)) {
		printf("FAILED mixer_play: force_mono not cleared\n");
		return false;
	}
	// Settled volume should be full (1,1): mix a little and check amplitude.
	sv_mix(256);
	int peak = 0;
	for (int i = 0; i < 256; i++) {
		int a = sv_out[i * 2];
		if (a < 0) a = -a;
		if (a > peak) peak = a;
	}
	if (peak < BL_AMP * 3 / 4) {
		printf("FAILED mixer_play: volume not reset (peak=%d)\n", peak);
		return false;
	}
	return true;
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

    const float bc_freqs[] = { 48000, 44100, 32000, 22050 };
    for (int i = 0; i < 4; i++) {
        total++;
        if (!test_mixer_block_codec_loop(bc_freqs[i]))
            failed++;
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
    for (int i = 0; i < 5; i++) {
        total++;
        if (!test_mixer_queue_depth(sv_freqs[i]))
            failed++;
    }
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

    // Resident / frequency-change / mid-stop / set_loop matrix.
    printf("Baseline mixer scenarios\n");
    fflush(stdout);
    bl_init();
    total++; if (!test_mixer_baseline_pcm(&bl_wave_oneshot_res, 0, 300)) failed++;
    total++; if (!test_mixer_baseline_pcm(&bl_wave_oneshot_stream, 0, 300)) failed++;
    total++; if (!test_mixer_baseline_pcm(&bl_wave_loop_res, BL_INTRO - 32, 1024)) failed++;
    total++; if (!test_mixer_baseline_pcm(&bl_wave_loop_stream, BL_INTRO - 32, 1024)) failed++;
    total++; if (!test_mixer_baseline_pcm(&bl_wave_loop_res, BL_INTRO + 8, 1024)) failed++;

    printf("mixer_ch_set_loop matrix\n");
    fflush(stdout);
    #define SL_RUN(w, when, s) do { \
        total++; if (!test_mixer_set_loop((w), (when), (s))) failed++; \
    } while (0)
    // PCM streamed mono: every note-off placement, including the fundamentals
    // near loop_end and just after a wrap. Release is shorter than a round.
    SL_RUN(&bl_wave_loop_stream, SL_BEFORE, bl_sample);
    SL_RUN(&bl_wave_loop_stream, SL_INSIDE, bl_sample);
    SL_RUN(&bl_wave_loop_stream, SL_NEAR_END, bl_sample);
    SL_RUN(&bl_wave_loop_stream, SL_AFTER_WRAP, bl_sample);
    // PCM resident with release (mid-loop only: wraps would Hermite into release).
    SL_RUN(&bl_wave_loop_res_rel, SL_INSIDE, bl_sample);
    // PCM stereo streamed.
    SL_RUN(&bl_wave_loop_stereo, SL_INSIDE, bl_sample);
    // Loop shorter than MIXER_LOOP_OVERREAD.
    SL_RUN(&bl_wave_tiny, SL_INSIDE, bl_tiny_sample);
    SL_RUN(&bl_wave_tiny, SL_NEAR_END, bl_tiny_sample);
    // Loop pinned in the samplebuffer.
    SL_RUN(&bl_wave_pin, SL_INSIDE, bl_pin_sample);
    SL_RUN(&bl_wave_pin, SL_AFTER_WRAP, bl_pin_sample);

    slv_init();
    // VADPCM streamed/resident; release length not a multiple of 16.
    // BEFORE is the note-off that never reached the sustain loop: the refill
    // after the unpin has to carry on from the intro, where no seek point is.
    SL_RUN(&slv_wave_stream, SL_BEFORE, slv_sample);
    SL_RUN(&slv_wave_stream, SL_INSIDE, slv_sample);
    SL_RUN(&slv_wave_stream, SL_NEAR_END, slv_sample);
    SL_RUN(&slv_wave_stream, SL_AFTER_WRAP, slv_sample);
    SL_RUN(&slv_wave_res, SL_INSIDE, slv_sample);
    // VADPCM stereo: exact left + L/R identity across unpin + release, with
    // both planes going through the intro and the post-wrap re-seed.
    SL_RUN(&slv_wave_stereo, SL_BEFORE, slv_sample);
    SL_RUN(&slv_wave_stereo, SL_INSIDE, slv_sample);
    SL_RUN(&slv_wave_stereo, SL_AFTER_WRAP, slv_sample);
    #undef SL_RUN

    printf("Volume ramps\n");
    fflush(stdout);
    total++; if (!test_mixer_vol_ramp(&bl_wave_loop_res)) failed++;
    total++; if (!test_mixer_vol_ramp(&bl_wave_loop_stereo)) failed++;
    total++; if (!test_mixer_vol_ramp_short()) failed++;
    total++; if (!test_mixer_vol_ramp_replace()) failed++;
    total++; if (!test_mixer_vol_ramp_land()) failed++;
    total++; if (!test_mixer_vol_ramp_smooth()) failed++;
    total++; if (!test_mixer_declick()) failed++;

    total++; if (!test_mixer_freq_change()) failed++;
    total++; if (!test_mixer_stop_mid()) failed++;
    rf_init(4);
    total++; if (!test_mixer_resident_vadpcm_loop()) failed++;

    printf("Channel allocation\n");
    fflush(stdout);
    total++; if (!test_mixer_alloc_order()) failed++;
    total++; if (!test_mixer_alloc_priority()) failed++;
    total++; if (!test_mixer_alloc_partial()) failed++;
    total++; if (!test_mixer_alloc_plan_only()) failed++;
    total++; if (!test_mixer_alloc_stereo()) failed++;
    total++; if (!test_mixer_alloc_limits()) failed++;
    total++; if (!test_mixer_play()) failed++;

    sv_silence();
    mixer_ch_set_limits(SV_CHANNEL, 0, 48000, 0);

    if (failed) {
        printf("\n%d/%d TESTS FAILED\n", failed, total);
        abort();
    }
    printf("\nALL TESTS PASSED (%d)\n", total);
    return 0;
}
