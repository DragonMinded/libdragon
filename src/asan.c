/**
 * @file asan.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief XASAN debug heap allocator and runtime
 * @ingroup asan
 */

#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <reent.h>
#include <unistd.h>
#include <malloc.h>

#include "asan.h"
#include "emux.h"
#include "debug.h"
#include "n64sys.h"

/** @brief Left redzone padding between header and payload (bytes) */
#define ASAN_LEFT_RZ        8

/** @brief Right redzone size (bytes) */
#define ASAN_RIGHT_RZ       16

/** @brief Shadow / alignment granule (bytes) */
#define ASAN_GRANULE        8

/** @brief Block header size (bytes); must be a multiple of ASAN_GRANULE */
#define ASAN_HEADER_SIZE    16

/** @brief Offset from raw block start to user payload for plain malloc */
#define ASAN_PAYLOAD_OFFSET (ASAN_HEADER_SIZE + ASAN_LEFT_RZ)

/** @brief Magic value stored in allocation headers */
#define ASAN_MAGIC          0x4153414eu

typedef struct {
    uint32_t magic;
    void *raw;
    size_t req_size;
} asan_hdr_t;

/** @brief True after successful XASAN initialization */
bool __asan_active;

extern char *__heap_top;
extern char *__heap_end;

extern void *__real__malloc_r(struct _reent *r, size_t n);
extern void __real__free_r(struct _reent *r, void *p);
extern void *__real__realloc_r(struct _reent *r, void *p, size_t n);
extern void *__real__memalign_r(struct _reent *r, size_t align, size_t n);
extern void *__real_sbrk_top(int incr);
extern struct mallinfo __real_mallinfo(void);
extern struct mallinfo __real__mallinfo_r(struct _reent *r);

static inline size_t asan_round8(size_t n)
{
    return (n + ASAN_GRANULE - 1) & ~(size_t)(ASAN_GRANULE - 1);
}

static inline void *asan_align_ptr(void *p, size_t align)
{
    uintptr_t v = (uintptr_t)p;
    return (void *)((v + align - 1) & ~(align - 1));
}

static inline asan_hdr_t *asan_hdr(void *payload)
{
    return (asan_hdr_t *)((char *)payload - ASAN_HEADER_SIZE);
}

static void asan_write_hdr(asan_hdr_t *hdr, void *raw, size_t req_size)
{
    hdr->magic = ASAN_MAGIC;
    hdr->raw = raw;
    hdr->req_size = req_size;
}

static bool asan_valid_hdr(const asan_hdr_t *hdr)
{
    return hdr->magic == ASAN_MAGIC;
}

static void asan_poison_region(const void *addr, size_t size, void (*poison_fn)(const void *, size_t))
{
    if (!size) return;
    uintptr_t base = (uintptr_t)addr & ~(uintptr_t)(ASAN_GRANULE - 1);
    uintptr_t end = ((uintptr_t)addr + size + ASAN_GRANULE - 1) & ~(uintptr_t)(ASAN_GRANULE - 1);
    poison_fn((const void *)base, end - base);
}

void asan_poison(void *ptr, size_t size)
{
    asan_poison_region(ptr, size, emux_xasan_poison_user);
}

void asan_unpoison(void *ptr, size_t size)
{
    asan_poison_region(ptr, size, emux_xasan_unpoison);
}

bool asan_enabled(void)
{
    return __asan_active;
}

__attribute__((constructor(103))) void __asan_init(void)
{
    if (!(emux_detect(1) & EMUX_FEAT1_XASAN))
        return;

    sbrk(0);
    emux_xasan_enable();
    emux_xasan_poison_unalloc(__heap_end, __heap_top - __heap_end);
    __asan_active = true;
}

void *__wrap__malloc_r(struct _reent *r, size_t n)
{
    if (!__asan_active) {
        static bool warned;
        if (!warned) {
            warned = true;
            assertf(0, "N64_ASAN=1 but the emulator/console does not support XASAN");
        }
        return __real__malloc_r(r, n);
    }

    size_t payload_size = asan_round8(n);
    size_t total = ASAN_PAYLOAD_OFFSET + payload_size + ASAN_RIGHT_RZ;
    void *raw;
    void *payload;
    asan_hdr_t *hdr;

    emux_xasan_disable();
    raw = __real__malloc_r(r, total);
    if (!raw) {
        emux_xasan_enable();
        return NULL;
    }

    payload = (char *)raw + ASAN_PAYLOAD_OFFSET;
    hdr = asan_hdr(payload);
    asan_write_hdr(hdr, raw, n);
    emux_xasan_enable();

    emux_xasan_poison_left(raw, (char *)payload - (char *)raw);
    emux_xasan_unpoison(payload, payload_size);
    emux_xasan_poison_right((char *)payload + payload_size, ASAN_RIGHT_RZ);

    return payload;
}

void __wrap__free_r(struct _reent *r, void *p)
{
    if (!p)
        return;

    if (!__asan_active) {
        __real__free_r(r, p);
        return;
    }

    asan_hdr_t *hdr = asan_hdr(p);
    emux_xasan_disable();
    if (asan_valid_hdr(hdr)) {
        void *raw = hdr->raw;
        size_t payload_size = asan_round8(hdr->req_size);
        size_t total = (char *)p + payload_size + ASAN_RIGHT_RZ - (char *)raw;
        emux_xasan_poison_freed(raw, total);
        __real__free_r(r, raw);
    } else {
        __real__free_r(r, p);
    }
    emux_xasan_enable();
}

void *__wrap__calloc_r(struct _reent *r, size_t a, size_t b)
{
    size_t n = a * b;
    void *p = __wrap__malloc_r(r, n);
    if (p)
        memset(p, 0, n);
    return p;
}

void *__wrap__realloc_r(struct _reent *r, void *p, size_t n)
{
    if (!p)
        return __wrap__malloc_r(r, n);
    if (n == 0) {
        __wrap__free_r(r, p);
        return NULL;
    }

    if (!__asan_active)
        return __real__realloc_r(r, p, n);

    emux_xasan_disable();
    size_t old_size = asan_valid_hdr(asan_hdr(p)) ? asan_hdr(p)->req_size : n;
    emux_xasan_enable();
    void *np = __wrap__malloc_r(r, n);
    if (!np)
        return NULL;
    memcpy(np, p, old_size < n ? old_size : n);
    __wrap__free_r(r, p);
    return np;
}

void *__wrap__memalign_r(struct _reent *r, size_t align, size_t n)
{
    if (!__asan_active)
        return __real__memalign_r(r, align, n);

    if (align < ASAN_GRANULE)
        align = ASAN_GRANULE;

    size_t payload_size = asan_round8(n);
    size_t total = ASAN_PAYLOAD_OFFSET + align - 1 + payload_size + ASAN_RIGHT_RZ;
    void *raw;
    void *payload;
    asan_hdr_t *hdr;

    emux_xasan_disable();
    raw = __real__malloc_r(r, total);
    if (!raw) {
        emux_xasan_enable();
        return NULL;
    }

    payload = asan_align_ptr((char *)raw + ASAN_PAYLOAD_OFFSET, align);
    hdr = asan_hdr(payload);
    asan_write_hdr(hdr, raw, n);
    emux_xasan_enable();

    emux_xasan_poison_left(raw, (char *)payload - (char *)raw);
    emux_xasan_unpoison(payload, payload_size);
    emux_xasan_poison_right((char *)payload + payload_size, ASAN_RIGHT_RZ);

    return payload;
}

void *__wrap_sbrk_top(int incr)
{
    void *p = __real_sbrk_top(incr);
    if (p != (void *)-1 && __asan_active)
        emux_xasan_unpoison(p, incr);
    return p;
}

struct mallinfo __wrap__mallinfo_r(struct _reent *r)
{
    if (!__asan_active)
        return __real__mallinfo_r(r);

    emux_xasan_disable();
    struct mallinfo m = __real__mallinfo_r(r);
    emux_xasan_enable();
    return m;
}
