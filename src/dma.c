/**
 * @file dma.c
 * @brief DMA Controller
 * @ingroup dma
 */
#include "libdragon.h"
#include "regsinternal.h"

/**
 * @defgroup dma DMA Controller
 * @ingroup libdragon
 * @{
 */

/**
 * @name PI Status Register Bit Definitions
 * @{
 */
/** @brief PI DMA Busy */
#define PI_STATUS_DMA_BUSY ( 1 << 0 )
/** @brief PI IO Busy */
#define PI_STATUS_IO_BUSY  ( 1 << 1 )
/** @brief PI Error */
#define PI_STATUS_ERROR    ( 1 << 2 )
/** @} */

/** @brief Structure used to interact with the PI registers */
static volatile struct PI_regs_s * const PI_regs = (struct PI_regs_s *)0xa4600000;

/** 
 * @brief Return whether the DMA controller is currently busy
 *
 * @return nonzero if the DMA controller is busy or 0 otherwise
 */
volatile int dma_busy() 
{
    return PI_regs->status & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY);
}

/**
 * @brief Read from a peripheral
 *
 * This function should be used when reading from the cartridge.
 *
 * @param[out] ram_address
 *             Pointer to a buffer to place read data
 * @param[in]  pi_address
 *             Memory address of the peripheral to read from
 * @param[in]  len
 *             Length in bytes to read into ram_address
 */
void dma_read(void * ram_address, unsigned long pi_address, unsigned long len) 
{
    while (dma_busy()) ;
    PI_regs->ram_address = ram_address;
    PI_regs->pi_address = pi_address;
    PI_regs->write_length = len-1;
    while (dma_busy()) ;
}

/**
 * @brief Write to a peripheral
 *
 * This function should be used when writing to the cartridge.
 *
 * @param[in] ram_address
 *            Pointer to a buffer to read data from
 * @param[in] pi_address
 *            Memory address of the peripheral to write to
 * @param[in] len
 *            Length in bytes to write to peripheral
 */
void dma_write(void * ram_address, unsigned long pi_address, unsigned long len) {
    while (dma_busy()) ;
    PI_regs->ram_address = ram_address;
    PI_regs->pi_address = pi_address;
    PI_regs->read_length = len-1;
    while (dma_busy()) ;
}

/**
 * @brief Read a 32 bit integer from a peripheral
 *
 * @param[in] pi_address
 *            Memory address of the peripheral to read from
 *
 * @return The 32 bit value read from the peripheral
 */
uint32_t io_read(uint32_t pi_address) {
    volatile uint32_t *uncached_address = (uint32_t *)(pi_address | 0xa0000000);
    while (dma_busy()) ;
    return *uncached_address;
}

/**
 * @brief Write a 32 bit integer to a peripheral
 *
 * @param[in] pi_address
 *            Memory address of the peripheral to write to
 * @param[in] data
 *            32 bit value to write to peripheral
 */
void io_write(uint32_t pi_address, uint32_t data) {
    volatile uint32_t *uncached_address = (uint32_t *)(pi_address | 0xa0000000);
    while (dma_busy()) ;
    *uncached_address = data;
}

/** @} */ /* dma */
