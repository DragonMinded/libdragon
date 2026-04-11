/*
    n64sym: generate a symbol table for an N64 ROM
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <strings.h>

#include <math.h>
#include <stdlib.h>
#include "../common/subprocess.h"

#include "../common/polyfill.h"
#include "../common/utils.h"
#include "../common/binout.h"
#include "../common/assetcomp.h"
#include "n64sym_huffman.h"

#include "../common/binout.c"

// Size of the runtime buffer that will be used during SYMT access/decompression.
// This is used to tune internal compressions to make sure the provided value
// is sufficient. Testing shows that growing after 512 has very minimal size
// savings.
#define MAX_BUFFER_SIZE  512
#define SYMT_VERSION     5

int flag_verbose = 0;
int flag_max_sym_len = 96;
bool flag_inlines = true;
bool flag_cpp_shorten = true;
const char *gccprefix_triplet = NULL;

// C++ symbol shortening function (n64sym_cppshorten.c)
char *cpp_shorten_symbol(const char *sym, int max_len);

// Printf if verbose
void verbose(int level, const char *fmt, ...) {
    if (flag_verbose >= level) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

void usage(const char *progname)
{
    fprintf(stderr, "%s - Generate symbol tables for N64 ROMs\n", progname);
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: %s [flags] <program.elf> [<program.sym>]\n", progname);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -v/--verbose          Verbose output\n");
    fprintf(stderr, "   -m/--max-len <N>      Maximum symbol length (default: 96)\n");
    fprintf(stderr, "   --cpp-shorten <0|1>   C++ demangled symbol shortening (default: true)\n");
    fprintf(stderr, "   --no-inlines          Do not export inlined symbols\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "This program requires a libdragon toolchain installed in $N64_INST.\n");
}

// Write a variable-length integer to the buffer
void w_varint(uint8_t **buf, uint32_t val)
{
    do {
        uint8_t byte = val & 0x7F;
        val >>= 7;
        if (val) byte |= 0x80;
        stbds_arrput(*buf, byte);
    } while (val);
}

// Write a signed variable-length integer to the buffer (zigzag encoding)
void w_signed_varint(uint8_t **buf, int32_t val)
{
    uint32_t uval = (val << 1) ^ (val >> 31);
    w_varint(buf, uval);
}

// Global map for strings
// Used to compress strings in the symbol table
struct gl_map { char *key; int value; };

// Map for address whitelist (uint32_t -> int)
struct addr_map { uint32_t key; int value; };

// Symbol table entry
struct symtable_s {
    uint32_t uuid;
    uint32_t addr;
    char *func;
    char *file;
    int line;

    bool is_func, is_inline;
} *symtable = NULL;

// Truncate a path ensuring it fits in max_len and possibly preserves the
// directory structure suffix (by cutting at a separator). This is useful
// as strings are compressed using a prefix-based algorithm.
char *truncate_path(const char *path, int len, int max_len) {
    if (len <= max_len)
        return strndup(path, len);

    int suffix_limit = max_len - 3;
    int offset = len - suffix_limit;
    
    // Find the first separator after offset (or at offset)
    const char *sep = NULL;
    for (int k = offset; k < len; k++) {
        if (path[k] == '/' || path[k] == '\\') {
            sep = path + k;
            break;
        }
    }
    
    int suffix_len;
    const char *suffix_src;
    
    if (sep) {
        // Found a separator, use it as start of suffix
        suffix_len = (path + len) - sep;
        suffix_src = sep;
    } else {
        // No separator found in the valid range, truncate arbitrarily
        suffix_len = suffix_limit;
        suffix_src = path + offset;
    }
    
    char *file = malloc(3 + suffix_len + 1);
    strcpy(file, "...");
    memcpy(file + 3, suffix_src, suffix_len);
    file[3 + suffix_len] = 0;
    return file;
}

void symbol_add(const char *elf, uint32_t addr, bool is_func)
{
    // We keep one addr2line process open for the last ELF file we processed.
    // This allows to convert multiple symbols very fast, avoiding spawning a
    // new process for each symbol.
    // NOTE: we cannot use popen() here because on some platforms (eg. glibc)
    // it only allows a single direction pipe, and we need both directions.
    // So we rely on the subprocess library for this.
    static char *addrbin = NULL;
    static struct subprocess_s subp;
    static FILE *addr2line_w = NULL, *addr2line_r = NULL;
    static const char *cur_elf = NULL;
    static char *line_buf = NULL;
    static size_t line_buf_size = 0;

    // Check if this is a new ELF file (or it's the first time we run this function)
    if (!cur_elf || strcmp(cur_elf, elf)) {
        if (cur_elf) {
            subprocess_terminate(&subp);
            cur_elf = NULL; addr2line_r = addr2line_w = NULL;
        }
        if (!addrbin)
            asprintf(&addrbin, "%saddr2line", gccprefix_triplet);

        const char *cmd_addr[16] = {0}; int i = 0;
        cmd_addr[i++] = addrbin;
        cmd_addr[i++] = "--addresses";
        cmd_addr[i++] = "--functions";
        cmd_addr[i++] = "--demangle";
        if (flag_inlines) cmd_addr[i++] = "--inlines";
        cmd_addr[i++] = "--exe";
        cmd_addr[i++] = elf;

        if (subprocess_create(cmd_addr, subprocess_option_no_window, &subp) != 0) {
            fprintf(stderr, "Error: cannot run: %s\n", addrbin);
            exit(1);
        }
        addr2line_w = subprocess_stdin(&subp);
        addr2line_r = subprocess_stdout(&subp);
        cur_elf = elf;
    }

    // Send the address to addr2line and fetch back the symbol and the function name
    // Since we activated the "--inlines" option, addr2line produces an unknown number
    // of output lines. This is a problem with pipes, as we don't know when to stop.
    // Thus, we always add a dummy second address (0xffffffff) so that we stop when we see the
    // reply for it. NOTE: we can't use 0x0 as dummy address as DSOs are partially
    // linked so there are really symbols at 0.
    fprintf(addr2line_w, "%08x\n0xffffffff\n", addr);
    fflush(addr2line_w);

    // First line is the address. It's just an echo, so ignore it.
    int n = getline(&line_buf, &line_buf_size, addr2line_r);
    assert(n >= 2 && strncmp(line_buf, "0x", 2) == 0);

    // Add one symbol for each inlined function
    bool at_least_one = false;
    while (1) {
        // First line is the function name. If instead it's the dummy 0x0 address,
        // it means that we're done.
        int n = getline(&line_buf, &line_buf_size, addr2line_r);
        if (strncmp(line_buf, "0xffffffff", 10) == 0) break;
        n--;
        if (line_buf[n-1] == '\r') n--; // Remove trailing \r (Windows)

        // Normalize/truncate function names to bounded length.
        int max_len = MIN(flag_max_sym_len, MAX_BUFFER_SIZE - 8);
        char *func_raw = strndup(line_buf, n);
        char *func = NULL;
        if (flag_cpp_shorten) {
            func = cpp_shorten_symbol(func_raw, max_len);
            verbose(2, "C++ shortening:\n");
            verbose(2, "  in  = %s\n", func_raw);
            verbose(2, "  out = %s\n", func);
        } else if (n <= max_len) {
            func = strdup(func_raw);
        } else {
            func = strndup(func_raw, max_len);
            if (max_len > 3) strcpy(&func[max_len-3], "...");
        }
        free(func_raw);

        // Second line is the file name and line number
        int ret = getline(&line_buf, &line_buf_size, addr2line_r);
        assert(ret != -1);
        char *colon = strrchr(line_buf, ':');
        
        // Filename must fit into max_len too (so both the maximum length requested by the user,
        // and the buffer size). If we need to truncate the filename, try to find a directory
        // separator so that we keep a valid path suffix.
        int file_len = colon - line_buf;
        char *file = truncate_path(line_buf, file_len, max_len);
        int line = atoi(colon + 1);

        // Add the callsite to the list
        stbds_arrput(symtable, ((struct symtable_s) {
            .uuid = stbds_arrlen(symtable),
            .addr = addr,
            .func = func,
            .file = file,
            .line = line,
            .is_func = false,
            .is_inline = true,
        }));
        at_least_one = true;
    }
    assert(at_least_one);
    symtable[stbds_arrlen(symtable)-1].is_inline = false;
    symtable[stbds_arrlen(symtable)-1].is_func = is_func;

    // Read and skip the two remaining lines (function and file position)
    // that refers to the dummy 0x0 address
    getline(&line_buf, &line_buf_size, addr2line_r);
    getline(&line_buf, &line_buf_size, addr2line_r);
}

void elf_read_function_symbols(const char *elf, struct addr_map **all_functions)
{
    char *cmd = NULL;
    asprintf(&cmd, "%sobjdump -t %s", gccprefix_triplet, elf);
    verbose(1, "Running: %s\n", cmd);
    FILE *f = popen(cmd, "r");
    if (!f) {
        fprintf(stderr, "Error: cannot run: %s\n", cmd);
        exit(1);
    }

    char *line = NULL; size_t line_size = 0;
    while (getline(&line, &line_size, f) != -1) {
        // Work with opt-out logic rather than opt-in because it is common eg.
        // for assembly functions not to be marked as such in debug symbols.
        // What we absolutely need to filter out is data symbols (objects as they
        // could not even be 4-byte aligned.
        if (!strstr(line, " O ")) {
            uint32_t addr = strtoul(line, NULL, 16);
            stbds_hmput(*all_functions, addr, 1);
        }
    }
    free(line);
    free(cmd);
    pclose(f);
}

bool elf_find_callsites(const char *elf)
{
    struct addr_map *all_functions = NULL;
    elf_read_function_symbols(elf, &all_functions);
    verbose(1, "Found %d function symbols\n", stbds_hmlen(all_functions));

    // Start objdump to parse the disassembly of the ELF file
    char *cmd = NULL;
    asprintf(&cmd, "%sobjdump -d %s", gccprefix_triplet, elf);
    verbose(1, "Running: %s\n", cmd);
    FILE *disasm = popen(cmd, "r");
    if (!disasm) {
        fprintf(stderr, "Error: cannot run: %s\n", cmd);
        exit(1);
    }

    // Parse the disassembly
    char *line = NULL; size_t line_size = 0;
    while (getline(&line, &line_size, disasm) != -1) {
        // Find the functions: use the all_functions whitelist to skip symbols
        // which are not functions in the text segment.
        if (strstr(line, ">:")) {
            uint32_t addr = strtoul(line, NULL, 16);
            if (stbds_hmgeti(all_functions, addr) >= 0) {
                symbol_add(elf, addr, true);
            }
        }
        // Find the callsites
        if (strstr(line, "\tjal\t") || strstr(line, "\tjalr\t") || strstr(line, "\tsyscall")) {
            uint32_t addr = strtoul(line, NULL, 16);
            symbol_add(elf, addr, false);
        }
    }
    free(line);
    free(cmd);
    stbds_hmfree(all_functions);
    int status = pclose(disasm);
#ifdef __MINGW32__
    return status == 0;
#else
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

int symtable_sort_by_addr(const void *a, const void *b)
{
    const struct symtable_s *sa = a;
    const struct symtable_s *sb = b;
    // In case the address match, it means that there are multiple
    // inlines at this address. Sort by insertion order (aka stable sort)
    // so that we preserve the inline order.
    if (sa->addr != sb->addr)
        return sa->addr - sb->addr;
    return sa->uuid - sb->uuid;
}

int cmp_string_ptr(const void *a, const void *b) {
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    return strcmp(sa, sb);
}

void compress_strings(char **strings, huff_code_t *huff_table, uint8_t **blob, uint32_t **index) 
{
    // Statistics
    int stats_raw_suffix_len = 0;
    int stats_huff_bits = 0;
    int stats_varint_bytes = 0;
    int stats_padding_bytes = 0;

    int n = stbds_arrlen(strings);
    int i = 0;
    while (i < n) {
        // Start a new block
        uint32_t block_start_idx = i;
        uint32_t block_blob_off = stbds_arrlen(*blob);
        stbds_arrput(*index, block_start_idx);
        stbds_arrput(*index, block_blob_off);
        
        // Use BitWriter for the block
        BitWriter bw;
        bw_init(&bw);
        
        int count = 0;
        const char *prev = NULL;
        int last_common = 0;

        // Try adding strings until full.
        while (i < n && count < 64) {
            const char *cur = strings[i];
            int common = 0;
            if (prev) {
                 int min_len = MIN(strlen(prev), strlen(cur));
                 while (common < min_len && prev[common] == cur[common]) common++;
            }
            
            const char *suffix = cur + common;
            int suffix_len = strlen(suffix);
            
            // Calculate how many bits this string would need
            int prefix_delta = common - last_common;
            uint32_t z_delta = zigzag_encode(prefix_delta);
            int bits_needed = exp_golomb_len(z_delta);
            
            for (int k=0; k<=suffix_len; k++) {
                unsigned char c = (unsigned char)suffix[k];
                bits_needed += huff_table[c].len;
            }
            
            // Calculate current buffer size in bits and what it would be after adding this string
            int current_bits = stbds_arrlen(bw.buf) * 8 + bw.cur_bit;
            int new_bits = current_bits + bits_needed;
            int new_size_bytes = (new_bits + 7) / 8;
            
            // Check if it would fit
            if (new_size_bytes > MAX_BUFFER_SIZE) {
                assert(count != 0 && "String too long for block buffer");
                break;
            }
            
            // It fits! Write it to the bitstream
            bw_write_exp_golomb(&bw, z_delta);
            for (int k=0; k<=suffix_len; k++) {
                unsigned char c = (unsigned char)suffix[k];
                bw_write(&bw, huff_table[c].code, huff_table[c].len);
            }
            
            // Update stats
            int z_delta_bits = exp_golomb_len(z_delta);
            int huff_bits = bits_needed - z_delta_bits;
            stats_raw_suffix_len += suffix_len;
            stats_varint_bytes += (z_delta_bits + 7) / 8;
            stats_huff_bits += huff_bits;
            
            last_common = common;
            prev = cur;
            i++;
            count++;
        }
        
        // Final flush and padding
        bw_flush(&bw);
        if (stbds_arrlen(bw.buf) % 2 != 0) {
            stbds_arrput(bw.buf, 0);
            stats_padding_bytes++;
        }
            
        // Append to blob
        int cur_len = stbds_arrlen(*blob);
        int block_len = stbds_arrlen(bw.buf);
        stbds_arrsetlen(*blob, cur_len + block_len);
        memcpy(*blob + cur_len, bw.buf, block_len);
        stbds_arrfree(bw.buf);
    }

    int stats_huff_bytes = (stats_huff_bits + 7) / 8;
    int stats_front_coding_size = stats_raw_suffix_len + stats_varint_bytes;

    verbose(1, "  Huffman Stats:\n");
    verbose(1, "    Raw suffixes: %d bytes\n", stats_raw_suffix_len);
    verbose(1, "    Equivalent Front Coding Size: %d bytes\n", stats_front_coding_size);
    verbose(1, "    Compressed suffixes (Huffman): %d bytes (%.1f%%)\n", 
        stats_huff_bytes, 100.0 * stats_huff_bytes / (stats_raw_suffix_len ? stats_raw_suffix_len : 1));
    verbose(1, "    Overhead: VarInts: %d bytes, Padding: %d bytes\n", 
        stats_varint_bytes, stats_padding_bytes);
    verbose(1, "    Total Blob Size: %d bytes (%.1f%% of Front Coding)\n", 
        stbds_arrlen(*blob), 100.0 * stbds_arrlen(*blob) / (stats_front_coding_size ? stats_front_coding_size : 1));
}

// Compress the symbol table into a delta-encoded stream split into chunks.
// The symbols must be sorted by address. The stream is divided into independent chunks
// that fit into MAX_BUFFER_SIZE. A separate index tracks the start address and
// stream offset of each chunk for random access.
int compress_symbols(
    struct symtable_s *symtable, int nsyms,
    struct gl_map *file_map, struct gl_map *func_map,
    uint8_t **stream, uint32_t **chunk_index)
{
    if (nsyms == 0) return 0;

    // Temp buffer for current chunk
    uint8_t *chunk_buf = NULL;
    uint32_t chunk_start_addr = symtable[0].addr;
    uint32_t last_func_addr = symtable[0].is_func ? symtable[0].addr : 0;
    
    // State machine
    uint32_t state_addr = chunk_start_addr;
    int state_file[2] = {0, 0}; // [0]=non-inline, [1]=inline
    int state_func[2] = {0, 0}; // [0]=non-inline, [1]=inline
    int state_line[2] = {0, 0}; // [0]=non-inline, [1]=inline

    // Write per-chunk header: func offset of first symbol (VarInt)
    uint32_t chunk_func_off = last_func_addr ? (chunk_start_addr - last_func_addr) : 0;
    w_varint(&chunk_buf, chunk_func_off);
    
    int emitted = 0;

    for (int i=0; i<nsyms; i++) {
        struct symtable_s *sym = &symtable[i];

        int file_idx = stbds_shget(file_map, sym->file);
        int func_idx = stbds_shget(func_map, sym->func);
        
        // Check if adding this symbol might overflow chunk size (estimate max 21 bytes)
        // If chunk is getting full, flush it.
        // This is used to tune internal compressions to make sure the provided value
        // is sufficient. Testing shows that growing after 512 has very minimal size
        // savings.
        bool flush = (stbds_arrlen(chunk_buf) > MAX_BUFFER_SIZE - 32);
        
        if (flush) {
            // Flush chunk
            stbds_arrput(*chunk_index, chunk_start_addr);
            stbds_arrput(*chunk_index, stbds_arrlen(*stream));
            
            // Append chunk to stream
            int cur_len = stbds_arrlen(*stream);
            int chunk_len = stbds_arrlen(chunk_buf);
            stbds_arrsetlen(*stream, cur_len + chunk_len);
            memcpy(*stream + cur_len, chunk_buf, chunk_len);
            
            // Reset for new chunk
            stbds_arrsetlen(chunk_buf, 0);
            chunk_start_addr = sym->addr;
            state_addr = sym->addr;
            state_file[0] = state_file[1] = 0;
            state_func[0] = state_func[1] = 0;
            state_line[0] = state_line[1] = 0;
            chunk_func_off = last_func_addr ? (chunk_start_addr - last_func_addr) : 0;
            w_varint(&chunk_buf, chunk_func_off);
        }
        
        int sid = sym->is_inline ? 1 : 0;

        // Calculate deltas
        int delta_file = file_idx - state_file[sid];
        int delta_func = func_idx - state_func[sid];
        int delta_line = sym->line - state_line[sid];
        uint32_t delta_addr = sym->addr - state_addr;
        
        // Encode to temp buffer
        uint8_t op = 0;
        if (delta_file != 0) op |= 0x80;
        if (delta_func != 0) op |= 0x40;
        if (delta_line != 0) op |= 0x20;
        if (sym->is_func)    op |= 0x10;
        if (sym->is_inline)  op |= 0x08;
        
        uint32_t addr_param = 0;
        bool has_addr_param = false;
        
        assert(delta_addr >= 0 && "Delta address is negative");
        assert(delta_addr % 4 == 0 && "Delta address is not divisible by 4");
        if (delta_addr / 4 <= 6) {
            op |= (delta_addr / 4);
        } else {
            op |= 7;
            addr_param = delta_addr / 4 - 7;
            has_addr_param = true;
        }
        
        stbds_arrput(chunk_buf, op);
        if (delta_file != 0) w_signed_varint(&chunk_buf, delta_file);
        if (delta_func != 0) w_signed_varint(&chunk_buf, delta_func);
        if (delta_line != 0) w_signed_varint(&chunk_buf, delta_line);
        if (has_addr_param)  w_varint(&chunk_buf, addr_param);

        emitted++;
        
        state_addr = sym->addr;
        state_file[sid] = file_idx;
        state_func[sid] = func_idx;
        state_line[sid] = sym->line;

        if (sym->is_func)
            last_func_addr = sym->addr;
    }
    
    // Flush final chunk
    if (stbds_arrlen(chunk_buf) > 0) {
        stbds_arrput(*chunk_index, chunk_start_addr);
        stbds_arrput(*chunk_index, stbds_arrlen(*stream));
        int cur_len = stbds_arrlen(*stream);
        int chunk_len = stbds_arrlen(chunk_buf);
        stbds_arrsetlen(*stream, cur_len + chunk_len);
        memcpy(*stream + cur_len, chunk_buf, chunk_len);
    }

    stbds_arrfree(chunk_buf);
    return emitted;
}

static void compress_symbol_chunks(
    uint8_t *plain_stream, uint32_t *plain_chunk_index,
    uint8_t **cmp_stream, uint32_t **cmp_chunk_index,
    uint32_t *max_chunk_margin)
{
    int num_chunks = stbds_arrlen(plain_chunk_index) / 2;
    *max_chunk_margin = 0;

    for (int i = 0; i < num_chunks; i++) {
        uint32_t start_addr = plain_chunk_index[i * 2 + 0];
        uint32_t plain_off = plain_chunk_index[i * 2 + 1];
        uint32_t plain_end = (i + 1 < num_chunks) ? plain_chunk_index[(i + 1) * 2 + 1] : stbds_arrlen(plain_stream);
        assert(plain_end >= plain_off);
        uint32_t plain_size = plain_end - plain_off;

        int cmp_size = 0;
        int winsize = 0;
        int margin = 0;
        uint8_t *cmp_buf = NULL;
        asset_compress_mem_raw(3, plain_stream + plain_off, (int)plain_size, &cmp_buf, &cmp_size, &winsize, &margin);
        if (!cmp_buf || cmp_size <= 0) {
            fprintf(stderr, "Error: compression failed for chunk %d\n", i);
            exit(1);
        }

        // Keep compressed chunk sizes even so the next chunk offset preserves
        // ROM/RAM parity constraints required by dma_read().
        if (cmp_size & 1) {
            cmp_buf = realloc(cmp_buf, cmp_size + 1);
            cmp_buf[cmp_size++] = 0;
        }

        if ((uint32_t)margin > *max_chunk_margin)
            *max_chunk_margin = margin;

        stbds_arrput(*cmp_chunk_index, start_addr);
        stbds_arrput(*cmp_chunk_index, stbds_arrlen(*cmp_stream));

        int cur_len = stbds_arrlen(*cmp_stream);
        stbds_arrsetlen(*cmp_stream, cur_len + cmp_size);
        memcpy(*cmp_stream + cur_len, cmp_buf, cmp_size);

        free(cmp_buf);
    }
}

void write_sym_file(const char *outfn, 
    int num_symbols, int num_chunks, int num_files, int num_funcs,
    uint32_t max_chunk_margin,
    uint32_t *chunk_index,
    uint32_t *file_offsets, uint8_t *file_blob,
    uint32_t *func_offsets, uint8_t *func_blob,
    uint8_t *huff_blob,
    uint8_t *stream)
{
    FILE *out = fopen(outfn, "wb");
    if (!out) {
        fprintf(stderr, "Cannot create file: symtable.bin\n");
        exit(1);
    }
    
    // Write SYMT header
    fwrite("SYMT", 4, 1, out);
    w32(out, SYMT_VERSION);
    w32(out, num_symbols);
    w32(out, num_chunks);
    
    // Placeholders for offsets
    int chunk_idx_off_ph = w32_placeholder(out);
    int file_tab_off_ph = w32_placeholder(out);
    int func_tab_off_ph = w32_placeholder(out);
    int huff_tab_off_ph = w32_placeholder(out);
    int file_blob_off_ph = w32_placeholder(out);
    int func_blob_off_ph = w32_placeholder(out);
    int stream_off_ph = w32_placeholder(out);
    w32(out, max_chunk_margin);
    
    // Sizes
    w32(out, num_files);
    w32(out, num_funcs);
    w32(out, stbds_arrlen(file_offsets) / 2);
    w32(out, stbds_arrlen(func_offsets) / 2);
    w32(out, stbds_arrlen(huff_blob));
    w32(out, stbds_arrlen(file_blob));
    w32(out, stbds_arrlen(func_blob));
    w32(out, stbds_arrlen(stream));
    
    // Write Sections
    walign(out, 8);
    w32_at(out, chunk_idx_off_ph, ftell(out));
    for(int i=0; i<stbds_arrlen(chunk_index); i++) w32(out, chunk_index[i]);
    
    walign(out, 8);
    w32_at(out, file_tab_off_ph, ftell(out));
    for(int i=0; i<stbds_arrlen(file_offsets); i++) w32(out, file_offsets[i]);
    
    walign(out, 8);
    w32_at(out, func_tab_off_ph, ftell(out));
    for(int i=0; i<stbds_arrlen(func_offsets); i++) w32(out, func_offsets[i]);

    walign(out, 8);
    w32_at(out, huff_tab_off_ph, ftell(out));
    fwrite(huff_blob, stbds_arrlen(huff_blob), 1, out);
    
    walign(out, 8);
    w32_at(out, file_blob_off_ph, ftell(out));
    fwrite(file_blob, stbds_arrlen(file_blob), 1, out);
    
    walign(out, 8);
    w32_at(out, func_blob_off_ph, ftell(out));
    fwrite(func_blob, stbds_arrlen(func_blob), 1, out);
    
    walign(out, 8);
    w32_at(out, stream_off_ph, ftell(out));
    fwrite(stream, stbds_arrlen(stream), 1, out);
    
    int size = ftell(out);
    verbose(1, "  Total File Size: %d bytes\n", size);

    fclose(out);
}

void process(const char *infn, const char *outfn)
{
    verbose(1, "Processing: %s -> %s\n", infn, outfn);

    // First, find all functions and call sites. We do this by disassembling
    // the ELF file and grepping it.
    if (!elf_find_callsites(infn)) {
        fprintf(stderr, "Error: objdump failed\n");
        exit(1);
    }
    verbose(1, "Found %d callsites\n", stbds_arrlen(symtable));

    // If the symtable is empty, there's nothing else to do
    if (stbds_arrlen(symtable) == 0) {
        verbose(1, "No symbols found\n");
        return;
    }

    // Sort the symbol table by address
    qsort(symtable, stbds_arrlen(symtable), sizeof(struct symtable_s), symtable_sort_by_addr);

    // Prepare in-memory buffers
    uint8_t *file_blob = NULL;
    uint8_t *func_blob = NULL;
    uint32_t *file_offsets = NULL;
    uint32_t *func_offsets = NULL;
    
    // Create sorted list of unique files/funcs
    char **unique_files = NULL;
    char **unique_funcs = NULL;
    
    for (int i=0; i<stbds_arrlen(symtable); i++) {
        struct symtable_s *s = &symtable[i];
        if (s->file) stbds_arrput(unique_files, s->file);
        if (s->func) stbds_arrput(unique_funcs, s->func);
    }
    
    // Sort and unique
    qsort(unique_files, stbds_arrlen(unique_files), sizeof(char*), cmp_string_ptr);
    qsort(unique_funcs, stbds_arrlen(unique_funcs), sizeof(char*), cmp_string_ptr);
    
    // Remove duplicates in place
    int nfiles = 0;
    if (stbds_arrlen(unique_files) > 0) {
        int w = 1;
        for (int r = 1; r < stbds_arrlen(unique_files); r++) {
            if (strcmp(unique_files[r], unique_files[r-1]) != 0)
                unique_files[w++] = unique_files[r];
        }
        stbds_arrsetlen(unique_files, w);
        nfiles = w;
    }
    
    int nfuncs = 0;
    if (stbds_arrlen(unique_funcs) > 0) {
        int w = 1;
        for (int r = 1; r < stbds_arrlen(unique_funcs); r++) {
            if (strcmp(unique_funcs[r], unique_funcs[r-1]) != 0)
                unique_funcs[w++] = unique_funcs[r];
        }
        stbds_arrsetlen(unique_funcs, w);
        nfuncs = w;
    }
    
    // Create maps from string -> index
    struct gl_map *file_map = NULL;
    struct gl_map *func_map = NULL;
    
    for (int i=0; i<nfiles; i++) stbds_shput(file_map, unique_files[i], i);
    for (int i=0; i<nfuncs; i++) stbds_shput(func_map, unique_funcs[i], i);
    
    // Shared Huffman Table
    verbose(1, "Calculating shared Huffman table...\n");
    int shared_freqs[256] = {0};
    collect_string_freqs(unique_files, shared_freqs);
    collect_string_freqs(unique_funcs, shared_freqs);
    
    huff_code_t shared_huff_table[256] = {0};
    build_limited_huffman_tree(shared_freqs, HUFF_MAX_CODE_LEN, shared_huff_table);
    
    CanonicalTables shared_ct;
    generate_canonical_tables(shared_huff_table, &shared_ct);
    
    // Write shared table to huff_blob
    uint8_t *huff_blob = NULL;
    write_huff_header(&shared_ct, &huff_blob);
    
    // Compress string blobs using shared table (no headers)
    verbose(1, "Compressing strings (Shared Table)...\n");
    compress_strings(unique_files, shared_huff_table, &file_blob, &file_offsets);
    compress_strings(unique_funcs, shared_huff_table, &func_blob, &func_offsets);
    
    free(shared_ct.symbols);
    
    // Compress Symbols
    uint8_t *plain_stream = NULL;
    uint32_t *plain_chunk_index = NULL; // Stores (start_addr, offset) pairs
    verbose(1, "Compressing symbols...\n");
    int num_emitted = compress_symbols(symtable, stbds_arrlen(symtable), file_map, func_map, &plain_stream, &plain_chunk_index);

    uint8_t *stream = NULL;
    uint32_t *chunk_index = NULL; // Stores (start_addr, compressed offset) pairs
    uint32_t max_chunk_margin = 0;
    verbose(1, "Applying compression to symbol chunks...\n");
    compress_symbol_chunks(plain_stream, plain_chunk_index, &stream, &chunk_index, &max_chunk_margin);
    
    verbose(1, "Stats:\n");
    verbose(1, "  Chunk Index: %zu bytes\n", stbds_arrlen(chunk_index) * 4);
    verbose(1, "  File Tab: %zu bytes\n", stbds_arrlen(file_offsets) * 4);
    verbose(1, "  Func Tab: %zu bytes\n", stbds_arrlen(func_offsets) * 4);
    verbose(1, "  Shared Huffman Table: %zu bytes\n", stbds_arrlen(huff_blob));
    verbose(1, "  File Blob: %zu bytes\n", stbds_arrlen(file_blob));
    verbose(1, "  Func Blob: %zu bytes\n", stbds_arrlen(func_blob));
    verbose(1, "  Stream (plain): %zu bytes\n", stbds_arrlen(plain_stream));
    verbose(1, "  Stream (compressed): %zu bytes\n", stbds_arrlen(stream));
    verbose(1, "  Max Chunk Margin: %u bytes\n", max_chunk_margin);

    write_sym_file(outfn, 
        num_emitted,  stbds_arrlen(chunk_index)/2, 
        stbds_arrlen(unique_files), stbds_arrlen(unique_funcs), 
        max_chunk_margin,
        chunk_index,
        file_offsets, file_blob,
        func_offsets, func_blob,
        huff_blob,
        stream);
    // Cleanup
    stbds_arrfree(file_blob);
    stbds_arrfree(func_blob);
    stbds_arrfree(huff_blob);
    stbds_arrfree(file_offsets);
    stbds_arrfree(func_offsets);
    stbds_arrfree(plain_stream);
    stbds_arrfree(plain_chunk_index);
    stbds_arrfree(stream);
    stbds_arrfree(chunk_index);
    stbds_arrfree(unique_files);
    stbds_arrfree(unique_funcs);
    stbds_shfree(file_map);
    stbds_shfree(func_map);
    placeholder_clear();
}

int main(int argc, char *argv[])
{
    winconsole_utf8();
    const char *outfn = NULL;

    int i;
    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0]);
            return 0;
        } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
            flag_verbose++;
        } else if (!strcmp(argv[i], "--no-inlines")) {
            flag_inlines = false;
        } else if (!strcmp(argv[i], "--cpp-shorten")) {
            if (++i == argc) {
                fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                return 1;
            }
            if (strcmp(argv[i], "0") == 0 || strcmp(argv[i], "false") == 0 || strcmp(argv[i], "no") == 0 || strcmp(argv[i], "off") == 0) {
                flag_cpp_shorten = false;
            } else if (strcmp(argv[i], "1") == 0 || strcmp(argv[i], "true") == 0 || strcmp(argv[i], "yes") == 0 || strcmp(argv[i], "on") == 0) {
                flag_cpp_shorten = true;
            } else {
                fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                return 1;
            }
        } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
            if (++i == argc) {
                fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                return 1;
            }
            outfn = argv[i];
        } else if (!strcmp(argv[i], "-m") || !strcmp(argv[i], "--max-len")) {
            if (++i == argc) {
                fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                return 1;
            }
            flag_max_sym_len = atoi(argv[i]);
        } else {
            fprintf(stderr, "invalid flag: %s\n", argv[i]);
            return 1;
        }
    }

    if (i == argc) {
        fprintf(stderr, "missing input filename\n");
        return 1;
    }

    // Find n64 installation directory
    gccprefix_triplet = n64_gccprefix_triplet();
    if (!gccprefix_triplet) {
        // Do not mention N64_GCCPREFIX in the error message, since it is
        // a seldom used configuration.
        fprintf(stderr, "Error: N64_INST environment variable not set\n");
        return 1;
    }

    const char *infn = argv[i];
    if (i < argc-1)
        outfn = argv[i+1];
    else
        outfn = change_ext(infn, ".sym");

    // Check that infn exists and is readable
    FILE *in = fopen(infn, "rb");
    if (!in) {
        fprintf(stderr, "Error: cannot open file: %s\n", infn);
        return 1;
    }
    fclose(in);

    process(infn, outfn);
    return 0;
}

