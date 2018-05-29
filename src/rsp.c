/**
 * @file rsp.c
 * @brief Hardware Vector Interface
 * @ingroup rsp
 */

#include "libdragon.h"
#include "regsinternal.h"

/** @brief SP DMA busy */
#define SP_STATUS_DMA_BUSY              ( 1 << 2 )
/** @brief SP IO busy */
#define SP_STATUS_IO_BUSY               ( 1 << 4 )
/** @brief SP broke */
#define SP_STATUS_INTERRUPT_ON_BREAK    ( 1 << 6 )
/** @brief SP halted */
#define SP_STATUS_HALTED                ( 1 )

#define SP_STATUS_CLEAR_HALT        0x00001
#define SP_STATUS_SET_HALT          0x00002
#define SP_STATUS_CLEAR_BROKE       0x00004
#define SP_STATUS_CLEAR_INTR        0x00008
#define SP_STATUS_SET_INTR          0x00010
#define SP_STATUS_CLEAR_SSTEP       0x00020
#define SP_STATUS_SET_SSTEP         0x00040
#define SP_STATUS_CLEAR_INTR_BREAK  0x00080
#define SP_STATUS_SET_INTR_BREAK    0x00100
#define SP_STATUS_CLEAR_SIG0        0x00200
#define SP_STATUS_SET_SIG0          0x00400
#define SP_STATUS_CLEAR_SIG1        0x00800
#define SP_STATUS_SET_SIG1          0x01000
#define SP_STATUS_CLEAR_SIG2        0x02000
#define SP_STATUS_SET_SIG2          0x04000
#define SP_STATUS_CLEAR_SIG3        0x08000
#define SP_STATUS_SET_SIG3          0x10000
#define SP_STATUS_CLEAR_SIG4        0x20000
#define SP_STATUS_SET_SIG4          0x40000
#define SP_STATUS_CLEAR_SIG5        0x80000
#define SP_STATUS_SET_SIG5          0x100000
#define SP_STATUS_CLEAR_SIG6        0x200000
#define SP_STATUS_SET_SIG6          0x400000
#define SP_STATUS_CLEAR_SIG7        0x800000
#define SP_STATUS_SET_SIG7          0x1000000

#define SP_DMA_DMEM 0x04000000
#define SP_DMA_IMEM 0x04001000

/** @brief Static structure to address SP registers */
static volatile struct SP_regs_s * const SP_regs = (struct SP_regs_s *)0xa4040000;

/**
 * @brief Wait until the SI is finished with a DMA request
 */
static void __SP_DMA_wait(void)
{
    while (SP_regs->status & (SP_STATUS_DMA_BUSY | SP_STATUS_IO_BUSY)) ;
}

void rsp_init()
{
    /* Make sure RSP is halted */
    *(volatile uint32_t *) 0xA4080000 = SP_DMA_IMEM;
    SP_regs->status = SP_STATUS_SET_HALT;

    return;
}

void load_ucode(void* start, unsigned long size)
{
    data_cache_hit_writeback_invalidate(start, size);

    disable_interrupts();
    __SP_DMA_wait();

    SP_regs->DRAM_addr = start;
    MEMORY_BARRIER();
    SP_regs->RSP_addr = (void*)SP_DMA_IMEM;
    MEMORY_BARRIER();
    SP_regs->rsp_read_length = size - 1;
    MEMORY_BARRIER();

    __SP_DMA_wait();
    enable_interrupts();

    data_cache_hit_invalidate(start, size);
    return;
}

void read_ucode(void* start, unsigned long size)
{
    data_cache_hit_writeback_invalidate(start, size);

    disable_interrupts();
    __SP_DMA_wait();

    SP_regs->DRAM_addr = start;
    MEMORY_BARRIER();
    SP_regs->RSP_addr = (void*)SP_DMA_IMEM;
    MEMORY_BARRIER();
    SP_regs->rsp_write_length = size - 1;
    MEMORY_BARRIER();
    __SP_DMA_wait();
    data_cache_hit_invalidate(start, size);

    enable_interrupts();
    return;
}

void run_ucode()
{
    // set RSP program counter
    *(volatile uint32_t *) 0xA4080000 = SP_DMA_IMEM;
    SP_regs->status = SP_STATUS_CLEAR_HALT | SP_STATUS_SET_INTR_BREAK;
    return;
}