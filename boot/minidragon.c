#include "minidragon.h"
#include <stdint.h>

void abort(void)
{
    while(1) {}
}

void io_write(uint32_t vaddrx, uint32_t value)
{
    volatile uint32_t *vaddr = (volatile uint32_t *)vaddrx;
    while (*PI_STATUS & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY)) {}
    *vaddr = value;
}
