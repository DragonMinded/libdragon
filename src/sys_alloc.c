/**
 * @file sys_alloc.c
 * @brief Weak default implementation of the tagged subsystem allocation seam.
 *
 * See sys_alloc.h. These defaults provide a standard heap allocation for each
 * subsystem; a build that does not override them uses them directly. An
 * application that wants to place these buffers elsewhere provides a strong
 * ::__sys_alloc / ::__sys_free.
 */
#include "sys_alloc.h"
#include "n64sys.h"     // malloc_uncached / free_uncached
#include <malloc.h>

__attribute__((weak))
void *__sys_alloc(sys_alloc_purpose_t purpose, size_t size, size_t align)
{
    // The YUV interleave buffer is DMA'd by the RSP and sampled by the RDP with
    // no CPU cache involvement, so it must be uncached.
    if (purpose == SYS_ALLOC_YUV)
        return malloc_uncached(size);
    return memalign(align ? align : 16, size);
}

__attribute__((weak))
void __sys_free(sys_alloc_purpose_t purpose, void *ptr)
{
    if (purpose == SYS_ALLOC_YUV) {
        free_uncached(ptr);
        return;
    }
    free(ptr);
}
