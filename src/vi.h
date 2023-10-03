/**
 * @file vi.h
 * @brief Video Interface Subsystem
 * @ingroup display
 */
#ifndef __LIBDRAGON_VI_H
#define __LIBDRAGON_VI_H


#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include "n64sys.h"
#include "utils.h"
#include "debug.h"
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

/**
 * @brief Video Interface borders structure
 *
 * This structure contains how thick should the borders around a framebuffer be in dots.
 * The dots assume VI scale (640x480 NTSC or 640x576 PAL).
 * They can be of different sizes and the framebuffer will be scaled to fit under them
 * For example when shown on CRT monitors, one can add borders around a framebuffer so
 * that the whole image could be seen on the monitor. 
 * 
 * Or if no borders are applied, 
 * the output will use the whole NSTC/PAL region space for showing a framebuffer, useful
 * for accurate emulators and LCD TV's.
 */
typedef struct vi_borders_s{
    float left, right, up, down;
} vi_borders_t;

/** @brief No borders are applied, 
 * the output will use the whole NSTC/PAL region space for showing a framebuffer */
#define VI_BORDERS_NONE (vi_borders_t){0, 0, 0, 0};
/** @brief Add borders around a framebuffer so
 * that the whole image could be seen on the CRT monitor */
#define VI_BORDERS_CRT  (vi_borders_t){26, 26, 26, 26};

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

/**
 * @name Video Mode Register Presets
 * @brief Presets to begin with when setting a particular video mode
 * @{
 */
static const vi_config_t vi_ntsc_p = {.regs = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x03e52239, 0x0000020d, 0x00000c15,
    0x0c150c15, 0x006402f3, 0x00230203, 0x000e0204,
    0x00000000, 0x00000000 }};
static const vi_config_t vi_pal_p =  {.regs = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x0404233a, 0x00000271, 0x00150c69,
    0x0c6f0c6e, 0x00780307, 0x002D026D, 0x0009026b,
    0x00000000, 0x00000000 }};
static const vi_config_t vi_mpal_p = {.regs = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x04651e39, 0x0000020d, 0x00040c11,
    0x0c190c1a, 0x006402f3, 0x00230203, 0x000e0204,
    0x00000000, 0x00000000 }};

static const vi_config_t vi_ntsc_i = {.regs = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x03e52239, 0x0000020c, 0x00000c15,
    0x0c150c15, 0x006402f3, 0x00220202, 0x000e0204,
    0x00000000, 0x00000000 }};
static const vi_config_t vi_pal_i = {.regs = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x0404233a, 0x00000270, 0x00150c69,
    0x0c6f0c6e, 0x00780307, 0x002D026D, 0x0009026b,
    0x00000000, 0x00000000 }};
static const vi_config_t vi_mpal_i = {.regs = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x04651e39, 0x0000020c, 0x00000c10,
    0x0c1c0c1c, 0x006402f3, 0x00220202, 0x000b0202,
    0x00000000, 0x00000000 }};

/** @} */

/** @brief Register initial value array */
static const vi_config_t vi_config_presets[2][3] = {
    {vi_pal_p, vi_ntsc_p, vi_mpal_p},
    {vi_pal_i, vi_ntsc_i, vi_mpal_i}
};

/** Under VI_CTRL */

/** @brief VI_CTRL Register setting: enable dedither filter. */
#define VI_DEDITHER_FILTER_ENABLE           (1<<16)
/** @brief VI_CTRL Register setting: default value for pixel advance. */
#define VI_PIXEL_ADVANCE_DEFAULT            (0b0011 << 12)
/** @brief VI_CTRL Register setting: default value for pixel advance on iQue. */
#define VI_PIXEL_ADVANCE_BBPLAYER           (0b0001 << 12)
/** @brief VI_CTRL Register setting: AA / resamp mask */
#define VI_AA_MODE_MASK                     (0b11 << 8)
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
#define VI_BURST_START(value)               ((value & 0x3F) << 20)
/** @brief VI_BURST Register: set vertical sync width in half lines. */
#define VI_VSYNC_WIDTH(value)               ((value & 0x7)  << 16)
/** @brief VI_BURST Register: set color burst width in pixels. */
#define VI_BURST_WIDTH(value)               ((value & 0xFF) << 8)
/** @brief VI_BURST Register: set horizontal sync width in pixels. */
#define VI_HSYNC_WIDTH(value)               ((value & 0xFF) << 0)

/** @brief VI_BURST Register: NTSC default start of color burst in pixels from hsync. */
#define VI_BURST_START_NTSC                 62
/** @brief VI_BURST Register: NTSC default vertical sync width in half lines. */
#define VI_VSYNC_WIDTH_NTSC                 5
/** @brief VI_BURST Register: NTSC default color burst width in pixels. */
#define VI_BURST_WIDTH_NTSC                 34
/** @brief VI_BURST Register: NTSC default horizontal sync width in pixels. */
#define VI_HSYNC_WIDTH_NTSC                 57

/** @brief VI_BURST Register: PAL default start of color burst in pixels from hsync. */
#define VI_BURST_START_PAL                  64
/** @brief VI_BURST Register: PAL default vertical sync width in half lines. */
#define VI_VSYNC_WIDTH_PAL                  4
/** @brief VI_BURST Register: PAL default color burst width in pixels.  */
#define VI_BURST_WIDTH_PAL                  35
/** @brief VI_BURST Register: PAL default horizontal sync width in pixels. */
#define VI_HSYNC_WIDTH_PAL                  58

/**  Under VI_X_SCALE   */
/** @brief VI_X_SCALE Register: set 1/horizontal scale up factor (value is converted to 2.10 format) */
#define VI_X_SCALE_SET(value)               (( 1024*(value) + 320 ) / 640)

/**  Under VI_Y_SCALE   */
/** @brief VI_Y_SCALE Register: set 1/vertical scale up factor (value is converted to 2.10 format) */
#define VI_Y_SCALE_SET(value)               (( 1024*(value) + 120 ) / 240)

/** @brief VI_SCALE Registers: set 1/horizontal scale up factor (value is converted to 2.10 format) */
#define VI_SCALE_FROMTO(from,to)            (( 1024*(from) + (to)/2 ) / (to))

/** @brief VI_OFFSET Registers: Horizontal subpixel offset (value is converted to 2.10 format) */
#define VI_OFFSET_SET(value)              (((int)( 1024*(value)) & 0xFFFF) << 16)

/**
 * @brief Write a register value to the Video Interface in a VBLANK period (unfinished)
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
 * @brief Write a register value to the Video Interface in a VI immediately
 *
 * @param[in] reg
 *            A pointer to a register to be written
 * @param[in] value
 *            Value to be written to the register
 */
inline void vi_write(volatile uint32_t *reg, uint32_t value){
    assert(reg); /* This should never happen */
    *reg = value;
    MEMORY_BARRIER();
}

// VI has a hardware bug that causes it to throw away (= display black) the fist 8 dots
// and the last 7 dots of the active display area. Notice that within these dots, 
// VI consider the area still active, so the X counter is incremented and the framebuffer
// sampled, but the output is then discarded.
// We need to account for this when calculating the resolution parameters because we
// don't want to crop our framebuffer, but rather display it correctly skipping the
// hidden dots.
#define VI_HIDDEN_DOTS_LEFT     8       ///< Black dots on the left side of the display area
#define VI_HIDDEN_DOTS_RIGHT    7       ///< Black dots on the right side of the display area

#define VI_ORIGIN_ALIGN         4       ///< VI_ORIGIN must be aligned to 4 pixels

/**
 * @brief Video Interface scaling and positioning of the output sampling
 *
 * Whenever trying to configure VI registers and calculating precise 
 * transformations, this struct can be very useful in writing comprehensive 
 * and verbose code.
 */
typedef struct {
    int fb_width;               ///< Width of the framebuffer (pixels)
    int h_start;                ///< Start of the active video image (dots)
    int h_end;                  ///< End of the active video image (dots)
    int vi_origin_offset;       ///< Offset to apply to VI_ORIGIN (pixels)
    int x_offset_fx10;          ///< Initial X counter (2.10)
    int x_scale_fx10;           ///< X counter increment (2.10)
} vi_resparms_t;

/**
 * @brief Return the transformation configuration an output framebuffer 
 * needs to use for a perfect output and midigate a inherit VI offset bug.
 *
 * @param[in] fb_width
 *            Width of the framebuffer in pixels
 * @param[in] h_start
 *            Where the VI starts to sample the framebuffer in dots
 * @param[in] h_end
 *            Where the VI stops to sample the framebuffer in dots
 * @return    The transformation configuration needed for the precise output
 */
inline vi_resparms_t vi_calc_resparms(int fb_width, int h_start, int h_end, bool resample) {
    const int FX10 = 1<<10;
    vi_resparms_t resparms = { .fb_width = fb_width, .h_start = h_start, .h_end = h_end };

    // Calculate the width of the active display area. Immediately remove
    // the right dots from it, as those can be safely be ignored for the rest
    // of the calculations.
    int display_width = h_end - h_start - VI_HIDDEN_DOTS_RIGHT;

    // Calculate the first "theoretical" XSCALE value
    resparms.x_scale_fx10 = (fb_width * FX10) / display_width;

    // Calculate how many pixels in the framebuffer would be invisible
    // because of the hidden dots. We will need move the framebuffer right
    // by this amount to avoid any cropping.
    int offset_fx10 = VI_HIDDEN_DOTS_LEFT * resparms.x_scale_fx10;

    // To move the framebuffer out of the hidden area, we can combine both
    // VI_ORIGIN (that can be offset in steps by 4 pixels because of alignment
    // constraints) and X_OFFSET, which is a fractional value in 2.10 format
    // so in the range [0..4). 
    resparms.vi_origin_offset = ROUND_UP(offset_fx10, VI_ORIGIN_ALIGN*FX10) / FX10;
    resparms.x_offset_fx10 = resparms.vi_origin_offset*FX10 - offset_fx10;
    #if 0
    debugf("fb_width=%d display_width=%d coarse=%d precise=%f\n", fb_width, display_width, resparms.vi_origin_offset, resparms.x_offset_fx10/1024.0f);
    #endif

    // Now let's calculate the final XSCALE. We previously calculated
    // fb_width/display_width but we now want a slightly different value that make
    // sure that the last pixel of the framebuffer is as centered as possible.
    // In case there is no resampling, this can be done by simply dividing the
    // framebuffer width by the display width, taking into account the offsets
    // we just calculated. We can ceil the result to go as far as possible into
    // the last pixel.
    if (!resample) {
        resparms.x_scale_fx10 = ((resparms.fb_width + resparms.vi_origin_offset)*FX10 - resparms.x_offset_fx10 + FX10 - 1) / display_width;
        return resparms;
    }

    // If there's resampling, this is the trickiest part. 
    // In fact, when resampling is active, unfortunately the VI will interpolate
    // between the last pixel and the whatever garbage is after it (usually, the first
    // pixel of next line). So we want the VI X value for the last pixel to be
    // as close as possible to fb_width-1, and without going over it to avoid
    // bleeding garbage in.
    resparms.x_scale_fx10 = (((resparms.fb_width-1) + resparms.vi_origin_offset)*FX10 - resparms.x_offset_fx10) / (display_width-1);
    
    // Let's now check the VI X value for the first and last framebuffer pixel, with the new
    // XSCALE we just computed
    int first_pixel_fx10 = resparms.x_offset_fx10 + VI_HIDDEN_DOTS_LEFT * resparms.x_scale_fx10 - resparms.vi_origin_offset*FX10;
    int last_pixel_fx10 =  resparms.x_offset_fx10 + (display_width-1)   * resparms.x_scale_fx10 - resparms.vi_origin_offset*FX10;
    #if 0
    debugf("hscale=%f first_pixel=%.2f last_pixel=%.2f\n", 
        resparms.x_scale_fx10/1024.0f, first_pixel_fx10/10240.f, last_pixel_fx10/1024.f);
    #endif

    // The goal was to made them as close as possible to 0 and fb_width-1, respectively,
    // while staying *within* the framebuffer.
    // Let's extract the approximation error
    int first_pixel_error = first_pixel_fx10 - 0;
    int last_pixel_error  = (resparms.fb_width-1)*FX10 - last_pixel_fx10;

    // If we did our computations correctly, both of these should be positive,
    // which means that neither of them will interpolate with a garbage value
    // before/after the framebuffer.
    assert(first_pixel_error >= 0 && last_pixel_error >= 0);

    // As a last step, we want to rebalance the error between the first and last pixel
    // so that we minimize the error on both sides.
    resparms.x_offset_fx10 += (last_pixel_error - first_pixel_error) / 2;

    #if 0
    debugf("precise2=%f first_pixel=%.2f last_pixel=%.2f\n", 
        resparms.x_offset_fx10/1024.f, 
        (resparms.x_offset_fx10 + VI_HIDDEN_DOTS_LEFT*resparms.x_scale_fx10)/1024.0f - resparms.vi_origin_offset, 
        (resparms.x_offset_fx10 + (display_width-1) * resparms.x_scale_fx10)/1024.0f - resparms.vi_origin_offset);
    #endif

    return resparms;
}

/**
 * @brief Write a set of video registers to the VI 
 * to configure display position, scale and bordering.
 *
 * @param[in] fb_width
 *            Width of the framebuffer to display in pixels
 * @param[in] fb_height
 *            Height of the framebuffer to display in pixels
 * @param[in] serrate
 *            Is serrate interlacing enabled for this display
 * @param[in] resample
 *            True if resampling is enabled, false if it's disabled
 * @param[in] aspect
 *            Pixel aspect ratio of the framebuffer within the borders
 *            (<=0 if this calculation needs to be skipped, stretching the framebuffer)
 * @param[in] borders
 *            Minimum border size in dots around the framebuffer
 */
static inline void vi_write_display(uint32_t fb_width, uint32_t fb_height, bool serrate, bool resample, float aspect, vi_borders_t borders){
    uint32_t tv_type = get_tv_type();
    const vi_config_t* config = &vi_config_presets[serrate][tv_type];

    uint16_t h_start, h_end, v_start, v_end;
    {
        // Get a borderless video config for the type of TV output we have
        uint32_t h_video = config->regs[VI_TO_INDEX(VI_H_VIDEO)];
        uint32_t v_video = config->regs[VI_TO_INDEX(VI_V_VIDEO)];
        // Extract values out of the config
        h_start = (h_video >> 16) & 0x3FF;
        h_end   =  h_video        & 0x3FF;
        v_start = (v_video >> 16) & 0x3FF;
        v_end   =  v_video        & 0x3FF;
    }

    // Add minimum borders
    h_start += borders.left;
    h_end   -= borders.right;
    v_start += borders.up;
    v_end   -= borders.down;

    // Calculate the active area of display
    int display_width = h_end - h_start - VI_HIDDEN_DOTS_LEFT - VI_HIDDEN_DOTS_RIGHT;
    int display_height = v_end - v_start;

    // Add aspect ratio borders
    if(aspect > 0) {
        float viaspect = (float)display_width / (float)display_height; // Calculate the VI aspect ratio after applying minumum borders
        if(aspect > viaspect){
            float factor = viaspect / aspect; // Get a vertical scale factor in range (0.0;1.0)
            float v_arborder = (display_height - (display_height * factor)) / 2;
            v_start += v_arborder;
            v_end -= v_arborder;
        } else {
            float factor = aspect / viaspect; // Get a horizontal scale factor in range (0.0;1.0)
            float h_arborder = (display_width - (display_width * factor)) / 2;
            h_start += h_arborder;
            h_end -= h_arborder;
        }
        display_height = v_end - v_start; // Get the new display height for scaling
    }

    // Miscellaneous edge-case fixes
    float v_offset = 0; // Should be used for subpixel precision, but it's not straightforward, so leave it as 0 for now
    vi_resparms_t resparms = vi_calc_resparms(fb_width, h_start, h_end, resample);

    // Scale and set the boundaries of the framebuffer to fit bordered layout 
    vi_write(VI_X_SCALE, (resparms.x_offset_fx10 << 16) | resparms.x_scale_fx10);
    vi_write(VI_Y_SCALE, VI_OFFSET_SET(v_offset) | (uint16_t)VI_SCALE_FROMTO(fb_height,display_height/2));

    // Finally write the dimentions of the output
    vi_write(VI_H_VIDEO, (resparms.h_start << 16) | resparms.h_end);
    vi_write(VI_V_VIDEO, (v_start << 16) | (v_end));
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
static inline void vi_write_image_is_blank()
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
