/**
 * @file rdpq_debug.c
 * @brief RDP Command queue: debugging helpers
 * @ingroup rdp
 */
#include "rdpq_debug.h"
#include "rdpq_debug_internal.h"
#ifdef N64
#include "rdpq.h"
#include "rspq.h"
#include "rdpq_rect.h"
#include "rdpq_mode.h"
#include "rdpq_internal.h"
#include "rdp.h"
#include "debug.h"
#include "interrupt.h"
#include "utils.h"
#include "rspq_constants.h"
#include "rdpq_constants.h"
#else
///@cond
#define debugf(msg, ...)  fprintf(stderr, msg, ##__VA_ARGS__)
#define MIN(a,b)          ((a)<(b)?(a):(b))
#define MAX(a,b)          ((a)>(b)?(a):(b))
///@endcond
#endif
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
///@cond
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
///@endcond

/** @brief RDP Debug command: turn on/off logging */
#define RDPQ_CMD_DEBUG_SHOWLOG  0x00010000
/** @brief RDP Debug command: debug message */
#define RDPQ_CMD_DEBUG_MESSAGE  0x00020000

/** @brief Flags that configure the logging */
int __rdpq_debug_log_flags;

#ifndef RDPQ_DEBUG_DEBUG
/**
 * @brief Internal debugging of rdpq_debug.
 * 
 * Define to 1 to active internal debugging of the rdpq debug module.
 * This is useful to trace bugs of rdpq itself, but it should not be
 * necessary for standard debugging sessions of application code, so it
 * is turned off by default.
 */
#define RDPQ_DEBUG_DEBUG     0
#endif

#if RDPQ_DEBUG_DEBUG
/** @brief Like debugf, but guarded by #RDPQ_DEBUG_DEBUG */
#define intdebugf(...) debugf(__VA_ARGS__)
#else
/** @brief Like debugf, but guarded by #RDPQ_DEBUG_DEBUG */
#define intdebugf(...) ({ })
#endif

/** @brief Extract bits from word */
#define BITS(v, b, e)  ((unsigned int)((v) << (63-(e)) >> (63-(e)+(b)))) 
/** @brief Extract bit from word */
#define BIT(v, b)      BITS(v, b, b)
/** @brief Extract bits from word as signed quantity */
#define SBITS(v, b, e) (int)BITS((int64_t)(v), b, e)
/** @brief Extract command ID from RDP command word */
#define CMD(v)         BITS((v), 56, 61)
/** @brief Check if a command is a triangle */
#define CMD_IS_TRI(cmd) ((cmd) >= 0x8 && (cmd) <= 0xF)

/** @brief A buffer sent to RDP via DMA */
typedef struct {
    uint64_t *start;    ///< Start pointer
    uint64_t *end;      ///< End pointer
    uint64_t *traced;   ///< End pointer of already-traced commands
} rdp_buffer_t;

/** @brief Decoded SET_COMBINE command */
typedef struct {
    ///@cond
    struct cc_cycle_s {
        struct { uint8_t suba, subb, mul, add; } rgb;
        struct { uint8_t suba, subb, mul, add; } alpha;
    } cyc[2];
    ///@endcond
} colorcombiner_t;

/** @brief Decoded SET_OTHER_MODES command */
typedef struct {
    ///@cond
    bool atomic;
    uint8_t cycle_type;
    struct { bool persp, detail, sharpen, lod; } tex;
    struct { bool enable; uint8_t type; } tlut;
    uint8_t sample_type;
    uint8_t tf_mode;
    bool chromakey;
    struct { uint8_t rgb, alpha; } dither;
    struct blender_s { uint8_t p, a, q, b; } blender[2];
    bool blend, read, aa;
    struct { uint8_t mode; bool color, sel_alpha, mul_alpha; } cvg;
    struct { uint8_t mode; bool upd, cmp, prim; } z;
    struct { bool enable, noise; } alphacmp;
    struct { bool fog, freeze, bl2; } rdpqx;     // rdpq extensions
    ///@endcond
} setothermodes_t;

/** 
 * @brief Current RDP state 
 * 
 * This structure represents a mirror of the internal state of the RDP.
 * It is updated by the validator as commands flow through, and is then used
 * to validate the consistency of next commands.
 */
static struct {
    struct { 
        bool pipe;                         ///< True if the pipe is busy (SYNC_PIPE required)
        bool tile[8];                      ///< True if each tile is a busy (SYNC_TILE required)
        uint8_t tmem[64];                  ///< Bitarray: busy state for each 8-byte word of TMEM (SYNC_LOAD required)
    } busy;                              ///< Busy entities (for SYNC commands)
    struct {
        bool sent_scissor : 1;               ///< True if at least one SET_SCISSOR was sent since reset
        bool sent_zprim : 1;                 ///< True if SET_PRIM_DEPTH was sent
        bool mode_changed : 1;               ///< True if there is a pending mode change to validate (SET_OTHER_MODES / SET_COMBINE)
        bool rendertarget_changed : 1;       ///< True if there is a pending render target change to validate (SET_COLOR_IMAGE / SET_SCISSOR)
    };
    uint64_t *last_som;                  ///< Pointer to last SOM command sent
    uint64_t last_som_data;              ///< Last SOM command (raw)
    uint64_t *last_cc;                   ///< Pointer to last CC command sent
    uint64_t last_cc_data;               ///< Last CC command (raw)
    uint64_t *last_col;                  ///< Pointer to last SET_COLOR_IMAGE command sent
    uint64_t last_col_data;              ///< Last COLOR command (raw)
    uint64_t *last_tex;                  ///< Pointer to last SET_TEX_IMAGE command sent
    uint64_t last_tex_data;              ///< Last TEX command (raw)
    uint64_t *last_z;                    ///< Pointer to last SET_Z_IMAGE command sent
    uint64_t last_z_data;                ///< Last Z command (raw)
    setothermodes_t som;                 ///< Current SOM state
    colorcombiner_t cc;                  ///< Current CC state
    struct tile_s { 
        uint64_t *last_settile;            ///< Pointer to last SET_TILE command sent
        uint64_t *last_setsize;            ///< Pointer to last LOAD_TILE/SET_TILE_SIZE command sent
        uint64_t last_settile_data;        ///< Last SET_TILE command (raw)
        uint64_t last_setsize_data;        ///< Last LOAD_TILE/SET_TILE_SIZE command (raw)
        uint8_t fmt, size;                 ///< Format & size (RDP format/size bits)
        uint8_t pal;                       ///< Palette number
        bool has_extents;                  ///< True if extents were set (via LOAD_TILE / SET_TILE_SIZE)
        float s0, t0, s1, t1;              ///< Extents of tile in TMEM
        int16_t tmem_addr;                 ///< Address in TMEM
        int16_t tmem_pitch;                ///< Pitch in TMEM
        struct {
            uint8_t mask;                   ///< Mask (RDP mask bits)
            bool clamp;                     ///< Clamping enabled
            bool mirror;                    ///< Mirroring enabled
        } s, t;                            ///< Settings for S&T coordinates
    } tile[8];                           ///< Current tile descriptors
    struct {
        uint8_t fmt, size;                 ///< Format & size (RDP format/size bits)
        uint16_t width, height;            ///< Dimensions of the color image
    } col;                               ///< Current associated color image
    struct {
        uint32_t physaddr;                 ///< Physical address of the texture
        uint8_t fmt, size;                 ///< Format & size (RDP format/size bits)
    } tex;                               ///< Current associated texture image
    struct {
        uint16_t x0,y0,x1,y1;              ///< Scissor extents
    } clip;                              ///< Current scissor extents
} rdp;

/**
 * @brief Validator context
 */
struct {
    uint64_t *buf;                         ///< Current instruction
    uint32_t flags;                        ///< Flags (see RDPQ_VALIDATION_*)
    int warns, errs;                       ///< Validators warnings/errors (stats)
    bool crashed;                          ///< True if the RDP chip crashed
} vctx;

/** @brief Triangle primitives names */
static const char *tri_name[] = { "TRI", "TRI_Z", "TRI_TEX", "TRI_TEX_Z",  "TRI_SHADE", "TRI_SHADE_Z", "TRI_TEX_SHADE", "TRI_TEX_SHADE_Z"};
static const char *tex_fmt_name[] = { "RGBA", "YUV", "CI", "IA", "I", "?", "?", "?" };

/** @brief Helper function to coalesce disassembled triangles */
static bool log_coalesce_tris(uint8_t cmd, uint8_t *last_tri_cmd, int *num_tris);

#ifdef N64
#define MAX_BUFFERS 12                                    ///< Maximum number of pending RDP buffers
#define MAX_HOOKS 4                                       ///< Maximum number of custom hooks
static rdp_buffer_t buffers[MAX_BUFFERS];                 ///< Pending RDP buffers (ring buffer)
static volatile int buf_ridx, buf_widx;                   ///< Read/write index into the ring buffer of RDP buffers
static bool buf_changed;                                  ///< True if the RDP has just switched buffer
static rdp_buffer_t last_buffer;                          ///< Last RDP buffer that was processed
static int show_log;                                      ///< != 0 if logging is enabled
static void (*hooks[MAX_HOOKS])(void*, uint64_t*, int);   ///< Custom hooks
static void* hooks_ctx[MAX_HOOKS];                        ///< Context for the hooks

// Documented in rdpq_debug_internal.h
void (*rdpq_trace)(void);
void (*rdpq_trace_fetch)(bool new_buffer);

/** @brief Run the actual trace flushing the cached buffers */
void __rdpq_trace_flush(void);

/** @brief Implementation of #rdpq_trace_fetch */
void __rdpq_trace_fetch(bool new_buffer)
{
    disable_interrupts();

    // Extract current start/end pointers from RDP registers (in the uncached segment)
    // Avoid race conditions versus RSP by reading the status register twice and retrying
    // if it changed in between.
    uint64_t *start, *end, status, status_prev;
    do {
        status_prev = *DP_STATUS;
        start = (void*)(*DP_START | 0xA0000000);
        end = (void*)(*DP_END | 0xA0000000);
        status = *DP_STATUS;
    } while (status != status_prev);

    // If the registers contain a new start pointer without its associated end pointer,
    // it means that we can't use this data: we don't know the full new buffer yet.
    // In this case, we just return and wait for the next call.
    if ((status & DP_STATUS_START_VALID) && !(status & DP_STATUS_END_VALID))
    {
        enable_interrupts();
        return;
    }

#if RDPQ_DEBUG_DEBUG
    intdebugf("__rdpq_trace_fetch: %p-%p\n", start, end);
    extern void *rspq_rdp_dynamic_buffers[2];
    for (int i=0;i<2;i++)
        if ((void*)start >= rspq_rdp_dynamic_buffers[i] && (void*)end <= rspq_rdp_dynamic_buffers[i]+RDPQ_DYNAMIC_BUFFER_SIZE)
            intdebugf("   -> dynamic buffer %d\n", i);
#endif

    assertf(start <= end, "rdpq_debug: invalid RDP buffer: %p-%p\n", start, end);

    // Coalesce with last written buffer if possible. Notice that rdpq_trace put the start
    // pointer to NULL to avoid coalescing when it begins dumping it, so this should avoid
    // race conditions.
    int prev = buf_widx ? buf_widx - 1 : MAX_BUFFERS-1;
    if (!buf_changed && buffers[prev].start == start) {
        if (buffers[prev].end == end) {
            buf_changed = new_buffer;
            enable_interrupts();
            intdebugf("   -> ignored because coalescing\n");
            return;
        }
        // If the previous buffer was bigger, it is a logic error, as RDP buffers should only grow            
        assertf(buffers[prev].end <= end, "rdpq_debug: RDP buffer shrinking (%p-%p => %p-%p)\n", 
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
        buf_changed = new_buffer;
        __rdpq_trace_flush();  // FIXME: remove this (see __rdpq_trace)
        enable_interrupts();
        return;
    }

    // If the buffer is full, we could continue logging by skipping a buffer, but the validator
    // is done with. So for now just abort.
    assertf((buf_widx + 1) % MAX_BUFFERS != buf_ridx, "validator buffer full\n");

    // Write the new buffer. It should be an empty slot
    buffers[buf_widx] = (rdp_buffer_t){ .start = start, .end = end, .traced = start };
    intdebugf("   -> written to slot %d\n", buf_widx);
    buf_widx = (buf_widx + 1) % MAX_BUFFERS;

    // If we know for sure that the RDP is about the change buffer, remember it so that
    // next reads will surely be a new one. For instance, this allows to process twice
    // a same buffer sent two times in a row.
    buf_changed = new_buffer;

    __rdpq_trace_flush();  // FIXME: remove this (see __rdpq_trace)
    enable_interrupts();
}

/** @brief Process a RDPQ_DEBUG command */
void __rdpq_debug_cmd(uint64_t cmd)
{
    switch(BITS(cmd, 48, 55)) {
    case 0x01: // Show log
        show_log += BIT(cmd, 0) ? 1 : -1;
        return;
    case 0x02: // Message
        // Nothing to do. Debugging messages are shown by the disassembler
        return;
    }
}

/** @brief Implementation of #rdpq_trace */
void __rdpq_trace(void)
{
    // Update buffers to current RDP status. This make sure the trace
    // is up to date.
    __rdpq_trace_fetch(false);
    __rdpq_trace_flush();
}

void __rdpq_trace_flush(void)
{
    while (1) {
        uint64_t *cur = 0, *end = 0;

        // Pop next RDP buffer from ring buffer. Do it atomically to avoid races
        disable_interrupts();
        if (buf_ridx != buf_widx) {
            cur = buffers[buf_ridx].traced;
            end = buffers[buf_ridx].end;
            buffers[buf_ridx].traced = end;
            buf_ridx = (buf_ridx + 1) % MAX_BUFFERS;
        }
        enable_interrupts();

        // If there are no more pending buffers, we are done
        if (!cur) break;

        // Go through the RDP buffer. If log is active, disassemble.
        // Run the validator on all the commands.
        while (cur < end) {
            uint8_t cmd = BITS(cur[0],56,61);
            int sz = rdpq_debug_disasm_size(cur);
            
            // Disassemble the command
            bool shown = false;
            if (show_log > 0)
                shown = rdpq_debug_disasm(cur, stderr);

            // Validate the command: if the command was already shown, we don't need
            // to further echo it.
            uint32_t val_flags = shown ? RDPQ_VALIDATE_FLAG_NOECHO : 0;
            rdpq_validate(cur, val_flags, NULL, NULL);

            // Run trace hooks
            for (int i=0;i<MAX_HOOKS && hooks[i];i++)
                hooks[i](hooks_ctx[i], cur, sz);

            // If this is a RDPQ_DEBUG command, execute it
            if (cmd == RDPQ_CMD_DEBUG) __rdpq_debug_cmd(cur[0]);
            cur += sz;
        }
    }

    // show the accumulated tris (if any)
    rdpq_debug_disasm(NULL, stderr);
}

void rdpq_debug_start(void)
{
    memset(buffers, 0, sizeof(buffers));
    memset(&last_buffer, 0, sizeof(last_buffer));
    memset(&rdp, 0, sizeof(rdp));
    memset(&vctx, 0, sizeof(vctx));
    memset(&hooks, 0, sizeof(hooks));
    buf_widx = buf_ridx = 0;
    show_log = 0;
    __rdpq_debug_log_flags = 0;

    rdpq_trace = __rdpq_trace;
    rdpq_trace_fetch = __rdpq_trace_fetch;

    assertf(__rdpq_inited, "rdpq_init() must be called before rdpq_debug_start()");
    rspq_write(RDPQ_OVL_ID, RDPQ_CMD_SET_DEBUG_MODE, 1);
}

void rdpq_debug_log(bool log)
{
    assertf(rdpq_trace, "rdpq trace engine not started");
    rdpq_passthrough_write((RDPQ_CMD_DEBUG, RDPQ_CMD_DEBUG_SHOWLOG, log ? 1 : 0));
}

void rdpq_debug_log_msg(const char *msg)
{
    if (rdpq_trace)
        rdpq_passthrough_write((RDPQ_CMD_DEBUG, RDPQ_CMD_DEBUG_MESSAGE, PhysicalAddr(msg)));
}

void rdpq_debug_stop(void)
{
    rdpq_trace = NULL;
    rdpq_trace_fetch = NULL;
    rspq_write(RDPQ_OVL_ID, RDPQ_CMD_SET_DEBUG_MODE, 0);
}

void rdpq_debug_install_hook(void (*hook)(void*, uint64_t*, int), void* ctx)
{
    for (int i=0;i<MAX_HOOKS;i++)
        if (!hooks[i]) {
            hooks[i] = hook;
            hooks_ctx[i] = ctx;
            return;
        }
    assertf(0, "reached maximum number of hooks (%d)", MAX_HOOKS);
}

#endif

/** @brief Decode a SET_COMBINE command into a #colorcombiner_t structure */
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

/** @brief Decode a SET_OTHER_MODES command into a #setothermodes_t structure */
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
        .alphacmp = { .enable = BIT(som, 0), .noise = BIT(som, 1) },
        .rdpqx = { .fog = BIT(som, 32), .freeze = BIT(som, 33), .bl2 = BIT(som, 15) },
    };
}

int rdpq_debug_disasm_size(uint64_t *buf) {
    switch (CMD(buf[0])) {
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

/** @brief Multiplication factor to convert a number to fixed point with precision n */
#define FX(n)          (1.0f / (1<<(n)))
/** @brief Convert a 16.16 fixed point number into floating point */
#define FX32(hi,lo)    ((int16_t)(hi) + (lo) * (1.f / 65536.f))

static void __rdpq_debug_disasm(uint64_t *addr, uint64_t *buf, FILE *out)
{
    const char* flag_prefix = "";
    ///@cond
    #define FLAG_RESET()   ({ flag_prefix = ""; })
    #define FLAG(v, s)     ({ if (v) fprintf(out, "%s%s", flag_prefix, s), flag_prefix = " "; })
    ///@endcond

    static const char *fmt[8] = {"rgba", "yuv", "ci", "ia", "i", "?fmt=5?", "?fmt=6?", "?fmt=7?"};
    static const char *size[4] = {"4", "8", "16", "32" };

    fprintf(out, "[%p] %016" PRIx64 "    ", addr, buf[0]);
    switch (CMD(buf[0])) {
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
                            BITS(buf[0], 44, 55)*FX(2), BITS(buf[0], 32, 43)*FX(2), BITS(buf[0], 12, 23)*FX(2), BITS(buf[0], 0, 11)*FX(2));
                            if(BITS(buf[0], 25, 25)) fprintf(out, " field=%s", BITS(buf[0], 24, 24) ? "odd" : "even");
                            fprintf(out, "\n"); return;
    case 0x36: fprintf(out, "FILL_RECT        xy=(%.2f,%.2f)-(%.2f,%.2f)\n",
                            BITS(buf[0], 12, 23)*FX(2), BITS(buf[0], 0, 11)*FX(2), BITS(buf[0], 44, 55)*FX(2), BITS(buf[0], 32, 43)*FX(2)); return;
    case 0x2E: fprintf(out, "SET_PRIM_DEPTH   z=0x%x deltaz=0x%x\n", BITS(buf[0], 16, 31), BITS(buf[1], 0, 15)); return;
    case 0x37: fprintf(out, "SET_FILL_COLOR   rgba16=(%d,%d,%d,%d) rgba32=(%d,%d,%d,%d)\n",
                            BITS(buf[0], 11, 15), BITS(buf[0], 6, 10), BITS(buf[0], 1, 5), BITS(buf[0], 0, 0),
                            BITS(buf[0], 24, 31), BITS(buf[0], 16, 23), BITS(buf[0], 8, 15), BITS(buf[0], 0, 7)); return;
    case 0x38: fprintf(out, "SET_FOG_COLOR    rgba32=(%d,%d,%d,%d)\n", BITS(buf[0], 24, 31), BITS(buf[0], 16, 23), BITS(buf[0], 8, 15), BITS(buf[0], 0, 7)); return;
    case 0x39: fprintf(out, "SET_BLEND_COLOR  rgba32=(%d,%d,%d,%d)\n", BITS(buf[0], 24, 31), BITS(buf[0], 16, 23), BITS(buf[0], 8, 15), BITS(buf[0], 0, 7)); return;
    case 0x3A: fprintf(out, "SET_PRIM_COLOR   rgba32=(%d,%d,%d,%d)\n", BITS(buf[0], 24, 31), BITS(buf[0], 16, 23), BITS(buf[0], 8, 15), BITS(buf[0], 0, 7)); return;
    case 0x3B: fprintf(out, "SET_ENV_COLOR    rgba32=(%d,%d,%d,%d)\n", BITS(buf[0], 24, 31), BITS(buf[0], 16, 23), BITS(buf[0], 8, 15), BITS(buf[0], 0, 7)); return;
    case 0x2F: { fprintf(out, "SET_OTHER_MODES  ");
        static const char* cyc[] = { "1cyc", "2cyc", "copy", "fill" };
        static const char* texinterp[] = { "point", "point", "bilinear", "median" };
        static const char* yuv1[] = { "yuv1", "yuv1_tex0" };
        static const char* zmode[] = { "opaque", "inter", "trans", "decal" };
        static const char* rgbdither[] = { "square", "bayer", "noise", "none" };
        static const char* alphadither[] = { "pat", "inv", "noise", "none" };
        static const char* cvgmode[] = { "clamp", "wrap", "zap", "save" };
        static const char* blend1_a[] = { "in", "mem", "blend", "fog" };
        static const char* blend1_b1[] = { "in.a", "fog.a", "shade.a", "0" };
        static const char* blend1_b1inv[] = { "(1-in.a)", "(1-fog.a)", "(1-shade.a)", "1" };
        static const char* blend1_b2[] = { "", "mem.a", "1", "0" };
        static const char* blend2_a[] = { "cyc1", "mem", "blend", "fog" };
        static const char* blend2_b1[] = { "in.a", "fog.a", "shade.a", "0" };
        static const char* blend2_b1inv[] = { "(1-in.a)", "(1-fog.a)", "(1-shade.a)", "1" };
        static const char* blend2_b2[] = { "", "mem.a", "1", "0" };
        setothermodes_t som = decode_som(buf[0]);

        fprintf(out, "%s", cyc[som.cycle_type]);
        if((som.cycle_type < 2) && (som.tex.persp || som.tex.detail || som.tex.sharpen || som.tex.lod || som.sample_type != 0 || som.tf_mode != 6)) {
            fprintf(out, " tex=["); FLAG_RESET();
            FLAG(som.tex.persp, "persp"); FLAG(som.tex.detail, "detail"); FLAG(som.tex.sharpen, "sharpen"); FLAG(som.tex.lod, "lod"); 
            FLAG(!(som.tf_mode & 4), "yuv0"); FLAG(!(som.tf_mode & 2), yuv1[som.tf_mode&1]); 
            FLAG(som.sample_type != 0, texinterp[som.sample_type]);
            fprintf(out, "]");
        }
        if(som.tlut.enable) fprintf(out, " tlut%s", som.tlut.type ? "=[ia]" : "");
        if(BITS(buf[0], 16, 31)) {
            if (som.blender[0].p==0 && som.blender[0].a==0 && som.blender[0].q==0 && som.blender[0].b==0)
                fprintf(out, " blend=[<passthrough>, ");
            else            
                fprintf(out, " blend=[%s*%s + %s*%s, ",
                    blend1_a[som.blender[0].p],  blend1_b1[som.blender[0].a], blend1_a[som.blender[0].q], som.blender[0].b ? blend1_b2[som.blender[0].b] : blend1_b1inv[som.blender[0].a]);
            fprintf(out, "%s*%s + %s*%s]",
                blend2_a[som.blender[1].p],  blend2_b1[som.blender[1].a], blend2_a[som.blender[1].q], som.blender[1].b ? blend2_b2[som.blender[1].b] : blend2_b1inv[som.blender[1].a]);
        }
        if(som.z.upd || som.z.cmp || som.z.prim) {
            fprintf(out, " z=["); FLAG_RESET();
            FLAG(som.z.cmp, "cmp"); FLAG(som.z.upd, "upd"); FLAG(som.z.prim, "prim"); FLAG(true, zmode[som.z.mode]);
            fprintf(out, "]");
        }
        flag_prefix = " ";
        FLAG(som.aa, "aa"); FLAG(som.read, "read"); FLAG(som.blend, "blend");
        FLAG(som.chromakey, "chroma_key"); FLAG(som.atomic, "atomic");

        if(som.alphacmp.enable) fprintf(out, " alpha_compare%s", som.alphacmp.noise ? "[noise]" : "");
        if((som.cycle_type < 2) && (som.dither.rgb != 3 || som.dither.alpha != 3)) fprintf(out, " dither=[%s,%s]", rgbdither[som.dither.rgb], alphadither[som.dither.alpha]);
        if(som.cvg.mode || som.cvg.color || som.cvg.sel_alpha || som.cvg.mul_alpha) {
            fprintf(out, " cvg=["); FLAG_RESET();
            FLAG(som.cvg.mode, cvgmode[som.cvg.mode]); FLAG(som.cvg.color, "color_ovf"); 
            FLAG(som.cvg.mul_alpha, "mul_alpha"); FLAG(som.cvg.sel_alpha, "sel_alpha");
            fprintf(out, "]");
        }
        if(som.rdpqx.bl2 || som.rdpqx.freeze || som.rdpqx.fog) {
            fprintf(out, " rdpq=["); FLAG_RESET();
            FLAG(som.rdpqx.bl2, "bl2"); FLAG(som.rdpqx.freeze, "freeze"); 
            FLAG(som.rdpqx.fog, "fog");
            fprintf(out, "]");
        }
        fprintf(out, "\n");
    }; return;
    case 0x3C: { fprintf(out, "SET_COMBINE_MODE ");
        static const char* rgb_suba[16] = {"comb", "tex0", "tex1", "prim", "shade", "env", "1", "noise", "0","0","0","0","0","0","0","0"};
        static const char* rgb_subb[16] = {"comb", "tex0", "tex1", "prim", "shade", "env", "keycenter", "k4", "0","0","0","0","0","0","0","0"};
        static const char* rgb_mul[32] = {"comb", "tex0", "tex1", "prim", "shade", "env", "keyscale", "comb.a", "tex0.a", "tex1.a", "prim.a", "shade.a", "env.a", "lod_frac", "prim_lod_frac", "k5", "0","0","0","0","0","0","0","0", "0","0","0","0","0","0","0","0"};
        static const char* rgb_add[8] = {"comb", "tex0", "tex1", "prim", "shade", "env", "1", "0"};
        static const char* alpha_addsub[8] = {"comb", "tex0", "tex1", "prim", "shade", "env", "1", "0"};
        static const char* alpha_mul[8] = {"lod_frac", "tex0", "tex1", "prim", "shade", "env", "prim_lod_frac", "0"};
        colorcombiner_t cc = decode_cc(buf[0]);
        fprintf(out, "cyc0=[(%s-%s)*%s+%s, (%s-%s)*%s+%s], ",
            rgb_suba[cc.cyc[0].rgb.suba], rgb_subb[cc.cyc[0].rgb.subb], rgb_mul[cc.cyc[0].rgb.mul], rgb_add[cc.cyc[0].rgb.add],
            alpha_addsub[cc.cyc[0].alpha.suba], alpha_addsub[cc.cyc[0].alpha.subb], alpha_mul[cc.cyc[0].alpha.mul], alpha_addsub[cc.cyc[0].alpha.add]);
        const struct cc_cycle_s passthrough = {0};
        if (!__builtin_memcmp(&cc.cyc[1], &passthrough, sizeof(struct cc_cycle_s))) fprintf(out, "cyc1=[<passthrough>]\n");
        else fprintf(out, "cyc1=[(%s-%s)*%s+%s, (%s-%s)*%s+%s]\n",
            rgb_suba[cc.cyc[1].rgb.suba], rgb_subb[cc.cyc[1].rgb.subb], rgb_mul[cc.cyc[1].rgb.mul], rgb_add[cc.cyc[1].rgb.add],
            alpha_addsub[cc.cyc[1].alpha.suba], alpha_addsub[cc.cyc[1].alpha.subb],   alpha_mul[cc.cyc[1].alpha.mul], alpha_addsub[cc.cyc[1].alpha.add]);
    } return;
    case 0x35: { fprintf(out, "SET_TILE         ");
        uint8_t f = BITS(buf[0], 53, 55);
        fprintf(out, "tile=%d %s%s tmem[0x%x,line=%d]", 
            BITS(buf[0], 24, 26), fmt[f], size[BITS(buf[0], 51, 52)],
            BITS(buf[0], 32, 40)*8, BITS(buf[0], 41, 49)*8);
        if (f==2) fprintf(out, " pal=%d", BITS(buf[0], 20, 23));
        fprintf(out, " mask=[%d, %d]", 1<<BITS(buf[0], 4, 7), 1<<BITS(buf[0], 14, 17));
        bool clamp = BIT(buf[0], 19) || BIT(buf[0], 9);
        bool mirror = BIT(buf[0], 18) || BIT(buf[0], 8);
        if (clamp) {
            fprintf(out, " clamp=["); FLAG_RESET();
            FLAG(BIT(buf[0], 9), "s"); FLAG(BIT(buf[0], 19), "t");
            fprintf(out, "]");
        }
        if (mirror) {
            fprintf(out, " mirror=["); FLAG_RESET();
            FLAG(BIT(buf[0], 8), "s"); FLAG(BIT(buf[0], 18), "t");
            fprintf(out, "]");
        }
        if (BITS(buf[0], 0, 3) || BITS(buf[0], 10, 13))
            fprintf(out, " shift=[%d, %d]", ((BITS(buf[0],0,3)+5)&15)-5, ((BITS(buf[0], 10, 13)+5)&15)-5);
        fprintf(out, "\n");
    } return;
    case 0x24 ... 0x25:
        if(CMD(buf[0]) == 0x24)
            fprintf(out, "TEX_RECT         ");
        else
            fprintf(out, "TEX_RECT_FLIP    ");
        fprintf(out, "tile=%d xy=(%.2f,%.2f)-(%.2f,%.2f)\n", BITS(buf[0], 24, 26),
            BITS(buf[0], 12, 23)*FX(2), BITS(buf[0], 0, 11)*FX(2), BITS(buf[0], 44, 55)*FX(2), BITS(buf[0], 32, 43)*FX(2));
        fprintf(out, "[%p] %016" PRIx64 "                     ", &addr[1], buf[1]);
        fprintf(out, "st=(%.2f,%.2f) dst=(%.5f,%.5f)\n",
            SBITS(buf[1], 48, 63)*FX(5), SBITS(buf[1], 32, 47)*FX(5), SBITS(buf[1], 16, 31)*FX(10), SBITS(buf[1], 0, 15)*FX(10));
        return;
    case 0x32: case 0x34:
        if(CMD(buf[0]) == 0x32)
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
        int cmd = CMD(buf[0])-0x8;
        fprintf(out, "%-17s", tri_name[cmd]);
        fprintf(out, "%s tile=%d lvl=%d y=(%.2f, %.2f, %.2f)\n",
            BITS(buf[0], 55, 55) ? "left" : "right", BITS(buf[0], 48, 50), BITS(buf[0], 51, 53)+1,
            SBITS(buf[0], 0, 13)*FX(2), SBITS(buf[0], 16, 29)*FX(2), SBITS(buf[0], 32, 45)*FX(2));
        fprintf(out, "[%p] %016" PRIx64 "                     xl=%.4f isl=%.4f\n", &addr[1], buf[1],
            SBITS(buf[1], 32, 63)*FX(16), SBITS(buf[1], 0, 31)*FX(16));
        fprintf(out, "[%p] %016" PRIx64 "                     xh=%.4f ish=%.4f\n", &addr[2], buf[2],
            SBITS(buf[2], 32, 63)*FX(16), SBITS(buf[2], 0, 31)*FX(16));
        fprintf(out, "[%p] %016" PRIx64 "                     xm=%.4f ism=%.4f\n", &addr[3], buf[3],
            SBITS(buf[3], 32, 63)*FX(16), SBITS(buf[3], 0, 31)*FX(16));
        int i=4;
        if (cmd & 0x4) {
            fprintf(out, "[%p] %016" PRIx64 "                     r=%.5f g=%.5f b=%.5f a=%.5f\n", &addr[i], buf[i],
                FX32(BITS(buf[i], 48, 63), BITS(buf[i+2], 48, 63)),
                FX32(BITS(buf[i], 32, 47), BITS(buf[i+2], 32, 47)),
                FX32(BITS(buf[i], 16, 31), BITS(buf[i+2], 16, 31)),
                FX32(BITS(buf[i],  0, 15), BITS(buf[i+2],  0, 15))); i++;
            fprintf(out, "[%p] %016" PRIx64 "                     drdx=%.5f dgdx=%.5f dbdx=%.5f dadx=%.5f\n", &buf[i], buf[i],
                FX32(BITS(buf[i], 48, 63), BITS(buf[i+2], 48, 63)),
                FX32(BITS(buf[i], 32, 47), BITS(buf[i+2], 32, 47)),
                FX32(BITS(buf[i], 16, 31), BITS(buf[i+2], 16, 31)),
                FX32(BITS(buf[i],  0, 15), BITS(buf[i+2],  0, 15))); i++;
            fprintf(out, "[%p] %016" PRIx64 "                     \n", &addr[i], buf[i]); i++;
            fprintf(out, "[%p] %016" PRIx64 "                     \n", &addr[i], buf[i]); i++;
            fprintf(out, "[%p] %016" PRIx64 "                     drde=%.5f dgde=%.5f dbde=%.5f dade=%.5f\n", &addr[i], buf[i],
                FX32(BITS(buf[i], 48, 63), BITS(buf[i+2], 48, 63)),
                FX32(BITS(buf[i], 32, 47), BITS(buf[i+2], 32, 47)),
                FX32(BITS(buf[i], 16, 31), BITS(buf[i+2], 16, 31)),
                FX32(BITS(buf[i],  0, 15), BITS(buf[i+2],  0, 15))); i++;
            fprintf(out, "[%p] %016" PRIx64 "                     drdy=%.5f dgdy=%.5f dbdy=%.5f dady=%.5f\n", &addr[i], buf[i],
                FX32(BITS(buf[i], 48, 63), BITS(buf[i+2], 48, 63)),
                FX32(BITS(buf[i], 32, 47), BITS(buf[i+2], 32, 47)),
                FX32(BITS(buf[i], 16, 31), BITS(buf[i+2], 16, 31)),
                FX32(BITS(buf[i],  0, 15), BITS(buf[i+2],  0, 15))); i++;
            fprintf(out, "[%p] %016" PRIx64 "                     \n", &addr[i], buf[i]); i++;
            fprintf(out, "[%p] %016" PRIx64 "                     \n", &addr[i], buf[i]); i++;
        }
        if (cmd & 0x2) {
            fprintf(out, "[%p] %016" PRIx64 "                     s=%.5f t=%.5f w=%.5f\n", &addr[i], buf[i],
                FX32(BITS(buf[i], 48, 63), BITS(buf[i+2], 48, 63)), 
                FX32(BITS(buf[i], 32, 47), BITS(buf[i+2], 32, 47)),
                FX32(BITS(buf[i], 16, 31), BITS(buf[i+2], 16, 31))); i++;
            fprintf(out, "[%p] %016" PRIx64 "                     dsdx=%.5f dtdx=%.5f dwdx=%.5f\n", &addr[i], buf[i],
                FX32(BITS(buf[i], 48, 63), BITS(buf[i+2], 48, 63)), 
                FX32(BITS(buf[i], 32, 47), BITS(buf[i+2], 32, 47)),
                FX32(BITS(buf[i], 16, 31), BITS(buf[i+2], 16, 31))); i++;
            fprintf(out, "[%p] %016" PRIx64 "                     \n", &addr[i], buf[i]); i++;
            fprintf(out, "[%p] %016" PRIx64 "                     \n", &addr[i], buf[i]); i++;
            fprintf(out, "[%p] %016" PRIx64 "                     dsde=%.5f dtde=%.5f dwde=%.5f\n", &addr[i], buf[i],
                FX32(BITS(buf[i], 48, 63), BITS(buf[i+2], 48, 63)), 
                FX32(BITS(buf[i], 32, 47), BITS(buf[i+2], 32, 47)),
                FX32(BITS(buf[i], 16, 31), BITS(buf[i+2], 16, 31))); i++;
            fprintf(out, "[%p] %016" PRIx64 "                     dsdy=%.5f dtdy=%.5f dwdy=%.5f\n", &addr[i], buf[i],
                FX32(BITS(buf[i], 48, 63), BITS(buf[i+2], 48, 63)), 
                FX32(BITS(buf[i], 32, 47), BITS(buf[i+2], 32, 47)),
                FX32(BITS(buf[i], 16, 31), BITS(buf[i+2], 16, 31))); i++;
            fprintf(out, "[%p] %016" PRIx64 "                     \n", &addr[i], buf[i]); i++;
            fprintf(out, "[%p] %016" PRIx64 "                     \n", &addr[i], buf[i]); i++;
        }
        if (cmd & 0x1) {
            fprintf(out, "[%p] %016" PRIx64 "                     z=%.5f dzdx=%.5f\n", &addr[i], buf[i],
                FX32(BITS(buf[i], 48, 63), BITS(buf[i], 32, 47)), 
                FX32(BITS(buf[i], 16, 31), BITS(buf[i],  0, 15))); i++;
            fprintf(out, "[%p] %016" PRIx64 "                     dzde=%.5f dzdy=%.5f\n", &addr[i], buf[i],
                FX32(BITS(buf[i], 48, 63), BITS(buf[i], 32, 47)), 
                FX32(BITS(buf[i], 16, 31), BITS(buf[i],  0, 15))); i++;
        }
        return;
    }
    case 0x3e: {
        fprintf(out, "SET_Z_IMAGE      ");
        uint32_t addr = BITS(buf[0], 0, 25);
        if (addr == RDPQ_VALIDATE_DETACH_ADDR) fprintf(out, "<detach>\n");
        else fprintf(out, "dram=%08" PRIx32 "\n", addr);
    }   return;
    case 0x3d: fprintf(out, "SET_TEX_IMAGE    dram=%08x w=%d %s%s\n", 
        BITS(buf[0], 0, 25), BITS(buf[0], 32, 41)+1, fmt[BITS(buf[0], 53, 55)], size[BITS(buf[0], 51, 52)]);
        return;
    case 0x3f: {
        fprintf(out, "SET_COLOR_IMAGE  ");
        uint32_t addr = BITS(buf[0], 0, 25);
        if (addr == RDPQ_VALIDATE_DETACH_ADDR) fprintf(out, "<detach>\n");
        else {
            fprintf(out, "dram=%08" PRIx32 " w=%d ", addr, BITS(buf[0], 32, 41)+1);        
            int height = BITS(buf[0], 42, 50) | (BIT(buf[0], 31) << 9);
            if (height) fprintf(out, "h=%d ", height+1); // libdragon extension
            fprintf(out, "%s%s\n", fmt[BITS(buf[0], 53, 55)], size[BITS(buf[0], 51, 52)]);
        }
    }   return;
    case 0x31: switch(BITS(buf[0], 48, 55)) {
        case 0x01: fprintf(out, "RDPQ_SHOWLOG     show=%d\n", BIT(buf[0], 0)); return;
        #ifdef N64
        case 0x02: fprintf(out, "RDPQ_MESSAGE     %s\n", (char*)CachedAddr(0x80000000|BITS(buf[0], 0, 24))); return;
        #endif
        default:   fprintf(out, "RDPQ_DEBUG       <unkwnown>\n"); return;
    }
    }
}

static bool log_coalesce_tris(uint8_t cmd, uint8_t *last_tri_cmd, int *num_tris) {
    if (!CMD_IS_TRI(cmd)) {
        if (*last_tri_cmd) {
            debugf("[..........] ................    %-16s num_cmds=%d\n", tri_name[*last_tri_cmd - 0x08], *num_tris);
            *last_tri_cmd = 0;
            *num_tris = 0;
        }
        return true;
    } else {
        if (*last_tri_cmd && *last_tri_cmd != cmd) {
            debugf("[..........] ................    %-16s num_cmds=%d\n", tri_name[*last_tri_cmd - 0x08], *num_tris);
            *last_tri_cmd = 0;
            *num_tris = 0;
        }
        *last_tri_cmd = cmd;
        *num_tris = *num_tris+1;
        return false;
    }
}


bool rdpq_debug_disasm(uint64_t *buf, FILE *out) {
    static uint8_t last_tri_cmd = 0; static int num_tris = 0;
 
    if (buf) {
        uint8_t cmd = BITS(buf[0],56,61);
        if ((__rdpq_debug_log_flags & RDPQ_LOG_FLAG_SHOWTRIS) || log_coalesce_tris(cmd, &last_tri_cmd, &num_tris)) {
            __rdpq_debug_disasm(buf, buf, out);
            return true;
        }
    } else {
        log_coalesce_tris(0, &last_tri_cmd, &num_tris);
    }
    return false;
}

#define EMIT_TYPE         0x3                 ///< Type of message (mask)
#define EMIT_CRASH        0x0                 ///< Message is a RDP crash
#define EMIT_ERROR        0x1                 ///< Message is an error
#define EMIT_WARN         0x2                 ///< Message is a warning

#define EMIT_CTX_SOM      0x4                 ///< Message context must show last SOM
#define EMIT_CTX_CC       0x8                 ///< Message context must show last CC
#define EMIT_CTX_TEX      0x10                ///< Message context must show last SET_TEX_IMAGE
#define EMIT_CTX_TILES    (0xFF << 5)         ///< Message context must show SET_TILE (mask)
#define EMIT_CTX_TILE(n)  (0x20 << (n))       ///< Message context must show tile n
#define EMIT_CTX_TILESIZE 0x2000              ///< Message context must show LOAD_TILE/SET_TILE_SIZE instead of SET_TILE

__attribute__((format(printf, 2, 3)))
static void validate_emit_error(int flags, const char *msg, ...)
{
    va_list args;

    if (!(vctx.flags & RDPQ_VALIDATE_FLAG_NOECHO)) {
        if (flags & EMIT_CTX_SOM) __rdpq_debug_disasm(rdp.last_som, &rdp.last_som_data, stderr);
        if (flags & EMIT_CTX_CC)  __rdpq_debug_disasm(rdp.last_cc,  &rdp.last_cc_data,  stderr);
        if (flags & EMIT_CTX_TEX) __rdpq_debug_disasm(rdp.last_tex, &rdp.last_tex_data, stderr);
        if (flags & EMIT_CTX_TILES) {
            for (int i = 0; i < 8; i++) {
                if (flags & EMIT_CTX_TILE(i)) {
                    __rdpq_debug_disasm(rdp.tile[i].last_settile, &rdp.tile[i].last_settile_data, stderr);
                    if (rdp.tile[i].has_extents)
                        __rdpq_debug_disasm(rdp.tile[i].last_setsize, &rdp.tile[i].last_setsize_data, stderr);
                    break;
                }
            }
        }
        rdpq_debug_disasm(vctx.buf, stderr);
    }

    switch (flags & EMIT_TYPE) {
    case EMIT_CRASH:
        fprintf(stderr, "[RDPQ_VALIDATION] CRASH: ");
        vctx.crashed = true;
        vctx.errs += 1;
        break;
    case EMIT_ERROR:
        fprintf(stderr, "[RDPQ_VALIDATION] ERROR: ");
        vctx.errs += 1;
        break;
    case EMIT_WARN:
        fprintf(stderr, "[RDPQ_VALIDATION] WARN:  ");
        vctx.warns += 1;
        break;
    }

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    if ((flags & EMIT_TYPE) == EMIT_CRASH)
        fprintf(stderr, "[RDPQ_VALIDATION]        This is a fatal error: a real RDP chip would stop working until reboot\n");

    if (flags & EMIT_CTX_SOM) fprintf(stderr, "[RDPQ_VALIDATION]        SET_OTHER_MODES last sent at %p\n", rdp.last_som);
    if (flags & EMIT_CTX_CC)  fprintf(stderr, "[RDPQ_VALIDATION]        SET_COMBINE_MODE last sent at %p\n", rdp.last_cc);
    if (flags & EMIT_CTX_TEX) fprintf(stderr, "[RDPQ_VALIDATION]        SET_TEX_IMAGE last sent at %p\n", rdp.last_tex);
    if (flags & EMIT_CTX_TILES) {
        for (int i = 0; i < 8; i++) {
            if (flags & EMIT_CTX_TILE(i)) {
                if (flags & EMIT_CTX_TILESIZE)
                    fprintf(stderr, "[RDPQ_VALIDATION]        %s last sent at %p\n",
                        CMD(rdp.tile[i].last_setsize_data) == 0x32 ? "SET_TILE_SIZE" : "LOAD_TILE",
                        rdp.tile[i].last_setsize);
                else
                    fprintf(stderr, "[RDPQ_VALIDATION]        SET_TILE last sent at %p\n", rdp.tile[i].last_settile);
                break;
            }
        }
    }

    #ifdef N64
    // On a real N64, let's assert on RDP crashes. This makes them very visible to everybody,
    // including people that don't have the debugging log on.
    // We just dump the message here, more information are in the log.
    if ((flags & EMIT_TYPE) == EMIT_CRASH) {
        char buf[1024];
        va_start(args, msg);
        vsprintf(buf, msg, args);
        va_end(args);
        assertf(0, "RDP CRASHED: the code triggered a RDP hardware bug.\n%s", buf);
    }
    #endif
}

/** @brief Internal validation macros (for both errors and warnings) */
#define __VALIDATE(flags, cond, msg, ...) ({ \
    if (!(cond)) validate_emit_error(flags, msg "\n", ##__VA_ARGS__); \
})

/** 
 * @brief Check and trigger a RDP crash.
 * 
 * This is the most fatal error condition, in which the RDP chip freezes and stop processing
 * commands until reboot.
 */
#define VALIDATE_CRASH(cond, msg, ...)      __VALIDATE(0, cond, msg, ##__VA_ARGS__)
/** @brief Validate and trigger a crash, with SOM context */
#define VALIDATE_CRASH_SOM(cond, msg, ...)  __VALIDATE(4, cond, msg, ##__VA_ARGS__)
/** @brief Validate and trigger a crash, with CC context */
#define VALIDATE_CRASH_CC(cond, msg, ...)   __VALIDATE(8, cond, msg, ##__VA_ARGS__)
/** @brief Validate and trigger a crash, with SET_TEX_IMAGE context */
#define VALIDATE_CRASH_TEX(cond, msg, ...)  __VALIDATE(16, cond, msg, ##__VA_ARGS__)
/** @brief Validate and trigger a crash, with tile context */
#define VALIDATE_CRASH_TILE(cond, tidx, msg, ...)  __VALIDATE(EMIT_CRASH | EMIT_CTX_TILE(tidx), cond, msg, ##__VA_ARGS__)
/** @brief Validate and trigger a crash, with tile extents context */
#define VALIDATE_CRASH_TILESIZE(cond, tidx, msg, ...)  __VALIDATE(EMIT_CRASH | EMIT_CTX_TILE(tidx) | EMIT_CTX_TILESIZE, cond, msg, ##__VA_ARGS__)

/** 
 * @brief Check and trigger a RDP validation error. 
 * 
 * This should be triggered only whenever the commands rely on an undefined hardware 
 * behaviour or in general strongly misbehave with respect to the reasonable
 * expectation of the programmer. Typical expected outcome on real hardware should be
 * garbled graphcis. */
#define VALIDATE_ERR(cond, msg, ...)      __VALIDATE(1, cond, msg, ##__VA_ARGS__)
/** @brief Validate and trigger an error, with SOM context */
#define VALIDATE_ERR_SOM(cond, msg, ...)  __VALIDATE(5, cond, msg, ##__VA_ARGS__)
/** @brief Validate and trigger an error, with CC context */
#define VALIDATE_ERR_CC(cond, msg, ...)   __VALIDATE(9, cond, msg, ##__VA_ARGS__)
/** @brief Validate and trigger an error, with SET_TEX_IMAGE context */
#define VALIDATE_ERR_TEX(cond, msg, ...)  __VALIDATE(17, cond, msg, ##__VA_ARGS__)
/** @brief Validate and trigger an error, with tile context */
#define VALIDATE_ERR_TILE(cond, tidx, msg, ...)  __VALIDATE(EMIT_ERROR | EMIT_CTX_TILE(tidx), cond, msg, ##__VA_ARGS__)
/** @brief Validate and trigger an error, with tile extents context */
#define VALIDATE_ERR_TILESIZE(cond, tidx, msg, ...)  __VALIDATE(EMIT_ERROR | EMIT_CTX_TILE(tidx) | EMIT_CTX_TILESIZE, cond, msg, ##__VA_ARGS__)

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
#define VALIDATE_WARN(cond, msg, ...)      __VALIDATE(2, cond, msg, ##__VA_ARGS__)
/** @brief Validate and trigger a warning, with SOM context */
#define VALIDATE_WARN_SOM(cond, msg, ...)  __VALIDATE(6, cond, msg, ##__VA_ARGS__)
/** @brief Validate and trigger a warning, with CC context */
#define VALIDATE_WARN_CC(cond, msg, ...)   __VALIDATE(10, cond, msg, ##__VA_ARGS__)
/** @brief Validate and trigger a warning, with SET_TEX_IMAGE context */
#define VALIDATE_WARN_TEX(cond, msg, ...)  __VALIDATE(18, cond, msg, ##__VA_ARGS__)
/** @brief Validate and trigger an error, with tile context */
#define VALIDATE_WARN_TILE(cond, tidx, msg, ...)  __VALIDATE(EMIT_WARN | EMIT_CTX_TILE(tidx), cond, msg, ##__VA_ARGS__)
/** @brief Validate and trigger a warning, with tile extents context */
#define VALIDATE_WARN_TILESIZE(cond, tidx, msg, ...)  __VALIDATE(EMIT_WARN | EMIT_CTX_TILE(tidx) | EMIT_CTX_TILESIZE, cond, msg, ##__VA_ARGS__)

/** 
 * @brief Perform lazy evaluation of render target changes (color buffer and scissoring).
 */
static void lazy_validate_rendertarget(void) {
    if (!rdp.rendertarget_changed) return;
    rdp.rendertarget_changed = false;

    VALIDATE_ERR(rdp.last_col,
        "undefined behavior: drawing command before a SET_COLOR_IMAGE was sent");
    VALIDATE_ERR(rdp.sent_scissor,
        "undefined behavior: drawing command before a SET_SCISSOR was sent");
    if (!rdp.last_col || !rdp.sent_scissor) return;

    // copy/fill mode use inclusive X coordinates for most things, including scissor
    int x1 = rdp.clip.x1;
    if (rdp.som.cycle_type >= 2) x1++;

    VALIDATE_WARN(rdp.clip.x0 < x1,
        "drawing command with null scissor rectangle (X:%d-%d)", rdp.clip.x0, rdp.clip.x1);
    VALIDATE_WARN(rdp.clip.y0 < rdp.clip.y1,
        "drawing command with null scissor rectangle (Y:%d-%d)", rdp.clip.y0, rdp.clip.y1);
    VALIDATE_WARN(rdp.clip.x1 <= rdp.col.width,
        "drawing command with scissor rectangle (X1=%d) outside of color buffer (W=%d)", rdp.clip.x1, rdp.col.width);
    if (rdp.col.height > 1) { // libdragon extension
        VALIDATE_WARN(rdp.clip.y1 <= rdp.col.height,
            "drawing command with scissor rectangle (Y1=%d) outside of color buffer (H=%d)", rdp.clip.y1, rdp.col.height);
    }
    if (rdp.som.cycle_type == 2) {
        VALIDATE_CRASH(rdp.clip.x0  == 0,
            "drawing command in COPY mode: scissor left bound (%d) must be zero", rdp.clip.x0);
    }
    if (rdp.som.cycle_type == 3) {
        VALIDATE_ERR(rdp.clip.x0 % 4 == 0,
            "drawing command in FILL mode: scissor rectangle x0 (%d) must be a multiple of 4", rdp.clip.x0);
    }
}

/** @brief Non-zero if the current CC uses the TEX0 slot aka the first texture in 1cycle mode */
static int cc_1cyc_use_tex0(void) {
    struct cc_cycle_s *cc = rdp.cc.cyc;

    int ret = 0;
    // Cycle1: reference to TEX0/TEX0_ALPHA slot
    if (cc[1].rgb.suba == 1 || cc[1].rgb.subb == 1 || cc[1].rgb.mul == 1 || cc[1].rgb.mul == 8 || cc[1].rgb.add == 1)
        ret |= 1;
    if (cc[1].alpha.suba == 1 || cc[1].alpha.subb == 1 || cc[1].alpha.mul == 1 || cc[1].alpha.add == 1)
        ret |= 1;
    return ret;
}

/** @brief Non-zero if the current CC uses the TEX0 slot aka the first texture, in 2cycle mode */
static int cc_2cyc_use_tex0(void) {
    struct cc_cycle_s *cc = rdp.cc.cyc;

    int ret = 0;
    // Cycle0: reference to TEX0/TEX0_ALPHA slot
    if (cc[0].rgb.suba == 1 || cc[0].rgb.subb == 1 || cc[0].rgb.mul == 1 || cc[0].rgb.mul == 8 || cc[0].rgb.add == 1)
        ret |= 1;
    if (cc[0].alpha.suba == 1 || cc[0].alpha.subb == 1 || cc[0].alpha.mul == 1 || cc[0].alpha.add == 1)
        ret |= 1;
    // Cycle1: reference to TEX1/TEX1_ALPHA slot (which actually points to TEX0)
    if (cc[1].rgb.suba == 2 || cc[1].rgb.subb == 2 || cc[1].rgb.mul == 2 || cc[1].rgb.mul == 9 || cc[1].rgb.add == 2)
        ret |= 2;
    if (cc[1].alpha.suba == 2 || cc[1].alpha.subb == 2 || cc[1].alpha.mul == 2 || cc[1].alpha.add == 2)
        ret |= 2;
    return ret;
}

/** @brief Non-zoer if the current CC uses the TEX1 slot aka the second texture, in 2cycle mode */
static int cc_2cyc_use_tex1(void) {
    struct cc_cycle_s *cc = rdp.cc.cyc;
    if ((rdp.som.tf_mode & 3) == 1) // TEX1 is the color-conversion of TEX0, so TEX1 is not used
        return false;
    int ret = 0;
    // Cycle0: reference to TEX1/TEX1_ALPHA slot
    if (cc[0].rgb.suba == 2 || cc[0].rgb.subb == 2 || cc[0].rgb.mul == 2 || cc[0].rgb.mul == 9 || cc[0].rgb.add == 2)
        ret |= 1;
    if (cc[0].alpha.suba == 2 || cc[0].alpha.subb == 2 || cc[0].alpha.mul == 2 || cc[0].alpha.add == 2)
        ret |= 1;
    // Cycle1: reference to TEX0/TEX0_ALPHA slot (which actually points to TEX1)
    if (cc[1].rgb.suba == 1 || cc[1].rgb.subb == 1 || cc[1].rgb.mul == 1 || cc[1].rgb.mul == 8 || cc[1].rgb.add == 1)
        ret |= 2;
    if (cc[1].alpha.suba == 1 || cc[1].alpha.subb == 1 || cc[1].alpha.mul == 1 || cc[1].alpha.add == 1)
        ret |= 2;
    return ret;
}


/** 
 * @brief Perform lazy evaluation of SOM and CC changes (on draw command).
 * 
 * Validation of color combiner requires to know the current cycle type (which is part of SOM).
 * Since it's possible to send SOM / CC in any order, what matters is if, at the point of a
 * drawing command, the configuration is correct.
 * 
 * Validation of CC is thus run lazily whenever a draw command is issued.
 * 
 * @note Do not perform validation of texture-related settings here. Use validate_use_tile instead,
 *       as that is the only place where we know exactly which tile is being used for drawing.
 */
static void lazy_validate_rendermode(void) {
    if (!rdp.mode_changed) return;
    rdp.mode_changed = false;

    // Fill mode validation
    if (rdp.som.cycle_type == 3) {
        if (rdp.last_col) {
            VALIDATE_CRASH_SOM(rdp.col.size != 0, "FILL mode not supported on 4-bit framebuffers");   
        }
        // These are a bunch of SOM settings that, in addition of being useless in FILL mode, they cause
        // a RDP crash.
        VALIDATE_CRASH_SOM(!rdp.som.read, "image read is enabled but is not supported in FILL mode");
        VALIDATE_CRASH_SOM(!rdp.som.z.cmp, "Z buffer compare is enabled but is not supported in FILL mode");
        VALIDATE_CRASH_SOM(!rdp.som.z.upd || rdp.som.z.prim, "Z buffer write is enabled but is not supported in FILL mode");
        return;
    }

    // Copy mode validation
    if (rdp.som.cycle_type == 2) {
        if (rdp.last_col) {
            int size = BITS(rdp.last_col_data, 51, 52);
            VALIDATE_CRASH_SOM(size != 3, "COPY mode not supported on 32-bit framebuffers");   
        }
        VALIDATE_ERR_SOM(!rdp.som.z.cmp, "Z buffer compare is enabled but is not supported in COPY mode");
        VALIDATE_ERR_SOM(!rdp.som.z.upd || rdp.som.z.prim, "Z buffer write is enabled but is not supported in COPY mode");
        VALIDATE_ERR_SOM(!rdp.som.tex.persp, "perspective correction is not supported in COPY mode");
        return;
    }

    // We are in 1-cycle/2-cycle mode. Proceed to validate blender and color combiner.

    // Validate blender setting. If there is any blender fomula configured, we should
    // expect one between SOM_BLENDING or SOM_ANTIALIAS, otherwise the formula will be ignored.
    struct blender_s *b0 = &rdp.som.blender[0];
    struct blender_s *b1 = &rdp.som.blender[1];
    bool has_bl0 = b0->p || b0->a || b0->q || b0->b;
    bool has_bl1 = b1->p || b1->a || b1->q || b1->b;
    VALIDATE_WARN_SOM(rdp.som.blend || rdp.som.aa || !(has_bl0 || has_bl1),
        "blender function will be ignored because SOM_BLENDING and SOM_ANTIALIAS are both disabled");
    if (rdp.som.cycle_type == 1) { // 2cyc
        VALIDATE_ERR_SOM((b0->b == 0) || (b0->b == 2 && b0->a == 3),  // INV_MUX_ALPHA, or ONE/ZERO (which still works)
            "in 2cycle mode, the first pass of the blender must use INV_MUX_ALPHA or equivalent");
        VALIDATE_ERR_SOM(b0->a != 1,
            "in 2cycle mode, the first pass of the blender cannot access MEMORY_RGB");
        VALIDATE_WARN_SOM(b1->a != 2,
            "in 2cycle mode, the second pass of the blender will use a SHADE_ALPHA value shifted by one pixel because of a hardware bug");
    }

    // Validate other SOM states
    if (!rdp.som.tex.lod) {
        VALIDATE_ERR_SOM(!rdp.som.tex.sharpen && !rdp.som.tex.detail,
            "sharpen/detail texture require texture LOD to be active");
    }
    if (rdp.som.z.cmp || rdp.som.z.upd) {
        VALIDATE_ERR_SOM(rdp.last_z,
            "Z buffer image not configured but Z buffer mode was requested in SOM");
    }

    if (!rdp.last_cc) {
        VALIDATE_ERR(rdp.last_cc, "SET_COMBINE not called before drawing primitive");
        return;
    }

    // Sanitize color combiner. We are going to check a few slots for specific values
    // but we want to avoid emitting errors for combinations that are "benign". For example,
    // COMBINED in 1cycle mode is an error, but if you do (COMB-COMB), then it doesn't
    // really matter.
    struct cc_cycle_s ccs[2] = { rdp.cc.cyc[0], rdp.cc.cyc[1] };
    for (int i=0; i<2; i++) {
        if (ccs[i].rgb.suba == ccs[i].rgb.subb || ccs[i].rgb.mul == 16)
            ccs[i].rgb.suba = ccs[i].rgb.subb = 8;  // change with 0, so that it doesn't matter
        if (ccs[i].alpha.suba == ccs[i].alpha.subb || ccs[i].alpha.mul == 7)
            ccs[i].alpha.suba = ccs[i].alpha.subb = 7;  // change with 0, so that it doesn't matter
    }

    if (rdp.som.cycle_type == 0) { // 1cyc
        VALIDATE_ERR_CC(ccs[1].rgb.suba != 0 && ccs[1].rgb.subb != 0 && ccs[1].rgb.mul != 0 && ccs[1].rgb.add != 0 &&
                        ccs[1].alpha.suba != 0 && ccs[1].alpha.subb != 0 && ccs[1].alpha.add != 0,
            "in 1cycle mode, the color combiner cannot access the COMBINED slot");
        VALIDATE_ERR_CC(ccs[1].rgb.suba != 2 && ccs[1].rgb.subb != 2 && ccs[1].rgb.mul != 2 && ccs[1].rgb.add != 2 &&
                        ccs[1].alpha.suba != 2 && ccs[1].alpha.subb != 2 && ccs[1].alpha.mul != 2 && ccs[1].alpha.add != 2,
            "in 1cycle mode, the color combiner cannot access the TEX1 slot");
        VALIDATE_ERR_CC(ccs[1].rgb.mul != 7,
            "in 1cycle mode, the color combiner cannot access the COMBINED_ALPHA slot");
        VALIDATE_ERR_CC(ccs[1].rgb.mul != 9,
            "in 1cycle mode, the color combiner cannot access the TEX1_ALPHA slot");
    } else { // 2 cyc
        VALIDATE_ERR_CC(ccs[0].rgb.suba != 0 && ccs[0].rgb.subb != 0 && ccs[0].rgb.mul != 0 && ccs[0].rgb.add != 0 &&
                        ccs[0].alpha.suba != 0 && ccs[0].alpha.subb != 0 && ccs[0].alpha.add != 0,
            "in 2cycle mode, the color combiner cannot access the COMBINED slot in the first cycle");
        VALIDATE_ERR_CC(ccs[1].rgb.suba != 2 && ccs[1].rgb.subb != 2 && ccs[1].rgb.mul != 2 && ccs[1].rgb.add != 2 &&
                        ccs[1].alpha.suba != 2 && ccs[1].alpha.subb != 2 && ccs[1].alpha.mul != 2 && ccs[1].alpha.add != 2,
            "in 2cycle mode, the color combiner cannot access the TEX1 slot in the second cycle (but TEX0 contains the second texture)");
        VALIDATE_ERR_CC(ccs[0].rgb.mul != 7,
            "in 2cycle mode, the color combiner cannot access the COMBINED_ALPHA slot in the first cycle");
        VALIDATE_ERR_CC(ccs[1].rgb.mul != 9,
            "in 2cycle mode, the color combiner cannot access the TEX1_ALPHA slot in the second cycle (but TEX0_ALPHA contains the second texture)");
        if (rdp.som.alphacmp.enable && !rdp.som.alphacmp.noise) {
            bool cc1_passthrough = (ccs[1].alpha.mul == 7 && ccs[1].alpha.add == 0);  // (any-any)*0+combined
            VALIDATE_ERR_CC(cc1_passthrough,
                "in 2cycle mode, alpha compare is broken if the second alpha combiner cycle is not a passthrough because of a hardware bug");
            VALIDATE_WARN_CC(!cc1_passthrough,
                "in 2cycle mode, alpha compare is often shifted by one pixel because of a hardware bug");
        }
    }
}

/**
 * @brief Perform validaation of a draw command (rectangle or triangle)
 * 
 * @param use_colors     True if the draw command has the shade component
 * @param use_tex        True if the draw command has the texture component
 * @param use_z          True if the draw command has the Z component
 * @param use_w          True if the draw command has the W component
 */
static void validate_draw_cmd(bool use_colors, bool use_tex, bool use_z, bool use_w)
{
    if (rdp.som.z.prim && (rdp.som.z.cmp || rdp.som.z.upd)) {
        VALIDATE_WARN_SOM(!use_z, "per-vertex Z value will be ignored because Z-source is set to primitive");
        VALIDATE_ERR_SOM(rdp.sent_zprim, "Z-source is set to primitive but SET_PRIM_DEPTH was never sent");
        use_z = true;
    }

    switch (rdp.som.cycle_type) {
    case 0 ... 1: { // 1cyc, 2cyc
        bool cc_use_tex0=false, cc_use_tex1=false, cc_use_tex0alpha=false, cc_use_tex1alpha=false;
        bool cc_use_shade=false, cc_use_shadealpha=false, bl_use_shadealpha=false;
        
        for (int i=0; i<=rdp.som.cycle_type; i++) {
            struct blender_s *bls = &rdp.som.blender[i];
            struct cc_cycle_s *ccs = &rdp.cc.cyc[i^1];
            uint8_t slots[8] = {
                ccs->rgb.suba,   ccs->rgb.subb,   ccs->rgb.mul,   ccs->rgb.add, 
                ccs->alpha.suba, ccs->alpha.subb, ccs->alpha.mul, ccs->alpha.add, 
            };

            cc_use_tex0 |= (bool)memchr(slots, 1, sizeof(slots));
            cc_use_tex1 |= (bool)memchr(slots, 2, sizeof(slots));
            cc_use_tex0alpha |= (ccs->rgb.mul == 8);
            cc_use_tex1alpha |= (ccs->rgb.mul == 9);

            cc_use_shade |= (bool)memchr(slots, 4, sizeof(slots));
            cc_use_shadealpha |= (ccs->rgb.mul == 11);
            bl_use_shadealpha |= (bls->a == 2);
        }

        if (use_tex) {
            VALIDATE_WARN_CC(cc_use_tex0 || cc_use_tex1 || cc_use_tex0alpha || cc_use_tex1alpha,
                "textured primitive drawn but the color combiner that does not use the TEX0/TEX1/TEX0_ALPHA/TEX1_ALPHA slots");
        } else {
            VALIDATE_ERR_CC(!cc_use_tex0,
                "cannot draw a non-textured primitive with a color combiner using the TEX0 slot");
            VALIDATE_ERR_CC(!cc_use_tex1,
                "cannot draw a non-textured primitive with a color combiner using the TEX1 slot");
            VALIDATE_ERR_CC(!cc_use_tex0alpha && !cc_use_tex1alpha,
                "cannot draw a non-shaded primitive with a color combiner using the TEX%d_ALPHA slot", cc_use_tex0alpha ? 0 : 1);
        }

        if (use_colors) {
            VALIDATE_WARN_CC(cc_use_shade || cc_use_shadealpha || bl_use_shadealpha,
                "shaded primitive drawn but neither the color combiner nor the blender use the SHADE/SHADE_ALPHA slots");
        } else {
            VALIDATE_ERR_CC(!cc_use_shade,
                "cannot draw a non-shaded primitive with a color combiner using the SHADE slot");
            VALIDATE_ERR_CC(!cc_use_shadealpha,
                "cannot draw a non-shaded primitive with a color combiner using the SHADE_ALPHA slot");
            VALIDATE_ERR_SOM(!bl_use_shadealpha, 
                "cannot draw a non-shaded primitive with a blender using the SHADE_ALPHA slot");
        }

        if (use_tex && !use_w)
            VALIDATE_ERR_SOM(!rdp.som.tex.persp,
                "cannot draw a textured primitive with perspective correction but without per-vertex W coordinate");

        if (!use_z) {
            VALIDATE_ERR_SOM(!rdp.som.z.cmp && !rdp.som.z.upd,
                "cannot draw a primitive without Z coordinate if Z buffer access is activated");
        }

    }   break;
    }
}

static void validate_busy_pipe(void) {
    VALIDATE_WARN(!rdp.busy.pipe, "pipe might be busy, SYNC_PIPE is missing");
    rdp.busy.pipe = false;
}

static void validate_busy_tile(int tidx) {
    VALIDATE_WARN(!rdp.busy.tile[tidx],
        "tile %d might be busy, SYNC_TILE is missing", tidx);
    rdp.busy.tile[tidx] = false;
}

/** @brief Mark TMEM as busy in range [addr..addr+size] */
static void mark_busy_tmem(int addr, int size) {
    int x0 = MIN(addr, 0x1000)/8, x1 = MIN(addr+size, 0x1000)/8, x = x0;
    while ((x&7) && x < x1) { rdp.busy.tmem[x/8] |= 1 << (x&7); x++;  }
    while (x+8 < x1)        { rdp.busy.tmem[x/8] = 0xFF;        x+=8; }
    while (x < x1)          { rdp.busy.tmem[x/8] |= 1 << (x&7); x++;  }
}

/** @brief Check if TMEM is busy in range [addr..addr+size] */
static bool is_busy_tmem(int addr, int size) {
    int x0 = MIN(addr, 0x1000)/8, x1 = MIN(addr+size, 0x1000)/8, x = x0;
    while ((x&7) && x < x1) { if (rdp.busy.tmem[x/8] & 1 << (x&7)) return true; x++;  }
    while (x+8 < x1)        { if (rdp.busy.tmem[x/8] != 0)         return true; x+=8; }
    while (x < x1)          { if (rdp.busy.tmem[x/8] & 1 << (x&7)) return true; x++;  }
    return false;
}

static void validate_busy_tmem(int addr, int size) {
    VALIDATE_WARN(!is_busy_tmem(addr, size), "writing to TMEM[0x%x:0x%x] while busy, SYNC_LOAD missing", addr, addr+size);
}

static bool check_loading_crash(int hpixels) {
    // Check for a very rare crash while loading from a misaligned address.
    // The address must have a special type of misalignment within the lower half of each 16-byte line.
    if ((rdp.tex.physaddr & 0xF) == 0) return false;
    if ((rdp.tex.physaddr & 0xF) >= 8) return false;
    // This crash doesn't apply to 4bpp textures. Notice that 4bpp always crash with LOAD_TILE (even aligned
    // addresses) but that's handled elsewhere. So this check applies to LOAD_BLOCK.
    if (rdp.tex.size == 0) return false;
    // At least ~58 bytes must be loaded in each horizontal line. This can vary a little bit depending
    // on bitdepth but the number is almost right.
    if (hpixels * (4 << rdp.tex.size) / 8 < 58) return false;
    // Crash triggered
    return true;
}

/**
 * @brief Perform validation of a tile descriptor being used as part of a drawing command.
 * 
 * @param tidx      tile ID
 * @param cycles    Bitfield of cycles in which the tile is accessed
 * @param texcoords Array of texture coordinates (S,T) used by the drawing command.
 * @param ncoords   Number of vertices in the array (the actual array element count will be double this number)
 */
static void validate_use_tile_internal(int tidx, int cycles, float *texcoords, int ncoords) {
    struct tile_s *tile = &rdp.tile[tidx];
    rdp.busy.tile[tidx] = true;
    bool use_outside = false;
    float out_s, out_t;

    if (!tile->last_settile)
        VALIDATE_ERR(tile->last_settile, "tile %d was not configured", tidx);
    else if (!tile->has_extents)
        VALIDATE_ERR_TILE(tile->has_extents, tidx, "tile %d has no extents set, missing LOAD_TILE or SET_TILE_SIZE", tidx);
    else {
        // Check whether there are texels outside the tile extents
        for (int i=0; i<ncoords && !use_outside; i++) {
            out_s = texcoords[i*2+0]; out_t = texcoords[i*2+1];
            use_outside = (out_s < tile->s0 || out_s > tile->s1 || out_t < tile->t0 || out_t > tile->t1);
        }
    }

    switch (rdp.som.cycle_type) {
        case 0: case 1: // 1-cycle / 2-cycle modes
            // YUV render mode mistakes in 1-cyc/2-cyc, that is when YUV conversion can be done.
            // In copy mode, YUV textures are copied as-is
            if (tile->fmt == 1) {
                if (cycles & 1) VALIDATE_ERR_SOM(!(rdp.som.tf_mode & (4>>0)),
                        "tile %d is YUV but texture filter in cycle %d does not activate YUV color conversion", tidx, 0);
                if (cycles & 2) VALIDATE_ERR_SOM(!(rdp.som.tf_mode & (4>>1)),
                        "tile %d is YUV but texture filter in cycle %d does not activate YUV color conversion", tidx, 1);
                if (rdp.som.sample_type > 1) {
                    static const char* texinterp[] = { "point", "point", "bilinear", "median" };
                    VALIDATE_ERR_SOM(rdp.som.tf_mode == 6 && rdp.som.cycle_type == 1,
                        "tile %d is YUV and %s filtering is active: TF1_YUVTEX0 mode must be configured in SOM", tidx, texinterp[rdp.som.sample_type]);
                    VALIDATE_ERR_SOM(rdp.som.cycle_type == 1,
                        "tile %d is YUV and %s filtering is active: 2-cycle mode must be configured", tidx, texinterp[rdp.som.sample_type]);
                }
            } else {
                if (cycles & 1) VALIDATE_ERR_SOM((rdp.som.tf_mode & (4>>0)),
                    "tile %d is RGB-based, but cycle %d is configured for YUV color conversion; try setting SOM_TF%d_RGB", tidx, 0, 0);
                if (cycles & 2) VALIDATE_ERR_SOM((rdp.som.tf_mode & (4>>1)),
                    "tile %d is RGB-based, but cycle %d is configured for YUV color conversion; try setting SOM_TF%d_RGB", tidx, 1, 1);
            }
            // Validate clamp/mirror/wrap modes
            if (use_outside) {
                VALIDATE_WARN_TILE(tile->s.clamp || tile->s.mask, tidx,
                    "tile %d will clamp horizontally because mask is 0, but clamp for S is not set", tidx);
                VALIDATE_WARN_TILE(tile->t.clamp || tile->t.mask, tidx,
                    "tile %d will clamp vertically because mask is 0, but clamp for T is not set", tidx);
            }
            break;
        case 2: // copy mode
            VALIDATE_ERR_SOM(tile->fmt != 3 && tile->fmt != 4 && (tile->fmt != 0 || tile->size != 3), 
                "tile %d is %s%d, but COPY mode does not support I4/I8/IA4/IA8/IA16/RGBA32", tidx, tex_fmt_name[tile->fmt], 4 << tile->size);
            VALIDATE_ERR_TILESIZE(!use_outside, tidx,
                "draw primitive accesses texel at (%.2f,%.2f) outside of the tile in COPY mode", out_s, out_t);
            break;
    }

    // Check that TLUT mode in SOM is active if the tile requires it (and vice-versa)
    if (tile->fmt == 2) // Color index
        VALIDATE_ERR_SOM(rdp.som.tlut.enable, "tile %d is CI (color index), but TLUT mode was not activated", tidx);
    else
        VALIDATE_ERR_SOM(!rdp.som.tlut.enable, "tile %d is not CI (color index), but TLUT mode is active", tidx);

    // Mark used areas of tmem
    switch (tile->fmt) {
    case 0: case 3: case 4: // RGBA, IA, I
        if (tile->size == 3) { // 32-bit: split between lo and hi TMEM
            mark_busy_tmem(tile->tmem_addr,         (tile->t1-tile->t0+1)*tile->tmem_pitch / 2);
            mark_busy_tmem(tile->tmem_addr + 0x800, (tile->t1-tile->t0+1)*tile->tmem_pitch / 2);
        } else {
            mark_busy_tmem(tile->tmem_addr,         (tile->t1-tile->t0+1)*tile->tmem_pitch);
        }
        break;
    case 1: // YUV: split between low and hi TMEM
        mark_busy_tmem(tile->tmem_addr,         (tile->t1-tile->t0+1)*tile->tmem_pitch / 2);
        mark_busy_tmem(tile->tmem_addr+0x800,   (tile->t1-tile->t0+1)*tile->tmem_pitch / 2);
        break;
    case 2: // color-index: mark also palette area of TMEM as used
        mark_busy_tmem(tile->tmem_addr,         (tile->t1-tile->t0+1)*tile->tmem_pitch);
        if (tile->size == 0) mark_busy_tmem(0x800 + tile->pal*64, 64);  // CI4
        if (tile->size == 1) mark_busy_tmem(0x800, 0x800);           // CI8
        break;
    }
}

/**
 * @brief Perform validation of a tile descriptor being used as part of a drawing command.
 * 
 * @param tidx      tile ID
 * @param texcoords Array of texture coordinates (S,T) used by the drawing command.
 * @param ncoords   Number of vertices in the array (the actual array element count will be double this number)
 */
static void validate_use_tile(int tidx, float *texcoords, int ncoords) {
    if (rdp.som.cycle_type == 3) return; // FILL mode does not use tiles
    if (rdp.som.cycle_type == 2) {
        // In COPY mode, the tile is used only in the first cycle
        validate_use_tile_internal(tidx, (1<<0), texcoords, ncoords);
        return;
    }

    // Tile indices. FIXME: this does not handle LODs and detail/sharpen
    int tidx0 = tidx;
    int tidx1 = (tidx+1) & 7;

    if (rdp.som.cycle_type == 1) {
        int cyc_tex0 = cc_2cyc_use_tex0();
        int cyc_tex1 = cc_2cyc_use_tex1();
        if (cyc_tex0) validate_use_tile_internal(tidx0, cyc_tex0, texcoords, ncoords);
        if (cyc_tex1) validate_use_tile_internal(tidx1, cyc_tex1, texcoords, ncoords);
    } else {
        int cyc_tex0 = cc_1cyc_use_tex0();
        if (cyc_tex0) validate_use_tile_internal(tidx0, cyc_tex0, texcoords, ncoords);
    }
}

void rdpq_validate(uint64_t *buf, uint32_t flags, int *r_errs, int *r_warns)
{
    vctx.buf = buf;
    vctx.flags = flags;
    if (r_errs)  *r_errs  = vctx.errs;
    if (r_warns) *r_warns = vctx.warns;

    uint8_t cmd = CMD(buf[0]);
    switch (cmd) {
    case 0x3F: { // SET_COLOR_IMAGE
        validate_busy_pipe();
        rdp.col.fmt = BITS(buf[0], 53, 55);
        rdp.col.size = BITS(buf[0], 51, 52);
        rdp.col.width = BITS(buf[0], 32, 41)+1;
        rdp.col.height = (BITS(buf[0], 42, 50) | (BIT(buf[0], 31) << 9))+1;  // libdragon extension
        int size = 4 << rdp.col.size;
        VALIDATE_ERR(BITS(buf[0], 0, 5) == 0, "color image must be aligned to 64 bytes");
        switch (size) {
        case 4:
            VALIDATE_ERR(false, "cannot render to 4bpp surface of type %s%d",
                tex_fmt_name[rdp.col.fmt], size); break;
        case 8: 
            VALIDATE_WARN(rdp.col.fmt == 2 || rdp.col.fmt == 4, "color image is defined %s%d but it will render as I8",
                tex_fmt_name[rdp.col.fmt], size); break;
        case 16: case 32:
            VALIDATE_WARN(rdp.col.fmt == 0, "color image is defined %s%d but it will render as RGBA%d",
                tex_fmt_name[rdp.col.fmt], size, size); break;
        }
        uint32_t addr = BITS(buf[0], 0, 24);
        if (RDPQ_VALIDATE_DETACH_ADDR && addr == RDPQ_VALIDATE_DETACH_ADDR) {
            // special case for libdragon: if the address is 0x800000, then it means
            // that the developer requested to detach the framebuffer. Treat it as
            // if SET_COLOR_IMAGE was never sent.
            rdp.last_col = NULL;
            rdp.last_col_data = 0;
        } else {
            VALIDATE_ERR(addr > 0x400, "color image address set to low RDRAM");
            VALIDATE_WARN(addr < 0x800000, "color image address is out of RDRAM");
            rdp.last_col = &buf[0];
            rdp.last_col_data = buf[0];
        }
        rdp.mode_changed = true; // revalidate render mode on different framebuffer format  
        rdp.rendertarget_changed = true; // revalidate clipping extents on render target
    }   break;
    case 0x3E: { // SET_Z_IMAGE
        validate_busy_pipe();
        VALIDATE_ERR(BITS(buf[0], 0, 5) == 0, "Z image must be aligned to 64 bytes");
        uint32_t addr = BITS(buf[0], 0, 24);
        if (RDPQ_VALIDATE_DETACH_ADDR && addr == RDPQ_VALIDATE_DETACH_ADDR) {
            // special case for libdragon: if the address is 0x800000, then it means
            // that the developer requested to detach the Z buffer. Treat it as
            // if SET_Z_IMAGE was never sent.
            rdp.last_z = NULL;
            rdp.last_z_data = 0;
        } else {
            VALIDATE_ERR(addr > 0x400, "Z image address set to low RDRAM");
            VALIDATE_WARN(addr < 0x800000, "Z image address is out of RDRAM");
            rdp.last_z = &buf[0];
            rdp.last_z_data = buf[0];
        }
        rdp.mode_changed = true; // revalidate render mode on different Z buffer
    }   break;
    case 0x3D: // SET_TEX_IMAGE
        validate_busy_pipe();
        VALIDATE_ERR(BITS(buf[0], 0, 2) == 0, "texture image must be aligned to 8 bytes");
        rdp.tex.physaddr = BITS(buf[0], 0, 24);
        rdp.tex.fmt = BITS(buf[0], 53, 55);
        rdp.tex.size = BITS(buf[0], 51, 52);
        rdp.last_tex = &buf[0];        
        rdp.last_tex_data = buf[0];
        break;
    case 0x35: { // SET_TILE
        int tidx = BITS(buf[0], 24, 26);
        validate_busy_tile(tidx);
        struct tile_s *t = &rdp.tile[tidx];
        *t = (struct tile_s){
            .last_settile = &buf[0],
            .last_settile_data = buf[0],
            .fmt = BITS(buf[0], 53, 55), .size = BITS(buf[0], 51, 52),
            .pal = BITS(buf[0], 20, 23),
            .has_extents = false,
            .tmem_addr = BITS(buf[0], 32, 40)*8,
            .tmem_pitch = BITS(buf[0], 41, 49)*8,
            .s.clamp = BIT(buf[0], 9), .t.clamp = BIT(buf[0], 19),
            .s.mirror = BIT(buf[0], 8), .t.mirror = BIT(buf[0], 18),
            .s.mask = BITS(buf[0], 4, 7), .t.mask = BITS(buf[0], 14, 17),
        };
        if (t->fmt == 2 && t->size == 1)
            VALIDATE_WARN(t->pal == 0, "invalid non-zero palette for CI8 tile");
        if (t->fmt == 1 || (t->fmt == 0 && t->size == 3))  // YUV && RGBA32
            VALIDATE_ERR(t->tmem_addr < 0x800, "format %s requires address in low TMEM (< 0x800)", t->fmt==1 ? "YUV" : "RGBA32");
    }   break;
    case 0x32: case 0x34: { // SET_TILE_SIZE, LOAD_TILE
        bool load = cmd == 0x34;
        int tidx = BITS(buf[0], 24, 26);
        struct tile_s *t = &rdp.tile[tidx];
        validate_busy_tile(tidx);
        if (load) {
            rdp.busy.tile[tidx] = true;  // mask as in use
            VALIDATE_CRASH_TEX(rdp.tex.size != 0, "LOAD_TILE does not support 4-bit textures");
        }
        t->has_extents = true;
        t->last_setsize = &buf[0];
        t->last_setsize_data = buf[0];
        t->s0 = BITS(buf[0], 44, 55)*FX(2); t->t0 = BITS(buf[0], 32, 43)*FX(2);
        t->s1 = BITS(buf[0], 12, 23)*FX(2); t->t1 = BITS(buf[0],  0, 11)*FX(2);
        if (load) {
            int hpixels = (int)t->s1 - (int)t->s0 + 1;
            VALIDATE_CRASH_TEX(!check_loading_crash(hpixels), "loading pixels from a misaligned texture image");
            validate_busy_tmem(t->tmem_addr, (t->t1-t->t0+1) * t->tmem_pitch);
        }
    }   break;
    case 0x33: { // LOAD_BLOCK
        int tidx = BITS(buf[0], 24, 26);
        int hpixels = BITS(buf[0], 12, 23)+1;
        VALIDATE_ERR_TEX(hpixels <= 2048, "cannot load more than 2048 texels at once");
        VALIDATE_CRASH_TEX(!check_loading_crash(hpixels), "loading pixels from a misaligned texture image");
        rdp.busy.tile[tidx] = true;  // mask as in use
    }   break;
    case 0x30: { // LOAD_TLUT
        int tidx = BITS(buf[0], 24, 26);
        rdp.busy.tile[tidx] = true;  // mask as in use
        struct tile_s *t = &rdp.tile[tidx];
        int low = BITS(buf[0], 44, 55), high = BITS(buf[0], 12, 23);
        if (rdp.tex.size == 0)
            VALIDATE_CRASH_TEX(rdp.tex.size != 0, "LOAD_TLUT does not support 4-bit textures");
        else
            VALIDATE_ERR_TEX(rdp.tex.fmt == 0 && rdp.tex.size == 2, "LOAD_TLUT requires texture in RGBA16 format");
        VALIDATE_ERR(t->tmem_addr >= 0x800, "palettes must be loaded in upper half of TMEM (address >= 0x800)");
        VALIDATE_WARN(!(low&3) && !(high&3), "lowest 2 bits of palette start/stop must be 0");
        VALIDATE_ERR(low>>2 < 256, "palette start index must be < 256");
        VALIDATE_ERR(high>>2 < 256, "palette stop index must be < 256");
        VALIDATE_CRASH(low>>2 <= high>>2, "palette stop index is lower than palette start index");
    }   break;
    case 0x2F: // SET_OTHER_MODES
        validate_busy_pipe();
        rdp.som = decode_som(buf[0]);
        rdp.last_som = &buf[0];
        rdp.last_som_data = buf[0];
        rdp.mode_changed = true;
        rdp.rendertarget_changed = true; // revalidate clipping extents on render target (cycle mode mught be changed)
        break;
    case 0x3C: // SET_COMBINE
        validate_busy_pipe();
        rdp.cc = decode_cc(buf[0]);
        rdp.last_cc = &buf[0];
        rdp.last_cc_data = buf[0];
        rdp.mode_changed = true;
        break;
    case 0x2D: // SET_SCISSOR
        rdp.clip.x0 = BITS(buf[0],44,55)*FX(2); rdp.clip.y0 = BITS(buf[0],32,43)*FX(2);
        rdp.clip.x1 = BITS(buf[0],12,23)*FX(2); rdp.clip.y1 = BITS(buf[0], 0,11)*FX(2);
        rdp.sent_scissor = true;
        rdp.rendertarget_changed = true;
        break;
    case 0x25: // TEX_RECT_FLIP
        VALIDATE_ERR(rdp.som.cycle_type < 2, "cannot draw texture rectangle flip in copy/fill mode");
        // passthrough
    case 0x24: { // TEX_RECT
        rdp.busy.pipe = true;
        lazy_validate_rendertarget();
        lazy_validate_rendermode();
        validate_draw_cmd(false, true, false, false);
        // Compute texture coordinates to validate tile usage
        int w = (BITS(buf[0], 44, 55) - BITS(buf[0], 12, 23))*FX(2);
        int h = (BITS(buf[0], 32, 43) - BITS(buf[0],  0, 11))*FX(2);
        float s0 = BITS(buf[1], 48, 63)*FX(5), t0 = BITS(buf[1], 32, 47)*FX(5);
        float sw = SBITS(buf[1], 16, 31)*FX(10), tw = SBITS(buf[1],  0, 15)*FX(10);
        if (rdp.som.cycle_type == 2) w += 1;    // copy mode has inclusive horizontal bounds
        if (rdp.som.cycle_type == 2) sw /= 4;   // copy mode has 4x horizontal scale
        validate_use_tile(BITS(buf[0], 24, 26), (float[]){s0, t0, s0+sw*(w-1), t0+tw*(h-1)}, 2);
        if (rdp.som.cycle_type == 2) {
            uint16_t dsdx = BITS(buf[1], 16, 31);
            if (dsdx != 4<<10) {
                if (dsdx > 4<<10 && dsdx <= 5<<10)
                    VALIDATE_WARN_SOM(0, "drawing texture rectangles in COPY mode with small horizontal reduction (< 20%%) will render without subpixel accuracy; consider using 1-cycle mode instead");
                else
                    VALIDATE_ERR_SOM(0, "horizontally-scaled texture rectangles in COPY mode will not correctly render");
            }
        }
        // Check mipmapping related quirks with rectangles
        VALIDATE_WARN_SOM(!rdp.som.tex.lod, "mipmapping does not work with texture rectangles, it will be ignored");
        if (!rdp.som.tex.lod && rdp.som.cycle_type < 2) {
            // avoid specific LOD_FRAC warnings if we already issued the previous one
            for (int i=0; i<=rdp.som.cycle_type; i++) {
                struct cc_cycle_s *ccs = &rdp.cc.cyc[i^1];
                bool lod_frac_rgb = ccs->rgb.mul == 13;
                bool lod_frac_alpha = ccs->alpha.mul == 0;
                if (lod_frac_alpha && ccs->alpha.suba == 0 && ccs->alpha.subb == 0)
                    lod_frac_alpha = false;  // (0-0)*lod_frac is allowed without warnings (it's used as passthrough)
                VALIDATE_WARN_CC(!lod_frac_rgb && !lod_frac_alpha,
                    "LOD_FRAC is not calculated correctly in rectangles (it's always 0x00 or 0xFF)");
            }
        }
    }   break;
    case 0x36: // FILL_RECTANGLE
        rdp.busy.pipe = true;
        lazy_validate_rendertarget();
        lazy_validate_rendermode();
        validate_draw_cmd(false, false, false, false);
        break;
    case 0x8 ... 0xF: // Triangles
        rdp.busy.pipe = true;
        VALIDATE_ERR_SOM(rdp.som.cycle_type < 2, "cannot draw triangles in copy/fill mode");
        lazy_validate_rendertarget();
        lazy_validate_rendermode();
        validate_draw_cmd(cmd & 4, cmd & 2, cmd & 1, cmd & 2);
        if (cmd & 2) validate_use_tile(BITS(buf[0], 48, 50), NULL, 0);  // TODO: pass texture coordinates here
        if (BITS(buf[0], 51, 53))
            VALIDATE_WARN_SOM(rdp.som.tex.lod, "triangle with %d mipmaps specified, but mipmapping is disabled",
                BITS(buf[0], 51, 53)+1);
        break;
    case 0x27: // SYNC_PIPE
        rdp.busy.pipe = false;
        break;
    case 0x29: // SYNC_FULL
        memset(&rdp.busy, 0, sizeof(rdp.busy));
        break;
    case 0x28: // SYNC_TILE
        memset(&rdp.busy.tile, 0, sizeof(rdp.busy.tile));
        break;
    case 0x26: // SYNC_LOAD
        memset(&rdp.busy.tmem, 0, sizeof(rdp.busy.tmem));
        break;
    case 0x2E: // SET_PRIM_DEPTH
        rdp.sent_zprim = true;
        break;
    case 0x3A: // SET_PRIM_COLOR
        break;
    case 0x37: // SET_FILL_COLOR
    case 0x38: // SET_FOG_COLOR
    case 0x39: // SET_BLEND_COLOR
    case 0x3B: // SET_ENV_COLOR
    case 0x2C: // SET_CONVERT
        validate_busy_pipe();
        break;
    case 0x31: // RDPQ extensions
    case 0x00: // NOP
        break;
    default: // Invalid command
        VALIDATE_WARN(0, "invalid RDP command 0x%02X", cmd);
        break;
    }

    if (r_errs)  *r_errs  = vctx.errs - *r_errs;
    if (r_warns) *r_warns = vctx.warns - *r_warns;
    vctx.buf = NULL;
}

#ifdef N64
surface_t rdpq_debug_get_tmem(void) {
    // Dump the TMEM as a 32x64 surface of 16bit pixels
    surface_t surf = surface_alloc(FMT_RGBA16, 32, 64);
    
    rdpq_attach(&surf, NULL);
    rdpq_mode_push();
    rdpq_set_mode_copy(false);
    rdpq_set_tile(RDPQ_TILE_INTERNAL, FMT_RGBA16, 0, 32*2, 0);   // pitch: 32 px * 16-bit
    rdpq_set_tile_size(RDPQ_TILE_INTERNAL, 0, 0, 32, 64);
    rdpq_texture_rectangle(RDPQ_TILE_INTERNAL,
        0, 0, 32, 64,          // x0,y0, x1,y1
        0, 0                   // s, t
    );
    rdpq_mode_pop();
    rdpq_detach_wait();

    // We dumped TMEM contents using a rectangle. When RDP accesses TMEM
    // for drawing, odd lines are dword-swapped. So we need to swap back
    // the contents of our buffer to restore the original TMEM layout.
    uint8_t *tmem = surf.buffer;
    for (int y=0;y<4096;y+=64) {
        if ((y/64)&1) { // odd line of 64x64 rectangle
            uint32_t *s = (uint32_t*)&tmem[y];
            for (int i=0;i<16;i+=2)
                SWAP(s[i], s[i+1]);
        }
    }

    return surf;
}
#endif