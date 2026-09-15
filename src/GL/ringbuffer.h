/**
 * @file ringbuffer.h
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 */
#ifndef __GL_RINGBUFFER
#define __GL_RINGBUFFER

#include <stdint.h>
#include <rspq.h>

typedef struct {
    uint8_t *buffer;
    rspq_syncpoint_t *syncpoints;
    uint32_t entry_size;
    uint32_t entry_count;
    uint32_t current_index;
} ringbuffer;


#ifdef __cplusplus
extern "C" {
#endif

void ringbuffer_init(ringbuffer *buf, uint32_t entry_size, uint32_t entry_count);
void ringbuffer_free(ringbuffer *buf);
void *ringbuffer_alloc_next(ringbuffer *buf);
void ringbuffer_release_current(ringbuffer *buf);
void *ringbuffer_get_current(ringbuffer *buf);

#ifdef __cplusplus
}
#endif

#endif
