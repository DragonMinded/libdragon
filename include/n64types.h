/**
 * @file n64types.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
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

/** 
 * @brief A physical address on the MIPS bus.
 * 
 * Physical addresses are 32-bit wide, and are used to address the memory
 * space of the MIPS R4300 CPU. The MIPS R4300 CPU has a 32-bit address bus,
 * and can address up to 4 GiB of memory.
 * 
 * Physical addresses are just numbers, they cannot be used as pointers (dereferenced).
 * To access them, you must first convert them virtual addresses using the
 * #VirtualCachedAddr or #VirtualUncachedAddr macros.
 * 
 * In general, libdragon will try to use #phys_addr_t whenever a physical
 * address is expected or returned, and C pointers for virtual addresses.
 * Unfortunately, not all codebase can be changed to follow this convention
 * for backward compatibility reasons.
 */
typedef uint32_t phys_addr_t;

/**
 * @brief A PI address (on the peripheral bus)
 * 
 * The peripheral bus (PI) is a 32-bit address space used to address
 * devices on the cartridge slot or the bottom slot (N64DD).
 * 
 * Accessing PI addresses is only possible via DMA or I/O operations, as performed
 * by #dma_read, #dma_write, #io_read, and #io_write.
 * 
 * A large portion of the PI address space is also memory mapped to the CPU's
 * address space, meaning that some PI addresses are also valid physical addresses.
 * Check the N64 memory map to see the full mapping of PI addresses, or use
 * #io_accessible if you need to check that at runtime.
 */
typedef uint32_t pi_addr_t;


#ifdef __cplusplus
}
#endif

#endif
