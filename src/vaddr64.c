/**
 * @file vaddr64.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief 64-bit virtual address space access functions
 * @ingroup n64sys
 */

#include "vaddr64.h"

/* Inline instantiations */
/// @cond
extern inline uint8_t  sys_vaddr_read8(uint64_t vaddr);
extern inline uint16_t sys_vaddr_read16(uint64_t vaddr);
extern inline uint32_t sys_vaddr_read32(uint64_t vaddr);
extern inline uint64_t sys_vaddr_read64(uint64_t vaddr);
extern inline void sys_vaddr_write8(uint64_t vaddr, uint8_t value);
extern inline void sys_vaddr_write16(uint64_t vaddr, uint16_t value);
extern inline void sys_vaddr_write32(uint64_t vaddr, uint32_t value);
extern inline void sys_vaddr_write64(uint64_t vaddr, uint64_t value);
/// @endcond
