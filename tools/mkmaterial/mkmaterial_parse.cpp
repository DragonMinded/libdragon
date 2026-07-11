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
#include "mkmaterial.h"
#include <regex>
#include <fstream>
#include <float.h>
#include "../common/json.hpp"
#include "../common/utils.h"

#define INI_HANDLER_LINENO 1
#define INI_INLINE_COMMENT_PREFIXES ";#"
#define INI_CALL_HANDLER_ON_NEW_SECTION 1
#include "ini.h"
#include "ini.c"

static uint32_t string_hash(const std::string &str)
{
    uint32_t hash = 5381;
    for (char c : str) {
        hash = hash * 33 + c;
    }
    return hash;
}

bool parse_bool(std::string value)
{
    if (value == "true" || value == "1" || value == "True") return true;
    if (value == "false" || value == "0" || value == "False") return false;
    throw std::runtime_error("invalid boolean value: " + value);
}

float parse_float(std::string value, float min, float max)
{
    try {
        size_t idx;
        float fval = std::stof(value, &idx);
        if (idx != value.size())
            throw std::runtime_error("invalid float value: " + value);
        if (fval < min || fval > max)
            throw std::runtime_error("float value out of range: allowed " + std::to_string(min) + "-" + std::to_string(max));
        return fval;
    } catch (std::invalid_argument &e) {
        throw std::runtime_error("invalid float value: " + value);
    }
}

int parse_int(std::string value, int min, int max)
{
    try {
        size_t idx; int ival;
        if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
            ival = std::stoi(value, &idx, 16);
        } else {
            ival = std::stoi(value, &idx);
        }
        if (idx != value.size())
            throw std::runtime_error("invalid integer value: " + value);
        if (ival < min || ival > max)
            throw std::runtime_error("integer value out of range: allowed " + std::to_string(min) + "-" + std::to_string(max));
        return ival;
    } catch (std::invalid_argument &e) {
        throw std::runtime_error("invalid integer value: " + value);
    }
}

std::vector<std::string> split_string(std::string str, char delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delimiter, start);
    }
    tokens.push_back(str.substr(start));
    return tokens;
}

Vec4 parse_color(std::string value)
{
    std::vector<std::string> tokens = split_string(value, ',');
    if (tokens.size() != 3 && tokens.size() != 4) {
        throw std::runtime_error("invalid color value: " + value);
    }
    Vec4 color{};
    for (size_t i = 0; i < tokens.size(); i++) {
        color[i] = parse_float(tokens[i], 0, 1);
    }
    if (tokens.size() < 4) {
        color[3] = 1;
    }
    return color;
}

std::string parse_enum(std::string value, const std::vector<std::string> &enums)
{
    for (size_t i = 0; i < enums.size(); i++) {
        if (value == enums[i]) return value;
    }

    std::string error = "invalid value: " + value + "; expected one of: ";
    for (size_t i = 0; i < enums.size(); i++) {
        error += enums[i];
        if (i < enums.size() - 1) error += ", ";
    }
    throw std::runtime_error(error);
}

void CombinerRegister::parse_float(std::string value)
{
    float v = ::parse_float(value, 0, 1);
    this->value = {v, v, v, v};
    is_set = true;
}

void CombinerRegister::parse_color(std::string value)
{
    this->value = ::parse_color(value);
    is_set = true;
}

void Combiner::parse_attr(std::string key, std::string value)
{
    static const std::regex expr(R"(\s*\(\s*([\w.]+)\s*,\s*([\w.]+)\s*,\s*([\w.]+)\s*,\s*([\w.]+)\s*\)(?:\s*,\s*\(\s*([\w.]+)\s*,\s*([\w.]+)\s*,\s*([\w.]+)\s*,\s*([\w.]+)\s*\))?)");

    if (key == "rgb") {
        combexpr::Parser parser(value);
        auto root = parser.parseExpression();
        combexpr::Matcher matcher(root);
    
        rgb = matcher.matchCombiner(combexpr::RGB);
        full = combexpr::CombinerExprFull(rgb, alpha);
    } else if (key == "alpha") {
        combexpr::Parser parser(value);
        auto root = parser.parseExpression();
        combexpr::Matcher matcher(root);
    
        alpha = matcher.matchCombiner(combexpr::ALPHA);
        full = combexpr::CombinerExprFull(rgb, alpha);
    } else if (key == "rgb.raw") {
        // Match regex for raw combiner expression: (a,b,c,d), optionally repeated for the second step
        std::smatch match;
        if (std::regex_match(value, match, expr)) {
            rgb = combexpr::CombinerExpr(combexpr::CombinerChannel::RGB, match[1], match[2], match[3], match[4]);
            if (match[5].matched) {
                rgb.set(1, 'a', match[5]); rgb.set(1, 'b', match[6]); rgb.set(1, 'c', match[7]); rgb.set(1, 'd', match[8]);
            }
            full = combexpr::CombinerExprFull(rgb, alpha);
        } else {
            throw std::runtime_error("invalid rgb.raw combiner expression: must be in format \"(a,b,c,d)\"");
        }
    } else if (key == "alpha.raw") {
        // Match regex for raw combiner expression: (a,b,c,d), optionally repeated for the second step
        std::smatch match;
        if (std::regex_match(value, match, expr)) {
            alpha = combexpr::CombinerExpr(combexpr::CombinerChannel::ALPHA, match[1], match[2], match[3], match[4]);
            if (match[5].matched) {
                alpha.set(1, 'a', match[5]); alpha.set(1, 'b', match[6]); alpha.set(1, 'c', match[7]); alpha.set(1, 'd', match[8]);
            }
            full = combexpr::CombinerExprFull(rgb, alpha);
        } else {
            throw std::runtime_error("invalid alpha.raw combiner expression: must be in format \"(a,b,c,d)\"");
        }
    } else if (key == "reg.k4") {
        registers[combexpr::internal::UNIFORM_K4].parse_float(value);
    } else if (key == "reg.k5") {
        registers[combexpr::internal::UNIFORM_K5].parse_float(value);
    } else if (key == "reg.prim_lod_frac") {
        registers[combexpr::internal::UNIFORM_PRIM_LOD_FRAC].parse_float(value);
    } else if (key == "reg.env") {
        registers[combexpr::internal::UNIFORM_ENV].parse_color(value);
    } else if (key == "reg.prim") {
        registers[combexpr::internal::UNIFORM_PRIM].parse_color(value);
    } else {
        throw std::runtime_error("Unknown combiner key: " + key);
    }
}

uint64_t Combiner::to_rdpq_mode_arg(void)
{
    return full.rdp_command() & ~(0x3Full << 56);
}

void Blender::parse_attr(std::string key, std::string value)
{
    if (key == "mode") {
        mode = parse_enum(value, {"none", "multiply", "multiply_const", "additive"});
    } else if (key == "const") {
        constant = parse_float(value, 0, 1);
    } else {
        throw std::runtime_error("Unknown blender key: " + key);
    }
}

void Blender::validate(void)
{
    if (mode.to_str() == "multiply_const" && constant < 0)
        throw std::runtime_error("blender.const must be specified for mode multiply_const");
    if (mode.to_str() != "multiply_const" && constant >= 0)
        throw std::runtime_error("blender.const is only valid for mode multiply_const");
}

void Texture::validate_name(void)
{
    if (name.size() >= 256)
        throw std::runtime_error("material name too long: " + name);

    if (file_exists(name.c_str())) return;

    for (const std::string &dir : texture_dirs) {
        std::string path = dir + "/" + name;
        if (file_exists(path.c_str())) {
            name = path;
            return;
        }
    }

    throw std::runtime_error("texture file not found: " + name);
}

void Texture::parse_attr(std::string key, std::string value)
{
    if (key == "name") {
        name = value;
        validate_name();
    } else if (key == "fmt") {
        fmt = parse_enum(value, {"AUTO", "RGBA16", "RGBA32", "CI4", "CI8", "IA4", "IA8", "IA16", "I4", "I8", "SHQ", "IHQ"});
    } else if (key == "mipmap") {
        mipmap = parse_enum(value, {"NONE", "BOX"});
    } else if (key == "dithering") {
        dithering = parse_enum(value, {"NONE", "RANDOM", "ORDERED"});
    } else if (key == "s.translate") {
        s.translate = parse_float(value, -1024, 1024);
    } else if (key == "s.scale") {
        s.scale = parse_int(value, -5, 10);
    } else if (key == "s.repeats") {
        if (value == "inf" || value == "infinity")
            s.repeats = 2048;
        else
            s.repeats = parse_float(value, 0, 1024);
    } else if (key == "s.mirror") {
        s.mirror = parse_bool(value);
    } else if (key == "t.translate") {
        t.translate = parse_float(value, -1024, 1024);
    } else if (key == "t.scale") {
        t.scale = parse_int(value, -5, 10);
    } else if (key == "t.repeats") {
        if (value == "inf" || value == "infinity")
            t.repeats = 2048;
        else
            t.repeats = parse_float(value, 0, 1024);
    } else if (key == "t.mirror") {
        t.mirror = parse_bool(value);
    } else {
        throw std::runtime_error("Unknown texture key: " + key + "; expected one of: "
        "name, fmt, mipmap, dithering, s.translate, s.scale, s.repeats, s.mirror, t.translate, t.scale, t.repeats, t.mirror");
    }
}

void RenderModes::parse_attr(std::string key, std::string value)
{
    if (key == "antialias") {
        antialias = value;
    } else if (key == "fog") {
        fog = value;
    } else if (key == "dither.rgb") {
        dither[0] = value;
    } else if (key == "dither.alpha") {
        dither[1] = value;
    } else if (key == "filtering") {
        filtering = value;
    } else if (key == "perspective") {
        perspective = parse_bool(value);
    } else if (key == "alpha_compare") {
        alpha_compare = parse_int(value, 0, 255);
    } else if (key == "zmode") {
        zmode = value;
    } else if (key == "z_override") {
        z_override = parse_int(value, 0, 0x7fff);
    } else if (key == "deltaz_override") {
        int dz = parse_int(value, -32768, 32767);
        if ((dz & -dz) != (dz >= 0 ? dz : -dz))
            throw std::runtime_error("deltaz_override must be a power of 2");
        deltaz_override = dz;
    } else {
        throw std::runtime_error("Unknown render mode key: " + key + "; expected one of: "
        "antialias, fog, dither.rgb, dither.alpha, filtering, perspective, alpha_compare, zcmp, zupd, z_override, deltaz_override");
    }
}

void Material::parse_attr(std::string key, std::string value)
{
    if (key.rfind("tex0.", 0) == 0) {
        tex[0].parse_attr(key.substr(5), value);
    } else if (key.rfind("tex1.", 0) == 0) {
        tex[1].parse_attr(key.substr(5), value);
    } else if (key.rfind("rm.", 0) == 0) {
        rm.parse_attr(key.substr(3), value);
    } else if (key.rfind("combiner.", 0) == 0) {
        cc.parse_attr(key.substr(9), value);
    } else if (key.rfind("blender.", 0) == 0) {
        bl.parse_attr(key.substr(8), value);
    } else if (key.rfind("ext.", 0) == 0) {
        Extension ext_attr;
        ext_attr.name = key.substr(4);
        if (value == "true" || value == "false") {
            ext_attr.type = {"bool"};
            ext_attr.value = value;
        } else if (std::regex_match(value, std::regex("^-?\\d+$")) || std::regex_match(value, std::regex("^0[xX][0-9a-fA-F]+$"))) {
            ext_attr.type = {"int"};
            ext_attr.value = value;
        } else if (std::regex_match(value, std::regex("^-?\\d*\\.?\\d*$"))) {
            ext_attr.type = {"float"};
            ext_attr.value = value;
        } else {
            ext_attr.type = {"string"};
            ext_attr.value = value;
        }
        ext_attr.hash = string_hash(ext_attr.name) & 0xFFFF;

        // Check for duplicate extension
        for (const auto &existing : ext) {
            if (existing.name == ext_attr.name) {
                fprintf(stderr, "%s:%d: duplicate extension attribute: %s\n", 
                        parse_info.filename.c_str(), parse_info.lineno, ext_attr.name.c_str());
                throw std::runtime_error("Duplicate extension attribute: " + ext_attr.name);
            }
        }
        ext.push_back(ext_attr);
    } else {
        throw std::runtime_error("Unknown material key: " + key);
    }
}

void Material::validate(void)
{
    bl.validate();
}

std::string dirname(std::string path)
{
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}   

std::vector<Material> parse_mat(const char *fn)
{
    Material mat;
    bool mat_error = false;
    std::vector<Material> materials;

    auto finish_material = [&]() -> int {
        if (!mat.name.empty() && !mat_error) {
            try {
                mat.validate();
            } catch (std::runtime_error &e) {
                fprintf(stderr, "%s: error: %s (material: %s)\n", fn, e.what(), mat.name.c_str());
                return 1;
            }

            // Check if the material already exists
            for (const auto &existing : materials) {
                if (existing.name == mat.name) {
                    fprintf(stderr, "%s:%d: duplicate material: %s (previous occurrence: %s:%d)\n", 
                            mat.parse_info.filename.c_str(), mat.parse_info.lineno, 
                            mat.name.c_str(), existing.parse_info.filename.c_str(), existing.parse_info.lineno);
                    return 1;
                }
            }

            verbose("%s: parsed material: %s\n", fn, mat.name.c_str());
            materials.push_back(std::move(mat));
        }
        return 0;
    };

    texture_dirs.push_front(dirname(fn));

    int err = ini_parse(fn, [&](void *user, const char *section, const char *key, const char *value, int lineno) {
        if (!key) {
            if (finish_material() != 0) {
                mat_error = true;
                return 0;
            }
            // Initialize new material
            mat = {};
            mat.name = section;
            mat.parse_info.filename = fn;
            mat.parse_info.lineno = lineno;
            mat_error = false;
            return 1;
        }
        try {
            mat.parse_attr(key, value);
            return 1;
        } catch (std::runtime_error &e) {
            fprintf(stderr, "%s:%d: error: %s (material: %s)\n", fn, lineno, e.what(), mat.name.c_str());
            mat_error = true;
            return 0;
        }
    }, nullptr);

    if (err < 0) {
        fprintf(stderr, "error: file not found: %s\n", fn);
        return {};
    }

    if (finish_material() != 0) {
        return {};
    }

    if (materials.empty()) {
        fprintf(stderr, "%s: error: no valid materials found\n", fn);
        return {};
    }

    texture_dirs.pop_front();
    return materials;
}

std::vector<Material> parse_jmat(const char *fn)
{
    using json = nlohmann::json;
    std::vector<Material> materials;
    
    json j;
    try {
        if (strcmp(fn, "-") == 0) {
            // Read from stdin
            fn = "<stdin>";
            std::cin >> j;
        } else {
            // Read JSON file
            std::ifstream file(fn);
            if (!file.is_open()) {
                fprintf(stderr, "error: file not found: %s\n", fn);
                return {};
            }
            file >> j;
        }
    } catch (const json::exception& e) {
        fprintf(stderr, "%s: error: JSON parsing error: %s\n", fn, e.what());
        return {};
    }
    
    if (!j.is_object()) {
        fprintf(stderr, "%s: error: JSON root must be an object\n", fn);
        return {};
    }
    
    // Use current directory if reading from stdin, otherwise use dirname of the file
    if (strcmp(fn, "<stdin>") == 0) {
        texture_dirs.push_front(".");
    } else {
        texture_dirs.push_front(dirname(fn));
    }
    
    // Iterate through each material in the JSON
    for (auto& [material_name, material_obj] : j.items()) {
        if (!material_obj.is_object()) {
            fprintf(stderr, "%s: error: material '%s' must be an object\n", fn, material_name.c_str());
            continue;
        }
        
        Material mat = {};
        mat.name = material_name;
        mat.parse_info.filename = fn;
        mat.parse_info.lineno = 0; // Line numbers not available for JSON format
        
        bool mat_error = false;
        
        // Parse all key-value pairs for this material
        for (auto& [key, value] : material_obj.items()) {
            if (!value.is_string()) {
                fprintf(stderr, "%s: error: value for key '%s' must be a string (material: %s)\n", 
                       fn, key.c_str(), material_name.c_str());
                mat_error = true;
                continue;
            }
            
            try {
                mat.parse_attr(key, value.get<std::string>());
            } catch (std::runtime_error &e) {
                fprintf(stderr, "%s: error: %s (material: %s, key: %s)\n", fn, e.what(), material_name.c_str(), key.c_str());
                mat_error = true;
            }
        }
        
        if (!mat_error) {
            try {
                mat.validate();
                
                // Check if the material already exists
                for (const auto &existing : materials) {
                    if (existing.name == mat.name) {
                        fprintf(stderr, "%s: duplicate material: %s (previous occurrence: %s)\n", 
                                fn, mat.name.c_str(), existing.parse_info.filename.c_str());
                        texture_dirs.pop_front();
                        return {};
                    }
                }
                
                verbose("%s: parsed material: %s\n", fn, mat.name.c_str());
                materials.push_back(std::move(mat));
            } catch (std::runtime_error &e) {
                fprintf(stderr, "%s: error: %s (material: %s)\n", fn, e.what(), material_name.c_str());
            }
        }
    }
    
    if (materials.empty()) {
        fprintf(stderr, "%s: error: no valid materials found\n", fn);
        texture_dirs.pop_front();
        return {};
    }
    
    texture_dirs.pop_front();
    return materials;
}
