/**
 * @file rompak.c
 * @brief ROM bundle support
 * @ingroup rompak
 */
#include "rompak_internal.h"
#include "n64sys.h"
#include "dma.h"
#include "debug.h"
#include <stdalign.h>
#include <string.h>
#include <stdlib.h>

#define TOC_MAGIC    0x544F4330         ///< Magic ID "TOC0"

/** @brief Physical address of the ROMPAK TOC */
#define TOC_ADDR     (0x10001000 + (__rom_end - __libdragon_text_start))

/** @brief ROMPAK TOC header */
typedef struct {
    uint32_t magic;       ///< Magic (#TOC_MAGIC)
    uint32_t toc_size;    ///< Size of the TOC in bytes
    uint32_t entry_size;  ///< Size of an entry of the TOC (in bytes)
    uint32_t num_entries; ///< Number of entries in the TOC
} header_t;

/** @brief ROMPAK TOC entry */
typedef struct {
    uint32_t offset;        ///< Offset of the file in the ROM
    char name[];            ///< Name of the file
} entry_t;

static bool extension_match(const char *ext, const char *name)
{
    int ext_len = strlen(ext);
    int name_len = strlen(name);
    if (ext_len > name_len) {
        return false;
    }
    return strcmp(ext, name + name_len - ext_len) == 0;
}

uint32_t rompak_search_ext(const char *ext)
{
    static bool rompak_corrupted = false;

    if (rompak_corrupted || io_read(TOC_ADDR) != TOC_MAGIC) {
        return 0;
    }

    header_t header;
    data_cache_hit_writeback_invalidate(&header, sizeof(header_t));
    dma_read(&header, TOC_ADDR, sizeof(header_t));

    // These asserts prevent a miscompiled TOC from causing a hard-to-diagnose
    // stack overflow because of alloca. The number 1024 is arbitrary, we just
    // want to protect against important corruptions (eg: little-endian / big-endian mistakes).
    if (header.entry_size >= 1024 || header.num_entries >= 1024) {
        rompak_corrupted = true;
        assertf(header.entry_size < 1024, "Corrupted rompak TOC: entry size too big (0x%lx)", header.entry_size);
        assertf(header.num_entries < 1024, "Corrupted rompak TOC: too many entries (0x%lx)", header.num_entries);
    }

    entry_t *entry = alloca(header.entry_size);
    for (int i=0; i < header.num_entries; i++) {
        data_cache_hit_writeback_invalidate(entry, header.entry_size);
        dma_read(entry, TOC_ADDR + sizeof(header_t) + i*header.entry_size, header.entry_size);

        if (extension_match(ext, entry->name))
            return 0x10000000 + entry->offset;
    }

    return 0;
}
