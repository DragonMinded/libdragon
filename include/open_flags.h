/**
 * @file open_flags.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief libdragon-specific flags for open().
 * @ingroup system
 */
#ifndef __LIBDRAGON_OPEN_FLAGS_H
#define __LIBDRAGON_OPEN_FLAGS_H

/**
 * @brief Hint that an opened file will be closed soon.
 *
 * Filesystems may use this libdragon-specific open flag to allocate per-open
 * descriptors or buffers from the scratch heap instead of the main heap.
 */
#ifndef O_SHORTLIVED
#define O_SHORTLIVED        0x01000000
#endif

#endif
