#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../common/binout.c"
#include "../common/assetcomp.h"
#include "../common/mips_elf.h"

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
    Elf32_Ehdr header;
    Elf32_Phdr *phdrs;
    uint8_t **phdr_body;
} elf_t;

static uint32_t bswap32(uint32_t ptr)
{
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap32(ptr);
    #else
    return ptr 
    #endif
}

static uint16_t bswap16(uint16_t ptr)
{
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap16(ptr);
    #else
    return ptr;
    #endif
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

static void hdr_bswap(Elf32_Ehdr *hdr)
{
    hdr->e_type      = bswap16(hdr->e_type);
    hdr->e_machine   = bswap16(hdr->e_machine);
    hdr->e_version   = bswap32(hdr->e_version);
    hdr->e_entry     = bswap32(hdr->e_entry);
    hdr->e_phoff     = bswap32(hdr->e_phoff);
    hdr->e_shoff     = bswap32(hdr->e_shoff);
    hdr->e_flags     = bswap32(hdr->e_flags);
    hdr->e_ehsize    = bswap16(hdr->e_ehsize);
    hdr->e_phentsize = bswap16(hdr->e_phentsize);
    hdr->e_phnum     = bswap16(hdr->e_phnum);
    hdr->e_shentsize = bswap16(hdr->e_shentsize);
    hdr->e_shnum     = bswap16(hdr->e_shnum);
    hdr->e_shstrndx  = bswap16(hdr->e_shstrndx);
}

static void phdr_bswap(Elf32_Phdr *phdr)
{
    phdr->p_type   = bswap32(phdr->p_type);
    phdr->p_offset = bswap32(phdr->p_offset);
    phdr->p_vaddr  = bswap32(phdr->p_vaddr);
    phdr->p_paddr  = bswap32(phdr->p_paddr);
    phdr->p_filesz = bswap32(phdr->p_filesz);
    phdr->p_memsz  = bswap32(phdr->p_memsz);
    phdr->p_flags  = bswap32(phdr->p_flags);
    phdr->p_align  = bswap32(phdr->p_align);
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
    FILE *in = fopen(infn, "rb");
    if (!in) {
        fprintf(stderr, "error opening input file: %s\n", infn);
        goto error;
    }

    elf = calloc(1, sizeof(elf_t));

    // Read ELF header
    if (fread(&elf->header, sizeof(elf->header), 1, in) != 1) {
        fprintf(stderr, "error reading ELF header\n");
        goto error;
    }

    // Check ELF magic
    if (memcmp(elf->header.e_ident, ELFMAG, SELFMAG)) {
        fprintf(stderr, "invalid ELF magic\n");
        goto error;
    }

    // Check ELF class
    if (elf->header.e_ident[EI_CLASS] != ELFCLASS32) {
        fprintf(stderr, "invalid ELF class\n");
        goto error;
    }

    // Check ELF data encoding
    if (elf->header.e_ident[EI_DATA] != ELFDATA2MSB) {
        fprintf(stderr, "invalid ELF data encoding\n");
        goto error;
    }

    // Byteswap ELF header
    hdr_bswap(&elf->header);

    // Read program headers
    if (elf->header.e_phnum) {
        elf->phdrs = calloc(elf->header.e_phnum, sizeof(Elf32_Phdr));
        fseek(in, elf->header.e_phoff, SEEK_SET);
        if (fread(elf->phdrs, sizeof(Elf32_Phdr), elf->header.e_phnum, in) != elf->header.e_phnum) {
            fprintf(stderr, "error reading program headers\n");
            goto error;
        }
        for (int i = 0; i < elf->header.e_phnum; i++)
            phdr_bswap(&elf->phdrs[i]);
        // Read program header body
        elf->phdr_body = calloc(elf->header.e_phnum, sizeof(uint8_t*));
        for (int i = 0; i < elf->header.e_phnum; i++) {
            elf->phdr_body[i] = calloc(elf->phdrs[i].p_filesz, sizeof(uint8_t));
            fseek(in, elf->phdrs[i].p_offset, SEEK_SET);
            if (fread(elf->phdr_body[i], sizeof(uint8_t), elf->phdrs[i].p_filesz, in) != elf->phdrs[i].p_filesz) {
                fprintf(stderr, "error reading program header body\n");
                goto error;
            }
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
    int body_off = sizeof(elf->header);
    if (elf->header.e_phnum) {
        elf->header.e_phoff = body_off;
        body_off += elf->header.e_phnum * sizeof(Elf32_Phdr);
    }
    for (int i = 0; i < elf->header.e_phnum; i++) {
        elf->phdrs[i].p_offset = body_off;
        body_off += elf->phdrs[i].p_filesz;
        body_off = (body_off + 7) & ~7;
    }
    
    // Write ELF header
    hdr_bswap(&elf->header);
    fwrite(&elf->header, sizeof(elf->header), 1, out);
    hdr_bswap(&elf->header);

    // Write program headers
    if (elf->header.e_phnum) {
        for (int i = 0; i < elf->header.e_phnum; i++)
            phdr_bswap(&elf->phdrs[i]);
        fwrite(elf->phdrs, sizeof(Elf32_Phdr), elf->header.e_phnum, out);
        for (int i = 0; i < elf->header.e_phnum; i++)
            phdr_bswap(&elf->phdrs[i]);
    }

    // Write program header body
    for (int i = 0; i < elf->header.e_phnum; i++) {
        fwrite(elf->phdr_body[i], sizeof(uint8_t), elf->phdrs[i].p_filesz, out);
        // roundup position to 8
        int pos = ftell(out);
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
                return false;
            }

            verbose("Compressing program header %d\n", i);

            int dec_size = elf->phdrs[i].p_filesz;
            uint8_t *outbuf; int cmp_size; int winsize = 0; int margin;
            asset_compress_mem(compression,
                elf->phdr_body[i], dec_size,
                &outbuf, &cmp_size,
                &winsize, &margin);

            // Assembly decompressors can corrupt up to 8 bytes after the current
            // write pointer, so add 8 bytes of safety.
            margin += 8;

            verbose("  %d => %d [margin=%d]\n", elf->phdrs[i].p_filesz, cmp_size, margin);
            
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
            int cmp_offset = dec_size - cmp_size + margin;
            if (cmp_offset & 7) cmp_offset = (cmp_offset + 7) & ~7;
            elf->phdrs[i].p_vaddr = elf->phdrs[i].p_paddr + cmp_offset;

            // Update the body pointer
            free(elf->phdr_body[i]);
            elf->phdr_body[i] = outbuf;
        }

        // Add a new program header for the decompressor
        struct decomp_s *dec = &decompressors[compression];
        elf->header.e_phnum++;
        elf->phdrs = realloc(elf->phdrs, elf->header.e_phnum * sizeof(Elf32_Phdr));
        elf->phdr_body = realloc(elf->phdr_body, elf->header.e_phnum * sizeof(uint8_t*));
        for (int i = elf->header.e_phnum - 1; i > 0; i--) {
            elf->phdrs[i] = elf->phdrs[i-1];
            elf->phdr_body[i] = elf->phdr_body[i-1];
        }
        memset(&elf->phdrs[0], 0, sizeof(Elf32_Phdr));
        elf->phdrs[0].p_type   = PT_N64_DECOMP;
        elf->phdrs[0].p_filesz = dec->size;
        elf->phdrs[0].p_flags  = PF_R | PF_X;
        elf->phdrs[0].p_align  = 8;
        elf->phdr_body[0] = malloc(dec->size);
        memcpy(elf->phdr_body[0], dec->data, dec->size);
    }

    elf_write(elf, outfn);
    elf_free(elf);
    return true;
}

int main(int argc, char *argv[])
{
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
