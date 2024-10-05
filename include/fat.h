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
 * If you need this, call #fat_mount to configure a FatFs volume (eg:
 * #FAT_VOLUME_CUSTOM), which you will then be able to access via standard
 * C file operations.
 */

#ifndef LIBDRAGON_FAT_H
#define LIBDRAGON_FAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * @brief Definition of the various FAT volumes.
 * 
 * Currently, we only allocate a single volume, which is used for the SD card
 * in flashcarts.
 * 
 * If you need to mount a FAT volume coming from some other sources, you
 * can use the #FAT_VOLUME_CUSTOM volume.
 */
enum {
    FAT_VOLUME_SD = 0,          ///< Volume for SD cards
    FAT_VOLUME_CUSTOM = 1,      ///< Custom volume, free for usage
};

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
 * @brief Mount a new FAT volume through the FatFs library.
 * 
 * This function allows to mount a new FAT volume through the FatFs library.
 * Access to the actual disk is done through the provided disk operations,
 * so that the volume can be backed by any kind of storage.
 * 
 * After calling this function, you will be able to access the volume via
 * standard C file operations.
 * 
 * @param prefix            Prefix to use for the volume in stdio calls like 
 *                          fopen (eg: "sd:"). If this is NULL, the volume
 *                          will not be accessible via stdio calls, but only
 *                          via the FatFs API.
 * @param fatfs_volume_id   ID of the volume within FatFs (eg: #FAT_VOLUME_CUSTOM)
 * @param disk              Table of disk operations to use for this volume
 * 
 * @return 0 on success, -1 on failure (errno will be set)
 */
int fat_mount(const char *prefix, int fatfs_volume_id, const fat_disk_t* disk);

#ifdef __cplusplus
}
#endif

#endif
