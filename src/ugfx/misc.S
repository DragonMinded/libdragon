rsp_command:
  #define tmp $2
  srl tmp, COMMAND_WORD, 22
  andi tmp, tmp, 0xFC
  lw tmp, %lo(jump_table)(tmp)
  jr tmp
  nop
  #undef tmp

link_commands:
  #define length $2
  #define is_load $3
  #define stack_addr $4
  #define current_ptr $5
  #define rem_len $6

    # Prepare length mask
    lui length, 0xFF
    ori length, length, 0xFFF8

    # Check if load flag is set
    andi is_load, COMMAND_WORD, 0x1
    bne is_load, $0, link_commands_skip_stack

    # Get new input length
    and length, length, COMMAND_WORD

    # Save current input pointer and remaining input length to stack
    lh stack_addr, %lo(cmd_stack_ptr)

    # current_ptr = INPUT_DRAM_START + INPUT_POINTER + 0x4 - input_buffer
    li current_ptr, %lo(input_buffer)
    sub current_ptr, INPUT_POINTER, current_ptr
    addi current_ptr, current_ptr, 0x4
    add current_ptr, current_ptr, INPUT_DRAM_START
    sw current_ptr, 0x0(stack_addr)

    sub rem_len, INPUT_DRAM_END, current_ptr
    sw rem_len, 0x4(stack_addr)

    # Advance stack
    addiu stack_addr, stack_addr, CMD_LIST_SIZE
    sh stack_addr, %lo(cmd_stack_ptr)

  link_commands_skip_stack:
    # TODO: address slot
    lw INPUT_DRAM_START, 0x0(INPUT_POINTER)

    # Force input buffer to be refilled
    addiu INPUT_POINTER, $0, input_buffer_end

    # Set new input pointer and input end
    addiu INPUT_DRAM_POINTER, INPUT_DRAM_START, 0
    j main_loop
    add INPUT_DRAM_END, INPUT_DRAM_POINTER, length

  #undef length
  #undef is_load
  #undef stack_addr
  #undef current_ptr
  #undef rem_len

finalize:
  #define stack_addr $2
  #define tmp $3

    lh stack_addr, %lo(cmd_stack_ptr)

    # If the stack is empty, the task is done.
    addiu tmp, $0, cmd_stack
    beq stack_addr, tmp, finish
    addiu stack_addr, stack_addr, -CMD_LIST_SIZE

    # Pop input pointer and length from stack
    lw INPUT_DRAM_START, 0x0(stack_addr)
    lw INPUT_DRAM_END, 0x4(stack_addr)
    addiu INPUT_DRAM_POINTER, INPUT_DRAM_START, 0
    add INPUT_DRAM_END, INPUT_DRAM_POINTER, INPUT_DRAM_END
    sh stack_addr, %lo(cmd_stack_ptr)

    j main_loop
    # Force input buffer to be refilled
    addiu INPUT_POINTER, $0, input_buffer_end
  #undef tmp
  #undef stack_addr

set_cull_mode:
  #define tmp $2
  andi tmp, COMMAND_WORD, 0x3
  j main_loop_advance_input
  sh tmp, %lo(cull_mode)
  #undef tmp

set_geometry_mode:
  #define tmp $2
  andi tmp, COMMAND_WORD, 0xFF
  j main_loop_advance_input
  sh tmp, %lo(g_mode)
  #undef tmp

set_persp_norm:
  #define tmp $2
  andi tmp, COMMAND_WORD, 0xFFFF
  j main_loop_advance_input
  sh tmp, %lo(persp_norm_factor)
  #undef tmp

set_texture_settings:
  #define tmp1 $2
  #define tmp2 $3
  andi tmp1, COMMAND_WORD, 0x3F
  sb tmp1, %lo(texture_settings)
  lw tmp2, 0x0(INPUT_POINTER)
  j main_loop_advance_input
  sw tmp2, %lo(texture_scale)
  #undef tmp1
  #undef tmp2

set_num_lights:
  #define tmp $2
  andi tmp, COMMAND_WORD, 0x38
  j main_loop_advance_input
  sh tmp, %lo(num_lights)
  #undef tmp

set_clip_ratio:
  #define tmp $2
  andi tmp, COMMAND_WORD, 0xFFFF
  sh tmp, %lo(clip_ratio) + 0x4
  j main_loop_advance_input
  sh tmp, %lo(clip_ratio) + 0x6
  #undef tmp

load_viewport:
  addiu dma_mem_addr, $0, vport
  addiu dma_len, $0, VPORT_SIZE  
  j load_from_address_slot
  addiu ra, $0, main_loop_advance_input

load_light:
  andi dma_mem_addr, COMMAND_WORD, 0x38
  addiu dma_mem_addr, dma_mem_addr, light_buffer
  addiu dma_len, $0, LIGHT_SIZE
  j load_from_address_slot
  addiu ra, $0, main_loop_advance_input
