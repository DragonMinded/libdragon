#include <regex>

#define INI_HANDLER_LINENO 1
#define INI_INLINE_COMMENT_PREFIXES ";#"
#define INI_CALL_HANDLER_ON_NEW_SECTION 1
#include "ini.h"
#include "ini.c"

static bool parse_bool(std::string value)
{
    if (value == "true" || value == "1" || value == "True") return true;
    if (value == "false" || value == "0" || value == "False") return false;
    throw std::runtime_error("invalid boolean value: " + value);
}

static float parse_float(std::string value, float min, float max)
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

static int parse_int(std::string value, int min, int max)
{
    try {
        size_t idx;
        int ival = std::stoi(value, &idx);
        if (idx != value.size())
            throw std::runtime_error("invalid integer value: " + value);
        if (ival < min || ival > max)
            throw std::runtime_error("integer value out of range: allowed " + std::to_string(min) + "-" + std::to_string(max));
        return ival;
    } catch (std::invalid_argument &e) {
        throw std::runtime_error("invalid integer value: " + value);
    }
}

static std::string parse_enum(std::string value, const std::vector<std::string> &enums)
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

void Combiner::parse_attr(std::string key, std::string value)
{
    static const std::regex expr(R"(\s*\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*\)(?:\s*,\s*\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*\))?)");

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
            if (match.length() > 5) {
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
            if (match.length() > 5) {
                alpha.set(1, 'a', match[5]); alpha.set(1, 'b', match[6]); alpha.set(1, 'c', match[7]); alpha.set(1, 'd', match[8]);
            }
            full = combexpr::CombinerExprFull(rgb, alpha);
        } else {
            throw std::runtime_error("invalid alpha.raw combiner expression: must be in format \"(a,b,c,d)\"");
        }
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
        mipmap = parse_enum(value, {"none", "box"});
    } else if (key == "dithering") {
        dithering = value;
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

int parse_mat(const char *fn, std::function<void(Material &&)> cb)
{
    Material mat;
    bool mat_error = false;

    auto finish_material = [&]() -> int {
        if (!mat.name.empty() && !mat_error) {
            try {
                mat.validate();
            } catch (std::runtime_error &e) {
                fprintf(stderr, "%s: error: %s (material: %s)\n", fn, e.what(), mat.name.c_str());
                return 1;
            }
            
            cb(std::move(mat));
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

    if (!finish_material()) {
        err = 1;
    }

    if (err < 0) {
        fprintf(stderr, "error: file not found: %s\n", fn);
    }

    texture_dirs.pop_front();
    return err;
}
