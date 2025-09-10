##############################################
# __mi_memset - Fast memset using MI repeat mode
#
# Authors: Giovanni Bajo <giovannibajo@gmail.com>
##############################################

#include "regs.S"
#define MI_WMODE_SET_REPEAT_MODE        0x100

    .section .text.__mi_memset
	.p2align 5
    .globl __mi_memset
    .globl __mi_memset16
    .globl __mi_memset32
    .globl __mi_memset64
    .set at
    .set noreorder

# Force the same N opcodes to be in the same cacheline
.macro .same_cacheline num_opcodes
    .p2align 5,,((\num_opcodes-1)*4)
.endm
.macro cache_wbi  addr
    cache HIT_WRITEBACK_INVALIDATE_D, \addr
.endm
.macro cache_i   addr
    cache HIT_INVALIDATE_D, \addr
.endm

#define ptr         a0
#define val         a1
#define len         a2
#define end         t8
#define srsave      t7
#define srnoirq     t6
#define MI_MODE     t5
#define duffptr     t4
#define uptr        v0


    # __mi_memsetN(void *ptr, int value, size_t len)
__mi_memset:
    dsll t0, val, 8
    or val, t0
__mi_memset16:
    dsll t0, val, 16
    or val, t0
__mi_memset32:
    dsll t0, val, 32
    or val, t0
__mi_memset64:
    addiu t0, len, -7
    bltz t0, .Lsmall
     addu end, ptr, len

    # Align to the next 8-byte boundary
    andi t0, ptr, 7
    beqz t0, .Laligned8
     sll t1, t0, 3

    # Write the first few bytes to reach 8-byte alignment
    sdl val, 0(ptr)
    neg t0
    andi t0, 7
    addu ptr, t0
    sub len, t0

    # Rotate the value after the partial write
    dsllv t2, val, t1
    neg t1, t1
    dsrlv val, val, t1
    or val, t2

.Laligned8:
    # We are now 8-byte aligned. From now on, we will be using
    # the repeat mode. Setup registers
    or uptr, ptr, 0x20000000
    la duffptr, .Lduffend
    mfc0 srsave, C0_SR
    and srnoirq, srsave, ~0x1
    beq uptr, ptr, .Lmirepeat_first     # Check if the pointer is uncached
     li MI_MODE, 0xA4300000

    # Write the possibly-partial first/last cachelines to avoid
    # invalidating false-shared data. We could do that always,
    # but make them conditional to save a few cycles for aligned cases.
    andi t0, ptr, 0xF
    bnezl t0, 1f
     cache_wbi (ptr)
    # End pointer could be misaligned, we need to sweat a bit more
1:  andi t0, end, 0xF
    beqz t0, 1f
     srl t0, end, 3
    sll t0, 3
    cache_wbi (t0)
1:

.Lmirepeat_first:
    # Now align to 128 bytes, by limiting the first repetition.
    # Repeat modes does not require 128 bytes alignment, but it does require
    # the whole write stay within a RDRAM row (0x800), and there is a maximum
    # of 128 bytes per repeat, so we kill two birds with one stone.
    neg t0, ptr
    andi t0, 0x7F
    beqzl t0, .Lmirepeat_len
.Lmirepeat:
     li t0, 128
.Lmirepeat_len:
    # t0 is now the maximum requested length for this
    # chunk (up to 128 bytes). Calculate t0 = min(t0, len)
    bltl len, t0, 1f
     move t0, len
1:
    # Prepare repeat mode register value in t1
    ori t1, t0, MI_WMODE_SET_REPEAT_MODE
    addiu t1, -1
    beq uptr, ptr, .Ldorepeat      # If the pointer is uncached, do not invalidate cache
     addiu t2, duffptr, 4
    # Calculate duff device pointer offset
    addiu t2, t0, -1
    srl t2, 4
    sll t2, 2
    sub t2, duffptr, t2
.Ldorepeat:
    # Do MI repeat write with interrupts off
    mtc0 srnoirq, C0_SR
    .same_cacheline 2
    sw t1, (MI_MODE)
    sd val, (uptr)
    jr t2
     mtc0 srsave, C0_SR
    # Duff device to clear the cache. Some of these cache
    # instructions will be skipped if < 128 bytes were written.
    cache_i 112(ptr)
    cache_i  96(ptr)
    cache_i  80(ptr)
    cache_i  64(ptr)
    cache_i  48(ptr)
    cache_i  32(ptr)
    cache_i  16(ptr)
.Lduffend:
    cache_i   0(ptr)

    # Loop epilogue: update pointers and remaining length
    sub len, t0
    add ptr, t0
    bgtz len, .Lmirepeat
     add uptr, t0
    jr ra
     nop

    # Fewer than 8 bytes: do it bytewise as speed is not critical
.Lsmall:
    dsrl t0, val, 56
    sb t0, 0(ptr)
    dsll val, 8
    addiu len, -1
    bgez len, .Lsmall
     addiu ptr, 1
    jr ra
     nop


