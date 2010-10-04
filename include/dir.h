#ifndef __LIBDRAGON_DIR_H
#define __LIBDRAGON_DIR_H

#ifdef __cplusplus
extern "C" {
#endif

#define DT_REG 1
#define DT_DIR 2

typedef struct
{
    char d_name[256];
    int d_type;
} dir_t;

int dir_findfirst( const char * const path, dir_t *dir );
int dir_findnext( const char * const path, dir_t *dir );

#ifdef __cplusplus
}
#endif

#endif
