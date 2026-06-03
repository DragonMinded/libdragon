/**
 * @file asan_weak.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Weak no-op fallbacks for the ASAN public API
 * @ingroup asan
 */

#include "asan.h"

__attribute__((weak)) void asan_poison(void *ptr, size_t size)
{
    (void)ptr;
    (void)size;
}

__attribute__((weak)) void asan_unpoison(void *ptr, size_t size)
{
    (void)ptr;
    (void)size;
}

__attribute__((weak)) bool asan_enabled(void)
{
    return false;
}
