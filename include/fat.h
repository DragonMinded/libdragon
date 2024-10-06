/**
 * @file fat.h
 * @brief FAT filesystem interface
 * @ingroup lowlevel
 * 
 * This file allows multiple clients to access and use the FatFs library 
 * within libdragon for different scopes.
 * 
 * FatFs is a generic FAT filesystem module for small embedded systems,
 * written by ChaN. It is available at http://elm-chan.org/fsw/ff/00index_e.html.
 * 
 * FatFs is currently used by libdragon for a single use case: to implement
 * access to the SD card in flashcarts. This access is currently implemented
 * by the debug library (debug.h), initialized via #debug_init_sdfs.
 * 
 * The APIs exported by this file are useful only if you need to mount a FAT
 * volume coming from some other sources (eg: a FAT image within a ROM, or
 * a FAT volume accessible via some custom USB protocol, or whatever else).
 * If you need this, call #fat_mount to configure a FatFs volume, which you
 * will then be able to access via standard C file operations.
 */

#ifndef LIBDRAGON_FAT_H
#define LIBDRAGON_FAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Interface for disk operations required to implement a volume.
 * 
 * These interfaces are identical to diskio.h from FatFs. It basically just
 * adds one indirection layer to it, to dispatch the calls to the correct
 * volume.
 */
typedef struct {
	///@cond
	int (*disk_initialize)(void);
	int (*disk_status)(void);
	int (*disk_read)(uint8_t* buff, int sector, int count);
	int (*disk_read_sdram)(uint8_t* buff, int sector, int count);
	int (*disk_write)(const uint8_t* buff, int sector, int count);
	int (*disk_ioctl)(uint8_t cmd, void* buff);
	///@endcond
} fat_disk_t;


/** 
 * @brief Mount the volume only when it is accessed for the first time.
 * 
 * This flag can be passed to #fat_mount to defer the actual mounting of the
 * volume until it is accessed for the first time. This can be useful to
 * avoid blocking the application for a long time during the mount operation.
 * 
 * When you pass this flag, fat_mount will return immediately after configuring
 * the internal data structure, but no I/O operation will be performed on the
 * volume.
 */
#define FAT_MOUNT_DEFERRED        0x0001  


/**
 * @brief Mount a new FAT volume through the FatFs library.
 * 
 * This function allows to mount a new FAT volume through the FatFs library.
 * Access to the actual disk is done through the provided disk operations,
 * so that the volume can be backed by any kind of storage.
 * 
 * After calling this function, you will be able to access the files on the
 * volume using two different APIs:
 * 
 * * Standard C file operations (fopen, fread, fwrite, fclose, etc), or 
 *   POSIX file operations (open, read, write, close, etc). This is the preferred
 *   way to access files, as it is the most portable and the most familiar to
 *   most developers. Files will be accessed using the prefix provided in the
 *   call to this function. For instance, if you provide "sd:" as the prefix,
 *   you will be able to access the files on the volume using paths like
 *   "sd:/path/to/file.txt".
 * * Direct FatFs API calls. This is the low-level API provided by the FatFs
 *   library itself. It is less portable and less familiar to most developers,
 *   but it might be required for certain very low level operations, like
 *   inspecting the actual FAT chains of a file. The volume ID for direct
 *   FatFs API usage will be returned by this function. To use this API, you
 *   will need to include the FatFs headers in your source files ("fatfs/fs.h"),
 *   and then refer to filename paths using the volume ID. For instance, if
 *   the volume ID is 2, you will be able to access the files on the volume
 *   using paths like "2:/path/to/file.txt".
 * 
 * @param prefix            Prefix to use for the volume in stdio calls like 
 *                          fopen (eg: "sd:"). If this is NULL, the volume
 *                          will not be accessible via standard C API, but only
 *                          via the FatFs API.
 * @param disk              Table of disk operations to use for this volume
 * @param flags             Flags to affect the behavior of the mount operation.
 *                          You can pass 0 as default, or one of the various
 *                          FAT_MOUNT_ flags.
 * 
 * @return >= 0 on success: the value will be the volume ID for direct FatFs API
 *         usage
 * @return -1 on mount failure (errno will be set). Eg: corrupted FAT header
 */
int fat_mount(const char *prefix, const fat_disk_t* disk, int flags);

#ifdef __cplusplus
}
#endif

#endif
