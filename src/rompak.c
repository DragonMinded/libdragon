/**
 * @file rompak.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief ROM bundle support
 * @ingroup rompak
 */
#include "rompak_internal.h"
#include "n64sys.h"
#include "dma.h"
#include "debug.h"
#include <stdalign.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#define TOC_MAGIC    0x544F4330         ///< Magic ID "TOC0"

static pi_addr_t toc_addr = 0;           ///< Address of the TOC in the ROM

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
    uint32_t size;          ///< Size of the file in bytes
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

static bool rompak_get_header(header_t *header)
{
    static bool rompak_corrupted = false;

    if (rompak_corrupted) {
        return false;
    }

    if (toc_addr == 0) {
        // Search for TOC at the beginning of ROM. We want to be lenient to
        // various IPL3 shenigans (eg: IPL3 that doesn't load at 0x1000). In
        // the standard situation, the TOC is at 0x1000 immediately after IPL3,
        // and that's the first position where we look. We then search for
        // a few 16-byte aligned addresses just in case, before punting
        const int TOC_ADDR = 0x10001000;

        for (int i=0; i<1024; i++) {
            if (io_read(TOC_ADDR + i*16) == TOC_MAGIC) {
                toc_addr = TOC_ADDR + i*16;
                break;
            }
        }
        if (!toc_addr) {
            rompak_corrupted = true;
            return 0;
        }
    }

    data_cache_hit_writeback_invalidate(header, sizeof(header_t));
    dma_read(header, toc_addr, sizeof(header_t));

    // These asserts prevent a miscompiled TOC from causing a hard-to-diagnose
    // stack overflow because of alloca. The number 1024 is arbitrary, we just
    // want to protect against important corruptions (eg: little-endian / big-endian mistakes).
    if (header->entry_size >= 1024 || header->num_entries >= 1024) {
        rompak_corrupted = true;
        assertf(header->entry_size < 1024, "Corrupted rompak TOC: entry size too big (0x%lx)", header->entry_size);
        assertf(header->num_entries < 1024, "Corrupted rompak TOC: too many entries (0x%lx)", header->num_entries);
        return false;
    }

    return true;
}

__attribute__((noinline))
void rompak_walk(rompak_walk_callback_t callback, void *ctx)
{
    header_t header;
    if (!rompak_get_header(&header)) {
        return;
    }

    entry_t *entry = alloca(header.entry_size);
    for (int i=0; i < header.num_entries; i++) {
        data_cache_hit_writeback_invalidate(entry, header.entry_size);
        dma_read(entry, toc_addr + sizeof(header_t) + i*header.entry_size, header.entry_size);

        if (!callback(ctx, entry->name, 0x10000000 + entry->offset, entry->size))
            return;
    }
}

pi_addr_t rompak_search_ext(const char *ext, size_t *size)
{
    pi_addr_t addr = 0;

    bool walkfn(void *ctx, const char *name, pi_addr_t address, size_t sz)
    {
        if (extension_match(ext, name)) {
            if (size) *size = sz;
            addr = address;
            return false;
        }
        return true;
    }

    rompak_walk(walkfn, NULL);
    return addr;
}


void rompak_version_parse(pi_addr_t file_addr, size_t size, rompak_version_callback_t callback, void *ctx)
{
	char *data = alloca(size + 1);
    data_cache_hit_writeback_invalidate(data, size);
    dma_read(data, file_addr, size);
	data[size] = 0; /* sentinel to safely terminate tokens */

    /*
     * Minimal JSON parsing for simplified subset: 
     * - One top level object
     * - Keys must be strings
     * - Values are returned as they are, without any parsing
     */
	char *p = data, *end = data + size;
	while (p < end && *p != '{' && *p != '"') p++;
	while (p < end) {
        // Search next quote, assume it's the key
		while (p < end && *p != '"') {
			if (*p == '}') return;
			p++;
		}
		if (p >= end) break;
		p++;

        // Parse key until closing quote
		char *key = p;
		while (p < end) {
			if (*p == '\\') { if (++p < end) p++; continue; }
			if (*p == '"') { *p++ = '\0'; break; }
			p++;
		}
		if (p >= end) break;

        // Search next colon, assume it's the value separator
		while (p < end && *p != ':') {
			if (*p == '}') return;
			p++;
		}
		if (p >= end) break;
		p++;

        // Skip whitespace
		while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
		if (p >= end) continue;

        char *val = p;
		if (*p == '"') {
			// String value: return pointer including quotes
			p++;
			while (p < end) {
				if (*p == '\\') { if (++p < end) p++; continue; }
				if (*p == '"') { p++; break; } // keep closing quote 
				p++;
			}
		} else {
			// "Word" value: return contiguous non-space token up to comma or '}'
			while (p < end) {
				char c = *p;
				if (c == ',' || c == '}' || c == ' ' || c == '\t' || c == '\n' || c == '\r') break;
				p++;
			}
		}
        if (p > end) break;
        *p++ = '\0'; // terminate right after closing quote
        callback(ctx, key, val);
	}
}
