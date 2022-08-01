#include "rdpq_debug.h"
#include "rdp.h"
#include "debug.h"
#include "interrupt.h"
#include "rspq_constants.h"
#include <string.h>
#include <assert.h>

// Define to 1 to active internal debugging of the rdpq debug module.
// This is useful to trace bugs of rdpq itself, but it should not be
// necessary for standard debugging sessions of application code, so it
// is turned off by default.
#ifndef RDPQ_DEBUG_DEBUG
#define RDPQ_DEBUG_DEBUG     0
#endif

#if RDPQ_DEBUG_DEBUG
#define intdebugf(...) debugf(__VA_ARGS__)
#else
#define intdebugf(...) ({ })
#endif

#define BITS(v, b, e)  ((unsigned int)((v) << (63-(e)) >> (63-(e)+(b)))) 
#define BIT(v, b)      BITS(v, b, b)
#define SBITS(v, b, e) (int)BITS((int64_t)(v), b, e)

typedef struct {
    uint64_t *start;
    uint64_t *end;
    uint64_t *traced;
} rdp_buffer_t;

typedef struct {
    struct cc_cycle_s {
        struct { uint8_t suba, subb, mul, add; } rgb;
        struct { uint8_t suba, subb, mul, add; } alpha;
    } cyc[2];
} colorcombiner_t;

typedef struct {
    bool atomic;
    uint8_t cycle_type;
    struct { bool persp, detail, sharpen, lod; } tex;
    struct { bool enable; uint8_t type; } tlut;
    uint8_t sample_type;
    uint8_t tf_mode;
    bool chromakey;
    struct { uint8_t rgb, alpha; } dither;
    struct { uint8_t p, a, q, b; } blender[2];
    bool blend, read, aa;
    struct { uint8_t mode; bool color, sel_alpha, mul_alpha; } cvg;
    struct { uint8_t mode; bool upd, cmp, prim; } z;
    struct { bool enable, dither; } alphacmp;
} setothermodes_t;

struct {
    bool sent_scissor;
    bool mode_changed;
    uint64_t *last_som;
    uint64_t *last_cc;
    setothermodes_t som;
    colorcombiner_t cc;
} rdpq_state;


#define NUM_BUFFERS 12
static rdp_buffer_t buffers[NUM_BUFFERS];
static volatile int buf_ridx, buf_widx;
static rdp_buffer_t last_buffer;
static bool show_log;
void (*rdpq_trace)(void);
void (*rdpq_trace_fetch)(void);

void __rdpq_trace_fetch(void)
{
    uint64_t *start = (void*)(*DP_START | 0xA0000000);
    uint64_t *end = (void*)(*DP_END | 0xA0000000);

#if RDPQ_DEBUG_DEBUG
    intdebugf("__rdpq_trace_fetch: %p-%p\n", start, end);
    extern void *rspq_rdp_dynamic_buffers[2];
    for (int i=0;i<2;i++)
        if ((void*)start >= rspq_rdp_dynamic_buffers[i] && (void*)end <= rspq_rdp_dynamic_buffers[i]+RSPQ_RDP_DYNAMIC_BUFFER_SIZE)
            intdebugf("   -> dynamic buffer %d\n", i);
#endif

    if (start == end) return;
    if (start > end) {
        debugf("[rdpq] ERROR: invalid RDP buffer: %p-%p\n", start, end);
        return;
    }

    disable_interrupts();

    // Coalesce with last written buffer if possible. Notice that rdpq_trace put the start
    // pointer to NULL to avoid coalescing when it begins dumping it, so this should avoid
    // race conditions.
    int prev = buf_widx ? buf_widx - 1 : NUM_BUFFERS-1;
    if (buffers[prev].start == start) {
        // If the previous buffer was bigger, it is a logic error, as RDP buffers should only grow            
        if (buffers[prev].end == end) {
            enable_interrupts();
            intdebugf("   -> ignored because coalescing\n");
            return;
        }
        if (buffers[prev].end > end)
            debugf("[rdpq] ERROR: RDP buffer shrinking (%p-%p => %p-%p)\n", 
                buffers[prev].start, buffers[prev].end, start, end);
        buffers[prev].end = end;

        // If the previous buffer was already dumped, dump it again as we added more
        // information to it. We do not modify the "traced" pointer so that previously
        // dumped commands are not dumped again.
        if (buf_ridx == buf_widx) {
            intdebugf("   -> replaying from %p\n", buffers[prev].traced);
            buf_ridx = prev;
        }

        intdebugf("   -> coalesced\n");
        enable_interrupts();
        return;
    }
    // If the buffer queue is full, drop the oldest. It might create confusion in the validator,
    // but at least the log should show the latest commands which is probably more important.
    if ((buf_widx + 1) % NUM_BUFFERS == buf_ridx) {
        debugf("[rdpq] logging buffer full, dropping %d commands\n", buffers[buf_ridx].end - buffers[buf_ridx].start);
        buf_ridx = (buf_ridx + 1) % NUM_BUFFERS;
    }

    // Write the new buffer. It should be an empty slot
    buffers[buf_widx] = (rdp_buffer_t){ .start = start, .end = end, .traced = start };
    buf_widx = (buf_widx + 1) % NUM_BUFFERS;
    enable_interrupts();
}

void __rdpq_trace(void)
{
    // Update buffers to current RDP status
    if (rdpq_trace_fetch) rdpq_trace_fetch();

    while (1) {
        uint64_t *cur = 0, *end = 0;

        disable_interrupts();
        if (buf_ridx != buf_widx) {
            cur = buffers[buf_ridx].traced;
            end = buffers[buf_ridx].end;
            buffers[buf_ridx].traced = end;
            buf_ridx = (buf_ridx + 1) % NUM_BUFFERS;
        }
        enable_interrupts();

        if (!cur) break;
        while (cur < end) {
            int sz = rdpq_disasm_size(cur);
            if (show_log) rdpq_disasm(cur, stderr);
            rdpq_validate(cur, NULL, NULL);
            cur += sz;
        }
    }
}

void rdpq_debug_start(void)
{
    memset(buffers, 0, sizeof(buffers));
    memset(&last_buffer, 0, sizeof(last_buffer));
    memset(&rdpq_state, 0, sizeof(rdpq_state));
    buf_widx = buf_ridx = 0;
    show_log = false;

    rdpq_trace = __rdpq_trace;
    rdpq_trace_fetch = __rdpq_trace_fetch;
}

void rdpq_debug_log(bool log)
{
    assertf(rdpq_trace, "rdpq trace engine not started");
    show_log = log;
}

void rdpq_debug_stop(void)
{
    rdpq_trace = NULL;
    rdpq_trace_fetch = NULL;
}

static inline colorcombiner_t decode_cc(uint64_t cc) {
    return (colorcombiner_t){
        .cyc = {{
            .rgb =   { BITS(cc, 52, 55), BITS(cc, 28, 31), BITS(cc, 47, 51), BITS(cc, 15, 17) },
            .alpha = { BITS(cc, 44, 46), BITS(cc, 12, 14), BITS(cc, 41, 43), BITS(cc, 9, 11)  },
        },{
            .rgb =   { BITS(cc, 37, 40), BITS(cc, 24, 27), BITS(cc, 32, 36), BITS(cc, 6, 8)   },
            .alpha = { BITS(cc, 21, 23), BITS(cc, 3, 5),   BITS(cc, 18, 20), BITS(cc, 0, 2)   },
        }}
    };
}

static inline setothermodes_t decode_som(uint64_t som) {
    return (setothermodes_t){
        .atomic = BIT(som, 55),
        .cycle_type = BITS(som, 52, 53),
        .tex = { .persp = BIT(som, 51), .detail = BIT(som, 50), .sharpen = BIT(som, 49), .lod = BIT(som, 48) },
        .tlut = { .enable = BIT(som, 47), .type = BIT(som, 46) },
        .sample_type = BITS(som, 44, 45),
        .tf_mode = BITS(som, 41, 43),
        .chromakey = BIT(som, 40),
        .dither = { .rgb = BITS(som, 38, 39), .alpha = BITS(som, 36, 37) },
        .blender = {
            { BITS(som, 30, 31), BITS(som, 26, 27), BITS(som, 22, 23), BITS(som, 18, 19) },
            { BITS(som, 28, 29), BITS(som, 24, 25), BITS(som, 20, 21), BITS(som, 16, 17) },
        },
        .blend = BIT(som, 14), .read = BIT(som, 6), .aa = BIT(som, 3),
        .cvg = { .mode = BITS(som, 8, 9), .color = BIT(som, 7), .mul_alpha = BIT(som, 12), .sel_alpha=BIT(som, 13) },
        .z = { .mode = BITS(som, 10, 11), .upd = BIT(som, 5), .cmp = BIT(som, 4), .prim = BIT(som, 2) },
        .alphacmp = { .enable = BIT(som, 0), .dither = BIT(som, 1) },
    };
}

int rdpq_disasm_size(uint64_t *buf) {
    switch (BITS(buf[0], 56, 61)) {
    default:   return 1;
    case 0x24: return 2;  // TEX_RECT
    case 0x25: return 2;  // TEX_RECT_FLIP
    case 0x08: return 4;  // TRI_FILL
    case 0x09: return 6;  // TRI_FILL_ZBUF
    case 0x0A: return 12; // TRI_TEX
    case 0x0B: return 14; // TRI_TEX_ZBUF
    case 0x0C: return 12; // TRI_SHADE
    case 0x0D: return 14; // TRI_SHADE_ZBUF
    case 0x0E: return 20; // TRI_SHADE_TEX
    case 0x0F: return 22; // TRI_SHADE_TEX_ZBUF
    }
}

#define FX(n)          (1.0f / (1<<(n)))

void rdpq_disasm(uint64_t *buf, FILE *out)
{
    const char* flag_prefix = "";
    #define FLAG_RESET()   ({ flag_prefix = ""; })
    #define FLAG(v, s)     ({ if (v) fprintf(out, "%s%s", flag_prefix, s), flag_prefix = " "; })

    const char *fmt[8] = {"rgba", "yuv", "ci", "ia", "i", "?fmt=5?", "?fmt=6?", "?fmt=7?"};
    const char *size[4] = {"4", "8", "16", "32" };

    fprintf(out, "[%p] %016llx    ", buf, buf[0]);
    switch (BITS(buf[0], 56, 61)) {
    default:   fprintf(out, "???\n"); return;
    case 0x00: fprintf(out, "NOP\n"); return;
    case 0x27: fprintf(out, "SYNC_PIPE\n"); return;
    case 0x28: fprintf(out, "SYNC_TILE\n"); return;
    case 0x29: fprintf(out, "SYNC_FULL\n"); return;
    case 0x26: fprintf(out, "SYNC_LOAD\n"); return;
    case 0x2A: fprintf(out, "SET_KEY_GB       WidthG=%d CenterG=%d ScaleG=%d, WidthB=%d CenterB=%d ScaleB=%d\n",
                            BITS(buf[0], 44, 55), BITS(buf[0], 24, 31), BITS(buf[0], 16, 23), BITS(buf[0], 32, 43), BITS(buf[0], 8, 15), BITS(buf[0], 0, 7)); return;
    case 0x2B: fprintf(out, "SET_KEY_R        WidthR=%d CenterR=%d ScaleR=%d\n",
                            BITS(buf[0], 16, 27), BITS(buf[0], 8, 15), BITS(buf[0], 0, 7)); return;
    case 0x2C: fprintf(out, "SET_CONVERT      k0=%d k1=%d k2=%d k3=%d k4=%d k5=%d\n",
                            BITS(buf[0], 45, 53), BITS(buf[0], 36, 44), BITS(buf[0], 27, 35), BITS(buf[0], 18, 26), BITS(buf[0], 9, 17), BITS(buf[0], 0, 8)); return;
    case 0x2D: fprintf(out, "SET_SCISSOR      xy=(%.2f,%.2f)-(%.2f,%.2f)",
                            BITS(buf[0], 32, 43)*FX(2), BITS(buf[0], 44, 55)*FX(2), BITS(buf[0], 12, 23)*FX(2), BITS(buf[0], 0, 11)*FX(2));
                            if(BITS(buf[0], 25, 25)) fprintf(out, " field=%s", BITS(buf[0], 24, 24) ? "odd" : "even");
                            fprintf(out, "\n"); return;
    case 0x36: fprintf(out, "FILL_RECT        xy=(%.2f,%.2f)-(%.2f,%.2f)\n",
                            BITS(buf[0], 12, 23)*FX(2), BITS(buf[0], 0, 11)*FX(2), BITS(buf[0], 44, 55)*FX(2), BITS(buf[0], 56, 61)*FX(2)); return;
    case 0x2E: fprintf(out, "SET_PRIM_DEPTH   z=0x%x deltaz=0x%x\n", BITS(buf[0], 16, 31), BITS(buf[1], 0, 15)); return;
    case 0x37: fprintf(out, "SET_FILL_COLOR   rgba16=(%d,%d,%d,%d) rgba32=(%d,%d,%d,%d)\n",
                            BITS(buf[0], 11, 15), BITS(buf[0], 6, 10), BITS(buf[0], 1, 5), BITS(buf[0], 0, 0),
                            BITS(buf[0], 24, 31), BITS(buf[0], 16, 23), BITS(buf[0], 8, 15), BITS(buf[0], 0, 7)); return;
    case 0x38: fprintf(out, "SET_FOG_COLOR    rgba32=(%d,%d,%d,%d)\n", BITS(buf[0], 24, 31), BITS(buf[0], 16, 23), BITS(buf[0], 8, 15), BITS(buf[0], 0, 7)); return;
    case 0x39: fprintf(out, "SET_BLEND_COLOR  rgba32=(%d,%d,%d,%d)\n", BITS(buf[0], 24, 31), BITS(buf[0], 16, 23), BITS(buf[0], 8, 15), BITS(buf[0], 0, 7)); return;
    case 0x3A: fprintf(out, "SET_PRIM_COLOR   rgba32=(%d,%d,%d,%d)\n", BITS(buf[0], 24, 31), BITS(buf[0], 16, 23), BITS(buf[0], 8, 15), BITS(buf[0], 0, 7)); return;
    case 0x3B: fprintf(out, "SET_ENV_COLOR    rgba32=(%d,%d,%d,%d)\n", BITS(buf[0], 24, 31), BITS(buf[0], 16, 23), BITS(buf[0], 8, 15), BITS(buf[0], 0, 7)); return;
    case 0x2F: { fprintf(out, "SET_OTHER_MODES  ");
        const char* cyc[] = { "1cyc", "2cyc", "copy", "fill" };
        const char* texinterp[] = { "point", "point", "bilinear", "mid" };
        const char* zmode[] = { "opaque", "inter", "trans", "decal" };
        const char* rgbdither[] = { "square", "bayer", "noise", "none" };
        const char* alphadither[] = { "pat", "inv", "noise", "none" };
        const char* cvgmode[] = { "clamp", "wrap", "zap", "save" };
        const char* blend1_a[] = { "in", "mem", "blend", "fog" };
        const char* blend1_b1[] = { "in.a", "fog.a", "shade.a", "0" };
        const char* blend1_b1inv[] = { "(1-in.a)", "(1-fog.a)", "(1-shade.a)", "1" };
        const char* blend1_b2[] = { "", "mem.a", "1", "0" };
        const char* blend2_a[] = { "cyc1", "mem", "blend", "fog" };
        const char* blend2_b1[] = { "cyc1.a", "fog.a", "shade.a", "0" };
        const char* blend2_b1inv[] = { "(1-cyc1.a)", "(1-fog.a)", "(1-shade.a)", "1" };
        const char* blend2_b2[] = { "", "mem.a", "1", "0" };
        setothermodes_t som = decode_som(buf[0]);

        fprintf(out, "%s", cyc[som.cycle_type]);
        if((som.cycle_type < 2) && (som.tex.persp || som.tex.detail || som.tex.sharpen || som.tex.lod || som.sample_type != 0 || som.tf_mode != 6)) {
            fprintf(out, " tex=["); FLAG_RESET();
            FLAG(som.tex.persp, "persp"); FLAG(som.tex.persp, "detail"); FLAG(som.tex.lod, "lod"); 
            FLAG(som.sample_type, "yuv"); FLAG(som.tf_mode != 6, texinterp[som.tf_mode]);
            fprintf(out, "]");
        }
        if(som.tlut.enable) fprintf(out, " tlut%s", som.tlut.type ? "=[ia]" : "");
        if(BITS(buf[0], 16, 31)) fprintf(out, " blend=[%s*%s + %s*%s, %s*%s + %s*%s]",
            blend1_a[som.blender[0].p],  blend1_b1[som.blender[0].a], blend1_a[som.blender[0].q], som.blender[0].b ? blend1_b2[som.blender[0].b] : blend1_b1inv[som.blender[0].a],
            blend2_a[som.blender[1].p],  blend2_b1[som.blender[1].a], blend2_a[som.blender[1].q], som.blender[1].b ? blend2_b2[som.blender[1].b] : blend2_b1inv[som.blender[1].a]);
        if(som.z.upd || som.z.cmp) {
            fprintf(out, " z=["); FLAG_RESET();
            FLAG(som.z.cmp, "cmp"); FLAG(som.z.upd, "upd"); FLAG(som.z.prim, "prim"); FLAG(true, zmode[som.z.mode]);
            fprintf(out, "]");
        }
        flag_prefix = " ";
        FLAG(som.aa, "aa"); FLAG(som.read, "read"); FLAG(som.blend, "blend");
        FLAG(som.chromakey, "chroma_key"); FLAG(som.atomic, "atomic");

        if(som.alphacmp.enable) fprintf(out, " alpha_compare%s", som.alphacmp.dither ? "[dither]" : "");
        if((som.cycle_type < 2) && (som.dither.rgb != 3 || som.dither.alpha != 3)) fprintf(out, " dither=[%s,%s]", rgbdither[som.dither.rgb], alphadither[som.dither.alpha]);
        if(som.cvg.mode || som.cvg.color || som.cvg.sel_alpha || som.cvg.mul_alpha) {
            fprintf(out, " cvg=["); FLAG_RESET();
            FLAG(som.cvg.mode, cvgmode[som.cvg.mode]); FLAG(som.cvg.color, "color"); 
            FLAG(som.cvg.mul_alpha, "mul_alpha"); FLAG(som.cvg.sel_alpha, "sel_alpha");
            fprintf(out, "]");
        }
        fprintf(out, "\n");
    }; return;
    case 0x3C: { fprintf(out, "SET_COMBINE_MODE ");
        const char* rgb_suba[16] = {"comb", "tex0", "tex1", "prim", "shade", "env", "1", "noise", "0","0","0","0","0","0","0","0"};
        const char* rgb_subb[16] = {"comb", "tex0", "tex1", "prim", "shade", "env", "keycenter", "k4", "0","0","0","0","0","0","0","0"};
        const char* rgb_mul[32] = {"comb", "tex0", "tex1", "prim", "shade", "env", "keyscale", "comb.a", "tex0.a", "tex1.a", "prim.a", "shade.a", "env.a", "lod_frac", "prim_lod_frac", "k5", "0","0","0","0","0","0","0","0", "0","0","0","0","0","0","0","0"};
        const char* rgb_add[8] = {"comb", "tex0", "tex1", "prim", "shade", "env", "1", "0"};
        const char* alpha_addsub[8] = {"comb", "tex0", "tex1", "prim", "shade", "env", "1", "0"};
        const char* alpha_mul[8] = {"lod_frac", "tex0", "tex1", "prim", "shade", "env", "prim_lod_frac", "0"};
        colorcombiner_t cc = decode_cc(buf[0]);
        fprintf(out, "cyc0=[(%s-%s)*%s+%s, (%s-%s)*%s+%s], cyc1=[(%s-%s)*%s+%s, (%s-%s)*%s+%s]\n",
            rgb_suba[cc.cyc[0].rgb.suba], rgb_subb[cc.cyc[0].rgb.subb], rgb_mul[cc.cyc[0].rgb.mul], rgb_add[cc.cyc[0].rgb.add],
            alpha_addsub[cc.cyc[0].alpha.suba], alpha_addsub[cc.cyc[0].alpha.subb], alpha_mul[cc.cyc[0].alpha.mul], alpha_addsub[cc.cyc[0].alpha.subb],
            rgb_suba[cc.cyc[1].rgb.suba], rgb_subb[cc.cyc[1].rgb.subb], rgb_mul[cc.cyc[1].rgb.mul], rgb_add[cc.cyc[1].rgb.add],
            alpha_addsub[cc.cyc[1].alpha.suba], alpha_addsub[cc.cyc[1].alpha.subb],   alpha_mul[cc.cyc[1].alpha.mul], alpha_addsub[cc.cyc[1].alpha.add]);
    } return;
    case 0x35: { fprintf(out, "SET_TILE         ");
        fprintf(out, "tile=%d %s%s tmem[0x%x,line=%d]", 
            BITS(buf[0], 24, 26), fmt[BITS(buf[0], 53, 55)], size[BITS(buf[0], 51, 52)],
            BITS(buf[0], 32, 40)*8, BITS(buf[0], 41, 49)*8);
        fprintf(out, "\n");
    } return;
    case 0x24 ... 0x25:
        if(BITS(buf[0], 56, 61) == 0x24)
            fprintf(out, "TEX_RECT         ");
        else
            fprintf(out, "TEX_RECT_FLIP    ");
        fprintf(out, "tile=%d xy=(%.2f,%.2f)-(%.2f,%.2f)\n", BITS(buf[0], 24, 26),
            BITS(buf[0], 12, 23)*FX(2), BITS(buf[0], 0, 11)*FX(2), BITS(buf[0], 44, 55)*FX(2), BITS(buf[0], 32, 43)*FX(2));
        fprintf(out, "[%p] %016llx                     ", &buf[1], buf[1]);
        fprintf(out, "st=(%.2f,%.2f) dst=(%.5f,%.5f)\n",
            SBITS(buf[1], 48, 63)*FX(5), SBITS(buf[1], 32, 47)*FX(5), SBITS(buf[1], 16, 31)*FX(10), SBITS(buf[1], 0, 15)*FX(10));
        return;
    case 0x32: case 0x34:
        if(BITS(buf[0], 56, 61) == 0x32)
            fprintf(out, "SET_TILE_SIZE    ");
        else
            fprintf(out, "LOAD_TILE        ");    
        fprintf(out, "tile=%d st=(%.2f,%.2f)-(%.2f,%.2f)\n",
            BITS(buf[0], 24, 26), BITS(buf[0], 44, 55)*FX(2), BITS(buf[0], 32, 43)*FX(2), 
                                BITS(buf[0], 12, 23)*FX(2), BITS(buf[0], 0, 11)*FX(2));
        return;
    case 0x30: fprintf(out, "LOAD_TLUT        tile=%d palidx=(%d-%d)\n",
        BITS(buf[0], 24, 26), BITS(buf[0], 46, 55), BITS(buf[0], 14, 23)); return;
    case 0x33: fprintf(out, "LOAD_BLOCK       tile=%d st=(%d,%d) n=%d dxt=%.5f\n",
        BITS(buf[0], 24, 26), BITS(buf[0], 44, 55), BITS(buf[0], 32, 43),
                              BITS(buf[0], 12, 23)+1, BITS(buf[0], 0, 11)*FX(11)); return;
    case 0x08 ... 0x0F: {
        const char *tri[] = { "TRI              ", "TRI_Z            ", "TRI_TEX          ", "TRI_TEX_Z        ",  "TRI_SHADE        ", "TRI_SHADE_Z      ", "TRI_TEX_SHADE    ", "TRI_TEX_SHADE_Z  "};
        int words[] = {4, 4+2, 4+8, 4+8+2, 4+8, 4+8+2, 4+8+8, 4+8+8+2};
        fprintf(out, "%s", tri[BITS(buf[0], 56, 61)-0x8]);
        fprintf(out, "%s tile=%d lvl=%d y=(%.2f, %.2f, %.2f)\n",
            BITS(buf[0], 55, 55) ? "left" : "right", BITS(buf[0], 48, 50), BITS(buf[0], 51, 53),
            SBITS(buf[0], 32, 45)*FX(2), SBITS(buf[0], 16, 29)*FX(2), SBITS(buf[0], 0, 13)*FX(2));
        fprintf(out, "[%p] %016llx                     xl=%.4f dxld=%.4f\n", &buf[1], buf[1],
            SBITS(buf[1], 32, 63)*FX(16), SBITS(buf[1], 0, 31)*FX(16));
        fprintf(out, "[%p] %016llx                     xh=%.4f dxhd=%.4f\n", &buf[2], buf[2],
            SBITS(buf[2], 32, 63)*FX(16), SBITS(buf[2], 0, 31)*FX(16));
        fprintf(out, "[%p] %016llx                     xm=%.4f dxmd=%.4f\n", &buf[3], buf[3],
            SBITS(buf[3], 32, 63)*FX(16), SBITS(buf[3], 0, 31)*FX(16));
        for (int i = 4; i < words[BITS(buf[0], 56, 61)-0x8]; i++)
            fprintf(out, "[%p] %016llx                     \n", &buf[i], buf[i]);
        return;
    }
    case 0x3e: fprintf(out, "SET_Z_IMAGE      dram=%08x\n", BITS(buf[0], 0, 25)); return;
    case 0x3d: fprintf(out, "SET_TEX_IMAGE    dram=%08x w=%d %s%s\n", 
        BITS(buf[0], 0, 25), BITS(buf[0], 32, 41)+1, fmt[BITS(buf[0], 53, 55)], size[BITS(buf[0], 51, 52)]);
        return;
    case 0x3f: fprintf(out, "SET_COLOR_IMAGE  dram=%08x w=%d %s%s\n", 
        BITS(buf[0], 0, 25), BITS(buf[0], 32, 41)+1, fmt[BITS(buf[0], 53, 55)], size[BITS(buf[0], 51, 52)]);
        return;
    }
}

/** 
 * @brief Check and trigger a RDP validation error. 
 * 
 * This should be triggered only whenever the commands rely on an undefined hardware 
 * behaviour or in general strongly misbehave with respect to the reasonable
 * expectation of the programmer. Typical expected outcome on real hardware should be
 * garbled graphcis or hardware freezes. */
#define VALIDATE_ERR(cond, msg, ...) ({ \
    if (!(cond)) { \
        debugf("[RDPQ_VALIDATION] ERROR: "); \
        debugf(msg "\n", ##__VA_ARGS__); \
        if (errs) *errs += 1; \
    }; \
})

/** 
 * @brief Check and trigger a RDP validation warning.
 * 
 * This should be triggered whenever the commands deviate from standard practice or
 * in general are dubious in their use. It does not necessarily mean that the RDP
 * is going to misbehave but it is likely that the programmer did not fully understand
 * what the RDP is going to do. It is OK to have false positives here -- if the situation
 * becomes too unwiedly, we can later add a way to disable classes of warning in specific
 * programs.
 */
#define VALIDATE_WARN(cond, msg, ...) ({ \
    if (!(cond)) { \
        debugf("[RDPQ_VALIDATION] WARN: "); \
        debugf(msg "\n", ##__VA_ARGS__); \
        if (warns) *warns += 1; \
    }; \
})

/** 
 * @brief Perform lazy evaluation of SOM and CC changes.
 * 
 * Validation of color combiner requires to know the current cycle type (which is part of SOM).
 * Since it's possible to send SOM / CC in any order, what matters is if, at the point of a
 * drawing command, the configuration is correct.
 * 
 * Validation of CC is thus run lazily whenever a draw command is issued.
 */
static void lazy_validate_cc(int *errs, int *warns) {
    if (rdpq_state.mode_changed) {
        rdpq_state.mode_changed = false;

        // We don't care about CC setting in fill/copy mode, where the CC is not used.
        if (rdpq_state.som.cycle_type >= 2)
            return;

        if (!rdpq_state.last_cc) {
            VALIDATE_ERR(rdpq_state.last_cc, "SET_COMBINE not called before drawing primitive");
            return;
        }
        struct cc_cycle_s *ccs = &rdpq_state.cc.cyc[0];
        if (rdpq_state.som.cycle_type == 0) { // 1cyc
            VALIDATE_WARN(memcmp(&ccs[0], &ccs[1], sizeof(struct cc_cycle_s)) == 0,
                "SET_COMBINE at %p: in 1cycle mode, the color combiner should be programmed identically in both cycles. Cycle 0 will be ignored.", rdpq_state.last_cc);
            VALIDATE_ERR(ccs[1].rgb.suba != 0 && ccs[1].rgb.suba != 0 && ccs[1].rgb.mul != 0 && ccs[1].rgb.add != 0 &&
                         ccs[1].alpha.suba != 0 && ccs[1].alpha.suba != 0 && ccs[1].alpha.mul != 0 && ccs[1].alpha.add != 0,
                "SET_COMBINE at %p: in 1cycle mode, the color combiner cannot access the COMBINED slot", rdpq_state.last_cc);
            VALIDATE_ERR(ccs[1].rgb.suba != 2 && ccs[1].rgb.subb != 2 && ccs[1].rgb.mul != 2 && ccs[1].rgb.add != 2 &&
                         ccs[1].alpha.suba != 2 && ccs[1].alpha.subb != 2 && ccs[1].alpha.mul != 2 && ccs[1].alpha.add != 2,
                "SET_COMBINE at %p: in 1cycle mode, the color combiner cannot access the TEX1 slot", rdpq_state.last_cc);
        } else { // 2 cyc
            struct cc_cycle_s *ccs = &rdpq_state.cc.cyc[0];
            VALIDATE_ERR(ccs[0].rgb.suba != 0 && ccs[0].rgb.suba != 0 && ccs[0].rgb.mul != 0 && ccs[0].rgb.add != 0 &&
                         ccs[0].alpha.suba != 0 && ccs[0].alpha.suba != 0 && ccs[0].alpha.mul != 0 && ccs[0].alpha.add != 0,
                "SET_COMBINE at %p: in 2cycle mode, the color combiner cannot access the COMBINED slot in the first cycle", rdpq_state.last_cc);
            VALIDATE_ERR(ccs[1].rgb.suba != 2 && ccs[1].rgb.suba != 2 && ccs[1].rgb.mul != 2 && ccs[1].rgb.add != 2 &&
                         ccs[1].alpha.suba != 2 && ccs[1].alpha.suba != 2 && ccs[1].alpha.mul != 2 && ccs[1].alpha.add != 2,
                "SET_COMBINE at %p: in 2cycle mode, the color combiner cannot access the TEX1 slot in the second cycle (but TEX0 contains the second texture)", rdpq_state.last_cc);
        }
    }
}

static void validate_draw_cmd(int *errs, int *warns, bool use_colors, bool use_tex, bool use_z)
{
    VALIDATE_ERR(rdpq_state.sent_scissor,
        "undefined behavior: drawing command before a SET_SCISSOR was sent");

    switch (rdpq_state.som.cycle_type) {
    case 0 ... 1: // 1cyc, 2cyc
        for (int i=1-rdpq_state.som.cycle_type; i<2; i++) {
            struct cc_cycle_s *ccs = &rdpq_state.cc.cyc[i];
            uint8_t slots[8] = {
                ccs->rgb.suba,   ccs->rgb.subb,   ccs->rgb.mul,   ccs->rgb.add, 
                ccs->alpha.suba, ccs->alpha.subb, ccs->alpha.mul, ccs->alpha.add, 
            };

            if (!use_tex) {
                VALIDATE_ERR(!memchr(slots, 1, sizeof(slots)),
                    "cannot draw a non-textured primitive with a color combiner using the TEX0 slot (CC set at %p)", rdpq_state.last_cc);
                VALIDATE_ERR(!memchr(slots, 2, sizeof(slots)),
                    "cannot draw a non-textured primitive with a color combiner using the TEX1 slot (CC set at %p)", rdpq_state.last_cc);
            }
            if (!use_colors) {
                VALIDATE_ERR(!memchr(slots, 4, sizeof(slots)),
                    "cannot draw a non-shaded primitive with a color combiner using the SHADE slot (CC set at %p)", rdpq_state.last_cc);
            }
        }

        if (use_tex && !use_z)
            VALIDATE_ERR(!rdpq_state.som.tex.persp,
                "cannot draw a textured primitive with perspective correction but without per-vertex W coordinate (SOM set at %p)", rdpq_state.last_som);
        break;
    }
}

void rdpq_validate(uint64_t *buf, int *errs, int *warns)
{
    uint8_t cmd = BITS(buf[0], 56, 61);
    switch (cmd) {
    case 0x2F: // SET_OTHER_MODES
        rdpq_state.som = decode_som(buf[0]);
        rdpq_state.mode_changed = &buf[0];
        break;
    case 0x3C: // SET_COMBINE
        rdpq_state.cc = decode_cc(buf[0]);
        rdpq_state.last_cc = &buf[0];
        rdpq_state.mode_changed = true;
        break;
    case 0x2D: // SET_SCISSOR
        rdpq_state.sent_scissor = true;
        break;
    case 0x25: // TEX_RECT_FLIP
        VALIDATE_ERR(rdpq_state.som.cycle_type < 2, "cannot draw texture flip in copy/flip mode");
        // passthrough
    case 0x24: // TEX_RECT
        lazy_validate_cc(errs, warns);
        validate_draw_cmd(errs, warns, false, true, false);
        break;
    case 0x36: // FILL_RECTANGLE
        lazy_validate_cc(errs, warns);
        validate_draw_cmd(errs, warns, false, false, false);
        break;
    case 0x8 ... 0xF: // Triangles
        VALIDATE_ERR(rdpq_state.som.cycle_type < 2, "cannot draw triangles in copy/fill mode (SOM set at %p)", rdpq_state.last_som);
        lazy_validate_cc(errs, warns);
        validate_draw_cmd(errs, warns, cmd & 4, cmd & 2, cmd & 1);
        break;
    }
}
