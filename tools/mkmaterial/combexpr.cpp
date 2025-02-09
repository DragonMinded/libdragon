#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cctype>
#include <map>
#include <unordered_set>
#include <assert.h>

namespace combexpr {

static const std::vector<std::string> RGB_SLOTS_A = { "combined", "tex0", "tex1", "prim", "shade", "env", "1", "noise", "0" };
static const std::vector<std::string> RGB_SLOTS_B = { "combined", "tex0", "tex1", "prim", "shade", "env", "keycenter", "k4", "0" };
static const std::vector<std::string> RGB_SLOTS_C = { "combined", "tex0", "tex1", "prim", "shade", "env", "keyscale", "combined.a", "tex0.a", "tex1.a", "prim.a", "shade.a", "env.a", "lod_frac", "prim_lod_frac", "k5", "0" };
static const std::vector<std::string> RGB_SLOTS_D = { "combined", "tex0", "tex1", "prim", "shade", "env", "1", "0" };

static const std::vector<std::string> ALPHA_SLOTS_A = { "combined", "tex0", "tex1", "prim", "shade", "env", "1", "0" };
static const std::vector<std::string> ALPHA_SLOTS_B = { "combined", "tex0", "tex1", "prim", "shade", "env", "1", "0" };
static const std::vector<std::string> ALPHA_SLOTS_C = { "lod_frac", "tex0", "tex1", "prim", "shade", "env", "prim_lod_frac", "0" };
static const std::vector<std::string> ALPHA_SLOTS_D = { "combined", "tex0", "tex1", "prim", "shade", "env", "1", "0" };

static const std::unordered_set<std::string> ALL_SLOTS = { "combined", "tex0", "tex1", "shade", "prim", "env", "noise", "1", "0", "k4", "k5", "tex0.a", "tex1.a", "shade.a", "prim.a", "env.a", "lod_frac", "prim_lod_frac", "keycenter", "keyscale" };

enum CombinerChannel {
    RGB = 0,
    ALPHA = 1,
};

struct CombinerStep {
    CombinerChannel ch; int step_idx;
    std::string a, b, c, d;

    operator bool() const {
        return !a.empty();
    }

    std::string& slot(char slot) {
        switch (slot) {
        case 'a': return a;
        case 'b': return b;
        case 'c': return c;
        case 'd': return d;
        default: assert(0); throw std::runtime_error("invalid slot: " + std::string(slot, 1));
        }
    }

    const std::string& slot(char slot) const {
        switch (slot) {
        case 'a': return a;
        case 'b': return b;
        case 'c': return c;
        case 'd': return d;
        default: assert(0); throw std::runtime_error("invalid slot: " + std::string(slot, 1));
        }
    }

    std::string to_string() const {
        return "(" + a + "," + b + "," + c + "," + d + ")";
    }

    std::vector<int> slot_indices() const {
        return {
            slot_index('a', a), 
            slot_index('b', b),
            slot_index('c', c),
            slot_index('d', d),
        };
    }

    int slot_index(char slot, std::string value) const {
        const std::vector<std::string>* slots = nullptr;

        switch (ch) {
        case RGB: switch (slot) {
            case 'a': slots = &RGB_SLOTS_A; break;
            case 'b': slots = &RGB_SLOTS_B; break;
            case 'c': slots = &RGB_SLOTS_C; break;
            case 'd': slots = &RGB_SLOTS_D; break;
        } break;
        case ALPHA: switch (slot) {
            case 'a': slots = &ALPHA_SLOTS_A; break;
            case 'b': slots = &ALPHA_SLOTS_B; break;
            case 'c': slots = &ALPHA_SLOTS_C; break;
            case 'd': slots = &ALPHA_SLOTS_D; break;
        } break;
        }

        if (step_idx == 1 && value == "tex0") value = "tex1";
        else if (step_idx == 1 && value == "tex1") value = "tex0";
        if (step_idx == 1 && value == "tex0.a") value = "tex1.a";
        else if (step_idx == 1 && value == "tex1.a") value = "tex0.a";

        for (int i = 0; i < slots->size(); i++) {
            if ((*slots)[i] == value) {
                return i;
            }
        }
        return -1;
    };
};

uint64_t to_rdp_command(bool two_steps, int rgb_indices[8], int alpha_indices[8]) {
    int second = two_steps ? 4 : 0;
    uint64_t command = 0;
    command |= (uint64_t)rgb_indices[0] << 52;
    command |= (uint64_t)rgb_indices[1] << 28;
    command |= (uint64_t)rgb_indices[2] << 47;
    command |= (uint64_t)rgb_indices[3] << 15;
    command |= (uint64_t)rgb_indices[second+0] << 37;
    command |= (uint64_t)rgb_indices[second+1] << 24;
    command |= (uint64_t)rgb_indices[second+2] << 32;
    command |= (uint64_t)rgb_indices[second+3] << 6;
    command |= (uint64_t)alpha_indices[0] << 44;
    command |= (uint64_t)alpha_indices[1] << 12;
    command |= (uint64_t)alpha_indices[2] << 41;
    command |= (uint64_t)alpha_indices[3] << 9;
    command |= (uint64_t)alpha_indices[second+0] << 21;
    command |= (uint64_t)alpha_indices[second+1] << 3;
    command |= (uint64_t)alpha_indices[second+2] << 18;
    command |= (uint64_t)alpha_indices[second+3] << 0;
    if (two_steps) command |= 1ull<<63;
    command |= 0x3Cull << 56;
    return command;
}

namespace internal {

enum UniformId {
    UNIFORM_K4,
    UNIFORM_K5,
    UNIFORM_KEYSCALE,
    UNIFORM_KEYCENTER,
    UNIFORM_PRIM_LOD_FRAC,
    UNIFORM_ENV,
    UNIFORM_PRIM,
};

struct Uniform {
    UniformId id;
    float value;
    bool forbidden;     // true if explicitly used by in the expression, so can't be used as uniform
    bool used;          // true if already allocated as uniform

    static std::vector<Uniform> create_uniforms(CombinerChannel ch) {
        switch (ch) {
        case RGB: return {
            { UNIFORM_K4 }, { UNIFORM_K5 }, { UNIFORM_KEYSCALE }, { UNIFORM_KEYCENTER },
            { UNIFORM_PRIM_LOD_FRAC }, { UNIFORM_ENV }, { UNIFORM_PRIM } };
        case ALPHA: return {
            { UNIFORM_PRIM_LOD_FRAC }, { UNIFORM_ENV }, { UNIFORM_PRIM } };
        default: throw std::invalid_argument("invalid combiner channel");
        }
    }

    bool can_use(char slot) const {
        if (forbidden) return false;
        if (used) return false;
        switch (id) {
        case UNIFORM_K4:            return slot == 'b';
        case UNIFORM_K5:            return slot == 'c';
        case UNIFORM_KEYSCALE:      return slot == 'c';
        case UNIFORM_KEYCENTER:     return slot == 'b';
        case UNIFORM_PRIM_LOD_FRAC: return slot == 'c';
        case UNIFORM_ENV:           return true;
        case UNIFORM_PRIM:          return true;
        default: throw std::invalid_argument("invalid uniform id");
        }
    }

    void set(float v) {
        value = v;
        used = true;
    }

    std::string to_slot() const {
        switch (id) {
        case UNIFORM_K4:            return "k4";
        case UNIFORM_K5:            return "k5";
        case UNIFORM_KEYSCALE:      return "keyscale";
        case UNIFORM_KEYCENTER:     return "keycenter";
        case UNIFORM_PRIM_LOD_FRAC: return "prim_lod_frac";
        case UNIFORM_ENV:           return "env";
        case UNIFORM_PRIM:          return "prim";
        default: throw std::invalid_argument("invalid uniform id");
        }
    }
};

} /* namespace internal */

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

struct CombinerExpr {
    CombinerChannel ch;
    CombinerStep step[2]{ { ch, 0 }, { ch, 1 } };
    std::vector<internal::Uniform> uniforms{internal::Uniform::create_uniforms(ch)};

    CombinerExpr(CombinerChannel ch) : ch(ch) {}
    CombinerExpr(CombinerChannel ch, std::string a, std::string b, std::string c, std::string d) : CombinerExpr(ch) {
        set(0, 'a', a);
        set(0, 'b', b);
        set(0, 'c', c);
        set(0, 'd', d);
    }

    std::string to_string() const {
        std::string ret = step[0].to_string();
        if (step[1])
            ret += "," + step[1].to_string();
        return ret;
    }

    bool has(int step_idx, char slot) const {
        return !step[step_idx].slot(slot).empty();
    }

    void set(int step_idx, char slot, std::string value)
    {
        if (!isdigit(value[0])) {
            if (ALL_SLOTS.find(value) == ALL_SLOTS.end())
                throw std::runtime_error("invalid slot name: " + value);
            if (step[step_idx].slot_index(slot, value) == -1)
                throw std::runtime_error("internal error: cannot set " + value + " in slot " + std::to_string(slot-'a') + " in channel " + std::to_string(ch) + " step " + std::to_string(step_idx));
        }
        auto& s = step[step_idx].slot(slot);
        assert(s.empty());
        s = value;
    }

    bool two_steps() const {
        return step[1];
    }

    void validate(void) {
        // This function just validate the expression looking for internal errors.
        for (int i=0; i<2; i++) {
            // Check steps configuration was not corrupted
            assert(step[i].ch == ch);
            assert(step[i].step_idx == i);
            if (!step[i]) continue;
            auto indices = step[i].slot_indices();
            for (int j=0; j<4; j++) {
                // If an index is -1, it has to be a number not yet matched to an uniform,
                // otherwise it's an internal error: it means a slot was misplaced
                // in disallowed position, that should not have happened.
                if (indices[j] == -1) {
                    std::string value = step[i].slot('a' + j);
                    if (!(isdigit(value[0]) || value[0] == '.'))
                        throw std::runtime_error("internal error: misplaced slot " + value + " in position " + std::to_string(j) + " in channel " + std::to_string(ch) + " step " + std::to_string(i));
                }
            }
        }
    }

    // Go through the combiner expression and look for slots that can be replaced with uniforms
    void allocate_uniforms(void)
    {
        for (int i=0; i<2; i++) {
            if (!step[i]) continue;
            auto indices = step[i].slot_indices();
            for (int j=0; j<4; j++) {
                // Search for a slot that doesn't map to a combiner input
                if (indices[j] != -1) continue;

                // It should now be a floating point number. Parse it.
                std::string& value = step[i].slot('a' + j);
                float v = parse_float(value, 0, 1);

                // Search if there's already an uniform with this value
                bool found = false;
                for (int k=0; k<uniforms.size(); k++) {
                    if (uniforms[k].used && uniforms[k].value == v) {
                        value = uniforms[k].to_slot();
                        found = true;
                        break;
                    }
                }
                if (found) continue;

                // Search for a uniform that can be used in this slot.
                for (auto& u : uniforms) {
                    if (u.can_use('a' + j)) {
                        // Assign the value to the uniform, and update the combiner slot
                        u.set(v);
                        value = u.to_slot();
                        found = true;
                        break;
                    }
                }

                if (!found)
                    throw std::runtime_error("no available uniform for value " + value + " in combiner expression");
            }
        }
    }

    internal::Uniform* find_uniform(internal::UniformId id) {
        for (auto& u : uniforms) {
            if (u.id == id && u.used) {
                return &u;
            }
        }
        return NULL;
    }

    std::vector<int> slot_indices() const {
        auto indices = step[0].slot_indices();
        if (two_steps()) {
            auto indices1 = step[1].slot_indices();
            indices.insert(indices.end(), indices1.begin(), indices1.end());
        }
        return indices;
    }
};

// Uniforms. An uniform is a color combiner input with a value that is fixed
// for the combiner expression and should be configured as part of the combiner
// setup. For instance, if the expression is "tex0 * 0.5", the parser will
// generate a combiner such as (env, 0, tex0, 0) and will report that the
// uniform "env" must contain the value 0.5.
//
// Use CombinerExprFull::rdp_uniforms() to get a list of all uniforms that need
// to be allocated in a certain combiner expression.
enum UniformId {
    // K4K5 is a special uniform corresponding to two YUV conversion parameters 
    // that can be specified as combiner input. To configure this, use
    // rdpq_set_yuv_parms(0, 0, 0, k4, k5).
    // The 32-bit value returned by CombinerExprFull::rdp_uniforms()
    // is packed as (k4 << 8) | k5.
    UNIFORM_K4K5,

    // This uniform configures two combiner inputs which are part of the RDP
    // chroma key feature: keycenter and keyscale. The 32-bit value returned by
    // CombinerExprFull::rdp_uniforms() is packed as (keycenter << 8) | keyscale.
    // FIXME: rdpq supports only keycenter, not keyscale. This should be fixed.
    UNIFORM_CHROMAKEY,

    // PRIM_LOD_FRAC is a uniform that normally configures the LOD fraction
    // for the texture. The value is in the range 0-255.
    // To configure this uniform, use rdpq_set_prim_lod_frac(value).
    UNIFORM_PRIM_LOD_FRAC,

    // ENV and PRIM are two color registers that can be configured via the
    // rdpq_set_env_color() and rdpq_set_prim_color() functions.
    // The 32-bit value returned by CombinerExprFull::rdp_uniforms() is packed
    // as RGBA32(r, g, b, a).
    UNIFORM_ENV,
    UNIFORM_PRIM,
};

// Return the name of the uniform as a string
std::string uniform_name(UniformId id) {
    switch (id) {
    case UNIFORM_K4K5: return "k4k5";
    case UNIFORM_CHROMAKEY: return "chromakey";
    case UNIFORM_PRIM_LOD_FRAC: return "prim_lod_frac";
    case UNIFORM_ENV: return "env";
    case UNIFORM_PRIM: return "prim";
    default: throw std::invalid_argument("invalid uniform id");
    }
}

// The full, parsed combiner expression, with both the RGB and ALPHA channels.
struct CombinerExprFull {
    CombinerExpr channels[2]{ { RGB }, { ALPHA } };

    CombinerExprFull() {}
    CombinerExprFull(CombinerExpr &rgb, CombinerExpr &alpha) {
        channels[RGB] = rgb;
        channels[ALPHA] = alpha;
        allocate_uniforms();
        fix_two_steps();
        fix_c_combined();
    }

    // Validate the CombinerExpression. This should never throw, unless the code
    // is buggy.
    void validate(void) {
        for (int i=0; i<2; i++) {
            channels[i].validate();
        }
    }

    // Return true if the combiner expression requires two steps
    bool two_steps(void) const {
        return channels[0].two_steps() || channels[1].two_steps();
    }

    // Return a string representation of the combiner expression
    std::pair<std::string,std::string> to_string(void) const {
        return { channels[RGB].to_string(), channels[ALPHA].to_string() };
    }

    // Return the slot indices for the combiner expression. The indices are
    // returned as a pair of vectors, one for the RGB channel and one for the
    // ALPHA channel.
    // The indices are either 4 or 8 elements long, depending on whether the
    // combiner expression requires one or two steps.
    std::pair<std::vector<int>,std::vector<int>> slot_indices() const {
        return { channels[RGB].slot_indices(), channels[ALPHA].slot_indices() };
    }

    // Return the 64-bit RDP command for the combiner expression.
    uint64_t rdp_command(void) const {
        auto [idx_rgb, idx_alpa] = slot_indices();

        // If the indices are 4 elements long, we need to duplicate them for the
        // second step. This is the correct way to represent indices in the
        // RDP combiner expression.
        if (!two_steps()) {
            idx_rgb.insert(idx_rgb.end(), idx_rgb.begin(), idx_rgb.end());
            idx_alpa.insert(idx_alpa.end(), idx_alpa.begin(), idx_alpa.end());
        }

        return ::combexpr::to_rdp_command(two_steps(), &idx_rgb[0], &idx_alpa[0]);
    }

    // Return a map of uniforms that need to be set for the combiner expression
    // to work. The values are packed into 32bit integers, with the layout depending
    // on the uniform (see UniformId).
    std::map<UniformId, uint32_t> rdp_uniforms(void) {
        std::map<UniformId, uint32_t> res;

        // Merge K4/k5. We stuff them into 32bit arbitrarily (8bit each)
        auto k4 = channels[RGB].find_uniform(internal::UNIFORM_K4);
        auto k5 = channels[RGB].find_uniform(internal::UNIFORM_K5);
        if (k4 || k5) {
            uint32_t value = 0;
            if (k4) value |= (int)(k4->value * 255 + 0.5f) << 8;
            if (k5) value |= (int)(k5->value * 255 + 0.5f) << 0;
            res[UNIFORM_K4K5] = value;
        }

        // Merge keycenter/keyscale
        auto keycenter = channels[RGB].find_uniform(internal::UNIFORM_KEYCENTER);
        auto keyscale  = channels[RGB].find_uniform(internal::UNIFORM_KEYSCALE);
        if (keycenter || keyscale) {
            uint32_t value = 0;
            if (keycenter) value |= (int)(keycenter->value * 255 + 0.5f) << 8;
            if (keyscale)  value |= (int)(keyscale->value * 255 + 0.5f) << 0;
            res[UNIFORM_CHROMAKEY] = value;
        }

        // Look for PRIM_LOD_FRAC in the RGB channel, because the value is copied
        // there by allocate_uniforms() even if it's just used in the alpha channel
        auto prim_lod_frac = channels[RGB].find_uniform(internal::UNIFORM_PRIM_LOD_FRAC);
        if (prim_lod_frac) {
            res[UNIFORM_PRIM_LOD_FRAC] = (int)(prim_lod_frac->value * 255 + 0.5f);
        }

        // Prim/Env
        for (auto udesc : std::vector<std::pair<internal::UniformId, UniformId>>
            { { internal::UNIFORM_PRIM, UNIFORM_PRIM }, { internal::UNIFORM_ENV, UNIFORM_ENV } }) {

            auto rgb = channels[RGB].find_uniform(udesc.first);
            auto alpha = channels[ALPHA].find_uniform(udesc.first);
            if (rgb || alpha) {
                uint32_t value = 0;
                if (rgb) {
                    int v = rgb->value * 255 + 0.5f;
                    value |= v << 24;
                    value |= v << 16;
                    value |= v << 8;
                }
                if (alpha) {
                    value |= (int)(alpha->value * 255 + 0.5f) << 0;
                }
                res[udesc.second] = value;
            }
        }

        return res;
    }

private:
    // Perform uniform allocation. This is done at construction. The two channels
    // are examined and all the raw floating point values are replaced with uniforms.
    // Allocation of uniforms ia a "global" problem to be performed when both
    // channels are available, so that some optimizations can be performed.
    void allocate_uniforms(void) {
        // Collect the names of all uniforms. This could be a compile time list but let's be more dynamic
        std::unordered_set<std::string> uniform_slot_names;
        for (int i=0; i<2; i++) {
            for (auto& u : channels[i].uniforms) {
                uniform_slot_names.insert(u.to_slot());
            }
        }

        // Search for slots that are using a potential uniform, and mark those as forbidden.
        for (int i=0; i<2; i++) {
            for (int j=0; j<2; j++) {
                if (!channels[i].step[j]) continue;
                for (int k=0; k<4; k++) {
                    auto& slot = channels[i].step[j].slot('a' + k);
                    if (uniform_slot_names.find(slot) != uniform_slot_names.end()) {
                        // We mark the uniform as forbidden in both channels, because
                        // in general there would be no easy way to merge a value provided by
                        // the user at runtime with one written in the material.
                        for (int ii=0; ii<2; ii++) {
                            for (auto& u : channels[ii].uniforms) {
                                if (u.to_slot() == slot) {
                                    u.forbidden = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Start with alpha channel, which is more starved of options for uniforms
        channels[ALPHA].allocate_uniforms();

        // PRIM_LOD_FRAC is the only uniform that can be shared between RGB and ALPHA
        // (ENV and PRIM technically are too, but those are colors so the actual values
        // are not shared between the two channels).
        // If it's used in the alpha channel, we mark it as used in the RGB channel as well.
        auto prim_lod_frac = channels[ALPHA].find_uniform(internal::UNIFORM_PRIM_LOD_FRAC);
        if (prim_lod_frac) {
            for (auto& u : channels[RGB].uniforms) {
                if (u.id == internal::UNIFORM_PRIM_LOD_FRAC) {
                    u.set(prim_lod_frac->value);
                    break;
                }
            }
        }

        // Now allocate uniforms for the RGB channel
        channels[RGB].allocate_uniforms();
    }

    // Make the combiner expression uniformly two steps if required
    void fix_two_steps(void)
    {
        bool rgb2 = channels[RGB].two_steps();
        bool alpha2 = channels[ALPHA].two_steps();
        bool two = rgb2 || alpha2;

        // Check if any slot uses tex1 or tex1: that would require two steps
        if (!two) {
            for (int ch=0; ch<2 && !two; ch++) {
                for (int s=0; s<2 && !two; s++) {
                    auto &step = channels[ch].step[s];
                    for (int j=0; j<4; j++) {
                        if (step.slot('a' + j) == "tex1" || step.slot('a' + j) == "tex1.a") {
                            two = true;
                            break;
                        }
                    }
                }
            }
        }

        // Make sure both channels are two steps if two steps is required
        if (two && !rgb2) {
            channels[RGB].set(1, 'a', "0");
            channels[RGB].set(1, 'b', "0");
            channels[RGB].set(1, 'c', "0");
            channels[RGB].set(1, 'd', "combined");
        }
        if (two && !alpha2) {
            channels[ALPHA].set(1, 'a', "0");
            channels[ALPHA].set(1, 'b', "0");
            channels[ALPHA].set(1, 'c', "0");
            channels[ALPHA].set(1, 'd', "combined");
        }

    }

    void fix_c_combined(void)
    {
        if (!two_steps())
            return;

        // As a special case, we want to avoid using COMBINED in the C slot,
        // because that can more often cause overflow of the intermediate
        // calculation. So we try to move it to the A slot if possible.
        // Not sure if this last-time patch is the best solution, but it
        // seems to work well in practice.
        for (int ch=0; ch<2; ch++) {
            auto& step = channels[ch].step[1];
            if (step.slot('c') == "combined" && step.slot('b') == "0")
                std::swap(step.slot('c'), step.slot('a'));
        }
    }
};


enum NodeType {
    NUMBER,
    IDENTIFIER,
    OP,
};

struct Node {
    NodeType type;
    std::string value;
    std::shared_ptr<Node> left;
    std::shared_ptr<Node> right;

    Node(std::string val) : value(std::move(val)), left(nullptr), right(nullptr) {
        if (isdigit(value[0])) {
            type = NUMBER;
        } else if (value == "+") {
            type = OP;
        } else if (value == "-") {
            type = OP;
        } else if (value == "*") {
            type = OP;
        } else {
            type = IDENTIFIER;
        }
    }
};

class Matcher {
public:
    std::shared_ptr<Node> root;

    explicit Matcher(const std::shared_ptr<Node>& root) : root(root) {}

    CombinerExpr matchCombiner(CombinerChannel ch) {
        CombinerExpr expr{ch};
        
        matchStructure(root, expr);
        if (expr.two_steps()) {
            // During matching, step[1] is actually the first step,
            // so now swap them to have the correct order.
            std::swap(expr.step[0], expr.step[1]);
            std::swap(expr.step[0].step_idx, expr.step[1].step_idx);
        }
        // Check there were not bugs that caused the expression to be invalid
        expr.validate();
        return expr;
    }

private:
    void setMinuend(CombinerExpr &expr, int step_idx, const std::string& value)
    {
        if (!expr.has(step_idx, 'a')) {
            expr.set(step_idx, 'a', value);
        } else {
            throw std::runtime_error("combiner expression is too complex: too many minuends");
        }
    }

    void setSubtrahend(CombinerExpr &expr, int step_idx, const std::string& value)
    {
        if (!expr.has(step_idx, 'b')) {
            expr.set(step_idx, 'b', value);
        } else {
            throw std::runtime_error("combiner expression is too complex: too many subtrahends");
        }
    }

    void matchMinuend(const std::shared_ptr<Node>& node, CombinerExpr &expr, int step_idx)
    {
        if (step_idx == 0) {
            matchStructure(node, expr, 1);
            setMinuend(expr, step_idx, "combined");
        } else
            throw std::runtime_error("combiner expression is too complex: two subtractions in second step");
    }

    void matchSubtrahend(const std::shared_ptr<Node>& node, CombinerExpr &expr, int step_idx)
    {
        if (step_idx == 0) {
            matchStructure(node, expr, 1);
            setSubtrahend(expr, step_idx, "combined");
        } else
            throw std::runtime_error("combiner expression is too complex: two subtractions in second step");
    }

    void matchSubtraction(const std::shared_ptr<Node>& node, CombinerExpr &expr, int step_idx)
    {
        auto left = node->left;
        auto right = node->right;

        if (left->type == OP)
            matchMinuend(left, expr, step_idx);
        if (right->type == OP)
            matchSubtrahend(right, expr, step_idx);

        if (left->type != OP)
            setMinuend(expr, step_idx, left->value);
        if (right->type != OP)
            setSubtrahend(expr, step_idx, right->value);
        
    }

    void setMultiplicand(CombinerExpr &expr, int step_idx, const std::string& value)
    {
        if (!expr.has(step_idx, 'c')) {
            try {
                expr.set(step_idx, 'c', value);
                return;
            } catch(std::runtime_error &e) {}
        } 
        
        if (!expr.has(step_idx, 'a') && !expr.has(step_idx, 'b')) {
            CombinerExpr copy = expr;
            try {
                expr.set(step_idx, 'a', value);
                expr.set(step_idx, 'b', "0");
                return;
            } catch(std::runtime_error &e) {
                expr = copy;
            }
        }

        throw std::runtime_error("combiner expression is too complex: cannot find a slot for " + value);
    }

    void matchMultiplicand(const std::shared_ptr<Node>& node, CombinerExpr &expr, int step_idx)
    {
        switch (node->value[0]) {
            case '+':
            case '*':
                if (step_idx == 0) {
                    matchStructure(node, expr, 1);
                    setMultiplicand(expr, step_idx, "combined");
                } else
                    throw std::runtime_error("combiner expression is too complex: two additions in second step");
                break;
            case '-':
                matchSubtraction(node, expr, step_idx);
                break;
            default:
                assert(0);
        }                   
    }

    void matchMultiplication(const std::shared_ptr<Node>& node, CombinerExpr &expr, int step_idx)
    {
        auto left = node->left;
        auto right = node->right;
        CombinerExpr copy = expr;

        for (int i=0; i<2; i++) {
            try {
                if (left->type == OP)
                    matchMultiplicand(left, expr, step_idx);
                if (right->type == OP)
                    matchMultiplicand(right, expr, step_idx);

                if (left->type != OP)
                    setMultiplicand(expr, step_idx, left->value);
                if (right->type != OP)
                    setMultiplicand(expr, step_idx, right->value);
                return;
            } catch (std::runtime_error &e) {
                expr = copy;
                if (i == 1) throw e;
                std::swap(left, right);
            }
        }
    }

    void setAddend(CombinerExpr &expr, int step_idx, const std::string& value)
    {
        if (!expr.has(step_idx, 'd')) {
            try {
                expr.set(step_idx, 'd', value);
                return;
            } catch (std::runtime_error &e) {}
        }
        
        if (!expr.has(step_idx, 'a') && !expr.has(step_idx, 'b') && !expr.has(step_idx, 'c')) {
            CombinerExpr copy = expr;
            try {
                expr.set(step_idx, 'a', "1");
                expr.set(step_idx, 'b', "0");
                expr.set(step_idx, 'c', value);
                return;
            } catch (std::runtime_error &e) {
                expr = copy;
            }

            try {
                expr.set(step_idx, 'a', value);
                expr.set(step_idx, 'b', "0");
                expr.set(step_idx, 'c', "1");
                return;
            } catch (std::runtime_error &e) {
                expr = copy;
            }
        }

        throw std::runtime_error("combiner expression is too complex: cannot find a slot for " + value);
    }

    void matchAddendExpr(const std::shared_ptr<Node>& node, CombinerExpr &expr, int step_idx)
    {
        switch (node->value[0]) {
            case '+':
                if (step_idx == 0) {
                    matchStructure(node, expr, 1);
                    setAddend(expr, 0, "combined");
                } else
                    throw std::runtime_error("combiner expression is too complex: two additions in second step");
                break;
            case '*':
                if (!expr.has(step_idx, 'c'))
                    matchMultiplication(node, expr, step_idx);
                else if (step_idx == 0) {
                    matchStructure(node, expr, 1);
                    setAddend(expr, 0, "combined");
                }
                break;
            case '-':
                setMultiplicand(expr, step_idx, "1");
                matchSubtraction(node, expr, step_idx);
                break;
            default:
                assert(0);
        }
    }

    void matchAddition(const std::shared_ptr<Node>& node, CombinerExpr &expr, int step_idx)
    {
        auto left = node->left;
        auto right = node->right;
        CombinerExpr copy = expr;

        for (int i=0; i<2; i++) {
            try {
                if (left->type == OP)
                    matchAddendExpr(left, expr, step_idx);
                if (right->type == OP) 
                    matchAddendExpr(right, expr, step_idx);

                if (left->type != OP)
                    setAddend(expr, step_idx, left->value);
                if (right->type != OP)
                    setAddend(expr, step_idx, right->value);
                return;
            } catch (std::runtime_error &e) {
                expr = copy;
                if (i == 1) throw e;
                std::swap(left, right);
            }
        }
    }

    void matchStructure(const std::shared_ptr<Node>& node, CombinerExpr &expr, int step_idx=0) {
        if (expr.step[step_idx]) {
            throw std::runtime_error("combiner expression is too complex: step already filled");
        }

        if (node->type == OP) {
            switch (node->value[0]) {
                case '+': 
                    matchAddition(node, expr, step_idx);
                    break;
                case '*': 
                    matchMultiplication(node, expr, step_idx);
                    setAddend(expr, step_idx, "0");
                    break;
                case '-': {
                    CombinerExpr copy = expr;
                    try {
                        matchSubtraction(node, expr, step_idx);
                        setMultiplicand(expr, step_idx, "1");
                        setAddend(expr, step_idx, "0");
                        break;
                    } catch (std::runtime_error &e) {
                        expr = copy;
                        if (step_idx == 1)
                            throw e;
                    }
                    if (node->left->type != OP) {
                        try {
                            setMinuend(expr, 0, node->left->value);
                            setSubtrahend(expr, 0, "combined");
                            setMultiplicand(expr, 0, "1");
                            setAddend(expr, 0, "0");
                            matchStructure(node->right, expr, 1);
                            break;
                        } catch (std::runtime_error &e) {
                            expr = copy;
                        }
                    }
                    if (node->right->type != OP) {
                        try {
                            setMinuend(expr, 0, "combined");
                            setSubtrahend(expr, 0, node->right->value);
                            setMultiplicand(expr, 0, "1");
                            setAddend(expr, 0, "0");
                            matchStructure(node->left, expr, 1);
                            break;
                        } catch (std::runtime_error &e) {
                            expr = copy;
                        }
                    }
                    throw std::runtime_error("combiner expression is too complex: subtraction cannot be placed ");
                } break;
                default: assert(0);
            }
        } else {
            CombinerExpr copy = expr;
            try {
                setAddend(expr, step_idx, node->value);
                setAddend(expr, step_idx, "0");
                return;
            } catch(std::runtime_error &e) {
                expr = copy;
            }
            try {
                setAddend(expr, step_idx, "0");
                setAddend(expr, step_idx, node->value);
                return;
            } catch(std::runtime_error &e) {
                expr = copy;
                throw;
            }
        }
    }
};

// Tokenizer and parser. This is a simple recursive descent parser that
// builds a tree of shared_ptr<Node> corresponding to the input expression.
// It also performs basic validation that all the identifiers are valid.
class Parser {
public:
    explicit Parser(const std::string& input) : input(input), pos(0) {}

    std::shared_ptr<Node> parseExpression() {
        bool nodeParens = false;
        skipWhitespace();
        auto node = parseTerm(&nodeParens);
        skipWhitespace();
        while (peek() == '+' || peek() == '-' || peek() == '*') {
            char op = advance();
            skipWhitespace();
            bool rightParens;
            auto right = parseTerm(&rightParens);
            auto parent = std::make_shared<Node>(std::string(1, op));
            if (opPriority(op) <= opPriority(node->value[0]) || nodeParens) {
                parent->left = node;
                parent->right = right;
                node = parent;
                nodeParens = false;
            } else {
                parent->left = node->right;
                node->right = parent;
                parent->right = right;
            }
            skipWhitespace();
        }
        return node;
    }

private:
    std::string input;
    size_t pos;

    char peek() const {
        return pos < input.size() ? input[pos] : '\0';
    }

    char advance() {
        return input[pos++];
    }

    int opPriority(char op) const {
        switch (op) {
            case '+':
            case '-':
                return 1;
            case '*':
                return 2;
            default:
                return 3;
        }
    }

    std::shared_ptr<Node> parseTerm(bool *parens) {
        skipWhitespace();
        if (peek() == '(') {
            advance();
            auto node = parseExpression();
            if (peek() == ')') advance();
            *parens = true;
            return node;
        } else if (isdigit(peek()) || peek() == '.') {
            *parens = false;
            return parseNumber();
        } else {
            *parens = false;
            return parseIdentifier();
        }
    }

    std::shared_ptr<Node> parseNumber() {
        std::string result;
        while (isdigit(peek()) || peek() == '.') {
            result += advance();
        }
        if (result[0] == '.') result = "0" + result;
        // Parse the float to make sure it is within the valid range (0-1)
        (void)parse_float(result, 0, 1);
        return std::make_shared<Node>(result);
    }

    std::shared_ptr<Node> parseIdentifier() {
        std::string result;
        while (isalnum(peek()) || peek() == '_' || peek() == '.') {
            result += advance();
        }
        if (ALL_SLOTS.find(result) == ALL_SLOTS.end()) {
            throw std::runtime_error("invalid identifier name: " + result);
        }
        return std::make_shared<Node>(result);
    }

    void skipWhitespace() {
        while (isspace(peek())) advance();
    }
};

void printTree(const std::shared_ptr<Node>& node, int depth = 0) {
    if (!node) return;
    printTree(node->left, depth + 1);
    std::cout << std::string(depth * 4, ' ') << node->value << "\n";
    printTree(node->right, depth + 1);
}

/**
 * @brief Parse a combiner expression
 * 
 * This is the entry point of the whole combexpr library. This function
 * takes two strings: one is the RGB expression, and the other is the
 * alpha expression. 
 * 
 * It returns a CombinerExprFull object that contains the combiner configuration
 * that corresponds to the input expressions. The caller is then expect to use
 * the CombinerExprFull methods to inspect and use the combiner configuration.
 * 
 * If any error occurs during parsing, the error string is filled with a message
 * describing the error, and the returned CombinerExprFull object is empty.
 * 
 * @param expr_rgb              The RGB combiner expression (eg: "tex0 * 0.5")
 * @param expr_alpha            The alpha combiner expression
 * @param error                 A pointer to a string that will be filled with the error message
 * @return CombinerExprFull     The combiner configuration
 */
CombinerExprFull parse(const std::string& expr_rgb, const std::string& expr_alpha, std::string *error)
{
    // STEP 1: parse the combiner expression into an AST tree. This can fail
    // if the expression contains invalid terms, that is identifier words that
    // are not combiner slots (eg: "texture0" instead of "tex0").
    Parser rgb_parser(expr_rgb);
    Parser alpha_parser(expr_alpha);
    std::shared_ptr<Node> root_rgb, root_alpha;

    try {
        root_rgb = rgb_parser.parseExpression();
    } catch (std::runtime_error &e) {
        if (error) *error = std::string("error parsing rgb expression: ") + e.what();
        return CombinerExprFull{};
    }
    
    try {
        root_alpha = alpha_parser.parseExpression();
    } catch (std::runtime_error &e) {
        if (error) *error = std::string("error parsing alpha expression: ") + e.what();
        return CombinerExprFull{};
    }

    // STEP 2: match the combiner expression. This is the where the main magic
    // happens. Pattern matching is performed on the AST tree to create the
    // resulting combiner configuration. Each channel is matched separately,
    // to provide separate error messages in case of failure.
    Matcher rgb_matcher(root_rgb);
    Matcher alpha_matcher(root_alpha);    
    CombinerExpr rgb{RGB}, alpha{ALPHA};

    try {
        rgb = rgb_matcher.matchCombiner(RGB);
    } catch (std::runtime_error &e) {
        if (error) *error = std::string("error parsing rgb expression: ") + e.what();
        return CombinerExprFull{};
    }

    try {
        alpha = alpha_matcher.matchCombiner(ALPHA);
    } catch (std::runtime_error &e) {
        if (error) *error = std::string("error parsing alpha expression: ") + e.what();
        return CombinerExprFull{};
    }
        
    // STEP 3: merge the two channels into a single CombinerExprFull object,
    // and perform uniform allocation.
    try {
        CombinerExprFull full_expr(rgb, alpha);
        if (error) error->clear();
        return full_expr;
    } catch (std::runtime_error &e) {
        if (error) *error = e.what();
        return CombinerExprFull{};
    }
}

} /* namespace combexpr */
