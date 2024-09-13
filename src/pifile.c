/**
 * @file pifile.c
 * @brief Load a PI-mapped file
 * @ingroup asset
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include "pifile.h"
#include "n64sys.h"
#include "system.h"
#include "dma.h"

/** @brief A PI-mapped open file */
typedef struct {
    uint32_t base;      ///< Base address in the PI bus
    int ptr;            ///< Current pointer
    int size;           ///< File size
} pifile_t;

static void *__pifile_open(char *name, int flags)
{
    if (flags != O_RDONLY) {
        errno = EACCES;
        return NULL;
    }

    // Parse the name in the foramt "ADDR:SIZE" as hexdecimal numbers
    char *end;
    uint32_t base = strtoul(name, &end, 16);
    if (*end != ':') {
        errno = EINVAL;
        return NULL;
    }
    int size = strtoul(end+1, &end, 16);
    if (*end != '\0') {
        errno = EINVAL;
        return NULL;
    }

    pifile_t *file = malloc(sizeof(pifile_t));
    if (!file) {
        errno = ENOMEM;
        return NULL;
    }

    file->base = base;
    file->size = size;
    file->ptr = 0;
    return file;
}

static int __pifile_fstat(void *file, struct stat *st)
{
    pifile_t *f = file;
    memset(st, 0, sizeof(struct stat));
    st->st_size = f->size;
    return 0;
}

static int __pifile_lseek(void *file, int offset, int whence)
{
    pifile_t *f = file;
    switch (whence) {
        case SEEK_SET:
            f->ptr = offset;
            break;
        case SEEK_CUR:
            f->ptr += offset;
            break;
        case SEEK_END:
            f->ptr = f->size + offset;
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    if (f->ptr < 0)
        f->ptr = 0;
    if (f->ptr > f->size)
        f->ptr = f->size;
    return f->ptr;
}

static int __pifile_read(void *file, uint8_t *buf, int len)
{
    pifile_t *f = file;
    if (f->ptr + len > f->size)
        len = f->size - f->ptr;
    if (len <= 0)
        return 0;

    // Check if we can DMA directly to the output buffer
    if ((((f->base + f->ptr) ^ (uint32_t)buf) & 1) == 0) {
        data_cache_hit_writeback_invalidate(buf, len);
        dma_read_async(buf, f->base + f->ptr, len);
        dma_wait();
        f->ptr += len;
    } else {
        // Go through a temporary buffer
        uint8_t *tmp = alloca(512+1);
        if ((f->base + f->ptr) & 1) tmp++;

        while (len > 0) {
            int n = len > 512 ? 512 : len;
            data_cache_hit_writeback_invalidate(tmp, n);
            dma_read_async(tmp, f->base + f->ptr, n);
            dma_wait();
            memcpy(buf, tmp, n);
            buf += n;
            f->ptr += n;
            len -= n;
        }
    }

    return len;
}

static int __pifile_close(void *file)
{
    free(file);
    return 0;
}

static filesystem_t pifile_fs = {
	.open = __pifile_open,
	.fstat = __pifile_fstat,
	.lseek = __pifile_lseek,
	.read = __pifile_read,
	.close = __pifile_close,
};

void pifile_init(void)
{
    attach_filesystem("pi:/", &pifile_fs);
}
