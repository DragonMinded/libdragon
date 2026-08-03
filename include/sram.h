/**
 * @file sram.h
 * @author thekovic <https://github.com/thekovic>
 * @brief SRAM access functions for N64 cartridges
 *
 * This header defines the interface for accessing SRAM present in certain N64
 * cartridges for the purposes of saving game data. It provides functions to
 * initialize the SRAM subsystem, read from SRAM, and write to SRAM.
 *
 */

#ifndef LIBDRAGON_SRAM_H
#define LIBDRAGON_SRAM_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SRAM_ADDRESS 0x08000000 ///< Base address of SRAM in PI address space.

/**
 * @brief Initialize the SRAM subsystem
 * 
 * This function configures the PI DOM2 registers to enable access to the SRAM
 * in the cartridge. It must be called before any other SRAM functions are used.
 */
void sram_init(void);

/**
 * @brief Detect if SRAM is present in the cartridge
 * 
 * This function checks if the cartridge has SRAM available for use.
 * 
 * @return Size of available SRAM if is detected, -1 otherwise.
 */
int sram_detect(void);

/**
 * @brief Read data from SRAM
 * 
 * This function reads data from the SRAM in the cartridge and stores them into
 * the provided destination buffer. To read the entire content of the SRAM,
 * the length should be set to the size of the SRAM as detected by sram_detect().
 * 
 * @param dst Destination buffer to store the read data.
 * @param offset Offset in SRAM to read from. Allowed range is 0 to 0x7FFF.
 * @param len Number of bytes to read.
 * @return Number of bytes read, or negative value in case of error.
 */
int sram_read(void* dst, size_t offset, size_t len);

/**
 * @brief Write data to SRAM
 * 
 * This function writes data to the SRAM in the cartridge from the provided
 * source buffer. To write the entire content of the SRAM, the length should
 * be set to the size of the SRAM as detected by sram_detect().
 * 
 * @param src Source buffer containing the data to write.
 * @param offset Offset in SRAM to write to. Allowed range is 0 to 0x7FFF.
 * @param len Number of bytes to write.
 * @return Number of bytes written, or negative value in case of error.
 */
int sram_write(const void* src, size_t offset, size_t len);

#ifdef __cplusplus
}
#endif

#endif
