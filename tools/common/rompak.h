/*
    rompak: locate files in a ROM's rompak TOC (host tools)
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
#ifndef LIBDRAGON_TOOLS_ROMPAK_H
#define LIBDRAGON_TOOLS_ROMPAK_H

#include "polyfill.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <assert.h>

// Layout mirrors src/rompak.c (runtime) and n64tool.c (build tool):
//  - TOC is searched at 16-byte aligned addresses starting at ROM offset
//    0x1000 (PI address 0x10001000), right after IPL3.
//  - Entry offsets are relative to the start of ROM (file offset 0).
//
// The rompak format has evolved while keeping the same "TOC0" magic, so the
// version is not encoded in the magic and must be detected from the layout:
//
//   Header (16 bytes, big-endian):
//     Legacy:  magic[4], toc_size(u32),  entry_size(u32), num_entries(u32)
//     Current: magic[4], cookie(u32), toc_size(u32), entry_size(u16), num_entries(u16)
//
//   Entry (entry_size bytes, big-endian):
//     v1:      offset(u32), name[]
//     v2/curr: offset(u32), size(u32), name[]

#define ROMPAK_TOC_MAGIC  "TOC0"

static uint32_t rompak_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

// Find a file in the ROM rompak by extension (e.g. ".dfs", ".sym").
// Returns the file offset in the ROM, or -1 on error (message printed to stderr).
// Only big-endian ROMs (.z64 byte order, header 0x80371240) are supported.
__attribute__((used))
static long rompak_find_ext(FILE *f, const char *ext)
{
    uint8_t magic0[4];
    fseek(f, 0, SEEK_SET);
    if (fread(magic0, 1, 4, f) != 4) {
        fprintf(stderr, "Error: cannot read ROM header\n");
        return -1;
    }
    if (rompak_be32(magic0) != 0x80371240) {
        fprintf(stderr, "Error: unsupported ROM byte order (header 0x%08x, expected 0x80371240). "
            "Only big-endian ROMs are supported; please convert byte-swapped (.v64/.n64) ROMs first.\n",
            rompak_be32(magic0));
        return -1;
    }

    long toc_off = -1;
    for (int i = 0; i < 1024; i++) {
        long pos = 0x1000 + (long)i * 16;
        uint8_t magic[4];
        if (fseek(f, pos, SEEK_SET) != 0) break;
        if (fread(magic, 1, 4, f) != 4) break;
        if (magic[0] == 'T' && magic[1] == 'O' && magic[2] == 'C') {
            if (memcmp(magic, ROMPAK_TOC_MAGIC, 4) != 0) {
                fprintf(stderr, "Error: unsupported rompak TOC format (magic '%.4s', expected '%s'). "
                    "This ROM was built with an incompatible version of the libdragon tools.\n",
                    (char*)magic, ROMPAK_TOC_MAGIC);
                return -1;
            }
            toc_off = pos;
            break;
        }
    }
    if (toc_off < 0) {
        fprintf(stderr, "Error: no rompak TOC found in ROM (is this a libdragon ROM?)\n");
        return -1;
    }

    uint8_t hdr[16];
    fseek(f, toc_off, SEEK_SET);
    if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        fprintf(stderr, "Error: cannot read rompak TOC header\n");
        return -1;
    }

    #define TOC_FIELDS_VALID(es, ne) ((es) >= 8 && (es) < 1024 && (ne) > 0 && (ne) < 1024)

    uint32_t cur_entry_size = ((uint32_t)hdr[12] << 8) | hdr[13];
    uint32_t cur_num_entries = ((uint32_t)hdr[14] << 8) | hdr[15];
    uint32_t leg_entry_size = rompak_be32(hdr + 8);
    uint32_t leg_num_entries = rompak_be32(hdr + 12);

    uint32_t entry_size, num_entries;
    if (TOC_FIELDS_VALID(cur_entry_size, cur_num_entries)) {
        entry_size = cur_entry_size;
        num_entries = cur_num_entries;
    } else if (TOC_FIELDS_VALID(leg_entry_size, leg_num_entries)) {
        entry_size = leg_entry_size;
        num_entries = leg_num_entries;
    } else {
        fprintf(stderr, "Error: unrecognized rompak TOC layout (this ROM was built with an "
            "incompatible version of the libdragon tools)\n");
        return -1;
    }
    #undef TOC_FIELDS_VALID

    long found = -1;
    uint8_t *entry = malloc(entry_size + 1);
    assert(entry);
    entry[entry_size] = 0;
    size_t ext_len = strlen(ext);
    for (uint32_t i = 0; i < num_entries; i++) {
        fseek(f, toc_off + sizeof(hdr) + (long)i * entry_size, SEEK_SET);
        if (fread(entry, 1, entry_size, f) != entry_size) {
            fprintf(stderr, "Error: cannot read rompak TOC entry\n");
            free(entry);
            return -1;
        }
        uint32_t offset = rompak_be32(entry);
        // Name follows offset, optionally preceded by a size field.
        // A size field starts with a near-zero byte, unlike a filename.
        int name_col = isprint(entry[4]) ? 4 : 8;
        const char *name = (const char *)(entry + name_col);
        size_t name_len = strnlen(name, entry_size - name_col);
        if (name_len >= ext_len && strcasecmp(name + name_len - ext_len, ext) == 0) {
            found = offset;
            break;
        }
    }
    free(entry);

    if (found < 0) {
        fprintf(stderr, "Error: no %s file found in the ROM rompak\n", ext);
        return -1;
    }
    return found;
}

#endif
