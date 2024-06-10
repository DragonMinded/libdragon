#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include "common/subprocess.h"
#include "common/polyfill.h"
#include "common/utils.h"
#include "common/binout.h"

#include "common/binout.c"

bool flag_verbose = false;
int flag_max_sym_len = 64;
bool flag_inlines = true;
const char *n64_inst = NULL;

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
    fprintf(stderr, "%s - Prepare symbol table for N64 ROMs\n", progname);
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

char *stringtable = NULL;
struct { char *key; int value; } *string_hash = NULL;

int stringtable_add(char *word)
{
    if (!string_hash) {
        stbds_sh_new_arena(string_hash);
        stbds_shdefault(string_hash, -1);
    }

    int word_len = strlen(word);
    if (stringtable) {
        int pos = stbds_shget(string_hash, word);
        if (pos >= 0)
            return pos;
    }

    // Append the word (without the trailing \0)
    int idx = stbds_arraddnindex(stringtable, word_len);
    memcpy(stringtable + idx, word, word_len);

    // Add all prefixes to the hash
    for (int i = word_len; i >= 2; --i) {
        char ch = word[i];
        word[i] = 0;
        stbds_shput(string_hash, word, idx);
        word[i] = ch;
    }
    return idx;
}

struct symtable_s {
    uint32_t uuid;
    uint32_t addr;
    char *func;
    char *file;
    int line;

    int func_sidx;
    int file_sidx;

    int func_offset;

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
            asprintf(&addrbin, "%s/bin/mips64-elf-addr2line", n64_inst);

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
        char *func = strndup(line_buf, MIN(n, flag_max_sym_len));
        if (n > flag_max_sym_len) strcpy(&func[flag_max_sym_len-3], "...");

        // Second line is the file name and line number
        int ret = getline(&line_buf, &line_buf_size, addr2line_r);
        assert(ret != -1);
        char *colon = strrchr(line_buf, ':');
        char *file = strndup(line_buf, colon - line_buf);
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

void elf_find_callsites(const char *elf)
{
    // Start objdump to parse the disassembly of the ELF file
    char *cmd = NULL;
    asprintf(&cmd, "%s/bin/mips64-elf-objdump -d %s", n64_inst, elf);
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
        if (strstr(line, "\tjal\t") || strstr(line, "\tjalr\t")) {
            uint32_t addr = strtoul(line, NULL, 16);
            symbol_add(elf, addr, false);
        }
    }
    free(line);
    pclose(disasm);
    free(cmd);
}

void compute_function_offsets(void)
{
    uint32_t func_addr = 0;
    for (int i=0; i<stbds_arrlen(symtable); i++) {
        struct symtable_s *s = &symtable[i];
        if (s->is_func) {
            func_addr = s->addr;
            s->func_offset = 0;
        } else {
            s->func_offset = s->addr - func_addr;
        }
    }
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

int symtable_sort_by_func(const void *a, const void *b)
{
    const struct symtable_s *sa = a;
    const struct symtable_s *sb = b;
    int sa_len = sa->func ? strlen(sa->func) : 0;
    int sb_len = sb->func ? strlen(sb->func) : 0;
    return sb_len - sa_len;
}

void process(const char *infn, const char *outfn)
{
    verbose("Processing: %s -> %s\n", infn, outfn);

    // First, find all functions and call sites. We do this by disassembling
    // the ELF file and grepping it.
    elf_find_callsites(infn);
    verbose("Found %d callsites\n", stbds_arrlen(symtable));

    // Sort the symbole table by symbol length. We want longer symbols
    // to go in first, so that shorter symbols can be found as substrings.
    // We sort by function name rather than file name, because we expect
    // substrings to match more in functions.
    verbose("Sorting symbol table...\n");
    qsort(symtable, stbds_arrlen(symtable), sizeof(struct symtable_s), symtable_sort_by_func);

    // Go through the symbol table and build the string table
    verbose("Creating string table...\n");
    for (int i=0; i < stbds_arrlen(symtable); i++) {
        if (i % 5000 == 0)
            verbose("  %d/%d\n", i, stbds_arrlen(symtable));
        struct symtable_s *sym = &symtable[i];
        if (sym->func)
            sym->func_sidx = stringtable_add(sym->func);
        else
            sym->func_sidx = -1;
        if (sym->file)
            sym->file_sidx = stringtable_add(sym->file);
        else
            sym->file_sidx = -1;
    }

    // Sort the symbol table by address
    qsort(symtable, stbds_arrlen(symtable), sizeof(struct symtable_s), symtable_sort_by_addr);

    // Fill in the function offset field in the entries in the symbol table.
    verbose("Computing function offsets...\n");
    compute_function_offsets();

    // Write the symbol table to file
    verbose("Writing %s\n", outfn);
    FILE *out = fopen(outfn, "wb");
    if (!out) {
        fprintf(stderr, "Cannot create file: symtable.bin\n");
        exit(1);
    }

    // Write header. See symtable_header_t in backtrace.c for the layout.
    fwrite("SYMT", 4, 1, out);
    w32(out, 2); // Version
    int addrtable_off = w32_placeholder(out);
    w32(out, stbds_arrlen(symtable));
    int symtable_off = w32_placeholder(out);
    w32(out, stbds_arrlen(symtable));
    int stringtable_off = w32_placeholder(out);
    w32(out, stbds_arrlen(stringtable));

    // Write address table. This is a sequence of 32-bit addresses.
    walign(out, 16);
    w32_at(out, addrtable_off, ftell(out));
    for (int i=0; i < stbds_arrlen(symtable); i++) {
        struct symtable_s *sym = &symtable[i];
        w32(out, sym->addr | (sym->is_func ? 0x1 : 0) | (sym->is_inline ? 0x2 : 0));
    }

    // Write symbol table. See symtable_entry_t in backtrace.c for the layout.
    walign(out, 16);
    w32_at(out, symtable_off, ftell(out));
    for (int i=0; i < stbds_arrlen(symtable); i++) {
        struct symtable_s *sym = &symtable[i];
        w32(out, sym->func_sidx);
        w32(out, sym->file_sidx);
        w16(out, strlen(sym->func));
        w16(out, strlen(sym->file));
        w16(out, (uint16_t)(sym->line < 65536 ? sym->line : 0));
        w16(out, (uint16_t)(sym->func_offset < 0x10000 ? sym->func_offset : 0));
    }

    walign(out, 16);
    w32_at(out, stringtable_off, ftell(out));
    fwrite(stringtable, stbds_arrlen(stringtable), 1, out);
    fclose(out);
}

// Change filename extension
char *change_ext(const char *fn, const char *ext)
{
    char *out = strdup(fn);
    char *dot = strrchr(out, '.');
    if (dot) *dot = 0;
    strcat(out, ext);
    return out;
}

int main(int argc, char *argv[])
{
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
    n64_inst = n64_toolchain_dir();
    if (!n64_inst) {
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

