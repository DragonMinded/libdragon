############################################################
# emux.inc - Implementation of the EMUX extensions
#
# Authors: Giovanni Bajo <giovannibajo@gmail.com>
############################################################
#include "emux.h"

.macro xdetect rd, code
    .word EMUX_XDETECT(\rd, \code)
.endm

.macro xbreak
    .word EMUX_XBREAK()
.endm

.macro xbreakpoint rd, op
    .word EMUX_XBREAKPOINT(\rd, \op)
.endm

.macro xtracestart num_insn=0
    .word EMUX_XTRACESTART(\num_insn)
.endm

.macro xtracestop
    .word EMUX_XTRACESTOP()
.endm

.macro xlog_string str
    .data
__xlog_string_\@: .asciiz "\str"
    .previous
    .set noat
    la $1, __xlog_string_\@
    .word EMUX_XLOG(1, 0, EMUX_LOG_ASCIIZ)
    .set at
.endm

.macro __xlogregs_gpr_rec reg0, regs:vararg
    hexGeneralRegisters
    .set gpr_mask, gpr_mask + (1 << hex.\reg0)
    .ifnb \regs
        __xlogregs_gpr_rec \regs
    .endif
.endm

.macro xlogregs_gpr regs:vararg
    .ifb \regs
        .set gpr_mask, 0
    .else
        .set gpr_mask, 0
        __xlogregs_gpr_rec \regs
    .endif
    .set noat
    li $1, gpr_mask
    .word EMUX_XLOGREGS(1, EMUX_LOGREGS_GPR)
    .set at
.endm

.macro __xlogregs_vpr_rec reg0, regs:vararg
    hexRegisters
    .set vpr_mask, vpr_mask + (1 << \reg0)
    .ifnb \regs
        __xlogregs_vpr_rec \regs
    .endif
.endm

.macro xlogregs_vpr regs:vararg
    .ifb \regs
        .set vpr_mask, 0
    .else
        .set vpr_mask, 0
        __xlogregs_vpr_rec \regs
    .endif
    .set noat
    li $1, vpr_mask
    .word EMUX_XLOGREGS(1, EMUX_LOGREGS_COP2)
    .set at
.endm

.macro xlogregs_vpr_accum regs:vararg
    .ifb \regs
        .set vpr_mask, 0
    .else
        .set vpr_mask, 0
        __xlogregs_vpr_rec \regs
    .endif
    .set noat
    li $1, vpr_mask
    .word EMUX_XLOGREGS(1, EMUX_LOGREGS_COP2 | EMUX_LOGREGS_EXTRA)
    .set at
.endm

.macro __xlogregs_cop0_rec reg0, regs:vararg
    .set cop0reg.$0,   0
    .set cop0reg.$1,   1
    .set cop0reg.$2,   2
    .set cop0reg.$3,   3
    .set cop0reg.$4,   4
    .set cop0reg.$5,   5
    .set cop0reg.$6,   6
    .set cop0reg.$7,   7
    .set cop0reg.$8,   8
    .set cop0reg.$9,   9
    .set cop0reg.$10,  10
    .set cop0reg.$11,  11
    .set cop0reg.$12,  12
    .set cop0reg.$13,  13
    .set cop0reg.$14,  14
    .set cop0reg.$15,  15
    .set cop0_mask, cop0_mask + (1 << cop0reg.\reg0)
    .ifnb \regs
        __xlogregs_cop0_rec \regs
    .endif
.endm

.macro xlogregs_cop0 regs:vararg
    .ifb \regs
        .set cop0_mask, 0
    .else
        .set cop0_mask, 0
        __xlogregs_cop0_rec \regs
    .endif
    .set noat
    li $1, cop0_mask
    .word EMUX_XLOGREGS(1, EMUX_LOGREGS_COP0)
    .set at
.endm

.macro xioctl_exit
    .word EMUX_XIOCTL(EMUX_IOCTL_EXIT)
.endm
