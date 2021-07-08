/**
 * @file dma.h
 * @brief DMA Controller
 * @ingroup dma
 */
#ifndef __LIBDRAGON_DMA_H
#define __LIBDRAGON_DMA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

volatile int dma_busy(void);

void dma_read(void * dest, uint32_t cart_address, uint32_t len);
void dma_write(const void * src, uint32_t cart_address, uint32_t len);

void pi_dma_read(void * dest, uint32_t pi_address, uint32_t len);
void pi_dma_write(const void * src, uint32_t pi_address, uint32_t len);

/* 32 bit IO read from PI device */
uint32_t io_read(uint32_t address);

/* 32 bit IO write to PI device */
void io_write(uint32_t address, uint32_t data);

#ifdef __cplusplus
}
#endif

#endif
