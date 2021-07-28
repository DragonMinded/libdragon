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
 * the cartridge domain.  Because of the nature of the cartridge interface,
 * code cannot use memcpy or standard pointer accesses on memory mapped to the
 * cartridge. Instead, the console's Peripheral Interface (PI) provides a
 * DMA controller for reading and writing data.
 *
 * The DMA controller requires no initialization.
 *
 * #pi_dma_read and #pi_dma_write allow reading and writing blocks of data from
 * cartridge peripherals such as the ROM, SRAM, or FlashRAM. Refer to the
 * @ref cart "Cartridge interface" for DMA functions in these specific domains.
 *
 * #io_read and #io_write allow safely reading and writing a word from
 * uncached memory. These are especially useful for manipulating registers
 * on a cartridge such as a GameShark. Your code should always use these
 * functions to access the cartridge domains to prevent caching issues
 * and collisions that can occur with an in-progress DMA transfer.
 *
 * @{
 */

#define KSEG1                (0xA0000000)
#define PI_BASE_REG          (0x04600000 | KSEG1)

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
static volatile struct PI_regs_s * const PI_regs = (struct PI_regs_s *)PI_BASE_REG;

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
 * @deprecated Replaced by #cart_rom_read
 *
 * @param[out] dest
 *             Pointer to a buffer to place read data
 * @param[in]  address
 *             Memory address of the peripheral to read from
 * @param[in]  len
 *             Length in bytes to read into dest
 */
__attribute__((deprecated)) void dma_read(void * dest, uint32_t address, uint32_t len)
{
    return cart_rom_read(dest, address, len);
}

/**
 * @brief Write to a peripheral
 *
 * @deprecated Replaced by #cart_rom_write
 *
 * @param[in] src
 *            Pointer to a buffer to read data from
 * @param[in] address
 *            Memory address of the peripheral to write to
 * @param[in] len
 *            Length in bytes to write to peripheral
 */
__attribute__((deprecated)) void dma_write(const void * src, uint32_t address, uint32_t len)
{
    return cart_rom_write(src, address, len);
}

/**
 * @brief Read from a peripheral
 *
 * This function should not be used directly unless you know what you're doing.
 * @see #cart_rom_read
 * @see #cart_dom2_dma_read
 *
 * @param[out] dest
 *             Pointer to a buffer to place read data
 *
 * @param[in]  pi_address
 *             Memory address of the peripheral to read from
 *
 * @param[in]  len
 *             Length in bytes to read into dest
 */
void pi_dma_read(void * dest, uint32_t pi_address, uint32_t len)
{
    assert(len > 1);

    disable_interrupts();

    while (dma_busy()) { /* Spinloop */ }
    MEMORY_BARRIER();
    PI_regs->ram_address = dest;
    MEMORY_BARRIER();
    PI_regs->pi_address = pi_address;
    MEMORY_BARRIER();
    PI_regs->write_length = len-1;
    MEMORY_BARRIER();
    while (dma_busy()) { /* Spinloop */ }

    enable_interrupts();
}

/**
 * @brief Write to a peripheral
 *
 * This function should not be used directly unless you know what you're doing.
 * @see #cart_rom_write
 * @see #cart_dom2_dma_write
 *
 * @param[in] src
 *            Pointer to a buffer to read data from
 *
 * @param[in] pi_address
 *            Memory address of the peripheral to write to
 *
 * @param[in] len
 *            Length in bytes to write to peripheral
 */
void pi_dma_write(const void * src, uint32_t pi_address, uint32_t len)
{
    assert(len > 1);

    disable_interrupts();

    while (dma_busy()) { /* Spinloop */ }
    MEMORY_BARRIER();
    PI_regs->ram_address = (void*)src;
    MEMORY_BARRIER();
    PI_regs->pi_address = pi_address;
    MEMORY_BARRIER();
    PI_regs->read_length = len-1;
    MEMORY_BARRIER();
    while (dma_busy()) { /* Spinloop */ }

    enable_interrupts();
}

/**
 * @brief Read a word from uncached memory safely.
 *
 * Waits for any active DMA transfers to finish.
 *
 * @param[in] address
 *            Memory address to read from
 *
 * @return the word read from uncached memory
 */
uint32_t io_read(uint32_t address)
{
    volatile uint32_t *uncached_address = (uint32_t *)(address | KSEG1);
    uint32_t data = 0;

    disable_interrupts();

    while (dma_busy()) { /* Spinloop */ }
    MEMORY_BARRIER();
    data = *uncached_address;
    MEMORY_BARRIER();

    enable_interrupts();

    return data;
}

/**
 * @brief Write a word to uncached memory safely.
 *
 * Waits for any active DMA transfers to finish.
 *
 * @param[in] address
 *            Memory address to write to
 *
 * @param[in] data
 *            Data to write into uncached memory
 */
void io_write(uint32_t address, uint32_t data)
{
    volatile uint32_t *uncached_address = (uint32_t *)(address | KSEG1);

    disable_interrupts();

    while (dma_busy()) { /* Spinloop */ }
    MEMORY_BARRIER();
    *uncached_address = data;
    MEMORY_BARRIER();

    enable_interrupts();
}

/** @} */ /* dma */
