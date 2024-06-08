/**
 * @file dma.h
 * @brief DMA Controller
 * @ingroup dma
 */
#ifndef __LIBDRAGON_DMA_H
#define __LIBDRAGON_DMA_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup dma DMA Controller
 * @ingroup lowlevel
 * @brief DMA functionality for transfers between cartridge space and RDRAM
 *
 * The DMA controller is responsible for handling block and word accesses from
 * the cartridge domain.  Because of the nature of the cartridge interface, code
 * cannot use memcpy or standard pointer accesses on memory mapped to the cartridge.
 * Consequently, the peripheral interface (PI) provides a DMA controller for
 * accessing data.
 *
 * The DMA controller requires no initialization.  Using #dma_read and #dma_write
 * will allow reading from the cartridge and writing to the cartridge respectively
 * in block mode.  #io_read and #io_write will allow a single 32-bit integer to
 * be read from or written to the cartridge.  These are especially useful for
 * manipulating registers on a cartridge such as a gameshark.  Code should never
 * make raw 32-bit reads or writes in the cartridge domain as it could collide with
 * an in-progress DMA transfer or run into caching issues.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#define PI_DRAM_ADDR    ((volatile uint32_t*)0xA4600000)  ///< PI DMA: DRAM address register
#define PI_CART_ADDR    ((volatile uint32_t*)0xA4600004)  ///< PI DMA: cartridge address register
#define PI_RD_LEN       ((volatile uint32_t*)0xA4600008)  ///< PI DMA: read length register
#define PI_WR_LEN       ((volatile uint32_t*)0xA460000C)  ///< PI DMA: write length register
#define PI_STATUS       ((volatile uint32_t*)0xA4600010)  ///< PI: status register

/**
 * @brief Start writing data to a peripheral through PI DMA (low-level)
 *
 * This function should be used when writing to a cartridge peripheral (typically
 * ROM). This function just begins executing a raw DMA transfer, which is
 * well-defined only for RAM addresses which are multiple of 8, ROM addresses
 * which are  multiple of 2, and lengths which are multiple of 2.
 *
 * Use #dma_wait to wait for the end of the transfer.
 *
 *
 * @param[out] ram_address
 *             Pointer to a buffer to read data from (must be 8-byte aligned)
 * @param[in]  pi_address
 *             Memory address of the peripheral to write to (must be 2-byte aligned)
 * @param[in]  len
 *             Length in bytes to write into pi_address (must be multiple of 2)
 */
void dma_write_raw_async(const void *ram_address, unsigned long pi_address, unsigned long len);

/**
 * @brief Write to a peripheral
 *
 * This function should be used when writing to the cartridge.
 *
 * @param[in] ram_address
 *            Pointer to a buffer to read data from
 * @param[in] pi_address
 *            Cartridge address to write to (must be in range (0x10000000-0x1FFFFFFF).
 * @param[in] len
 *            Length in bytes to write to peripheral
 * 
 * @note This function has always had an historical mistake: the pi_address is mangled
 *       to be forced into the ROM area (0x10000000-0x1FFFFFFF). This is wrong as the
 *       PI bus has full 32-bit address, and the same function could have been used
 *       to access the whole range.
 *       If you need to read outside the ROM area, use #dma_write_raw_async instead.
 */
void dma_write(const void * ram_address, unsigned long pi_address, unsigned long len);


/**
 * @brief Start reading data from a peripheral through PI DMA (low-level)
 *
 * This function should be used when reading from a cartridge peripheral (typically
 * ROM). This function just begins executing a raw DMA transfer, which is
 * well-defined only for RAM addresses which are multiple of 8, ROM addresses
 * which are  multiple of 2, and lengths which are multiple of 2.
 * 
 * Use #dma_wait to wait for the end of the transfer.
 * 
 * See #dma_read_async for a higher level primitive which can perform almost
 * arbitrary transfers.
 *
 * @param[out] ram_address
 *             Pointer to a buffer to place read data (must be 8-byte aligned)
 * @param[in]  pi_address
 *             Memory address of the peripheral to read from (must be 2-byte aligned)
 * @param[in]  len
 *             Length in bytes to read into ram_address (must be multiple of 2)
 */
void dma_read_raw_async(void *ram_address, unsigned long pi_address, unsigned long len);

/**
 * @brief Start reading data from a peripheral through PI DMA
 *
 * This function must be used when reading a chunk of data from a cartridge 
 * peripheral (typically, ROM). It is a wrapper over #dma_read_raw_async that allows
 * arbitrary aligned addresses and any length (including odd sizes). For
 * fully-aligned addresses it quickly falls back to #dma_read_raw_async, so it can
 * be used generically as "default" PI DMA transfer function.
 * 
 * The only constraint on alignment is that the RAM and PI addresses must have
 * the same 1-bit misalignment, that is they must either be even addresses or
 * odd addresses. Notice that this function will assert if this constraint is
 * not respected.
 * 
 * Use #dma_wait to wait for the end of the transfer.
 *
 * For non performance sensitive tasks such as reading and parsing data from
 * ROM at loading time, a better option is to use DragonFS, where #dfs_read
 * falls back to a CPU memory copy to realign the data when required.
 * 
 * @param[out] ram_pointer
 *             Pointer to a buffer in RDRAM to place read data
 * @param[in]  pi_address
 *             Memory address of the peripheral to read from
 * @param[in]  len
 *             Length in bytes to read into ram_pointer
 */
void dma_read_async(void *ram_pointer, unsigned long pi_address, unsigned long len);

/** 
 * @brief Read data from a peripheral through PI DMA, waiting for completion.
 * 
 * This function performs a blocking read. See #dma_read_async for more information.
 * 
 * @param[out] ram_address
 *             Pointer to a buffer in RDRAM to place read data
 * @param[in]  pi_address
 *             ROM address to read from (must be in range (0x10000000-0x1FFFFFFF).
 * @param[in]  len
 *             Length in bytes to read into ram_address
 * 
 * @note This function has always had an historical mistake: the pi_address is mangled
 *       to be forced into the ROM area (0x10000000-0x1FFFFFFF). This is wrong as the
 *       PI bus has full 32-bit address, and the same function could have been used
 *       to access the whole range.
 *       If you need to read outside the ROM area, use #dma_read_async instead.
 */
void dma_read(void * ram_address, unsigned long pi_address, unsigned long len);


/** 
 * @brief Wait until an async DMA or I/O transfer is finished.
 */
void dma_wait(void);


/**
 * @brief Read a 32 bit integer from a peripheral using the CPU.
 *
 * @param[in] pi_address
 *            Memory address of the peripheral to read from
 *
 * @return The 32 bit value read from the peripheral
 * 
 * @note This function only works if the specified PI address falls within a range
 *       which is memory mapped on the CPU. See #io_accessible for more information.
 * 
 * @see #io_accessible
 */
uint32_t io_read(uint32_t pi_address);

/**
 * @brief Write a 32 bit integer to a peripheral using the CPU.
 * 
 * Notice that writes are performed asynchronously, so the data might have not been
 * fully written to the peripheral yet when the function returns. Use #dma_wait if
 * you need to wait for the transfer to be finished.
 *
 * @param[in] pi_address
 *            Memory address of the peripheral to write to
 * @param[in] data
 *            32 bit value to write to peripheral
 *
 * @note This function only works if the specified PI address falls within a range
 *       which is memory mapped on the CPU. See #io_accessible for more information.
 *
 * @see #io_accessible
 */
void io_write(uint32_t pi_address, uint32_t data);

/**
 * @brief Check whether the specified PI address can be accessed doing I/O from CPU
 * 
 * The PI bus covers the full 32-bit address range. The full range is only accessible
 * via DMA, though. A part of the range is also memory mapped to the CPU and can be
 * accessed via #io_read and #io_write.
 * 
 * The ranges of PI address that can be accessed via CPU are:
 * 
 *  * 0x0500_0000 - 0x0FFF_FFFF: used by N64DD and SRAM on cartridge
 *  * 0x1000_0000 - 0x1FBF_FFFF: cartridge ROM
 *  * 0x1FD0_0000 - 0x1FFF_FFFF: no known PI peripherals use this
 * 
 * The rest of the 32-bit address range is only accessible via DMA.
 * 
 * Notice also that the range 0x2000_0000 - 0x7FFF_FFFF is theoretically accessible
 * by the CPU but only via 64-bit addressing, so it requires assembly instructions
 * (as the libdragon toolchain uses 32-bit pointers). No known PI peripherals use this
 * range anyway.
 * 
 * This function checks whether the specified address falls into the range accessible
 * via CPU or not.
 * 
 * @param pi_address        PI address to check
 * @return                  True if the address is memory mapped, false if it is not
 */
bool io_accessible(uint32_t pi_address);

__attribute__((deprecated("use dma_wait instead"))) 
volatile int dma_busy(void);


#ifdef __cplusplus
}
#endif

/** @} */ /* dma */

#endif
