address_slot_decode_from_input:
  # Inputs
  #define address_decode_input $15
  # Outputs
  #define address_slot $16
  
    lw address_decode_input, 0x0(INPUT_POINTER)
address_slot_decode:

    srl address_slot, address_decode_input, 26
    andi address_slot, address_slot, 0x3C
    li dma_dram_addr, 0x01FFFFFF
    jr ra
    and dma_dram_addr, dma_dram_addr, address_decode_input

load_from_address_slot:
  #define tmp $15
  #define return $14

    addiu return, ra, 0
    jal address_slot_decode_from_input
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
  jal address_slot_decode_from_input
  nop
  j main_loop_advance_input
  sw dma_dram_addr, %lo(address_slots)(address_slot)
    
  #undef tmp
  #undef return
