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
 * partial updates via either the low-level #rdpq_change_other_modes_raw
 * function (where it is possible to change only a subset of the 56 bits),
 * or via the high-level rdpq_mode_* APIs (eg: #rdpq_mode_dithering), which
 * mostly build upon #rdpq_change_other_modes_raw in their implementation.
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
#include "rdpq_internal.h"
#include "rdpq_constants.h"
#include "rdpq_debug_internal.h"
#include "rspq.h"
#include "rspq/rspq_internal.h"
#include "rspq_constants.h"
#include "rdpq_macros.h"
#include "interrupt.h"
#include "utils.h"
#include "rdp.h"
#include <string.h>
#include <math.h>
#include <float.h>

static void rdpq_assert_handler(rsp_snapshot_t *state, uint16_t assert_code);

DEFINE_RSP_UCODE(rsp_rdpq, 
    .assert_handler=rdpq_assert_handler);

/** @brief State of the rdpq overlay */
typedef struct rdpq_state_s {
    uint64_t sync_full;                 ///< Last SYNC_FULL command
    uint64_t scissor_rect;              ///< Current scissoring rectangle
    struct __attribute__((packed)) {
        uint64_t comb_1cyc;             ///< Combiner to use in 1cycle mode
        uint32_t blend_1cyc;            ///< Blender to use in 1cycle mode
        uint64_t comb_2cyc;             ///< Combiner to use in 2cycle mode
        uint32_t blend_2cyc;            ///< Blender to use in 2cycle mode
        uint64_t other_modes;           ///< SET_OTHER_MODES configuration
    } modes[4];                         ///< Modes stack (position 0 is current)
    uint32_t address_table[RDPQ_ADDRESS_TABLE_SIZE];    ///< Address lookup table
    uint32_t fill_color;                ///< Current fill color (FMT_RGBA32)
    uint32_t rdram_state_address;       ///< Address of this state in RDRAM
    uint8_t target_bitdepth;            ///< Current render target bitdepth
} rdpq_state_t;

bool __rdpq_inited = false;
bool __rdpq_zero_blocks = false;

volatile uint32_t *rdpq_block_ptr;
volatile uint32_t *rdpq_block_end;

static rdpq_state_t *rdpq_state;
static uint32_t rdpq_config;
static uint32_t rdpq_autosync_state[2];

/** True if we're currently creating a rspq block */
static rdpq_block_t *rdpq_block, *rdpq_block_first;
static int rdpq_block_size;

static volatile uint32_t *last_rdp_append_buffer;

static void __rdpq_interrupt(void) {
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
    // Do nothing if rdpq was already initialized
    if (__rdpq_inited)
        return;

    rspq_init();

    rdpq_state = UncachedAddr(rspq_overlay_get_state(&rsp_rdpq));
    _Static_assert(sizeof(rdpq_state->modes[0]) == 32,   "invalid sizeof: rdpq_state->modes[0]");
    _Static_assert(sizeof(rdpq_state->modes)    == 32*4, "invalid sizeof: rdpq_state->modes");

    memset(rdpq_state, 0, sizeof(rdpq_state_t));
    rdpq_state->rdram_state_address = PhysicalAddr(rdpq_state);
    for (int i=0;i<4;i++)
        rdpq_state->modes[i].other_modes = ((uint64_t)RDPQ_OVL_ID << 32) + ((uint64_t)RDPQ_CMD_SET_OTHER_MODES << 56);

    // The (1 << 12) is to prevent underflow in case set other modes is called before any set scissor command.
    // Depending on the cycle mode, 1 subpixel is subtracted from the right edge of the scissor rect.
    rdpq_state->scissor_rect = (((uint64_t)RDPQ_OVL_ID << 32) + ((uint64_t)RDPQ_CMD_SET_SCISSOR << 56)) | (1 << 12);
    
    rspq_overlay_register_static(&rsp_rdpq, RDPQ_OVL_ID);

    rdpq_block = NULL;
    rdpq_block_first = NULL;
    rdpq_config = RDPQ_CFG_DEFAULT;
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

uint32_t rdpq_config_set(uint32_t cfg)
{
    uint32_t prev = rdpq_config;
    rdpq_config = cfg;
    return prev;
}

uint32_t rdpq_config_enable(uint32_t cfg)
{
    return rdpq_config_set(rdpq_config | cfg);
}

uint32_t rdpq_config_disable(uint32_t cfg)
{
    return rdpq_config_set(rdpq_config & ~cfg);
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
    case RDPQ_ASSERT_FILLCOPY_BLENDING:
        printf("Cannot call rdpq_mode_blending in fill or copy mode\n");
        break;

    default:
        printf("Unknown assert\n");
        break;
    }
}

void __rdpq_autosync_use(uint32_t res) { 
    rdpq_autosync_state[0] |= res;
}

void __rdpq_autosync_change(uint32_t res) {
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

void __rdpq_block_update_reset(void)
{
    last_rdp_append_buffer = NULL;
}

void __rdpq_block_update(uint32_t* old, uint32_t *new)
{
    uint32_t phys_old = PhysicalAddr(old);
    uint32_t phys_new = PhysicalAddr(new);

    assertf((phys_old & 0x7) == 0, "old not aligned to 8 bytes: %lx", phys_old);
    assertf((phys_new & 0x7) == 0, "new not aligned to 8 bytes: %lx", phys_new);

    if (last_rdp_append_buffer && (*last_rdp_append_buffer & 0xFFFFFF) == phys_old) {
        // Update the previous command.
        // It can be either a RSPQ_CMD_RDP_SET_BUFFER or RSPQ_CMD_RDP_APPEND_BUFFER,
        // but we still need to update it to the new END pointer.
        *last_rdp_append_buffer = (*last_rdp_append_buffer & 0xFF000000) | phys_new;
    } else {
        // A fixup has emitted some commands, so we need to emit a new
        // RSPQ_CMD_RDP_APPEND_BUFFER in the RSP queue of the block
        extern volatile uint32_t *rspq_cur_pointer;
        last_rdp_append_buffer = rspq_cur_pointer;
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
    last_rdp_append_buffer = rspq_cur_pointer;
    rspq_int_write(RSPQ_CMD_RDP_SET_BUFFER,
        PhysicalAddr(rdpq_block_ptr), PhysicalAddr(rdpq_block_ptr), PhysicalAddr(rdpq_block_end));
}

void __rdpq_block_next_buffer(void)
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
    rdpq_block = NULL;
    rdpq_block_first = NULL;
    rdpq_block_ptr = NULL;
    rdpq_block_end = NULL;
    last_rdp_append_buffer = NULL;
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

    if (rdpq_block_first) {
        rdpq_block_first->autosync_state = rdpq_autosync_state[0];
    }
    // pop on autosync state stack (recover state before building the block)
    rdpq_autosync_state[0] = rdpq_autosync_state[1];

    // clean state
    rdpq_block_first = NULL;
    rdpq_block = NULL;
    last_rdp_append_buffer = NULL;

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

__attribute__((noinline))
void __rdpq_write8(uint32_t cmd_id, uint32_t arg0, uint32_t arg1)
{
    rdpq_write((cmd_id, arg0, arg1));
}

__attribute__((noinline))
void __rdpq_write8_syncchange(uint32_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t autosync)
{
    __rdpq_autosync_change(autosync);
    __rdpq_write8(cmd_id, arg0, arg1);
}

__attribute__((noinline))
void __rdpq_write8_syncuse(uint32_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t autosync)
{
    __rdpq_autosync_use(autosync);
    __rdpq_write8(cmd_id, arg0, arg1);
}

__attribute__((noinline))
void __rdpq_write8_syncchangeuse(uint32_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t autosync_c, uint32_t autosync_u)
{
    __rdpq_autosync_change(autosync_c);
    __rdpq_autosync_use(autosync_u);
    __rdpq_write8(cmd_id, arg0, arg1);
}

__attribute__((noinline))
void __rdpq_write16(uint32_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    rdpq_write((cmd_id, arg0, arg1, arg2, arg3));
}

__attribute__((noinline))
void __rdpq_write16_syncchange(uint32_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t autosync)
{
    __rdpq_autosync_change(autosync);
    __rdpq_write16(cmd_id, arg0, arg1, arg2, arg3);
}

__attribute__((noinline))
void __rdpq_write16_syncuse(uint32_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t autosync)
{
    __rdpq_autosync_use(autosync);
    __rdpq_write16(cmd_id, arg0, arg1, arg2, arg3);
}

__attribute__((noinline))
void __rdpq_texture_rectangle(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    int tile = (w1 >> 24) & 7;
    // FIXME: this can also use tile+1 in case the combiner refers to TEX1
    // FIXME: this can also use tile+2 and +3 in case SOM activates texture detail / sharpen
    __rdpq_autosync_use(AUTOSYNC_PIPE | AUTOSYNC_TILE(tile) | AUTOSYNC_TMEM(0));
    rdpq_fixup_write(
        (RDPQ_CMD_TEXTURE_RECTANGLE_EX, w0, w1, w2, w3),  // RSP
        (RDPQ_CMD_TEXTURE_RECTANGLE_EX, w0, w1, w2, w3)   // RDP
    );
}

__attribute__((noinline))
void __rdpq_set_scissor(uint32_t w0, uint32_t w1)
{
    // NOTE: SET_SCISSOR does not require SYNC_PIPE
    rdpq_fixup_write(
        (RDPQ_CMD_SET_SCISSOR_EX, w0, w1),  // RSP
        (RDPQ_CMD_SET_SCISSOR_EX, w0, w1)   // RDP
    );
}

__attribute__((noinline))
void __rdpq_set_fill_color(uint32_t w1)
{
    __rdpq_autosync_change(AUTOSYNC_PIPE);
    rdpq_fixup_write(
        (RDPQ_CMD_SET_FILL_COLOR_32, 0, w1), // RSP
        (RDPQ_CMD_SET_FILL_COLOR_32, 0, w1)  // RDP
    );
}

__attribute__((noinline))
void __rdpq_fixup_write8_pipe(uint32_t cmd_id, uint32_t w0, uint32_t w1)
{
    __rdpq_autosync_change(AUTOSYNC_PIPE);
    rdpq_fixup_write(
        (cmd_id, w0, w1),
        (cmd_id, w0, w1)
    );
}

__attribute__((noinline))
void __rdpq_set_color_image(uint32_t w0, uint32_t w1, uint32_t sw0, uint32_t sw1)
{
    // SET_COLOR_IMAGE on RSP always generates an additional SET_SCISSOR, so make sure there is
    // space for it in case of a static buffer (in a block).
    __rdpq_autosync_change(AUTOSYNC_PIPE);
    rdpq_fixup_write(
        (RDPQ_CMD_SET_COLOR_IMAGE, w0, w1), // RSP
        (RDPQ_CMD_SET_COLOR_IMAGE, w0, w1), (RDPQ_CMD_SET_FILL_COLOR, 0, 0) // RDP
    );

    if (rdpq_config & RDPQ_CFG_AUTOSCISSOR)
        __rdpq_set_scissor(sw0, sw1);
}

void rdpq_set_color_image(surface_t *surface)
{
    assertf((PhysicalAddr(surface->buffer) & 63) == 0,
        "buffer pointer is not aligned to 64 bytes, so it cannot be used as RDP color image");
    rdpq_set_color_image_raw(0, PhysicalAddr(surface->buffer), 
        surface_get_format(surface), surface->width, surface->height, surface->stride);
}

void rdpq_set_z_image(surface_t *surface)
{
    assertf(surface_get_format(surface) == FMT_RGBA16, "the format of the Z-buffer surface must be RGBA16");
    assertf((PhysicalAddr(surface->buffer) & 63) == 0,
        "buffer pointer is not aligned to 64 bytes, so it cannot be used as RDP Z image");
    rdpq_set_z_image_raw(0, PhysicalAddr(surface->buffer));
}

void rdpq_set_texture_image(surface_t *surface)
{
    // FIXME: we currently don't know how to handle a texture which is a sub-surface, that is
    // with excess space. So better rule it out for now, and we can enbale that later once we
    // make sure it works correctly.
    assertf(TEX_FORMAT_PIX2BYTES(surface_get_format(surface), surface->width) == surface->stride,
        "configure sub-surfaces as textures is not supported");
    rdpq_set_texture_image_raw(0, PhysicalAddr(surface->buffer), surface_get_format(surface), surface->width, surface->height);
}

__attribute__((noinline))
void __rdpq_set_other_modes(uint32_t w0, uint32_t w1)
{
    __rdpq_autosync_change(AUTOSYNC_PIPE);
    rdpq_fixup_write(
        (RDPQ_CMD_SET_OTHER_MODES, w0, w1),  // RSP
        (RDPQ_CMD_SET_OTHER_MODES, w0, w1), (RDPQ_CMD_SET_SCISSOR, 0, 0)   // RDP
    );
}

__attribute__((noinline))
void __rdpq_modify_other_modes(uint32_t w0, uint32_t w1, uint32_t w2)
{
    __rdpq_autosync_change(AUTOSYNC_PIPE);
    rdpq_fixup_write(
        (RDPQ_CMD_MODIFY_OTHER_MODES, w0, w1, w2),
        (RDPQ_CMD_SET_OTHER_MODES, 0, 0), (RDPQ_CMD_SET_SCISSOR, 0, 0)   // RDP
    );
}

uint64_t rdpq_get_other_modes_raw(void)
{
    rdpq_state_t *rdpq_state = rspq_overlay_get_state(&rsp_rdpq);
    return rdpq_state->modes[0].other_modes;
}

void rdpq_sync_full(void (*callback)(void*), void* arg)
{
    uint32_t w0 = PhysicalAddr(callback);
    uint32_t w1 = (uint32_t)arg;

    // We encode in the command (w0/w1) the callback for the RDP interrupt,
    // and we need that to be forwarded to RSP dynamic command.
    rdpq_fixup_write(
        (RDPQ_CMD_SYNC_FULL, w0, w1), // RSP
        (RDPQ_CMD_SYNC_FULL, w0, w1)  // RDP
    );

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

/* Extern inline instantiations. */
extern inline void rdpq_set_fill_color(color_t color);
extern inline void rdpq_set_fill_color_stripes(color_t color1, color_t color2);
extern inline void rdpq_set_fog_color(color_t color);
extern inline void rdpq_set_blend_color(color_t color);
extern inline void rdpq_set_prim_color(color_t color);
extern inline void rdpq_set_env_color(color_t color);
extern inline void rdpq_set_prim_depth_fx(uint16_t primitive_z, int16_t primitive_delta_z);
extern inline void rdpq_load_tlut(tile_t tile, uint8_t lowidx, uint8_t highidx);
extern inline void rdpq_set_tile_size_fx(tile_t tile, uint16_t s0, uint16_t t0, uint16_t s1, uint16_t t1);
extern inline void rdpq_load_block(tile_t tile, uint16_t s0, uint16_t t0, uint16_t num_texels, uint16_t tmem_pitch);
extern inline void rdpq_load_block_fx(tile_t tile, uint16_t s0, uint16_t t0, uint16_t num_texels, uint16_t dxt);
extern inline void rdpq_load_tile_fx(tile_t tile, uint16_t s0, uint16_t t0, uint16_t s1, uint16_t t1);
extern inline void rdpq_set_tile_full(tile_t tile, tex_format_t format, uint16_t tmem_addr, uint16_t tmem_pitch, uint8_t palette, uint8_t ct, uint8_t mt, uint8_t mask_t, uint8_t shift_t, uint8_t cs, uint8_t ms, uint8_t mask_s, uint8_t shift_s);
extern inline void rdpq_set_other_modes_raw(uint64_t mode);
extern inline void rdpq_change_other_modes_raw(uint64_t mask, uint64_t val);
extern inline void rdpq_fill_rectangle_fx(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
extern inline void rdpq_set_color_image_raw(uint8_t index, uint32_t offset, tex_format_t format, uint32_t width, uint32_t height, uint32_t stride);
extern inline void rdpq_set_z_image_raw(uint8_t index, uint32_t offset);
extern inline void rdpq_set_texture_image_raw(uint8_t index, uint32_t offset, tex_format_t format, uint16_t width, uint16_t height);
extern inline void rdpq_set_lookup_address(uint8_t index, void* rdram_addr);
extern inline void rdpq_set_tile(tile_t tile, tex_format_t format, uint16_t tmem_addr, uint16_t tmem_pitch, uint8_t palette);
