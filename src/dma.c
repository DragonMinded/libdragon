#include "libdragon.h"
#include "regsinternal.h"

#define PI_STATUS_DMA_BUSY ( 1 << 0 )
#define PI_STATUS_IO_BUSY  ( 1 << 1 )
#define PI_STATUS_ERROR    ( 1 << 2 )

static volatile struct PI_regs_s * const PI_regs = (struct PI_regs_s *)0xa4600000;

volatile int dma_busy() 
{
    return PI_regs->status & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY);
}

void dma_read(void * ram_address, unsigned long pi_address, unsigned long len) 
{
    while (dma_busy()) ;
    PI_regs->ram_address = ram_address;
    PI_regs->pi_address = pi_address;
    PI_regs->write_length = len-1;
    while (dma_busy()) ;
}

void dma_write(void * ram_address, unsigned long pi_address, unsigned long len) {
    while (dma_busy()) ;
    PI_regs->ram_address = ram_address;
    PI_regs->pi_address = pi_address;
    PI_regs->read_length = len-1;
    while (dma_busy()) ;
}
