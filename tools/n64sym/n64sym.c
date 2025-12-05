/*
    n64sym: generate a symbol table for an N64 ROM
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
 */
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include <math.h>
#include <stdlib.h>
#include "../common/subprocess.h"

#include "../common/polyfill.h"
#include "../common/utils.h"
#include "../common/binout.h"
#include "n64sym.h"

#include "../common/binout.c"

// Size of the runtime buffer that will be used during SYMT access/decompression.
// This is used to tune internal compressions to make sure the provided value
// is sufficient. Testing shows that growing after 512 has very minimal size
// savings.
#define MAX_BUFFER_SIZE  512

bool flag_verbose = false;
int flag_max_sym_len = 64;
bool flag_inlines = true;
const char *gccprefix_triplet = NULL;

// Printf if verbose
void verbose(const char *fmt, ...) {
    if (flag_verbose) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
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
    fprintf(stderr, "   -m/--max-len <N>      Maximum symbol length (default: 64)\n");
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

// Symbol table entry
struct symtable_s {
    uint32_t uuid;
    uint32_t addr;
    char *func;
    char *file;
    int line;

    bool is_func, is_inline;
} *symtable = NULL;

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

        // If the function of name is longer than 64 bytes, truncate it. This also
        // avoid paradoxically long function names like in C++ that can even be
        // several thousands of characters long.
        // Also ensure it fits in the runtime buffer.
        int max_len = MIN(flag_max_sym_len, MAX_BUFFER_SIZE - 8);
        char *func = strndup(line_buf, MIN(n, max_len));
        if (n > max_len) strcpy(&func[max_len-3], "...");

        // Second line is the file name and line number
        int ret = getline(&line_buf, &line_buf_size, addr2line_r);
        assert(ret != -1);
        char *colon = strrchr(line_buf, ':');
        
        // Filename must fit into max_len too (so both the maximum length requested by the user,
        // and the buffer size). If we need to truncate the filename, elide the prefix until we fit.
        int file_len = colon - line_buf;
        char *file;
        if (file_len > max_len) {
            file = malloc(max_len + 1);
            strcpy(file, "...");
            int suffix_len = max_len - 3;
            memcpy(file + 3, line_buf + (file_len - suffix_len), suffix_len);
            file[max_len] = 0;
        } else {
            file = strndup(line_buf, file_len);
        }
        int line = atoi(colon + 1);

        // Add the callsite to the list
        stbds_arrput(symtable, ((struct symtable_s) {
            .uuid = stbds_arrlen(symtable),
            .addr = addr,
            .func = func,
            .file = file,
            .line = line,
            .is_func = is_func,
            .is_inline = true,
        }));
        at_least_one = true;
    }
    assert(at_least_one);
    symtable[stbds_arrlen(symtable)-1].is_inline = false;

    // Read and skip the two remaining lines (function and file position)
    // that refers to the dummy 0x0 address
    getline(&line_buf, &line_buf_size, addr2line_r);
    getline(&line_buf, &line_buf_size, addr2line_r);
}

bool elf_find_callsites(const char *elf)
{
    // Start objdump to parse the disassembly of the ELF file
    char *cmd = NULL;
    asprintf(&cmd, "%sobjdump -d %s", gccprefix_triplet, elf);
    verbose("Running: %s\n", cmd);
    FILE *disasm = popen(cmd, "r");
    if (!disasm) {
        fprintf(stderr, "Error: cannot run: %s\n", cmd);
        exit(1);
    }

    // Parse the disassembly
    char *line = NULL; size_t line_size = 0;
    while (getline(&line, &line_size, disasm) != -1) {
        // Find the functions
        if (strstr(line, ">:")) {
            uint32_t addr = strtoul(line, NULL, 16);
            symbol_add(elf, addr, true);
        }
        // Find the callsites
        if (strstr(line, "\tjal\t") || strstr(line, "\tjalr\t") || strstr(line, "\tsyscall")) {
            uint32_t addr = strtoul(line, NULL, 16);
            symbol_add(elf, addr, false);
        }
    }
    free(line);
    free(cmd);
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

void compress_strings(char **strings, huff_code_t *huff_table, CanonicalTables *ct, 
                           bool write_header, uint8_t **blob, uint32_t **index) 
{
    if (write_header) {
        write_huff_header(ct, blob);
    }
    
    // Calculate header size for stats
    int header_size = write_header ? stbds_arrlen(*blob) : 0;

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
            
            // Stats accumulation
            stats_raw_suffix_len += suffix_len;
            
            int prefix_delta = common - last_common;
            uint32_t z_delta = zigzag_encode(prefix_delta);
            int z_delta_bits = exp_golomb_len(z_delta);
            stats_varint_bytes += (z_delta_bits + 7) / 8; // Approx stats
            
            for (int k=0; k<=suffix_len; k++) {
                unsigned char c = (unsigned char)suffix[k];
                stats_huff_bits += huff_table[c].len;
            }

            // Backup current buffer state
            int old_buf_len = stbds_arrlen(bw.buf);
            uint8_t old_cur_byte = bw.cur_byte;
            int old_cur_bit = bw.cur_bit;

            // Encode to bitstream: Delta Prefix (Exp-Golomb) + Suffix (Huffman)
            bw_write_exp_golomb(&bw, z_delta);
            for (int k=0; k<=suffix_len; k++) {
                unsigned char c = (unsigned char)suffix[k];
                bw_write(&bw, huff_table[c].code, huff_table[c].len);
            }
            
            // Check size
            int current_size = stbds_arrlen(bw.buf) + (bw.cur_bit > 0 ? 1 : 0);
            
            if (current_size > MAX_BUFFER_SIZE) {
                assert(count != 0 && "String too long for block buffer despite truncation");
                // Backtrack stats
                stats_raw_suffix_len -= suffix_len;
                stats_varint_bytes -= (z_delta_bits + 7) / 8;
                for (int k=0; k<=suffix_len; k++) {
                    unsigned char c = (unsigned char)suffix[k];
                    stats_huff_bits -= huff_table[c].len;
                }

                // Revert
                stbds_arrsetlen(bw.buf, old_buf_len);
                bw.cur_byte = old_cur_byte;
                bw.cur_bit = old_cur_bit;
                break;
            }
            
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

    verbose("  Huffman Stats:\n");
    verbose("    Raw suffixes: %d bytes\n", stats_raw_suffix_len);
    verbose("    Equivalent Front Coding Size: %d bytes\n", stats_front_coding_size);
    verbose("    Compressed suffixes (Huffman): %d bytes (%.1f%%)\n", 
        stats_huff_bytes, 100.0 * stats_huff_bytes / (stats_raw_suffix_len ? stats_raw_suffix_len : 1));
    verbose("    Overhead: VarInts: %d bytes, Header: %d bytes, Padding: %d bytes\n", 
        stats_varint_bytes, header_size, stats_padding_bytes);
    verbose("    Total Blob Size: %d bytes (%.1f%% of Front Coding)\n", 
        stbds_arrlen(*blob), 100.0 * stbds_arrlen(*blob) / (stats_front_coding_size ? stats_front_coding_size : 1));
}

// Compress the symbol table into a delta-encoded stream split into chunks.
// The symbols must be sorted by address. The stream is divided into independent chunks
// that fit into MAX_BUFFER_SIZE. A separate index tracks the start address and
// stream offset of each chunk for random access.
void compress_symbols(
    struct symtable_s *symtable, int nsyms,
    struct gl_map *file_map, struct gl_map *func_map,
    uint8_t **stream, uint32_t **chunk_index)
{
    // Temp buffer for current chunk
    uint8_t *chunk_buf = NULL;
    uint32_t chunk_start_addr = symtable[0].addr;
    
    // State machine
    uint32_t state_addr = chunk_start_addr;
    int state_file = 0;
    int state_func = 0;
    int state_line = 0;
    
    for (int i=0; i<nsyms; i++) {
        struct symtable_s *sym = &symtable[i];
        struct symtable_s *prev_sym = i > 0 ? &symtable[i-1] : NULL;

        // If a function contains a jal as first opcode, we will find two symbols here:
        // one for the function start, and one for the callsite. We need to skip the second one.
        if (prev_sym && prev_sym->is_func && !sym->is_func && prev_sym->addr == sym->addr)
            continue;

        // Sometimes we get spurious inlines; skip those as well
        if (prev_sym && prev_sym->is_inline && !sym->is_inline && prev_sym->addr == sym->addr)
            continue;
        
        int file_idx = stbds_shget(file_map, sym->file);
        int func_idx = stbds_shget(func_map, sym->func);
        
        // Check if adding this symbol might overflow chunk size (estimate max 21 bytes)
        // If chunk is getting full, flush it.
        // This is used to tune internal compressions to make sure the provided value
        // is sufficient. Testing shows that growing after 512 has very minimal size
        // savings.
        bool flush = (stbds_arrlen(chunk_buf) > MAX_BUFFER_SIZE - 32);
        
        if (flush) {
            // End of chunk marker
            stbds_arrput(chunk_buf, 0x00);

            // Pad chunk to 2 bytes alignment for DMA
            if (stbds_arrlen(chunk_buf) % 2 != 0)
                stbds_arrput(chunk_buf, 0x00);

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
            state_file = 0;
            state_func = 0;
            state_line = 0;
        }
        
        // Calculate deltas
        int delta_file = file_idx - state_file;
        int delta_func = func_idx - state_func;
        int delta_line = sym->line - state_line;
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
        
        assert(op != 0x00 && "Opcode should never be 0 (EOM) within the stream");
        stbds_arrput(chunk_buf, op);
        if (delta_file != 0) w_signed_varint(&chunk_buf, delta_file);
        if (delta_func != 0) w_signed_varint(&chunk_buf, delta_func);
        if (delta_line != 0) w_signed_varint(&chunk_buf, delta_line);
        if (has_addr_param)  w_varint(&chunk_buf, addr_param);
        
        state_addr = sym->addr;
        state_file = file_idx;
        state_func = func_idx;
        state_line = sym->line;
    }
    
    // Flush final chunk
    if (stbds_arrlen(chunk_buf) > 0) {
        stbds_arrput(*chunk_index, chunk_start_addr);
        stbds_arrput(*chunk_index, stbds_arrlen(*stream));
        stbds_arrput(chunk_buf, 0x00); // EOM
        // Pad chunk to 2 bytes alignment for DMA
        if (stbds_arrlen(chunk_buf) % 2 != 0)
            stbds_arrput(chunk_buf, 0x00);
        int cur_len = stbds_arrlen(*stream);
        int chunk_len = stbds_arrlen(chunk_buf);
        stbds_arrsetlen(*stream, cur_len + chunk_len);
        memcpy(*stream + cur_len, chunk_buf, chunk_len);
    }

    stbds_arrfree(chunk_buf);
}

void write_sym_file(const char *outfn, 
    int num_symbols, int num_chunks,
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
    
    // Write V3 Header
    fwrite("SYMT", 4, 1, out);
    w32(out, 3); // Version 3
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
    
    // Sizes
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
    verbose("  Total File Size: %d bytes\n", size);

    fclose(out);
}

void process(const char *infn, const char *outfn)
{
    verbose("Processing: %s -> %s\n", infn, outfn);

    // First, find all functions and call sites. We do this by disassembling
    // the ELF file and grepping it.
    if (!elf_find_callsites(infn)) {
        fprintf(stderr, "Error: objdump failed\n");
        exit(1);
    }
    verbose("Found %d callsites\n", stbds_arrlen(symtable));

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
    verbose("Calculating shared Huffman table...\n");
    int shared_freqs[256] = {0};
    collect_string_freqs(unique_files, shared_freqs);
    collect_string_freqs(unique_funcs, shared_freqs);
    
    huff_code_t shared_huff_table[256] = {0};
    huff_node_t *shared_root = build_huffman_tree(shared_freqs);
    huff_calc_lengths(shared_root, 0, shared_huff_table);
    
    CanonicalTables shared_ct;
    generate_canonical_tables(shared_huff_table, &shared_ct);
    
    // Write shared table to huff_blob
    uint8_t *huff_blob = NULL;
    write_huff_header(&shared_ct, &huff_blob);
    
    // Compress string blobs using shared table (no headers)
    verbose("Compressing strings (Shared Table)...\n");
    compress_strings(unique_files, shared_huff_table, &shared_ct, false, &file_blob, &file_offsets);
    compress_strings(unique_funcs, shared_huff_table, &shared_ct, false, &func_blob, &func_offsets);
    
    free(shared_ct.symbols);
    huff_free_tree(shared_root);
    
    // Compress Symbols
    uint8_t *stream = NULL;
    uint32_t *chunk_index = NULL; // Stores (start_addr, offset) pairs    
    verbose("Compressing symbols...\n");
    compress_symbols(symtable, stbds_arrlen(symtable), file_map, func_map, &stream, &chunk_index);
    
    verbose("Stats:\n");
    verbose("  Chunk Index: %zu bytes\n", stbds_arrlen(chunk_index) * 4);
    verbose("  File Tab: %zu bytes\n", stbds_arrlen(file_offsets) * 4);
    verbose("  Func Tab: %zu bytes\n", stbds_arrlen(func_offsets) * 4);
    verbose("  Shared Huffman Table: %zu bytes\n", stbds_arrlen(huff_blob));
    verbose("  File Blob: %zu bytes\n", stbds_arrlen(file_blob));
    verbose("  Func Blob: %zu bytes\n", stbds_arrlen(func_blob));
    verbose("  Stream: %zu bytes\n", stbds_arrlen(stream));

    write_sym_file(outfn, 
        stbds_arrlen(symtable), stbds_arrlen(chunk_index)/2, chunk_index,
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
            flag_verbose = true;
        } else if (!strcmp(argv[i], "--no-inlines")) {
            flag_inlines = false;
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

