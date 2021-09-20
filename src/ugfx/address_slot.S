address_slot_decode:
  # Outputs
  #define address_slot $16
  # Clobbers
  #define tmp $15

    lw tmp, 0x0(INPUT_POINTER)
    srl address_slot, tmp, 26
    andi address_slot, address_slot, 0x3C
    li dma_dram_addr, 0x01FFFFFF
    jr ra
    and dma_dram_addr, dma_dram_addr, tmp

  #undef tmp

load_from_address_slot:
  #define tmp $15
  #define return $14

    addiu return, ra, 0
    jal address_slot_decode
    addiu dma_is_write, $0, 0
    lw tmp, %lo(address_slots)(address_slot)
    jal dma_start
    add dma_dram_addr, dma_dram_addr, tmp

  dma_wait:
    mfc0 tmp, COP0_DMA_BUSY
    bne tmp, $0, dma_wait
    nop
    jr return
    nop

  set_address_slot:
    jal address_slot_decode
    nop
    j main_loop_advance_input
    sw dma_dram_addr, %lo(address_slots)(address_slot)
    
  #undef tmp
  #undef return
