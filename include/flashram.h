/**
 * @file flashram.h
 * @brief FlashRAM access functions for N64 cartridges
 * @ingroup flashram
 */

#ifndef LIBDRAGON_FLASHRAM_H
#define LIBDRAGON_FLASHRAM_H

#include "dma.h"
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
 * The API is an SRAM-like byte-range interface (#flashram_read /
 * #flashram_write) that accepts arbitrary offsets and lengths. #flashram_write
 * performs a read-modify-write over the affected sectors so that data outside
 * the written range (but within the same erase sector) is preserved.
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

/**
 * @brief FlashRAM chip layout, expressed as bits-per-component.
 *
 * Encoding each dimension as a power of two describes both byte- and
 * word-indexed parts uniformly and generalizes to other page/sector sizes.
 * Some are word-indexed (@p unit_bits = 1): a PI-bus address selects a
 * 16-bit word, so a logical byte offset is halved to reach the right word.
 * Byte-indexed parts (@p unit_bits = 0) address individual bytes, like
 * SRAM. The layout is a fixed property of the silicon, looked up from its
 * silicon ID.
 *
 * Derived quantities (all powers of two):
 *  - unit size (bytes)   = 1 << unit_bits
 *  - page size (bytes)   = 1 << (unit_bits + offset_bits)
 *  - pages per sector    = 1 << page_bits
 *  - sector size (bytes) = 1 << (unit_bits + offset_bits + page_bits)
 *  - sectors per chip    = 1 << sector_bits
 *  - total size (bytes)  = 1 << (unit_bits + offset_bits + page_bits + sector_bits)
 *  - DMA read boundary (bytes, not to be crossed by a single transfer)
 *                        = 1 << (unit_bits + offset_bits + read_page_bits)
 */
typedef struct
{
    uint8_t unit_bits;       ///< log2(bytes per addressable unit): 0 = byte-indexed, 1 = word-indexed.
    uint8_t offset_bits;     ///< log2(addressable units per page).
    uint8_t page_bits;       ///< log2(pages per sector).
    uint8_t sector_bits;     ///< log2(sectors per chip).
    uint8_t read_page_bits;  ///< log2(pages) of the DMA read boundary a single transfer must not cross.
} flashram_layout_t;

/**
 * @brief Identity and layout of the detected FlashRAM chip.
 *
 * Filled in by #flashram_init. @p layout describes the addressing convention
 * and geometry as bits-per-component; @p total_size / @p sector_size /
 * @p page_size and the derived counts are that geometry expanded into bytes for
 * convenience.
 */
typedef struct
{
    uint32_t          type_id;         ///< FLASH_TYPE_ID (expected 0x11118001).
    uint16_t          manufacturer_id; ///< Manufacturer ID (e.g. 0x00C2 Macronix, 0x0032 Matsushita).
    uint16_t          device_id;       ///< Device ID (identifies the specific part).
    flashram_layout_t layout;          ///< Chip layout: byte/word addressing and geometry.
    const char*       name;            ///< Human-readable model name ("unknown" if not in the table).
    size_t            total_size;      ///< Total capacity in bytes (derived from @p layout).
    size_t            sector_size;     ///< Erase-sector size in bytes (derived from @p layout).
    size_t            page_size;       ///< Program-page size in bytes (derived from @p layout).
    unsigned int      num_sectors;     ///< Number of erase sectors (derived from @p layout).
    unsigned int      num_pages;       ///< Number of program pages (derived from @p layout).
} flashram_info_t;

/**
 * @brief Initialize the FlashRAM subsystem and detect the chip
 *
 * Configures the PI DOM2 registers, then reads the chip's silicon ID to check
 * for FlashRAM and, when present, look up the model: its name and (crucially)
 * its layout, which is cached and drives every subsequent read/write. Leaves the
 * chip in read mode. Must be called before any other FlashRAM function.
 *
 * Detection is non-destructive (it does not write to the save area). The layout
 * must be known for reads/writes to address the array correctly on word-indexed
 * parts, which is why it is resolved here rather than in a separate step.
 *
 * @param timings Optional PI DOM2 (#pi_dom_timings_t) bus timings; pass NULL for
 *                the standard defaults (latency 0x05, pulse 0x0C, page size 0x0F,
 *                release 0x02). @p page_size must stay at its maximum (0x0F) so
 *                the PI never auto-splits DMAs and the driver can split them
 *                itself -- required for word-indexed parts.
 * @param info    Optional out-parameter; when non-NULL and FlashRAM is present,
 *                it is filled with the chip identity and layout.
 * @return true if FlashRAM is present, false otherwise.
 */
bool flashram_init(const pi_dom_timings_t* timings, flashram_info_t* info);

/**
 * @brief Read data from FlashRAM
 *
 * Reads a byte range from FlashRAM into @p dst. Any @b even offset and any
 * length are accepted (an odd offset or out-of-range range asserts)
 *
 * @param dst    Destination buffer to store the read data.
 * @param offset Even byte offset in FlashRAM to read from (0 to the detected total_size - 1).
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
 * @param offset Byte offset in FlashRAM to write to (0 to the detected total_size - 1).
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
