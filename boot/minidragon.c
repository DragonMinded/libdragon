#include "minidragon.h"
#include <stdint.h>

void io_write(uint32_t vaddrx, uint32_t value)
{
    volatile uint32_t *vaddr = (volatile uint32_t *)vaddrx;
    *vaddr = value;
    while (*PI_STATUS & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY)) {}
}
