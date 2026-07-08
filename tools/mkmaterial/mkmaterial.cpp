/*
    mkmaterial: convert a MAT INI/JSON file into a binary material database
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdarg.h>
#include "mkmaterial.h"
#include "../common/assetcomp.h"
#include "../common/utils.h"
#include "../common/polyfill.h"

char *flag_texdb_path = NULL;
const char *flag_output_path = ".";
int flag_compress = DEFAULT_COMPRESSION;
int flag_verbose = 0;
const char *n64_inst = NULL;
std::deque<std::string> texture_dirs;

void verbose(const char *fmt, ...)
{
    if (!flag_verbose) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void usage(void)
{
    fprintf(stderr, "Usage: mkmaterial [flags] <file.mat>...\n\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "  -v, --verbose            verbose output\n");
    fprintf(stderr, "  -h, --help               print this help message\n");
    fprintf(stderr, "  -I, --include [path]     specify additional texture path\n");
    fprintf(stderr, "  -o, --output [path]      specify output path (default: .)\n");
    fprintf(stderr, "  -t, --texdb [path]       specify texture database path (default: {output}/texdb)\n");
    fprintf(stderr, "  -c. --compress [level]   specify compression level for textures (default: %d)\n", DEFAULT_COMPRESSION);
    fprintf(stderr, "  --raw-material           generate a single raw headerless material instead of a database\n");
}

int main(int argc, char *argv[])
{
    winconsole_utf8();
    std::map<std::string, Material> materials;
    bool error = false;
    int nfiles = 0;
    bool raw_material = false;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
                flag_verbose++;
                continue;
            } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                usage();
                return 0;
            } else if (!strcmp(argv[i], "-I") || !strcmp(argv[i], "--include")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                texture_dirs.push_back(argv[i++]);
            } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                flag_output_path = argv[i];
            } else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--texdb")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                flag_texdb_path = argv[i];
            } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--compress")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                flag_compress = atoi(argv[i]);
                if (flag_compress < 0 || flag_compress > MAX_COMPRESSION) {
                    fprintf(stderr, "invalid compression level: %d\n", flag_compress);
                    return 1;
                }
            } else if (!strcmp(argv[i], "--raw-material")) {
                raw_material = true;
            } else {
                fprintf(stderr, "error: unknown option: %s\n", argv[i]);
                return 1;
            }
        } else {         
            nfiles++;   

            // Find n64 tool directory
            if (!n64_inst) {
                n64_inst = n64_tools_dir();
                if (!n64_inst) {
                    fprintf(stderr, "Error: N64_INST environment variable not set\n");
                    return 1;
                }
            }

            std::vector<Material> materials;
            if (strstr(argv[i], ".jmat") || strcmp(argv[i], "-") == 0) {
                materials = parse_jmat(argv[i]);
            } else if (strstr(argv[i], ".mat")) {
                materials = parse_mat(argv[i]);
            } else {
                fprintf(stderr, "error: unknown file type: %s\n", argv[i]);
                continue;
            }

            if (materials.empty()) {
                error = true;
                continue;
            }

            if (raw_material && materials.size() > 1) {
                fprintf(stderr, "error: cannot generate raw material when input contains multiple materials\n");
                error = true;
                continue;
            }

            // Open output file, named after the filename. Use stdout
            // if the input file is stdin.
            FILE *f; bool is_stdout = false;
            char *out = nullptr;
            
            if (strcmp(argv[i], "-") == 0) {
                // Write to stdout when input is stdin
                is_stdout = true;
                f = tmpfile();
                if (!f) {
                    fprintf(stderr, "error: cannot open temporary file\n");
                    return 1;
                }
                verbose("writing material database to stdout\n");

                if (!flag_texdb_path) {
                    fprintf(stderr, "error: cannot write to stdout without a texture database path\n");
                    return 1;
                }
            } else {
                char *infn = argv[i];
                char *basename = strrchr(infn, '/');
                if (!basename) basename = infn; else basename += 1;
                char* basename_noext = strdup(basename);
                char* ext = strrchr(basename_noext, '.');
                if (ext) *ext = '\0';

                if (raw_material) {
                    asprintf(&out, "%s/%s.mraw", flag_output_path, basename_noext);
                    verbose("writing raw material: %s\n", out);
                } else {
                    asprintf(&out, "%s/%s.mdb", flag_output_path, basename_noext);
                    verbose("writing material: %s\n", out);
                }

                // If the texdb path is not set, use the default
                if (!flag_texdb_path)
                    asprintf(&flag_texdb_path, "%s/texdb", flag_output_path);
                if (mkdir(flag_texdb_path, 0755) && errno != EEXIST) {
                    fprintf(stderr, "error: cannot create texture database directory: %s\n", flag_texdb_path);
                    return 1;
                }

                verbose("texture DB: %s\n", flag_texdb_path);

                f = fopen(out, "wb");
                if (!f) {
                    fprintf(stderr, "error: cannot open output file: %s\n", out);
                    free(out);
                    return 1;
                }
            }

            for (auto& mat : materials) {
                verbose("converting material: %s\n", mat.name.c_str());
                mat_convert(mat);
            }

            // Write the material database
            if (raw_material) {
                materials[0].write(f);
            } else {
                mat_writedb(f, materials);
            }
            
            if (is_stdout) {
                int sz;
                uint8_t *buf = slurp_fp(f, &sz);
                if (!buf || fwrite(buf, 1, sz, stdout) != (size_t)sz) {
                    free(buf);
                    fprintf(stderr, "error: cannot write to stdout\n");
                    return 1;
                }
                free(buf);
            }
            fclose(f);
            if (out) {
                free(out);
            }
        }
    }

    if (nfiles == 0) {
        usage();
        return 1;
    }

    return !error ? 0 : 1;
}
