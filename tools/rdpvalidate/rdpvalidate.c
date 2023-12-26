#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "rdpq_debug.h"
#include "../../src/rdpq/rdpq_debug_internal.h"
#include "../../src/rdpq/rdpq_debug.c"

void usage(void) {
    printf("rdpvalidate -- RDP validation tool\n");
    printf("\n");
    printf("This tool disassembles and validates a sequence of RDP commands provided in binary or hex format.\n");
    printf("Validation is accurate only if the sequence of commands is complete; partial sequences might\n");
    printf("have spurious warnings or errors.\n");
    printf("\n");
    printf("Usage:\n");
    printf("   rdpvalidate [flags] <file>\n");
    printf("\n");
    printf("Options:\n");
    printf("   -H / --hex            File is ASCII in hex format. Default is autodetect.\n");
    printf("   -B / --binary         File is binary. Default is autodetect.\n");
    printf("   -d / --disassemble    Disassemble the file (default is off, just validate).\n");
    printf("   -t / --triangles      When disassembling, also show all triangles in the output.\n");
    printf("\n");
    printf("Hex format is an ASCII file: one line per RDP command, written in hexadecimal format.\n");
    printf("Lines starting with '#' are skipped.\n");
    printf("Binary format is a raw sequence of 8-bytes RDP commands.\n");
}

void arr_append(uint64_t **buf, int *size, int *cap, uint64_t val)
{
    if (*size == *cap) {
        *cap *= 2;
        if (!*cap) *cap = 128;
        *buf = (uint64_t*)realloc(*buf, *cap * sizeof(uint64_t));
    }
    (*buf)[*size] = val;
    *size += 1;
}

bool detect_ascii(FILE *f) {
    unsigned char buf[16];
    int n = fread(buf, 1, 16, f);
    for (int i=0;i<n;i++) {
        if (!isprint(buf[i]) && buf[i] != '\r' && buf[i] != '\n')
            return false;
    }
    return true;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage();
        return 1;
    }

    enum { MODE_BINARY=0, MODE_HEX=1, MODE_AUTODETECT=-1 };

    bool disasm = false;
    bool show_tris = false;
    int mode = MODE_AUTODETECT;
    int i;
    for (i=1; i<argc; i++) {
        if (argv[i][0] == '-') {	
            if (!strcmp(argv[i], "-H") || !strcmp(argv[i], "--hex"))  {
                mode = MODE_HEX;
            } else if (!strcmp(argv[i], "-B") || !strcmp(argv[i], "--binary")) {
                mode = MODE_BINARY;
            } else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--disassemble")) {
                disasm = true;
            } else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--triangles")) {
                show_tris = true;
            } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                usage();
                return 0;
            } else {
                fprintf(stderr, "ERROR: unknown option: %s", argv[i]);
                return 1;
            }
        } else {
            break;
        }		
    }

    if (i == argc) {
        fprintf(stderr, "ERROR: missing filename to process\n");
        return 1;
    }

    FILE *f = fopen(argv[i], "rb");
    if (!f) {
        fprintf(stderr, "ERROR: cannot open file: %s\n", argv[i]);
        return 1;
    }

    if (mode == MODE_AUTODETECT) {
        mode = detect_ascii(f) ? MODE_HEX : MODE_BINARY;
        rewind(f);
    }

    uint64_t *cmds = NULL; int size=0,cap=0;

    int num_line = 0;
    while (!feof(f)) {
        num_line++;
        uint64_t cmd;
        if (mode == MODE_HEX) {
            char line[256] = {0};
            if (!fgets(line, sizeof(line), f))
                break;
            if (line[0] == '#') continue;
            char *end;
            cmd = strtoull(line, &end, 16);
            while (*end == ' ' || *end == '\t') end++;
            if (*end != '\n' && *end != '\r' && *end != '#')
                fprintf(stderr, "WARNING: ignored spurious characters on line %d\n", num_line);
        } else {
            if (!fread(&cmd, 8, 1, f))
                break;
        }
        
        arr_append(&cmds, &size, &cap, cmd);
    }

    // Enable dump of all triangles
    if (show_tris)
        __rdpq_debug_log_flags |= RDPQ_LOG_FLAG_SHOWTRIS;

    uint64_t *cur = cmds;
    uint64_t *end = cmds + size;
    while (cur < end) {
        int sz = rdpq_debug_disasm_size(cur);

        bool shown = false;
        if (disasm)
            shown = rdpq_debug_disasm(cur, stderr);

        uint32_t val_flags = shown ? RDPQ_VALIDATE_FLAG_NOECHO : 0;
        rdpq_validate(cur, val_flags, NULL, NULL);
        
        cur += sz;
    }
}
