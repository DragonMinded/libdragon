/**
 * @file minishrinkler.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Minishrinkler compression API implementation
 * 
 * This file contains the core compression logic for Minishrinkler,
 * providing a buffer-to-buffer compression API without any file I/O.
 *
 * This implements the Shrinkler bitstream with a very simplified compression
 * algorithm. Since the LZ coder is very basic, the compression ratio is good
 * only on small files up to a few KiB (where the range coder itself beats
 * standard LZ+Huffman constructs). Once the file size grows beyond that,
 * the lack of advanced LZ techniques makes the compression ratio degrade
 * quickly, and for large files the compression ratio is much worse than
 * standard algorithms like DEFLATE.
 */
#include "minishrinkler_internal.h"
#include "scratch.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#if TRACE_SHRINKLER
#include <stdarg.h>
#endif
#include <stdbool.h>

///@cond

// Compile-time trace control
#define TRACE_SHRINKLER 0

#if TRACE_SHRINKLER
static void tracef(const char *fmt, ...) {
    static FILE *trace_file = NULL;
    if (!trace_file) {
        trace_file = fopen("minishrinkler.log", "w");
    }
    if (trace_file) {
        va_list args;
        va_start(args, fmt);
        vfprintf(trace_file, fmt, args);
        va_end(args);
    }
}
#else
#define tracef(fmt, ...) ((void)0)
#endif

// Configuration - Embedded optimized
#define MAX_FILE_SIZE (1024*1024)  // 1MB max for embedded
#define MAX_MATCH_LENGTH 65535
#define MAX_OFFSET 65535
#define MIN_MATCH_LENGTH 3
#define WINDOW_SIZE 65536

// Context configuration (must match decompressor)
#define ADJUST_SHIFT 4
#define NUM_SINGLE_CONTEXTS 1
#define NUM_CONTEXT_GROUPS 4
#define CONTEXT_GROUP_SIZE 256
#define NUM_CONTEXTS (NUM_SINGLE_CONTEXTS + NUM_CONTEXT_GROUPS * CONTEXT_GROUP_SIZE)

// Context indices
#define CONTEXT_KIND 0
#define CONTEXT_REPEATED -1
#define CONTEXT_GROUP_LIT 0
#define CONTEXT_GROUP_OFFSET 2
#define CONTEXT_GROUP_LENGTH 3

///@endcond

// Moving window: computed dynamically from work memory; keep max at 64K
// (we will compute window_size and mask at runtime based on available memory)

/** @brief Range coder context */
typedef struct {
    uint16_t contexts[NUM_CONTEXTS];            ///< Context modeling
    uint8_t *output;                            ///< Output buffer
    uint32_t output_size;                       ///< Output buffer size
    uint32_t output_capacity;                   ///< Output buffer capacity
    int32_t dest_bit;                           ///< Destination bit
    uint32_t intervalsize;                      ///< Interval size
    uint32_t intervalmin;                       ///< Interval minimum
    int lit_avg_events;                         ///< EWMA of literal event cost (init ~9)
} shr_rangecoder_t;

/** @brief LZ coder state */
typedef struct {
    bool after_first;                          ///< First symbol was done?
    bool prev_was_ref;                         ///< Previous symbols was a match?
    int parity;                                ///< Parity bit
    uint16_t last_offsets[3];                  ///< Recent offsets LRU: [0]=last, [1]=prev, [2]=older
} shr_lzstate_t;

/** @brief Size table for encoding numbers */
static const uint8_t size_table[128] = {
    0x40,0x3f,0x3f,0x3e,0x3d,0x3c,0x3c,0x3b,0x3a,0x3a,0x39,0x38,0x38,0x37,0x36,0x36,0x35,0x34,0x34,0x33,0x33,0x32,0x31,0x31,0x30,0x30,0x2f,0x2e,0x2e,0x2d,0x2d,0x2c,0x2b,0x2b,0x2a,0x2a,0x29,0x29,0x28,0x27,0x27,0x26,0x26,0x25,0x25,0x24,0x24,0x23,0x23,0x22,0x22,0x21,0x21,0x20,0x20,0x1f,0x1e,0x1e,0x1d,0x1d,0x1d,0x1c,0x1c,0x1b,0x1b,0x1a,0x1a,0x19,0x19,0x18,0x18,0x17,0x17,0x16,0x16,0x15,0x15,0x15,0x14,0x14,0x13,0x13,0x12,0x12,0x11,0x11,0x11,0x10,0x10,0xf,0xf,0xe,0xe,0xe,0xd,0xd,0xc,0xc,0xc,0xb,0xb,0xa,0xa,0x9,0x9,0x9,0x8,0x8,0x8,0x7,0x7,0x6,0x6,0x6,0x5,0x5,0x4,0x4,0x4,0x3,0x3,0x3,0x2,0x2,0x1,0x1,0x1,0x0
};

/** 
 * @brief Layout structure for working memory bufferr
 *
 * All the work memory used by the compressor is described by this structure.
 * No dynamic allocations beyond the single arena.
 *
 * The match finder uses a compact set-associative hash table carved from
 * the provided work memory. Each bucket keeps a small number (ways) of wrapped
 * 16-bit positions.
 */
typedef struct {
    shr_rangecoder_t coder;        ///< Range coder context
    shr_lzstate_t state;           ///< LZ coder state

    // Hash table configuration and sliding window
    int hash_size;                ///< Number of buckets (power of two)
    int hash_mask;                ///< hash_size - 1
    int ways;                     ///< Associativity (entries per bucket)
    int window_bits;              ///< such that window_size == (1 << window_bits)
    int window_size;              ///< sliding window size (<= 65536)
    int window_mask;              ///< window_size - 1

    // Pointers into the single malloc arena (immediately after this struct)
    uint16_t *buckets;            ///< size = hash_size * ways; each is wrapped pos (0..mask) or 0xFFFF if empty
    uint8_t  *repl_index;         ///< size = hash_size; round-robin replacement index per bucket

    // Rolling estimate of literal event cost (EWMA of encode_literal sizes)
    int lit_avg_events;           ///< average range-coder events per literal (init ~9)
} shr_work_buffer_t;

// Utility functions
static int min(int a, int b) { return a < b ? a : b; }

#if 0
// Initialize size table
static void init_size_table(shr_work_buffer_t *mem) {
    for (int i = 0; i < 128; i++) {
        mem->size_table[i] = (int) floor(0.5 + (8.0 - log((double) (128 + i)) / log(2.0)) * (1 << 6));
    }
}
#endif

// Improved hash function for 3-byte sequences
static unsigned int hash3(const unsigned char *data) {
    // Mix into 32-bit and rely on power-of-two mask for index
    unsigned int v = (unsigned int)data[0] | ((unsigned int)data[1] << 8) | ((unsigned int)data[2] << 16);
    v *= 0x9E3779B1u; // golden ratio multiplier
    v ^= v >> 16;
    return v;
}

// Approximate cost of encode_number as used by the coder: number >= 2
static int estimate_number_cost_int(int number) {
    int i = 0;
    while ((4 << i) <= number) i++;
    // continuation bits (i), stop bit (1), payload bits (i+1)
    return i + 1 + (i + 1);
}

// Update hash table with new position (set-associative, round-robin replacement)
static void update_hash(shr_work_buffer_t *mem, const unsigned char *data, int pos, int data_size) {
    if (pos + 2 >= data_size) return;
    
    unsigned int hash = hash3(&data[pos]) & (unsigned int)mem->hash_mask;

    // Round-robin replacement within the bucket
    int way = mem->repl_index[hash] % mem->ways;
    mem->repl_index[hash] = (uint8_t)((mem->repl_index[hash] + 1) % mem->ways);

    // Store wrapped position
    uint16_t wrapped = (uint16_t)(pos & mem->window_mask);
    mem->buckets[hash * mem->ways + way] = wrapped;

    tracef("UPDATE_HASH: pos=%d, hash=%d, way=%d stored_pos=%u (wrapped, mask=0x%04x)\n",
        pos, hash, way, wrapped, (unsigned)mem->window_mask);
}

// Exact copy of original RangeCoder logic (adapted for static allocation)
static void range_coder_init(shr_rangecoder_t *coder, uint8_t *output, int capacity) {
    coder->output = output;
    coder->output_capacity = capacity;
    coder->output_size = 0;
    coder->dest_bit = -1; // Start at -1 like original
    coder->intervalsize = 0x8000;
    coder->intervalmin = 0;
    
    // Initialize contexts (must match decompressor)
    for (int i = 0; i < NUM_CONTEXTS; i++) {
        coder->contexts[i] = 0x8000;
    }
    
    // Ensure first byte is initialized
    output[0] = 0;
    coder->lit_avg_events = 9;
}

// Forward declaration for tracing (only when tracing is enabled)
#if TRACE_SHRINKLER
static void range_coder_trace_state(shr_rangecoder_t *coder, const char *operation, int context, int bit, int size);
#endif

// Exact copy of original add_bit function (adapted for static allocation)
static void add_bit(shr_rangecoder_t *coder) {
    int pos = coder->dest_bit;
    int bytepos;
    int bitmask;
    
    // tracef("    ADD_BIT: start pos=%d\n", pos);
    do {
        pos--;
        if (pos < 0) return;
        bytepos = pos >> 3;
        bitmask = 0x80 >> (pos & 7);
        
        // Initialize byte to 0 if not already done
        while (bytepos >= (int)coder->output_size) {
            coder->output[coder->output_size++] = 0;
        }
        
        assert(bytepos < (int)coder->output_capacity);
        coder->output[bytepos] ^= bitmask;
        
        // tracef("    ADD_BIT: pos=%d bytepos=%d bitmask=0x%02x old_byte=0x%02x new_byte=0x%02x\n", 
        //        pos, bytepos, bitmask, old_byte, new_byte);
    } while ((coder->output[bytepos] & bitmask) == 0);
}

// Exact copy of original rangecoder_code function
static int range_coder_code(shr_rangecoder_t *coder, int context_index, int bit) {
    // Handle special context indices
    if (context_index < 0) {
        // Special contexts like CONTEXT_REPEATED (-1) are handled differently
        // For now, just return 0 size for these
        return 0;
    }
    
    // Calculate size_before, handling the case where dest_bit is -1 (initial state)
    int size_before;
    if (coder->dest_bit < 0) {
        // Initial state: dest_bit is -1, so size is just the sizetable entry
        size_before = size_table[(coder->intervalsize - 0x8000) >> 8];
    } else {
        size_before = (coder->dest_bit << 6) + size_table[(coder->intervalsize - 0x8000) >> 8];  // BIT_PRECISION = 6
    }
    
    // Trace the input state
    // range_coder_trace_state(coder, "CODE_START", context_index, bit, size_before);
    
    unsigned prob = coder->contexts[context_index];
    unsigned threshold = (coder->intervalsize * prob) >> 16;
    unsigned new_prob;
    
    if (!bit) {
        // Zero
        coder->intervalmin += threshold;
        if (coder->intervalmin & 0x10000) {
            add_bit(coder);
        }
        coder->intervalsize = coder->intervalsize - threshold;
        new_prob = prob - (prob >> ADJUST_SHIFT);
    } else {
        // One
        coder->intervalsize = threshold;
        new_prob = prob + (0xffff >> ADJUST_SHIFT) - (prob >> ADJUST_SHIFT);
    }
    
    coder->contexts[context_index] = new_prob;
    
    // Renormalization
    int renormalizations = 0;
    while (coder->intervalsize < 0x8000) {
        coder->dest_bit++;
        coder->intervalsize <<= 1;
        coder->intervalmin <<= 1;
        if (coder->intervalmin & 0x10000) {
            tracef("    RENORM: carry bit detected, calling add_bit\n");
            add_bit(coder);
        }
        renormalizations++;
    }
    coder->intervalmin &= 0xffff;
    
    if (renormalizations > 0) {
        tracef("    RENORM: %d renormalizations, final intervalmin=0x%04x intervalsize=0x%04x dest_bit=%d\n", 
               renormalizations, coder->intervalmin, coder->intervalsize, coder->dest_bit);
    }
    
    // Calculate size_after, handling the case where dest_bit is -1 (initial state)
    int size_after;
    if (coder->dest_bit < 0) {
        // Initial state: dest_bit is -1, so size is just the sizetable entry
        size_after = size_table[(coder->intervalsize - 0x8000) >> 8];
    } else {
        size_after = (coder->dest_bit << 6) + size_table[(coder->intervalsize - 0x8000) >> 8];  // BIT_PRECISION = 6
    }
    int size_diff = size_after - size_before;
    
    // Trace the output state
    // range_coder_trace_state(coder, "CODE_END", context_index, bit, size_diff);
    
    return size_diff;
}

#if TRACE_SHRINKLER
static void range_coder_trace_state(shr_rangecoder_t *coder, const char *operation, int context, int bit, int size) {
    (void)coder;      // Suppress unused parameter warning
    (void)operation;  // Suppress unused parameter warning
    (void)context;    // Suppress unused parameter warning
    (void)bit;        // Suppress unused parameter warning
    (void)size;       // Suppress unused parameter warning
    // tracef("RANGECODER: %s context=%d bit=%d size=%d intervalmin=0x%04x intervalsize=0x%04x dest_bit=%d\n",
    //        operation, context, bit, size, coder->intervalmin, coder->intervalsize, coder->dest_bit);
}
#endif

// Exact copy of original rangecoder_finish function
static void range_coder_finish(shr_rangecoder_t *coder) {
    // Trace the finish start
    // tracef("RANGECODER: FINISH_START intervalmin=0x%04x intervalsize=0x%04x dest_bit=%d\n",
    //        coder->intervalmin, coder->intervalsize, coder->dest_bit);
    
    int intervalmax = coder->intervalmin + coder->intervalsize;
    int final_min = 0;
    int final_size = 0x10000;
    
    // tracef("RANGECODER: FINISH_DETAILED_START intervalmin=0x%04x intervalsize=0x%04x dest_bit=%d intervalmax=0x%04x\n",
    //        coder->intervalmin, coder->intervalsize, coder->dest_bit, intervalmax);
    
    while (final_min < (int)coder->intervalmin || final_min + final_size >= intervalmax) {
        if (final_min + final_size < intervalmax) {
            // tracef("RANGECODER: FINISH_ITERATION %d: final_min=0x%04x final_size=0x%04x < intervalmax=0x%04x -> add_bit\n",
            //        finish_iterations, final_min, final_size, intervalmax);
            add_bit(coder);
            final_min += final_size;
        } else {
            // tracef("RANGECODER: FINISH_ITERATION %d: final_min=0x%04x final_size=0x%04x >= intervalmax=0x%04x -> no add_bit\n",
            //        finish_iterations, final_min, final_size, intervalmax);
        }
        coder->dest_bit++;
        final_size >>= 1;
    }
    
    int required_bytes = ((coder->dest_bit - 1) >> 3) + 1;
    while (required_bytes > (int)coder->output_size) {
        coder->output[coder->output_size++] = 0;
    }
    
    // Trace the finish end
    // tracef("RANGECODER: FINISH_END final dest_bit=%d out_size=%d required_bytes=%d\n",
    //        coder->dest_bit, coder->output_size, required_bytes);
}

// LZ Encoder
static void lz_state_init(shr_lzstate_t *state) {
    state->after_first = 0;
    state->prev_was_ref = 0;
    state->parity = 0;
    state->last_offsets[0] = 0;
    state->last_offsets[1] = 0;
    state->last_offsets[2] = 0;
}

static int encode_literal(shr_rangecoder_t *coder, unsigned char value, shr_lzstate_t *state) {
    int parity = state->parity & 1;
    int size = 0;
    
    // Trace literal encoding
    tracef("LZ: ENCODE_LITERAL value=0x%02x (%c) parity=%d after_first=%d\n", 
           value, (value >= 32 && value <= 126) ? value : '.', parity, state->after_first);
    
    if (state->after_first) {
        range_coder_code(coder, 1 + CONTEXT_KIND + (parity << 8), 0); // KIND_LIT
        size++;
    }
    
    int context = 1;
    for (int i = 7; i >= 0; i--) {
        int bit = ((value >> i) & 1);
        int actual_context = 1 + ((parity << 8) | context);  // Correct context calculation
        range_coder_code(coder, actual_context, bit);
        size++;
        context = (context << 1) | bit;
    }
    
    // Update state
    state->after_first = 1;
    state->prev_was_ref = 0;
    state->parity = state->parity + 1;
    // Update EWMA of literal events (alpha=1/8)
    // Note: size is in the same "event" units used by the cost model
    coder->lit_avg_events = (coder->lit_avg_events * 7 + size) >> 3;
    
    return size;
}

// Exact copy of coder_encode_number from cruncher_c/Coder.c (adapted for RangeCoder)
static int encode_number(shr_rangecoder_t *coder, int context_group, int number) {
    int base_context = 1 + (context_group << 8);
    
    // Trace number encoding start
    tracef("ENCODE_NUMBER: group=%d number=%d base_context=%d\n", 
           context_group, number, base_context);
    
    // Must be >= 2 like original
    if (number < 2) {
        tracef("ERROR: number < 2 (%d)\n", number);
        return 0;
    }
    
    int size = 0;
    int context;
    int i;
    
    // First loop: continuation bits (exact copy)
    for (i = 0 ; (4 << i) <= number ; i++) {
        context = base_context + (i * 2 + 2);
        tracef("  CONTINUE_BIT: i=%d context=%d bit=1 (4<<i=%d <= %d)\n", 
               i, context, (4 << i), number);
        size += range_coder_code(coder, context, 1);
    }
    
    // Stop bit (exact copy)
    context = base_context + (i * 2 + 2);
    tracef("  STOP_BIT: i=%d context=%d bit=0 (4<<i=%d > %d)\n", 
           i, context, (4 << i), number);
    size += range_coder_code(coder, context, 0);
    
    // Second loop: actual bits (exact copy)
    for (; i >= 0 ; i--) {
        int bit = ((number >> i) & 1);
        context = base_context + (i * 2 + 1);
        tracef("  NUMBER_BIT: i=%d context=%d bit=%d (number>>%d&1)\n", i, context, bit, i);
        size += range_coder_code(coder, context, bit);
    }
    
    tracef("ENCODE_NUMBER: COMPLETE size=%d\n", size);
    
    return size;
}

static int encode_reference(shr_rangecoder_t *coder, int offset, int length, shr_lzstate_t *state) {
    int parity = state->parity & 1;
    int size = 0;
    
    // Trace reference encoding
    tracef("=== ENCODE_REFERENCE START ===\n");
    tracef("LZ: ENCODE_REFERENCE offset=%d length=%d parity=%d prev_was_ref=%d last_offset=%d\n", 
           offset, length, parity, state->prev_was_ref, state->last_offsets[0]);
    tracef("KIND_REF: context=%d bit=1\n", 1 + CONTEXT_KIND + (parity << 8));
    
    range_coder_code(coder, 1 + CONTEXT_KIND + (parity << 8), 1); // KIND_REF
    size++;
    
    if (!state->prev_was_ref) {
        int repeated = offset == state->last_offsets[0];
        tracef("REPEATED: context=%d bit=%d (offset %s last_offset)\n", 
               1 + CONTEXT_REPEATED, repeated, repeated ? "==" : "!=");
        range_coder_code(coder, 1 + CONTEXT_REPEATED, repeated);
        size++;
    }
    
    if (offset != state->last_offsets[0]) {
        tracef("ENCODE_OFFSET: offset=%d (encoded as %d)\n", offset, offset + 2);
        size += encode_number(coder, CONTEXT_GROUP_OFFSET, offset + 2);
    } else {
        tracef("SKIP_OFFSET: offset=%d same as last_offset\n", offset);
    }
    
    tracef("ENCODE_LENGTH: length=%d\n", length);
    size += encode_number(coder, CONTEXT_GROUP_LENGTH, length);
    
    // Update state
    state->after_first = 1;
    state->prev_was_ref = 1;
    state->parity = state->parity + length;
    // Update LRU offsets: move offset to front
    if (offset != state->last_offsets[0]) {
        // remove offset if present in [1] or [2]
        if (offset == state->last_offsets[1]) {
            state->last_offsets[1] = state->last_offsets[0];
            state->last_offsets[0] = (uint16_t)offset;
        } else {
            state->last_offsets[2] = state->last_offsets[1];
            state->last_offsets[1] = state->last_offsets[0];
            state->last_offsets[0] = (uint16_t)offset;
        }
    }
    assert(offset == state->last_offsets[0] && "reference offset too large");
    
    tracef("=== ENCODE_REFERENCE END ===\n");
    tracef("STATE UPDATE: after_first=%d prev_was_ref=%d parity=%d last_offset=%d\n", 
           state->after_first, state->prev_was_ref, state->parity, state->last_offsets[0]);
    tracef("TOTAL SIZE: %d\n\n", size);
    
    return size;
}

// Intelligent Match Finder using enhanced hash table
static int find_match(shr_work_buffer_t *mem, const unsigned char *data, int data_size, int pos, 
                     int *best_offset, int *best_length) {
    *best_length = 0;
    *best_offset = 0;
    
    if (pos + 2 >= data_size) return 0;
    
    int max_len = min(MAX_MATCH_LENGTH, data_size - pos);
    int window_limit = mem->window_size - 1;
    int max_offset = min(pos, min(MAX_OFFSET, window_limit));
    
    // Probe recent offsets (cheap LRU of 2)
    for (int pi = 1; pi <= 2; ++pi) {
        int poff = mem->state.last_offsets[pi];
        if (poff <= 0 || poff > max_offset || poff == mem->state.last_offsets[0]) continue;
        int absolute_candidate_pos = pos - poff;
        if (absolute_candidate_pos < 0) continue;
        // Quick 2-byte check
        if (absolute_candidate_pos + 2 <= data_size && pos + 2 <= data_size) {
            if (data[pos] != data[absolute_candidate_pos] || data[pos+1] != data[absolute_candidate_pos+1]) continue;
        }
        int match_len = 0;
        while (match_len < max_len &&
               pos + match_len < data_size &&
               absolute_candidate_pos + match_len < data_size &&
               data[pos + match_len] == data[absolute_candidate_pos + match_len]) {
            match_len++;
        }
        if (match_len >= MIN_MATCH_LENGTH) {
            int repeated_needed = mem->state.prev_was_ref ? 0 : 1;
            int offset_changed = (poff != mem->state.last_offsets[0]);
            int base_offset_penalty = estimate_number_cost_int(poff + 2) >> 2;
            int ref_cost = 1 + repeated_needed + base_offset_penalty;
            if (offset_changed) ref_cost += estimate_number_cost_int(poff + 2);
            ref_cost += estimate_number_cost_int(match_len);
            int lit_cost = mem->coder.lit_avg_events ? mem->coder.lit_avg_events : 9;
            int net_cost = ref_cost - match_len * lit_cost;
            if (*best_length == 0 || net_cost < 0 || (net_cost == 0 && match_len > *best_length)) {
                *best_length = match_len;
                *best_offset = poff;
            }
        }
    }

    // Use hash table to find potential matches
    unsigned int hash = hash3(&data[pos]) & (unsigned int)mem->hash_mask;

    int best_quality = 0;

    // Iterate all ways in the bucket in MRU order: last inserted first
    int start = mem->repl_index[hash];
    for (int k = 0; k < mem->ways; k++) {
        int w = (start - 1 - k);
        if (w < 0) w += mem->ways * ((-w) / mem->ways + 1);
        w %= mem->ways;
        uint16_t wrapped_pos = mem->buckets[hash * mem->ways + w];
        if (wrapped_pos == 0xFFFF) continue; // empty slot

        // Reconstruct absolute candidate position within the current window span
        int absolute_candidate_pos = (pos & ~mem->window_mask) | wrapped_pos;
        if (absolute_candidate_pos > pos) absolute_candidate_pos -= mem->window_size;

        int offset = pos - absolute_candidate_pos;
        if (offset <= 0 || offset > max_offset) continue;

        // Quick pre-check for minimum length
        if (absolute_candidate_pos + MIN_MATCH_LENGTH > data_size) continue;
        int valid = 1;
        for (int i = 0; i < MIN_MATCH_LENGTH; i++) {
            if (pos + i >= data_size || data[pos + i] != data[absolute_candidate_pos + i]) {
                valid = 0;
                break;
            }
        }
        if (!valid) continue;

        // Extend match
        int match_len = MIN_MATCH_LENGTH;
        while (match_len < max_len &&
               pos + match_len < data_size &&
               absolute_candidate_pos + match_len < data_size &&
               data[pos + match_len] == data[absolute_candidate_pos + match_len]) {
            match_len++;
        }

        // Dynamic minimal length requirement for far matches
        int min_len_req = MIN_MATCH_LENGTH;
        if (offset > 1024) min_len_req++;
        if (offset > 4096) min_len_req += 2;

        if (match_len >= min_len_req) {
            // Estimate reference cost (same units as before)
            int repeated_needed = mem->state.prev_was_ref ? 0 : 1;
            int offset_changed = (offset != mem->state.last_offsets[0]);
            int base_offset_penalty = estimate_number_cost_int(offset + 2) >> 2; // ~25%
            int ref_cost = 1; // KIND_REF
            ref_cost += repeated_needed;
            ref_cost += base_offset_penalty;
            if (offset_changed) ref_cost += estimate_number_cost_int(offset + 2);
            ref_cost += estimate_number_cost_int(match_len);

            // Credit saved literal cost using EWMA of literal size
            int lit_cost = mem->coder.lit_avg_events ? mem->coder.lit_avg_events : 9;
            int saved = match_len * lit_cost;
            int net_cost = ref_cost - saved;

            int is_better = 0;
            if (*best_length == 0) {
                is_better = 1;
            } else {
                // Prefer lower net cost; break ties with longer length, then closer offset
                if (best_quality == 0 || net_cost < best_quality) {
                    is_better = 1;
                } else if (net_cost == best_quality) {
                    if (match_len > *best_length) {
                        is_better = 1;
                    } else if (match_len == *best_length && offset < *best_offset) {
                        is_better = 1;
                    }
                }
            }
            if (is_better) {
                *best_length = match_len;
                *best_offset = offset;
                best_quality = net_cost; // store best net cost here
            }
        }
    }
    
    return *best_length >= MIN_MATCH_LENGTH;
}

// Main compression function
static int compress_data(const unsigned char *input, int input_size, 
                        unsigned char *output, int output_capacity,
                        size_t work_memory_size) {
    int pos = 0;
    
    // Derive match-finder sizes from provided work memory (single malloc arena)
    // We use a set-associative table with "ways" entries per bucket and
    // a round-robin replacement index per bucket (1 byte).
    int ways = (work_memory_size >= 4096) ? 4 : 2; // increase associativity when memory allows
    if (work_memory_size <= sizeof(shr_work_buffer_t)) {
        return -4; // Not enough memory even for control structure
    }

    size_t available = work_memory_size - sizeof(shr_work_buffer_t);

    // Compute the maximum number of buckets we can afford in the available arena:
    // each bucket costs (ways * 2 bytes) + 1 byte for replacement index
    size_t cost_per_bucket = (size_t)(ways * 2 + 1);
    size_t max_buckets = available / cost_per_bucket;
    if (max_buckets == 0) {
        return -4; // Not enough memory for even a single bucket
    }

    // Choose hash_size as largest power-of-two <= max_buckets for fast masking
    int hash_size = 1;
    while (((size_t)hash_size << 1) <= max_buckets) hash_size <<= 1;

    // Determine window size based on total storable entries (hash_size * ways)
    // Pick a power-of-two window_size <= min(65536, 2 * hash_size * ways) with a minimum of 256
    size_t target_entries = (size_t)hash_size * (size_t)ways;
    size_t target_window = target_entries * 2; // tighter coupling with actual indexable entries
    if (target_window < 256) target_window = 256;
    if (target_window > 65536) target_window = 65536;
    // Round down to power of two
    size_t window_size = 1;
    while ((window_size << 1) <= target_window) window_size <<= 1;
    int window_bits = 0;
    size_t tmp_ws = window_size;
    while ((tmp_ws >>= 1) != 0) window_bits++;
    int window_mask = (int)(window_size - 1);

    // Compute actual arena size and allocate
    size_t buckets_bytes = (size_t)hash_size * (size_t)ways * sizeof(uint16_t);
    size_t repl_bytes = (size_t)hash_size * sizeof(uint8_t);

    // Guard in case rounding left us slightly over the available arena; adjust down
    if (buckets_bytes + repl_bytes > available) {
        size_t usable_buckets = available / cost_per_bucket;
        if (usable_buckets == 0) return -4;
        // round down to power of two
        hash_size = 1;
        while (((size_t)hash_size << 1) <= usable_buckets) hash_size <<= 1;
        buckets_bytes = (size_t)hash_size * (size_t)ways * sizeof(uint16_t);
        repl_bytes = (size_t)hash_size * sizeof(uint8_t);
    }

    size_t total_size = work_memory_size; // allocate exactly what caller provides

    shr_work_buffer_t *mem = scratch_malloc(total_size);
    if (!mem) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return -4; // Memory allocation failed
    }
    // Zero the entire arena for deterministic behavior
    memset(mem, 0, work_memory_size);
    uint8_t *arena = (uint8_t *)(mem + 1);
    mem->buckets = (uint16_t *)arena;
    mem->repl_index = (uint8_t *)(arena + buckets_bytes);

    // Initialize match finder configuration
    mem->hash_size = hash_size;
    mem->ways = ways;
    mem->hash_mask = hash_size - 1;
    mem->window_bits = window_bits;
    mem->window_size = (int)window_size;
    mem->window_mask = window_mask;

    // Initialize tables: 0xFFFF means empty slot
    for (int i = 0; i < mem->hash_size * mem->ways; i++) mem->buckets[i] = 0xFFFF;
    memset(mem->repl_index, 0, (size_t)mem->hash_size);
    
    // Initialize
    range_coder_init(&mem->coder, output, output_capacity);
    lz_state_init(&mem->state);
    
    // Enable tracing
    tracef("=== MINISHRINKLER TRACE ===\n");
    tracef("Input size: %d bytes\n", input_size);
    tracef("Input data: ");
    for (int i = 0; i < min(input_size, 32); i++) {
        tracef("%02x ", input[i]);
    }
    if (input_size > 32) tracef("...");
    tracef("\n\n");
    
    // Compression with lazy matching
    while (pos < input_size) {
        // Update hash table
        update_hash(mem, input, pos, input_size);
        
        int best_offset, best_length;
        
        if (find_match(mem, input, input_size, pos, &best_offset, &best_length)) {
            // Lazy matching: look ahead for better matches
            if (best_length >= 4 && pos + 1 < input_size) {
                int next_offset, next_length;
                
                // Update hash for next position
                update_hash(mem, input, pos + 1, input_size);
                
                if (find_match(mem, input, input_size, pos + 1, &next_offset, &next_length)) {
                    // Compare current match vs next position match
                    int current_cost = 2 + (best_length >= 8 ? 1 : 0) + (best_offset >= 256 ? 1 : 0);
                    int next_cost = 2 + (next_length >= 8 ? 1 : 0) + (next_offset >= 256 ? 1 : 0);
                    
                    // If next match is significantly better, encode literal and continue
                    if (next_length > best_length + 1 || 
                        (next_length == best_length + 1 && next_cost <= current_cost)) {
                        tracef("POS %d: LAZY LITERAL 0x%02x (waiting for better match)\n", pos, input[pos]);
                        encode_literal(&mem->coder, input[pos], &mem->state);
                        pos++;
                        continue;
                    }
                }
            }
            
            // Encode reference
            tracef("POS %d: MATCH offset=%d length=%d\n", pos, best_offset, best_length);
            encode_reference(&mem->coder, best_offset, best_length, &mem->state);
            pos += best_length;
        } else {
            // Encode literal
            tracef("POS %d: LITERAL 0x%02x (%c)\n", pos, input[pos], 
                   (input[pos] >= 32 && input[pos] <= 126) ? input[pos] : '.');
            encode_literal(&mem->coder, input[pos], &mem->state);
            pos++;
        }
    }

    // Encode end marker (offset 0)
    tracef("END: ENCODE_END_MARKER\n");
    int parity = mem->state.parity & 1;
    range_coder_code(&mem->coder, 1 + CONTEXT_KIND + (parity << 8), 1); // KIND_REF
    range_coder_code(&mem->coder, 1 + CONTEXT_REPEATED, 0); // Not repeated
    encode_number(&mem->coder, CONTEXT_GROUP_OFFSET, 2); // Offset 0 + 2 = 2 = end

    // Finalize
    range_coder_finish(&mem->coder);

    tracef("=== COMPRESSION COMPLETE ===\n");
    tracef("Final output size: %d bytes\n", mem->coder.output_size);
    
    // Save output size before freeing memory
    int output_size = mem->coder.output_size;
    
    // Free embedded memory
    scratch_free(mem);
    
    return output_size;
}

/**
 * @brief Get the maximum compressed size for given input size
 */
size_t minishrinkler_get_max_compressed_size(size_t input_size) {
    // Worst case: no compression + range coder overhead
    // Each byte might need up to 9 bits in worst case
    return (input_size * 9 + 7) / 8 + 64; // Add some safety margin
}

/**
 * @brief Compress data from input buffer to output buffer
 */
int minishrinkler_compress(
    const uint8_t *input_data,
    size_t input_size,
    uint8_t *output_buffer,
    size_t output_capacity,
    size_t work_memory_size
) {
    // Validate input parameters
    if (!input_data || !output_buffer || input_size == 0 || output_capacity == 0) {
        return -2; // Invalid parameters
    }
    
    // Check if output buffer is large enough
    size_t max_compressed_size = minishrinkler_get_max_compressed_size(input_size);
    if (output_capacity < max_compressed_size) {
        return -1; // Output buffer too small
    }
    
    // Check input size limit
    if (input_size > MAX_FILE_SIZE) {
        return -3; // Input too large
    }
    
    // Call the original compression function
    int result = compress_data((const unsigned char*)input_data, (int)input_size, 
                              (unsigned char*)output_buffer, (int)output_capacity,
                              work_memory_size);
    
    return result;
}
