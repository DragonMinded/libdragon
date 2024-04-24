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

/** @brief The special ID value in #directory_entry::flags defining the root sector */
#define ROOT_FLAGS      0xFFFFFFFF
/** @brief The special ID value in #directory_entry::next_entry defining the root sector */
#define ROOT_NEXT_ENTRY 0xDEADBEEF
/** @brief Special path value in #directory_entry::path defining the root sector */
#define ROOT_PATH       "DragonFS 2.0"

/** @brief The size of a sector */
#define SECTOR_SIZE     256
/** @brief The size of a sector payload */
#define SECTOR_PAYLOAD  252

/** @brief Representation of a directory entry */
struct directory_entry
{
    /** @brief Offset to next directory entry */
    uint32_t next_entry;
    /** @brief File size and flags.  See #FLAGS_FILE, #FLAGS_DIR and #FLAGS_EOF */
    uint32_t flags;
    /** @brief The file or directory name */
    char path[MAX_FILENAME_LEN+1];
    /** @brief Offset to start sector of the file */
    uint32_t file_pointer;
} __attribute__((__packed__));

_Static_assert(sizeof(struct directory_entry) == SECTOR_SIZE, "invalid directory_entry size");

/** @brief Type definition */
typedef struct directory_entry directory_entry_t;

/** @brief Open file handle structure */
typedef struct dfs_open_file_s
{
    /** @brief The size in bytes of this file */
    uint32_t size;
    /** @brief The offset of the current location in the file */
    uint32_t loc;
    /** @brief The offset within the filesystem where the file is stored */
    uint32_t cart_start_loc;
} dfs_open_file_t;

/** @} */ /* dfs */

#endif
