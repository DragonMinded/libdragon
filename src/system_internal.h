/**
 * @file system_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_SYSTEM_INTERNAL_H
#define LIBDRAGON_SYSTEM_INTERNAL_H

#include <stddef.h>
#include <stdlib.h>
#include "system.h"

#ifdef N64
#include "scratch.h"
#endif

void* sbrk_top( int incr );

static inline void *fs_alloc_descriptor(size_t size, int flags)
{
#ifdef N64
    if (flags & O_SHORTLIVED)
        return scratch_malloc(size);
#endif
    return malloc(size);
}

static inline void fs_free_descriptor(void *ptr)
{
#ifdef N64
    if (scratch_owns(ptr)) {
        scratch_free(ptr);
        return;
    }
#endif
    free(ptr);
}

#endif
