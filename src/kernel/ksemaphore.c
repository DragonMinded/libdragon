/**
 * @file ksemaphore.c
 * @brief Kernel Semaphore prmitive
 * @ingroup kernel
 * 
 */
#include "ksemaphore.h"

void ksemaphore_init(ksemaphore_t *sem, int value)
{
    kmutex_init(&sem->mutex, KMUTEX_STANDARD);
    kcond_init(&sem->cond);
    sem->count = value;
}

void ksemaphore_destroy(ksemaphore_t *sem)
{
    kcond_destroy(&sem->cond);
    kmutex_destroy(&sem->mutex);
}

void ksemaphore_wait(ksemaphore_t *sem)
{
    kmutex_lock(&sem->mutex);
    while (sem->count <= 0)
        kcond_wait(&sem->cond, &sem->mutex);
    sem->count--;
    kmutex_unlock(&sem->mutex);
}

bool ksemaphore_try_wait(ksemaphore_t *sem, uint32_t ticks_)
{
    bool ret = false;
    int64_t ticks = ticks_;
    kmutex_lock(&sem->mutex);
    while (sem->count <= 0 && ticks > 0) {
        uint64_t now = get_ticks();
        if (!kcond_wait_timeout(&sem->cond, &sem->mutex, ticks))
            break;
        ticks -= get_ticks() - now;
    }
    if (sem->count > 0) {
        sem->count--;
        ret = true;
    }
    kmutex_unlock(&sem->mutex);
    return ret;
}

void ksemaphore_post(ksemaphore_t *sem)
{
    kmutex_lock(&sem->mutex);
    sem->count++;
    kcond_signal(&sem->cond);
    kmutex_unlock(&sem->mutex);
}
