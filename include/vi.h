/**
 * @file vi.h
 * @brief Video Interface Subsystem
 * @ingroup display
 */
#ifndef __LIBDRAGON_VI_H
#define __LIBDRAGON_VI_H

#include <stdint.h>
#include <stdbool.h>
#include "regsinternal.h"
#include "n64sys.h"
#include "display.h"
#include "interrupt.h"
#include "utils.h"
#include "debug.h"
#include "surface.h"
#include "vi.h"

/**
 * @addtogroup display
 * @{
 */

/** @brief Register uncached location in memory of VI */
#define VI_REGISTERS_ADDR       0xA4400000
/** @brief Number of useful 32-bit registers at the register base */
#define VI_REGISTERS_COUNT      14

/**
 * @brief Video Interface register structure
 *
 * Whenever trying to configure VI registers, 
 * this struct and its index definitions below can be very useful 
 * in writing comprehensive and verbose code.
 */
typedef struct vi_config_s{
    uint32_t regs[VI_REGISTERS_COUNT];
} vi_config_t;

/** @brief VI Index register of controlling general display filters/bitdepth configuration */
#define VI_CTRL         0
/** @brief VI Index register of RDRAM base address of the video output Frame Buffer. This can be changed as needed to implement double or triple buffering. */
#define VI_ORIGIN       1
/** @brief VI Index register of width in pixels of the frame buffer. */
#define VI_WIDTH        2
/** @brief VI Index register of vertical interrupt. */
#define VI_V_INTR       3
/** @brief VI Index register of the current half line, sampled once per line. */
#define VI_V_CURRENT    4
/** @brief VI Index register of sync/burst values */
#define VI_BURST        5
/** @brief VI Index register of total visible and non-visible lines. 
 * This should match either NTSC (non-interlaced: 0x20D, interlaced: 0x20C) or PAL (non-interlaced: 0x271, interlaced: 0x270) */
#define VI_V_SYNC       6
/** @brief VI Index register of total width of a line */
#define VI_H_SYNC       7
/** @brief VI Index register of an alternate scanline length for one scanline during vsync. */
#define VI_H_SYNC_LEAP  8
/** @brief VI Index register of start/end of the active video image, in screen pixels */
#define VI_H_VIDEO      9
/** @brief VI Index register of start/end of the active video image, in screen half-lines. */
#define VI_V_VIDEO      10
/** @brief VI Index register of start/end of the color burst enable, in half-lines. */
#define VI_V_BURST      11
/** @brief VI Index register of horizontal subpixel offset and 1/horizontal scale up factor. */
#define VI_X_SCALE      12
/** @brief VI Index register of vertical subpixel offset and 1/vertical scale up factor. */
#define VI_Y_SCALE      13

/**
 * @name Video Mode Register Presets
 * @brief Presets to use when setting a particular video mode
 * @{
 */
static const vi_config_t vi_ntsc_p = {.regs = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x03e52239, 0x0000020d, 0x00000c15,
    0x0c150c15, 0x006c02ec, 0x002501ff, 0x000e0204,
    0x00000000, 0x00000000 }};
static const vi_config_t vi_pal_p =  {.regs = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x0404233a, 0x00000271, 0x00150c69,
    0x0c6f0c6e, 0x00800300, 0x005f0239, 0x0009026b,
    0x00000000, 0x00000000 }};
static const vi_config_t vi_mpal_p = {.regs = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x04651e39, 0x0000020d, 0x00040c11,
    0x0c190c1a, 0x006c02ec, 0x002501ff, 0x000e0204,
    0x00000000, 0x00000000 }};
static const vi_config_t vi_ntsc_i = {.regs = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x03e52239, 0x0000020c, 0x00000c15,
    0x0c150c15, 0x006c02ec, 0x002301fd, 0x000e0204,
    0x00000000, 0x00000000 }};
static const vi_config_t vi_pal_i = {.regs = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x0404233a, 0x00000270, 0x00150c69,
    0x0c6f0c6e, 0x00800300, 0x005d0237, 0x0009026b,
    0x00000000, 0x00000000 }};
static const vi_config_t vi_mpal_i = {.regs = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x04651e39, 0x0000020c, 0x00000c10,
    0x0c1c0c1c, 0x006c02ec, 0x002301fd, 0x000b0202,
    0x00000000, 0x00000000 }};
/** @} */

/** @brief Register initial value array */
static const vi_config_t vi_config_presets[2][3] = {
    {vi_pal_p, vi_ntsc_p, vi_mpal_p},
    {vi_pal_i, vi_ntsc_i, vi_mpal_i}
};

#define VI_TV_TYPES_INTERLACE 3

/** Under VI_CTRL */
#define VI_DEDITHER_FILTER_ENABLE           (1<<16)
#define VI_PIXEL_ADVANCE_DEFAULT            (0b0011 << 12)
#define VI_PIXEL_ADVANCE_BBPLAYER           (0b0001 << 12)
#define VI_AA_MODE_NONE                     (0b11 << 8)
#define VI_AA_MODE_RESAMPLE                 (0b10 << 8)
#define VI_AA_MODE_RESAMPLE_FETCH_NEEDED    (0b01 << 8)
#define VI_AA_MODE_RESAMPLE_FETCH_ALWAYS    (0b00 << 8)
#define VI_CTRL_SERRATE                     (1<<6)
#define VI_DIVOT_ENABLE                     (1<<4)
#define VI_GAMMA_ENABLE                     (1<<3)
#define VI_GAMMA_DITHER_ENABLE              (1<<2)
#define VI_CTRL_TYPE_32_BIT                 (0b11)
#define VI_CTRL_TYPE_16_BIT                 (0b10)
#define VI_CTRL_TYPE_BLANK                  (0b00)

/** Under VI_ORIGIN  */
#define VI_ORIGIN_SET(value)      ((value & 0xFFFFFF) << 0)

/** Under VI_WIDTH   */
#define VI_WIDTH_SET(value)       ((value & 0xFFF) << 0)

/** Under VI_V_CURRENT  */
#define VI_V_CURRENT_VBLANK     2

/** Under VI_V_INTR    */
#define VI_V_INTR_SET(value)      ((value & 0x3FF) << 0)
#define VI_V_INTR_DEFAULT         0x3FF

/** Under VI_BURST     */
#define VI_BURST_START(value)      ((value & 0x3F) << 20)
#define VI_VSYNC_WIDTH(value)      ((value & 0x7)  << 16)
#define VI_BURST_WIDTH(value)      ((value & 0xFF) << 8)
#define VI_HSYNC_WIDTH(value)      ((value & 0xFF) << 0)

#define VI_BURST_START_NTSC     62
#define VI_VSYNC_WIDTH_NTSC     5
#define VI_BURST_WIDTH_NTSC     34
#define VI_HSYNC_WIDTH_NTSC     57

#define VI_BURST_START_PAL      64
#define VI_VSYNC_WIDTH_PAL      4
#define VI_BURST_WIDTH_PAL      35
#define VI_HSYNC_WIDTH_PAL      58


inline volatile uint32_t *vi_register(int index) {
    return &(((uint32_t *)VI_REGISTERS_ADDR)[index]);
}

/**
 * @brief Write a set of video registers to the VI
 *
 * @param[in] config
 *            A pointer to a set of register values to be written
 */
inline void vi_configure_registers(vi_config_t *config)
{
    /* This should never happen */
    assert(config);

    /* Just straight copy */
    for( int i = 0; i < VI_REGISTERS_COUNT; i++ )
    {
        /* Don't clear interrupts */
        if( i == 3 ) { continue; }
        if( i == 4 ) { continue; }

        *vi_register(i) = config->regs[i];
        MEMORY_BARRIER();
    }
}

/**
 * @brief Update the framebuffer pointer in the VI
 *
 * @param[in] dram_val
 *            The new framebuffer to use for display.  Should be aligned and uncached.
 */
static void vi_write_dram_register( void const * const dram_val )
{
    *vi_register(VI_ORIGIN) = VI_ORIGIN_SET(PhysicalAddr(dram_val));
    MEMORY_BARRIER();
}

/** @brief Wait until entering the vblank period */
static void vi_wait_for_vblank()
{
    while(*vi_register(VI_V_CURRENT) != VI_V_CURRENT_VBLANK ) {  }
}

/** @brief Return true if VI is active (H_VIDEO != 0) */
static inline bool vi_is_active()
{
    return (*vi_register(VI_H_VIDEO)? true : false);
}

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

/** @} */ /* vi */

#endif
