/**
 * @file dma.h
 * @brief DMA Controller
 * @ingroup dma
 */
#ifndef __LIBDRAGON_DMA_H
#define __LIBDRAGON_DMA_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PI_DRAM_ADDR    ((volatile uint32_t*)0xA4600000)  ///< PI DMA: DRAM address register
#define PI_CART_ADDR    ((volatile uint32_t*)0xA4600004)  ///< PI DMA: cartridge address register
#define PI_RD_LEN       ((volatile uint32_t*)0xA4600008)  ///< PI DMA: read length register
#define PI_WR_LEN       ((volatile uint32_t*)0xA460000C)  ///< PI DMA: write length register
#define PI_STATUS       ((volatile uint32_t*)0xA4600010)  ///< PI: status register

void dma_write_raw_async(const void *ram_address, unsigned long pi_address, unsigned long len);
void dma_write(const void * ram_address, unsigned long pi_address, unsigned long len);

void dma_read_raw_async(void *ram_address, unsigned long pi_address, unsigned long len);
void dma_read_async(void *ram_address, unsigned long pi_address, unsigned long len);
void dma_read(void * ram_address, unsigned long pi_address, unsigned long len);

void dma_wait(void);

/* 32 bit IO read from PI device */
uint32_t io_read(uint32_t pi_address);

/* 32 bit IO write to PI device */
void io_write(uint32_t pi_address, uint32_t data);

bool io_accessible(uint32_t pi_address);

__attribute__((deprecated("use dma_wait instead"))) 
volatile int dma_busy(void);


#ifdef __cplusplus
}
#endif

#endif
