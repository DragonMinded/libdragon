.set noreorder
.set at

#include <rsp.inc>

#define DMA_ALIGN           3
#define VEC_ALIGN           4

#define IMEM_SIZE           0x1000
#define DMEM_SIZE           0x1000

#define VPORT_SIZE          0x0010

#define ADDRESS_SLOT_SIZE   0x0004
#define ADDRESS_SLOT_COUNT  16

#define VTX_IN_SIZE         0x0010
#define VTX_SIZE            0x0028
#define VTX_CACHE_COUNT     32
#define VTX_CACHE_SIZE      VTX_CACHE_COUNT * VTX_SIZE
#define VTX_IN_BUFFER_SIZE  VTX_CACHE_COUNT * VTX_IN_SIZE

#define CPLANE_COUNT        6
#define CLIP_LIST_COUNT     9
#define CLIP_LIST_SIZE      CLIP_LIST_COUNT * VTX_SIZE
#define CLIP_LISTS_SIZE     CLIP_LIST_SIZE * 2

#define MTX_SIZE            0x0040
#define MTX_STACK_COUNT     10
#define MTX_STACK_SIZE      MTX_STACK_COUNT * MTX_SIZE

#define LIGHT_SIZE          0x0008
#define LIGHT_BUFFER_COUNT  8
#define LIGHT_BUFFER_SIZE   LIGHT_BUFFER_COUNT * LIGHT_SIZE

#define CMD_LIST_SIZE       0x0008
#define CMD_STACK_COUNT     6
#define CMD_STACK_SIZE      CMD_STACK_COUNT * CMD_LIST_SIZE

#define TMP_SIZE            CLIP_LISTS_SIZE

#define INPUT_BUFFER_SIZE   0x0100


.data
# Task parameters
.align 4
input:
.word 0x0
input_size:
.word 0x0
output_buf:
.word 0x0
output_buf_size:
.word 0x0

.align VEC_ALIGN
shift_left_constants:
.half 0x0001 # 1
.half 0x0002 # 2
.half 0x0004 # 4
.half 0x0008 # 8
.half 0x0010 # 16
.half 0x0020 # 32
.half 0x0040 # 64
.half 0x0080 # 128

.align VEC_ALIGN
shift_right_constants:
.half 0x0000 # 1/1
.half 0x8000 # 1/2
.half 0x4000 # 1/4
.half 0x2000 # 1/8
.half 0x1000 # 1/16
.half 0x0800 # 1/32
.half 0x0400 # 1/64
.half 0x0200 # 1/128

.align 3
clip_plane_coefficients:
# x <= w
.half 0x0001
.half 0x0000
.half 0x0000
.half 0x0001

# y <= w
.half 0x0000
.half 0x0001
.half 0x0000
.half 0x0001

# z <= w
.half 0x0000
.half 0x0000
.half 0x0001
.half 0x0001

# x >= -w
.half 0x0001
.half 0x0000
.half 0x0000
.half 0xFFFF

# y >= -w
.half 0x0000
.half 0x0001
.half 0x0000
.half 0xFFFF

# z >= -w
.half 0x0000
.half 0x0000
.half 0x0001
.half 0xFFFF

.align 3
clip_ratio:
.half 0x0001
.half 0x0001
.half 0x0001
.half 0x0001

# TODO: How to compress the jump table to 16 bits per entry?
#       The problem is: if we replace .word with .half, for some reason the assembler/linker
#       seems to substitute the jump addresses incorrectly.
.macro jumpTableEntry label
  .word \label
.endm

.align 2
jump_table:
jumpTableEntry finalize
jumpTableEntry load_vertices
jumpTableEntry load_matrix
jumpTableEntry load_viewport
jumpTableEntry load_light
jumpTableEntry link_commands
jumpTableEntry set_cull_mode
jumpTableEntry set_geometry_mode
jumpTableEntry set_persp_norm
jumpTableEntry set_texture_settings
jumpTableEntry set_num_lights
jumpTableEntry set_clip_ratio
jumpTableEntry set_address_slot
jumpTableEntry draw_triangle

.align 2
address_slots:
.fill ADDRESS_SLOT_COUNT, ADDRESS_SLOT_SIZE, 0x0

texture_scale:
.word 0x80008000

clip_plane_masks:
.half 0x0001
.half 0x0002
.half 0x0004
.half 0x0100
.half 0x0200
.half 0x0400

texture_settings:
.half 0x0

mtx_mv_stack_ptr:
.half 0x0

cull_mode:
.half 0x0

g_mode:
.half 0x0

cmd_stack_ptr:
.half 0x0

persp_norm_factor:
.half 0xFFFF

num_lights:
.half 0x0

.align DMA_ALIGN
data_end:

.bss

.align VEC_ALIGN
mtx_current:
.skip MTX_SIZE

.align VEC_ALIGN
mtx_proj:
.skip MTX_SIZE

.align VEC_ALIGN
mtx_stack:
.skip MTX_STACK_SIZE
mtx_stack_end:

.align DMA_ALIGN
vport:
.skip VPORT_SIZE

.align DMA_ALIGN
cmd_stack:
.skip CMD_STACK_SIZE

.align DMA_ALIGN
light_buffer:
.skip LIGHT_BUFFER_SIZE

.align DMA_ALIGN
tmp_buffer:
.skip TMP_SIZE

.align DMA_ALIGN
vtx_cache:
.skip VTX_CACHE_SIZE

.align DMA_ALIGN
input_buffer:
.skip INPUT_BUFFER_SIZE
input_buffer_end:

.align DMA_ALIGN
output_buffer:

.text

#define vzero $v00
#define vshl  $v30
#define vshr  $v31

#define COMMAND_WORD        $30
#define INPUT_POINTER       $29
#define OUTPUT_POINTER      $28
#define INPUT_DRAM_POINTER  $27
#define INPUT_DRAM_START    $26
#define INPUT_DRAM_END      $25
#define OUTPUT_DRAM_POINTER $24
#define OUTPUT_DRAM_START   $23
#define OUTPUT_DRAM_END     $22
#define OUTPUT_CMD_SIZE     $21

init:
  #define tmp  $2
  #define tmp2 $3
  
  # Initialize the RSP
  addiu tmp, $0, SP_WSTATUS_RESET_RSP_INTERRUPT | SP_WSTATUS_SET_INTR_ON_BREAK
  mtc0 tmp, COP0_SP_STATUS

  # Initialize the RDP
  addiu tmp2, $0, DP_WSTATUS_RESET_FREEZE | DP_WSTATUS_RESET_FLUSH | DP_WSTATUS_RESET_XBUS_DMEM_DMA
  mtc0 tmp2, COP0_DP_STATUS

  # Initialize constant vector registers
  vxor vzero, vzero, vzero, VE_V
  li tmp, %lo(shift_left_constants)
  lqv $v30, 0, 0, tmp
  li tmp, %lo(shift_right_constants)
  lqv $v31, 0, 0, tmp

  # Load task parameters
  lw INPUT_DRAM_START,  %lo(input)
  lw INPUT_DRAM_END,    %lo(input_size)
  lw OUTPUT_DRAM_START, %lo(output_buf)
  lw OUTPUT_DRAM_END,   %lo(output_buf_size)

  addiu INPUT_DRAM_POINTER, INPUT_DRAM_START, 0
  addiu OUTPUT_DRAM_POINTER, OUTPUT_DRAM_START, 0

  # Align input size to 8 bits
  lui tmp, 0xFFFF
  ori tmp, tmp, 0xFFF8
  and INPUT_DRAM_END, INPUT_DRAM_END, tmp
  addu INPUT_DRAM_END, INPUT_DRAM_END, INPUT_DRAM_START

  addu OUTPUT_DRAM_END, OUTPUT_DRAM_END, OUTPUT_DRAM_START

  addiu INPUT_POINTER, $0, input_buffer_end
  addiu OUTPUT_POINTER, $0, output_buffer

  addiu tmp2, $0, cmd_stack
  sh tmp2, %lo(cmd_stack_ptr)

wait_until_dp_ready:
  mfc0 tmp, COP0_DP_STATUS
  andi tmp, DP_STATUS_START_VALID
  bnez tmp, wait_until_dp_ready

  # Reset matrix state
  addiu tmp2, $0, mtx_stack
  sh tmp2, %lo(mtx_mv_stack_ptr)

  mtc0 OUTPUT_DRAM_POINTER, COP0_DP_START
  j main_loop
  mtc0 OUTPUT_DRAM_POINTER, COP0_DP_END
  #undef tmp
  #undef tmp2

#include "dma.S"
#include "input.S"
#include "output.S"

main_loop_advance_input:
  addiu INPUT_POINTER, INPUT_POINTER, 0x4
main_loop:
  #define tmp $2
  #define tmp2 $3
  #define out_size $4
  #define out_ptr $5

  jal fetch_input
  nop
  # Check if command is RSP processing command.
  bltz COMMAND_WORD, rsp_command

  # Check if command is "Texture Rectangle [Flip]"
  # Shift by 24 + 1, because the LSB does not matter 
  srl tmp, COMMAND_WORD, 25
  andi tmp, tmp, 0x1F
  # 0x12 == 0x24 >> 1
  li tmp2, 0x12

  bne tmp, tmp2, no_tex_rect
  addiu output_open_size, $0, 0x8
  # If it matches, double the output size
  sll output_open_size, output_open_size, 1

no_tex_rect:
  # Reserve space in the output buffer
  jal output_open
  addiu out_size, output_open_size, 0

  addiu out_ptr, OUTPUT_POINTER, 0

copy_to_output:
  # Copy RDP command through to output stream.
  lw tmp, 0x0(INPUT_POINTER)
  sw COMMAND_WORD, 0x0(out_ptr)
  sw tmp, 0x4(out_ptr)

  addi out_size, out_size, -0x8
  blez out_size, submit
  addiu INPUT_POINTER, INPUT_POINTER, 0x4

  addiu out_ptr, out_ptr, 0x8

  # Fetch next command and loop back
  j fetch_input
  addiu ra, $0, copy_to_output

submit:
  j output_close
  addiu ra, $0, main_loop

  #undef tmp
  #undef tmp2
  #undef out_size
  #undef out_ptr

#include "address_slot.S"
#include "matrix_mult.S"
#include "newton_raphson.S"
#include "vtx_to_scr.S"
#include "misc.S"
#include "load_matrix.S"
#include "load_vertices.S"
#include "draw_triangle.S"

finish:
  jal output_flush
  nop

  break
  nop
