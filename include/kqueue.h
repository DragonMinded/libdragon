#ifndef LIBDRAGON_KERNEL_KQUEUE_H
#define LIBDRAGON_KERNEL_KQUEUE_H

#include <stdbool.h>

/** 
 * @brief A thread-safe FIFO queue 
 * 
 * This structure is used to create a thread-safe FIFO queue. The queue
 * is implemented as a circular buffer of void* pointers.
 * 
 * It is possible to enqueue and dequeue elements from the queue, and
 * the queue will block if the queue is full or empty, respectively.
 * 
 * The size of the queue is fixed at creation time, and cannot be changed
 * afterwards.
 */
typedef struct kqueue_s kqueue_t;

/**
 * @brief Create a new queue
 * 
 * This function creates a new queue with the specified size. The size
 * is the number of elements that the queue can hold.
 * 
 * @param[in] size 	Size of the queue in number of elements
 * 
 * @return A pointer to the queue structure created
 */
kqueue_t *kqueue_new(int size);

/**
 * @brief Destroy a queue
 * 
 * This function destroys a queue, freeing all the resources associated
 * with it.
 * 
 * @param[in] queue 	Pointer to the queue structure
 */
void kqueue_destroy(kqueue_t *queue);

/**
 * @brief Add an element to the queue
 * 
 * This function adds an element to the queue. If the queue is full, the
 * function will block until there is space in the queue.
 * 
 * @param[in] queue 	Pointer to the queue structure
 * @param[in] element 	Pointer to the element to add
 */
void kqueue_put(kqueue_t *queue, void *element);

/**
 * @brief Remove an element from the queue
 * 
 * This function removes an element from the queue. If the queue is empty,
 * the function will block until there is an element in the queue.
 * 
 * @param[in] queue 	Pointer to the queue structure
 * 
 * @return Pointer to the element removed
 */
void *kqueue_get(kqueue_t *queue);

/**
 * @brief Get the number of elements in the queue
 * 
 * @param[in] queue 	Pointer to the queue structure
 * 
 * @return The number of elements in the queue
 */
int kqueue_count(kqueue_t *queue);

/**
 * @brief Get the size of the queue
 * 
 * @param[in] queue 	Pointer to the queue structure
 * 
 * @return The size of the queue
 */
int kqueue_size(kqueue_t *queue);

/**
 * @brief Check if the queue is empty
 * 
 * @param[in] queue 	Pointer to the queue structure
 * 
 * @return true if the queue is empty, false otherwise
 */
bool kqueue_empty(kqueue_t *queue);

/**
 * @brief Check if the queue is full
 * 
 * @param[in] queue 	Pointer to the queue structure
 * 
 * @return true if the queue is full, false otherwise
 */
bool kqueue_full(kqueue_t *queue);

/**
 * @brief Peek at the element at the head of the queue
 * 
 * @param[in] queue 	Pointer to the queue structure
 * 
 * @return Element at the head of the queue, or NULL if the queue is empty
 */
void *kqueue_peek(kqueue_t *queue);

#endif
