#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../common/binout.c"
#include "../common/assetcomp.h"
#include "../common/mips_elf.h"

#define PF_COMPRESSED   0x80000000

int flag_verbose = 0;
bool flag_keep_sections = false;

typedef struct {
    Elf32_Ehdr header;
    Elf32_Phdr *phdrs;
    Elf32_Shdr *shdrs;
    uint8_t **phdr_body;
    uint8_t **shdr_body;
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
    fprintf(stderr, "   -k/--keep-sections          Keep all sections. (default: strip them away)\n");
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

static void shdr_bswap(Elf32_Shdr *shdr)
{
    shdr->sh_name      = bswap32(shdr->sh_name);
    shdr->sh_type      = bswap32(shdr->sh_type);
    shdr->sh_flags     = bswap32(shdr->sh_flags);
    shdr->sh_addr      = bswap32(shdr->sh_addr);
    shdr->sh_offset    = bswap32(shdr->sh_offset);
    shdr->sh_size      = bswap32(shdr->sh_size);
    shdr->sh_link      = bswap32(shdr->sh_link);
    shdr->sh_info      = bswap32(shdr->sh_info);
    shdr->sh_addralign = bswap32(shdr->sh_addralign);
    shdr->sh_entsize   = bswap32(shdr->sh_entsize);
}

void elf_free(elf_t *elf)
{
    if (elf && elf->phdrs) free(elf->phdrs);
    if (elf && elf->shdrs) free(elf->shdrs);
    if (elf && elf->phdr_body) {
        for (int i = 0; i < elf->header.e_phnum; i++)
            if (elf->phdr_body[i]) free(elf->phdr_body[i]);
        free(elf->phdr_body);
    }
    if (elf && elf->shdr_body) {
        for (int i = 0; i < elf->header.e_shnum; i++)
            if (elf->shdr_body[i]) free(elf->shdr_body[i]);
        free(elf->shdr_body);
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

    // Read section headers
    if (elf->header.e_shnum) {
        elf->shdrs = calloc(elf->header.e_shnum, sizeof(Elf32_Shdr));
        fseek(in, elf->header.e_shoff, SEEK_SET);
        if (fread(elf->shdrs, sizeof(Elf32_Shdr), elf->header.e_shnum, in) != elf->header.e_shnum) {
            fprintf(stderr, "error reading section headers\n");
            goto error;
        }
        for (int i = 0; i < elf->header.e_shnum; i++)
            shdr_bswap(&elf->shdrs[i]);
        // Read section header body
        elf->shdr_body = calloc(elf->header.e_shnum, sizeof(uint8_t*));
        for (int i = 0; i < elf->header.e_shnum; i++) {
            elf->shdr_body[i] = calloc(elf->shdrs[i].sh_size, sizeof(uint8_t));
            fseek(in, elf->shdrs[i].sh_offset, SEEK_SET);
            if (fread(elf->shdr_body[i], sizeof(uint8_t), elf->shdrs[i].sh_size, in) != elf->shdrs[i].sh_size) {
                fprintf(stderr, "error reading section header body\n");
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

    // Update file offsets
    int body_off = sizeof(elf->header);
    if (elf->header.e_phnum) {
        elf->header.e_phoff = body_off;
        body_off += elf->header.e_phnum * sizeof(Elf32_Phdr);
    }
    if (elf->header.e_shnum) {
        elf->header.e_shoff = body_off;
        body_off += elf->header.e_shnum * sizeof(Elf32_Shdr);
    }
    for (int i = 0; i < elf->header.e_phnum; i++) {
        elf->phdrs[i].p_offset = body_off;
        body_off += elf->phdrs[i].p_filesz;
        body_off = (body_off + 7) & ~7;
    }
    for (int i = 0; i < elf->header.e_shnum; i++) {
        elf->shdrs[i].sh_offset = body_off;
        body_off += elf->shdrs[i].sh_size;
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
    // Write section headers
    if (elf->header.e_shnum) {
        for (int i = 0; i < elf->header.e_shnum; i++)
            shdr_bswap(&elf->shdrs[i]);
        fwrite(elf->shdrs, sizeof(Elf32_Shdr), elf->header.e_shnum, out);
        for (int i = 0; i < elf->header.e_shnum; i++)
            shdr_bswap(&elf->shdrs[i]);
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

    // Write section header body
    for (int i = 0; i < elf->header.e_shnum; i++) {
        fwrite(elf->shdr_body[i], sizeof(uint8_t), elf->shdrs[i].sh_size, out);
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

bool elf_compress(char *infn, char *outfn, int compression)
{
    elf_t *elf = elf_load(infn);
    if (!elf) {
        fprintf(stderr, "error loading ELF file: %s\n", infn);
        return false;
    }

    // Compress program header loadable sections
    for (int i = 0; i < elf->header.e_phnum; i++) {
        if (elf->phdrs[i].p_type != PT_LOAD) continue;
        verbose("Compressing program header %d\n", i);

        uint8_t *outbuf; int outsize; int winsize = 0; int margin;
        asset_compress_mem(compression,
            elf->phdr_body[i], elf->phdrs[i].p_filesz,
            &outbuf, &outsize,
            &winsize, &margin);

        verbose("  %d => %d [margin=%d]\n", elf->phdrs[i].p_filesz, outsize, margin);
        
        // If the compressed size is larger than the original, don't compress
        if (outsize >= elf->phdrs[i].p_filesz) {
            free(outbuf);
            continue;
        }

        // Update program header
        elf->phdrs[i].p_memsz  = elf->phdrs[i].p_filesz;
        elf->phdrs[i].p_filesz = outsize;
        elf->phdrs[i].p_flags |= PF_COMPRESSED;
        elf->phdrs[i].p_paddr = elf->phdrs[i].p_vaddr;
        elf->phdrs[i].p_vaddr = elf->phdrs[i].p_paddr + elf->phdrs[i].p_memsz - elf->phdrs[i].p_filesz + margin;

        // Update the body pointer
        free(elf->phdr_body[i]);
        elf->phdr_body[i] = outbuf;
    }

    if (!flag_keep_sections) {
        // Remove all sections
        for (int i = 0; i < elf->header.e_shnum; i++) {
            free(elf->shdr_body[i]);
        }
        free(elf->shdr_body);
        elf->shdr_body = NULL;
        free(elf->shdrs);
        elf->shdrs = NULL;
        elf->header.e_shoff = 0;
        elf->header.e_shnum = 0;
        elf->header.e_shstrndx = 0;
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
            } else if (!strcmp(argv[i], "-k") || !strcmp(argv[i], "--keep-sections")) {
                //Keep all sections
                flag_keep_sections = true;    
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

        if (!elf_compress(infn, outfn, compression)) {
            return 1;
        }

        free(outfn);
    }
    return 0;
}
