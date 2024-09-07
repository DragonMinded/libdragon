/**
 * @file pifile.h
 * @brief Load a PI-mapped file
 * @ingroup asset
 * 
 * This module exports a virtual filesystem called "pi:/" that allows to
 * access a portion of the bus as if it was a file (PI-mapped files).
 * 
 * This is sometimes needed for specific use cases, for instance:
 * 
 * * Flashcarts might allow to load data from the SD card directly into
 *   the internal SDRAM, which is mapped to the PI bus. If you want then
 *   to access the file, you can use the "pi:/" filesystem.
 * * Some applications might embed data directly into the ROM without using
 *   the DragonFS, and they might want to access it directly from the PI bus
 *   as if the data was a file.
 * 
 * Normally, you should use the DragonFS filesystem to embed assets in ROM,
 * which is more flexible, but this could be useful for specific cases.
 * 
 * To open a file from the PI bus, use the standard fopen function with
 * a filename starting with "pi:/". For instance, to open a file at address
 * 0x10400000 with length 0xF000, use "pi:/10400000:F000".
 * 
 * Notice that the specified address must be a PI bus address (not a CPU
 * virtual address). All accesses will be performed using DMA so the
 * full 32-bit PI address space is accessible.
 */

#ifndef LIBDRAGON_PIFILE_H
#define LIBDRAGON_PIFILE_H

/**
 * @brief Initialize the PI file system
 * 
 * This function initializes the PI file system. After calling this function,
 * the "pi:/" filesystem will be available to open files via fopen().
 */
void pifile_init(void);

#endif
