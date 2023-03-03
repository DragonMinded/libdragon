/**
 * @file n64types.h
 * @brief Custom types used by libdragon
 * @ingroup libdragon
 */

#ifndef __LIBDRAGON_N64TYPES_H
#define __LIBDRAGON_N64TYPES_H

#include <stdint.h>
#include <stdalign.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Unaligned 64-bit integer type.
 * 
 * This type is used to represent 64-bit integers that are not aligned to 8-byte.
 * Accessing memory through a pointer of this type will make the compiler
 * issue the appropriate unaligned load/store instructions (LDL/LDR/SDL/SDR).
 */
typedef uint64_t u_uint64_t __attribute__((aligned(1)));

/**
 * @brief Unaligned 32-bit integer type.
 * 
 * This type is used to represent 32-bit integers that are not aligned to 4-byte.
 * Accessing memory through a pointer of this type will make the compiler
 * issue the appropriate unaligned load/store instructions (LWL/LWR/SWL/SWR).
 */
typedef uint32_t u_uint32_t __attribute__((aligned(1)));

/**
 * @brief Unaligned 16-bit integer type.
 * 
 * This type is used to represent 16-bit integers that are not aligned to 2-byte.
 * Accessing memory through a pointer of this type will make the compiler
 * issue the appropriate sequence (eg: loading two bytes and combining them)
 */
typedef uint16_t u_uint16_t __attribute__((aligned(1)));

#ifdef __cplusplus
}
#endif

#endif
