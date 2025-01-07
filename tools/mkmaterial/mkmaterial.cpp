#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <utility>
#include <map>
#include <set>
#include <deque>
#include "../common/assetcomp.h"
#include "../common/utils.h"
#include "../common/subprocess.h"
#include "../common/binout.h"
#include "../common/binout.c"
#include "../../include/rdpq_macros.h"
#include "../../src/rdpq/rdpq_mat_internal.h"
#include "combexpr.cpp"

int flag_compress = DEFAULT_COMPRESSION;
int flag_verbose = 0;
const char *n64_inst = NULL;
std::deque<std::string> texture_dirs;

struct MyEnum {
    std::vector<std::string> values;
    int idx{-1};

    MyEnum(std::vector<std::string> values_) : values(std::move(values_)) {}
    MyEnum(int def, std::vector<std::string> values_) : values(std::move(values_)), idx(def) {
        if (idx < 0 || idx >= (int)values.size())
            throw std::runtime_error("invalid enum value: " + std::to_string(idx));        
    }
    operator int() const { return idx; }
    std::string to_str() const { return values[idx]; }

    void operator=(int i) {
        if (i < 0 || i >= (int)values.size())
            throw std::runtime_error("invalid enum value: " + std::to_string(i));
        idx = i;
    }

    void operator=(std::string value) {
        for (size_t i = 0; i < values.size(); i++) {
            if (value == values[i]) {
                idx = i;
                return;
            }
        }

        std::string error = "invalid value: " + value + "; expected one of: ";
        for (size_t i = 0; i < values.size(); i++) {
            error += values[i];
            if (i < values.size() - 1) error += ", ";
        }
        throw std::runtime_error(error);
    }
};

struct Texture {
    std::string name{""};
    std::string fmt{"auto"};
    std::string mipmap{"none"};
    std::string dithering{"none"};
    struct {
        float translate{0};
        int scale{1};
        float repeats{2048};
        bool mirror{false};
    } s, t;

    operator bool() const { return !name.empty(); }
    void parse_attr(std::string key, std::string value);
    void validate_name(void);

    std::vector<uint8_t> sprite;
};

struct RenderModes {
    MyEnum antialias{{"none", "standard", "reduced"}};
    MyEnum fog{{"none", "standard"}};
    MyEnum dither[2]{ {{"none", "noise", "bayer", "square"}}, {{"none", "noise", "bayer", "square", "invbayer", "invsquare"}} };
    MyEnum filtering{{"point", "bilinear", "median"}};
    int perspective{-1};
    int alpha_compare{-1};
    MyEnum zmode{{"none", "compare", "update", "compare+update"}};
    int z_override{-1};
    int deltaz_override{0};

    void parse_attr(std::string key, std::string value);
};

struct Combiner {
    combexpr::CombinerExpr rgb{combexpr::CombinerChannel::RGB, "0", "0", "0", "tex0"};
    combexpr::CombinerExpr alpha{combexpr::CombinerChannel::ALPHA, "0", "0", "0", "tex0"};
    combexpr::CombinerExprFull full;

    void parse_attr(std::string key, std::string value);
    uint64_t to_rdpq_mode_arg(void);
};

struct Blender {
    MyEnum mode{0, {"off", "multiply", "multiply_const", "additive"}};
    float constant{-1};

    void parse_attr(std::string key, std::string value);
    void validate(void);
};

struct Material {
    std::string name;
    struct {
        std::string filename;
        int lineno{0};
    } parse_info;
    Texture tex[2];
    RenderModes rm;
    Combiner cc;
    Blender bl;

    Material() = default;

    void parse_attr(std::string key, std::string value);
    void validate(void);
    void write(FILE *f);
};

void verbose(const char *fmt, ...)
{
    if (!flag_verbose) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}


#include "mkmaterial_parse.cpp"
#include "mkmaterial_export.cpp"


void usage(void)
{
    fprintf(stderr, "Usage: mkmaterial [flags] <file.mat>...\n\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "  -v, --verbose            verbose output\n");
    fprintf(stderr, "  -h, --help               print this help message\n");
    fprintf(stderr, "  -I, --include [path]     specify additional texture path\n");
    fprintf(stderr, "  -o, --output [file]      specify output file (default: materials.mdb)\n");
    fprintf(stderr, "  -c. --compress [level]   specify compression level (default: %d)\n", DEFAULT_COMPRESSION);
}

int main(int argc, char *argv[])
{
    std::map<std::string, Material> materials;
    bool error = false;
    const char *output_file = "materials.mdb";
    int nfiles = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
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
                output_file = argv[i];
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
            } else {
                fprintf(stderr, "error: unknown option: %s\n", argv[i]);
                return 1;
            }
        } else {
            ++nfiles;
            if (parse_mat(argv[i], [&](Material &&mat) {
                verbose("parsed material: %s\n", mat.name.c_str());

                if (materials.find(mat.name) != materials.end()) {
                    fprintf(stderr, "%s:%d: duplicate material: %s (previous occurrence: %s:%d)\n", mat.parse_info.filename.c_str(), mat.parse_info.lineno, mat.name.c_str(), 
                        materials[mat.name].parse_info.filename.c_str(), materials[mat.name].parse_info.lineno);
                    error = true;
                    return;
                }

                materials[mat.name] = std::move(mat);
            }) != 0) {
                error = true;
            }
        }
    }

    if (nfiles == 0) {
        usage();
        return 1;
    }

    if (error) {
        // If at least one material contained syntax errors, do not get on
        // with conversion.
        return 1;
    }

    // Find n64 tool directory
    if (!n64_inst) {
        n64_inst = n64_tools_dir();
        if (!n64_inst) {
            fprintf(stderr, "Error: N64_INST environment variable not set\n");
            return 1;
        }
    }

    for (auto& [name, mat] : materials) {
        verbose("converting material: %s\n", name.c_str());
        mat_convert(mat);
    }

    // Write the material database
    FILE *f = fopen(output_file, "wb");
    if (!f) {
        fprintf(stderr, "Error: cannot open output file: %s\n", output_file);
        return 1;
    }

    mat_writedb(f, materials);
    fclose(f);

    return !error ? 0 : 1;
}
