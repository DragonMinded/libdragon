/**
 * @file ktls.h
 * @author Liam Coleman <gamemasterplc@gmail.com>
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_KERNEL_TLS_H
#define LIBDRAGON_KERNEL_TLS_H

/** 
 * @brief Invalid thread pointer value 
 * 
 * This is used to indicate that the current thread pointer is invalid, which
 * happens during exception processing. If the code happens to access TLS,
 * it will dereference this, and the exception code in __get_exception_name will
 * be able to detect this and print an useful error message.
 */
#define KERNEL_TP_INVALID ((void *)0x5FFF8001)

#ifdef N64_DSO
/** @brief Assembly macro for DSO builds that errors on thread-local variable usage */
__asm__ (
    ".ifndef __RDHWR_WAS_DEFINED" "\n"
    ".macro rdhwr rt, rd" "\n"
    "   .error \" Usage of thread-local variables is not supported in DSOs. \"" "\n"
    ".endm"               "\n"
    ".set __RDHWR_WAS_DEFINED, 1" "\n"
    ".endif" "\n"
);
#else
/** @brief Assembly macro for thread-local storage access */
__asm__ (
    ".ifndef __RDHWR_WAS_DEFINED" "\n"
    ".macro rdhwr rt, rd" "\n"
    "    lw \\rt, %gprel(th_cur_tp)($gp)" "\n"
    ".endm"               "\n"
    ".set __RDHWR_WAS_DEFINED, 1" "\n"
    ".endif" "\n"
);
#endif

extern void *th_cur_tp;

#endif
