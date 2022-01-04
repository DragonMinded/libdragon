/**
 * @file rsp.c
 * @brief Hardware Vector Interface
 * @ingroup rsp
 */

#include "libdragon.h"
#include "regsinternal.h"

/** @brief Static structure to address SP registers */
static volatile struct SP_regs_s * const SP_regs = (struct SP_regs_s *)0xa4040000;

/** @brief Current ucode being loaded */
static rsp_ucode_t *cur_ucode = NULL;

/**
 * @brief Wait until the SI is finished with a DMA request
 */
static void __SP_DMA_wait(void)
{
    while (SP_regs->status & (SP_STATUS_DMA_BUSY | SP_STATUS_IO_BUSY)) ;
}

void rsp_init(void)
{
    /* Make sure RSP is halted */
    *SP_PC = 0x1000;
    SP_regs->status = SP_WSTATUS_SET_HALT;
}

void rsp_load(rsp_ucode_t *ucode) {
    if (cur_ucode != ucode) {
        rsp_load_code(ucode->code, (uint8_t*)ucode->code_end - ucode->code, 0);
        rsp_load_data(ucode->data, (uint8_t*)ucode->data_end - ucode->data, 0);
        cur_ucode = ucode;
    }
}

void rsp_load_code(void* start, unsigned long size, unsigned int imem_offset)
{
    assert(((uint32_t)start % 8) == 0);
    assert((imem_offset % 8) == 0);

    if (cur_ucode != NULL) {
        cur_ucode = NULL;
    }

    disable_interrupts();
    __SP_DMA_wait();

    SP_regs->DRAM_addr = start;
    MEMORY_BARRIER();
    SP_regs->RSP_addr = SP_IMEM + imem_offset/4;
    MEMORY_BARRIER();
    SP_regs->rsp_read_length = size - 1;
    MEMORY_BARRIER();

    __SP_DMA_wait();
    enable_interrupts();
}

void rsp_load_data(void* start, unsigned long size, unsigned int dmem_offset)
{
    assert(((uint32_t)start % 8) == 0);
    assert((dmem_offset % 8) == 0);

    disable_interrupts();
    __SP_DMA_wait();

    SP_regs->DRAM_addr = start;
    MEMORY_BARRIER();
    SP_regs->RSP_addr = SP_DMEM + dmem_offset/4;
    MEMORY_BARRIER();
    SP_regs->rsp_read_length = size - 1;
    MEMORY_BARRIER();

    __SP_DMA_wait();
    enable_interrupts();
}

void rsp_read_code(void* start, unsigned long size, unsigned int imem_offset)
{
    assert(((uint32_t)start % 8) == 0);
    assert((imem_offset % 8) == 0);
    data_cache_hit_writeback_invalidate(start, size);

    disable_interrupts();
    __SP_DMA_wait();

    SP_regs->DRAM_addr = start;
    MEMORY_BARRIER();
    SP_regs->RSP_addr = SP_IMEM + imem_offset/4;
    MEMORY_BARRIER();
    SP_regs->rsp_write_length = size - 1;
    MEMORY_BARRIER();
    __SP_DMA_wait();

    enable_interrupts();
}


void rsp_read_data(void* start, unsigned long size, unsigned int dmem_offset)
{
    assert(((uint32_t)start % 8) == 0);
    assert((dmem_offset % 8) == 0);
    data_cache_hit_writeback_invalidate(start, size);

    disable_interrupts();
    __SP_DMA_wait();

    SP_regs->DRAM_addr = start;
    MEMORY_BARRIER();
    SP_regs->RSP_addr = SP_DMEM + dmem_offset/4;
    MEMORY_BARRIER();
    SP_regs->rsp_write_length = size - 1;
    MEMORY_BARRIER();
    __SP_DMA_wait();

    enable_interrupts();
}

void rsp_run_async(void)
{
    // set RSP program counter
    *SP_PC = cur_ucode ? cur_ucode->start_pc : 0;
    MEMORY_BARRIER();
    *SP_STATUS = SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_SET_INTR_BREAK;
}

void rsp_wait(void)
{
    while (!(*SP_STATUS & SP_STATUS_HALTED)) {}
}

void rsp_run(void)
{
    rsp_run_async();
    rsp_wait();
}
