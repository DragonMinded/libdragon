load_matrix:
  #define is_proj $2
  #define is_force $3
  #define is_pop $4
  #define is_push $5
  #define is_mul $6
  #define tmp $7

    # Check if matrix is being forced as the current matrix
    andi is_force, COMMAND_WORD, 0x2
    bne is_force, $0, load_matrix_dma
    addiu dma_mem_addr, $0, mtx_current

    # Check if the matrix should be set as the projection matrix
    andi is_proj, COMMAND_WORD, 0x1
    bne is_proj, $0, load_matrix_dma
    addiu dma_mem_addr, $0, mtx_proj

    # At this point we are working with the mv stack
    lh dma_mem_addr, %lo(mtx_mv_stack_ptr)

    # Check if the stack should be popped
    andi is_pop, COMMAND_WORD, 0x4
    beq is_pop, $0, load_matrix_mv_stack

    # Check if we are already at the bottom
    addiu tmp, $0, mtx_stack
    beq dma_mem_addr, tmp, main_loop_advance_input
    nop

    # Move stack pointer backwards
    addiu dma_mem_addr, dma_mem_addr, -MTX_SIZE
    j load_matrix_update_current
    sh dma_mem_addr, %lo(mtx_mv_stack_ptr)

  load_matrix_mv_stack:
    # Check if the matrix should be pushed (or just loaded)
    andi is_push, COMMAND_WORD, 0x10
    beq is_push, $0, load_matrix_dma

    # Check if there is room on the stack (if not then just load/mult it at the top)
    addiu tmp, $0, mtx_stack_end - MTX_SIZE
    beq dma_mem_addr, tmp, load_matrix_dma
    nop

    # Advance the stack pointer
    addiu dma_mem_addr, dma_mem_addr, MTX_SIZE
    sh dma_mem_addr, %lo(mtx_mv_stack_ptr)
    
  load_matrix_dma:
    # The second half of the input is the dram address
    jal load_from_address_slot
    addiu dma_len, $0, MTX_SIZE

    # We dont need to update the matrix if it is being forced
    bne is_force, $0, main_loop_advance_input
    nop

    # Dont update mv stack if this is a projection matrix
    bne is_proj, $0, load_matrix_update_current

  load_matrix_update_mv:
    # Check if the matrix should be multiplied into the stack
    andi is_mul, COMMAND_WORD, 0x08
    beq is_mul, $0, load_matrix_update_current

    # Check if we are at the bottom of the stack
    addiu tmp, $0, mtx_stack
    beq dma_mem_addr, tmp, load_matrix_update_current

    # Multiply with the previous entry in the stack
    addiu mtx_mult_a, dma_mem_addr, -MTX_SIZE
    addiu mtx_mult_b, dma_mem_addr, 0
    jal matrix_mult_mtx
    addiu mtx_mult_out, dma_mem_addr, 0

  load_matrix_update_current:
    addiu mtx_mult_a, $0, mtx_proj
    addiu mtx_mult_out, $0, mtx_current
    lh mtx_mult_b, %lo(mtx_mv_stack_ptr)
    j matrix_mult_mtx
    addiu ra, $0, main_loop_advance_input
    
  #undef tmp
  #undef is_proj
  #undef is_force
  #undef is_pop
  #undef is_push
  #undef is_mul
