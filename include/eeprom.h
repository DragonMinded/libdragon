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
 * the block being corrupted.
 */

#include <stdbool.h>
#include <stdint.h>

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
 * This operation will wait for the EEPROM busy bit to clear before reading;
 * you may want to pause audio before calling this to prevent stuttering.
 *
 * This operation is quite fast (compared to writes) as it takes approximately
 * 750 µs.
 *
 * @param[in]  block
 *             Block to read data from. Joybus accesses EEPROM in 8-byte blocks.
 *
 * @param[out] dest
 *             Destination buffer for the eight bytes read from EEPROM.
 */
void eeprom_read( uint8_t block, void * dest );

/**
 * @brief Write a block to EEPROM.
 * 
 * Once a block is written, the EEPROM will be busy for up to 6 milliseconds.
 * This operation will wait for the EEPROM busy bit to clear before writing;
 * you may want to pause audio before calling this to prevent stuttering.
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
 * This is a high-level convenience helper that abstracts away the
 * one-at-a-time EEPROM block access pattern.
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
 * This is a high-level convenience helper that abstracts away the
 * one-at-a-time EEPROM block access pattern.
 *
 * Each EEPROM block write takes approximately 6 milliseconds;
 * this operation may block for a while with large buffer sizes:
 *
 * * 4k EEPROM: 64 blocks * 6ms = 384ms!
 * * 16k EEPROM: 256 blocks * 6ms = 1536ms!
 *
 * You may want to pause audio before calling this.
 *
 * @param[in] src
 *            Source buffer containing data to write
 *
 * @param[in] start
 *            Byte offset in EEPROM to start writing data to
 *
 * @param[in] len
 *            Byte length of the src buffer
 */
void eeprom_write_bytes( const void * src, size_t start, size_t len );

/**
 * @brief Return whether EEPROM flush is currently running.
 *
 * This function reports whether the background flusher is actively persisting
 * data to EEPROM.
 *
 * Typical usage is to continue gameplay/UI while periodically checking this,
 * and show feedback such as "saving..." until it returns false.
 *
 * @return true if background flush is currently active, false otherwise.
 */
bool eeprom_is_busy(void);


/**
 * @brief Wait until the EEPROM is completely idle.
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
void eeprom_wait_idle(void);

#ifdef __cplusplus
}
#endif

#endif
