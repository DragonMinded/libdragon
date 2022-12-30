/**
 * @file rompak_internal.h
 * @brief ROM bundle support
 * @ingroup rompak
 */

#ifndef __LIBDRAGON_ROM_INTERNAL_H
#define __LIBDRAGON_ROM_INTERNAL_H

#include <stdint.h>

/**
 * @defgroup rompak ROM bundle support
 * @ingroup lowlevel
 * @brief Rompak functions (private API)
 * 
 * Libdragon ROMs created by n64tool allows to have several data files
 * attached to them. We call this super minimal filesystem "rompak".
 * 
 * The rompak can optionally contain a TOC (table of contents) which is
 * a directory that allows to list the files and know their offset. The
 * libdragon build system (n64.mk) creates this by default.
 * 
 * Rompak is used by libdragon itself to provide a few features. Users
 * should not typically use rompak directly, but rather use the
 * DragonFS (which is itself a single file in the rompak).
 * 
 * @{
 */

/**
 * @brief Search a file in the rompak by extension
 * 
 * Files in the rompak are usually named as the ROM itself, with
 * different extensions. To avoid forcing to embed the ROM name in the
 * code itself, the most typical pattern is to look for a file by
 * its extension.
 * 
 * @param ext     Extension to search for (will be matched case sensitively).
 *                This extension must contain the dot, e.g. ".bin".
 * @return        Physical address of the file in the ROM, or 0 if the file
 *                doesn't exist or the TOC is not present.
 */
uint32_t rompak_search_ext(const char *ext);

/** @} */

#endif
