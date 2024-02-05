#define DMA_RACE_MARGIN     16
#define PI_STATUS           0xA4600010
#define PI_DRAM_ADDR        0xA4600000

#define inbuf       $a0
#define outbuf      $a2
#define nlit        $t2
#define cc          $t3
#define cc_count    $t4
#define match_off   $t5
#define match_len   $t6
#define outbuf_orig $t7
#define dma_ptr     $t8

#define ra2         $a1
#define ra3         $a3

# Read one bit from CC, and jump to target if it matches
# the specified value
.macro cc_check value, target
    bltzal cc_count, .Lreadcc
     addiu cc_count, -1
    .if \value != 0
        bltz cc, \target
    .else
        bgez cc, \target
    .endif
    sll cc, 1
.endm

# Peek one bit from CC, and run next opcode if it matches
# the specified value. The bit is not removed from CC.
.macro cc_peek value
    bltzal cc_count, .Lreadcc
     addiu cc_count, -1
    .if \value != 0
        bltzl cc, 1f
    .else
        bgezl cc, 1f
    .endif
1:
.endm

    .section .text.decompress_aplib_full_fast
	.p2align 5
    .globl decompress_aplib_full_fast 
    .func decompress_aplib_full_fast
    .set at
    .set noreorder

decompress_aplib_full_fast:
    move ra3, $ra
    move outbuf_orig, outbuf
    bal .Lwaitdma
     li dma_ptr, 0
    b .Lcmd_literal
     li cc_count, -1

    # Main copy-match loop. This is the basic LZ constrcut, where
    # we copy match_len bytes from match_off bytes before the current
    # outbuf pointer. Notice that match_off can be less than match_len
    # in which case it is mandatory to copy byte by byte for correct
    # behavior (memmove() vs memcpy()).
    # We implement 3 different loops for optimization purposes:
    #  - 1-byte copy loop: copy 1 byte at a time
    #  - 8-byte copy loop: copy 8 bytes at a time
    #  - memset loop: read one 1 byte (match_off=1), memset 8 bytes at a time
    # To make the second and third loop more generally useful, we
    # allow the code to write past the end of the output buffer (up to 8 bytes).
.Lmatch:
    blt match_off, match_len, .Lmatch1_loop_check
     sub $v0, outbuf, match_off
.Lmatch8_loop:                                  # 8-byte copy loop
    ldl $t0, 0($v0)                             # load 8 bytes
    ldr $t0, 7($v0)
    addiu $v0, 8
    sdl $t0, 0(outbuf)                          # store 8 bytes
    sdr $t0, 7(outbuf)
    addiu match_len, -8
    bgtz match_len, .Lmatch8_loop               # check we went past match_len
     addiu outbuf, 8
    b .Lloop_lit2                               # jump to main loop
     addu outbuf, match_len                     # adjust pointer remove extra bytes

.Lmatch1_memset:                                # prepare memset loop (value in t0)
    dsll $t1, $t0, 8                            # duplicate the LSB into all bytes
    or $t0, $t1
    dsll $t1, $t0, 16
    or $t0, $t1
    dsll $t1, $t0, 32
    or $t0, $t1
.Lmatch1_memset_loop:                           # memset loop
    sdl $t0, 0(outbuf)                          # store 8 bytes
    sdr $t0, 7(outbuf)                           
    addiu match_len, -8                         # adjust match_len
    bgtz match_len, .Lmatch1_memset_loop        # check we went past match_len
     addiu outbuf, 8
    b .Lloop_lit2                               # jump to main loop
     addu outbuf, match_len                     # adjust pointer remove extra bytes

.Lmatch1_loop_check:                            # 1-byte copy loop
    beq match_off, 1, .Lmatch1_memset
.Lmatch1_loop:                                  # 1-byte copy loop
    lbu $t0, 0($v0)                             # load 1 byte
    addiu $v0, 1
    sb $t0, 0(outbuf)                           # store 1 byte
    addiu match_len, -1
    bgtz match_len, .Lmatch1_loop               # check we went past match_len
     addiu outbuf, 1


    # Aplib main loop. This is the main loop of the decompressor.
    # We pattern-match the bits in CC to determine the command.
.Lloop_lit2:
    li nlit, 2
.Lloop:                                         # main loop
    #tne ra, ra, 0x10
    sub $t0, inbuf, dma_ptr                     # check if we need to wait for dma
    bgezal $t0, .Lwaitdma                       # if inbuf >= dma_ptr, wait for dma
     nop
    cc_check 0, .Lcmd_literal                   # 0xx => literal
    cc_check 0, .Lcmd_offset8                   # 10x => offset 8
    cc_check 0, .Lcmd_offset7                   # 110 => offset 7
.Lcmd_offset4:                                  # 111 => offset 4
    li nlit, 3
    li $v0, 0

    cc_peek 1                                   # if next CC bit is 1
     ori $v0, 0x8                               #  set bit 0x8 in v0
    sll cc, 1                                   # shift CC
    cc_peek 1                                   # if next CC bit is 1
     ori $v0, 0x4                               #  set bit 0x4 in v0
    sll cc, 1                                   # shift CC
    cc_peek 1                                   # if next CC bit is 1
     ori $v0, 0x2                               #  set bit 0x2 in v0
    sll cc, 1                                   # shift CC
    cc_peek 1                                   # if next CC bit is 1
     ori $v0, 0x1                               #  set bit 0x1 in v0
    sll cc, 1                                   # shift CC

    beqz $v0, .Loutz                            # if v0 == 0, store zero into outbuf
     li $t0, 0
    sub $v0, outbuf, $v0                        # else v0 is the match offset
    lbu $t0, 0($v0)                             # load byte from match offset
.Loutz:
    sb $t0, 0(outbuf)                           # store byte
    b .Lloop                                    # back to main loop
     addiu outbuf, 1

.Lcmd_literal:                                  # literal command
    lbu $t0, 0(inbuf)                           # load byte from inbuf
    addiu inbuf, 1
    sb $t0, 0(outbuf)                            # store byte
    addiu outbuf, 1
    b .Lloop                                    # back to main loop
     li nlit, 3

.Lcmd_offset7:                                  # offset 7 command
    lbu $t0, 0(inbuf)                           # load byte from inbuf
    addiu inbuf, 1
    beqz $t0, .Ldone                            # if byte == 0, we're done
     srl match_off, $t0, 1                      # match_offset is bits 1..7
    andi $t0, 1                                 # match_len is 2 + bit 0
    sub $v0, outbuf, match_off                  # compute match pointer
    lbu $t1, 0($v0)                             # load byte from match pointer
    addiu outbuf, 2
    sb $t1, -2(outbuf)                          # store byte
    lbu $t1, 1($v0)                             # load byte from match pointer + 1
    beqz $t0, .Lloop_lit2                       # if match_len == 2, back to main loop
     sb $t1, -1(outbuf)                         # store byte
    lbu $t1, 2($v0)                             # load byte from match pointer + 2
    addiu outbuf, 1
    b .Lloop_lit2                               # back to main loop
     sb $t1, -1(outbuf)                         # store byte

.Lcmd_offset8:                                  # offset 8 command
    #tne ra, ra, 0x10
    bal .Lreadgamma                             # read gamma-encoded value
     nop
    sub $v0, nlit                               # if value-nlit < 0, it's a repmatch
    bltz $v0, .Lrepmatch                        # so jump to repmatch code
     nop
    sll match_off, $v0, 8                       # else, it is the MSB of match_off
    lbu $t0, 0(inbuf)                           # load byte from inbuf (LSB of match off)
    addiu inbuf, 1
    bal .Lreadgamma                             # read gamma-encoded value (match_len)
     or match_off, $t0                          # compute match_off
    bltl match_off, 128, 1f                     
     addiu $v0, 2
1:  bgel match_off, 32000, 1f
     addiu $v0, 1
1:  bgel match_off, 1280, 1f
     addiu $v0, 1
1:
.Ljump_match:                                   
    b .Lmatch                                    # jump to match code
     move match_len, $v0
.Lrepmatch:                                      # repmatch: just read mach_len
    bal .Lreadgamma                              # read gamma-encoded value
     addiu $ra, -(1f - .Ljump_match)             # when done, return to jump_match
1:

.Lreadgamma:                                     # read gamma-encoded value
    move ra2, $ra
    li $v0, 1                                     # Start from 1
.Lreadgamma_loop:
    sll $v0, 1                                   # Shift to make space for new bit
    cc_check 0, .Lreadgamma_check                # Read one bit from CC
    ori $v0, 1                                   # If 1, set the LSB
.Lreadgamma_check:
    cc_check 1, .Lreadgamma_loop                 # if next CC bit is 0, we are done
    jr ra2
     nop

.Lreadcc:                                        # read new CC byte 
    lwl cc, 0(inbuf)                             # load current byte into MSB
    addiu inbuf, 1                               # (other read bytes will be ignored)
    jr $ra
     li cc_count, 6                              # reset CC counter

.Lwaitdma:
    li $t1, 0x80000000 - DMA_RACE_MARGIN
.Lwaitdma_loop:
    lw $t0, (PI_STATUS)
    andi $t0, 1
    beqz $t0, .Lwaitdma_end
     li dma_ptr, 0xffffffff
    lw dma_ptr, (PI_DRAM_ADDR)
    andi $t0, dma_ptr, 0xF
    xor dma_ptr, $t0
    addu dma_ptr, $t1
    ble dma_ptr, inbuf, .Lwaitdma_loop
     nop
.Lwaitdma_end:
    jr $ra
     nop

.Ldone:
    jr ra3
     sub $v0, outbuf, outbuf_orig

    .endfunc
