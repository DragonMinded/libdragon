#include "rdpq.h"
#include "rdpq_block.h"
#include "rdpq_constants.h"
#include "rspq.h"
#include "rspq/rspq_commands.h"
#include "rspq_constants.h"
#include "rdp_commands.h"
#include "interrupt.h"
#include <string.h>
#include <math.h>
#include <float.h>

#define SWAP(a, b) do { float t = a; a = b; b = t; } while(0)
#define TRUNCATE_S11_2(x) (0x3fff&((((x)&0x1fff) | (((x)&0x80000000)>>18))))

#define RDPQ_MAX_COMMAND_SIZE 44
#define RDPQ_BLOCK_MIN_SIZE   64
#define RDPQ_BLOCK_MAX_SIZE   4192

#define RDPQ_OVL_ID (0xC << 28)

static void rdpq_assert_handler(rsp_snapshot_t *state, uint16_t assert_code);

DEFINE_RSP_UCODE(rsp_rdpq, 
    .assert_handler=rdpq_assert_handler);

typedef struct rdpq_state_s {
    uint64_t sync_full;
    uint32_t address_table[RDPQ_ADDRESS_TABLE_SIZE];
    uint64_t other_modes;
    uint64_t scissor_rect;
    uint32_t fill_color;
    uint32_t rdram_state_address;
    uint8_t target_bitdepth;
} rdpq_state_t;

bool __rdpq_inited = false;

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

    memset(rdpq_state, 0, sizeof(rdpq_state_t));
    rdpq_state->rdram_state_address = PhysicalAddr(rdpq_state);
    rdpq_state->other_modes = ((uint64_t)RDPQ_OVL_ID << 32) + ((uint64_t)RDPQ_CMD_SET_OTHER_MODES << 56);

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

void __rdpq_block_flush(uint32_t *start, uint32_t *end)
{
    assertf(((uint32_t)start & 0x7) == 0, "start not aligned to 8 bytes: %lx", (uint32_t)start);
    assertf(((uint32_t)end & 0x7) == 0, "end not aligned to 8 bytes: %lx", (uint32_t)end);

    uint32_t phys_start = PhysicalAddr(start);
    uint32_t phys_end = PhysicalAddr(end);

    // FIXME: Updating the previous command won't work across buffer switches
    extern volatile uint32_t *rspq_cur_pointer;
    uint32_t diff = rspq_cur_pointer - last_rdp_cmd;
    if (diff == 2 && (*last_rdp_cmd&0xFFFFFF) == phys_start) {
        // Update the previous command
        *last_rdp_cmd = (RSPQ_CMD_RDP<<24) | phys_end;
    } else {
        // Put a command in the regular RSP queue that will submit the last buffer of RDP commands.
        last_rdp_cmd = rspq_cur_pointer;
        rspq_int_write(RSPQ_CMD_RDP, phys_end, phys_start);
    }
}

void __rdpq_block_switch_buffer(uint32_t *new, uint32_t size)
{
    assert(size >= RDPQ_MAX_COMMAND_SIZE);

    rdpq_block_ptr = new;
    rdpq_block_end = new + size - RDPQ_MAX_COMMAND_SIZE;

    // Enqueue a command that will point RDP to the start of the block so that static fixup commands still work.
    // Those commands rely on the fact that DP_END always points to the end of the current static block.
    __rdpq_block_flush((uint32_t*)rdpq_block_ptr, (uint32_t*)rdpq_block_ptr);
}

void __rdpq_block_next_buffer()
{
    // Allocate next chunk (double the size of the current one).
    // We use doubling here to reduce overheads for large blocks
    // and at the same time start small.
    rdpq_block_t *b = malloc_uncached(sizeof(rdpq_block_t) + rdpq_block_size*sizeof(uint32_t));
    b->next = NULL;
    if (rdpq_block) rdpq_block->next = b;
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
    volatile uint32_t *ptr = rdpq_block_ptr; \
    *ptr++ = (RDPQ_OVL_ID + ((cmd_id)<<24)) | (arg0); \
    __CALL_FOREACH(_rdpq_write_arg, ##__VA_ARGS__); \
    __rdpq_block_flush((uint32_t*)rdpq_block_ptr, (uint32_t*)ptr); \
    rdpq_block_ptr = ptr; \
    if (__builtin_expect(rdpq_block_ptr > rdpq_block_end, 0)) \
        __rdpq_block_next_buffer(); \
})

#define rdpq_static_skip(size) ({ \
    rdpq_block_ptr += size; \
    if (__builtin_expect(rdpq_block_ptr > rdpq_block_end, 0)) \
        __rdpq_block_next_buffer(); \
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
void rdpq_fixup_write8(uint32_t cmd_id_dyn, uint32_t cmd_id_fix, int skip_size, uint32_t arg0, uint32_t arg1)
{
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

__attribute__((noinline))
void rdpq_triangle(float x1, float y1, float x2, float y2, float x3, float y3)
{
    const float to_fixed_11_2 = 4.0f;
    const float to_fixed_16_16 = 65536.0f;

    if( y1 > y2 ) { SWAP(y1, y2); SWAP(x1, x2); }
    if( y2 > y3 ) { SWAP(y2, y3); SWAP(x2, x3); }
    if( y1 > y2 ) { SWAP(y1, y2); SWAP(x1, x2); }

    const int y1f = TRUNCATE_S11_2((int)(y1*to_fixed_11_2));
    const int y2f = TRUNCATE_S11_2((int)(y2*to_fixed_11_2));
    const int y3f = TRUNCATE_S11_2((int)(y3*to_fixed_11_2));

    const float Hx = x3 - x1;        
    const float Hy = y3 - y1;        
    const float Mx = x2 - x1;
    const float My = y2 - y1;
    const float Lx = x3 - x2;
    const float Ly = y3 - y2;
    const float nz = (Hx*My) - (Hy*Mx);
    const uint32_t lft = nz < 0;

    const float ish = (fabs(Hy) > FLT_MIN) ? (Hx / Hy) : 0;
    const float ism = (fabs(My) > FLT_MIN) ? (Mx / My) : 0;
    const float isl = (fabs(Ly) > FLT_MIN) ? (Lx / Ly) : 0;
    const float FY = floorf(y1) - y1;
    const float CY = ceilf(4*y2);

    const float xh = x1 + FY * ish;
    const float xm = x1 + FY * ism;
    const float xl = x2 + ( ((CY/4) - y2) * isl );

    autosync_use(AUTOSYNC_PIPE);

    rdpq_write(RDPQ_CMD_TRI, 
        _carg(lft, 0x1, 23) | _carg(y3f, 0x3FFF, 0),
        _carg(y2f, 0x3FFF, 16) | _carg(y1f, 0x3FFF, 0),
        (int32_t)(xl * to_fixed_16_16),
        (int32_t)(isl * to_fixed_16_16),
        (int32_t)(xh * to_fixed_16_16),
        (int32_t)(ish * to_fixed_16_16),
        (int32_t)(xm * to_fixed_16_16),
        (int32_t)(ism * to_fixed_16_16));
}

void rdpq_triangle_shade(float x1, float y1, float x2, float y2, float x3, float y3, float v1R, float v1G, float v1B, 
                float v2R, float v2G, float v2B, float v3R, float v3G, float v3B)
{
    autosync_use(AUTOSYNC_PIPE);
    rspq_write_t w = rspq_write_begin(RDPQ_OVL_ID, RDPQ_CMD_TRI_SHADE, 24);

    const float to_fixed_11_2 = 4.0f;
    const float to_fixed_16_16 = 65536.0f;

    if( y1 > y2 ) { SWAP(y1, y2); SWAP(x1, x2); SWAP(v1R, v2R); SWAP(v1G, v2G); SWAP(v1B, v2B); }
    if( y2 > y3 ) { SWAP(y2, y3); SWAP(x2, x3); SWAP(v2R, v3R); SWAP(v2G, v3G); SWAP(v2B, v3B); }
    if( y1 > y2 ) { SWAP(y1, y2); SWAP(x1, x2); SWAP(v1R, v2R); SWAP(v1G, v2G); SWAP(v1B, v2B); }

    const int y1f = TRUNCATE_S11_2((int)(y1*to_fixed_11_2));
    const int y2f = TRUNCATE_S11_2((int)(y2*to_fixed_11_2));
    const int y3f = TRUNCATE_S11_2((int)(y3*to_fixed_11_2));

    const float Hx = x3 - x1;        
    const float Hy = y3 - y1;        
    const float Mx = x2 - x1;
    const float My = y2 - y1;
    const float Lx = x3 - x2;
    const float Ly = y3 - y2;
    const float nz = (Hx*My) - (Hy*Mx);
    const uint32_t lft = nz < 0;

    rspq_write_arg(&w, _carg(lft, 0x1, 23) | _carg(y3f, 0x3FFF, 0));
    rspq_write_arg(&w, _carg(y2f, 0x3FFF, 16) | _carg(y1f, 0x3FFF, 0));

    const float ish = (fabs(Hy) > FLT_MIN) ? (Hx / Hy) : 0;
    const float ism = (fabs(My) > FLT_MIN) ? (Mx / My) : 0;
    const float isl = (fabs(Ly) > FLT_MIN) ? (Lx / Ly) : 0;
    const float FY = floorf(y1) - y1;
    const float CY = ceilf(4*y2);

    const float xh = x1 + FY * ish;
    const float xm = x1 + FY * ism;
    const float xl = x2 + ( ((CY/4) - y2) * isl );

    rspq_write_arg(&w, (int)( xl * to_fixed_16_16 ));
    rspq_write_arg(&w, (int)( isl * to_fixed_16_16 ));
    rspq_write_arg(&w, (int)( xh * to_fixed_16_16 ));
    rspq_write_arg(&w, (int)( ish * to_fixed_16_16 ));
    rspq_write_arg(&w, (int)( xm * to_fixed_16_16 ));
    rspq_write_arg(&w, (int)( ism * to_fixed_16_16 ));

    const float mr = v2R - v1R;
    const float mg = v2G - v1G;
    const float mb = v2B - v1B;
    const float hr = v3R - v1R;
    const float hg = v3G - v1G;
    const float hb = v3B - v1B;

    const float nxR = Hy*mr - hr*My;
    const float nyR = hr*Mx - Hx*mr;
    const float nxG = Hy*mg - hg*My;
    const float nyG = hg*Mx - Hx*mg;
    const float nxB = Hy*mb - hb*My;
    const float nyB = hb*Mx - Hx*mb;

    const float DrDx = (fabs(nz) > FLT_MIN) ? (- nxR / nz) : 0;
    const float DgDx = (fabs(nz) > FLT_MIN) ? (- nxG / nz) : 0;
    const float DbDx = (fabs(nz) > FLT_MIN) ? (- nxB / nz) : 0;
    const float DrDy = (fabs(nz) > FLT_MIN) ? (- nyR / nz) : 0;
    const float DgDy = (fabs(nz) > FLT_MIN) ? (- nyG / nz) : 0;
    const float DbDy = (fabs(nz) > FLT_MIN) ? (- nyB / nz) : 0;

    const float DrDe = DrDy + DrDx * ish;
    const float DgDe = DgDy + DgDx * ish;
    const float DbDe = DbDy + DbDx * ish;

    const int final_r = (v1R + FY * DrDe) * to_fixed_16_16;
    const int final_g = (v1G + FY * DgDe) * to_fixed_16_16;
    const int final_b = (v1B + FY * DbDe) * to_fixed_16_16;
    rspq_write_arg(&w, (final_r&0xffff0000) | (0xffff&(final_g>>16)));  
    rspq_write_arg(&w, (final_b&0xffff0000) | 0x00ff); // the 0x00ff is opaque alpha hopefully

    const int DrDx_fixed = DrDx * to_fixed_16_16;
    const int DgDx_fixed = DgDx * to_fixed_16_16;
    const int DbDx_fixed = DbDx * to_fixed_16_16;

    rspq_write_arg(&w, (DrDx_fixed&0xffff0000) | (0xffff&(DgDx_fixed>>16)));
    rspq_write_arg(&w, (DbDx_fixed&0xffff0000));    

    rspq_write_arg(&w, 0);  // not dealing with the color fractions right now
    rspq_write_arg(&w, 0);

    rspq_write_arg(&w, (DrDx_fixed<<16) | (DgDx_fixed&0xffff));
    rspq_write_arg(&w, (DbDx_fixed<<16));

    const int DrDe_fixed = DrDe * to_fixed_16_16;
    const int DgDe_fixed = DgDe * to_fixed_16_16;
    const int DbDe_fixed = DbDe * to_fixed_16_16;

    rspq_write_arg(&w, (DrDe_fixed&0xffff0000) | (0xffff&(DgDe_fixed>>16)));
    rspq_write_arg(&w, (DbDe_fixed&0xffff0000));

    const int DrDy_fixed = DrDy * to_fixed_16_16;
    const int DgDy_fixed = DgDy * to_fixed_16_16;
    const int DbDy_fixed = DbDy * to_fixed_16_16;

    rspq_write_arg(&w, (DrDy_fixed&0xffff0000) | (0xffff&(DgDy_fixed>>16)));
    rspq_write_arg(&w, (DbDy_fixed&0xffff0000));

    rspq_write_arg(&w, (DrDe_fixed<<16) | (DgDe_fixed&0xffff));
    rspq_write_arg(&w, (DbDe_fixed<<16));

    rspq_write_arg(&w, (DrDy_fixed<<16) | (DgDy_fixed&&0xffff));
    rspq_write_arg(&w, (DbDy_fixed<<16));

    rspq_write_end(&w);
}

__attribute__((noinline))
void __rdpq_texture_rectangle(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    int tile = (w1 >> 24) & 7;
    autosync_use(AUTOSYNC_PIPE | AUTOSYNC_TILE(tile) | AUTOSYNC_TMEM(0));
    rdpq_fixup_write(RDPQ_CMD_TEXTURE_RECTANGLE_EX, RDPQ_CMD_TEXTURE_RECTANGLE_EX_FIX, 4, w0, w1, w2, w3);
}

__attribute__((noinline))
void __rdpq_set_scissor(uint32_t w0, uint32_t w1)
{
    // NOTE: SET_SCISSOR does not require SYNC_PIPE
    rdpq_fixup_write8(RDPQ_CMD_SET_SCISSOR_EX, RDPQ_CMD_SET_SCISSOR_EX_FIX, 2, w0, w1);
}

__attribute__((noinline))
void __rdpq_set_fill_color(uint32_t w1)
{
    autosync_change(AUTOSYNC_PIPE);
    rdpq_fixup_write8(RDPQ_CMD_SET_FILL_COLOR_32, RDPQ_CMD_SET_FILL_COLOR_32_FIX, 2, 0, w1);
}

__attribute__((noinline))
void __rdpq_set_fixup_image(uint32_t cmd_id_dyn, uint32_t cmd_id_fix, uint32_t w0, uint32_t w1)
{
    autosync_change(AUTOSYNC_PIPE);
    rdpq_fixup_write8(cmd_id_dyn, cmd_id_fix, 2, w0, w1);
}

__attribute__((noinline))
void __rdpq_set_color_image(uint32_t w0, uint32_t w1)
{
    autosync_change(AUTOSYNC_PIPE);
    rdpq_fixup_write8(RDPQ_CMD_SET_COLOR_IMAGE, RDPQ_CMD_SET_COLOR_IMAGE_FIX, 4, w0, w1);
}

__attribute__((noinline))
void __rdpq_set_other_modes(uint32_t w0, uint32_t w1)
{
    autosync_change(AUTOSYNC_PIPE);
    if (in_block()) {
        __rdpq_block_check(); \
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


/* Extern inline instantiations. */
extern inline void rdpq_set_fill_color(color_t color);
extern inline void rdpq_set_color_image(void* dram_ptr, tex_format_t format, uint32_t width, uint32_t height, uint32_t stride);
