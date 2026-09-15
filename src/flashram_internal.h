/**
 * @file flashram_internal.h
 * @brief Internal low-level FlashRAM page/sector interface
 * @ingroup flashram
 *
 * These operations map directly onto the FlashRAM command state machine. They
 * are kept internal for now: the public API (flashram.h) exposes only the
 * SRAM-like byte-range interface (#flashram_read / #flashram_write).
 */

#ifndef LIBDRAGON_FLASHRAM_INTERNAL_H
#define LIBDRAGON_FLASHRAM_INTERNAL_H

#include "flashram.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Read the 8-bit FlashRAM status register.
 *
 * Enters status mode and returns the status byte. The low bits report
 * program/erase busy and last-operation success.
 *
 * @return The 8-bit status byte.
 */
uint8_t flashram_status(void);

/**
 * @brief Clear the FlashRAM status register.
 *
 * Resets the ERASE_OK / PROGRAM_OK latch by writing 0 at the array origin while
 * in status mode. Should be called after a failed erase or program so a later
 * operation's success flag is not masked by the previous result.
 */
void flashram_clear_status(void);

/**
 * @brief Erase the 16 KiB sector that contains a page.
 *
 * The hardware sector-erase command is addressed by page number; the chip
 * derives the enclosing sector from it. Sets every byte of that sector to 0xFF.
 * Blocking.
 *
 * @param page Any page number within the sector to erase (0 to num_pages - 1).
 * @return true on success, false on erase timeout/failure. An out-of-range page asserts.
 */
bool flashram_erase_sector_at_page(unsigned int page);

/**
 * @brief Erase the entire chip.
 *
 * Sets every byte of every sector to 0xFF in a single operation. Blocking.
 * Currently unused by the high-level driver (which erases per sector) but kept
 * for completeness of the low-level protocol.
 *
 * @return true on success, false on erase timeout/failure.
 */
bool flashram_erase_chip(void);

/**
 * @brief Program a single 128-byte page.
 *
 * The page's sector must have been erased first; programming can only clear
 * bits (1 -> 0). Blocking.
 *
 * @param page Page number (0 to num_pages - 1).
 * @param data Pointer to one page (page_size bytes) of data (any alignment).
 * @return true on success, false on program timeout/failure. An out-of-range page asserts.
 */
bool flashram_program_page(unsigned int page, const void* data);

#endif
