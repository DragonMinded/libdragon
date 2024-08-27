#include "kernel.h"
#include "kqueue.h"
#include <stdlib.h>

/** @brief A thread-safe FIFO queue */
typedef struct kqueue_s {
    /** @brief The size of the buffer */
    int16_t size;
    /** @brief The head of the queue */
    int16_t head;
    /** @brief The tail of the queue */
    int16_t tail;
    /** @brief The number of elements in the queue */
    int16_t count;
    /** @brief The mutex to protect the queue */
    kmutex_t mutex;
    /** @brief The condition variable to signal when the queue is not empty */
    kcond_t not_empty;
    /** @brief The condition variable to signal when the queue is not full */
    kcond_t not_full;
    /** @brief Buffer of the queue */
    void *buffer[];
} kqueue_t;

kqueue_t *kqueue_new(int size)
{
    kqueue_t *queue = calloc(1, sizeof(kqueue_t) + size * sizeof(void*));
    if (queue)
    {
        queue->size = size;
        kmutex_init(&queue->mutex, KMUTEX_STANDARD);
        kcond_init(&queue->not_empty);
        kcond_init(&queue->not_full);
    }
    return queue;
}

void kqueue_destroy(kqueue_t *queue)
{
    kmutex_destroy(&queue->mutex);
    kcond_destroy(&queue->not_empty);
    kcond_destroy(&queue->not_full);
    free(queue);
}

void kqueue_put(kqueue_t *queue, void *element)
{
    kmutex_lock(&queue->mutex);
    while (queue->count == queue->size)
        kcond_wait(&queue->not_full, &queue->mutex);
    queue->buffer[queue->tail] = element;
    queue->tail = (queue->tail + 1) % queue->size;
    queue->count++;
    kcond_signal(&queue->not_empty);
    kmutex_unlock(&queue->mutex);
}

void *kqueue_get(kqueue_t *queue)
{
    void *element = NULL;
    kmutex_lock(&queue->mutex);
    while (queue->count == 0)
        kcond_wait(&queue->not_empty, &queue->mutex);
    element = queue->buffer[queue->head];
    queue->head = (queue->head + 1) % queue->size;
    queue->count--;
    kcond_signal(&queue->not_full);
    kmutex_unlock(&queue->mutex);
    return element;
}

int kqueue_count(kqueue_t *queue)
{
    return queue->count;
}

int kqueue_size(kqueue_t *queue)
{
    return queue->size;
}

bool kqueue_empty(kqueue_t *queue)
{
    return queue->count == 0;
}

bool kqueue_full(kqueue_t *queue)
{
    return queue->count == queue->size;
}

void *kqueue_peek(kqueue_t *queue)
{
    return queue->count ? queue->buffer[queue->head] : NULL;
}
