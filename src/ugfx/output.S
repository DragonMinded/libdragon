output_open:
  # Inputs
  #define output_open_size $20
  # Clobbers
  # $20
  # $19
  # $18
  # $17
  #define tmp  $16

    # Check if the reserved size would overflow the buffer
    addu tmp, OUTPUT_POINTER, output_open_size
    addi tmp, tmp, -DMEM_SIZE

    # If there is no more space left in the buffer, flush it
    bgtz tmp, output_flush
    addiu OUTPUT_CMD_SIZE, output_open_size, 0

    jr ra
    nop

  #undef tmp

output_close:
  # Clobbers
  # $20
  # $19
  # $18
  # $17
  #define tmp $16

    # Advance output pointer and Reset output command size
    addu OUTPUT_POINTER, OUTPUT_POINTER, OUTPUT_CMD_SIZE

    # If the stored command happens to fill the buffer exactly, flush it immediately
    addi tmp, OUTPUT_POINTER, -DMEM_SIZE
    bgez tmp, output_flush
    addiu OUTPUT_CMD_SIZE, $0, 0

    jr ra
    nop
  #undef tmp

output_flush:
  # Clobbers
  # $20
  # $19
  # $18
  # $17
  #define tmp $16
  #define tmp2 $15
  #define return $14

    # Save return address
    addiu return, ra, 0

    li tmp, %lo(output_buffer)
    sub dma_len, OUTPUT_POINTER, tmp
    blez dma_len, output_flush_done

    # Check if output would overflow buffer in DRAM
    addu tmp, OUTPUT_DRAM_POINTER, dma_len
    sub tmp, tmp, OUTPUT_DRAM_END
    blez tmp, output_flush_fit

  output_flush_wait_for_dp:
    # Wait until DP is done with the current buffer
    mfc0 tmp2, COP0_DP_STATUS
    andi tmp2, DP_STATUS_START_VALID
    bnez tmp2, output_flush_wait_for_dp

  output_flush_advance:
    mfc0 tmp, COP0_DP_CURRENT
    addiu OUTPUT_DRAM_POINTER, OUTPUT_DRAM_START, 0
    beq tmp, OUTPUT_DRAM_POINTER, output_flush_advance
    nop

    mtc0 OUTPUT_DRAM_POINTER, COP0_DP_START

  output_flush_fit:
    mfc0 tmp, COP0_DP_CURRENT
    sub tmp2, OUTPUT_DRAM_POINTER, tmp
    bgez tmp2, output_flush_dma

    add tmp2, OUTPUT_DRAM_POINTER, dma_len
    sub tmp, tmp2, tmp
    bgez tmp, output_flush_fit
    nop

  output_flush_dma:
    addiu dma_mem_addr, $0, output_buffer
    addiu dma_dram_addr, OUTPUT_DRAM_POINTER, 0
    jal dma_start
    addiu dma_is_write, $0, 1

  output_flush_dma_wait:
    mfc0 tmp, COP0_DMA_BUSY
    bne tmp, $0, output_flush_dma_wait

    addiu OUTPUT_POINTER, $0, output_buffer
    addu OUTPUT_DRAM_POINTER, OUTPUT_DRAM_POINTER, dma_len
    mtc0 OUTPUT_DRAM_POINTER, COP0_DP_END

  output_flush_done:
    jr return
    nop

  #undef tmp
  #undef tmp2
  #undef return
