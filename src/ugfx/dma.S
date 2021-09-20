dma_start:
  # Inputs
  #define dma_mem_addr  $20
  #define dma_dram_addr $19
  #define dma_len       $18
  #define dma_is_write  $17
  # Clobbers
  #define tmp1      $16
  #define tmp2      $15

    mfc0  tmp1, COP0_DMA_FULL
    bne   tmp1, $0, dma_start
    addi  tmp2, dma_len, -1
    mtc0  dma_mem_addr, COP0_DMA_SPADDR
    bgtz  dma_is_write, dma_write
    mtc0  dma_dram_addr, COP0_DMA_RAMADDR
    j     dma_done
    mtc0  tmp2, COP0_DMA_READ
  dma_write:
    mtc0  tmp2, COP0_DMA_WRITE
  dma_done:
    jr    ra
    nop
  #undef tmp1
  #undef tmp2
