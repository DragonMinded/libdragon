/**
 * @file wav64_ulc.c
 * @author Dominic Szablewski <dominic@phoboslab.org>
 * @brief Support for ulc-compressed WAV64 files
 * 
 * ULC (Ultra-Low-Complexity Codec) is an MDCT-based transform codec, created
 * by Aikku93 - https://github.com/Aikku93. For encoding, PCM audio is split 
 * into fixed-size blocks, transformed into frequency-domain coefficients, 
 * quantized, and then stored with a compact scheme. Each coefficient, zero run 
 * or noise fill is described by one or more nibbles, i.e. the stream is 
 * 4 bit aligned, though every new block starts at a byte boundary.
 * 
 * This decoder is based on https://github.com/Aikku93/ulc-codec - the reference 
 * implementation and adapted for the N64. Specifically the IMDCT, lapping 
 * output scaling and stereo interleaving is done on the RSP, while bistream
 * reading and unpacking of coefficients remains on the CPU.
 * 
 * The RSP works entirely on 16bit values, interpreted as fixed point with 
 * 14 fractional bits. This produces a higher noise floor than the reference 
 * decoder, but with the benefit of fitting a block of 1024 coefficients/samples 
 * on the RSP. This is also the reason the block size of 1024 is fixed for this
 * implementation, while ULC itself supports different block sizes.
 * 
 * Great care has been taken to make this implementation as free of rspq_wait()
 * and rspq_highpri_sync() calls as possible. Coefficients are unpacked into the
 * space provided by samplebuffer_apppend(). For mono output the RSP can 
 * directly overwrite the coefficients. For stereo, we have to store the planar
 * mid/side samples into a temp buffer and then - through another command in the
 * RSP overlay - interleave them and write them to the samplebuffer. 
 * 
 * Because ULC has 2 blocks of "preroll" that produce no output, we need two 
 * more temp buffers to store the coefficients for those blocks. Otherwise the
 * CPU decoder risks overwriting coefficients before the RSP has consumed them.
 * 
 * To allow seeking to arbitrary position inside the stream, a sparse table with
 * the byte offset of each 8th block is appended at the end of the wav64 file. 
 * We read the offset for the closest block before the target from this table 
 * and then seek through the bitstream until we reach the target. This sparse 
 * table adds just ~21 bytes per second to the encoded file size and requires
 * no previously defined seek-points.
 */

#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdalign.h>
#include <stdbool.h>
#include <unistd.h>
#include "wav64.h"
#include "wav64_internal.h"
#include "wav64_ulc_internal.h"
#include "samplebuffer.h"
#include "n64sys.h"
#include "rspq.h"
#include "utils.h"

/** @brief Fixed transform block size (samples).
 *
 * Fixed so the RSP code can make size assumptions; 1024 is also the
 * maximum that fits in RSP DMEM.
 */
#define ULC_BLOCK_SIZE 1024

/** @brief Number of blocks between consecutive seek-table entries. */
#define ULC_SEEK_INTERVAL_BLOCKS 8

/** @brief Codec delay blocks that must be decoded before valid output.
 *
 * Required at stream start and after seeking.
 */
#define ULC_PREROLL_BLOCKS 2

/** @brief Per-channel ULC decoder state
 *
 * Stored in the samplebuffer state area. Immediately after this struct
 * (64-byte aligned) live #temp_buffer and #transform_inv_lap.
 */
typedef struct ulc_state_t {
    int channels;               ///< Number of channels (1 or 2)
    int last_sb_size;           ///< Size of the last decoded subblock (for overlap)
    uint32_t stream_offset;     ///< Byte offset of the next compressed block from the stream base
    uint32_t block_index;       ///< Index of the next block to decode
    int16_t *temp_buffer;       ///< Scratch for planar mid/side and preroll coefficient retention
    int16_t *transform_inv_lap; ///< Retained second half of previous IMDCT transforms (overlap-add)
} ulc_state_t;

_Static_assert(sizeof(ulc_state_t) == 24, "invalid ULC decoder state size");

// Subblock decimation pattern
// Each subblock is coded in 4 bits (LSB to MSB):
//  Bit0..2: Subblock shift (ie. block_size >> Shift)
//  Bit3:    Transient flag (ie. apply overlap scaling to that subblock)
static const uint16_t ulc_decimation_patterns[] = {
    0x0000 | 0x0000, // 0000: N/1 (Unused)
    0x0000 | 0x0008, // 0001: N/1*
    0x0011 | 0x0008, // 0010: N/2*,N/2
    0x0011 | 0x0080, // 0011: N/2,N/2*
    0x0122 | 0x0008, // 0100: N/4*,N/4,N/2
    0x0122 | 0x0080, // 0101: N/4,N/4*,N/2
    0x0221 | 0x0080, // 0110: N/2,N/4*,N/4
    0x0221 | 0x0800, // 0111: N/2,N/4,N/4*
    0x1233 | 0x0008, // 1000: N/8*,N/8,N/4,N/2
    0x1233 | 0x0080, // 1001: N/8,N/8*,N/4,N/2
    0x1332 | 0x0080, // 1010: N/4,N/8*,N/8,N/2
    0x1332 | 0x0800, // 1011: N/4,N/8,N/8*,N/2
    0x2331 | 0x0080, // 1100: N/2,N/8*,N/8,N/4
    0x2331 | 0x0800, // 1101: N/2,N/8,N/8*,N/4
    0x3321 | 0x0800, // 1110: N/2,N/4,N/8*,N/8
    0x3321 | 0x8000, // 1111: N/2,N/4,N/8,N/8*
};

static uint32_t ulc_rsp_overlay_id = 0;

/** @brief RSP overlay ID for the ULC decoder. */
DEFINE_RSP_UCODE(rsp_ulc);

enum {
    ULC_RSP_CMD_synthesize = 0x0,
    ULC_RSP_CMD_stereo_interleave = 0x1
};

// These flags control which steps are executed on the RSP for CMD_synthesize.
// - TRANSFORM: apply the inverse MDCT, converting frequency coefficients
//     into time-domain samples.
// - LAP: overlap-add the current transform with the retained second half of
//     previous transforms, and update the retained lap state.
// - CLEAR_LAP: when seeking we need to clear the lap buffer for the very first 
//     decoded block or subblock for each channel. Be sure to unset this flag 
//     after the first call.
// - OUTPUT_DISCARD: preroll blocks need to be decoded and update the lap
//     buffer, but we don't need to output samples.
// - SCALE_SAMPLES: for mono we can directly scale from q14 to q15 and write 
//     the final output to dst. For stereo, scaling is done in the separate 
//     stereo interleave call.

#define ULC_RSP_TRANSFORM      0x80000000u          ///< Apply the inverse MDCT, converting frequency coefficients into time-domain samples.
#define ULC_RSP_LAP            0x40000000u          ///< Overlap-add the current transform with the retained second half of previous transforms, and update the retained lap state.
#define ULC_RSP_CLEAR_LAP      0x20000000u          ///< Clear the lap buffer for the very first decoded block or subblock for each channel.
#define ULC_RSP_DISCARD_OUTPUT 0x10000000u          ///< Preroll blocks need to be decoded and update the lap buffer, but we don't need to output samples.
#define ULC_RSP_SCALE_SAMPLES  0x08000000u          ///< For mono we can directly scale from q14 to q15 and write the final output to dst. For stereo, scaling is done in the separate stereo interleave call.


// The RSP code works in int16 and assumes 14 fractional bits. We need to scale
// decoded coefficients accordingly.

#define ULC_FP_BITS 14                          ///< Fixed point precision bits.

#define ULC_FP_ONE (1 << ULC_FP_BITS)          ///< Fixed point one.

/** @brief Escape sequence for stop code. */
#define ULC_ESCAPE_SEQUENCE_STOP           -1

/** @brief Escape sequence for noise fill to end. */
#define ULC_ESCAPE_SEQUENCE_STOP_NOISEFILL -2

static inline uint32_t ulc_rand(void) {
    static uint32_t seed = 1234567;
    seed ^= seed << 13; // Xorshift
    seed ^= seed >> 17;
    seed ^= seed <<  5;
    return seed;
}

static inline uint8_t ulc_read_nibble(const uint8_t **src, int *size) {
    // Fetch and shift nybble
    uint8_t x = *(*src);
    *size += 4;
    if ((*size) % 8u == 0) {
        x >>= 4; 
        (*src)++;
    }
    return x & 0xF;
}

static inline int ulc_read_quantizer(const uint8_t **src, int *size) {
    // Fh,0h..Dh:     Quantizer change
    int qi = ulc_read_nibble(src, size);
    if (qi == 0xF) {
        // Fh,Fh,Zh,Yh,Xh: Noise fill (to end; exp-decay)
        return ULC_ESCAPE_SEQUENCE_STOP_NOISEFILL;
    }
    if (qi == 0xE) {
        // Fh,Eh,0h..Ch:   Quantizer change (extended precision)
        qi += ulc_read_nibble(src, size); 
        if (qi == 0xE + 0xF) {
            return ULC_ESCAPE_SEQUENCE_STOP;
        }
    }
    return qi;
}

static inline float ulc_expand_quantizer(int qi) {
    return ULC_FP_ONE * 0x1.0p-31f * ((1u<<(31-5)) >> qi); //! q14 / (2^5 * 2^qi)
}

static void ulc_decode_coeffs(int16_t *coeff_dst, int sb_size, const uint8_t **src, int *size) {
    int32_t n, v;

    // Check first quantizer for Stop code
    v = ulc_read_quantizer(src, size);
    if (v == ULC_ESCAPE_SEQUENCE_STOP) {
        // [Fh,]Eh,Fh: Stop
        do {
            *coeff_dst++ = 0; 
        } while(--sb_size);
        return;
    }

    // Unpack the [sub]block's coefficients
    int quantizer = v;
    while (sb_size > 0) {
        v = ulc_read_nibble(src, size);

        // -7h..-2h, +2..+7h: Normal coefficient
        if (!(0x8103u & (1u << v))) {
            // Dequantization is an exact power-of-two scale:
            // q14 * 2^-5 * 2^-quantizer.
            v = (v ^ 0x8) - 0x8;
            int magnitude = (v * v * (ULC_FP_ONE >> 5)) >> quantizer;
            *coeff_dst++ = v < 0 ? -magnitude : magnitude;
            sb_size--;
        }

        // 0h,0h..Fh: Zeros fill (1 .. 16 coefficients)
        else if (v == 0x0) {
            n = ulc_read_nibble(src, size) + 1;
            sb_size -= n;
            do {
                *coeff_dst++ = 0; 
            } while (--n);
        }

        // 1h,Yh,Xh: 33 .. 288 zeros fill
        else if (v == 0x1) {
            n = ulc_read_nibble(src, size);
            n = ulc_read_nibble(src, size) | (n<<4);
            n += 33;
            sb_size -= n;
            do {
                *coeff_dst++ = 0; 
            } while (--n);
        }

        // 8h,Zh,Yh,Xh: 16 .. 527 noise fill
        else if (v == 0x8) {
            n = ulc_read_nibble(src, size);
            n = ulc_read_nibble(src, size) | (n<<4);
            v = ulc_read_nibble(src, size);
            n = (v&1) | (n<<1);
            v = (v>>1) + 1;
            n += 16;
            sb_size -= n;

            float p = v * v * ulc_expand_quantizer(quantizer) * (1.0f/4.0f);
            do {
                if (ulc_rand() & 0x80000000) {
                    p = -p;
                }
                *coeff_dst++ = p;
            } while (--n);
        }

        // Fh,0h..Dh:   quantizer change
        else /* if (v == 0xF) */ {
            // Fh,Eh,0h..Ch: quantizer change (extended precision)
            v = ulc_read_quantizer(src, size);
            if (v >= 0) {
                quantizer = v;
            }

            // Both noise-fill-to-end and zeros-fill-to-end finish the subblock.
            else {
                // Fh,Fh,Zh,Yh,Xh: Noise fill (to end; exp-decay)
                if (v == ULC_ESCAPE_SEQUENCE_STOP_NOISEFILL) {
                    v = ulc_read_nibble(src, size) + 1;
                    n = ulc_read_nibble(src, size);
                    n = ulc_read_nibble(src, size) | (n<<4);
                    float p = v * v * ulc_expand_quantizer(quantizer) * (1.0f/16.0f);
                    float r = 1.0f + n * n * -0x1.0p-19f;
                    do {
                        if (ulc_rand() & 0x80000000) {
                            p = -p;
                        }
                        *coeff_dst++ = p;
                        p = p * r;
                    } while (--sb_size);
                }

                // Fh,Eh,Dh: Unused
                // Fh,Eh,Eh: Unused
                // Fh,Eh,Fh: Zeros fill (to end)
                else if (v == ULC_ESCAPE_SEQUENCE_STOP) {
                    do {
                        *coeff_dst++ = 0;
                    } while (--sb_size);
                }
            }
        }
    }
}

static inline void ulc_rsp_synthesize(int16_t *coeffs, int16_t *lap, int16_t *dst, int flags) {
    rspq_write(ulc_rsp_overlay_id, ULC_RSP_CMD_synthesize, PhysicalAddr(coeffs), PhysicalAddr(lap), PhysicalAddr(dst), flags);
}

static inline void ulc_rsp_stereo_interleave(int16_t *mid, int16_t *side, int16_t *dst) {
    rspq_write(ulc_rsp_overlay_id, ULC_RSP_CMD_stereo_interleave, PhysicalAddr(mid), PhysicalAddr(side), PhysicalAddr(dst));
}

static int ulc_decode_block(ulc_state_t *state, int16_t *dst, const uint8_t *src, bool clear_lap, int preroll_index) {
    int channels = state->channels;
    int16_t *lap_buffer = state->transform_inv_lap;

    int size = 0;
    int last_sb_size = 0;
    int window_ctrl = ulc_read_nibble(&src, &size);
    window_ctrl |= (window_ctrl & 0x8)
        ? ulc_read_nibble(&src, &size) << 4
        : 1 << 4;

    for (int c = 0; c < channels; c++) {
        // Reset overlap scaling for this channel
        last_sb_size = state->last_sb_size;

        // Decode the complete channel before transforming it. The subblocks
        // always sum to ULC_BLOCK_SIZE.
        struct {
            uint16_t offset;
            uint16_t size;
            uint16_t overlap;
        } sb[4];
        int sb_len = 0;

        // Normal coefficients are decoded directly into the final samplebuffer.
        // Mono output overwrites them in place, while stereo output is staged
        // in the first planar temp bank before interleaving. Each queued
        // preroll block needs its own coefficient bank because the RSP might
        // not have DMAed an earlier block before the CPU decodes the next one.
        int16_t *channel_buffer = (preroll_index >= 0)
            ? state->temp_buffer + (preroll_index * channels + c) * ULC_BLOCK_SIZE
            : dst + c * ULC_BLOCK_SIZE;
        int16_t *channel_output = (channels == 1 || preroll_index >= 0)
            ? channel_buffer
            : state->temp_buffer + c * ULC_BLOCK_SIZE;

        int coeff_offset = 0;
        uint16_t decimation_pattern = ulc_decimation_patterns[window_ctrl >> 4];
        do {
            int sb_size = ULC_BLOCK_SIZE >> (decimation_pattern & 0x7);
            ulc_decode_coeffs(channel_buffer + coeff_offset, sb_size, &src, &size);

            // Get + update overlap size and limit to that of the last subblock
            int overlap = sb_size;
            if (decimation_pattern & 0x8) {
                overlap >>= (window_ctrl & 0x7);
            }
            if (overlap > last_sb_size) {
                overlap = last_sb_size;
            }
            last_sb_size = sb_size;

            sb[sb_len].offset = coeff_offset;
            sb[sb_len].size = sb_size;
            sb[sb_len].overlap = overlap;
            sb_len++;
            coeff_offset += sb_size;
        } while (decimation_pattern >>= 4);

        uint32_t flags =
            (channels == 1      ? ULC_RSP_SCALE_SAMPLES : 0) |
            (preroll_index >= 0 ? ULC_RSP_DISCARD_OUTPUT : 0) |
            (clear_lap          ? ULC_RSP_CLEAR_LAP : 0);

        // A full 1024-sample block can remain on the RSP through transform,
        // overlap-add, lap-state update, and final output DMA.
        if (sb_len == 1) {
            uint32_t sb_flags = flags | ULC_RSP_TRANSFORM | ULC_RSP_LAP | (sb[0].overlap << 16) | ULC_BLOCK_SIZE;
            ulc_rsp_synthesize(channel_buffer, lap_buffer, channel_output, sb_flags);
        }

        // Multiple subblocks need to be transformed and _then_ lapped.
        else {
            for (int i = 0; i < sb_len; i++) {
                uint32_t sb_flags = ULC_RSP_TRANSFORM | sb[i].size;
                int16_t *sb_buffer = channel_buffer + sb[i].offset;
                ulc_rsp_synthesize(sb_buffer, 0, sb_buffer, sb_flags);
            }
            for (int i = 0; i < sb_len; i++) {
                uint32_t sb_flags = flags | ULC_RSP_LAP | (sb[i].overlap << 16) | sb[i].size;
                int16_t *sb_buffer = channel_buffer + sb[i].offset;
                int16_t *sb_output = channel_output + sb[i].offset;
                ulc_rsp_synthesize(sb_buffer, lap_buffer, sb_output, sb_flags);
                flags &= ~ULC_RSP_CLEAR_LAP;
            }
        }

        // Move to next channel
        lap_buffer += ULC_BLOCK_SIZE >> 1;
    }

    if (channels == 2 && preroll_index < 0) {
        ulc_rsp_stereo_interleave(state->temp_buffer, state->temp_buffer + ULC_BLOCK_SIZE, dst);
    }

    // Store the last [sub]block size, and return the number of bits read
    state->last_sb_size = last_sb_size;
    return size;
}

static int ulc_skip_block(const ulc_state_t *state, const uint8_t *src) {
    int size = 0;
    int window_ctrl = ulc_read_nibble(&src, &size);
    window_ctrl |= (window_ctrl & 0x8)
        ? ulc_read_nibble(&src, &size) << 4
        : 1 << 4;

    for (int c = 0; c < state->channels; c++) {
        uint16_t decimation_pattern = ulc_decimation_patterns[window_ctrl >> 4];
        do {
            int sb_size = ULC_BLOCK_SIZE >> (decimation_pattern & 0x7);
            int32_t v = ulc_read_quantizer(&src, &size);
            if (v != ULC_ESCAPE_SEQUENCE_STOP) {
                while (sb_size > 0) {
                    v = ulc_read_nibble(&src, &size);
                    if (!(0x8103u & (1u << v))) {
                        sb_size--;
                    }
                    else if (v == 0x0) {
                        v = ulc_read_nibble(&src, &size) + 1;
                        sb_size -= v;
                    }
                    else if (v == 0x1) {
                        v = ulc_read_nibble(&src, &size);
                        v = ulc_read_nibble(&src, &size) | (v << 4);
                        sb_size -= v + 33;
                    }
                    else if (v == 0x8) {
                        v = ulc_read_nibble(&src, &size);
                        v = ulc_read_nibble(&src, &size) | (v << 4);
                        v = (ulc_read_nibble(&src, &size) & 1) | (v << 1);
                        sb_size -= v + 16;
                    }
                    else /* if (v == 0xF) */ {
                        int v = ulc_read_quantizer(&src, &size);
                        if (v == ULC_ESCAPE_SEQUENCE_STOP_NOISEFILL) {
                            ulc_read_nibble(&src, &size);
                            ulc_read_nibble(&src, &size);
                            ulc_read_nibble(&src, &size);
                            break;
                        }
                        else if (v == ULC_ESCAPE_SEQUENCE_STOP) {
                            break;
                        }
                    }
                }
            }
        } while (decimation_pattern >>= 4);
    }

    return size;
}



// -----------------------------------------------------------------------------
// wav64 interface

static int wav64_ulc_state_size(int channels) {
    int size = sizeof(ulc_state_t) + 63;
    size += sizeof(int16_t) * 2 * channels * ULC_BLOCK_SIZE;
    size += sizeof(int16_t) * channels * (ULC_BLOCK_SIZE / 2);
    return size;
}

static void waveform_ulc_start(void *ctx, samplebuffer_t *sbuf) {
    wav64_t *wav = (wav64_t *)sbuf->wave;
    ulc_state_t *state = (ulc_state_t *)sbuf->state;

    assert(sbuf->state_size >= wav64_ulc_state_size(wav->wave.channels));

    uintptr_t buffer = ROUND_UP((uintptr_t)(state + 1), 64);
    state->channels = wav->wave.channels;
    state->temp_buffer = (int16_t *)buffer;
    state->transform_inv_lap = state->temp_buffer + 2 * wav->wave.channels * ULC_BLOCK_SIZE;
    state->last_sb_size = 0;
    state->stream_offset = 0;
    state->block_index = 0;
}

static void waveform_ulc_load_block(wav64_t *wav, const wav64_header_ulc_t *ext, const ulc_state_t *state, uint8_t *input) {
    lseek(wav->st->current_fd, wav->st->base_offset + state->stream_offset, SEEK_SET);

    // read() can DMA directly into this cached stack buffer. Invalidate it
    // first so the CPU decoder does not consume stale cache lines.
    data_cache_hit_writeback_invalidate(input, ext->max_block_size);
    int size = read(wav->st->current_fd, input, ext->max_block_size);
    assertf(size > 0, "wav64: %s: ULC read past end", wav->wave.name);
}

static void waveform_ulc_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
    wav64_t *wav = (wav64_t *)sbuf->wave;
    wav64_header_ulc_t *ext = wav->st->ext;
    ulc_state_t *state = (ulc_state_t *)sbuf->state;

    bool clear_lap = false;
    int preroll_blocks = 0;
    int blocks_len = DIVIDE_CEIL(wlen, ULC_BLOCK_SIZE);

    int appended = blocks_len * ULC_BLOCK_SIZE;
    int16_t *dst = samplebuffer_append(sbuf, appended);

    uint8_t alignas(16) input[ext->max_block_size];

    if (seeking) {
        assertf(wpos % ULC_BLOCK_SIZE == 0, "wav64: %s: unaligned ULC seek: %d", wav->wave.name, wpos);
        state->last_sb_size = 0;

        if (wpos == 0) {
            state->stream_offset = 0;
            state->block_index = 0;
        }
        else {
            uint32_t target_block = CLAMP(wpos / ULC_BLOCK_SIZE, 0, ext->blocks_len - 3);
            uint32_t anchor_block = target_block / ULC_SEEK_INTERVAL_BLOCKS * ULC_SEEK_INTERVAL_BLOCKS;
            uint32_t anchor_index = anchor_block / ULC_SEEK_INTERVAL_BLOCKS;

            // Read the block offset from the sparse seek table at the end of the file
            lseek(wav->st->current_fd, wav->st->base_offset + ext->seek_table_offset + anchor_index * sizeof(uint32_t), SEEK_SET);
            uint32_t anchor_offset;
            int size = read(wav->st->current_fd, &anchor_offset, sizeof(anchor_offset));
            assertf(size == sizeof(anchor_offset), "wav64: %s: cannot read ULC seek table", wav->wave.name);

            state->stream_offset = anchor_offset;
            state->block_index = anchor_block;

            // Walk from the sparse anchor to the preroll block without doing
            // coefficient reconstruction or transforms. This happens entirely
            // on the cpu.
            while (state->block_index < target_block) {
                waveform_ulc_load_block(wav, ext, state, input);
                int bits = ulc_skip_block(state, input);
                state->stream_offset += DIVIDE_CEIL(bits, 8);
                state->block_index++;
            }
        }

        clear_lap = true;
        preroll_blocks = ULC_PREROLL_BLOCKS;
        blocks_len += ULC_PREROLL_BLOCKS;
    }

    // Batch all the decode commands in a single highpri sequence. When the
    // mixer calls us from within its own highpri burst, this simply nests.
    rspq_highpri_begin();

    for (int i = 0; i < blocks_len; i++) {
        assertf(state->block_index < ext->blocks_len, "wav64: %s: ULC blocks exhausted: %lu/%lu", wav->wave.name, state->block_index, ext->blocks_len);

        waveform_ulc_load_block(wav, ext, state, input);
        int preroll_index = i < preroll_blocks ? i : -1;
        int bits = ulc_decode_block(state, dst, input, clear_lap, preroll_index);
        state->stream_offset += DIVIDE_CEIL(bits, 8);
        state->block_index++;

        clear_lap = false;
        if (preroll_index < 0) {
            dst += ULC_BLOCK_SIZE * wav->wave.channels;
        }
    }

    rspq_highpri_end();

    int valid = wav->wave.loop_len ? MIN(appended, wav->wave.len - wpos) : appended;
    if (appended > valid) {
        samplebuffer_undo(sbuf, appended - valid);
        rspq_highpri_sync();
    }
}

void wav64_ulc_init(wav64_t *wav, int state_size) {
    wav64_header_ulc_t *ext = wav->st->ext;
    assertf(ext->block_size == ULC_BLOCK_SIZE, "wav64: %s: unsupported ULC block size: %d", wav->wave.name, ext->block_size);
    assertf(ext->max_block_size > 0, "wav64: %s: invalid ULC maximum block size", wav->wave.name);
    assertf(ext->blocks_len >= 3, "wav64: %s: invalid ULC block count: %lu", wav->wave.name, ext->blocks_len);
    assertf(wav->wave.channels >= 1 && wav->wave.channels <= 2, "wav64: %s: unsupported ULC channel count: %d", wav->wave.name, wav->wave.channels);
    assertf(state_size >= wav64_ulc_state_size(wav->wave.channels), "wav64: %s: ULC state too small; regenerate your asset files", wav->wave.name);

    if (!ulc_rsp_overlay_id) {
        rspq_init();
        ulc_rsp_overlay_id = rspq_overlay_register(&rsp_ulc);
    }

    wav->wave.start = waveform_ulc_start;
    wav->wave.read = waveform_ulc_read;
    wav->wave.append_units = ULC_BLOCK_SIZE;
    wav->wave.rsp_written = true;
    wav->wave.loop_restart_only = true;
}

int wav64_ulc_get_bitrate(wav64_t *wav) {
    wav64_header_ulc_t *ext = wav->st->ext;
    return ext->bitrate_bps;
}

int wav64_ulc_adjust_seek(wav64_t *wav, int wpos) {
    wav64_header_ulc_t *ext = wav->st->ext;
    if (ext->seek_table_offset == 0) {
        return 0;
    }
    return wpos / ULC_BLOCK_SIZE * ULC_BLOCK_SIZE;
}
