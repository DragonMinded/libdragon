/**
 * @file rompak_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief ROM bundle support
 * @ingroup rompak
 */

#ifndef __LIBDRAGON_ROM_INTERNAL_H
#define __LIBDRAGON_ROM_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "n64types.h"

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

#include "n64types.h"

/**
 * @brief Callback function for rompak walking
 * 
 * @param ctx      Context pointer to pass to the callback
 * @param name     Name of the file
 * @param address  Physical address of the file in the ROM
 * @param size     Size of the file
 * @return true    If the callback should continue walking
 * @return false   If the callback should stop walking
 */
typedef bool (*rompak_walk_callback_t)(void *ctx, const char *name, pi_addr_t address, size_t size);

 /**
  * @brief Walk the rompak and call the callback for each file found
  * 
  * @param callback Callback function to call for each file found
  * @param ctx      Context pointer to pass to the callback
  */
 void rompak_walk(rompak_walk_callback_t callback, void *ctx);


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
 * @param size    Optional pointer to store the size of the file.
 * @return        Physical address of the file in the ROM, or 0 if the file
 *                doesn't exist or the TOC is not present.
 */
pi_addr_t rompak_search_ext(const char *ext, size_t *size);



/**
 * @brief Callback function for version file parsing
 * 
 * @param ctx      Context pointer to pass to the callback
 * @param key      Key of the version entry
 * @param value    Value of the version entry
 */
typedef void (*rompak_version_callback_t)(void *ctx, char *key, char *value);


/**
 * @brief Parse a version file and call the callback for each key-value pair
 *
 * Rompak can contain version files which are JSON files that contain versions
 * related to libdragon, the toolchain or other dependencies. These files
 * have extension ".version".
 *
 * Version files are in JSON format to facilitate parsing also with PC tools.
 * This function provides a minimal, non-validating parser that just extracts
 * keys and values are raw strings.
 * 
 * @param file_addr     Physical address of the version file in the ROM
 * @param size          Size of the version file in bytes   
 * @param callback      Callback function to call for each key-value pair
 * @param ctx           Context pointer to pass to the callback
 */
void rompak_version_parse(pi_addr_t file_addr, size_t size, 
    rompak_version_callback_t callback, void *ctx);

/** @} */

#endif
