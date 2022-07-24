/**
 * @file rdpq.c
 * @brief RDP Command queue
 * @ingroup rsp
 *
 *
 * ## Improvements over raw hardware programming
 * 
 * RDPQ provides a very low-level API over the RDP graphics chips,
 * exposing all its settings and most of its limits. Still, rdpq
 * tries to hide a few low-level hardware details to make programming the RDP
 * less surprising and more orthogonal. To do so, it "patches" some RDP
 * commands, typically via RSP code and depending on the current RDP state. We
 * called these improvements "fixups".
 * 
 * The documentation of the public rdpq API does not explicitly mention which
 * behavior has been adjusted via fixups. Instead, this section explains in
 * details all the fixups performed by rdpq. Reading this section is not
 * necessary to understand and use rdpq, but it might be useful for people
 * that are familiar with RDP outside of libdragon (eg: libultra programmers),
 * to avoid getting confused in places where rdpq deviates from RDP (even if
 * for the better).
 * 
 * ### Scissoring and texrects: consistent coordinates
 * 
 * The RDP SET_SCISSOR and TEXTURE_RECTANGLE commands accept a rectangle
 * whose major bounds (bottom and right) are either inclusive or exclusive,
 * depending on the current RDP cycle type (fill/copy: exclusive, 1cyc/2cyc: inclusive).
 * #rdpq_set_scissor and #rdpq_texture_rectangle, instead, always use exclusive
 * major bounds, and automatically adjust them depending on the current RDP cycle
 * type. 
 * 
 * Moreover, any time the RDP cycle type changes, the current scissoring is
 * adjusted to guarantee consistent results. This is especially important
 * where the scissoring covers the whole framebuffer, because otherwise the
 * RDP might overflow the buffer while drawing.
 * 
 * ### Avoid color image buffer overflows with auto-scissoring
 * 
 * The RDP SET_COLOR_IMAGE command only contains a memory pointer and a pitch:
 * the RDP is not aware of the actual size of the buffer in terms of width/height,
 * and expects commands to be correctly clipped, or scissoring to be configured.
 * To avoid mistakes leading to memory corruption, #rdpq_set_color_image always
 * reconfigures scissoring to respect the actual buffer size.
 * 
 * Note also that when the RDP is cold-booted, the internal scissoring register
 * contains random data. This means tthat this auto-scissoring fixup also
 * provides a workaround to this, by making sure scissoring is always configured
 * at least once. In fact, by forgetting to configure scissoring, the RDP
 * can happily draw outside the framebuffer, or draw nothing, or even freeze.
 * 
 * ### Autosyncs
 * 
 * The RDP has different internal parallel units and exposes three different
 * syncing primitives to stall and avoid write-during-use bugs: SYNC_PIPE,
 * SYNC_LOAD and SYNC_TILE. Correct usage of these commands is not complicated
 * but it can be complex to get right, and require extensive hardware testing
 * because emulators do not implement the bugs caused by the absence of RDP stalls.
 * 
 * rdpq implements a smart auto-syncing engine that tracks the commands sent
 * to RDP (on the CPU) and automatically inserts syncing whenever necessary.
 * Insertion of syncing primitives is optimal for SYNC_PIPE and SYNC_TILE, and
 * conservative for SYNC_LOAD (it does not currently handle partial TMEM updates).
 * 
 * Autosyncing also works within blocks, but since it is not possible to know
 * the context in which a block will be run, it has to be conservative and
 * might issue more stalls than necessary.
 * 
 * ### Partial render mode changes
 * 
 * The RDP command SET_OTHER_MODES contains most the RDP mode settings.
 * Unfortunately the command does not allow to change only some settings, but
 * all of them must be reconfigured. This is in contrast with most graphics APIs
 * that allow to configure each render mode setting by itself (eg: it is possible
 * to just change the dithering algorithm).
 * 
 * rdpq instead tracks the current render mode on the RSP, and allows to do
 * partial updates via either the low-level #rdpq_change_other_mode_raw
 * function (where it is possible to change only a subset of the 56 bits),
 * or via the high-level rdpq_mode_* APIs (eg: #rdpq_mode_dithering), which
 * mostly build upon #rdpq_change_other_mode_raw in their implementation.
 *
 * ### Automatic 1/2 cycle type selection
 * 
 * The RDP has two main operating modes: 1 cycle per pixel and 2 cycles per pixel.
 * The latter is twice as slow, as the name implies, but it allows more complex
 * color combiners and/or blenders. Moreover, 2-cycles mode also allows for
 * multitexturing.
 * 
 * At the hardware level, it is up to the programmer to explicitly activate
 * either 1-cycle or 2-cycle mode. The problem with this is that there are
 * specific rules to follow for either mode, which does not compose cleanly
 * with partial mode changes. For instance, fogging is typically implemented
 * using the 2-cycle mode as it requires two passes in the blender. If the
 * user disables fogging for some meshes, it might be more performant to switch
 * back to 1-cycle mode, but that requires also reconfiguring the combiner.
 * 
 * To solve this problem, the higher level rdpq mode APIs (rdpq_mode_*)
 * automatically select the best cycle type depending on the current settings.
 * More specifically, 1-cycle mode is preferred as it is faster, but 2-cycle
 * mode is activated whenever one of the following conditions is true:
 *
 * * A two-pass blender is configured.
 * * A two-pass combiner is configured.
 *  
 * The correct cycle-type is automatically reconfigured any time that either
 * the blender or the combiner settings are changed. Notice that this means
 * that rdpq also transparently handles a few more details for the user, to
 * make it for an easier API:
 * 
 * * In 1 cycle mode, rdpq makes sure that the second pass of the combiner and
 *   the second pass of the blender are configured exactly like the respective
 *   first passes, because the RDP hardware requires this to operate correctly.
 * * In 2 cycles mode, if a one-pass combiner was configured by the user,
 *   the second pass is automatically configured as a simple passthrough
 *   (equivalent to `((ZERO, ZERO, ZERO, COMBINED), (ZERO, ZERO, ZERO, COMBINED))`).
 * * In 2 cycles mode, if a one-pass blender was configured by the user,
 *   it is configured in the second pass, while the first pass is defined
 *   as a passthrough (equivalent to `((PIXEL_RGB, ZERO, PIXEL_RGB, ONE))`).
 *   Notice that this is required because there is no pure passthrough in
 *   second step of the blender.
 * * RDPQ_COMBINER2 macro transparently handles the texture index swap in the
 *   second cycle. So while using the macro, TEX0 always refers to the first
 *   texture and TEX1 always refers to the second texture. Moreover, uses
 *   of TEX0/TEX1 in passes where they are not allowed would cause compilation
 *   errors, to avoid triggering undefined behaviours in RDP hardware.
 * 
 * ### Fill color as standard 32-bit color
 * 
 * The RDP command SET_FILL_COLOR (used to configure the color register
 * to be used in fill cycle type) has a very low-level interface: its argument
 * is basically a 32-bit value which is copied to the framebuffer as-is,
 * irrespective of the framebuffer color depth. For a 16-bit buffer, then,
 * it must be programmed with two copies of the same 16-bit color.
 * 
 * #rdpq_set_fill_color, instead, accepts a #color_t argument and does the
 * conversion to the "packed" format internally, depending on the current
 * framebuffer's color depth.
 * 
 */

#include "rdpq.h"
#include "rdpq_block.h"
#include "rdpq_constants.h"
#include "rdpq_debug.h"
#include "rspq.h"
#include "rspq/rspq_commands.h"
#include "rspq_constants.h"
#include "rdp_commands.h"
#include "interrupt.h"
#include "utils.h"
#include "rdp.h"
#include <string.h>
#include <math.h>
#include <float.h>

#define RDPQ_OVL_ID (0xC << 28)

static void rdpq_assert_handler(rsp_snapshot_t *state, uint16_t assert_code);

DEFINE_RSP_UCODE(rsp_rdpq, 
    .assert_handler=rdpq_assert_handler);

typedef struct rdpq_state_s {
    uint64_t sync_full;
    uint64_t scissor_rect;
    struct __attribute__((packed)) {
        uint64_t comb_1cyc; uint32_t blend_1cyc;
        uint64_t comb_2cyc; uint32_t blend_2cyc;
        uint64_t other_modes;
    } modes[4];
    uint32_t address_table[RDPQ_ADDRESS_TABLE_SIZE];
    uint32_t fill_color;
    uint32_t rdram_state_address;
    uint8_t target_bitdepth;
} rdpq_state_t;

bool __rdpq_inited = false;
bool __rdpq_zero_blocks = false;

static volatile uint32_t *rdpq_block_ptr;
static volatile uint32_t *rdpq_block_end;

static bool rdpq_block_active;
static uint8_t rdpq_config;

static uint32_t rdpq_autosync_state[2];

static rdpq_block_t *rdpq_block, *rdpq_block_first;
static int rdpq_block_size;

static volatile uint32_t *last_rdp_cmd;

static void __rdpq_interrupt(void) {
    rdpq_state_t *rdpq_state = UncachedAddr(rspq_overlay_get_state(&rsp_rdpq));

    assert(*SP_STATUS & SP_STATUS_SIG_RDPSYNCFULL);

    // Fetch the current RDP buffer for tracing
    if (rdpq_trace_fetch) rdpq_trace_fetch();

    // The state has been updated to contain a copy of the last SYNC_FULL command
    // that was sent to RDP. The command might contain a callback to invoke.
    // Extract it to local variables.
    uint32_t w0 = (rdpq_state->sync_full >> 32) & 0x00FFFFFF;
    uint32_t w1 = (rdpq_state->sync_full >>  0) & 0xFFFFFFFF;

    // Notify the RSP that we've serviced this SYNC_FULL interrupt. If others
    // are pending, they can be scheduled now, even as we execute the callback.
    MEMORY_BARRIER();
    *SP_STATUS = SP_WSTATUS_CLEAR_SIG_RDPSYNCFULL;

    // If there was a callback registered, call it.
    if (w0) {
        void (*callback)(void*) = (void (*)(void*))CachedAddr(w0 | 0x80000000);
        void* arg = (void*)w1;

        callback(arg);
    }
}

void rdpq_init()
{
    rdpq_state_t *rdpq_state = UncachedAddr(rspq_overlay_get_state(&rsp_rdpq));
    _Static_assert(sizeof(rdpq_state->modes[0]) == 32,   "invalid sizeof: rdpq_state->modes[0]");
    _Static_assert(sizeof(rdpq_state->modes)    == 32*4, "invalid sizeof: rdpq_state->modes");

    memset(rdpq_state, 0, sizeof(rdpq_state_t));
    rdpq_state->rdram_state_address = PhysicalAddr(rdpq_state);
    for (int i=0;i<4;i++)
        rdpq_state->modes[i].other_modes = ((uint64_t)RDPQ_OVL_ID << 32) + ((uint64_t)RDPQ_CMD_SET_OTHER_MODES << 56);

    // The (1 << 12) is to prevent underflow in case set other modes is called before any set scissor command.
    // Depending on the cycle mode, 1 subpixel is subtracted from the right edge of the scissor rect.
    rdpq_state->scissor_rect = (((uint64_t)RDPQ_OVL_ID << 32) + ((uint64_t)RDPQ_CMD_SET_SCISSOR_EX_FIX << 56)) | (1 << 12);

    rspq_init();
    rspq_overlay_register_static(&rsp_rdpq, RDPQ_OVL_ID);

    rdpq_block = NULL;
    rdpq_block_first = NULL;
    rdpq_block_active = false;
    rdpq_config = RDPQ_CFG_AUTOSYNCPIPE | RDPQ_CFG_AUTOSYNCLOAD | RDPQ_CFG_AUTOSYNCTILE;
    rdpq_autosync_state[0] = 0;

    __rdpq_inited = true;

    register_DP_handler(__rdpq_interrupt);
    set_DP_interrupt(1);
}

void rdpq_close()
{
    rspq_overlay_unregister(RDPQ_OVL_ID);
    __rdpq_inited = false;

    set_DP_interrupt( 0 );
    unregister_DP_handler(__rdpq_interrupt);
}

uint32_t rdpq_get_config(void)
{
    return rdpq_config;
}

void rdpq_set_config(uint32_t cfg)
{
    rdpq_config = cfg;
}

uint32_t rdpq_change_config(uint32_t on, uint32_t off)
{
    uint32_t old = rdpq_config;
    rdpq_config |= on;
    rdpq_config &= ~off;
    return old;
}


void rdpq_fence(void)
{
    rdpq_sync_full(NULL, NULL);
    rspq_int_write(RSPQ_CMD_RDP_WAIT_IDLE);
}

static void rdpq_assert_handler(rsp_snapshot_t *state, uint16_t assert_code)
{
    switch (assert_code)
    {
    case RDPQ_ASSERT_FLIP_COPY:
        printf("TextureRectangleFlip cannot be used in copy mode\n");
        break;

    case RDPQ_ASSERT_TRI_FILL:
        printf("Triangles cannot be used in copy or fill mode\n");
        break;
    
    case RDPQ_ASSERT_FILLCOPY_BLENDING:
        printf("Cannot call rdpq_mode_blending in fill or copy mode\n");
        break;

    default:
        printf("Unknown assert\n");
        break;
    }
}

static void autosync_use(uint32_t res) { 
    rdpq_autosync_state[0] |= res;
}

static void autosync_change(uint32_t res) {
    res &= rdpq_autosync_state[0];
    if (res) {
        if ((res & AUTOSYNC_TILES) && (rdpq_config & RDPQ_CFG_AUTOSYNCTILE))
            rdpq_sync_tile();
        if ((res & AUTOSYNC_TMEMS) && (rdpq_config & RDPQ_CFG_AUTOSYNCLOAD))
            rdpq_sync_load();
        if ((res & AUTOSYNC_PIPE)  && (rdpq_config & RDPQ_CFG_AUTOSYNCPIPE))
            rdpq_sync_pipe();
    }
}

void __rdpq_block_skip(int nwords)
{
    rdpq_block_ptr += nwords;
    last_rdp_cmd = NULL;
}

void __rdpq_block_update(uint32_t* old, uint32_t *new)
{
    uint32_t phys_old = PhysicalAddr(old);
    uint32_t phys_new = PhysicalAddr(new);

    assertf((phys_old & 0x7) == 0, "old not aligned to 8 bytes: %lx", phys_old);
    assertf((phys_new & 0x7) == 0, "new not aligned to 8 bytes: %lx", phys_new);

    if (last_rdp_cmd && (*last_rdp_cmd & 0xFFFFFF) == phys_old) {
        // Update the previous command.
        // It can be either a RSPQ_CMD_RDP_SET_BUFFER or RSPQ_CMD_RDP_APPEND_BUFFER,
        // but we still need to update it to the new END pointer.
        *last_rdp_cmd = (*last_rdp_cmd & 0xFF000000) | phys_new;
    } else {
        // A fixup has emitted some commands, so we need to emit a new
        // RSPQ_CMD_RDP_APPEND_BUFFER in the RSP queue of the block
        extern volatile uint32_t *rspq_cur_pointer;
        last_rdp_cmd = rspq_cur_pointer;
        rspq_int_write(RSPQ_CMD_RDP_APPEND_BUFFER, phys_new);
    }
}

void __rdpq_block_switch_buffer(uint32_t *new, uint32_t size)
{
    assert(size >= RDPQ_MAX_COMMAND_SIZE);

    rdpq_block_ptr = new;
    rdpq_block_end = new + size;

    assertf((PhysicalAddr(rdpq_block_ptr) & 0x7) == 0,
        "start not aligned to 8 bytes: %lx", PhysicalAddr(rdpq_block_ptr));
    assertf((PhysicalAddr(rdpq_block_end) & 0x7) == 0,
        "end not aligned to 8 bytes: %lx", PhysicalAddr(rdpq_block_end));

    extern volatile uint32_t *rspq_cur_pointer;
    last_rdp_cmd = rspq_cur_pointer;
    rspq_int_write(RSPQ_CMD_RDP_SET_BUFFER,
        PhysicalAddr(rdpq_block_ptr), PhysicalAddr(rdpq_block_ptr), PhysicalAddr(rdpq_block_end));
}

void __rdpq_block_next_buffer()
{
    // Allocate next chunk (double the size of the current one).
    // We use doubling here to reduce overheads for large blocks
    // and at the same time start small.
    int memsz = sizeof(rdpq_block_t) + rdpq_block_size*sizeof(uint32_t);
    rdpq_block_t *b = malloc_uncached(memsz);

    // Clean the buffer if requested (in tests). Cleaning the buffer is
    // not necessary for correct operation, but it helps writing tests that
    // want to inspect the block contents.
    if (__rdpq_zero_blocks)
        memset(b, 0, memsz);

    b->next = NULL;
    if (rdpq_block) {
        rdpq_block->next = b;
    }
    rdpq_block = b;
    if (!rdpq_block_first) rdpq_block_first = b;

    // Switch to new buffer
    __rdpq_block_switch_buffer(rdpq_block->cmds, rdpq_block_size);

    // Grow size for next buffer
    if (rdpq_block_size < RDPQ_BLOCK_MAX_SIZE) rdpq_block_size *= 2;
}

void __rdpq_block_begin()
{
    rdpq_block_active = true;
    rdpq_block = NULL;
    rdpq_block_first = NULL;
    last_rdp_cmd = NULL;
    rdpq_block_size = RDPQ_BLOCK_MIN_SIZE;
    // push on autosync state stack (to recover the state later)
    rdpq_autosync_state[1] = rdpq_autosync_state[0];
    // current autosync status is unknown because blocks can be
    // played in any context. So assume the worst: all resources
    // are being used. This will cause all SYNCs to be generated,
    // which is the safest option.
    rdpq_autosync_state[0] = 0xFFFFFFFF;
}

rdpq_block_t* __rdpq_block_end()
{
    rdpq_block_t *ret = rdpq_block_first;

    rdpq_block_active = false;
    if (rdpq_block_first) {
        rdpq_block_first->autosync_state = rdpq_autosync_state[0];
    }
    // pop on autosync state stack (recover state before building the block)
    rdpq_autosync_state[0] = rdpq_autosync_state[1];

    // clean state
    rdpq_block_first = NULL;
    rdpq_block = NULL;
    last_rdp_cmd = NULL;

    return ret;
}

void __rdpq_block_run(rdpq_block_t *block)
{
    // Set as current autosync state the one recorded at the end of
    // the block that is going to be played.
    if (block)
        rdpq_autosync_state[0] = block->autosync_state;
}

void __rdpq_block_free(rdpq_block_t *block)
{
    while (block) {
        void *b = block;
        block = block->next;
        free_uncached(b);
    }
}

static void __rdpq_block_check(void)
{
    if (rdpq_block_active && rdpq_block == NULL)
        __rdpq_block_next_buffer();
}

/// @cond

#define _rdpq_write_arg(arg) \
    *ptr++ = (arg);

/// @endcond

#define rdpq_dynamic_write(cmd_id, ...) ({ \
    rspq_write(RDPQ_OVL_ID, (cmd_id), ##__VA_ARGS__); \
})

#define rdpq_static_write(cmd_id, arg0, ...) ({ \
    if (__builtin_expect(rdpq_block_ptr + 1 + __COUNT_VARARGS(__VA_ARGS___) > rdpq_block_end, 0)) \
        __rdpq_block_next_buffer(); \
    volatile uint32_t *ptr = rdpq_block_ptr; \
    *ptr++ = (RDPQ_OVL_ID + ((cmd_id)<<24)) | (arg0); \
    __CALL_FOREACH(_rdpq_write_arg, ##__VA_ARGS__); \
    __rdpq_block_update((uint32_t*)rdpq_block_ptr, (uint32_t*)ptr); \
    rdpq_block_ptr = ptr; \
})

#define rdpq_static_skip(size) ({ \
    if (__builtin_expect(rdpq_block_ptr + size > rdpq_block_end, 0)) \
        __rdpq_block_next_buffer(); \
    __rdpq_block_skip(size); \
})

static inline bool in_block(void) {
    return rdpq_block_active;
}

#define rdpq_write(cmd_id, arg0, ...) ({ \
    if (in_block()) { \
        __rdpq_block_check(); \
        rdpq_static_write(cmd_id, arg0, ##__VA_ARGS__); \
    } else { \
        rdpq_dynamic_write(cmd_id, arg0, ##__VA_ARGS__); \
    } \
})

#define rdpq_fixup_write(cmd_id_dyn, cmd_id_fix, skip_size, arg0, ...) ({ \
    if (in_block()) { \
        __rdpq_block_check(); \
        rdpq_dynamic_write(cmd_id_fix, arg0, ##__VA_ARGS__); \
        rdpq_static_skip(skip_size); \
    } else { \
        rdpq_dynamic_write(cmd_id_dyn, arg0, ##__VA_ARGS__); \
    } \
})

__attribute__((noinline))
void __rdpq_fixup_write8(uint32_t cmd_id_dyn, uint32_t cmd_id_fix, int skip_size, uint32_t arg0, uint32_t arg1)
{
    rdpq_fixup_write(cmd_id_dyn, cmd_id_fix, skip_size, arg0, arg1);
}

__attribute__((noinline))
void __rdpq_fixup_write8_syncchange(uint32_t cmd_id_dyn, uint32_t cmd_id_fix, int skip_size, uint32_t arg0, uint32_t arg1, uint32_t autosync)
{
    autosync_change(autosync);
    rdpq_fixup_write(cmd_id_dyn, cmd_id_fix, skip_size, arg0, arg1);
}

__attribute__((noinline))
void __rdpq_dynamic_write8(uint32_t cmd_id, uint32_t arg0, uint32_t arg1)
{
    rdpq_dynamic_write(cmd_id, arg0, arg1);
}

__attribute__((noinline))
void __rdpq_write8(uint32_t cmd_id, uint32_t arg0, uint32_t arg1)
{
    rdpq_write(cmd_id, arg0, arg1);
}

__attribute__((noinline))
void __rdpq_write8_syncchange(uint32_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t autosync)
{
    autosync_change(autosync);
    __rdpq_write8(cmd_id, arg0, arg1);
}

__attribute__((noinline))
void __rdpq_write8_syncuse(uint32_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t autosync)
{
    autosync_use(autosync);
    __rdpq_write8(cmd_id, arg0, arg1);
}

__attribute__((noinline))
void __rdpq_write8_syncchangeuse(uint32_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t autosync_c, uint32_t autosync_u)
{
    autosync_change(autosync_c);
    autosync_use(autosync_u);
    __rdpq_write8(cmd_id, arg0, arg1);
}

__attribute__((noinline))
void __rdpq_write16(uint32_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    rdpq_write(cmd_id, arg0, arg1, arg2, arg3);    
}

__attribute__((noinline))
void __rdpq_write16_syncchange(uint32_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t autosync)
{
    autosync_change(autosync);
    __rdpq_write16(cmd_id, arg0, arg1, arg2, arg3);
}

__attribute__((noinline))
void __rdpq_write16_syncuse(uint32_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t autosync)
{
    autosync_use(autosync);
    __rdpq_write16(cmd_id, arg0, arg1, arg2, arg3);
}

#define TRUNCATE_S11_2(x) (((x)&0x1fff) | (((x)>>18)&~0x1fff))

/** @brief Converts a float to a s16.16 fixed point number */
int32_t float_to_s16_16(float f)
{
    // Currently the float must be clamped to this range because
    // otherwise the trunc.w.s instruction can potentially trigger
    // an unimplemented operation exception due to integer overflow.
    // TODO: maybe handle the exception? Clamp the value in the exception handler?
    if (f >= 32768.f) {
        return 0x7FFFFFFF;
    }

    if (f < -32768.f) {
        return 0x80000000;
    }

    return floor(f * 65536.f);
}

typedef struct {
    float hx, hy;
    float mx, my;
    float fy;
    float ish;
    float attr_factor;
} rdpq_tri_edge_data_t;

__attribute__((always_inline))
inline void __rdpq_write_edge_coeffs(rspq_write_t *w, rdpq_tri_edge_data_t *data, uint8_t tile, uint8_t level, const float *v1, const float *v2, const float *v3)
{
    const float x1 = v1[0];
    const float x2 = v2[0];
    const float x3 = v3[0];
    const float y1 = floorf(v1[1]*4)/4;
    const float y2 = floorf(v2[1]*4)/4;
    const float y3 = floorf(v3[1]*4)/4;

    const float to_fixed_11_2 = 4.0f;
    int32_t y1f = TRUNCATE_S11_2((int32_t)floorf(v1[1]*to_fixed_11_2));
    int32_t y2f = TRUNCATE_S11_2((int32_t)floorf(v2[1]*to_fixed_11_2));
    int32_t y3f = TRUNCATE_S11_2((int32_t)floorf(v3[1]*to_fixed_11_2));

    data->hx = x3 - x1;        
    data->hy = y3 - y1;        
    data->mx = x2 - x1;
    data->my = y2 - y1;
    float lx = x3 - x2;
    float ly = y3 - y2;

    const float nz = (data->hx*data->my) - (data->hy*data->mx);
    data->attr_factor = (fabs(nz) > FLT_MIN) ? (-1.0f / nz) : 0;
    const uint32_t lft = nz < 0;

    data->ish = (fabs(data->hy) > FLT_MIN) ? (data->hx / data->hy) : 0;
    float ism = (fabs(data->my) > FLT_MIN) ? (data->mx / data->my) : 0;
    float isl = (fabs(ly) > FLT_MIN) ? (lx / ly) : 0;
    data->fy = floorf(y1) - y1;

    const float xh = x1 + data->fy * data->ish;
    const float xm = x1 + data->fy * ism;
    const float xl = x2;

    rspq_write_arg(w, _carg(lft, 0x1, 23) | _carg(level, 0x7, 19) | _carg(tile, 0x7, 16) | _carg(y3f, 0x3FFF, 0));
    rspq_write_arg(w, _carg(y2f, 0x3FFF, 16) | _carg(y1f, 0x3FFF, 0));
    rspq_write_arg(w, float_to_s16_16(xl));
    rspq_write_arg(w, float_to_s16_16(isl));
    rspq_write_arg(w, float_to_s16_16(xh));
    rspq_write_arg(w, float_to_s16_16(data->ish));
    rspq_write_arg(w, float_to_s16_16(xm));
    rspq_write_arg(w, float_to_s16_16(ism));
}

__attribute__((always_inline))
static inline void __rdpq_write_shade_coeffs(rspq_write_t *w, rdpq_tri_edge_data_t *data, const float *v1, const float *v2, const float *v3)
{
    const float mr = v2[0] - v1[0];
    const float mg = v2[1] - v1[1];
    const float mb = v2[2] - v1[2];
    const float ma = v2[3] - v1[3];
    const float hr = v3[0] - v1[0];
    const float hg = v3[1] - v1[1];
    const float hb = v3[2] - v1[2];
    const float ha = v3[3] - v1[3];

    const float nxR = data->hy*mr - data->my*hr;
    const float nxG = data->hy*mg - data->my*hg;
    const float nxB = data->hy*mb - data->my*hb;
    const float nxA = data->hy*ma - data->my*ha;
    const float nyR = data->mx*hr - data->hx*mr;
    const float nyG = data->mx*hg - data->hx*mg;
    const float nyB = data->mx*hb - data->hx*mb;
    const float nyA = data->mx*ha - data->hx*ma;

    const float DrDx = nxR * data->attr_factor;
    const float DgDx = nxG * data->attr_factor;
    const float DbDx = nxB * data->attr_factor;
    const float DaDx = nxA * data->attr_factor;
    const float DrDy = nyR * data->attr_factor;
    const float DgDy = nyG * data->attr_factor;
    const float DbDy = nyB * data->attr_factor;
    const float DaDy = nyA * data->attr_factor;

    const float DrDe = DrDy + DrDx * data->ish;
    const float DgDe = DgDy + DgDx * data->ish;
    const float DbDe = DbDy + DbDx * data->ish;
    const float DaDe = DaDy + DaDx * data->ish;

    const int32_t final_r = float_to_s16_16(v1[0] + data->fy * DrDe);
    const int32_t final_g = float_to_s16_16(v1[1] + data->fy * DgDe);
    const int32_t final_b = float_to_s16_16(v1[2] + data->fy * DbDe);
    const int32_t final_a = float_to_s16_16(v1[3] + data->fy * DaDe);

    const int32_t DrDx_fixed = float_to_s16_16(DrDx);
    const int32_t DgDx_fixed = float_to_s16_16(DgDx);
    const int32_t DbDx_fixed = float_to_s16_16(DbDx);
    const int32_t DaDx_fixed = float_to_s16_16(DaDx);

    const int32_t DrDe_fixed = float_to_s16_16(DrDe);
    const int32_t DgDe_fixed = float_to_s16_16(DgDe);
    const int32_t DbDe_fixed = float_to_s16_16(DbDe);
    const int32_t DaDe_fixed = float_to_s16_16(DaDe);

    const int32_t DrDy_fixed = float_to_s16_16(DrDy);
    const int32_t DgDy_fixed = float_to_s16_16(DgDy);
    const int32_t DbDy_fixed = float_to_s16_16(DbDy);
    const int32_t DaDy_fixed = float_to_s16_16(DaDy);

    rspq_write_arg(w, (final_r&0xffff0000) | (0xffff&(final_g>>16)));
    rspq_write_arg(w, (final_b&0xffff0000) | (0xffff&(final_a>>16)));
    rspq_write_arg(w, (DrDx_fixed&0xffff0000) | (0xffff&(DgDx_fixed>>16)));
    rspq_write_arg(w, (DbDx_fixed&0xffff0000) | (0xffff&(DaDx_fixed>>16)));
    rspq_write_arg(w, (final_r<<16) | (final_g&0xffff));
    rspq_write_arg(w, (final_b<<16) | (final_a&0xffff));
    rspq_write_arg(w, (DrDx_fixed<<16) | (DgDx_fixed&0xffff));
    rspq_write_arg(w, (DbDx_fixed<<16) | (DaDx_fixed&0xffff));
    rspq_write_arg(w, (DrDe_fixed&0xffff0000) | (0xffff&(DgDe_fixed>>16)));
    rspq_write_arg(w, (DbDe_fixed&0xffff0000) | (0xffff&(DaDe_fixed>>16)));
    rspq_write_arg(w, (DrDy_fixed&0xffff0000) | (0xffff&(DgDy_fixed>>16)));
    rspq_write_arg(w, (DbDy_fixed&0xffff0000) | (0xffff&(DaDy_fixed>>16)));
    rspq_write_arg(w, (DrDe_fixed<<16) | (DgDe_fixed&0xffff));
    rspq_write_arg(w, (DbDe_fixed<<16) | (DaDe_fixed&0xffff));
    rspq_write_arg(w, (DrDy_fixed<<16) | (DgDy_fixed&&0xffff));
    rspq_write_arg(w, (DbDy_fixed<<16) | (DaDy_fixed&&0xffff));
}

__attribute__((always_inline))
inline void __rdpq_write_tex_coeffs(rspq_write_t *w, rdpq_tri_edge_data_t *data, const float *v1, const float *v2, const float *v3)
{
    float s1 = v1[0], t1 = v1[1], w1 = v1[2];
    float s2 = v2[0], t2 = v2[1], w2 = v2[2];
    float s3 = v3[0], t3 = v3[1], w3 = v3[2];

    const float w_factor = 1.0f / MAX(MAX(w1, w2), w3);

    w1 *= w_factor;
    w2 *= w_factor;
    w3 *= w_factor;

    s1 *= w1;
    t1 *= w1;
    s2 *= w2;
    t2 *= w2;
    s3 *= w3;
    t3 *= w3;

    w1 *= 0x7FFF;
    w2 *= 0x7FFF;
    w3 *= 0x7FFF;

    const float ms = s2 - s1;
    const float mt = t2 - t1;
    const float mw = w2 - w1;
    const float hs = s3 - s1;
    const float ht = t3 - t1;
    const float hw = w3 - w1;

    const float nxS = data->hy*ms - data->my*hs;
    const float nxT = data->hy*mt - data->my*ht;
    const float nxW = data->hy*mw - data->my*hw;
    const float nyS = data->mx*hs - data->hx*ms;
    const float nyT = data->mx*ht - data->hx*mt;
    const float nyW = data->mx*hw - data->hx*mw;

    const float DsDx = nxS * data->attr_factor;
    const float DtDx = nxT * data->attr_factor;
    const float DwDx = nxW * data->attr_factor;
    const float DsDy = nyS * data->attr_factor;
    const float DtDy = nyT * data->attr_factor;
    const float DwDy = nyW * data->attr_factor;

    const float DsDe = DsDy + DsDx * data->ish;
    const float DtDe = DtDy + DtDx * data->ish;
    const float DwDe = DwDy + DwDx * data->ish;

    const int32_t final_s = float_to_s16_16(s1 + data->fy * DsDe);
    const int32_t final_t = float_to_s16_16(t1 + data->fy * DtDe);
    const int32_t final_w = float_to_s16_16(w1 + data->fy * DwDe);

    const int32_t DsDx_fixed = float_to_s16_16(DsDx);
    const int32_t DtDx_fixed = float_to_s16_16(DtDx);
    const int32_t DwDx_fixed = float_to_s16_16(DwDx);

    const int32_t DsDe_fixed = float_to_s16_16(DsDe);
    const int32_t DtDe_fixed = float_to_s16_16(DtDe);
    const int32_t DwDe_fixed = float_to_s16_16(DwDe);

    const int32_t DsDy_fixed = float_to_s16_16(DsDy);
    const int32_t DtDy_fixed = float_to_s16_16(DtDy);
    const int32_t DwDy_fixed = float_to_s16_16(DwDy);

    rspq_write_arg(w, (final_s&0xffff0000) | (0xffff&(final_t>>16)));  
    rspq_write_arg(w, (final_w&0xffff0000));
    rspq_write_arg(w, (DsDx_fixed&0xffff0000) | (0xffff&(DtDx_fixed>>16)));
    rspq_write_arg(w, (DwDx_fixed&0xffff0000));    
    rspq_write_arg(w, (final_s<<16) | (final_t&0xffff));
    rspq_write_arg(w, (final_w<<16));
    rspq_write_arg(w, (DsDx_fixed<<16) | (DtDx_fixed&0xffff));
    rspq_write_arg(w, (DwDx_fixed<<16));
    rspq_write_arg(w, (DsDe_fixed&0xffff0000) | (0xffff&(DtDe_fixed>>16)));
    rspq_write_arg(w, (DwDe_fixed&0xffff0000));
    rspq_write_arg(w, (DsDy_fixed&0xffff0000) | (0xffff&(DtDy_fixed>>16)));
    rspq_write_arg(w, (DwDy_fixed&0xffff0000));
    rspq_write_arg(w, (DsDe_fixed<<16) | (DtDe_fixed&0xffff));
    rspq_write_arg(w, (DwDe_fixed<<16));
    rspq_write_arg(w, (DsDy_fixed<<16) | (DtDy_fixed&&0xffff));
    rspq_write_arg(w, (DwDy_fixed<<16));
}

__attribute__((always_inline))
inline void __rdpq_write_zbuf_coeffs(rspq_write_t *w, rdpq_tri_edge_data_t *data, const float *v1, const float *v2, const float *v3)
{
    const float mz = v2[0] - v1[0];
    const float hz = v3[0] - v1[0];

    const float nxz = data->hy*mz - data->my*hz;
    const float nyz = data->mx*hz - data->hx*mz;

    const float DzDx = nxz * data->attr_factor;
    const float DzDy = nyz * data->attr_factor;
    const float DzDe = DzDy + DzDx * data->ish;

    const int32_t final_z = float_to_s16_16(v1[0] + data->fy * DzDe);
    const int32_t DzDx_fixed = float_to_s16_16(DzDx);
    const int32_t DzDe_fixed = float_to_s16_16(DzDe);
    const int32_t DzDy_fixed = float_to_s16_16(DzDy);

    rspq_write_arg(w, final_z);
    rspq_write_arg(w, DzDx_fixed);
    rspq_write_arg(w, DzDe_fixed);
    rspq_write_arg(w, DzDy_fixed);
}

__attribute__((noinline))
void rdpq_triangle(uint8_t tile, uint8_t level, int32_t pos_offset, int32_t shade_offset, int32_t tex_offset, int32_t z_offset, const float *v1, const float *v2, const float *v3)
{
    uint32_t res = AUTOSYNC_PIPE;
    if (tex_offset >= 0) {
        res |= AUTOSYNC_TILE(tile);
    }
    autosync_use(res);

    uint32_t cmd_id = RDPQ_CMD_TRI;

    uint32_t size = 8;
    if (shade_offset >= 0) {
        size += 16;
        cmd_id |= 0x4;
    }
    if (tex_offset >= 0) {
        size += 16;
        cmd_id |= 0x2;
    }
    if (z_offset >= 0) {
        size += 4;
        cmd_id |= 0x1;
    }

    rspq_write_t w = rspq_write_begin(RDPQ_OVL_ID, cmd_id, size);

    if( v1[pos_offset + 1] > v2[pos_offset + 1] ) { SWAP(v1, v2); }
    if( v2[pos_offset + 1] > v3[pos_offset + 1] ) { SWAP(v2, v3); }
    if( v1[pos_offset + 1] > v2[pos_offset + 1] ) { SWAP(v1, v2); }

    rdpq_tri_edge_data_t data;
    __rdpq_write_edge_coeffs(&w, &data, tile, level, v1 + pos_offset, v2 + pos_offset, v3 + pos_offset);

    if (shade_offset >= 0) {
        __rdpq_write_shade_coeffs(&w, &data, v1 + shade_offset, v2 + shade_offset, v3 + shade_offset);
    }

    if (tex_offset >= 0) {
        __rdpq_write_tex_coeffs(&w, &data, v1 + tex_offset, v2 + tex_offset, v3 + tex_offset);
    }

    if (z_offset >= 0) {
        __rdpq_write_zbuf_coeffs(&w, &data, v1 + z_offset, v2 + z_offset, v3 + z_offset);
    }

    rspq_write_end(&w);
}

__attribute__((noinline))
void __rdpq_texture_rectangle(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    int tile = (w1 >> 24) & 7;
    // FIXME: this can also use tile+1 in case the combiner refers to TEX1
    // FIXME: this can also use tile+2 and +3 in case SOM activates texture detail / sharpen
    autosync_use(AUTOSYNC_PIPE | AUTOSYNC_TILE(tile) | AUTOSYNC_TMEM(0));
    rdpq_fixup_write(RDPQ_CMD_TEXTURE_RECTANGLE_EX, RDPQ_CMD_TEXTURE_RECTANGLE_EX_FIX, 4, w0, w1, w2, w3);
}

__attribute__((noinline))
void __rdpq_set_scissor(uint32_t w0, uint32_t w1)
{
    // NOTE: SET_SCISSOR does not require SYNC_PIPE
    __rdpq_fixup_write8(RDPQ_CMD_SET_SCISSOR_EX, RDPQ_CMD_SET_SCISSOR_EX_FIX, 2, w0, w1);
}

__attribute__((noinline))
void __rdpq_set_fill_color(uint32_t w1)
{
    __rdpq_fixup_write8_syncchange(RDPQ_CMD_SET_FILL_COLOR_32, RDPQ_CMD_SET_FILL_COLOR_32_FIX, 2, 0, w1, AUTOSYNC_PIPE);
}

__attribute__((noinline))
void __rdpq_set_fixup_image(uint32_t cmd_id_dyn, uint32_t cmd_id_fix, uint32_t w0, uint32_t w1)
{
    __rdpq_fixup_write8_syncchange(cmd_id_dyn, cmd_id_fix, 2, w0, w1, AUTOSYNC_PIPE);
}

__attribute__((noinline))
void __rdpq_set_color_image(uint32_t w0, uint32_t w1)
{
    __rdpq_fixup_write8_syncchange(RDPQ_CMD_SET_COLOR_IMAGE, RDPQ_CMD_SET_COLOR_IMAGE_FIX, 4, w0, w1, AUTOSYNC_PIPE);
}

__attribute__((noinline))
void __rdpq_set_other_modes(uint32_t w0, uint32_t w1)
{
    autosync_change(AUTOSYNC_PIPE);
    if (in_block()) {
        __rdpq_block_check();
        // Write set other modes normally first, because it doesn't need to be modified
        rdpq_static_write(RDPQ_CMD_SET_OTHER_MODES, w0, w1);
        // This command will just record the other modes to DMEM and output a set scissor command
        rdpq_dynamic_write(RDPQ_CMD_SET_OTHER_MODES_FIX, w0, w1);
        // Placeholder for the set scissor
        rdpq_static_skip(2);
    } else {
        // The regular dynamic command will output both the set other modes and the set scissor commands
        rdpq_dynamic_write(RDPQ_CMD_SET_OTHER_MODES, w0, w1);
    }
}

__attribute__((noinline))
void __rdpq_modify_other_modes(uint32_t w0, uint32_t w1, uint32_t w2)
{
    autosync_change(AUTOSYNC_PIPE);
    rdpq_fixup_write(RDPQ_CMD_MODIFY_OTHER_MODES, RDPQ_CMD_MODIFY_OTHER_MODES_FIX, 4, w0, w1, w2);
}

uint64_t rdpq_get_other_modes_raw(void)
{
    rspq_wait();
    rdpq_state_t *rdpq_state = rspq_overlay_get_state(&rsp_rdpq);
    return rdpq_state->modes[0].other_modes;
}

void rdpq_sync_full(void (*callback)(void*), void* arg)
{
    uint32_t w0 = PhysicalAddr(callback);
    uint32_t w1 = (uint32_t)arg;

    // We encode in the command (w0/w1) the callback for the RDP interrupt,
    // and we need that to be forwarded to RSP dynamic command.
    if (in_block()) {
        // In block mode, schedule the command in both static and dynamic mode.
        __rdpq_block_check();
        rdpq_dynamic_write(RDPQ_CMD_SYNC_FULL_FIX, w0, w1);
        rdpq_static_write(RDPQ_CMD_SYNC_FULL, w0, w1);
    } else {
        rdpq_dynamic_write(RDPQ_CMD_SYNC_FULL, w0, w1);
    }

    // The RDP is fully idle after this command, so no sync is necessary.
    rdpq_autosync_state[0] = 0;
}

void rdpq_sync_pipe(void)
{
    __rdpq_write8(RDPQ_CMD_SYNC_PIPE, 0, 0);
    rdpq_autosync_state[0] &= ~AUTOSYNC_PIPE;
}

void rdpq_sync_tile(void)
{
    __rdpq_write8(RDPQ_CMD_SYNC_TILE, 0, 0);
    rdpq_autosync_state[0] &= ~AUTOSYNC_TILES;
}

void rdpq_sync_load(void)
{
    __rdpq_write8(RDPQ_CMD_SYNC_LOAD, 0, 0);
    rdpq_autosync_state[0] &= ~AUTOSYNC_TMEMS;
}

void rdpq_mode_push(void)
{
    __rdpq_write8(RDPQ_CMD_PUSH_RENDER_MODE, 0, 0);
}

void rdpq_mode_pop(void)
{
    __rdpq_fixup_write8_syncchange(RDPQ_CMD_POP_RENDER_MODE, RDPQ_CMD_POP_RENDER_MODE_FIX, 4, 0, 0, AUTOSYNC_PIPE);
}


/* Extern inline instantiations. */
extern inline void rdpq_set_fill_color(color_t color);
extern inline void rdpq_set_color_image(void* dram_ptr, tex_format_t format, uint32_t width, uint32_t height, uint32_t stride);
extern inline void rdpq_set_other_modes_raw(uint64_t mode);
extern inline void rdpq_change_other_modes_raw(uint64_t mask, uint64_t val);
extern inline void rdpq_set_mode_fill(color_t color);
extern inline void rdpq_mode_combiner(rdpq_combiner_t comb);
extern inline void rdpq_mode_blender(rdpq_blender_t blend);
extern inline void rdpq_mode_blender_off(void);
