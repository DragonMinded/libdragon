/**
 * @file cpak.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Controller Pak raw access
 * @ingroup controllerpak
 * 
 * This module contains lower-level access to the Controller Pak, allowing
 * direct read and write operations to the pak's memory, and similar
 * operations that do not involve the filesystem.
 * 
 * Most applications should use the higher-level filesystem interface
 * provided by cpakfs.h, which allows to access the notes in the controller
 * pak as files in a filesystem.
 */

#ifndef LIBDRAGON_CPAK_H
#define LIBDRAGON_CPAK_H

#include "joypad.h"

/**
 * @brief Read data from a controller pak
 * 
 * This is a low-level operation that reads a sequence of raw bytes from the
 * controller pak, with no filesystem semantics. A typical use case for this
 * function is dumping the contents of a controller pak.
 * 
 * The function will switch to the specified bank (unless it is already in that bank),
 * and then read the specified number of bytes from the specified address.
 * The read can be of any length, and the address does not need to be aligned,
 * but the read cannot cross banks.
 * 
 * @note There is no way for this function to know how many banks the controller pak has.
 *       The caller must ensure that the bank exists, otherwise the behavior is undefined.
 *       You can use the #cpak_probe_banks function to determine the number of banks,
 *       via hardware probing, or #cpakfs_get_stats to get the number of banks
 *       as recorded in the filesystem.
 * 
 * @param port          Joypad port number (#joypad_port_t)
 * @param bank          Bank number to read from (0-61). The caller must ensure
 *                      that the bank exists, otherwise the behavior is undefined.    
 * @param address       Starting address in the controller pak to read from. Allowed
 *                      range is 0-0x7FFF.
 * @param buffer        Buffer to read the data into
 * @param len           Number of bytes to read. The caller must ensure that the length
 *                      does not exceed the bank size (0x8000).
 * @return int          Number of bytes read, or negative value in case of error
 *                      (errno will be set).
 */
int cpak_read(joypad_port_t port, uint8_t bank, uint16_t address, void *buffer, size_t len);

/**
 * @brief Write data to a controller pak
 * 
 * This is a low-level operation that writes a sequence of raw bytes to the
 * controller pak, with no filesystem semantics. A typical use case for this
 * function is restoring a dump to  a controller pak.
 * 
 * The function will switch to the specified bank (unless it is already in that bank),
 * and then write the specified number of bytes to the specified address.
 * The write can be of any length, and the address does not need to be aligned,
 * but the write cannot cross banks.
 *
 * @note There is no way for this function to know how many banks the controller pak has.
 *       The caller must ensure that the bank exists, otherwise the behavior is undefined.
 *       You can use the #cpak_probe_banks function to determine the number of banks,
 *       via hardware probing, or #cpakfs_get_stats to get the number of banks
 *       as recorded in the filesystem.
 * 
 * @param port          Joypad port number (#joypad_port_t)
 * @param bank          Bank number to write to (0-61). The caller must ensure
 *                      that the bank exists, otherwise the behavior is undefined.
 * @param address       Starting address in the controller pak to write to. Allowed
 *                      range is 0-0x7FFF.
 * @param buffer        Buffer to write the data from
 * @param len           Number of bytes to write. The caller must ensure that the length
 *                      does not exceed the bank size (0x8000).
 * @return int          Number of bytes written, or negative value in case of error
 *                      (errno will be set).
 */
int cpak_write(joypad_port_t port, uint8_t bank, uint16_t address, const void *buffer, size_t len);

/**
 * @brief Check if a controller pak is multi-bank or not.
 * 
 * This function does not perform I/O, as the accessory detection code already
 * checked if the controller pak is multi-bank or not. This function only returns
 * this cached information.
 * 
 * @param port          Joypad port to check
 * @return true         if the controller pak is multi-bank, false otherwise
 */
bool cpak_is_multibank(joypad_port_t port);

/**
 * @brief Probe the number of banks in a controller pak
 * 
 * This function probes the number of banks in a controller pak, by attempting
 * to switch to each bank and performing a write test to check if the bank
 * actually exists. It then restores the existing contents in all banks, so
 * that the operation is non-destructive.
 * 
 * The standard controller paks have a single bank of 32 KiB, so this function
 * will always return 1 for them.
 * 
 * @note This is a low-level operation that is useful during formatting or
 *       similar operations. For most applications, just mount the filesystem and
 *       use the #cpakfs_get_stats function to get the number of banks as recorded
 *       in the filesystem.
 * 
 * @note The function uses the first block in each bank. With the standard filesystem,
 *       this block is called "label area", is unused, and is often used/corrupted
 *       by games, so even if the cpak is removed during the operation, the
 *       filesystem will not be corrupted.
 * 
 * @param port      Joypad port to check
 * @return int      Number of banks found, or negative value in case of error
 */
int cpak_probe_banks(joypad_port_t port);

#endif
