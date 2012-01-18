/**
 * @file dma.h
 * @brief DMA Controller
 * @ingroup dma
 */
#ifndef __LIBDRAGON_DMA_H
#define __LIBDRAGON_DMA_H

#ifdef __cplusplus
extern "C" {
#endif

void dma_write(void * ram_address, unsigned long pi_address, unsigned long len);
void dma_read(void * ram_address, unsigned long pi_address, unsigned long len);
volatile int dma_busy();

/* 32 bit IO read from PI device */
uint32_t io_read(uint32_t pi_address);

/* 32 bit IO write to PI device */
void io_write(uint32_t pi_address, uint32_t data);

#ifdef __cplusplus
}
#endif

#endif
