/**
 * @file vaddr64.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief 64-bit virtual address space access functions
 * @ingroup n64sys
 * 
 * Libdragon uses the O64 ABI, in which pointers are 32-bit wide. This
 * is the right choice for basically all standard use cases because
 * doubling the size of the pointers would waste more memory in all data
 * structures where pointers are stored.
 * 
 * The VR4300 CPU does support a full 64-bit virtual address space
 * though, which might be used for some very niche use case
 * (like e.g. emulator tests) Since it is not possible to create a
 * 64-bit pointer in C because of the chosen ABI, these functions
 * are provided in substitution.
 * 
 * The virtual address must be provided as a #vaddr64_t (uint64_t), as
 * defined in n64types.h.
 */

#ifndef LIBDRAGON_VADDR64_H
#define LIBDRAGON_VADDR64_H

#include <stdint.h>
#include "n64types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read a 8-bit value from memory at the given 64-bit virtual address
 * 
 * @param vaddr   64-bit virtual address
 * @return the read value
 */
inline uint8_t sys_vaddr_read8(vaddr64_t vaddr) {
    uint8_t value;
    asm volatile (
        "lbu %[value], 0(%[vaddr])  \n" :
        [value] "=r" (value):
        [vaddr] "r" (vaddr)
    );
    return value;    
}

/**
 * @brief Read a 16-bit value from memory at the given 64-bit virtual address
 * 
 * @param vaddr   64-bit virtual address
 * @return the read value
 */
inline uint16_t sys_vaddr_read16(vaddr64_t vaddr) {
    uint16_t value;
    asm volatile (
        "lhu %[value], 0(%[vaddr])  \n" :
        [value] "=r" (value):
        [vaddr] "r" (vaddr)
    );
    return value;    
}

/**
 * @brief Read a 32-bit value from memory at the given 64-bit virtual address
 * 
 * @param vaddr   64-bit virtual address
 * @return the read value
 */
inline uint32_t sys_vaddr_read32(vaddr64_t vaddr) {
    uint32_t value;
    asm volatile (
        "lw %[value], 0(%[vaddr])  \n" :
        [value] "=r" (value):
        [vaddr] "r" (vaddr)
    );
    return value;    
}

/**
 * @brief Read a 64-bit value from memory at the given 64-bit virtual address
 * 
 * @param vaddr   64-bit virtual address
 * @return the read value
 */
inline uint64_t sys_vaddr_read64(vaddr64_t vaddr) {
    uint64_t value;
    asm volatile (
        "ld %[value], 0(%[vaddr])  \n" :
        [value] "=r" (value):
        [vaddr] "r" (vaddr)
    );
    return value;    
}

/**
 * @brief Write an 8-bit value to memory at the given 64-bit virtual address
 *
 * @param vaddr   64-bit virtual address
 * @param value   8-bit value to write
 */
inline void sys_vaddr_write8(vaddr64_t vaddr, uint8_t value) {
    asm volatile (
        "sb %[value], 0(%[vaddr])  \n" :
        :
        [value] "r" (value),
        [vaddr] "r" (vaddr)
    );
}

/**
 * @brief Write a 16-bit value to memory at the given 64-bit virtual address
 *
 * @param vaddr   64-bit virtual address
 * @param value   16-bit value to write
 */
inline void sys_vaddr_write16(vaddr64_t vaddr, uint16_t value) {
    asm volatile (
        "sh %[value], 0(%[vaddr])  \n" :
        :
        [value] "r" (value),
        [vaddr] "r" (vaddr)
    );
}

/**
 * @brief Write a 32-bit value to memory at the given 64-bit virtual address
 *
 * @param vaddr   64-bit virtual address
 * @param value   32-bit value to write
 */
inline void sys_vaddr_write32(vaddr64_t vaddr, uint32_t value) {
    asm volatile (
        "sw %[value], 0(%[vaddr])  \n" :
        :
        [value] "r" (value),
        [vaddr] "r" (vaddr)
    );
}

/**
 * @brief Write a 64-bit value to memory at the given 64-bit virtual address
 *
 * @param vaddr   64-bit virtual address
 * @param value   64-bit value to write
 */
inline void sys_vaddr_write64(vaddr64_t vaddr, uint64_t value) {
    asm volatile (
        "sd %[value], 0(%[vaddr])  \n" :
        :
        [value] "r" (value),
        [vaddr] "r" (vaddr)
    );
}

#ifdef __cplusplus
}
#endif

#endif // LIBDRAGON_VADDR64_H
