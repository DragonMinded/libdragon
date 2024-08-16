/**
 * @file dfsinternal.h
 * @brief Internal DFS Definitions
 * @ingroup dfs
 */
#ifndef __LIBDRAGON_DFSINTERNAL_H
#define __LIBDRAGON_DFSINTERNAL_H

#include <stdint.h>

#define DFS_MAGIC 0x44465333 //"DFS3"

typedef struct dfs_file_s {
    uint32_t path_hash;
    uint32_t data_ofs;
    uint32_t data_len;
} dfs_file_t;

typedef struct dfs_header_s {
    uint32_t magic;
    uint32_t num_files;
} dfs_header_t;

#endif
