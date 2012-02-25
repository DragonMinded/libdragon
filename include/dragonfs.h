/**
 * @file dragonfs.h
 * @brief DragonFS
 * @ingroup dfs
 */
#ifndef __LIBDRAGON_DRAGONFS_H
#define __LIBDRAGON_DRAGONFS_H

/** 
 * @addtogroup dfs
 * @{
 */

/**
 * @brief Default filesystem location 
 *
 * The default is 1MB into the ROM space, plus the header offset
 */
#define DFS_DEFAULT_LOCATION    0xB0101000

/**
 * @brief Maximum open files in DragonFS
 */
#define MAX_OPEN_FILES      4

/**
 * @brief Maximum filename length
 *
 * This value is due to the direcory structure
 */
#define MAX_FILENAME_LEN    243

/**
 * @brief Maximum depth of directories supported
 */
#define MAX_DIRECTORY_DEPTH 100

/**
 * @name DragonFS Return values
 * @{
 */
/** @brief Success */
#define DFS_ESUCCESS        0
/** @brief Input parameters invalid */
#define DFS_EBADINPUT       -1
/** @brief File does not exist */
#define DFS_ENOFILE         -2
/** @brief Bad filesystem */
#define DFS_EBADFS          -3
/** @brief No memory for operation */
#define DFS_ENOMEM          -4
/** @brief Invalid file handle */
#define DFS_EBADHANDLE      -5
/** @} */

/**
 * @brief Macro to extract the file type from a DragonFS file flag
 *
 * @param[in] x
 *            File flags from DFS entry
 *
 * @return The file type flag
 */
#define FILETYPE(x)         ((x) & 3)

/**
 * @name DragonFS file type flags
 * @{
 */
/** @brief This is a file entry */
#define FLAGS_FILE          0x0
/** @brief This is a directory entry */
#define FLAGS_DIR           0x1
/** @brief This is the end of a directory list */
#define FLAGS_EOF           0x2
/** @} */

/** @} */

#ifdef __cplusplus
extern "C" {
#endif

int dfs_init(uint32_t base_fs_loc);
int dfs_chdir(const char * const path);
int dfs_dir_findfirst(const char * const path, char *buf);
int dfs_dir_findnext(char *buf);

int dfs_open(const char * const path);
int dfs_read(void * const buf, int size, int count, uint32_t handle);
int dfs_seek(uint32_t handle, int offset, int origin);
int dfs_tell(uint32_t handle);
int dfs_close(uint32_t handle);
int dfs_eof(uint32_t handle);
int dfs_size(uint32_t handle);

#ifdef __cplusplus
}
#endif

#endif
