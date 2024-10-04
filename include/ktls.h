#ifndef LIBDRAGON_KERNEL_TLS_H
#define LIBDRAGON_KERNEL_TLS_H

#define KERNEL_TP_INVALID ((void *)0x5FFF8001)

#ifdef N64_DSO
__asm__ (
    ".macro rdhwr rt, rd" "\n"
    "   .error \" Usage of thread-local variables is not supported in DSOs. \"" "\n"
    ".endm"               "\n"
);
#else
__asm__ (
    ".macro rdhwr rt, rd" "\n"
    "    lw \\rt, %gprel(th_cur_tp)($gp)" "\n"
    ".endm"               "\n"
);
#endif

extern void *th_cur_tp;

#endif
