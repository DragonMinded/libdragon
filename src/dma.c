/**
 * @file dma.c
 * @brief DMA Controller
 * @ingroup dma
 */
#include "libdragon.h"
#include "regsinternal.h"

/**
 * @defgroup dma DMA Controller
 * @ingroup lowlevel
 * @brief DMA functionality for transfers between cartridge space and RDRAM
 *
 * The DMA controller is responsible for handling block and word accesses from
 * the catridge domain.  Because of the nature of the catridge interface, code
 * cannot use memcpy or standard pointer accesses on memory mapped to the catridge.
 * Consequently, the peripheral interface (PI) provides a DMA controller for
 * accessing data.
 *
 * The DMA controller requires no initialization.  Using #dma_read and #dma_write
 * will allow reading from the cartridge and writing to the catridge respectively
 * in block mode.  #io_read and #io_write will allow a single 32-bit integer to
 * be read from or written to the cartridge.  These are especially useful for
 * manipulating registers on a cartridge such as a gameshark.  Code should never
 * make raw 32-bit reads or writes in the cartridge domain as it could collide with
 * an in-progress DMA transfer or run into caching issues.
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
    disable_interrupts();

    while (dma_busy()) ;
    PI_regs->ram_address = ram_address;
    PI_regs->pi_address = (pi_address | 0x10000000) & 0x1FFFFFFF;
    PI_regs->write_length = len-1;
    while (dma_busy()) ;

    enable_interrupts();
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
void dma_write(void * ram_address, unsigned long pi_address, unsigned long len) 
{
    disable_interrupts();

    while (dma_busy()) ;
    PI_regs->ram_address = ram_address;
    PI_regs->pi_address = (pi_address | 0x10000000) & 0x1FFFFFFF;
    PI_regs->read_length = len-1;
    while (dma_busy()) ;

    enable_interrupts();
}

/**
 * @brief Read a 32 bit integer from a peripheral
 *
 * @param[in] pi_address
 *            Memory address of the peripheral to read from
 *
 * @return The 32 bit value read from the peripheral
 */
uint32_t io_read(uint32_t pi_address)
{
    volatile uint32_t *uncached_address = (uint32_t *)(pi_address | 0xa0000000);
    uint32_t retval = 0;

    disable_interrupts();

    /* Wait until there isn't a DMA transfer and grab a word */
    while (dma_busy()) ;
    retval = *uncached_address;

    enable_interrupts();

    return retval;
}

/**
 * @brief Write a 32 bit integer to a peripheral
 *
 * @param[in] pi_address
 *            Memory address of the peripheral to write to
 * @param[in] data
 *            32 bit value to write to peripheral
 */
void io_write(uint32_t pi_address, uint32_t data) 
{
    volatile uint32_t *uncached_address = (uint32_t *)(pi_address | 0xa0000000);

    disable_interrupts();

    while (dma_busy()) ;
    *uncached_address = data;

    enable_interrupts();
}

/** @} */ /* dma */
