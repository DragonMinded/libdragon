#include "rompak_internal.h"
#include "n64sys.h"
#include "dma.h"
#include "debug.h"
#include <stdalign.h>
#include <string.h>
#include <stdlib.h>

#define TOC_MAGIC    0x544F4330   // "TOC0"
#define TOC_ADDR     (0x10001000 + (__rom_end - __libdragon_text_start))

typedef struct {
    uint32_t magic;
    uint32_t toc_size;
    uint32_t entry_size;
    uint32_t num_entries;
} header_t;

typedef struct {
    uint32_t offset;
    char name[];
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
    if (io_read(TOC_ADDR) != TOC_MAGIC) {
        return 0;
    }

    header_t header;
    data_cache_hit_writeback_invalidate(&header, sizeof(header_t));
    dma_read(&header, TOC_ADDR, sizeof(header_t));

    entry_t *entry = alloca(header.entry_size);
    for (int i=0; i < header.num_entries; i++) {
        data_cache_hit_writeback_invalidate(entry, header.entry_size);
        dma_read(entry, TOC_ADDR + sizeof(header_t) + i*header.entry_size, header.entry_size);

        if (extension_match(ext, entry->name))
            return 0x10000000 + entry->offset;
    }

    return 0;
}
