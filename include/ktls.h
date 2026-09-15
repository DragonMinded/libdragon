/**
 * @file ktls.h
 * @author Liam Coleman <gamemasterplc@gmail.com>
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_KERNEL_TLS_H
#define LIBDRAGON_KERNEL_TLS_H

#ifndef __ASSEMBLER__
/** @brief Assembly macro for thread-local storage access */
__asm__ (
    ".ifndef __RDHWR_WAS_DEFINED" "\n"
    ".macro rdhwr rt, rd" "\n"
    "    lw \\rt, %gprel(__th_cur_tp)($gp)" "\n"
    ".endm"               "\n"
    ".set __RDHWR_WAS_DEFINED, 1" "\n"
    ".endif" "\n"
);
#endif

#endif
