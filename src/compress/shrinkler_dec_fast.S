##############################################
#  Shrinkler - Fast decompressor in assembly
#
#  This decompressor implements the following variant of Shrinkler:
#    * Forward decoding
#    * No parity
##############################################

#define inbuf           $a0
#define outbuf          $a1

#define intervalsize    $t2   
#define intervalvalue   $t3
#define nbits           $t4
#define match_off       $t5
#define prev_match      $t6
#define lit_len         $t7
#define match_len       $t7
#define num_len         $t7
#define ctx             $t8

#define ra2             $a2
#define ra3             $a3

#define ROUND_UP(n,d)   (((n) + (d) - 1) / (d) * (d))

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
    tne $ra,$ra,0x10
    addiu $t1, $sp, -16
    addiu $sp, $sp, -ROUND_UP(NUM_CONTEXTS*2, 8)-16
    move $t0, $sp
    li $t2, 0x80008000
.Linit_contexts_loop:
    sw $t2, 0($t0)
    sw $t2, 4($t0)
    blt $t0, $t1, .Linit_contexts_loop
     addiu $t0, 8

    move ra3, $ra
    ldl intervalvalue, 0(inbuf)
    ldr intervalvalue, 7(inbuf)
    addiu inbuf, 7
    dsrl intervalvalue, 8
    dsll intervalvalue, 7
    li nbits, 1
    b .Lliteral
     li intervalsize, 0x8000

.Lloop:
    bal .Llzdecode
     li ctx, CONTEXT_KIND
    bnez $v0, .Lmatch
     nop

.Lliteral:
    li prev_match, 0
    li lit_len, 7
    li $v1, 1
.Lliteral_loop:
    bal .Llzdecode
     addiu ctx, $v1, 1
    sll $v1, 1
    or $v1, $v0
    bnez lit_len, .Lliteral_loop
     addiu lit_len, -1

    tne $ra,$ra,0x10
    sb $v1, 0(outbuf)
    b .Lloop
     addiu outbuf, 1

.Lmatch:
    tne $ra,$ra,0x10
    li ctx, CONTEXT_REPEATED
    bgezal prev_match, .Llzdecode
     li $v0, 0
    sll $v0, 31
    li ctx, CONTEXT_GROUP_OFFSET
    bgezal $v0, .Llzdecode_number
     addiu $v1, match_off, 2
    addiu $v1, -2
    beqz $v1, .Lend
     move match_off, $v1
    bal .Llzdecode_number
     li ctx, CONTEXT_GROUP_LENGTH
    move match_len, $v1
    li prev_match, 1<<31
    tne $ra,$ra,0x10
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
    move $v0, $a1
    jr ra3
     addiu $sp, $sp, ROUND_UP(NUM_CONTEXTS*2, 8)+16

###################################################
# Function: .Llzdecode_number
#   Arithmetic decoder of a variable sized number
#
# Input:
#  * ctx: first context to use
# Output:
#  * $v1: decoded number
# Destroy:
#  * $t0, $t1, $v0 (through .Llzdecode)
#  * num_len ($t7) (aliased with all other len variables)
#
###################################################

.Llzdecode_number:
    move ra2, $ra
    li num_len, 0
.Llzdecode_number_len:
    bal .Llzdecode
     addiu ctx, 2
    bnez $v0, .Llzdecode_number_len
     addiu num_len, 1
    li $v1, 1
    addiu ctx, -1
.Llzdecode_number_value:
    bal .Llzdecode
     addiu num_len, -1
    sll $v1, 1
    addiu ctx, -2
    bnez num_len, .Llzdecode_number_value
     or $v1, $v0
.Llzdecode_number_end:
    jr ra2
     nop

###################################################
# Function: .Llzdecode
#   Arithmetic decoder of one bit
#
# Input: 
#   * ctx: context to use
# Return:
#   * $v0: decoded bit (0 or 1)
# Destroy:
#   * $t0, $t1
###################################################

.Llzdecode:
    sll intervalsize, 16
    bltz intervalsize, .Llzdecode_prob
     sll $v0, ctx, 1
.Llzdecode_bit_loop:
    bnez nbits, .Llzdecode_next_bit
     addiu nbits, -1
    lbu $t0, 0(inbuf)
    addiu inbuf, 1
    or intervalvalue, $t0
    li nbits, 7
.Llzdecode_next_bit:     
    sll intervalsize, 1
    bgez intervalsize, .Llzdecode_bit_loop
     dsll intervalvalue, 1
.Llzdecode_prob:
    addu $v0, $sp
    lhu $t0, 0($v0)
    srl intervalsize, 16

    .set noat
    multu intervalsize, $t0
    mflo $t1
    srl $t1, 16

    dsrl $at, intervalvalue, 48
    sub $at, $t1
    bgez $at, .Llzdecode_prob_false
     srl $at, $t0, ADJUST_SHIFT

.Llzdecode_prob_true:
    move intervalsize, $t1
    sub $t0, $at
    addiu $t0, (0xffff >> ADJUST_SHIFT)
    sh $t0, 0($v0)
    jr $ra
     li $v0, 1

.Llzdecode_prob_false:
    sub intervalsize, $t1
    dsll $t1, $t1, 48
    dsubu intervalvalue, $t1
    sub $t0, $at
    sh $t0, 0($v0)
    jr $ra
     li $v0, 0
    .set at
