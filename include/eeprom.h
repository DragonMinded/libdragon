/**
 * @file eeprom.h
 * @author Christopher Bonhage <me@christopherbonhage.com>
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @author thekovic <https://github.com/thekovic>
 * @brief EEPROM support
 * @ingroup eeprom
 */

#ifndef __LIBDRAGON_EEPROM_H
#define __LIBDRAGON_EEPROM_H


/**
 * @defgroup peripherals Peripherals subsystem
 * @ingroup libdragon
 * @brief Management of serial peripherals, reachable through Joybus
 * 
 * This module contains higher-level routine to access different peripherals
 * that are accessible via the JoyBus protocol and the PIF serial chip.
 */

/**
 * @defgroup eeprom EEPROM subsystem
 * @ingroup peripherals
 * @brief Management of EEPROM for saves
 * 
 * This subsytem is made of two different APIs:
 * 
 *  * A lower-level API (eeprom.h) for raw low-level access to EEPROM bytes
 *  * A higher-level API (eepromfs.h) for higher-level access to EEPROM
 *    with structured data.
 *
 * The low-level API exposes:
 * - #eeprom_present and #eeprom_total_blocks to probe EEPROM capacity
 * - #eeprom_read and #eeprom_read_bytes to read save data
 * - #eeprom_write and #eeprom_write_bytes to update save data
 * - #eeprom_is_busy to query whether background flush is still pending
 *
 * Current implementation uses a RAM master-copy with background write-back:
 * reads are served from RAM cache (with lazy block fetch from EEPROM),
 * writes update RAM immediately and are persisted asynchronously.
 * This implies eventual consistency with physical EEPROM.
 *
 * Since writes happen in background, make sure to provide user feedback
 * while writes are happening, checking with #eeprom_is_busy and telling
 * the user that a write is in progress.
 *
 * Shutting down the console while a write is in progress can result in 
 * the block being corrupted. EEPFS (eepromfs.h) provides higher-level
 * filesystem integrity features to mitigate this.
 *
 * Activating EEPROM in emulators or flashcarts
 * --------------------------------------------
 * Libdragon offers support to advertise the need of an EEPROM to emulators
 * and flashcarts. To do so, add <code>N64_ROM_SAVETYPE=eeprom4k</code> or
 * <code>N64_ROM_SAVETYPE=eeprom16k</code> to your Makefile. 
 *
 * This uses the advanced homebrew header which is supported by most emulators
 * and flashcarts compatible with libdragon. It still advised to use the
 * #eeprom_present and #eeprom_total_blocks functions to probe the EEPROM
 * and gracefully handles the case where no EEPROM is present, even if it
 * was requested.
 *
 * Cheap flashcarts might not support the advanced homebrew header,
 * and they most likely default to a 4k EEPROM as default save states for
 * non-commercial ROMs. If your ROM requires a 16k EEPROM to operate
 * properly, users of those flashcarts will need to manually configure the
 * EEPROM type using flashcart-specific instructions.
 */

#include <stdbool.h>
#include <stdint.h>
#include "preview.h"

/**
 * @brief EEPROM Probe Values
 * @see #eeprom_present
 */
typedef enum eeprom_type_t
{
    /** @brief No EEPROM present */
    EEPROM_NONE = 0,
    /** @brief 4 kilobit (64-block) EEPROM present */
    EEPROM_4K   = 1,
    /** @brief 16 kilobit (256-block) EEPROM present */
    EEPROM_16K  = 2
} eeprom_type_t; 

/**
 * @brief Size of an EEPROM save block in bytes.
 */
#define EEPROM_BLOCK_SIZE 8

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Probe the EEPROM interface on the cartridge.
 *
 * Inspect the identifier half-word of the EEPROM status response to
 * determine which EEPROM save type is available (if any).
 *
 * @return which EEPROM type was detected on the cartridge.
 */
eeprom_type_t eeprom_present( void );

/**
 * @brief Determine how many blocks of EEPROM exist on the cartridge.
 *
 * @return 0 if EEPROM was not detected
 *         or the number of EEPROM 8-byte save blocks available.
 */
size_t eeprom_total_blocks( void );

/**
 * @brief Read a block from EEPROM.
 * 
 * This function will read a block of data from EEPROM (8 bytes). Most users
 * will want to use #eeprom_read_bytes instead, which is more flexible.
 *
 * @param[in]  block
 *             Block to read data from. Joybus accesses EEPROM in 8-byte blocks.
 *
 * @param[out] dest
 *             Destination buffer for the eight bytes read from EEPROM.
 *
 * @see #eeprom_read_bytes
 */
void eeprom_read( uint8_t block, void * dest );

/**
 * @brief Write a block to EEPROM.
 * 
 * This function writes a block of data to EEPROM (8 bytes). Most users
 * will want to use #eeprom_write_bytes instead, which is more flexible.
 *
 * @note Writes are eventually consistent to the EEPROM, so they will be
 *       persisted in background to the actual EEPROM. The written data is
 *       immediately visible to read APIs though.
 *
 * @param[in] block
 *            Block to write data to. Joybus accesses EEPROM in 8-byte blocks.
 *
 * @param[in] src
 *            Source buffer for the eight bytes of data to write to EEPROM.
 *
 * @return the EEPROM status byte
 */
uint8_t eeprom_write( uint8_t block, const void * src );

/**
 * @brief Read a buffer of bytes from EEPROM.
 *
 * Read an arbitrary amount of data from the EEPROM. Normally reads are quite
 * fast, even more so if the internal cache is already populated. In general
 * you should be able to use reads without long stalls that might affect
 * graphics or audio.
 *
 * @param[out] dest
 *             Destination buffer to read data into
 * @param[in]  start
 *             Byte offset in EEPROM to start reading data from
 * @param[in]  len
 *             Byte length of data to read into buffer
 */
void eeprom_read_bytes( void * dest, size_t start, size_t len );

/**
 * @brief Write a buffer of bytes to EEPROM.
 *
 * Writes an arbitrary amount of data to the EEPROM. Notice that writes are
 * eventually consistent to the EEPROM: this function will return immediately,
 * but the data will be persisted in background to the actual EEPROM.
 *
 * The background persistence is quite slow. These are measured times for
 * writing the whole EEPROM:
 *
 * * 4k EEPROM: 64 blocks * 6ms = 384ms
 * * 16k EEPROM: 256 blocks * 6ms = 1536ms
 *
 * Make sure the user is aware that a write is in progress and do not turn off
 * the console, otherwise the EEPROM contents might get corrupted. You may want
 * to display a "save in progress" message or indicator on the screen.
 *
 * You can use #eeprom_is_busy to check if a write is in progress at any point.
 *
 * @param[in] src
 *            Source buffer containing data to write
 *
 * @param[in] start
 *            Byte offset in EEPROM to start writing data to
 *
 * @param[in] len
 *            Byte length of the src buffer
 *
 * @see #eeprom_is_busy
 */
void eeprom_write_bytes( const void * src, size_t start, size_t len );

/**
 * @brief Return whether EEPROM flush is currently running.
 * @preview
 *
 * This function reports whether the background flusher is actively persisting
 * data to EEPROM.
 *
 * Typical usage is to continue gameplay/UI while periodically checking this,
 * and show feedback such as "saving..." until it returns false.
 *
 * @return true if background flush is currently active, false otherwise.
 */
LIBDRAGON_PREVIEW_API
bool eeprom_is_busy(void);


/**
 * @brief Wait until the EEPROM is completely idle.
 * @preview
 * 
 * This function will block until the EEPROM is completely idle, i.e. until
 * all background writes have completed.
 *
 * This is similar to making a loop around #eeprom_is_busy, but it allows
 * to switch to other threads while waiting.
 *
 * @note EEPROM writes are quite slow, so this function can block for a
 *       long time, up to hundreds of milliseconds. Therefore it should be
 *       used only when there is no graphics or audio to process.
 */
LIBDRAGON_PREVIEW_API
void eeprom_wait_idle(void);

#ifdef __cplusplus
}
#endif

#endif
