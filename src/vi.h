/**
 * @file vi.h
 * @brief Video Interface Subsystem
 * @ingroup display
 */
#ifndef __LIBDRAGON_VI_H
#define __LIBDRAGON_VI_H

#include <stdint.h>
#include <stdbool.h>
#include "n64sys.h"

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

/** @brief Base pointer to hardware Video interface registers that control various aspects of VI configuration.
 * Shouldn't be used by itself, use VI_ registers to get/set their values. */
#define VI_REGISTERS      ((volatile uint32_t*)VI_REGISTERS_ADDR)
/** @brief VI Index register of controlling general display filters/bitdepth configuration */
#define VI_CTRL           (&VI_REGISTERS[0])
/** @brief VI Index register of RDRAM base address of the video output Frame Buffer. This can be changed as needed to implement double or triple buffering. */
#define VI_ORIGIN         (&VI_REGISTERS[1])
/** @brief VI Index register of width in pixels of the frame buffer. */
#define VI_WIDTH          (&VI_REGISTERS[2])
/** @brief VI Index register of vertical interrupt. */
#define VI_V_INTR         (&VI_REGISTERS[3])
/** @brief VI Index register of the current half line, sampled once per line. */
#define VI_V_CURRENT      (&VI_REGISTERS[4])
/** @brief VI Index register of sync/burst values */
#define VI_BURST          (&VI_REGISTERS[5])
/** @brief VI Index register of total visible and non-visible lines. 
 * This should match either NTSC (non-interlaced: 0x20D, interlaced: 0x20C) or PAL (non-interlaced: 0x271, interlaced: 0x270) */
#define VI_V_SYNC         (&VI_REGISTERS[6])
/** @brief VI Index register of total width of a line */
#define VI_H_SYNC         (&VI_REGISTERS[7])
/** @brief VI Index register of an alternate scanline length for one scanline during vsync. */
#define VI_H_SYNC_LEAP    (&VI_REGISTERS[8])
/** @brief VI Index register of start/end of the active video image, in screen pixels */
#define VI_H_VIDEO        (&VI_REGISTERS[9])
/** @brief VI Index register of start/end of the active video image, in screen half-lines. */
#define VI_V_VIDEO        (&VI_REGISTERS[10])
/** @brief VI Index register of start/end of the color burst enable, in half-lines. */
#define VI_V_BURST        (&VI_REGISTERS[11])
/** @brief VI Index register of horizontal subpixel offset and 1/horizontal scale up factor. */
#define VI_X_SCALE        (&VI_REGISTERS[12])
/** @brief VI Index register of vertical subpixel offset and 1/vertical scale up factor. */
#define VI_Y_SCALE        (&VI_REGISTERS[13])

/** @brief VI register by index (0-13)*/
#define VI_TO_REGISTER(index) (((index) >= 0 && (index) <= VI_REGISTERS_COUNT)? &VI_REGISTERS[index] : NULL)

/** @brief VI index from register */
#define VI_TO_INDEX(reg) ((reg) - VI_REGISTERS)

/** Under VI_CTRL */

/** @brief VI_CTRL Register setting: enable dedither filter. */
#define VI_DEDITHER_FILTER_ENABLE           (1<<16)
/** @brief VI_CTRL Register setting: default value for pixel advance. */
#define VI_PIXEL_ADVANCE_DEFAULT            (0b0011 << 12)
/** @brief VI_CTRL Register setting: default value for pixel advance on iQue. */
#define VI_PIXEL_ADVANCE_BBPLAYER           (0b0001 << 12)
/** @brief VI_CTRL Register setting: disable AA / resamp. */
#define VI_AA_MODE_NONE                     (0b11 << 8)
/** @brief VI_CTRL Register setting: disable AA / enable resamp. */
#define VI_AA_MODE_RESAMPLE                 (0b10 << 8)
/** @brief VI_CTRL Register setting: enable AA / enable resamp, fetch pixels when needed. */
#define VI_AA_MODE_RESAMPLE_FETCH_NEEDED    (0b01 << 8)
/** @brief VI_CTRL Register setting: enable AA / enable resamp, fetch pixels always. */
#define VI_AA_MODE_RESAMPLE_FETCH_ALWAYS    (0b00 << 8)
/** @brief VI_CTRL Register setting: enable interlaced output. */
#define VI_CTRL_SERRATE                     (1<<6)
/** @brief VI_CTRL Register setting: enable divot filter (fixes 1 pixel holes after AA). */
#define VI_DIVOT_ENABLE                     (1<<4)
/** @brief VI_CTRL Register setting: enable gamma correction filter. */
#define VI_GAMMA_ENABLE                     (1<<3)
/** @brief VI_CTRL Register setting: enable gamma correction filter and hardware dither the least significant color bit on output. */
#define VI_GAMMA_DITHER_ENABLE              (1<<2)
/** @brief VI_CTRL Register setting: framebuffer source format */
#define VI_CTRL_TYPE                        (0b11)
/** @brief VI_CTRL Register setting: set the framebuffer source as 32-bit. */
#define VI_CTRL_TYPE_32_BPP                 (0b11)
/** @brief VI_CTRL Register setting: set the framebuffer source as 16-bit (5-5-5-3). */
#define VI_CTRL_TYPE_16_BPP                 (0b10)
/** @brief VI_CTRL Register setting: set the framebuffer source as blank (no data and no sync, TV screens will either show static or nothing). */
#define VI_CTRL_TYPE_BLANK                  (0b00)

/** Under VI_ORIGIN  */
/** @brief VI_ORIGIN Register: set the address of a framebuffer. */
#define VI_ORIGIN_SET(value)                ((value & 0xFFFFFF) << 0)

/** Under VI_WIDTH   */
/** @brief VI_ORIGIN Register: set the width of a framebuffer. */
#define VI_WIDTH_SET(value)                 ((value & 0xFFF) << 0)

/** Under VI_V_CURRENT  */
/** @brief VI_V_CURRENT Register: default value for vblank begin line. */
#define VI_V_CURRENT_VBLANK                 2

/** Under VI_V_INTR    */
/** @brief VI_V_INTR Register: set value for vertical interrupt. */
#define VI_V_INTR_SET(value)                ((value & 0x3FF) << 0)
/** @brief VI_V_INTR Register: default value for vertical interrupt. */
#define VI_V_INTR_DEFAULT                   0x3FF

/** Under VI_BURST     */
/** @brief VI_BURST Register: set start of color burst in pixels from hsync. */
#define VI_BURST_START(value)               ((value & 0x3FF) << 20)
/** @brief VI_BURST Register: set vertical sync width in half lines. */
#define VI_VSYNC_WIDTH(value)               ((value & 0xF)  << 16)
/** @brief VI_BURST Register: set color burst width in pixels. */
#define VI_BURST_WIDTH(value)               ((value & 0xFF) << 8)
/** @brief VI_BURST Register: set horizontal sync width in pixels. */
#define VI_HSYNC_WIDTH(value)               ((value & 0xFF) << 0)
/** @brief VI_BURST Register: set all values. */
#define VI_BURST_SET(burst_start, vsync_width, burst_width, hsync_width)                                \
                                            VI_BURST_START(burst_start) | VI_VSYNC_WIDTH(vsync_width) | \
                                            VI_BURST_WIDTH(burst_width) | VI_HSYNC_WIDTH(hsync_width)

/** @brief VI_BURST Register: NTSC default start of color burst in pixels from hsync. */
#define VI_BURST_START_NTSC                 62
/** @brief VI_BURST Register: NTSC default vertical sync width in half lines. */
#define VI_VSYNC_WIDTH_NTSC                 5
/** @brief VI_BURST Register: NTSC default color burst width in pixels. */
#define VI_BURST_WIDTH_NTSC                 34
/** @brief VI_BURST Register: NTSC default horizontal sync width in pixels. */
#define VI_HSYNC_WIDTH_NTSC                 57
/** @brief VI_BURST Register: NTSC default setting. */
#define VI_BURST_NTSC                       VI_BURST_SET(VI_BURST_START_NTSC, VI_VSYNC_WIDTH_NTSC, VI_BURST_WIDTH_NTSC, VI_HSYNC_WIDTH_NTSC)

/** @brief VI_BURST Register: PAL default start of color burst in pixels from hsync. */
#define VI_BURST_START_PAL                  64
/** @brief VI_BURST Register: PAL default vertical sync width in half lines. */
#define VI_VSYNC_WIDTH_PAL                  4
/** @brief VI_BURST Register: PAL default color burst width in pixels.  */
#define VI_BURST_WIDTH_PAL                  35
/** @brief VI_BURST Register: PAL default horizontal sync width in pixels. */
#define VI_HSYNC_WIDTH_PAL                  58
/** @brief VI_BURST Register: PAL default setting. */
#define VI_BURST_PAL                        VI_BURST_SET(VI_BURST_START_PAL, VI_VSYNC_WIDTH_PAL, VI_BURST_WIDTH_PAL, VI_HSYNC_WIDTH_PAL)

/** @brief VI_BURST Register: MPAL default start of color burst in pixels from hsync. */
#define VI_BURST_START_MPAL                 70
/** @brief VI_BURST Register: MPAL default vertical sync width in half lines. */
#define VI_VSYNC_WIDTH_MPAL                 5
/** @brief VI_BURST Register: MPAL default color burst width in pixels.  */
#define VI_BURST_WIDTH_MPAL                 30
/** @brief VI_BURST Register: MPAL default horizontal sync width in pixels. */
#define VI_HSYNC_WIDTH_MPAL                 57
/** @brief VI_BURST Register: MPAL default setting. */
#define VI_BURST_MPAL                       VI_BURST_SET(VI_BURST_START_MPAL, VI_VSYNC_WIDTH_MPAL, VI_BURST_WIDTH_MPAL, VI_HSYNC_WIDTH_MPAL)

/**  Under VI_V_SYNC */
/** @brief VI_V_SYNC Register: set the total number of visible and non-visible half-lines (-1). */
#define VI_V_SYNC_SET(vsync)                (vsync)

/**  Under VI_H_SYNC */
/** @brief VI_H_SYNC Register: set the total width of a line in quarter-pixel units (-1), and the 5-bit leap pattern. */
#define VI_H_SYNC_SET(leap_pattern, hsync)  ((((leap_pattern) & 0x1F) << 16) | ((hsync) & 0xFFF))

/**  Under VI_H_SYNC_LEAP */
/** @brief VI_H_SYNC_LEAP Register: set alternate scanline lengths for one scanline during vsync, leap_a and leap_b are selected based on the leap pattern in VI_H_SYNC. */
#define VI_H_SYNC_LEAP_SET(leap_a, leap_b)  ((((leap_a) & 0xFFF) << 16) | ((leap_b) & 0xFFF))

/**  Under VI_H_VIDEO */
/** @brief VI_H_VIDEO Register: set the horizontal start and end of the active video area, in screen pixels */
#define VI_H_VIDEO_SET(start, end)          ((((start) & 0x3FF) << 16) | ((end) & 0x3FF))

/**  Under VI_V_VIDEO */
/** @brief VI_V_VIDEO Register: set the vertical start and end of the active video area, in half-lines */
#define VI_V_VIDEO_SET(start, end)          ((((start) & 0x3FF) << 16) | ((end) & 0x3FF))

/**  Under VI_V_BURST */
/** @brief VI_V_BURST Register: set the start and end of color burst enable, in half-lines */
#define VI_V_BURST_SET(start, end)          ((((start) & 0x3FF) << 16) | ((end) & 0x3FF))

/**  Under VI_X_SCALE   */
/** @brief VI_X_SCALE Register: set 1/horizontal scale up factor (value is converted to 2.10 format) */
#define VI_X_SCALE_SET(value)               (( 1024*(value) + 320 ) / 640)

/**  Under VI_Y_SCALE   */
/** @brief VI_Y_SCALE Register: set 1/vertical scale up factor (value is converted to 2.10 format) */
#define VI_Y_SCALE_SET_240_LINES(value)               (( 1024*(value) + 120 ) / 240)

/**  Under VI_Y_SCALE   */
/** @brief VI_Y_SCALE Register: set 1/vertical scale up factor (value is converted to 2.10 format) */
#define VI_Y_SCALE_SET_288_LINES(value)               (( 1024*(value) + 144 ) / 288)

/** @brief VI period for showing one NTSC and MPAL picture in ms. */
#define VI_PERIOD_NTSC_MPAL                 ((float)1000/60)
/** @brief VI period for showing one PAL picture in ms. */
#define VI_PERIOD_PAL                       ((float)1000/50)

/**
 * @name Video Mode Register Presets
 * @brief Presets to begin with when setting a particular video mode
 * @{
 */
static const vi_config_t vi_ntsc_p = {.regs = {
    0,
    VI_ORIGIN_SET(0),
    VI_WIDTH_SET(0),
    VI_V_INTR_SET(2),
    0,
    VI_BURST_NTSC,
    VI_V_SYNC_SET(525),
    VI_H_SYNC_SET(0b00000, 3093),
    VI_H_SYNC_LEAP_SET(3093, 3093),
    VI_H_VIDEO_SET(108, 748),
    VI_V_VIDEO_SET(35, 515),
    VI_V_BURST_SET(14, 516),
    VI_X_SCALE_SET(0),
    VI_Y_SCALE_SET_240_LINES(0),
}};
static const vi_config_t vi_pal_p =  {.regs = {
    0,
    VI_ORIGIN_SET(0),
    VI_WIDTH_SET(0),
    VI_V_INTR_SET(2),
    0,
    VI_BURST_PAL,
    VI_V_SYNC_SET(625),
    VI_H_SYNC_SET(0b10101, 3177),
    VI_H_SYNC_LEAP_SET(3183, 3182),
    VI_H_VIDEO_SET(128, 768),
    VI_V_VIDEO_SET(45, 621),
    VI_V_BURST_SET(9, 619),
    VI_X_SCALE_SET(0),
    VI_Y_SCALE_SET_288_LINES(0),
}};
static const vi_config_t vi_mpal_p = {.regs = {
    0,
    VI_ORIGIN_SET(0),
    VI_WIDTH_SET(0),
    VI_V_INTR_SET(2),
    0,
    VI_BURST_MPAL,
    VI_V_SYNC_SET(525),
    VI_H_SYNC_SET(0b00100, 3089),
    VI_H_SYNC_LEAP_SET(3097, 3098),
    VI_H_VIDEO_SET(108, 748),
    VI_V_VIDEO_SET(37, 511),
    VI_V_BURST_SET(14, 516),
    VI_X_SCALE_SET(0),
    VI_Y_SCALE_SET_240_LINES(0)
}};
static const vi_config_t vi_ntsc_i = {.regs = {
    0,
    VI_ORIGIN_SET(0),
    VI_WIDTH_SET(0),
    VI_V_INTR_SET(2),
    0,
    VI_BURST_NTSC,
    VI_V_SYNC_SET(524),
    VI_H_SYNC_SET(0b00000, 3093),
    VI_H_SYNC_LEAP_SET(3093, 3093),
    VI_H_VIDEO_SET(108, 748),
    VI_V_VIDEO_SET(35, 515),
    VI_V_BURST_SET(14, 516),
    VI_X_SCALE_SET(0),
    VI_Y_SCALE_SET_240_LINES(0)
}};
static const vi_config_t vi_pal_i = {.regs = {
    0,
    VI_ORIGIN_SET(0),
    VI_WIDTH_SET(0),
    VI_V_INTR_SET(2),
    0,
    VI_BURST_PAL,
    VI_V_SYNC_SET(624),
    VI_H_SYNC_SET(0b10101, 3177),
    VI_H_SYNC_LEAP_SET(3183, 3182),
    VI_H_VIDEO_SET(128, 768),
    VI_V_VIDEO_SET(45, 621),
    VI_V_BURST_SET(9, 619),
    VI_X_SCALE_SET(0),
    VI_Y_SCALE_SET_288_LINES(0)
}};
static const vi_config_t vi_mpal_i = {.regs = {
    0,
    VI_ORIGIN_SET(0),
    VI_WIDTH_SET(0),
    VI_V_INTR_SET(2),
    0,
    VI_BURST_MPAL,
    VI_V_SYNC_SET(524),
    VI_H_SYNC_SET(0b00000, 3088),
    VI_H_SYNC_LEAP_SET(3100, 3100),
    VI_H_VIDEO_SET(108, 748),
    VI_V_VIDEO_SET(35, 509),
    VI_V_BURST_SET(11, 514),
    VI_X_SCALE_SET(0),
    VI_Y_SCALE_SET_240_LINES(0)
}};
/** @} */

/** @brief Register initial value array */
static const vi_config_t vi_config_presets[2][3] = {
    {vi_pal_p, vi_ntsc_p, vi_mpal_p},
    {vi_pal_i, vi_ntsc_i, vi_mpal_i}
};

/**
 * @brief Write a set of video registers to the VI
 *
 * @param[in] reg
 *            A pointer to a register to be written
 * @param[in] value
 *            Value to be written to the register
 */
inline void vi_write_safe(volatile uint32_t *reg, uint32_t value){
    assert(reg); /* This should never happen */
    *reg = value;
    MEMORY_BARRIER();
}

/**
 * @brief Write a set of video registers to the VI
 *
 * @param[in] config
 *            A pointer to a set of register values to be written
 */
inline void vi_write_config(const vi_config_t* config)
{
    /* This should never happen */
    assert(config);

    /* Just straight copy */
    for( int i = 0; i < VI_REGISTERS_COUNT; i++ )
    {
        /* Don't clear interrupts */
        if( i == 3 ) { continue; }
        if( i == 4 ) { continue; }

        *VI_TO_REGISTER(i) = config->regs[i];
        MEMORY_BARRIER();
    }
}

/**
 * @brief Update the framebuffer pointer in the VI
 *
 * @param[in] dram_val
 *            The new framebuffer to use for display.  Should be aligned and uncached.
 */
static inline void vi_write_dram_register( void const * const dram_val )
{
    *VI_ORIGIN = VI_ORIGIN_SET(PhysicalAddr(dram_val));
    MEMORY_BARRIER();
}

/** @brief Wait until entering the vblank period */
static inline void vi_wait_for_vblank()
{
    while(*VI_V_CURRENT != VI_V_CURRENT_VBLANK ) {  }
}

/** @brief Return true if VI is sending a video signal (16-bit or 32-bit color set) */
static inline bool vi_is_active()
{
    return (*VI_CTRL & VI_CTRL_TYPE) != VI_CTRL_TYPE_BLANK;
}

/** @brief Set active image width to 0, which keeps VI signal active but only sending a blank image */
static inline void vi_set_blank_image()
{
    vi_write_safe(VI_H_VIDEO, 0);
}


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

/** @} */ /* vi */

#endif
