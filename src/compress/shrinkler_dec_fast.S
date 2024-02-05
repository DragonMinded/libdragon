##############################################
#  Shrinkler - Fast decompressor in assembly
#
#  This decompressor implements the following variant of Shrinkler:
#    * Forward decoding
#    * Parity mask: 1. This has been tested to provide
#      the best compression ratio on average.
##############################################

#define inbuf           $a0
#define outbuf          $a2

#define intervalsize    $t2   
#define intervalvalue   $t3
#define nbits           $t4
#define match_off       $t5
#define prev_match      $t6
#define lit_len         $t7
#define match_len       $t7
#define num_len         $t7
#define ctx             $t8
#define outbuf_orig     $t9
#define parity          $v1

#define ra2             $a1
#define lit_ctx         $a1
#define ra3             $a3

#define ROUND_UP(n,d)   (((n) + (d) - 1) / (d) * (d))

#define DMA_RACE_MARGIN             16
#define PI_REGS             0xA4600000
#define PI_STATUS                 0x10
#define PI_DRAM_ADDR              0x00

#define ADJUST_SHIFT                4
#define NUM_SINGLE_CONTEXTS         1
#define NUM_CONTEXT_GROUPS          4
#define CONTEXT_GROUP_SIZE          256
#define NUM_CONTEXTS                (NUM_SINGLE_CONTEXTS + NUM_CONTEXT_GROUPS * CONTEXT_GROUP_SIZE)

#define CONTEXT_KIND             (0 + 1)
#define CONTEXT_REPEATED         (-1 + 1)
#define CONTEXT_GROUP_OFFSET     ((2<<8) + 1)
#define CONTEXT_GROUP_LENGTH     ((3<<8) + 1)

    .section .text.decompress_shrinkler_full_fast
	.p2align 5
    .globl decompress_shrinkler_full_fast 
    .func decompress_shrinkler_full_fast
    .set at
    .set noreorder

decompress_shrinkler_full_fast:
    move outbuf_orig, outbuf
    addiu $t1, $sp, -16
    addiu $sp, $sp, -ROUND_UP(NUM_CONTEXTS*2, 8)-16
    move $t0, $sp
    li $t2, 0x80008000
.Linit_contexts_loop:
    sw $t2, 0($t0)
    sw $t2, 4($t0)
    blt $t0, $t1, .Linit_contexts_loop
     addiu $t0, 8
    # For Shrinkler, we don't do real DMA racing becuase the algorithm
    # is so slow that PI DMA is always faster. The only real risk is
    # at the very beginning: if this code is already in cache, we might
    # fetch the first word before the DMA has written it. So we just
    # wait for the DMA to write a little bit, and then we start decoding.
    li $t2, PI_REGS
.Lwaitdma_loop:
    lw $t0, PI_STATUS($t2)
    andi $t0, 1
    beqz $t0, .Linitial_setup
    lw $t1, PI_DRAM_ADDR($t2)
    addu $t1, 0x80000000 - DMA_RACE_MARGIN
    ble $t1, inbuf, .Lwaitdma_loop
.Linitial_setup:
     move ra3, $ra
    lwu intervalvalue, 0(inbuf)
    addiu inbuf, 4
    dsll intervalvalue, 31
    li nbits, 1
    li intervalsize, 0x8000
    li parity, CONTEXT_KIND*2
    b .Lliteral
     li $v0, 1

.Lloop:
    li $v0, 0
    bal .Llzdecode
     addu ctx, parity, $sp
    bnez $v0, .Lmatch
     xori $v0, 1

.Lliteral:
    li prev_match, 0
    li lit_len, 7
    addu lit_ctx, parity, $sp
.Lliteral_loop:
    sll $v0, 1
    bal .Llzdecode
     addu ctx, lit_ctx, $v0
    bnez lit_len, .Lliteral_loop
     addiu lit_len, -1

    xori parity, 0x200
    sb $v0, 0(outbuf)
    b .Lloop
     addiu outbuf, 1

.Lmatch:
    bgezal prev_match, .Llzdecode
     addiu ctx, $sp, CONTEXT_REPEATED*2 
    sll $v0, 31
    addiu ctx, $sp, CONTEXT_GROUP_OFFSET*2
    bgezal $v0, .Llzdecode_number
     addiu $v0, match_off, 2
    addiu $v0, -2
    beqz $v0, .Lend
     move match_off, $v0
    bal .Llzdecode_number
     addiu ctx, $sp, CONTEXT_GROUP_LENGTH*2
    move match_len, $v0
    andi $t0, match_len, 1
    sll $t0, 9
    xor parity, $t0
    li prev_match, 1<<31
    blt match_off, match_len, .Lmatch1_loop_check   # check if we can do 8-byte copy
     sub $v0, outbuf, match_off                 # calculate start of match
.Lmatch8_loop:                                  # 8-byte copy loop
    ldl $t0, 0($v0)                             # load 8 bytes
    ldr $t0, 7($v0)
    addiu $v0, 8
    sdl $t0, 0(outbuf)                          # store 8 bytes
    sdr $t0, 7(outbuf)
    addiu match_len, -8
    bgtz match_len, .Lmatch8_loop               # check we went past match_len
     addiu outbuf, 8
    b .Lloop                                    # jump to main loop
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
    b .Lloop                                    # jump to main loop
     addu outbuf, match_len                     # adjust pointer remove extra bytes

.Lmatch1_loop_check:                            # 1-byte copy loop
    beq match_off, 1, .Lmatch1_memset           # if match_off is 1, it's a memset
.Lmatch1_loop:                                  # 1-byte copy loop
    lbu $t0, 0($v0)                             # load 1 byte
    addiu $v0, 1
    sb $t0, 0(outbuf)                           # store 1 byte
    addiu match_len, -1
    bgtz match_len, .Lmatch1_loop               # check we went past match_len
     addiu outbuf, 1
    b .Lloop                                    # jump to main loop
     nop

.Lend:
    addiu $sp, $sp, ROUND_UP(NUM_CONTEXTS*2, 8)+16
    jr ra3
     sub $v0, outbuf, outbuf_orig

###################################################
# Function: .Llzdecode_number
#   Arithmetic decoder of a variable sized number
#
# Input:
#  * ctx: first context to use
# Output:
#  * $v0: decoded number
# Destroy:
#  * $t0, $t1
#  * num_len ($t7) (aliased with all other len variables)
#
###################################################

.Llzdecode_number:
    move ra2, $ra
    li num_len, 0
.Llzdecode_number_len:
    li $v0, 0
    bal .Llzdecode
     addiu ctx, 2*2
    bnez $v0, .Llzdecode_number_len
     addiu num_len, 1
    li $v0, 1
    addiu ctx, -1*2
.Llzdecode_number_value:
    sll $v0, 1
    bal .Llzdecode
     addiu num_len, -1
    bnez num_len, .Llzdecode_number_value
     addiu ctx, -2*2
.Llzdecode_number_end:
    jr ra2
     nop

###################################################
# Function: .Llzdecode
#   Arithmetic decoder of one bit
#
# Input: 
#   * ctx: context to use
#   * $v0: value to modify
# Return:
#   * $v0: set last bit to 1 if dcoded bit is 1,
#          otherwise leave it unchanged
# Destroy:
#   * $t0, $t1
###################################################

    .set noat
.Llzdecode:
    sll intervalsize, 16
    bltz intervalsize, .Llzdecode_prob
     lhu $t0, 0(ctx)
.Llzdecode_bit_loop:
    bnez nbits, .Llzdecode_next_bit
     addiu nbits, -1
    lwu $at, 0(inbuf)
    addiu inbuf, 4
    or intervalvalue, $at
    li nbits, 31
.Llzdecode_next_bit:     
    sll intervalsize, 1
    bgez intervalsize, .Llzdecode_bit_loop
     dsll intervalvalue, 1
.Llzdecode_prob:
    dsrl $at, intervalvalue, 48
    multu intervalsize, $t0
    srl intervalsize, 16
    mfhi $t1

    sub $at, $t1
    bgez $at, .Llzdecode_prob_false
     srl $at, $t0, ADJUST_SHIFT

.Llzdecode_prob_true:
    move intervalsize, $t1
    sub $t0, $at
    addiu $t0, (0xffff >> ADJUST_SHIFT)
    sh $t0, 0(ctx)
    jr $ra
     ori $v0, 1

.Llzdecode_prob_false:
    sub intervalsize, $t1
    dsll $t1, $t1, 48
    dsubu intervalvalue, $t1
    sub $t0, $at
    jr $ra
     sh $t0, 0(ctx)
    .set at
