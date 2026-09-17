#include "ringbuffer.h"

void ringbuffer_init(ringbuffer *buf, uint32_t entry_size, uint32_t entry_count)
{
    buf->entry_size = entry_size;
    buf->entry_count = entry_count;
    buf->current_index = 0;
    buf->buffer = malloc_uncached(entry_size * entry_count);
    buf->syncpoints = calloc(entry_count, sizeof(rspq_syncpoint_t));
}

void ringbuffer_free(ringbuffer *buf)
{
    free(buf->syncpoints);
    free_uncached(buf->buffer);
}

void *ringbuffer_alloc_next(ringbuffer *buf)
{
    uint32_t next_index = (buf->current_index + 1) % buf->entry_count;
    rspq_syncpoint_wait(buf->syncpoints[next_index]);

    buf->current_index = next_index;
    return ringbuffer_get_current(buf);
}

void ringbuffer_release_current(ringbuffer *buf)
{
    buf->syncpoints[buf->current_index] = rspq_syncpoint_new();
}

void *ringbuffer_get_current(ringbuffer *buf)
{
    return buf->buffer + (buf->entry_size * buf->current_index);
}
