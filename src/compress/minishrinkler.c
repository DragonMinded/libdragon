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
 * 
 * Since the only use case we know of on the N64 is to compress save files, the
 * focus has been on optimizing for that scenario. Save files tend to be small,
 * and I/O tends to be very very slow, so a good, slow compressor seems to hit
 * the sweet spot.
 */
#include "minishrinkler_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <stdarg.h>
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
#define NUM_SINGLE_CONTEXTS 513
#define NUM_CONTEXT_GROUPS 4
#define CONTEXT_GROUP_SIZE 256
#define NUM_CONTEXTS (NUM_SINGLE_CONTEXTS + NUM_CONTEXT_GROUPS * CONTEXT_GROUP_SIZE)

// Context indices
#define CONTEXT_KIND 0
#define CONTEXT_REPEATED -1
#define CONTEXT_GROUP_LIT 0
#define CONTEXT_GROUP_OFFSET 2
#define CONTEXT_GROUP_LENGTH 3

// Hash table configuration
#define HASH_SIZE 921  // 4608 bytes / 5 bytes per entry

// Moving window configuration (16-bit = 64KB window)
#define HASH_WINDOW_BITS 16
#define HASH_WINDOW_SIZE (1 << HASH_WINDOW_BITS)  // 65536
#define HASH_WINDOW_MASK (HASH_WINDOW_SIZE - 1)   // 0xFFFF

// Data structures - Embedded optimized
typedef struct {
    uint16_t contexts[NUM_CONTEXTS];
    uint8_t *output;
    uint32_t output_size;    
    uint32_t output_capacity;
    int32_t dest_bit;        
    uint32_t intervalsize;
    uint32_t intervalmin;
} shr_rangecoder_t;

typedef struct {
    bool after_first;     
    bool prev_was_ref;    
    int parity;                           
    uint16_t last_offset;
} shr_lzstate_t;

typedef struct {
    uint16_t pos;                         // 16-bit position (65535 max)
    uint16_t next;                        // 16-bit next pointer
    uint32_t match_len : 10;              // 10-bit cached match length (0-1023)
    uint32_t quality : 6;                 // 6-bit quality indicator (0-63)
    // Total: 48 bits = 6 bytes exactly!
} __attribute__((packed)) shr_hash_entry_t;

// Embedded memory optimization: no static allocations
// All memory will be allocated dynamically in a single buffer

const uint8_t size_table[128] = {
    0x40,0x3f,0x3f,0x3e,0x3d,0x3c,0x3c,0x3b,0x3a,0x3a,0x39,0x38,0x38,0x37,0x36,0x36,0x35,0x34,0x34,0x33,0x33,0x32,0x31,0x31,0x30,0x30,0x2f,0x2e,0x2e,0x2d,0x2d,0x2c,0x2b,0x2b,0x2a,0x2a,0x29,0x29,0x28,0x27,0x27,0x26,0x26,0x25,0x25,0x24,0x24,0x23,0x23,0x22,0x22,0x21,0x21,0x20,0x20,0x1f,0x1e,0x1e,0x1d,0x1d,0x1d,0x1c,0x1c,0x1b,0x1b,0x1a,0x1a,0x19,0x19,0x18,0x18,0x17,0x17,0x16,0x16,0x15,0x15,0x15,0x14,0x14,0x13,0x13,0x12,0x12,0x11,0x11,0x11,0x10,0x10,0xf,0xf,0xe,0xe,0xe,0xd,0xd,0xc,0xc,0xc,0xb,0xb,0xa,0xa,0x9,0x9,0x9,0x8,0x8,0x8,0x7,0x7,0x6,0x6,0x6,0x5,0x5,0x4,0x4,0x4,0x3,0x3,0x3,0x2,0x2,0x1,0x1,0x1,0x0
};

// Memory layout structure for embedded allocation
typedef struct {
    shr_rangecoder_t coder;
    shr_lzstate_t state;
    int hash_table_size;  // Number of hash table entries
    shr_hash_entry_t hash_table[];  // Variable size array at the end
} shr_work_buffer_t;

///@endcond

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
static unsigned int hash3(const unsigned char *data, int hash_size) {
    // Better distribution than simple bit shifting
    return ((data[0] * 31 + data[1]) * 31 + data[2]) % hash_size;
}

// Update hash table with new position and cache match information
static void update_hash(shr_work_buffer_t *mem, const unsigned char *data, int pos, int data_size) {
    if (pos + 2 >= data_size) return;
    
    unsigned int hash = hash3(&data[pos], mem->hash_table_size);
    
    // Calculate match quality based on data characteristics (6-bit: 0-63)
    uint8_t quality = 0;
    if (pos + 3 < data_size) {
        // Better quality heuristic: focus on compressible patterns
        if (data[pos] == data[pos + 1] && data[pos + 1] == data[pos + 2]) {
            quality = 63;  // Excellent quality for repeated data (RLE)
        } else if (data[pos] != data[pos + 1] && data[pos + 1] != data[pos + 2] && data[pos] != data[pos + 2]) {
            quality = 16;  // Lower quality for varied data (harder to compress)
        } else {
            quality = 32;  // Medium quality for mixed patterns
        }
    }
    
    // Update hash table with new entry (wrapping window)
    mem->hash_table[hash].next = mem->hash_table[hash].pos;
    // Store position with wrapping
    mem->hash_table[hash].pos = pos & HASH_WINDOW_MASK;
    mem->hash_table[hash].quality = quality;
    mem->hash_table[hash].match_len = 0; // Will be calculated during match finding
    
    tracef("UPDATE_HASH: pos=%d, hash=%d, stored_pos=%d (wrapped)\n",
        pos, hash, mem->hash_table[hash].pos);
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
    state->last_offset = 0;
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
           offset, length, parity, state->prev_was_ref, state->last_offset);
    tracef("KIND_REF: context=%d bit=1\n", 1 + CONTEXT_KIND + (parity << 8));
    
    range_coder_code(coder, 1 + CONTEXT_KIND + (parity << 8), 1); // KIND_REF
    size++;
    
    if (!state->prev_was_ref) {
        int repeated = offset == state->last_offset;
        tracef("REPEATED: context=%d bit=%d (offset %s last_offset)\n", 
               1 + CONTEXT_REPEATED, repeated, repeated ? "==" : "!=");
        range_coder_code(coder, 1 + CONTEXT_REPEATED, repeated);
        size++;
    }
    
    if (offset != state->last_offset) {
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
    state->last_offset = offset;
    assert(offset == state->last_offset && "reference offset too large");
    
    tracef("=== ENCODE_REFERENCE END ===\n");
    tracef("STATE UPDATE: after_first=%d prev_was_ref=%d parity=%d last_offset=%d\n", 
           state->after_first, state->prev_was_ref, state->parity, state->last_offset);
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
    int max_offset = min(pos, MAX_OFFSET);
    
    // Use hash table to find potential matches
    unsigned int hash = hash3(&data[pos], mem->hash_table_size);
    int candidate_pos = mem->hash_table[hash].pos;
    int matches_checked = 0;
    
    // Adaptive max_matches based on data size and position
    int max_matches = (data_size < 1024) ? 24 : 64; // More candidates for better compression
    
    // Track best quality match for early exit optimization
    int best_quality = 0;
    
    while (candidate_pos > 0 && matches_checked < max_matches) {
        // Wrapping window logic: reconstruct absolute position from wrapped position
        int absolute_candidate_pos = candidate_pos;
        
        // Handle wrapping: if candidate_pos is greater than current pos, it's from a previous window
        if (absolute_candidate_pos > pos) {
            // This position is from a previous window, skip it
            candidate_pos = mem->hash_table[hash].next;
            matches_checked++;
            continue;
        }
        
        // Skip if position is invalid (negative or too far back)
        if (absolute_candidate_pos < 0 || (pos - absolute_candidate_pos) > HASH_WINDOW_MASK) {
            candidate_pos = mem->hash_table[hash].next;
            matches_checked++;
            continue;
        }
        
        if ((pos - absolute_candidate_pos) > max_offset) break;
        
        // Use cached match length if available and recent
        int match_len = 0;
        if (mem->hash_table[hash].match_len > 0 && 
            candidate_pos == mem->hash_table[hash].pos) {
            // Use cached length as starting point
            match_len = mem->hash_table[hash].match_len;
        }
        
        // Validate that this is actually a match (not a wrapping false positive)
        // First check if at least MIN_MATCH_LENGTH bytes match
        int valid_match = 1;
        if (absolute_candidate_pos + MIN_MATCH_LENGTH > data_size) {
            valid_match = 0;  // Can't read enough bytes
            tracef("  WRAP_DEBUG: invalid match - can't read enough bytes\n");
        } else {
            for (int i = 0; i < MIN_MATCH_LENGTH; i++) {
                if (pos + i >= data_size || data[pos + i] != data[absolute_candidate_pos + i]) {
                    valid_match = 0;
                    tracef("  WRAP_DEBUG: invalid match - byte %d differs: pos[%d]=%02x vs candidate[%d]=%02x\n", 
                           i, pos + i, data[pos + i], absolute_candidate_pos + i, data[absolute_candidate_pos + i]);
                    break;
                }
            }
        }
        
        // Skip if this is a false positive from wrapping
        if (!valid_match) {
            candidate_pos = mem->hash_table[hash].next;
            matches_checked++;
            continue;
        }
        
        int offset = pos - absolute_candidate_pos;
        
        // Assert that absolute_candidate_pos is not greater than pos
        assert(pos >= absolute_candidate_pos);
        
        // Skip self-matches (offset 0)
        if (offset == 0) {
            candidate_pos = mem->hash_table[hash].next;
            matches_checked++;
            continue;
        }
        
        // Assert that we never have offset 0
        assert(offset > 0);
        
        // Check if we have a match (now we know it's valid)
        while (match_len < max_len && 
               pos + match_len < data_size && 
               absolute_candidate_pos + match_len < data_size &&
               data[pos + match_len] == data[absolute_candidate_pos + match_len]) {
            match_len++;
        }
        
        // Update cached match length (with overflow protection)
        if (candidate_pos == mem->hash_table[hash].pos) {
            mem->hash_table[hash].match_len = (match_len > 1023) ? 1023 : match_len;  // 10-bit max = 1023
        }
        
        // Calculate match quality score with encoding cost consideration (6-bit scale)
        int quality = mem->hash_table[hash].quality;
        if (match_len >= 8) quality += 16;   // Higher bonus for longer matches
        if (match_len >= 16) quality += 8;   // Extra bonus for very long matches
        if (offset <= 256) quality += 8;     // Higher bonus for closer matches
        if (offset <= 64) quality += 4;      // Extra bonus for very close matches
        
        // Consider encoding cost for better match selection
        int encoding_cost = 0;
        if (match_len >= 8) encoding_cost += 1;  // Extra bit for longer matches
        if (offset >= 256) encoding_cost += 1;   // Extra bit for larger offsets
        
        // Adjust quality based on encoding efficiency
        quality -= encoding_cost * 4;   // Penalize expensive encodings
        
        // Update if we found a better match (exclude offset 0)
        if (match_len >= MIN_MATCH_LENGTH && offset > 0) {
            // Enhanced match selection with pattern recognition
            int is_better = 0;
            
            // Primary criteria: length
            if (match_len > *best_length) {
                is_better = 1;
            } else if (match_len == *best_length) {
                // Secondary criteria: quality score
                if (quality > best_quality) {
                    is_better = 1;
                } else if (quality == best_quality) {
                    // Tertiary criteria: offset (prefer closer)
                    if (offset < *best_offset) {
                        is_better = 1;
                    }
                }
            }
            
            if (is_better) {
                *best_length = match_len;
                *best_offset = offset;
                best_quality = quality;
                
                // Early exit for excellent matches
                if (match_len >= 16 && quality >= 50) break;
            }
        }
        
        candidate_pos = mem->hash_table[hash].next;
        matches_checked++;
    }
    
    return *best_length >= MIN_MATCH_LENGTH;
}

int minishrinkler_compress(const uint8_t *input, int input_size, 
                           uint8_t *output, int output_capacity,
                           int work_memory_size)
{
    int pos = 0;
    
    // Calculate hash table size from work memory size
    size_t hash_table_entries = work_memory_size / sizeof(shr_hash_entry_t);
    size_t total_size = sizeof(shr_work_buffer_t) + hash_table_entries * sizeof(shr_hash_entry_t);
    
    // Allocate embedded memory with variable hash table size
    shr_work_buffer_t *mem = malloc(total_size);
    if (!mem) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return -4; // Memory allocation failed
    }
    
    // Initialize memory to zero
    memset(mem, 0, total_size);
    mem->hash_table_size = (int)hash_table_entries;
    
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
    free(mem);
    
    return output_size;
}

size_t minishrinkler_get_max_compressed_size(size_t input_size) {
    // Worst case: no compression + range coder overhead
    // Each byte might need up to 9 bits in worst case
    return (input_size * 9 + 7) / 8 + 64; // Add some safety margin
}
