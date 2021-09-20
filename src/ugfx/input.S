fetch_input:
  # Clobbers
  # $20
  # $19
  # $18
  # $17
  #define tmp $16
  #define tmp2 $15
  #define return $14

    # Check if input buffer is empty
    li tmp, %lo(input_buffer_end)
    sub tmp, INPUT_POINTER, tmp
    bltz tmp, fetch_input_done
    addiu return, ra, 0

    # Compute remaining input length
    sub tmp, INPUT_DRAM_END, INPUT_DRAM_POINTER

    # Check if there is any more input to load
    blez tmp, finalize
    addiu dma_is_write, $0, 0

    # Check which is shorter: remaining input or input buffer?
    addiu dma_len, $0, INPUT_BUFFER_SIZE
    slt tmp2, dma_len, tmp
    bgtz tmp2, fetch_input_dma
    # Branch delay
    addiu dma_mem_addr, $0, input_buffer

    # Remaining input is shorter: assign it to dma_len parameter
    addu dma_len, tmp, $0

  fetch_input_dma:
    jal dma_start
    addiu dma_dram_addr, INPUT_DRAM_POINTER, 0

  fetch_input_dma_wait:
    # Wait for DMA to finish
    mfc0 tmp, COP0_DMA_BUSY
    bne tmp, $0, fetch_input_dma_wait

    # Reset input pointer to beginning of buffer
    addiu INPUT_POINTER, $0, input_buffer
    # Advance dram input pointer
    addu INPUT_DRAM_POINTER, INPUT_DRAM_POINTER, dma_len

  fetch_input_done:
    # Prepare input to be read
    lw COMMAND_WORD, 0x0(INPUT_POINTER)
    jr return
    addiu INPUT_POINTER, INPUT_POINTER, 0x4

  #undef tmp
  #undef tmp2
  #undef return
