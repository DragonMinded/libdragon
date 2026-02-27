/*
    mkmaterial: convert a MAT INI/JSON file into a binary material database
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/

#include <deque>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cstdint>
#include <cstdio>
#include <utility>
#include "combexpr.cpp"

extern char *flag_texdb_path;
extern const char *flag_output_path;
extern int flag_compress;
extern int flag_verbose;
extern const char *n64_inst;
extern std::deque<std::string> texture_dirs;

void verbose(const char *fmt, ...);

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
    std::string fmt{"AUTO"};
    std::string mipmap{"NONE"};
    std::string dithering{"NONE"};
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
    MyEnum mode{0, {"none", "multiply", "multiply_const", "additive"}};
    float constant{-1};

    void parse_attr(std::string key, std::string value);
    void validate(void);
};

struct Extension {
    MyEnum type{0, {"bool", "int", "string", "float"}};
    std::string name;
    std::string value;

    uint32_t hash{0};
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
    std::vector<Extension> ext; // extension attributes

    Material() = default;

    void parse_attr(std::string key, std::string value);
    void validate(void);
    void write(FILE *f);
};

// mkmaterial_parse.cpp
bool parse_bool(std::string value);
float parse_float(std::string value, float min, float max);
int parse_int(std::string value, int min, int max);
std::string parse_enum(std::string value, const std::vector<std::string> &enums);
std::vector<Material> parse_mat(const char *fn);
std::vector<Material> parse_jmat(const char *fn);

// mkmaterial_export.cpp
void mat_convert(Material &mat);
void mat_writedb(FILE *f, std::vector<Material> &materials);
