/**
 * @file system_newlib_locks.c
 * @brief newlib lock implementation
 * @ingroup system
 * 
 * This file contains the lock implementation used in libc (newlib). They are
 * used to guarantee thread safety in the C library functions like malloc
 * or file descriptors.
 * 
 * All functions in this file must be defined, so that it takes precedence
 * over libc's default implementation (which is empty).
 */
#include "kernel.h"
#include <stdio.h>

// Toolchains since Sept 2024 build with threads and retargetable locks enabled,
// both of which are required for libc to be thread safe and thus the kernel
// to work correctly (otherwise, kernel_init() will assert).
// We want to be able to build libdragon with old toolchains as well, obviously
// for applications not using the kernel. This file wouldn't complete there
// because on those toolchains, LOCK_T is a typedef to int. So we just disable it.
#ifdef _RETARGETABLE_LOCKING

/** Lock structure: use a kmutex from our kernel */
struct __lock {
    kmutex_t mut;
};

///@cond
struct __lock __lock___sfp_recursive_mutex;
struct __lock __lock___atexit_recursive_mutex;
struct __lock __lock___at_quick_exit_mutex;
struct __lock __lock___malloc_recursive_mutex;
struct __lock __lock___env_recursive_mutex;
struct __lock __lock___tz_mutex;
struct __lock __lock___dd_hash_mutex;
struct __lock __lock___arc4random_mutex;
///@endcond

/** Pool of dynamically allocated lists  */
static struct __lock __libc_mutexes[64];
static uint64_t __libc_mutexes_bitmap = 0;
extern bool __kernel;

/** Alloca a dynamic lock from our static pool */
static struct __lock* __alloc_libc_mutex(void) {
    for (int i = 0; i < 64; i++) {
        if (!(__libc_mutexes_bitmap & (1ull << i))) {
            __libc_mutexes_bitmap |= (1ull << i);
            return &__libc_mutexes[i];
        }
    }
    assert(0);
    return NULL;
}

/** Free a dynamic lock from our static pool */
static void __free_libc_mutex(struct __lock* lock) {
    int i = lock - __libc_mutexes;
    __libc_mutexes_bitmap &= ~(1ull << i);
}

///@cond
// Not part of libdragon public API, no need to document these

void __retarget_lock_init(_LOCK_T *lock) {
    // Allow to allocate the locks even if the kernel is not initialized.
    // For instance, this will happen for stderr. If later the kernel is initialized,
    // the mutex will start working, otherwise they are nops.
    *lock = __alloc_libc_mutex();
    kmutex_init(&(*lock)->mut, KMUTEX_STANDARD);
}
void __retarget_lock_init_recursive(_LOCK_T *lock) {
    *lock = __alloc_libc_mutex();
    kmutex_init(&(*lock)->mut, KMUTEX_RECURSIVE);
}

void __retarget_lock_close(_LOCK_T lock) {
    kmutex_destroy(&lock->mut);
    __free_libc_mutex(lock);
}
void __retarget_lock_close_recursive(_LOCK_T lock) {
    kmutex_destroy(&lock->mut);
    __free_libc_mutex(lock);
}

void __retarget_lock_acquire(_LOCK_T lock) {
    if (!__kernel) return;
    kmutex_lock(&lock->mut);
}
void __retarget_lock_acquire_recursive(_LOCK_T lock) {
    if (!__kernel) return;
    // HACK: some libc locks (eg: malloc) are recursive, but they are statically
    // defined and are designed to be zero-initialized. Since in our implementation
    // zero means non-recursive, we need to set the flag here.
    lock->mut.flags |= KMUTEX_RECURSIVE;
    kmutex_lock(&lock->mut);
}

int __retarget_lock_try_acquire(_LOCK_T lock) {
    if (!__kernel) return 1;
    return kmutex_try_lock(&lock->mut, 0);
}
int __retarget_lock_try_acquire_recursive(_LOCK_T lock) {
    if (!__kernel) return 1;
    lock->mut.flags |= KMUTEX_RECURSIVE;
    return kmutex_try_lock(&lock->mut, 0);
}

void __retarget_lock_release(_LOCK_T lock) {
    if (!__kernel) return;
    kmutex_unlock(&lock->mut);
}
void __retarget_lock_release_recursive (_LOCK_T lock) {
    if (!__kernel) return;
    kmutex_unlock(&lock->mut);
}

///@endcond

#endif