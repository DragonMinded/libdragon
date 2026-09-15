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
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include <algorithm>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../common/subprocess.h"
#include "../common/polyfill.h"
#include "../common/utils.h"
#include "../common/binout.h"
#include "../common/assetcomp.h"
#include "../common/thread_utils.h"

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
bool flag_all = false;
bool flag_inlines = true;
bool flag_cpp_shorten = true;
const char *gccprefix_triplet = NULL;

// C++ symbol shortening function (n64sym_cppshorten.c)
extern "C" char *cpp_shorten_symbol(const char *sym, int max_len);

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
    fprintf(stderr, "   -a/--all              Add line info for exception-prone instructions\n");
    fprintf(stderr, "   -m/--max-len <N>      Maximum symbol length (default: 96)\n");
    fprintf(stderr, "   --cpp-shorten <0|1>   C++ demangled symbol shortening (default: true)\n");
    fprintf(stderr, "   --no-inlines          Do not export inlined symbols\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "This program requires a libdragon toolchain installed in $N64_INST.\n");
}

// Write a variable-length integer to the buffer
void w_varint(std::vector<uint8_t> &buf, uint32_t val)
{
    do {
        uint8_t byte = val & 0x7F;
        val >>= 7;
        if (val) byte |= 0x80;
        buf.push_back(byte);
    } while (val);
}

// Write a signed variable-length integer to the buffer (zigzag encoding)
void w_signed_varint(std::vector<uint8_t> &buf, int32_t val)
{
    uint32_t uval = (val << 1) ^ (val >> 31);
    w_varint(buf, uval);
}

// Symbol table entry
struct symtable_s {
    uint32_t uuid;
    uint32_t addr;
    char *func;
    char *file;
    int line;

    bool is_func, is_inline;
};
std::vector<symtable_s> symtable;

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
    
    char *file = (char *)malloc(3 + suffix_len + 1);
    strcpy(file, "...");
    memcpy(file + 3, suffix_src, suffix_len);
    file[3 + suffix_len] = 0;
    return file;
}

// Add one inlined frame of an address to the symbol table, normalizing and
// truncating the function name and the file path to the maximum length that
// fits the runtime buffer.
static void symbol_add_frame(uint32_t addr, const std::string &fn, const std::string &pos)
{
    int max_len = MIN(flag_max_sym_len, MAX_BUFFER_SIZE - 8);
    char *func;
    if (flag_cpp_shorten) {
        func = cpp_shorten_symbol(fn.c_str(), max_len);
        verbose(2, "C++ shortening:\n  in  = %s\n  out = %s\n", fn.c_str(), func);
    } else if ((int)fn.size() <= max_len) {
        func = strdup(fn.c_str());
    } else {
        func = strndup(fn.c_str(), max_len);
        if (max_len > 3) strcpy(&func[max_len-3], "...");
    }

    // pos is "file:line". If we need to truncate the filename, truncate_path
    // tries to cut at a directory separator so that we keep a valid path suffix.
    size_t colon = pos.rfind(':');
    assert(colon != std::string::npos);

    symtable.push_back({
        .uuid = (uint32_t)symtable.size(),
        .addr = addr,
        .func = func,
        .file = truncate_path(pos.c_str(), colon, max_len),
        .line = atoi(pos.c_str() + colon + 1),
        .is_func = false,
        .is_inline = true,
    });
}

// Addresses are symbolized in batches by a pool of long-lived addr2line
// processes, so that the work overlaps with the disassembly that produces them.
// See elf_find_callsites() for the whole pipeline.
#define A2L_BATCH    4096
#define A2L_WORKERS  4
#define A2L_QMAX     8

struct a2l_frame { std::string func, pos; };
struct a2l_addr {
    uint32_t addr;
    bool is_func;
    std::vector<a2l_frame> frames;   // innermost inlined frame first
};
typedef std::vector<a2l_addr> a2l_job;

// A worker: a long-lived addr2line process, with its reusable getline buffer
struct a2l_worker { subprocess_s subp; char *buf; size_t cap; };

// Symbolizes a stream of addresses. Addresses are accumulated into batches which
// are streamed to a pool of addr2line workers, so that the work overlaps with
// whatever is producing the addresses. The queue is bounded, so a caller feeding
// addresses faster than they can be resolved is throttled.
struct a2l_s {
    a2l_s(const char *elf) : queue(A2L_QMAX)
    {
        char *bin = NULL;
        asprintf(&bin, "%saddr2line", gccprefix_triplet);
        const char *cmd[16] = {0}; int n = 0;
        cmd[n++] = bin;
        cmd[n++] = "--addresses";
        cmd[n++] = "--functions";
        cmd[n++] = "--demangle";
        if (flag_inlines) cmd[n++] = "--inlines";
        cmd[n++] = "--exe";
        cmd[n++] = elf;
        for (a2l_worker &w : workers) {
            if (subprocess_create(cmd, subprocess_option_no_window, &w.subp) != 0) {
                fprintf(stderr, "Error: cannot run: %s\n", bin);
                exit(1);
            }
        }
        free(bin);
        verbose(1, "Started %d addr2line workers (batch %d)\n", A2L_WORKERS, A2L_BATCH);

        // Run the pool on a side thread, so that the caller can keep feeding us
        pool = std::thread([this]{
            thParaLoop(queue, [this](a2l_job *job, int w) { resolve(job, workers[w]); }, A2L_WORKERS);
        });
    }

    // Wait for all the pending batches to be resolved, add their symbols to the
    // symbol table, and then tear down the addr2line processes.
    ~a2l_s()
    {
        flush();
        queue.close();
        pool.join();
        for (a2l_worker &w : workers) { free(w.buf); subprocess_destroy(&w.subp); }

        verbose(1, "Resolved %zu addr2line batches\n", jobs.size());
        for (auto &job : jobs)
            for (a2l_addr &a : *job) {
                for (a2l_frame &f : a.frames)
                    symbol_add_frame(a.addr, f.func, f.pos);
                symtable.back().is_inline = false;
                symtable.back().is_func = a.is_func;
            }
    }

    // Add an address to symbolize. Duplicates are ignored: the first occurrence
    // wins, so an address which has a function symbol must be added as such first.
    void add(uint32_t addr, bool is_func)
    {
        if (!emitted.insert(addr).second) {
            assert(!is_func && "duplicate address reached with is_func=true; function symbol must be emitted first");
            return;
        }
        batch.push_back({ addr, is_func });
        if (batch.size() >= A2L_BATCH) flush();
    }

private:
    a2l_worker workers[A2L_WORKERS] = {};
    thQueue<a2l_job*> queue;
    std::vector<std::unique_ptr<a2l_job>> jobs;   // all batches, in submission order
    a2l_job batch;
    std::unordered_set<uint32_t> emitted;
    std::thread pool;

    // Hand out the current batch to the pool. Batches are kept around so that
    // their symbols can be emitted in submission order once all are resolved.
    void flush()
    {
        if (batch.empty()) return;
        jobs.push_back(std::make_unique<a2l_job>(std::move(batch)));
        batch.clear();
        queue.push(jobs.back().get());
    }

    // Read a line stripping the EOL, or return NULL on EOF
    static char *readline(a2l_worker &w, FILE *f)
    {
        ssize_t n = getline(&w.buf, &w.cap, f);
        if (n < 0) return NULL;
        while (n && (w.buf[n-1] == '\n' || w.buf[n-1] == '\r')) w.buf[--n] = 0;
        return w.buf;
    }

    // Send a whole batch of addresses to addr2line, and store back the inlined
    // frames resolved for each of them. Runs on a worker thread, so it is static:
    // the only state it is allowed to touch is its own worker and batch.
    static void resolve(a2l_job *job, a2l_worker &w)
    {
        FILE *wr = subprocess_stdin(&w.subp), *rd = subprocess_stdout(&w.subp);

        // With --inlines, each address produces an unknown number of lines, so we
        // detect the end of an address by the echo of the next one. Append a dummy
        // address as sentinel to terminate the last one. NOTE: 0x0 cannot be used
        // as sentinel, as DSOs are partially linked and have symbols at 0.
        auto wr_thread = std::thread([wr, job] {
            for (a2l_addr &a : *job) fprintf(wr, "%08x\n", a.addr);
            fprintf(wr, "0xffffffff\n");
            fflush(wr);
        });

        readline(w, rd);   // echo of the first address
        for (a2l_addr &a : *job) {
            while (char *fn = readline(w, rd)) {
                if (!strncmp(fn, "0x", 2)) break;   // echo of the next address
                std::string func = fn;
                char *pos = readline(w, rd);
                assert(pos);
                a.frames.push_back({ std::move(func), pos });
            }
            assert(!a.frames.empty());
        }
        readline(w, rd);   // sentinel function and position
        readline(w, rd);
        wr_thread.join();
    }
};

void elf_read_function_symbols(const char *elf, std::unordered_set<uint32_t> &all_functions)
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
            all_functions.insert(addr);
        }
    }
    free(line);
    free(cmd);
    pclose(f);
}

static bool disasm_get_fields(const char *line, char **mnemonic_out, char **operands_out)
{
    *mnemonic_out = NULL;
    *operands_out = NULL;

    // Keep parsing simple: split objdump line by tabs and take fields.
    // Format is usually: ADDR: \t ENCODING \t MNEMONIC \t OPERANDS
    char *tmp = strdup(line);
    if (!tmp) return false;

    char *saveptr = NULL;
    char *f_addr = strtok_r(tmp, "\t", &saveptr);
    char *f_encoding = strtok_r(NULL, "\t", &saveptr);
    char *f_mnemonic = strtok_r(NULL, "\t", &saveptr);
    char *f_operands = strtok_r(NULL, "\t", &saveptr);
    if (!f_addr || !f_encoding || !f_mnemonic) {
        free(tmp);
        return false;
    }

    if (!f_mnemonic[0]) {
        free(tmp);
        return false;
    }

    *mnemonic_out = strdup(f_mnemonic);
    if (f_operands && f_operands[0]) {
        *operands_out = strdup(f_operands);
    }
    free(tmp);
    return *mnemonic_out != NULL;
}

// Returns true if the mnemonic can generate an exception at runtime. This is
// a "pragmatic" version, as all opcodes could theoretically trap in weird cases
// (eg when run via an invalid TLB). But we want to keep SYM to a decent size, so
// we want to cover the "99% common" cases here. This means:
//  * FPU opcodes as they can always trigger exceptions for invalid operands
//  * Memory accesses. We ignore accesses via sp (stack) and gp (small data), as
//    those are extremely unlikely to trigger exceptions. Normally, crashes are
//    because of invalid pointers built at runtime by user code, and those will
//    go through different registers.
//  * Explicitly trapping instructions like teq / tne, etc. These are rare anyway.
static bool mnemonic_can_trap(const char *mn, const char *ops)
{
    static const char *memory_mnemonics[] = {
        "lb", "lbu", "lh", "lhu", "lw", "lwl", "lwr", "ld", "ldl", "ldr",
        "sb", "sh", "sw", "swl", "swr", "sd", "sdl", "sdr",
        "ll", "sc", "lld", "scd",
        "lwc1", "swc1", "ldc1", "sdc1",
    };
    static const char *fault_mnemonics[] = {
        "syscall", "break",
        "teq", "tne", "tge", "tgeu", "tlt", "tltu",
        "teqi", "tnei", "tgei", "tgeiu", "tlti", "tltiu",
    };
    static const char *fpu_mnemonics[] = {
        "add.", "sub.", "mul.", "div.", "sqrt.", "abs.", "mov.", "neg.",
        "round.", "trunc.", "ceil.", "floor.", "cvt.", "c.",
    };

    for (size_t i = 0; i < sizeof(memory_mnemonics) / sizeof(memory_mnemonics[0]); i++) {
        if (strcmp(mn, memory_mnemonics[i]) == 0) {
            if (strstr(ops, "(sp)") || strstr(ops, "($sp)")) return false;
            if (strstr(ops, "(gp)") || strstr(ops, "($gp)")) return false;
            return true;
        }
    }
    for (size_t i = 0; i < sizeof(fault_mnemonics) / sizeof(fault_mnemonics[0]); i++) {
        if (strcmp(mn, fault_mnemonics[i]) == 0) return true;
    }

    for (size_t i = 0; i < sizeof(fpu_mnemonics) / sizeof(fpu_mnemonics[0]); i++) {
        size_t n = strlen(fpu_mnemonics[i]);
        if (strncmp(mn, fpu_mnemonics[i], n) == 0) return true;
    }

    return false;
}

bool elf_find_callsites(const char *elf)
{
    std::unordered_set<uint32_t> all_functions;
    elf_read_function_symbols(elf, all_functions);
    verbose(1, "Found %zu function symbols\n", all_functions.size());

    // Start the symbolizer. This must happen before the popen() below, or the
    // addr2line processes would inherit the objdump pipe write-end, and we would
    // then never see EOF while reading the disassembly. Batches still pending are
    // drained (and their symbols emitted) when a2l goes out of scope.
    a2l_s a2l(elf);

    // Start objdump to parse the disassembly of the ELF file
    char *cmd = NULL;
    asprintf(&cmd, "%sobjdump -d %s", gccprefix_triplet, elf);
    verbose(1, "Running: %s\n", cmd);
    FILE *disasm = popen(cmd, "r");
    if (!disasm) {
        fprintf(stderr, "Error: cannot run: %s\n", cmd);
        exit(1);
    }
    free(cmd);

    char *line = NULL; size_t line_size = 0;
    while (getline(&line, &line_size, disasm) != -1) {
        // Find the functions: use the all_functions whitelist to skip symbols
        // which are not functions in the text segment.
        if (strstr(line, ">:")) {
            uint32_t addr = strtoul(line, NULL, 16);
            if (all_functions.count(addr)) {
                a2l.add(addr, true);
            }
        }

        char *mn = NULL;
        char *ops = NULL;
        if (!disasm_get_fields(line, &mn, &ops)) continue;

        bool should_emit = false;
        if (strcmp(mn, "jal") == 0 || strcmp(mn, "jalr") == 0 || strcmp(mn, "syscall") == 0) {
            // Keep default behavior for callsites regardless of --all.
            should_emit = true;
        } else if (flag_all) {
            if (mnemonic_can_trap(mn, ops)) {
                should_emit = true;
            }
        }

        if (should_emit) {
            uint32_t addr = strtoul(line, NULL, 16);
            a2l.add(addr, false);
        }
        free(mn);
        free(ops);
    }
    free(line);
    int status = pclose(disasm);

#ifdef __MINGW32__
    return status == 0;
#else
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

void compress_strings(const std::vector<char*> &strings, huff_code_t *huff_table,
                      std::vector<uint8_t> &blob, std::vector<uint32_t> &index)
{
    // Statistics
    int stats_raw_suffix_len = 0;
    int stats_huff_bits = 0;
    int stats_varint_bytes = 0;
    int stats_padding_bytes = 0;

    int n = (int)strings.size();
    int i = 0;
    while (i < n) {
        // Start a new block
        uint32_t block_start_idx = i;
        uint32_t block_blob_off = (uint32_t)blob.size();
        index.push_back(block_start_idx);
        index.push_back(block_blob_off);
        
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
            int current_bits = (int)stbds_arrlen(bw.buf) * 8 + bw.cur_bit;
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
        int block_len = (int)stbds_arrlen(bw.buf);
        blob.insert(blob.end(), bw.buf, bw.buf + block_len);
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
    verbose(1, "    Total Blob Size: %zu bytes (%.1f%% of Front Coding)\n", 
        blob.size(), 100.0 * blob.size() / (stats_front_coding_size ? stats_front_coding_size : 1));
}

// Compress the symbol table into a delta-encoded stream split into chunks.
// The symbols must be sorted by address. The stream is divided into independent chunks
// that fit into MAX_BUFFER_SIZE. A separate index tracks the start address and
// stream offset of each chunk for random access.
int compress_symbols(
    const std::vector<symtable_s> &symtable,
    const std::unordered_map<std::string, int> &file_map,
    const std::unordered_map<std::string, int> &func_map,
    std::vector<uint8_t> &stream, std::vector<uint32_t> &chunk_index)
{
    int nsyms = (int)symtable.size();
    if (nsyms == 0) return 0;

    // Temp buffer for current chunk
    std::vector<uint8_t> chunk_buf;
    uint32_t chunk_start_addr = symtable[0].addr;
    uint32_t last_func_addr = symtable[0].is_func ? symtable[0].addr : 0;
    
    // State machine
    uint32_t state_addr = chunk_start_addr;
    int state_file[2] = {0, 0}; // [0]=non-inline, [1]=inline
    int state_func[2] = {0, 0}; // [0]=non-inline, [1]=inline
    int state_line[2] = {0, 0}; // [0]=non-inline, [1]=inline

    // Write per-chunk header: func offset of first symbol (VarInt)
    uint32_t chunk_func_off = last_func_addr ? (chunk_start_addr - last_func_addr) : 0;
    w_varint(chunk_buf, chunk_func_off);
    
    int emitted = 0;

    for (int i=0; i<nsyms; i++) {
        const symtable_s *sym = &symtable[i];

        int file_idx = file_map.at(sym->file);
        int func_idx = func_map.at(sym->func);
        
        // Check if adding this symbol might overflow chunk size (estimate max 21 bytes)
        // If chunk is getting full, flush it.
        // This is used to tune internal compressions to make sure the provided value
        // is sufficient. Testing shows that growing after 512 has very minimal size
        // savings.
        bool flush = (chunk_buf.size() > MAX_BUFFER_SIZE - 32);
        
        if (flush) {
            // Flush chunk
            chunk_index.push_back(chunk_start_addr);
            chunk_index.push_back((uint32_t)stream.size());
            
            // Append chunk to stream
            stream.insert(stream.end(), chunk_buf.begin(), chunk_buf.end());
            
            // Reset for new chunk
            chunk_buf.clear();
            chunk_start_addr = sym->addr;
            state_addr = sym->addr;
            state_file[0] = state_file[1] = 0;
            state_func[0] = state_func[1] = 0;
            state_line[0] = state_line[1] = 0;
            chunk_func_off = last_func_addr ? (chunk_start_addr - last_func_addr) : 0;
            w_varint(chunk_buf, chunk_func_off);
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
        
        chunk_buf.push_back(op);
        if (delta_file != 0) w_signed_varint(chunk_buf, delta_file);
        if (delta_func != 0) w_signed_varint(chunk_buf, delta_func);
        if (delta_line != 0) w_signed_varint(chunk_buf, delta_line);
        if (has_addr_param)  w_varint(chunk_buf, addr_param);

        emitted++;
        
        state_addr = sym->addr;
        state_file[sid] = file_idx;
        state_func[sid] = func_idx;
        state_line[sid] = sym->line;

        if (sym->is_func)
            last_func_addr = sym->addr;
    }
    
    // Flush final chunk
    if (!chunk_buf.empty()) {
        chunk_index.push_back(chunk_start_addr);
        chunk_index.push_back((uint32_t)stream.size());
        stream.insert(stream.end(), chunk_buf.begin(), chunk_buf.end());
    }

    return emitted;
}

static void compress_symbol_chunks(
    const std::vector<uint8_t> &plain_stream, const std::vector<uint32_t> &plain_chunk_index,
    std::vector<uint8_t> &cmp_stream, std::vector<uint32_t> &cmp_chunk_index,
    uint32_t *max_chunk_margin)
{
    int num_chunks = (int)plain_chunk_index.size() / 2;
    *max_chunk_margin = 0;
    if (num_chunks == 0) return;

    struct cmp_res { uint8_t *buf; int size; int margin; };
    std::vector<cmp_res> res(num_chunks);

    thParaLoop(num_chunks, [&](int i) {
        uint32_t plain_off = plain_chunk_index[i * 2 + 1];
        uint32_t plain_end = (i + 1 < num_chunks) ? plain_chunk_index[(i + 1) * 2 + 1] : (uint32_t)plain_stream.size();
        assert(plain_end >= plain_off);
        uint32_t plain_size = plain_end - plain_off;

        int cmp_size = 0, winsize = 0, margin = 0;
        uint8_t *cmp_buf = NULL;
        asset_compress_mem_raw(3, plain_stream.data() + plain_off, (int)plain_size,
                               &cmp_buf, &cmp_size, &winsize, &margin);
        if (!cmp_buf || cmp_size <= 0) {
            fprintf(stderr, "Error: compression failed for chunk %d\n", i);
            exit(1);
        }

        // Keep compressed chunk sizes even so the next chunk offset preserves
        // ROM/RAM parity constraints required by dma_read().
        if (cmp_size & 1) {
            cmp_buf = (uint8_t *)realloc(cmp_buf, cmp_size + 1);
            cmp_buf[cmp_size++] = 0;
        }
        res[i] = { cmp_buf, cmp_size, margin };
    }, 4);

    for (int i = 0; i < num_chunks; i++) {
        if ((uint32_t)res[i].margin > *max_chunk_margin)
            *max_chunk_margin = res[i].margin;

        cmp_chunk_index.push_back(plain_chunk_index[i * 2 + 0]);
        cmp_chunk_index.push_back((uint32_t)cmp_stream.size());
        cmp_stream.insert(cmp_stream.end(), res[i].buf, res[i].buf + res[i].size);
        free(res[i].buf);
    }
}

void write_sym_file(const char *outfn, 
    int num_symbols, int num_chunks, int num_files, int num_funcs,
    uint32_t max_chunk_margin,
    const std::vector<uint32_t> &chunk_index,
    const std::vector<uint32_t> &file_offsets, const std::vector<uint8_t> &file_blob,
    const std::vector<uint32_t> &func_offsets, const std::vector<uint8_t> &func_blob,
    const std::vector<uint8_t> &huff_blob,
    const std::vector<uint8_t> &stream)
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
    w32(out, (uint32_t)(file_offsets.size() / 2));
    w32(out, (uint32_t)(func_offsets.size() / 2));
    w32(out, (uint32_t)huff_blob.size());
    w32(out, (uint32_t)file_blob.size());
    w32(out, (uint32_t)func_blob.size());
    w32(out, (uint32_t)stream.size());
    
    // Write Sections
    walign(out, 8);
    w32_at(out, chunk_idx_off_ph, ftell(out));
    for (size_t i=0; i<chunk_index.size(); i++) w32(out, chunk_index[i]);
    
    walign(out, 8);
    w32_at(out, file_tab_off_ph, ftell(out));
    for (size_t i=0; i<file_offsets.size(); i++) w32(out, file_offsets[i]);
    
    walign(out, 8);
    w32_at(out, func_tab_off_ph, ftell(out));
    for (size_t i=0; i<func_offsets.size(); i++) w32(out, func_offsets[i]);

    walign(out, 8);
    w32_at(out, huff_tab_off_ph, ftell(out));
    fwrite(huff_blob.data(), huff_blob.size(), 1, out);
    
    walign(out, 8);
    w32_at(out, file_blob_off_ph, ftell(out));
    fwrite(file_blob.data(), file_blob.size(), 1, out);
    
    walign(out, 8);
    w32_at(out, func_blob_off_ph, ftell(out));
    fwrite(func_blob.data(), func_blob.size(), 1, out);
    
    walign(out, 8);
    w32_at(out, stream_off_ph, ftell(out));
    fwrite(stream.data(), stream.size(), 1, out);
    
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
    verbose(1, "Found %zu callsites\n", symtable.size());

    // If the symtable is empty, there's nothing else to do
    if (symtable.empty()) {
        verbose(1, "No symbols found\n");
        return;
    }

    // Sort the symbol table by address. On address match, keep insertion order
    // (uuid) so inline chains stay stable.
    std::sort(symtable.begin(), symtable.end(), [](const symtable_s &a, const symtable_s &b) {
        if (a.addr != b.addr) return a.addr < b.addr;
        return a.uuid < b.uuid;
    });

    // Prepare in-memory buffers
    std::vector<uint8_t> file_blob;
    std::vector<uint8_t> func_blob;
    std::vector<uint32_t> file_offsets;
    std::vector<uint32_t> func_offsets;
    
    // Create sorted list of unique files/funcs
    std::vector<char*> unique_files;
    std::vector<char*> unique_funcs;
    
    for (size_t i=0; i<symtable.size(); i++) {
        symtable_s *s = &symtable[i];
        if (s->file) unique_files.push_back(s->file);
        if (s->func) unique_funcs.push_back(s->func);
    }
    
    // Sort and unique
    auto cmp_str = [](const char *a, const char *b) { return strcmp(a, b) < 0; };
    std::sort(unique_files.begin(), unique_files.end(), cmp_str);
    std::sort(unique_funcs.begin(), unique_funcs.end(), cmp_str);
    
    // Remove duplicates in place
    int nfiles = 0;
    if (!unique_files.empty()) {
        size_t w = 1;
        for (size_t r = 1; r < unique_files.size(); r++) {
            if (strcmp(unique_files[r], unique_files[r-1]) != 0)
                unique_files[w++] = unique_files[r];
        }
        unique_files.resize(w);
        nfiles = (int)w;
    }
    
    int nfuncs = 0;
    if (!unique_funcs.empty()) {
        size_t w = 1;
        for (size_t r = 1; r < unique_funcs.size(); r++) {
            if (strcmp(unique_funcs[r], unique_funcs[r-1]) != 0)
                unique_funcs[w++] = unique_funcs[r];
        }
        unique_funcs.resize(w);
        nfuncs = (int)w;
    }
    
    // Create maps from string -> index
    std::unordered_map<std::string, int> file_map;
    std::unordered_map<std::string, int> func_map;
    
    for (int i=0; i<nfiles; i++) file_map[unique_files[i]] = i;
    for (int i=0; i<nfuncs; i++) func_map[unique_funcs[i]] = i;
    
    // Shared Huffman Table
    verbose(1, "Calculating shared Huffman table...\n");
    int shared_freqs[256] = {0};
    // collect_string_freqs still expects a stbds array
    char **freq_files = NULL;
    char **freq_funcs = NULL;
    for (char *s : unique_files) stbds_arrput(freq_files, s);
    for (char *s : unique_funcs) stbds_arrput(freq_funcs, s);
    collect_string_freqs(freq_files, shared_freqs);
    collect_string_freqs(freq_funcs, shared_freqs);
    stbds_arrfree(freq_files);
    stbds_arrfree(freq_funcs);
    
    huff_code_t shared_huff_table[256] = {};
    build_limited_huffman_tree(shared_freqs, HUFF_MAX_CODE_LEN, shared_huff_table);
    
    CanonicalTables shared_ct;
    generate_canonical_tables(shared_huff_table, &shared_ct);
    
    // Write shared table to huff_blob (huffman API still uses stbds)
    uint8_t *huff_tmp = NULL;
    write_huff_header(&shared_ct, &huff_tmp);
    std::vector<uint8_t> huff_blob(huff_tmp, huff_tmp + stbds_arrlen(huff_tmp));
    stbds_arrfree(huff_tmp);
    
    // Compress string blobs using shared table (no headers)
    verbose(1, "Compressing strings (Shared Table)...\n");
    compress_strings(unique_files, shared_huff_table, file_blob, file_offsets);
    compress_strings(unique_funcs, shared_huff_table, func_blob, func_offsets);
    
    free(shared_ct.symbols);
    
    // Compress Symbols
    std::vector<uint8_t> plain_stream;
    std::vector<uint32_t> plain_chunk_index; // Stores (start_addr, offset) pairs
    verbose(1, "Compressing symbols...\n");
    int num_emitted = compress_symbols(symtable, file_map, func_map, plain_stream, plain_chunk_index);

    std::vector<uint8_t> stream;
    std::vector<uint32_t> chunk_index; // Stores (start_addr, compressed offset) pairs
    uint32_t max_chunk_margin = 0;
    verbose(1, "Applying compression to symbol chunks...\n");
    compress_symbol_chunks(plain_stream, plain_chunk_index, stream, chunk_index, &max_chunk_margin);
    
    verbose(1, "Stats:\n");
    verbose(1, "  Chunk Index: %zu bytes\n", chunk_index.size() * 4);
    verbose(1, "  File Tab: %zu bytes\n", file_offsets.size() * 4);
    verbose(1, "  Func Tab: %zu bytes\n", func_offsets.size() * 4);
    verbose(1, "  Shared Huffman Table: %zu bytes\n", huff_blob.size());
    verbose(1, "  File Blob: %zu bytes\n", file_blob.size());
    verbose(1, "  Func Blob: %zu bytes\n", func_blob.size());
    verbose(1, "  Stream (plain): %zu bytes\n", plain_stream.size());
    verbose(1, "  Stream (compressed): %zu bytes\n", stream.size());
    verbose(1, "  Max Chunk Margin: %u bytes\n", max_chunk_margin);

    write_sym_file(outfn, 
        num_emitted,  (int)(chunk_index.size()/2), 
        (int)unique_files.size(), (int)unique_funcs.size(), 
        max_chunk_margin,
        chunk_index,
        file_offsets, file_blob,
        func_offsets, func_blob,
        huff_blob,
        stream);
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
        } else if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--all")) {
            flag_all = true;
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
