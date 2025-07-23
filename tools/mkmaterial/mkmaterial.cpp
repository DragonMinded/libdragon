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

const char *flag_texdb_path = "texdb";
const char *flag_output_path = ".";
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

    uint32_t hash{0};
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
    fprintf(stderr, "  -o, --output [path]      specify output path (default: .)\n");
    fprintf(stderr, "  -c. --compress [level]   specify compression level for textures (default: %d)\n", DEFAULT_COMPRESSION);
}

int main(int argc, char *argv[])
{
    std::map<std::string, Material> materials;
    bool error = false;
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
                flag_output_path = argv[i];
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
            if (strstr(argv[i], ".jmat")) {
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

            for (auto& mat : materials) {
                verbose("converting material: %s\n", mat.name.c_str());
                mat_convert(mat);
            }

            // Open output file, named after the filename
            char *out = change_ext(argv[i], ".mdb");
            verbose("writing material database: %s\n", out);
            FILE *f = fopen(out, "wb");
            free(out);
            if (!f) {
                fprintf(stderr, "error: cannot open output file: %s\n", out);
                return 1;
            }

            // Write the material database
            mat_writedb(f, materials);
            fclose(f);
        }
    }

    if (nfiles == 0) {
        usage();
        return 1;
    }

    return !error ? 0 : 1;
}
