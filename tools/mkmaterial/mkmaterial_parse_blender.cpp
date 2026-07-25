#include "mkmaterial.h"
#include <regex>
#include "../include/rdpq_macros.h"

static const std::vector<std::string>& get_slots(bool is_two_step, int step_idx, char slot)
{
    const static std::vector<std::string> p_q_values = {
        "in_rgb", "memory_rgb", "blend_rgb", "fog_rgb"
    };
    const static std::vector<std::string> p_q_2c_values = {
        "cycle1_rgb", "memory_rgb", "blend_rgb", "fog_rgb"
    };
    const static std::vector<std::string> a_values = {
        "in_alpha", "fog_alpha", "shade_alpha", "0"
    };
    const static std::vector<std::string> b_values = {
        "inv_mux_alpha", "memory_cvg", "1", "0"
    };
    const static std::vector<std::string> b_1c_values = {
        "inv_mux_alpha"
    };

    switch (slot) {
        case 'p':
            return is_two_step && step_idx == 1 ? p_q_2c_values : p_q_values;
        case 'a':
            return a_values;
        case 'q':
            return is_two_step && step_idx == 1 ? p_q_2c_values : p_q_values;
        case 'b':
            return is_two_step && step_idx == 0 ? b_1c_values : b_values;
        default:
            throw std::runtime_error("Invalid blender slot");
    }
}

static uint32_t get_slot_index(bool is_two_step, int step_idx, char slot, std::string value)
{
    const auto& slots = get_slots(is_two_step, step_idx, slot);

    for (int i = 0; i < slots.size(); i++) {
        if (slots[i] == value) {
            return i;
        }
    }

    throw std::runtime_error("invalid blender slot value: " + value);
}

static uint32_t get_slot_shift(char slot)
{
    switch (slot) {
        case 'p':
            return 28;
        case 'a':
            return 24;
        case 'q':
            return 20;
        case 'b':
            return 16;
        default:
            throw std::runtime_error("Invalid blender slot");
    }
}

static uint32_t get_index_shift(int step_idx, char slot)
{
    uint32_t shift = get_slot_shift(slot);
    if (step_idx == 0) shift += 2;
    return shift;
}

static uint32_t get_extra(std::string value)
{
    if (value == "memory_rgb" || value == "memory_cvg") {
        return SOM_READ_ENABLE;
    }
    return 0;
}

static uint32_t parse_slot(bool is_two_step, int step_idx, char slot, std::string value)
{
    uint32_t index = get_slot_index(is_two_step, step_idx, slot, value);

    if (is_two_step && step_idx == 0 && value == "memory_rgb") {
        throw std::runtime_error("'memory_rgb' cannot be used in the first pass of two-cycle mode!");
    }

    uint32_t result = index << get_index_shift(step_idx, slot);
    uint32_t extra = get_extra(value);

    return result | extra;
}

static uint32_t parse_step(bool is_two_step, int step_idx, std::string p, std::string a, std::string q, std::string b)
{
    return parse_slot(is_two_step, step_idx, 'p', p) |
            parse_slot(is_two_step, step_idx, 'a', a) |
            parse_slot(is_two_step, step_idx, 'q', q) |
            parse_slot(is_two_step, step_idx, 'b', b);
}

BlenderExpr parse_blender(std::string value)
{
    static const std::regex expr(R"(\s*\(\s*([\w.]+)\s*,\s*([\w.]+)\s*,\s*([\w.]+)\s*,\s*([\w.]+)\s*\)(?:\s*,\s*\(\s*([\w.]+)\s*,\s*([\w.]+)\s*,\s*([\w.]+)\s*,\s*([\w.]+)\s*\))?)");

    std::smatch match;
    if (std::regex_match(value, match, expr)) {
        auto is_two_step = match[5].matched;
        uint32_t result = parse_step(is_two_step, 0, match[1], match[2], match[3], match[4]);
        if (is_two_step) {
            result |= parse_step(true, 1, match[5], match[6], match[7], match[8]) | SOMX_BLEND_2PASS;
        } else {
            result |= parse_step(false, 1, match[1], match[2], match[3], match[4]);
        }
        return BlenderExpr(result);
    } else {
        throw std::runtime_error("invalid mode.raw blender expression: must be in format \"(p,a,q,b)\"");
    }
}
