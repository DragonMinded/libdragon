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
 * cartridges (a 1 Mibit / 128 KiB Macronix- or Matsushita-family NOR flash).
 * Unlike SRAM, which is a flat, byte-addressable memory (see @ref sram.h),
 * FlashRAM is driven through a small command state machine on the PI bus and can
 * only be erased in 16 KiB sectors and programmed in 128-byte pages.
 *
 * The public API is an SRAM-like byte-range interface (#flashram_read /
 * #flashram_write) that accepts arbitrary offsets and lengths. #flashram_write
 * performs a read-modify-write over the affected sectors so that data outside
 * the written range (but within the same erase sector) is preserved.
 *
 * A lower-level page/sector interface that maps directly onto the chip command
 * protocol exists internally (src/flashram_internal.h), but it is not (yet)
 * part of the public API while its shape is being decided.
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
 * @brief How a FlashRAM chip interprets array addresses.
 *
 * "Older" Macronix parts are word-indexed: a PI-bus address selects a 16-bit
 * word, so a logical byte offset must be halved to reach the right word (and
 * the DMA no-cross boundary sits at half the byte value). Byte-indexed parts
 * address individual bytes, like SRAM. The addressing mode is a fixed property
 * of the silicon and is looked up from the chip's silicon ID.
 */
typedef enum
{
    FLASHRAM_ADDRESSING_BYTE = 0,   ///< Addresses select bytes (SRAM-like).
    FLASHRAM_ADDRESSING_WORD = 1,   ///< Addresses select 16-bit words (offset must be halved).
} flashram_addressing_t;

/**
 * @brief Identity and layout of the detected FlashRAM chip.
 *
 * Filled in by #flashram_detect. @p total_size / @p sector_size / @p page_size
 * and the derived counts describe the erase/program geometry; @p addressing
 * tells read/write which address convention the part uses.
 */
typedef struct
{
    uint32_t              type_id;         ///< FLASH_TYPE_ID (expected 0x11118001).
    uint16_t              manufacturer_id; ///< Manufacturer ID (e.g. 0x00C2 Macronix, 0x0032 Matsushita).
    uint16_t              device_id;       ///< Device ID (identifies the specific part).
    flashram_addressing_t addressing;      ///< Byte- vs word-indexed addressing.
    const char*           name;            ///< Human-readable model name ("unknown" if not in the table).
    size_t                total_size;      ///< Total capacity in bytes (#FLASHRAM_SIZE).
    size_t                sector_size;     ///< Erase-sector size in bytes (#FLASHRAM_SECTOR_SIZE).
    size_t                page_size;       ///< Program-page size in bytes (#FLASHRAM_PAGE_SIZE).
    unsigned int          num_sectors;     ///< Number of erase sectors (#FLASHRAM_NUM_SECTORS).
    unsigned int          num_pages;       ///< Number of program pages (#FLASHRAM_NUM_PAGES).
} flashram_info_t;

/**
 * @brief Initialize the FlashRAM subsystem
 *
 * Configures the PI DOM2 registers to enable access to the FlashRAM chip and
 * places it into read mode. Must be called before any other FlashRAM function.
 */
void flashram_init(void);

/**
 * @brief Detect whether FlashRAM is present and identify the chip
 *
 * Reads the chip's silicon ID and checks it against the known FlashRAM type
 * identifier. When present, the manufacturer/device IDs are looked up to
 * determine the model name and (crucially) the byte-vs-word addressing mode,
 * which is cached and used by every subsequent read/write. Non-destructive: it
 * does not write to the save area.
 *
 * @param info Optional out-parameter; when non-NULL and FlashRAM is present, it
 *             is filled with the chip identity and layout.
 * @return true if FlashRAM is detected, false otherwise.
 */
bool flashram_detect(flashram_info_t* info);

/**
 * @brief Read data from FlashRAM
 *
 * Reads a byte range from FlashRAM into @p dst. Any @b even offset and any
 * length are accepted (an odd offset or out-of-range range asserts, since the
 * array is moved over the 2-byte PI bus); the read is served via PI DMA, which
 * transparently handles the word-indexed parts' address halving and the 2-byte
 * granularity.
 *
 * @param dst    Destination buffer to store the read data.
 * @param offset Even byte offset in FlashRAM to read from (0 to #FLASHRAM_SIZE - 1).
 * @param len    Number of bytes to read.
 * @return Number of bytes read (equal to @p len). Invalid arguments assert.
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
 * This is a blocking operation and can take up to a few hundred milliseconds
 * per touched sector on real hardware (sector erase plus up to 128 page
 * programs).
 *
 * @param src    Source buffer containing the data to write.
 * @param offset Byte offset in FlashRAM to write to (0 to #FLASHRAM_SIZE - 1).
 * @param len    Number of bytes to write.
 * @return Number of bytes written, or a negative value if an erase/program fails
 *         on the hardware. Invalid arguments assert.
 */
int flashram_write(const void* src, size_t offset, size_t len);

#ifdef __cplusplus
}
#endif

/** @} */ /* flashram */

#endif
