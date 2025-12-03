/**
 * @file dfs_internal.h
 * @author Jennifer Taylor <dragonminded@dragonminded.com>
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @author Liam Coleman <gamemasterplc@gmail.com>
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
#define ROOT_PATH       "DragonFS 2.1"


/** @brief The size of a sector */
#define MAX_DIRENT_SIZE     256

/** @brief Prime number used for hash lookups */
#define DFS_LOOKUP_PRIME 31

/** @brief Representation of a directory entry */
struct directory_entry
{
    /** @brief Offset to next directory entry */
    uint32_t next_entry;
    /** @brief File size and flags.  See #FLAGS_FILE, #FLAGS_DIR and #FLAGS_EOF */
    uint32_t flags;
    /** @brief Offset to start sector of the file */
    uint32_t file_pointer;
    /** @brief The file or directory name */
    char path[];
};

/** @brief Size of the ID dirent entry (beginning of the filesystem) */
#define ID_DIRENT_SIZE   (((sizeof(directory_entry_t) + strlen(ROOT_PATH) + 1) + 1) / 2 * 2)

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

/** @brief Data for a single file in dfs_lookup_t */
typedef struct dfs_lookup_file_s {
    /** @brief Hash of the path string */
    uint32_t path_hash;
    /** @brief Top 12 bits: length of the path string; lowest 20 bits: offset of the path string */
    uint32_t path_ofs;
    /** @brief Data offset for file */
    uint32_t data_ofs;
    /** @brief Data length for file */
    uint32_t data_len;
} dfs_lookup_file_t;

/** @brief Data for DFS file lookup used to speed up file open performance */
typedef struct dfs_lookup_s {
    /** @brief Number of files */
    uint32_t num_files;
    /** @brief Base offset for path data */
    uint32_t path_ofs;
    /** @brief Array of file entries */
    dfs_lookup_file_t files[];
} dfs_lookup_t;

/** @} */ /* dfs */

#endif