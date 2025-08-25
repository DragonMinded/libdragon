/**
 * @file minishrinkler_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Minishrinkler compression API - Simplified Shrinkler compressor
 * 
 * This header provides a simple buffer-to-buffer compression API for the
 * Minishrinkler compressor, a simplified version of the Shrinkler algorithm.
 * 
 * Features:
 * - No dynamic memory allocations (uses static buffers)
 * - Compatible with official Shrinkler decompressor
 * - LZ77 compression with hash-based match finding
 * - Range coder with context modeling
 * - Generates raw compressed data (no header)
 * 
 * Usage:
 * @code
 * #include "minishrinkler_internal.h"
 * 
 * uint8_t input_buffer[1024];
 * uint8_t output_buffer[2048];  // Should be larger than input
 * 
 * int compressed_size = minishrinkler_compress(
 *     input_buffer, 1024,      // input data and size
 *     output_buffer, 2048,     // output buffer and capacity
 *     4608                     // work memory size for hash table
 * );
 * 
 * if (compressed_size > 0) {
 *     // Success: compressed_size contains the actual compressed size
 * } else {
 *     // Error: output buffer too small or other error
 * }
 * @endcode
 */

#ifndef MINISHRINKLER_H
#define MINISHRINKLER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compress data from input buffer to output buffer
 * 
 * This function compresses the input data using the Minishrinkler algorithm
 * and writes the compressed data to the output buffer. The compressed data
 * is compatible with the official Shrinkler decompressor.
 * 
 * @param input_data Pointer to the input data to compress
 * @param input_size Size of the input data in bytes
 * @param output_buffer Pointer to the output buffer for compressed data
 * @param output_capacity Maximum capacity of the output buffer in bytes
 * @param work_memory_size Size of work memory to allocate for hash table in bytes
 * 
 * @return On success: number of bytes written to output_buffer (compressed size)
 * @return On failure: negative value indicating error
 *         -1: output buffer too small
 *         -2: invalid input parameters
 *         -3: compression failed
 * 
 * @note The output buffer should be at least as large as the input data
 *       to handle worst-case scenarios where compression doesn't help.
 *       For typical data, the compressed size will be smaller than input.
 * 
 * @note The compressed output is raw data without any header, compatible
 *       with the official Shrinkler decompressor using the -d option.
 * 
 * @note This function does not perform any file I/O operations.
 */
int minishrinkler_compress(
    const uint8_t *input_data,
    int input_size,
    uint8_t *output_buffer,
    int output_capacity,
    int work_memory_size
);

/**
 * @brief Get the maximum compressed size for given input size
 * 
 * This function returns the maximum possible compressed size for a given
 * input size. This is useful for allocating output buffers.
 * 
 * @param input_size Size of the input data in bytes
 * @return Maximum compressed size in bytes
 * 
 * @note The actual compressed size will typically be much smaller.
 *       This is just the worst-case scenario.
 */
size_t minishrinkler_get_max_compressed_size(size_t input_size);

#ifdef __cplusplus
}
#endif

#endif /* MINISHRINKLER_H */
