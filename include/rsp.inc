##################################################
# RSP.INC - Main include file to write RSP ucode
##################################################

#ifndef RSP_INC
#define RSP_INC

# MIPS general register definitions
#include <regdef.h>

###############################################################################¶
# libdragon MIPS conventions for ucode.
#
# You are not required to follow these for your own code, but we would like all
# upstream code to follow these, plus shared libraries will also follow these.
#
# * microcode always starts at 0x0000 in IMEM
# * "set noreorder" by default. The code is expected to explicitly handle delay
#   slots.
# * "set at" by default, but it is of course possible to switch to "set noat"
#   to have one more scratch register when needed (eg: very useful in macros
#   and helpers).
# * No stack configured. Everything will happen using GPRs and manual loads/saves
#   from DMEM. Normally, it is possible to have up to 2 levels of nested function
#   calls, because "sp" is reserved to this (can be called "ra2"). (Ab)use tail
#   calls to help with this. If you need more, try to refactor, or in the worse
#   case, you need to allocate another register as "ra3".
# * t0-t3: scratch value registers, often used also as arguments to functions.
#   These are considered clobbered by defaults in function calls.
# * s0-s4: scratch pointer registers, often used also as arguments to functions.
#   These are often clobbered by functions calls.
# * a0-a3: used to store the input to microcode (eg: in case of display list
#   processing, they can contain the current command).
# * Any other register: use it for keeping values around in larger sections of
#   the code (or even the whole microcode). Rename each register with #define
#   to begin using, and then #undef after you don't need it.
###############################################################################¶

    .set at
    .set noreorder

#define ra2 sp


###############################################################################
# Not implemented macro.
#
# We use this to disable standard MIPS opcodes that are not
# available on RSP. There are quite a few of them.
###############################################################################
.macro makeNotImplemented name
    .macro \name _:vararg
        .error "\name is not supported by RSP"
    .endm
.endm

makeNotImplemented ldl
makeNotImplemented ldr
makeNotImplemented lwl
makeNotImplemented lwr
makeNotImplemented lwu
makeNotImplemented swl
makeNotImplemented sdl
makeNotImplemented sdr
makeNotImplemented swr
makeNotImplemented ll
makeNotImplemented lld
makeNotImplemented ldc1
makeNotImplemented ldc2
makeNotImplemented ld
makeNotImplemented sdc1
makeNotImplemented sdc2
makeNotImplemented sd
makeNotImplemented sc
makeNotImplemented scd
makeNotImplemented beql
makeNotImplemented bnel
makeNotImplemented blezl
makeNotImplemented bgtzl
makeNotImplemented bltzl
makeNotImplemented bgezl
makeNotImplemented bltzall
makeNotImplemented bgtzall
makeNotImplemented bgezall
makeNotImplemented mfhi
makeNotImplemented mthi
makeNotImplemented mflo
makeNotImplemented mtlo
makeNotImplemented daddi
makeNotImplemented daddiu
makeNotImplemented dsllv
makeNotImplemented dsrlv
makeNotImplemented dsrav
makeNotImplemented dmult
makeNotImplemented dmultu
makeNotImplemented ddiv
makeNotImplemented ddivu
makeNotImplemented dadd
makeNotImplemented daddu
makeNotImplemented dsub
makeNotImplemented dsubu
makeNotImplemented dsll
makeNotImplemented dsrl
makeNotImplemented dsra
makeNotImplemented dsll32
makeNotImplemented dsrl32
makeNotImplemented dsra32
makeNotImplemented mult
makeNotImplemented multu
makeNotImplemented div
makeNotImplemented divu
makeNotImplemented syscall
makeNotImplemented sync
makeNotImplemented bc1f
makeNotImplemented bc1t
makeNotImplemented tge
makeNotImplemented tgeu
makeNotImplemented tlt
makeNotImplemented tltu
makeNotImplemented teq
makeNotImplemented tne
makeNotImplemented tgei
makeNotImplemented tgeiu
makeNotImplemented tlti
makeNotImplemented tltiu
makeNotImplemented teqi
makeNotImplemented tnei

##################################################
# Vector opcode definitions
##################################################

.macro hexGeneralRegisters
    .equ hex.zero, 0
    .equ hex.at, 1
    .equ hex.v0, 2
    .equ hex.v1, 3
    .equ hex.a0, 4
    .equ hex.a1, 5
    .equ hex.a2, 6
    .equ hex.a3, 7
    .equ hex.t0, 8
    .equ hex.t1, 9
    .equ hex.t2, 10
    .equ hex.t3, 11
    .equ hex.t4, 12
    .equ hex.t5, 13
    .equ hex.t6, 14
    .equ hex.t7, 15
    .equ hex.s0, 16
    .equ hex.s1, 17
    .equ hex.s2, 18
    .equ hex.s3, 19
    .equ hex.s4, 20
    .equ hex.s5, 21
    .equ hex.s6, 22
    .equ hex.s7, 23
    .equ hex.t8, 24
    .equ hex.t9, 25
    .equ hex.k0, 26
    .equ hex.k1, 27
    .equ hex.gp, 28
    .equ hex.sp, 29
    .equ hex.fp, 30
    .equ hex.ra, 31
.endm

.macro hexRegisters
    .set $v00, 0x0
    .set $v01, 0x1
    .set $v02, 0x2
    .set $v03, 0x3
    .set $v04, 0x4
    .set $v05, 0x5
    .set $v06, 0x6
    .set $v07, 0x7
    .set $v08, 0x8
    .set $v09, 0x9
    .set $v10, 0xA
    .set $v11, 0xB
    .set $v12, 0xC
    .set $v13, 0xD
    .set $v14, 0xE
    .set $v15, 0xF
    .set $v16, 0x10
    .set $v17, 0x11
    .set $v18, 0x12
    .set $v19, 0x13
    .set $v20, 0x14
    .set $v21, 0x15
    .set $v22, 0x16
    .set $v23, 0x17
    .set $v24, 0x18
    .set $v25, 0x19
    .set $v26, 0x1A
    .set $v27, 0x1B
    .set $v28, 0x1C
    .set $v29, 0x1D
    .set $v30, 0x1E
    .set $v31, 0x1F
.endm

.macro renameRegisters
    .set $v00, $0
    .set $v01, $1
    .set $v02, $2
    .set $v03, $3
    .set $v04, $4
    .set $v05, $5
    .set $v06, $6
    .set $v07, $7
    .set $v08, $8
    .set $v09, $9
    .set $v10, $10
    .set $v11, $11
    .set $v12, $12
    .set $v13, $13
    .set $v14, $14
    .set $v15, $15
    .set $v16, $16
    .set $v17, $17
    .set $v18, $18
    .set $v19, $19
    .set $v20, $20
    .set $v21, $21
    .set $v22, $22
    .set $v23, $23
    .set $v24, $24
    .set $v25, $25
    .set $v26, $26
    .set $v27, $27
    .set $v28, $28
    .set $v29, $29
    .set $v30, $30
    .set $v31, $31
.endm

.macro encodeVectorRegs
    .set $v00, 0x100
    .set $v01, 0x101
    .set $v02, 0x102
    .set $v03, 0x103
    .set $v04, 0x104
    .set $v05, 0x105
    .set $v06, 0x106
    .set $v07, 0x107
    .set $v08, 0x108
    .set $v09, 0x109
    .set $v10, 0x10A
    .set $v11, 0x10B
    .set $v12, 0x10C
    .set $v13, 0x10D
    .set $v14, 0x10E
    .set $v15, 0x10F
    .set $v16, 0x110
    .set $v17, 0x111
    .set $v18, 0x112
    .set $v19, 0x113
    .set $v20, 0x114
    .set $v21, 0x115
    .set $v22, 0x116
    .set $v23, 0x117
    .set $v24, 0x118
    .set $v25, 0x119
    .set $v26, 0x11A
    .set $v27, 0x11B
    .set $v28, 0x11C
    .set $v29, 0x11D
    .set $v30, 0x11E
    .set $v31, 0x11F
.endm

# Encode vector elements for use in e(x) style syntax.
.macro veVectorElements
    .set VE_v,  0x0
    .set VE_0q, 0x2
    .set VE_1q, 0x3
    .set VE_0h, 0x4
    .set VE_1h, 0x5
    .set VE_2h, 0x6
    .set VE_3h, 0x7
    .set VE_0,  0x8
    .set VE_1,  0x9
    .set VE_2,  0xA
    .set VE_3,  0xB
    .set VE_4,  0xC
    .set VE_5,  0xD
    .set VE_6,  0xE
    .set VE_7,  0xF
.endm

# Encode byte offsets for use in e(x) style syntax.
.macro byteVectorElements
    .set VE_v,  -1
    .set VE_0q, -1
    .set VE_1q, -1
    .set VE_0h, -1
    .set VE_1h, -1
    .set VE_2h, -1
    .set VE_3h, -1
    .set VE_0,  0x0
    .set VE_1,  0x2
    .set VE_2,  0x4
    .set VE_3,  0x6
    .set VE_4,  0x8
    .set VE_5,  0xA
    .set VE_6,  0xC
    .set VE_7,  0xE
.endm

# Encode byte offsets for use in e(x) style syntax.
.macro laneAsByteVectorElements
    byteVectorElements
.endm

# Encode lane indices for use in e(x) style syntax.
.macro laneVectorElements
    .set VE_v,  -1
    .set VE_0q, -1
    .set VE_1q, -1
    .set VE_0h, -1
    .set VE_1h, -1
    .set VE_2h, -1
    .set VE_3h, -1
    .set VE_0,  0x0
    .set VE_1,  0x1
    .set VE_2,  0x2
    .set VE_3,  0x3
    .set VE_4,  0x4
    .set VE_5,  0x5
    .set VE_6,  0x6
    .set VE_7,  0x7
.endm

# This will encode element accessors for computational
# opcodes, which interpret the element as either lane index + 8
# or broadcast specifiers. For these, the .bN accessors are not valid.
.macro veAccessorEncoder prefix, base
    .set \prefix\().v,   (\base + 0x0)
    .set \prefix\().q0,  (\base + 0x2)
    .set \prefix\().q1,  (\base + 0x3)
    .set \prefix\().h0,  (\base + 0x4)
    .set \prefix\().h1,  (\base + 0x5)
    .set \prefix\().h2,  (\base + 0x6)
    .set \prefix\().h3,  (\base + 0x7)
    .set \prefix\().e0,  (\base + 0x8)
    .set \prefix\().e1,  (\base + 0x9)
    .set \prefix\().e2,  (\base + 0xA)
    .set \prefix\().e3,  (\base + 0xB)
    .set \prefix\().e4,  (\base + 0xC)
    .set \prefix\().e5,  (\base + 0xD)
    .set \prefix\().e6,  (\base + 0xE)
    .set \prefix\().e7,  (\base + 0xF)
    .set \prefix\().b0,  -1
    .set \prefix\().b1,  -1
    .set \prefix\().b2,  -1
    .set \prefix\().b3,  -1
    .set \prefix\().b4,  -1
    .set \prefix\().b5,  -1
    .set \prefix\().b6,  -1
    .set \prefix\().b7,  -1
    .set \prefix\().b8,  -1
    .set \prefix\().b9,  -1
    .set \prefix\().b10, -1
    .set \prefix\().b11, -1
    .set \prefix\().b12, -1
    .set \prefix\().b13, -1
    .set \prefix\().b14, -1
    .set \prefix\().b15, -1
.endm

# This will encode element accessors for computational
# opcodes, which interpret the element as either lane index + 8
# or broadcast specifiers. For these, the .bN accessors are not valid.
.macro singleLaneAccessorEncoder prefix, base
    .set \prefix\().v,   -1
    .set \prefix\().q0,  -1
    .set \prefix\().q1,  -1
    .set \prefix\().h0,  -1
    .set \prefix\().h1,  -1
    .set \prefix\().h2,  -1
    .set \prefix\().h3,  -1
    .set \prefix\().e0,  (\base + 0x8)
    .set \prefix\().e1,  (\base + 0x9)
    .set \prefix\().e2,  (\base + 0xA)
    .set \prefix\().e3,  (\base + 0xB)
    .set \prefix\().e4,  (\base + 0xC)
    .set \prefix\().e5,  (\base + 0xD)
    .set \prefix\().e6,  (\base + 0xE)
    .set \prefix\().e7,  (\base + 0xF)
    .set \prefix\().b0,  -1
    .set \prefix\().b1,  -1
    .set \prefix\().b2,  -1
    .set \prefix\().b3,  -1
    .set \prefix\().b4,  -1
    .set \prefix\().b5,  -1
    .set \prefix\().b6,  -1
    .set \prefix\().b7,  -1
    .set \prefix\().b8,  -1
    .set \prefix\().b9,  -1
    .set \prefix\().b10, -1
    .set \prefix\().b11, -1
    .set \prefix\().b12, -1
    .set \prefix\().b13, -1
    .set \prefix\().b14, -1
    .set \prefix\().b15, -1
.endm

# This will encode element accessors for opcodes which interpret
# the element as lane index (only packed load/store ops).
# For these, only the .eN accessors are valid.
.macro laneAccessorEncoder prefix, base
    .set \prefix\().v,   -1
    .set \prefix\().q0,  -1
    .set \prefix\().q1,  -1
    .set \prefix\().h0,  -1
    .set \prefix\().h1,  -1
    .set \prefix\().h2,  -1
    .set \prefix\().h3,  -1
    .set \prefix\().e0,  (\base + 0x0)
    .set \prefix\().e1,  (\base + 0x1)
    .set \prefix\().e2,  (\base + 0x2)
    .set \prefix\().e3,  (\base + 0x3)
    .set \prefix\().e4,  (\base + 0x4)
    .set \prefix\().e5,  (\base + 0x5)
    .set \prefix\().e6,  (\base + 0x6)
    .set \prefix\().e7,  (\base + 0x7)
    .set \prefix\().b0,  -1
    .set \prefix\().b1,  -1
    .set \prefix\().b2,  -1
    .set \prefix\().b3,  -1
    .set \prefix\().b4,  -1
    .set \prefix\().b5,  -1
    .set \prefix\().b6,  -1
    .set \prefix\().b7,  -1
    .set \prefix\().b8,  -1
    .set \prefix\().b9,  -1
    .set \prefix\().b10, -1
    .set \prefix\().b11, -1
    .set \prefix\().b12, -1
    .set \prefix\().b13, -1
    .set \prefix\().b14, -1
    .set \prefix\().b15, -1
.endm

# This will encode element accessors for opcodes which interpret
# the element as byte offset (mtc2/mfc2 and most load/store ops).
# For these, both .eN (lane index) and .bN (byte offset) accessors
# are valid. The broadcast accessors are not.
.macro byteAccessorEncoder prefix, base
    .set \prefix\().v,   -1
    .set \prefix\().q0,  -1
    .set \prefix\().q1,  -1
    .set \prefix\().h0,  -1
    .set \prefix\().h1,  -1
    .set \prefix\().h2,  -1
    .set \prefix\().h3,  -1
    .set \prefix\().e0,  (\base + 0x0)
    .set \prefix\().e1,  (\base + 0x2)
    .set \prefix\().e2,  (\base + 0x4)
    .set \prefix\().e3,  (\base + 0x6)
    .set \prefix\().e4,  (\base + 0x8)
    .set \prefix\().e5,  (\base + 0xA)
    .set \prefix\().e6,  (\base + 0xC)
    .set \prefix\().e7,  (\base + 0xE)
    .set \prefix\().b0,  (\base + 0x0)
    .set \prefix\().b1,  (\base + 0x1)
    .set \prefix\().b2,  (\base + 0x2)
    .set \prefix\().b3,  (\base + 0x3)
    .set \prefix\().b4,  (\base + 0x4)
    .set \prefix\().b5,  (\base + 0x5)
    .set \prefix\().b6,  (\base + 0x6)
    .set \prefix\().b7,  (\base + 0x7)
    .set \prefix\().b8,  (\base + 0x8)
    .set \prefix\().b9,  (\base + 0x9)
    .set \prefix\().b10, (\base + 0xA)
    .set \prefix\().b11, (\base + 0xB)
    .set \prefix\().b12, (\base + 0xC)
    .set \prefix\().b13, (\base + 0xD)
    .set \prefix\().b14, (\base + 0xE)
    .set \prefix\().b15, (\base + 0xF)
.endm

# This will encode element accessors for opcodes which interpret
# the element as byte offset, but only even offsets are valid
# (so effectively they accept lane indices, but encoded as byte offset).
# This applies to ltv and stv.
.macro laneAsByteAccessorEncoder prefix, base
    .set \prefix\().v,   -1
    .set \prefix\().q0,  -1
    .set \prefix\().q1,  -1
    .set \prefix\().h0,  -1
    .set \prefix\().h1,  -1
    .set \prefix\().h2,  -1
    .set \prefix\().h3,  -1
    .set \prefix\().e0,  (\base + 0x0)
    .set \prefix\().e1,  (\base + 0x2)
    .set \prefix\().e2,  (\base + 0x4)
    .set \prefix\().e3,  (\base + 0x6)
    .set \prefix\().e4,  (\base + 0x8)
    .set \prefix\().e5,  (\base + 0xA)
    .set \prefix\().e6,  (\base + 0xC)
    .set \prefix\().e7,  (\base + 0xE)
    .set \prefix\().b0,  -1
    .set \prefix\().b1,  -1
    .set \prefix\().b2,  -1
    .set \prefix\().b3,  -1
    .set \prefix\().b4,  -1
    .set \prefix\().b5,  -1
    .set \prefix\().b6,  -1
    .set \prefix\().b7,  -1
    .set \prefix\().b8,  -1
    .set \prefix\().b9,  -1
    .set \prefix\().b10, -1
    .set \prefix\().b11, -1
    .set \prefix\().b12, -1
    .set \prefix\().b13, -1
    .set \prefix\().b14, -1
    .set \prefix\().b15, -1
.endm

# This macro will encode all possible combinations of vector registers and
# element accessors (dot-syntax) as numbers >= 0x200. The lower 4 bits
# encode the element, the 5 bits above those encode the register number.
.macro defineVectorAccessors encoder
    \encoder $v00, 0x200
    \encoder $v01, 0x210
    \encoder $v02, 0x220
    \encoder $v03, 0x230
    \encoder $v04, 0x240
    \encoder $v05, 0x250
    \encoder $v06, 0x260
    \encoder $v07, 0x270
    \encoder $v08, 0x280
    \encoder $v09, 0x290
    \encoder $v10, 0x2A0
    \encoder $v11, 0x2B0
    \encoder $v12, 0x2C0
    \encoder $v13, 0x2D0
    \encoder $v14, 0x2E0
    \encoder $v15, 0x2F0
    \encoder $v16, 0x300
    \encoder $v17, 0x310
    \encoder $v18, 0x320
    \encoder $v19, 0x330
    \encoder $v20, 0x340
    \encoder $v21, 0x350
    \encoder $v22, 0x360
    \encoder $v23, 0x370
    \encoder $v24, 0x380
    \encoder $v25, 0x390
    \encoder $v26, 0x3A0
    \encoder $v27, 0x3B0
    \encoder $v28, 0x3C0
    \encoder $v29, 0x3D0
    \encoder $v30, 0x3E0
    \encoder $v31, 0x3F0
.endm

.macro veVectorAccessors
    defineVectorAccessors veAccessorEncoder
.endm

.macro singleLaneVectorAccessors
    defineVectorAccessors singleLaneAccessorEncoder
.endm

.macro laneVectorAccessors
    defineVectorAccessors laneAccessorEncoder
.endm

.macro byteVectorAccessors
    defineVectorAccessors byteAccessorEncoder
.endm

.macro laneAsByteVectorAccessors
    defineVectorAccessors laneAsByteAccessorEncoder
.endm

# The following two macros are required for load/store instructions,
# where we need to map back to GPR style syntax
.macro regAccessorEncoder prefix, value
    .set reg.\prefix\().v,   \value
    .set reg.\prefix\().q0,  \value
    .set reg.\prefix\().q1,  \value
    .set reg.\prefix\().h0,  \value
    .set reg.\prefix\().h1,  \value
    .set reg.\prefix\().h2,  \value
    .set reg.\prefix\().h3,  \value
    .set reg.\prefix\().e0,  \value
    .set reg.\prefix\().e1,  \value
    .set reg.\prefix\().e2,  \value
    .set reg.\prefix\().e3,  \value
    .set reg.\prefix\().e4,  \value
    .set reg.\prefix\().e5,  \value
    .set reg.\prefix\().e6,  \value
    .set reg.\prefix\().e7,  \value
    .set reg.\prefix\().b0,  \value
    .set reg.\prefix\().b1,  \value
    .set reg.\prefix\().b2,  \value
    .set reg.\prefix\().b3,  \value
    .set reg.\prefix\().b4,  \value
    .set reg.\prefix\().b5,  \value
    .set reg.\prefix\().b6,  \value
    .set reg.\prefix\().b7,  \value
    .set reg.\prefix\().b8,  \value
    .set reg.\prefix\().b9,  \value
    .set reg.\prefix\().b10, \value
    .set reg.\prefix\().b11, \value
    .set reg.\prefix\().b12, \value
    .set reg.\prefix\().b13, \value
    .set reg.\prefix\().b14, \value
    .set reg.\prefix\().b15, \value
.endm

.macro regVectorAccessors
    regAccessorEncoder $v00, $0
    regAccessorEncoder $v01, $1
    regAccessorEncoder $v02, $2
    regAccessorEncoder $v03, $3
    regAccessorEncoder $v04, $4
    regAccessorEncoder $v05, $5
    regAccessorEncoder $v06, $6
    regAccessorEncoder $v07, $7
    regAccessorEncoder $v08, $8
    regAccessorEncoder $v09, $9
    regAccessorEncoder $v10, $10
    regAccessorEncoder $v11, $11
    regAccessorEncoder $v12, $12
    regAccessorEncoder $v13, $13
    regAccessorEncoder $v14, $14
    regAccessorEncoder $v15, $15
    regAccessorEncoder $v16, $16
    regAccessorEncoder $v17, $17
    regAccessorEncoder $v18, $18
    regAccessorEncoder $v19, $19
    regAccessorEncoder $v20, $20
    regAccessorEncoder $v21, $21
    regAccessorEncoder $v22, $22
    regAccessorEncoder $v23, $23
    regAccessorEncoder $v24, $24
    regAccessorEncoder $v25, $25
    regAccessorEncoder $v26, $26
    regAccessorEncoder $v27, $27
    regAccessorEncoder $v28, $28
    regAccessorEncoder $v29, $29
    regAccessorEncoder $v30, $30
    regAccessorEncoder $v31, $31
.endm

# Instead of using the constants above, it is possible to use the syntax 'e(x)'
# via the following macros.
#define _PPCAT2(n,x) n ## x
#define _PPCAT(n,x) _PPCAT2(n,x)
#define e(xx) _PPCAT(VE_, xx)

/** @brief Syntactic sugar for cop2 instuctions */
.macro vectorOp opcode, vd, vs, vt, element
    .ifgt (\element >> 4)
        .error "Invalid element"
        .exitm
    .endif

    hexRegisters

    .ifgt (\vd >> 5)
        .error "Invalid vd register"
        .exitm
    .endif

    .ifgt (\vs >> 5)
        .error "Invalid vs register"
        .exitm
    .endif

    .ifgt (\vt >> 5)
        .error "Invalid vt register"
        .exitm
    .endif

    cop2 (\element << 21 | \vt << 16 | \vs << 11 | \vd << 6 | \opcode)
.endm

.macro makeOpInstruction name, opcode
    # Overloads:
    #   op vd, vs, vt, element
    #   op vd, vs, vt
    #   op vd, vs, vt.e
    #   op vd, vt, element
    #   op vd, vt
    #   op vd, vt.e
    .macro \name vd, vsOrVt, vtOrElement, elementOrEmpty
        veVectorElements

        # If the last argument is specified we can be sure that the full syntax is being used
        .ifnb \elementOrEmpty
            # All operands with element: op $v1, $v2, $v3, e(x)
            vectorOp \opcode, \vd, \vsOrVt, \vtOrElement, \elementOrEmpty
            .exitm
        .endif

        veVectorAccessors
        encodeVectorRegs

        # If only the last argument was omitted, we need to check which syntax is being used
        .ifnb \vtOrElement
            .iflt (\vtOrElement)
                .error "Invalid element"
                .exitm
            .endif

            # Dot-style syntax is encoded as numbers >= 0x200
            .if (\vtOrElement >= 0x200)
                # All operands with vt and element combined into accessor syntax: op $v1, $v2, $v3.e
                vectorOp \opcode, \vd, \vsOrVt, ((\vtOrElement >> 4) & 0x1F), (\vtOrElement & 0xF)
            # Because we encode vector registers as numbers >= 0x100, we can check if 
            # the third argument is vt or the vector element (which are < 0x10):
            .elseif (\vtOrElement >= 0x100)
                # All operands without element: op $v1, $v2, $v3
                vectorOp \opcode, \vd, \vsOrVt, \vtOrElement, 0
            .else
                # vs omitted with element: op $v1, $v2, e(x)
                vectorOp \opcode, \vd, \vd, \vsOrVt, \vtOrElement
            .endif
            .exitm
        .endif

        # Dot-style syntax is encoded as numbers >= 0x200
        .if (\vsOrVt >= 0x200)
            # vs omitted with vt and element combined into accessor syntax: op $v1, $v2.e
            vectorOp \opcode, \vd, \vd, ((\vsOrVt >> 4) & 0x1F), (\vsOrVt & 0xF)
        .else
            # vs omitted without element: op $v1, $v2
            vectorOp \opcode, \vd, \vd, \vsOrVt, 0
        .endif
    .endm
.endm

/** @brief Vector Absolute Value of Short Elements */
makeOpInstruction vabs, 0b010011
/** @brief Vector Add of Short Elements */
makeOpInstruction vadd, 0b010000
/** @brief Vector Add Short Elements With Carry */
makeOpInstruction vaddc, 0b010100
/** @brief Vector AND of Short Elements */
makeOpInstruction vand, 0b101000
/** @brief Vector Select Clip Test High */
makeOpInstruction vch, 0b100101
/** @brief Vector Select Clip Test Low */
makeOpInstruction vcl, 0b100100
/** @brief Vector Select Crimp Test Low */
makeOpInstruction vcr, 0b100110
/** @brief Vector Select Equal */
makeOpInstruction veq, 0b100001
/** @brief Vector Select Greater Than or Equal */
makeOpInstruction vge, 0b100011
/** @brief Vector Select Less Than */
makeOpInstruction vlt, 0b100000
/** @brief Vector Multiply-Accumulate of Signed Fractions */
makeOpInstruction vmacf, 0b001000
/** @brief Vector Multiply-Accumulate of Unsigned Fractions */
makeOpInstruction vmacu, 0b001001
/** @brief Vector Multiply-Accumulate of High Partial Products */
makeOpInstruction vmadh, 0b001111
/** @brief Vector Multiply-Accumulate of Low Partial Products */
makeOpInstruction vmadl, 0b001100
/** @brief Vector Multiply-Accumulate of Mid Partial Products */
makeOpInstruction vmadm, 0b001101
/** @brief Vector Multiply-Accumulate of Mid Partial Products */
makeOpInstruction vmadn, 0b001110
/** @brief Vector Select Merge */
makeOpInstruction vmrg, 0b100111
/** @brief Vector Multiply of High Partial Products */
makeOpInstruction vmudh, 0b000111
/** @brief Vector Multiply of Low Partial Products */
makeOpInstruction vmudl, 0b000100
/** @brief Vector Multiply of Mid Partial Products */
makeOpInstruction vmudm, 0b000101
/** @brief Vector Multiply of Mid Partial Products */
makeOpInstruction vmudn, 0b000110
/** @brief Vector Multiply of Signed Fractions */
makeOpInstruction vmulf, 0b000000
/** @brief Vector Multiply MPEG Quantization */
makeOpInstruction vmulq, 0b000011
/** @brief Vector Multiply of Unsigned Fractions */
makeOpInstruction vmulu, 0b000001
/** @brief Vector NAND of Short Elements */
makeOpInstruction vnand, 0b101001
/** @brief Vector Select Not Equal */
makeOpInstruction vne, 0b100010
/** @brief Vector NOR of Short Elements */
makeOpInstruction vnor, 0b101011
/** @brief Vector NXOR of Short Elements */
makeOpInstruction vnxor, 0b101101
/** @brief Vector OR of Short Elements */
makeOpInstruction vor, 0b101010
/** @brief Vector Subtraction of Short Elements */
makeOpInstruction vsub, 0b010001
/** @brief Vector Subtraction of Short Elements With Carry */
makeOpInstruction vsubc, 0b010101
/** @brief Vector XOR of Short Elements */
makeOpInstruction vxor, 0b101100

.macro makeSingleLaneInstruction name, opcode
    # Overloads:
    #   op vd, de, vt, element
    #   op vd, de, vt.e
    #   op vd.e, vt, element
    #   op vd.e, vt.e
    .macro \name vd, edOrVt, vtOrElement, elementOrEmpty
        veVectorElements
        
        .ifnb \elementOrEmpty
            # 4-arg syntax: op $v1, e(x), $v2, e(x)
            vectorOp \opcode, \vd, \edOrVt, \vtOrElement, \elementOrEmpty
            .exitm
        .endif

        singleLaneVectorAccessors

        .iflt (\vd)
            .error "Invalid destination element"
            .exitm
        .endif

        .ifnb \vtOrElement
            .iflt (\vtOrElement)
                .error "Invalid source element"
                .exitm
            .endif

            .if (\vtOrElement >= 0x200)
                # 3-arg syntax: op $v1, e(x), $v2.e
                vectorOp \opcode, \vd, \edOrVt, ((\vtOrElement >> 4) & 0x1F), (\vtOrElement & 0xF)
            .elseif (\vd >= 0x200)
                # 3-arg syntax: op $v1.e, $v2, e(x)
                vectorOp \opcode, ((\vd >> 4) & 0x1F), (\vd & 0xF), \edOrVt, \vtOrElement
            .else 
                .error "Invalid syntax"
            .endif
            .exitm
        .endif

        .iflt (\edOrVt)
            .error "Invalid source element"
            .exitm
        .endif

        .if (\vd < 0x200)
            .error "Element is required on destination register"
            .exitm
        .endif

        .if (\edOrVt < 0x200)
            .error "Element is required on source register"
            .exitm
        .endif

        # 2-arg syntax: op $v1.e, $v2.e
        vectorOp \opcode, ((\vd >> 4) & 0x1F), (\vd & 0xF), ((\edOrVt >> 4) & 0x1F), (\edOrVt & 0xF)
    .endm
.endm

/** @brief Vector Element Scalar Move */
makeSingleLaneInstruction vmov, 0b110011
/** @brief Vector Element Scalar Reciprocal (Single Precision) */
makeSingleLaneInstruction vrcp, 0b110000
/** @brief Vector Element Scalar Reciprocal (Double Prec. High) */
makeSingleLaneInstruction vrcph, 0b110010
/** @brief Vector Element Scalar Reciprocal (Double Prec. Low) */
makeSingleLaneInstruction vrcpl, 0b110001
/** @brief Vector Element Scalar SQRT Reciprocal */
makeSingleLaneInstruction vrsq, 0b110100
/** @brief Vector Element Scalar SQRT Reciprocal (Double Prec. High) */
makeSingleLaneInstruction vrsqh, 0b110110
/** @brief Vector Element Scalar SQRT Reciprocal (Double Prec. Low) */
makeSingleLaneInstruction vrsql, 0b110101

/** @brief Vector Null Instruction */
.macro vnop
    vectorOp 0b110111, 0, 0, 0, 0
.endm

#define COP2_ACC_HI             0x8
#define COP2_ACC_MD             0x9
#define COP2_ACC_LO             0xA

.macro vsarInternal vd, element
    .if (\element == COP2_ACC_HI || \element == COP2_ACC_MD || \element == COP2_ACC_LO)
        vectorOp 0b011101, \vd, 0, 0, \element
    .else
        .error "Invalid element"
    .endif
.endm

/** @brief Vector Accumulator Read (and Write) */
.macro vsar vd, vs, vt, element
    .ifnb \element
        # 4-arg syntax (Deprecated)
        vsarInternal \vd, \element
        .exitm
    .endif

    .ifnb \vt
        .error "Wrong number of arguments. Use syntax: vsar <vd> <element>"
        .exitm
    .endif

    .ifnb \vs
        # \vs is element
        vsarInternal \vd, \vs
        .exitm
    .endif

    .error "Wrong number of arguments. Use syntax: vsar <vd> <element>"
.endm

/** @brief Vector Accumulator DCT Rounding (Positive/Negative) 
  *
  * These are special vector opcodes that use the RS field
  * as flag: when 1, the operator is pre-shifted by 16.
  *
  * Export them as vrndn16 / vrndp16, so that they can be
  * used without making mistakes.
  */
.macro makeVrndpOp name, opcode, flag
    # Overloads:
    #   op vd, vt, element
    #   op vd, vt
    #   op vd, vt.e
    .macro \name vd, vt, element=0
        veVectorElements
        veVectorAccessors

        .iflt (\vt)
            .error "Invalid element"
            .exitm
        .endif

        # Dot-style syntax is encoded as numbers >= 0x200
        .if (\vt >= 0x200)
            vectorOp \opcode, \vd, \flag, ((\vt >> 4) & 0x1F), (\element & 0xF)
        .else
            vectorOp \opcode, \vd, \flag, \vt, \element
        .endif
    .endm
.endm

makeVrndpOp vrndn,   0b001010, 0
makeVrndpOp vrndn16, 0b001010, 1
makeVrndpOp vrndp,   0b000010, 0
makeVrndpOp vrndp16, 0b000010, 1

/** 
 * @brief Vector Accumulator Oddification
 *
 * This is a MPEG1-specific opcode, that is meant to be
 * used in sequence with "vmulq". The mnemonic has been chosen
 * for that reason, but has nothing to do with MAC or even
 * multiplication. It only accepts a destination register.
 */
.macro vmacq vd
    vectorOp 0b001011, \vd, 0, 0, 0
.endm


/** @brief Syntactic sugar for lwc2 instuctions */
.macro loadVector, opcode, vt, element, offset, base
    renameRegisters
    lwc2 \vt, (\opcode << 11 | \element << 7 | \offset) (\base)
.endm

/** @brief Syntactic sugar for swc2 instuctions */
.macro storeVector opcode, vt, element, offset, base
    renameRegisters
    swc2 \vt, (\opcode << 11 | \element << 7 | \offset) (\base)
.endm

.macro lsInstruction mode, opcode, vt, element, offset, base, size, rangemin, rangemax
    .ifgt (\element >> 4)
        .error "Invalid element"
        .exitm
    .endif

    .ifne ((\offset) % \size)
        .error "Invalid offset - must be multiple of \size"
        .exitm
    .endif

    .ifge (\offset)
        .ifgt ((\offset) - \rangemax)
            .error "Invalid offset - valid range: [\rangemin, \rangemax]"
            .exitm
        .endif

        \mode\()Vector \opcode, \vt, \element, ((\offset) / \size), \base
    .else
        .iflt ((\offset) - \rangemin)
            .error "Invalid offset - valid range: [\rangemin, \rangemax]"
            .exitm
        .endif

        \mode\()Vector \opcode, \vt, \element, (128 + ((\offset) / \size)), \base
    .endif
.endm

.macro makeLsInstruction mode, elMode, name, opcode, size, rangemin, rangemax
    # Overloads:
    #   op vt, element, offset, base
    #   op vt, offset, base
    #   op vt.e, offset, base
    .macro \name vt, elementOrOffset, offsetOrBase, baseOrEmpty
        \elMode\()VectorElements

        .ifnb \baseOrEmpty
            lsInstruction \mode, \opcode, \vt, \elementOrOffset, \offsetOrBase, \baseOrEmpty, \size, \rangemin, \rangemax
            .exitm
        .endif

        \elMode\()VectorAccessors
        encodeVectorRegs

        .iflt (\vt)
            .error "Invalid element"
            .exitm
        .endif

        # Dot-style syntax is encoded as numbers >= 0x200
        .if (\vt >= 0x200)
            regVectorAccessors
            lsInstruction \mode, \opcode, reg.\vt, (\vt & 0xF), \elementOrOffset, \offsetOrBase, \size, \rangemin, \rangemax
        .else
            lsInstruction \mode, \opcode, \vt, 0, \elementOrOffset, \offsetOrBase, \size, \rangemin, \rangemax
        .endif
    .endm
.endm

.macro makeLsInstructionQuad mode, elMode, name, opcode
    makeLsInstruction \mode, \elMode, \name, \opcode, 16, -1024, 1008
.endm

.macro makeLsInstructionDouble mode, elMode, name, opcode
    makeLsInstruction \mode, \elMode, \name, \opcode, 8, -512, 504
.endm

.macro makeLsInstructionLong mode, elMode, name, opcode
    makeLsInstruction \mode, \elMode, \name, \opcode, 4, -256, 252
.endm

.macro makeLsInstructionShort mode, elMode, name, opcode
    makeLsInstruction \mode, \elMode, \name, \opcode, 2, -128, 126
.endm

.macro makeLsInstructionByte mode, elMode, name, opcode
    makeLsInstruction \mode, \elMode, \name, \opcode, 1, -64, 63
.endm

/** @brief Load Byte into Vector Register */
makeLsInstructionByte load, byte, lbv, 0b00000
/** @brief Load Double into Vector Register */
makeLsInstructionDouble load, byte, ldv, 0b00011
/** @brief Load Packed Fourth into Vector Register */
makeLsInstructionQuad load, byte, lfv, 0b01001
/** @brief Load Packed Half into Vector Register */
makeLsInstructionQuad load, byte, lhv, 0b01000
/** @brief Load Long into Vector Register */
makeLsInstructionLong load, byte, llv, 0b00010
/** @brief Load Packed Bytes into Vector Register */
makeLsInstructionDouble load, lane, lpv, 0b00110
/** @brief Load Quad into Vector Register */
makeLsInstructionQuad load, byte, lqv, 0b00100
/** @brief Load Quad (Rest) into Vector Register */
makeLsInstructionQuad load, byte, lrv, 0b00101
/** @brief Load Short into Vector Register */
makeLsInstructionShort load, byte, lsv, 0b00001
/** @brief Load Transpose into Vector Register */
makeLsInstructionQuad load, laneAsByte, ltv, 0b01011
/** @brief Load Unsigned Packed into Vector Register */
makeLsInstructionDouble load, lane, luv, 0b00111

/** @brief Store Byte from Vector Register */
makeLsInstructionByte store, byte, sbv, 0b00000
/** @brief Store Double from Vector Register */
makeLsInstructionDouble store, byte, sdv, 0b00011
/** @brief Store Packed Fourth from Vector Register */
makeLsInstructionQuad store, byte, sfv, 0b01001
/** @brief Store Packed Half from Vector Register */
makeLsInstructionQuad store, byte, shv, 0b01000
/** @brief Store Long from Vector Register */
makeLsInstructionLong store, byte, slv, 0b00010
/** @brief Store Packed Bytes from Vector Register */
makeLsInstructionDouble store, lane, spv, 0b00110
/** @brief Store Quad from Vector Register */
makeLsInstructionQuad store, byte, sqv, 0b00100
/** @brief Store Quad (Rest) from Vector Register */
makeLsInstructionQuad store, byte, srv, 0b00101
/** @brief Store Short from Vector Register */
makeLsInstructionShort store, byte, ssv, 0b00001
/** @brief Store Transpose from Vector Register */
makeLsInstructionQuad store, laneAsByte, stv, 0b01011
/** @brief Store Unsigned Packed from Vector Register */
makeLsInstructionDouble store, lane, suv, 0b00111
/** @brief Store Wrapped vector from Vector Register */
makeLsInstructionQuad store, byte, swv, 0b01010

.macro mxc2 opcode, reg, vreg, element
    .ifgt (\element >> 4)
        .error "Invalid element"
        .exitm
    .endif
    .long (0x12 << 26 | \opcode << 21 | \vreg << 11 | \reg << 16 | \element << 7)
.endm

.macro makeMxc2Op name, opcode
    # Overloads:
    #   op reg, vreg, element
    #   op reg, vreg
    #   op reg, vreg.e
    .macro \name reg, vreg, element=0
        hexRegisters
        hexGeneralRegisters
        byteVectorElements
        byteVectorAccessors

        .iflt (\vreg)
            .error "Invalid element"
            .exitm
        .endif

        # Dot-style syntax is encoded as numbers >= 0x200
        .if (\vreg >= 0x200)
            mxc2 \opcode, hex.\reg, ((\vreg >> 4) & 0x1F), (\vreg & 0xF)
        .else
            mxc2 \opcode, hex.\reg, \vreg, \element
        .endif
    .endm
.endm

makeMxc2Op mtc2, 0x4
makeMxc2Op mfc2, 0x0

##################################################
# Vector shift pseudo-opcodes
##################################################
# Helper macros for vector shifts.
# How to use:
#   1) Call macro "vsll_data" and/or "vsll8_data" in the data segment to define
#      the required constants.
#   2) Call macro "setup_vsll <VREG>" / "setup_vssl8 <VREG>" in code, passing
#      one vreg that will hold shift constants.
#   3) Use virtual opcodes:
#        "vsll <VDST>, <VSRC>, <QTY>" to perform a left shift up to 7 bits
#        "vsll8 <VDST>, <VSRC>, <QTY>" to perform a left shift from 8 to 15 bits
#        "vsrl <VDST>, <VSRC>, <QTY>" to perform a logical right shift up to 7 bits
#        "vsrl8 <VDST>, <VSRC>, <QTY>" to perform a logical right shift from 8 to 15 bits
#        "vsra <VDST>, <VSRC>, <QTY>" to perform an arithmetic right shift up to 7 bits
#        "vsra8 <VDST>, <VSRC>, <QTY>" to perform a arithmetic right shift from 8 to 15 bits
#
.macro vsll_data
        .align 4
    V_SHIFT:    .half 0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1
.endm

.macro setup_vsll vshiftreg, emitload
    .ifnb \emitload
        .ifgt \emitload
            .set noat
            la $1,%lo(V_SHIFT)
            lqv \vshiftreg,0,  0,$1
            .set at
        .endif
    .else
        .set noat
        la $1,%lo(V_SHIFT)
        lqv \vshiftreg,0,  0,$1
        .set at
    .endif

    .macro vsll vdstreg, vsrcreg, qty
        .if (\qty == 7)
            vmudn \vdstreg, \vsrcreg, \vshiftreg, e(0)
        .elseif (\qty == 6)
            vmudn \vdstreg, \vsrcreg, \vshiftreg, e(1)
        .elseif (\qty == 5)
            vmudn \vdstreg, \vsrcreg, \vshiftreg, e(2)
        .elseif (\qty == 4)
            vmudn \vdstreg, \vsrcreg, \vshiftreg, e(3)
        .elseif (\qty == 3)
            vmudn \vdstreg, \vsrcreg, \vshiftreg, e(4)
        .elseif (\qty == 2)
            vmudn \vdstreg, \vsrcreg, \vshiftreg, e(5)
        .elseif (\qty == 1)
            vmudn \vdstreg, \vsrcreg, \vshiftreg, e(6)
        .elseif (\qty == 0)
            vmudn \vdstreg, \vsrcreg, \vshiftreg, e(7)
        .elseif (\qty >= 8 && \qty <= 15)
            .error "Use vsll8 for quantities in range 8-15"
        .else
            .error "Invalid quantity in vsll"
        .endif
    .endm

    .macro vsrl8 vdstreg, vsrcreg, qty
        .if (\qty == 9)
            vmudl \vdstreg, \vsrcreg, \vshiftreg, e(0)
        .elseif (\qty == 10)
            vmudl \vdstreg, \vsrcreg, \vshiftreg, e(1)
        .elseif (\qty == 11)
            vmudl \vdstreg, \vsrcreg, \vshiftreg, e(2)
        .elseif (\qty == 12)
            vmudl \vdstreg, \vsrcreg, \vshiftreg, e(3)
        .elseif (\qty == 13)
            vmudl \vdstreg, \vsrcreg, \vshiftreg, e(4)
        .elseif (\qty == 14)
            vmudl \vdstreg, \vsrcreg, \vshiftreg, e(5)
        .elseif (\qty == 15)
            vmudl \vdstreg, \vsrcreg, \vshiftreg, e(6)
        .elseif (\qty >= 1 && \qty <= 8)
            .error "Use vsrl for quantities in range 1-8"
        .else
            .error "Invalid quantity in vsrl8"
        .endif
    .endm

    .macro vsra8 vdstreg, vsrcreg, qty
        .if (\qty == 9)
            vmudm \vdstreg, \vsrcreg, \vshiftreg, e(0)
        .elseif (\qty == 10)
            vmudm \vdstreg, \vsrcreg, \vshiftreg, e(1)
        .elseif (\qty == 11)
            vmudm \vdstreg, \vsrcreg, \vshiftreg, e(2)
        .elseif (\qty == 12)
            vmudm \vdstreg, \vsrcreg, \vshiftreg, e(3)
        .elseif (\qty == 13)
            vmudm \vdstreg, \vsrcreg, \vshiftreg, e(4)
        .elseif (\qty == 14)
            vmudm \vdstreg, \vsrcreg, \vshiftreg, e(5)
        .elseif (\qty == 15)
            vmudm \vdstreg, \vsrcreg, \vshiftreg, e(6)
        .elseif (\qty >= 1 && \qty <= 8)
            .error "Use vsra for quantities in range 1-8"
        .else
            .error "Invalid quantity in vsra8"
        .endif
    .endm

.endm

.macro vsll8_data
        .align 4
    V_SHIFT8:   .half 0x8000, 0x4000, 0x2000, 0x1000, 0x800, 0x400, 0x200, 0x100
.endm

.macro setup_vsll8 vshiftreg, emitload
    .ifnb \emitload
        .ifgt \emitload
            .set noat
            la $1,%lo(V_SHIFT8)
            lqv \vshiftreg,0,  0,$1
            .set at
        .endif
    .else
        .set noat
        la $1,%lo(V_SHIFT8)
        lqv \vshiftreg,0,  0,$1
        .set at
    .endif

    .macro vsll8 vdstreg, vsrcreg, qty
        .if (\qty == 15)
            vmudn \vdstreg, \vsrcreg, \vshiftreg, e(0)
        .elseif (\qty == 14)
            vmudn \vdstreg, \vsrcreg, \vshiftreg, e(1)
        .elseif (\qty == 13)
            vmudn \vdstreg, \vsrcreg, \vshiftreg, e(2)
        .elseif (\qty == 12)
            vmudn \vdstreg, \vsrcreg, \vshiftreg, e(3)
        .elseif (\qty == 11)
            vmudn \vdstreg, \vsrcreg, \vshiftreg, e(4)
        .elseif (\qty == 10)
            vmudn \vdstreg, \vsrcreg, \vshiftreg, e(5)
        .elseif (\qty == 9)
            vmudn \vdstreg, \vsrcreg, \vshiftreg, e(6)
        .elseif (\qty == 8)
            vmudn \vdstreg, \vsrcreg, \vshiftreg, e(7)
        .elseif (\qty >= 0 && \qty <= 7)
            .error "Use vsll for quantities in range 0-7"
        .else
            .error "Invalid quantity in vsll8"
        .endif
    .endm

    .macro vsrl vdstreg, vsrcreg, qty
        .if (\qty == 1)
            vmudl \vdstreg, \vsrcreg, \vshiftreg, e(0)
        .elseif (\qty == 2)
            vmudl \vdstreg, \vsrcreg, \vshiftreg, e(1)
        .elseif (\qty == 3)
            vmudl \vdstreg, \vsrcreg, \vshiftreg, e(2)
        .elseif (\qty == 4)
            vmudl \vdstreg, \vsrcreg, \vshiftreg, e(3)
        .elseif (\qty == 5)
            vmudl \vdstreg, \vsrcreg, \vshiftreg, e(4)
        .elseif (\qty == 6)
            vmudl \vdstreg, \vsrcreg, \vshiftreg, e(5)
        .elseif (\qty == 7)
            vmudl \vdstreg, \vsrcreg, \vshiftreg, e(6)
        .elseif (\qty == 8)
            vmudl \vdstreg, \vsrcreg, \vshiftreg, e(7)
        .elseif (\qty >= 9 && \qty <= 15)
            .error "Use vsrl8 for quantities in range 9-15"
        .else
            .error "Invalid quantity in vsrl"
        .endif
    .endm

    .macro vsra vdstreg, vsrcreg, qty
        .if (\qty == 1)
            vmudm \vdstreg, \vsrcreg, \vshiftreg, e(0)
        .elseif (\qty == 2)
            vmudm \vdstreg, \vsrcreg, \vshiftreg, e(1)
        .elseif (\qty == 3)
            vmudm \vdstreg, \vsrcreg, \vshiftreg, e(2)
        .elseif (\qty == 4)
            vmudm \vdstreg, \vsrcreg, \vshiftreg, e(3)
        .elseif (\qty == 5)
            vmudm \vdstreg, \vsrcreg, \vshiftreg, e(4)
        .elseif (\qty == 6)
            vmudm \vdstreg, \vsrcreg, \vshiftreg, e(5)
        .elseif (\qty == 7)
            vmudm \vdstreg, \vsrcreg, \vshiftreg, e(6)
        .elseif (\qty == 8)
            vmudm \vdstreg, \vsrcreg, \vshiftreg, e(7)
        .elseif (\qty >= 9 && \qty <= 15)
            .error "Use vsra8 for quantities in range 9-15"
        .else
            .error "Invalid quantity in vsra"
        .endif
    .endm
.endm

##################################################
# emux
##################################################

.macro emux reg, code
    .word (0<<26) | (0x36 << 0) | (\reg << 21) | (\reg << 16) | (\code << 6)
.endm

.macro emux_trace_start
    emux 0, 0x20
.endm

.macro emux_trace_stop
    emux 0, 0x22
.endm

.macro emux_trace_count num_insn
    .set noat
    li $1, \num_insn
    emux 1, 0x21
    .set at
.endm

.macro emux_breakpoint
    emux 0, 0x10
.endm

.macro emux_log_string str
    .data
__emux_log_string_\@: .asciiz "\str"
    .previous
    .set noat
    la $1, __emux_log_string_\@
    emux 1, 0x31
    .set at
.endm

.macro __emux_dump_gpr_rec reg0, regs:vararg
    hexGeneralRegisters
    .set gpr_mask, gpr_mask + (1 << hex.\reg0)
    .ifnb \regs
        __emux_dump_gpr_rec \regs
    .endif
.endm

.macro emux_dump_gpr regs:vararg
    .ifb \regs
        .set gpr_mask, 0xFFFFFFFF
    .else
        .set gpr_mask, 0
        __emux_dump_gpr_rec \regs
    .endif
    .set noat
    li $1, gpr_mask
    emux 1, 0x40
    .set at
.endm

.macro __emux_dump_vpr_rec reg0, regs:vararg
    hexRegisters
    .set vpr_mask, vpr_mask + (1 << \reg0)
    .ifnb \regs
        __emux_dump_vpr_rec \regs
    .endif
.endm

.macro emux_dump_vpr regs:vararg
    .ifb \regs
        .set vpr_mask, 0xFFFFFFFF
    .else
        .set vpr_mask, 0
        __emux_dump_vpr_rec \regs
    .endif
    .set noat
    li $1, vpr_mask
    emux 1, 0x42
    .set at
.endm

.macro emux_dump_vpr_accum regs:vararg
    .ifb \regs
        .set vpr_mask, 0xFFFFFFFF
    .else
        .set vpr_mask, 0
        __emux_dump_vpr_rec \regs
    .endif
    .set noat
    li $1, vpr_mask
    emux 1, 0x46
    .set at
.endm

.macro __emux_dump_cop0_rec reg0, regs:vararg
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
        __emux_dump_cop0_rec \regs
    .endif
.endm

.macro emux_dump_cop0 regs:vararg
    .ifb \regs
        .set cop0_mask, 0xFFFFFFFF
    .else
        .set cop0_mask, 0
        __emux_dump_cop0_rec \regs
    .endif
    .set noat
    li $1, cop0_mask
    emux 1, 0x41
    .set at
.endm

##################################################
# Other psuedo-ops
##################################################

# jal_and_j implements a tail-call-optimized subroutine call followed by
# a jump. In other words, this:
#
#   jal_and_j func, label
#
# is equivalent to:
#
#   jal func
#   nop
#   j label
#   nop
#
# but using only two instructions instead of four.
.macro jal_and_j func, label
    j \func
    li ra, %lo(\label)
.endm


##################################################
# Hardware registers
##################################################

#define SP_STATUS_HALTED        (1<<0)
#define SP_STATUS_BROKE         (1<<1)
#define SP_STATUS_DMA_BUSY      (1<<2)
#define SP_STATUS_DMA_FULL      (1<<3)
#define SP_STATUS_IO_FULL       (1<<4)
#define SP_STATUS_SSTEP         (1<<5)
#define SP_STATUS_INTR_BREAK    (1<<6)
#define SP_STATUS_SIG0          (1<<7)
#define SP_STATUS_SIG1          (1<<8)
#define SP_STATUS_SIG2          (1<<9)
#define SP_STATUS_SIG3          (1<<10)
#define SP_STATUS_SIG4          (1<<11)
#define SP_STATUS_SIG5          (1<<12)
#define SP_STATUS_SIG6          (1<<13)
#define SP_STATUS_SIG7          (1<<14)

#define SP_WSTATUS_CLEAR_HALT          (1<<0)
#define SP_WSTATUS_SET_HALT            (1<<1)
#define SP_WSTATUS_CLEAR_BROKE         (1<<2)
#define SP_WSTATUS_CLEAR_INTR          (1<<3)
#define SP_WSTATUS_SET_INTR            (1<<4)
#define SP_WSTATUS_CLEAR_SSTEP         (1<<5)
#define SP_WSTATUS_SET_SSTEP           (1<<6)
#define SP_WSTATUS_CLEAR_INTR_BREAK    (1<<7)
#define SP_WSTATUS_SET_INTR_BREAK      (1<<8)
#define SP_WSTATUS_CLEAR_SIG0          (1<<9)
#define SP_WSTATUS_SET_SIG0            (1<<10)
#define SP_WSTATUS_CLEAR_SIG1          (1<<11)
#define SP_WSTATUS_SET_SIG1            (1<<12)
#define SP_WSTATUS_CLEAR_SIG2          (1<<13)
#define SP_WSTATUS_SET_SIG2            (1<<14)
#define SP_WSTATUS_CLEAR_SIG3          (1<<15)
#define SP_WSTATUS_SET_SIG3            (1<<16)
#define SP_WSTATUS_CLEAR_SIG4          (1<<17)
#define SP_WSTATUS_SET_SIG4            (1<<18)
#define SP_WSTATUS_CLEAR_SIG5          (1<<19)
#define SP_WSTATUS_SET_SIG5            (1<<20)
#define SP_WSTATUS_CLEAR_SIG6          (1<<21)
#define SP_WSTATUS_SET_SIG6            (1<<22)
#define SP_WSTATUS_CLEAR_SIG7          (1<<23)
#define SP_WSTATUS_SET_SIG7            (1<<24)

// ***************************************************
// Deprected SP_WSTATUS naming. DO NOT USE IN NEW CODE
// ***************************************************
#define SP_WSTATUS_RESET_HALT          (1<<0)
#define SP_WSTATUS_RESET_BROKE         (1<<2)
#define SP_WSTATUS_RESET_RSP_INTERRUPT (1<<3)
#define SP_WSTATUS_SET_RSP_INTERRUPT   (1<<4)
#define SP_WSTATUS_RESET_SINGLE_STEP   (1<<5)
#define SP_WSTATUS_SET_SINGLE_STEP     (1<<6)
#define SP_WSTATUS_RESET_INTR_ON_BREAK (1<<7)
#define SP_WSTATUS_SET_INTR_ON_BREAK   (1<<8)
#define SP_WSTATUS_RESET_SIG0          (1<<9)
#define SP_WSTATUS_RESET_SIG1          (1<<11)
#define SP_WSTATUS_RESET_SIG2          (1<<13)
#define SP_WSTATUS_RESET_SIG3          (1<<15)
#define SP_WSTATUS_RESET_SIG4          (1<<17)
#define SP_WSTATUS_RESET_SIG5          (1<<19)
#define SP_WSTATUS_RESET_SIG6          (1<<21)
#define SP_WSTATUS_RESET_SIG7          (1<<23)
// ***************************************************


#define DP_STATUS_DMEM_DMA             (1<<0)
#define DP_STATUS_FREEZE               (1<<1)
#define DP_STATUS_FLUSH                (1<<2)
#define DP_STATUS_GCLK_ALIVE           (1<<3)
#define DP_STATUS_TMEM_BUSY            (1<<4)
#define DP_STATUS_PIPE_BUSY            (1<<5)
#define DP_STATUS_BUSY                 (1<<6)
#define DP_STATUS_BUFFER_READY         (1<<7)
#define DP_STATUS_DMA_BUSY             (1<<8)
#define DP_STATUS_END_VALID            (1<<9)
#define DP_STATUS_START_VALID          (1<<10)

#define DP_WSTATUS_RESET_XBUS_DMEM_DMA  (1<<0)
#define DP_WSTATUS_SET_XBUS_DMEM_DMA    (1<<1)
#define DP_WSTATUS_RESET_FREEZE         (1<<2)
#define DP_WSTATUS_SET_FREEZE           (1<<3)
#define DP_WSTATUS_RESET_FLUSH          (1<<4)
#define DP_WSTATUS_SET_FLUSH            (1<<5)
#define DP_WSTATUS_RESET_TMEM_COUNTER   (1<<6)
#define DP_WSTATUS_RESET_PIPE_COUNTER   (1<<7)
#define DP_WSTATUS_RESET_CMD_COUNTER    (1<<8)
#define DP_WSTATUS_RESET_CLOCK_COUNTER  (1<<9)

#define COP0_DMA_SPADDR         $0
#define COP0_DMA_RAMADDR        $1
#define COP0_DMA_READ           $2
#define COP0_DMA_WRITE          $3
#define COP0_SP_STATUS          $4
#define COP0_DMA_FULL           $5
#define COP0_DMA_BUSY           $6
#define COP0_SEMAPHORE          $7
#define COP0_DP_START           $8
#define COP0_DP_END             $9
#define COP0_DP_CURRENT         $10
#define COP0_DP_STATUS          $11
#define COP0_DP_CLOCK           $12
#define COP0_DP_BUSY            $13
#define COP0_DP_PIPE_BUSY       $14
#define COP0_DP_TMEM_BUSY       $15

#define COP2_CTRL_VCO           $0
#define COP2_CTRL_VCC           $1
#define COP2_CTRL_VCE           $2


##################################################
# RSP_DMA.INC macros
##################################################

# Compose the RSP DMA size register with immediate constants "width" and "height".
# For a linear transfer, use height=1.
# Notice that if height!=1, a pitch must be specified in the register. This
# version of the macro is useful for the common case in which the pitch is not
# a compile-time constant and thus will need to be inserted at runtime.
#define DMA_SIZE(width, height)          (((width)-1) | (((height)-1)<<12))

# Compose the RSP DMA size register with immediate constants
# "width", "height", and "pitch".
# It is similar to DMA_SIZE, but it can be used when the pitch is a compile-time
# constant.
#define DMA_SIZE3(width, height, pitch)  (((width)-1) | (((height)-1)<<12) | (((pitch)-(width))<<20))

# DMAExec operation macros. Pass one of these in t2 to DMAExec
#define DMA_IN_ASYNC     0x00000000
#define DMA_OUT_ASYNC    0xFFFF8000
#define DMA_IN           (DMA_IN_ASYNC  | SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL)
#define DMA_OUT          (DMA_OUT_ASYNC | SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL)


##################################################
# RSP_ASSERT.INC macros
##################################################

#ifndef NDEBUG
    .macro assert code
        j assertion_failed
        .set noat
        lui $1, \code
        .set at
    .endm
    .macro assert_eq v0, v1, code
        bne \v0, \v1, assertion_failed
        .set noat
        lui $1, \code
        .set at
    .endm
    .macro assert_ne v0, v1, code
        beq \v0, \v1, assertion_failed
        .set noat
        lui $1, \code
        .set at
    .endm
    .macro assert_ge v0, v1, code
        blt \v0, \v1, assertion_failed
        .set noat
        lui $1, \code
        .set at
    .endm
    .macro assert_gt v0, v1, code
        ble \v0, \v1, assertion_failed
        .set noat
        lui $1, \code
        .set at
    .endm
    .macro assert_lt v0, v1, code
        bge \v0, \v1, assertion_failed
        .set noat
        lui $1, \code
        .set at
    .endm
    .macro assert_le v0, v1, code
        bgt \v0, \v1, assertion_failed
        .set noat
        lui $1, \code
        .set at
    .endm

#else
    .macro assert code
    .endm
    .macro assert_eq v0, v1, code
    .endm
    .macro assert_ne v0, v1, code
    .endm
    .macro assert_ge v0, v1, code
    .endm
    .macro assert_gt v0, v1, code
    .endm
    .macro assert_lt v0, v1, code
    .endm
#endif

#endif /* RSP_INC */
