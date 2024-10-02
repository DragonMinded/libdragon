#ifndef LIBDRAGON_KERNEL_TLS_H
#define LIBDRAGON_KERNEL_TLS_H

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

#endif
