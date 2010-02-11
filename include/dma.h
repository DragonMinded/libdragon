#ifndef __LIBDRAGON_DMA_H
#define __LIBDRAGON_DMA_H

/* Write to peripheral */
void dma_write(void * ram_address, unsigned long pi_address, unsigned long len);

/* Read from peripheral */
void dma_read(void * ram_address, unsigned long pi_address, unsigned long len);

/* Return whether peripheral interface is busy */
volatile int dma_busy();

#endif
