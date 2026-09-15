/*
    n64elfcompress: compress ELF files for the N64
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <inttypes.h>

#include "../common/binout.c"
#include "../common/assetcomp.h"
#include "../common/mips_elf.h"
#include "../common/polyfill.h"

#define INCBIN_SILENCE_BITCODE_WARNING
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_PREFIX g_
#include "../common/incbin.h"

INCBIN(mips_decomp_l1, "common/mips_decomp_l1.bin");
INCBIN(mips_decomp_l2, "common/mips_decomp_l2.bin");
INCBIN(mips_decomp_l3, "common/mips_decomp_l3.bin");

struct decomp_s {
    const uint8_t *data;
    uint32_t size;
} decompressors[MAX_COMPRESSION+1];

#define PT_MIPS_REGINFO     0x70000000
#define PT_MIPS_RTPROC      0x70000001
#define PT_MIPS_OPTIONS     0x70000002
#define PT_MIPS_ABIFLAGS    0x70000003

#define PT_N64              (PT_LOOS + 0x4e36340)
#define PT_N64_DECOMP       (PT_N64  + 1)

#define PF_N64_COMPRESSED   0x1000

int flag_verbose = 0;

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf_header_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} elf_phdr_t;

typedef struct {
    bool elf64;
    elf_header_t header;
    elf_phdr_t *phdrs;
    uint8_t **phdr_body;
} elf_t;

#define ELF32_EHDR_SIZE  52
#define ELF64_EHDR_SIZE  64
#define ELF32_PHDR_SIZE  32
#define ELF64_PHDR_SIZE  56

static uint16_t read_be16(const uint8_t *ptr)
{
    return (uint16_t)ptr[0] << 8 | ptr[1];
}

static uint32_t read_be32(const uint8_t *ptr)
{
    return (uint32_t)ptr[0] << 24 |
           (uint32_t)ptr[1] << 16 |
           (uint32_t)ptr[2] << 8 |
           ptr[3];
}

static uint64_t read_be64(const uint8_t *ptr)
{
    return (uint64_t)read_be32(ptr) << 32 | read_be32(ptr + 4);
}

static void write_be16(uint8_t *ptr, uint16_t value)
{
    ptr[0] = value >> 8;
    ptr[1] = value;
}

static void write_be32(uint8_t *ptr, uint32_t value)
{
    ptr[0] = value >> 24;
    ptr[1] = value >> 16;
    ptr[2] = value >> 8;
    ptr[3] = value;
}

static void write_be64(uint8_t *ptr, uint64_t value)
{
    write_be32(ptr, value >> 32);
    write_be32(ptr + 4, value);
}

// Printf if verbose
void verbose(const char *fmt, ...) {
    if (flag_verbose) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
}

void print_args(char *name)
{
    fprintf(stderr, "Usage: %s [flags] <input elfs>\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -v/--verbose                Verbose output\n");
    fprintf(stderr, "   -o/--output <dir>           Specify output directory (default: .)\n");
    fprintf(stderr, "   -c/--compress <level>       Compression level (default: %d)\n", DEFAULT_COMPRESSION);
    fprintf(stderr, "\n");
}

static void header_decode(elf_t *elf, const uint8_t *raw)
{
    elf_header_t *hdr = &elf->header;
    memcpy(hdr->e_ident, raw, EI_NIDENT);
    hdr->e_type      = read_be16(raw + 16);
    hdr->e_machine   = read_be16(raw + 18);
    hdr->e_version   = read_be32(raw + 20);
    hdr->e_entry     = elf->elf64 ? read_be64(raw + 24) : read_be32(raw + 24);
    hdr->e_phoff     = elf->elf64 ? read_be64(raw + 32) : read_be32(raw + 28);
    hdr->e_shoff     = elf->elf64 ? read_be64(raw + 40) : read_be32(raw + 32);
    hdr->e_flags     = read_be32(raw + (elf->elf64 ? 48 : 36));
    hdr->e_ehsize    = read_be16(raw + (elf->elf64 ? 52 : 40));
    hdr->e_phentsize = read_be16(raw + (elf->elf64 ? 54 : 42));
    hdr->e_phnum     = read_be16(raw + (elf->elf64 ? 56 : 44));
    hdr->e_shentsize = read_be16(raw + (elf->elf64 ? 58 : 46));
    hdr->e_shnum     = read_be16(raw + (elf->elf64 ? 60 : 48));
    hdr->e_shstrndx  = read_be16(raw + (elf->elf64 ? 62 : 50));
}

static void header_encode(const elf_t *elf, uint8_t *raw)
{
    const elf_header_t *hdr = &elf->header;
    memset(raw, 0, elf->elf64 ? ELF64_EHDR_SIZE : ELF32_EHDR_SIZE);
    memcpy(raw, hdr->e_ident, EI_NIDENT);
    write_be16(raw + 16, hdr->e_type);
    write_be16(raw + 18, hdr->e_machine);
    write_be32(raw + 20, hdr->e_version);
    if (elf->elf64) {
        write_be64(raw + 24, hdr->e_entry);
        write_be64(raw + 32, hdr->e_phoff);
        write_be64(raw + 40, hdr->e_shoff);
        write_be32(raw + 48, hdr->e_flags);
        write_be16(raw + 52, hdr->e_ehsize);
        write_be16(raw + 54, hdr->e_phentsize);
        write_be16(raw + 56, hdr->e_phnum);
        write_be16(raw + 58, hdr->e_shentsize);
        write_be16(raw + 60, hdr->e_shnum);
        write_be16(raw + 62, hdr->e_shstrndx);
    } else {
        write_be32(raw + 24, hdr->e_entry);
        write_be32(raw + 28, hdr->e_phoff);
        write_be32(raw + 32, hdr->e_shoff);
        write_be32(raw + 36, hdr->e_flags);
        write_be16(raw + 40, hdr->e_ehsize);
        write_be16(raw + 42, hdr->e_phentsize);
        write_be16(raw + 44, hdr->e_phnum);
        write_be16(raw + 46, hdr->e_shentsize);
        write_be16(raw + 48, hdr->e_shnum);
        write_be16(raw + 50, hdr->e_shstrndx);
    }
}

static void phdr_decode(const elf_t *elf, elf_phdr_t *phdr, const uint8_t *raw)
{
    phdr->p_type = read_be32(raw);
    if (elf->elf64) {
        phdr->p_flags  = read_be32(raw + 4);
        phdr->p_offset = read_be64(raw + 8);
        phdr->p_vaddr  = read_be64(raw + 16);
        phdr->p_paddr  = read_be64(raw + 24);
        phdr->p_filesz = read_be64(raw + 32);
        phdr->p_memsz  = read_be64(raw + 40);
        phdr->p_align  = read_be64(raw + 48);
    } else {
        phdr->p_offset = read_be32(raw + 4);
        phdr->p_vaddr  = read_be32(raw + 8);
        phdr->p_paddr  = read_be32(raw + 12);
        phdr->p_filesz = read_be32(raw + 16);
        phdr->p_memsz  = read_be32(raw + 20);
        phdr->p_flags  = read_be32(raw + 24);
        phdr->p_align  = read_be32(raw + 28);
    }
}

static void phdr_encode(const elf_t *elf, const elf_phdr_t *phdr, uint8_t *raw)
{
    memset(raw, 0, elf->elf64 ? ELF64_PHDR_SIZE : ELF32_PHDR_SIZE);
    write_be32(raw, phdr->p_type);
    if (elf->elf64) {
        write_be32(raw + 4, phdr->p_flags);
        write_be64(raw + 8, phdr->p_offset);
        write_be64(raw + 16, phdr->p_vaddr);
        write_be64(raw + 24, phdr->p_paddr);
        write_be64(raw + 32, phdr->p_filesz);
        write_be64(raw + 40, phdr->p_memsz);
        write_be64(raw + 48, phdr->p_align);
    } else {
        write_be32(raw + 4, phdr->p_offset);
        write_be32(raw + 8, phdr->p_vaddr);
        write_be32(raw + 12, phdr->p_paddr);
        write_be32(raw + 16, phdr->p_filesz);
        write_be32(raw + 20, phdr->p_memsz);
        write_be32(raw + 24, phdr->p_flags);
        write_be32(raw + 28, phdr->p_align);
    }
}

const char *elf_phtype_to_str(uint32_t type)
{
    switch (type) {
    case PT_NULL:           return "PT_NULL";
    case PT_LOAD:           return "PT_LOAD";
    case PT_DYNAMIC:        return "PT_DYNAMIC";
    case PT_INTERP:         return "PT_INTERP";
    case PT_NOTE:           return "PT_NOTE";
    case PT_SHLIB:          return "PT_SHLIB";
    case PT_PHDR:           return "PT_PHDR";
    case PT_TLS:            return "PT_TLS";
    case PT_GNU_EH_FRAME:   return "PT_GNU_EH_FRAME";
    case PT_GNU_STACK:      return "PT_GNU_STACK";
    case PT_GNU_RELRO:      return "PT_GNU_RELRO";
    case PT_MIPS_REGINFO:   return "PT_MIPS_REGINFO";
    case PT_MIPS_RTPROC:    return "PT_MIPS_RTPROC";
    case PT_MIPS_OPTIONS:   return "PT_MIPS_OPTIONS";
    case PT_MIPS_ABIFLAGS:  return "PT_MIPS_ABIFLAGS";
    default:                return "UNKNOWN";
    }
}

void elf_free(elf_t *elf)
{
    if (elf && elf->phdrs) free(elf->phdrs);
    if (elf && elf->phdr_body) {
        for (int i = 0; i < elf->header.e_phnum; i++)
            if (elf->phdr_body[i]) free(elf->phdr_body[i]);
        free(elf->phdr_body);
    }
    if (elf) free(elf);
}

elf_t* elf_load(const char *infn)
{
    elf_t *elf = NULL;
    uint8_t raw_header[ELF64_EHDR_SIZE];
    FILE *in = fopen(infn, "rb");
    if (!in) {
        fprintf(stderr, "error opening input file: %s\n", infn);
        goto error;
    }

    elf = calloc(1, sizeof(elf_t));

    // Read enough of the ELF header to determine its class.
    fread(raw_header, 1, EI_NIDENT, in);

    // Check ELF magic
    if (memcmp(raw_header, ELFMAG, SELFMAG)) {
        fprintf(stderr, "invalid ELF magic\n");
        goto error;
    }

    // Check ELF class
    if (raw_header[EI_CLASS] != ELFCLASS32 && raw_header[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "invalid ELF class\n");
        goto error;
    }
    elf->elf64 = raw_header[EI_CLASS] == ELFCLASS64;

    // Check ELF data encoding
    if (raw_header[EI_DATA] != ELFDATA2MSB) {
        fprintf(stderr, "invalid ELF data encoding\n");
        goto error;
    }

    size_t ehdr_size = elf->elf64 ? ELF64_EHDR_SIZE : ELF32_EHDR_SIZE;
    fread(raw_header + EI_NIDENT, 1, ehdr_size - EI_NIDENT, in);
    header_decode(elf, raw_header);

    size_t phdr_size = elf->elf64 ? ELF64_PHDR_SIZE : ELF32_PHDR_SIZE;
    if (elf->header.e_ehsize != ehdr_size) {
        fprintf(stderr, "invalid ELF header size\n");
        goto error;
    }
    if (elf->header.e_phnum && elf->header.e_phentsize != phdr_size) {
        fprintf(stderr, "invalid ELF program header size\n");
        goto error;
    }

    // Read program headers
    if (elf->header.e_phnum) {
        elf->phdrs = calloc(elf->header.e_phnum, sizeof(elf_phdr_t));
        elf->phdr_body = calloc(elf->header.e_phnum, sizeof(uint8_t*));

        assert(elf->header.e_phoff <= LONG_MAX);
        fseek(in, (long)elf->header.e_phoff, SEEK_SET);
        for (int i = 0; i < elf->header.e_phnum; i++) {
            uint8_t raw_phdr[ELF64_PHDR_SIZE];
            fread(raw_phdr, 1, phdr_size, in);
            phdr_decode(elf, &elf->phdrs[i], raw_phdr);
        }

        // Read program header body
        for (int i = 0; i < elf->header.e_phnum; i++) {
            assert(elf->phdrs[i].p_filesz <= SIZE_MAX);
            size_t body_size = elf->phdrs[i].p_filesz;
            if (!body_size) continue;

            elf->phdr_body[i] = malloc(body_size);
            assert(elf->phdrs[i].p_offset <= LONG_MAX);
            fseek(in, (long)elf->phdrs[i].p_offset, SEEK_SET);
            fread(elf->phdr_body[i], 1, body_size, in);
        }
    }

    fclose(in);
    return elf;

error:
    if (in) fclose(in);
    if (elf) elf_free(elf);
    return NULL;
}

bool elf_write(elf_t *elf, const char *outfn)
{
    uint8_t raw_header[ELF64_EHDR_SIZE];
    size_t ehdr_size = elf->elf64 ? ELF64_EHDR_SIZE : ELF32_EHDR_SIZE;
    size_t phdr_size = elf->elf64 ? ELF64_PHDR_SIZE : ELF32_PHDR_SIZE;
    FILE *out = fopen(outfn, "wb");
    if (!out) {
        fprintf(stderr, "error opening output file: %s\n", outfn);
        return false;
    }

    // Remove all section offsets from file (that were not read)
    elf->header.e_shnum = 0;
    elf->header.e_shoff = 0;
    elf->header.e_shstrndx = 0;

    // Update file offsets
    elf->header.e_ehsize = ehdr_size;
    elf->header.e_phentsize = phdr_size;
    uint64_t body_off = ehdr_size;
    if (elf->header.e_phnum) {
        elf->header.e_phoff = body_off;
        body_off += elf->header.e_phnum * phdr_size;
    } else {
        elf->header.e_phoff = 0;
    }
    for (int i = 0; i < elf->header.e_phnum; i++) {
        elf->phdrs[i].p_offset = body_off;
        assert(body_off <= UINT32_MAX);
        assert(elf->phdrs[i].p_filesz <= UINT32_MAX - body_off);
        body_off += elf->phdrs[i].p_filesz;
        body_off = (body_off + 7) & ~UINT64_C(7);
    }
    
    // Write ELF header
    header_encode(elf, raw_header);
    fwrite(raw_header, 1, ehdr_size, out);

    // Write program headers
    for (int i = 0; i < elf->header.e_phnum; i++) {
        uint8_t raw_phdr[ELF64_PHDR_SIZE];
        phdr_encode(elf, &elf->phdrs[i], raw_phdr);
        fwrite(raw_phdr, 1, phdr_size, out);
    }

    // Write program header body
    for (int i = 0; i < elf->header.e_phnum; i++) {
        size_t body_size = elf->phdrs[i].p_filesz;
        fwrite(elf->phdr_body[i], 1, body_size, out);
        uint64_t pos = elf->phdrs[i].p_offset + body_size;
        while (pos & 7) {
            fputc(0, out);
            pos++;
        }
    }

    fclose(out);
    return true;
}

bool process(char *infn, char *outfn, int compression)
{
    elf_t *elf = elf_load(infn);
    if (!elf) {
        fprintf(stderr, "error loading ELF file: %s\n", infn);
        return false;
    }

    // Remove all program headers which are not loadable
    int i = 0;
    while (i < elf->header.e_phnum) {
        if (elf->phdrs[i].p_type != PT_LOAD) {
            verbose("Removing program header %d (type: %s)\n", i, elf_phtype_to_str(elf->phdrs[i].p_type));
            free(elf->phdr_body[i]);
            for (int j = i; j < elf->header.e_phnum - 1; j++) {
                elf->phdrs[j] = elf->phdrs[j+1];
                elf->phdr_body[j] = elf->phdr_body[j+1];
            }
            elf->header.e_phnum--;
        } else {
            i++;
        }
    }

    // Compress program header loadable sections
    if (compression > 0) {
        for (int i = 0; i < elf->header.e_phnum; i++) {
            if (elf->phdrs[i].p_filesz == 0) continue;
            if (elf->phdrs[i].p_flags & PF_N64_COMPRESSED) {
                fprintf(stderr, "error: already compressed program header %d\n", i);
                elf_free(elf);
                return false;
            }
            assert(elf->phdrs[i].p_filesz <= INT_MAX);

            verbose("Compressing program header %d\n", i);

            int dec_size = elf->phdrs[i].p_filesz;
            uint8_t *outbuf; int cmp_size; int winsize = 0; int margin;
            asset_compress_mem_raw(compression,
                elf->phdr_body[i], dec_size,
                &outbuf, &cmp_size,
                &winsize, &margin);

            // Assembly decompressors can corrupt up to 8 bytes after the current
            // write pointer, so add 8 bytes of safety.
            margin += 8;

            verbose("  %" PRIu64 " => %d [margin=%d]\n", elf->phdrs[i].p_filesz, cmp_size, margin);
            
            // If the compressed size is larger than the original, don't compress
            if (cmp_size >= dec_size) {
                free(outbuf);
                continue;
            }

            // Update program header
            elf->phdrs[i].p_filesz = cmp_size;
            elf->phdrs[i].p_flags |= PF_N64_COMPRESSED;
            elf->phdrs[i].p_paddr = elf->phdrs[i].p_vaddr;

            // Make sure the compressed data is aligned to 8 bytes
            uint64_t cmp_offset = dec_size - cmp_size + margin;
            cmp_offset = (cmp_offset + 7) & ~UINT64_C(7);
            elf->phdrs[i].p_vaddr = elf->phdrs[i].p_paddr + cmp_offset;

            // Update the body pointer
            free(elf->phdr_body[i]);
            elf->phdr_body[i] = outbuf;
        }

        // Add a new program header for the decompressor
        struct decomp_s *dec = &decompressors[compression];
        assert(elf->header.e_phnum < UINT16_MAX);
        int new_phnum = elf->header.e_phnum + 1;
        elf->phdrs = realloc(elf->phdrs, new_phnum * sizeof(elf_phdr_t));
        elf->phdr_body = realloc(elf->phdr_body, new_phnum * sizeof(uint8_t*));
        elf->header.e_phnum = new_phnum;
        for (int i = elf->header.e_phnum - 1; i > 0; i--) {
            elf->phdrs[i] = elf->phdrs[i-1];
            elf->phdr_body[i] = elf->phdr_body[i-1];
        }
        memset(&elf->phdrs[0], 0, sizeof(elf_phdr_t));
        elf->phdrs[0].p_type   = PT_N64_DECOMP;
        elf->phdrs[0].p_filesz = dec->size;
        elf->phdrs[0].p_flags  = PF_R | PF_X;
        elf->phdrs[0].p_align  = 8;
        elf->phdr_body[0] = malloc(dec->size);
        memcpy(elf->phdr_body[0], dec->data, dec->size);
    }

    bool success = elf_write(elf, outfn);
    elf_free(elf);
    return success;
}

int main(int argc, char *argv[])
{
    winconsole_utf8();
    int compression = DEFAULT_COMPRESSION;
    char *outdir = ".";
    if(argc < 2) {
        //Print usage if too few arguments are passed
        print_args(argv[0]);
        return 1;
    }

    decompressors[1].data = g_mips_decomp_l1_data;
    decompressors[1].size = g_mips_decomp_l1_size;
    decompressors[2].data = g_mips_decomp_l2_data;
    decompressors[2].size = g_mips_decomp_l2_size;
    decompressors[3].data = g_mips_decomp_l3_data;
    decompressors[3].size = g_mips_decomp_l3_size;

    for(int i=1; i<argc; i++) {
        char *infn;
        char *outfn;
        if(argv[i][0] == '-') {
            //Option detected
            if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                //Print help
                print_args(argv[0]);
                return 0;
            } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
                //Mark output as verbose
                flag_verbose++;
            } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
                //Set output directory in next argument
                if(++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                outdir = argv[i];
            } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--compress")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                char extra;
                if (sscanf(argv[i], "%d%c", &compression, &extra) != 1) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
                if (compression < 0 || compression > MAX_COMPRESSION) {
                    fprintf(stderr, "invalid compression level: %d\n", compression);
                    return 1;
                }
            } else {
                //Complain about invalid flag
                fprintf(stderr, "invalid flag: %s\n", argv[i]);
                return 1;
            }
            continue;
        }

        infn = argv[i];
        char *basename = strrchr(infn, '/');
        if (!basename) basename = infn; else basename += 1;

        asprintf(&outfn, "%s/%s", outdir, basename);

        if (flag_verbose)
            printf("Compressing: %s => %s [algo=%d]\n", infn, outfn, compression);

        if (!process(infn, outfn, compression)) {
            return 1;
        }

        free(outfn);
    }
    return 0;
}
