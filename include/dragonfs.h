#ifndef __LIBDRAGON_DRAGONFS_H
#define __LIBDRAGON_DRAGONFS_H

/* Default is 1MB into the ROM space */
#define DEFAULT_LOCATION    0xB0100000;

/* 4 files ought to be enough for anyone! */
#define MAX_OPEN_FILES      4

/* Due to direcory structure, max filename length */
#define MAX_FILENAME_LEN    243

/* Maximum number of directories supported */
#define MAX_DIRECTORY_DEPTH 100

/* Return values */
#define DFS_ESUCCESS        0
#define DFS_EBADINPUT       -1
#define DFS_ENOFILE         -2
#define DFS_EBADFS          -3
#define DFS_ENOMEM          -4
#define DFS_EBADHANDLE      -5

/* File flags */
#define FILETYPE(x)         ((x) & 3)
#define FLAGS_FILE          0x0
#define FLAGS_DIR           0x1
#define FLAGS_EOF           0x2

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
