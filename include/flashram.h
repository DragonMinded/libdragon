/**
 * @file flashram.h
 * @brief FlashRAM access functions for N64 cartridges
 * @ingroup flashram
 */

#ifndef LIBDRAGON_FLASHRAM_H
#define LIBDRAGON_FLASHRAM_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup flashram FlashRAM
 * @ingroup peripherals
 * @brief FlashRAM save storage for N64 cartridges
 *
 * This module provides access to the FlashRAM save chip found in some N64
 * cartridges (a 1 Mibit / 128 KiB Macronix-family NOR flash). Unlike SRAM,
 * which is a flat, byte-addressable memory (see @ref sram.h), FlashRAM is
 * driven through a small command state machine on the PI bus and can only be
 * erased in 16 KiB sectors and programmed in 128-byte pages.
 *
 * Two levels of API are provided:
 *
 *  - A high-level byte-range interface (#flashram_read / #flashram_write) that
 *    behaves like SRAM: arbitrary offsets and lengths. #flashram_write performs
 *    a read-modify-write over the affected sectors so that data outside the
 *    written range (but within the same erase sector) is preserved.
 *  - A low-level page/sector interface (#flashram_erase_sector /
 *    #flashram_program_page / #flashram_status) that maps directly onto the
 *    chip protocol, for callers that want to manage erase/program themselves.
 *
 * Call #flashram_init once at boot before using any other function, and set
 * `N64_ROM_SAVETYPE = flashram` in your Makefile so emulators and flashcarts
 * configure the correct save chip.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#define FLASHRAM_ADDRESS     0x08000000     ///< Base address of FlashRAM in PI address space (shared with SRAM domain).
#define FLASHRAM_SIZE        0x00020000     ///< Total FlashRAM size (128 KiB / 1 Mibit).
#define FLASHRAM_PAGE_SIZE   0x00000080     ///< Program granularity (128 bytes).
#define FLASHRAM_SECTOR_SIZE 0x00004000     ///< Erase granularity (16 KiB).
#define FLASHRAM_NUM_PAGES   (FLASHRAM_SIZE / FLASHRAM_PAGE_SIZE)     ///< Number of programmable pages (1024).
#define FLASHRAM_NUM_SECTORS (FLASHRAM_SIZE / FLASHRAM_SECTOR_SIZE)   ///< Number of erase sectors (8).

/**
 * @brief Initialize the FlashRAM subsystem
 *
 * Configures the PI DOM2 registers to enable access to the FlashRAM chip and
 * places it into read mode. Must be called before any other FlashRAM function.
 */
void flashram_init(void);

/**
 * @brief Detect whether FlashRAM is present in the cartridge
 *
 * Reads the chip's silicon ID and checks it against the known FlashRAM
 * identifier. Unlike SRAM detection this is non-destructive (it does not write
 * to the save area).
 *
 * @return #FLASHRAM_SIZE if FlashRAM is detected, 0 otherwise.
 */
int flashram_detect(void);

/**
 * @brief Read the FlashRAM status register
 *
 * @return The 32-bit status word. The low bits report program/erase busy and
 *         completion state (see the implementation for the bit layout).
 */
uint32_t flashram_status(void);

/**
 * @brief Read data from FlashRAM
 *
 * Reads a byte range from FlashRAM into @p dst. Any offset and length are
 * accepted; the read is served entirely via PI DMA (FlashRAM array data cannot
 * be read with CPU I/O).
 *
 * @param dst    Destination buffer to store the read data.
 * @param offset Byte offset in FlashRAM to read from (0 to #FLASHRAM_SIZE - 1).
 * @param len    Number of bytes to read.
 * @return Number of bytes read, or a negative value on error (sets @c errno).
 */
int flashram_read(void* dst, size_t offset, size_t len);

/**
 * @brief Write data to FlashRAM (read-modify-write)
 *
 * Writes a byte range to FlashRAM from @p src. Because FlashRAM can only be
 * erased a whole 16 KiB sector at a time, this performs a read-modify-write on
 * every sector the range touches: sectors that are only partially overwritten
 * are read back first so their untouched bytes are preserved. Pages whose final
 * content is entirely erased (all 0xFF) are skipped.
 *
 * This is a blocking operation and can take on the order of tens of
 * milliseconds per touched sector on real hardware.
 *
 * @param src    Source buffer containing the data to write.
 * @param offset Byte offset in FlashRAM to write to (0 to #FLASHRAM_SIZE - 1).
 * @param len    Number of bytes to write.
 * @return Number of bytes written, or a negative value on error (sets @c errno).
 */
int flashram_write(const void* src, size_t offset, size_t len);

/**
 * @brief Erase a single FlashRAM sector
 *
 * Erases one 16 KiB sector, setting all of its bytes to 0xFF. Blocking.
 *
 * @param sector Sector index (0 to #FLASHRAM_NUM_SECTORS - 1).
 * @return true on success, false on timeout/failure (sets @c errno on bad args).
 */
bool flashram_erase_sector(unsigned int sector);

/**
 * @brief Program a single FlashRAM page
 *
 * Programs one 128-byte page. The page's sector must have been erased first;
 * FlashRAM programming can only clear bits (1 -> 0). Blocking.
 *
 * @param page Page index (0 to #FLASHRAM_NUM_PAGES - 1).
 * @param data Pointer to #FLASHRAM_PAGE_SIZE bytes of data (any alignment).
 * @return true on success, false on timeout/failure (sets @c errno on bad args).
 */
bool flashram_program_page(unsigned int page, const void* data);

#ifdef __cplusplus
}
#endif

/** @} */ /* flashram */

#endif
