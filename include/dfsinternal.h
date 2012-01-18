/**
 * @file dfsinternal.h
 * @brief Internal DFS Definitions
 * @ingroup dfs
 */
#ifndef __LIBDRAGON_DFSINTERNAL_H
#define __LIBDRAGON_DFSINTERNAL_H

/**
 * @addtogroup dfs
 * @{
 */

/** @brief The special ID value in #directory_entry::flags defining the master sector */
#define FLAGS_ID        0xFFFFFFFF
/** @brief The special ID value in #directory_entry::next_entry defining the master sector */
#define NEXTENTRY_ID    0xDEADBEEF

/** @brief The size of a sector */
#define SECTOR_SIZE     256
/** @brief The size of a sector payload */
#define SECTOR_PAYLOAD  252

/** @brief Representation of a directory entry */
struct directory_entry
{
    /** @brief File size and flags.  See #FLAGS_FILE, #FLAGS_DIR and #FLAGS_EOF */
    uint32_t flags;
    /** @brief Offset to next directory entry */
    uint32_t next_entry;
    /** @brief The file or directory name */
    char path[MAX_FILENAME_LEN+1];
    /** @brief Offset to start sector of the file */
    uint32_t file_pointer;
} __attribute__((__packed__));

/** @brief Type definition */
typedef struct directory_entry directory_entry_t;

/** @brief Representation of a file sector */
struct file_entry
{
    /** @brief Offset of next sector of the file */
    uint32_t next_sector;
    /** @brief File data */
    uint8_t data[SECTOR_PAYLOAD];
} __attribute__((__packed__));

/** @brief Type definition */
typedef struct file_entry file_entry_t;

/** @brief Open file handle structure */
typedef struct open_file
{
    /** @brief Cached copy of the current sector */
    file_entry_t cur_sector;
    /** @brief Pointer to the first sector */
    file_entry_t *start_sector;
    /** @brief The unique file handle to refer to this file by */
    uint32_t handle;
    /** @brief The size in bytes of this file */
    uint32_t size;
    /** @brief The offset of the current location in the file */
    uint32_t loc;
    /** @brief The sector number of the current sector */
    uint32_t sector_number;
    /** 
     * @brief Padding
     * 
     * If this isn't here, the second file opened will fail due to cur_sector
     * not being on a 8 byte aligned boundary, so I just aligned it to 512
     * bytes. 
     */
    uint8_t padding[236];
} open_file_t;

/** @} */ /* dfs */

#endif
