/**
 * @file vi.h
 * @brief Video Interface Subsystem
 * @ingroup display
 * 
 * This module offers a low-level interface to VI programming. Most applications
 * should use display.h, which sits on top of vi.h, and offers a higher level
 * interface, plus automatic memory management of multiple framebuffers,
 * FPS utilities, and much more.
 * 
 * ## Register read/write access
 * 
 * This module defines a set of macros to directly access the VI registers,
 * including macros to compose different fields in the registers. While read
 * access is straightforward, write access is generally more complicated
 * because VI is quite picky about the timing of register writes: most registers
 * can only be written during vblank. 
 * 
 * To help with that, this module provides a set of functions to write registers
 * that will be applied at the next vblank: #vi_write and #vi_write_masked.
 * To batch multiple register writes, you can use #vi_write_begin and #vi_write_end,
 * so that you can be sure that all the registers will be written atomically
 * at the next vblank.
 * 
 * #vi_read can be used to read the current state of a register, including any
 * pending changes (applied via #vi_write) that were not yet applied.
 * 
 * ## Higher level APIs
 * 
 * This module also includes some higher level APIs to configure the VI:
 * 
 * * #vi_show is a simple function to just show a #surface_t to the screen.
 * * #vi_set_origin, #vi_set_xscale and #vi_set_yscale are more granular
 *   functions to configure the framebuffer.
 * * #vi_set_aa_mode, #vi_set_divot, #vi_set_dedither and #vi_set_gamma
 *   are functions to configure various filters and modes of the VI.
 * * #vi_set_interlaced is a function to turn on and off interlaced display mode.
 * * #vi_set_output defines the active display area on the screen.
 *   #vi_get_output_bounds will provide the limits for it, while
 *   #vi_move_output and #vi_scroll_output can be used to just pan the display.
 * * #vi_set_borders is an alternative to #vi_set_output, that specifies the
 *   display area in terms of borders (with respect to the default one).
 * 
 * ## Active display area and framebuffer scaling
 * 
 * VI has a powerful resampling engine: the framebuffer is not displayed as-is
 * on TV, but it is actually resampled (scaled, stretched), optionally with
 * bilinear filtering. This is a very powerful hardware feature that is a bit
 * complicated to configure, so an effort was made to expose an intuitive API
 * to programmers.
 *
 * By default, this module configures the VI to resample an arbitrary framebuffer
 * picture into a virtual 640x480 display output with 4:3 aspect ratio (on NTSC
 * and MPAL), or a virtual 640x576 display output though with the same 4:3
 * aspect ratio on PAL. This is done to achieve TV type independence: since
 * both resolution share the same aspect ratio on their respective TV standards
 * (given that dots are square on NTSC but they are not on PAL), it means
 * that the framebuffer will look the same on all TVs; on PAL, you will be
 * trading additional vertical resolution (assuming the framebuffer is big
 * enough to have that detail). 
 * 
 * This also means that games don't have to do anything special to handle NTSC
 * vs PAL (besides maybe accounting for the different refresh rate), which is
 * an important goal.
 * 
 * In reality, TVs didn't have a 480-lines vertical resolution so the actual
 * output depends on whether you request interlaced display or not:
 * 
 *  * In case of non interlaced display, the actual resolution is 640x240, but
 *    since dots will be configured to be twice as big vertically, the aspect
 *    ratio will be 4:3 as-if the image was 640x480 (with duplicated scanlines)
 *  * In case of interlaced display, you do get to display 480 scanlines, by
 *    alternating two slightly-shifted 640x240 pictures.
 * 
 * As an example, if you configure a framebuffer resolution like 512x320, with
 * interlacing turned off, what happens is that the image gets scaled into
 * 640x240, so horizontally some pixels will be duplicated to enlarge the
 * resolution to 640, but vertically some scanlines will be dropped. The
 * output display aspect ratio will still be 4:3, which is not the source aspect
 * ratio of the framebuffer (512 / 320 = 1.6666 = 16:10), so the image will
 * appear squished, unless obviously this was accounted for while drawing to
 * the framebuffer.
 * 
 * While resampling the framebuffer into the display output, the VI can use either
 * bilinear filtering or simple nearest sampling (duplicating or dropping pixels).
 * See #vi_aa_mode_t for more information on configuring the VI image filters.
 * 
 * The 640x480 virtual display output can be fully viewed on emulators and on
 * modern screens (via grabbers, converters, etc.). When displaying on old
 * CRTs though, part of the display will be hidden because of the overscan.
 * To account for this, you can configure a smaller display output via two
 * different APIs which are just two ways to express the same concept:
 * 
 *  * #vi_set_borders: this function configures the display output on the screen
 *    by specifying the size of the borders, with respect to the default 640x480
 *    display output.
 *  * #vi_set_output: this function configures the display output on the screen
 *    by specifying the top-left and bottom-right corners of the display area,
 *    as absolute coordinates in the VI coordinate system.
 * 
 * For instance, if you specify 12 dots of borders on all the four edges, you
 * will get a 616x456 display output (on NTSC), plus the requested 12 dots of
 * borders on all sides; The same effect can be obtained by specifying the
 * display output area as (120, 47) - (736, 503) with #vi_set_output. This
 * is because the default display area on NTSC is (108, 35) - (748, 515)
 * (which can be found out via #vi_get_output).
 * 
 * Notice that adding borders also affects the aspect ratio of the display output;
 * for instance, in the above example, the 616x456 display output is not
 * exactly 4:3 anymore, but more like 4.05:3. By carefully calculating borders,
 * thus, it is possible to obtain specific display outputs with custom aspect
 * ratios (eg: 16:9).
 * 
 * To help calculating the borders by taking both potential goals into account
 * (overscan compensation and aspect ratio changes), you can use #vi_calc_borders.
 */
#ifndef __LIBDRAGON_VI_H
#define __LIBDRAGON_VI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct surface_s surface_t;
///@endcond

/** @brief Number of useful 32-bit registers at the register base */
#define VI_REGISTERS_COUNT      14

/** @brief Base pointer to hardware Video interface registers that control various aspects of VI configuration.
 * Shouldn't be used by itself, use VI_ registers to get/set their values. */
#define VI_REGISTERS      ((volatile uint32_t*)0xA4400000)
/** @brief VI register of controlling general display filters/bitdepth configuration */
#define VI_CTRL           (&VI_REGISTERS[0])
/** @brief VI register of RDRAM base address of the video output Frame Buffer. */
#define VI_ORIGIN         (&VI_REGISTERS[1])
/** @brief VI register of width in pixels of the frame buffer. */
#define VI_WIDTH          (&VI_REGISTERS[2])
/** @brief VI register of vertical interrupt. */
#define VI_V_INTR         (&VI_REGISTERS[3])
/** @brief VI register of the current half line, sampled once per line. */
#define VI_V_CURRENT      (&VI_REGISTERS[4])
/** @brief VI register of sync/burst values */
#define VI_BURST          (&VI_REGISTERS[5])
/** @brief VI register of total visible and non-visible lines. */
#define VI_V_TOTAL        (&VI_REGISTERS[6])
/** @brief VI register of total width of a line */
#define VI_H_TOTAL        (&VI_REGISTERS[7])
/** @brief VI register of an alternate scanline length for one scanline during vsync. */
#define VI_H_TOTAL_LEAP   (&VI_REGISTERS[8])
/** @brief VI register of start/end of the active video image, in screen pixels */
#define VI_H_VIDEO        (&VI_REGISTERS[9])
/** @brief VI register of start/end of the active video image, in screen half-lines. */
#define VI_V_VIDEO        (&VI_REGISTERS[10])
/** @brief VI register of start/end of the color burst enable, in half-lines. */
#define VI_V_BURST        (&VI_REGISTERS[11])
/** @brief VI register of horizontal subpixel offset and 1/horizontal scale up factor. */
#define VI_X_SCALE        (&VI_REGISTERS[12])
/** @brief VI register of vertical subpixel offset and 1/vertical scale up factor. */
#define VI_Y_SCALE        (&VI_REGISTERS[13])

/** @brief VI register by index (0-13)*/
#define VI_TO_REGISTER(index) (((index) >= 0 && (index) <= VI_REGISTERS_COUNT)? &VI_REGISTERS[index] : NULL)

/** Under VI_CTRL */

/** @brief VI_CTRL Register setting: enable dedither filter. */
#define VI_DEDITHER_FILTER_ENABLE           (1<<16)
/** @brief VI_CTRL Register setting: default value for pixel advance. */
#define VI_PIXEL_ADVANCE_DEFAULT            (0b0011 << 12)
/** @brief VI_CTRL Register setting: default value for pixel advance on iQue. */
#define VI_PIXEL_ADVANCE_BBPLAYER           (0b0001 << 12)
/** @brief VI_CTRL Register setting: mask for AA mode configuration */
#define VI_AA_MODE_MASK                     (0b11 << 8)
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

/**  Under VI_V_TOTAL */
/** @brief VI_V_TOTAL Register: set the total number of visible and non-visible half-lines. */
#define VI_V_TOTAL_SET(vsync)               ((vsync)-1)

/**  Under VI_H_TOTAL */
/** @brief VI_H_TOTAL Register: set the total width of a line in quarter-pixel units (-1), and the 5-bit leap pattern. */
#define VI_H_TOTAL_SET(leap_pattern, hsync)  ((((leap_pattern) & 0x1F) << 16) | ((int)((hsync)*4-1) & 0xFFF))

/**  Under VI_H_TOTAL_LEAP */
/** @brief VI_H_TOTAL_LEAP Register: set alternate scanline lengths for one scanline during vsync, leap_a and leap_b are selected based on the leap pattern in VI_H_SYNC. */
#define VI_H_TOTAL_LEAP_SET(leap_a, leap_b)  ((((int)((leap_a)*4-1) & 0xFFF) << 16) | ((int)((leap_b)*4-1) & 0xFFF))

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
#define VI_X_SCALE_SET(from, to)            ((1024 * (from) + (to) / 2 ) / (to))

/**  Under VI_Y_SCALE   */
/** @brief VI_Y_SCALE Register: set 1/vertical scale up factor (value is converted to 2.10 format) */
#define VI_Y_SCALE_SET(from, to)            ((1024 * (from) + (to) / 2 ) / (to))

///@cond
// Private symbols, do not use
extern uint32_t __vi_cfg[VI_REGISTERS_COUNT];
///@endcond

/**
 * @brief Video Interface borders structure
 *
 * This structure defines how thick (in dots) should the borders around
 * a framebuffer be.
 * 
 * The dots refer to the VI virtual display output (640x480, on both NTSC, PAL,
 * and M-PAL), and thus reduce the actual display output, and even potentially
 * modify the aspect ratio. The framebuffer will be scaled to fit under them.
 * 
 * For example, when displaying on CRT TVs, one can add borders around a
 * framebuffer so that the whole image can be seen on the screen. 
 * 
 * If no borders are applied, the output will use the entire virtual dsplay
 * output (640x480) for showing a framebuffer. This is useful for emulators,
 * upscalers, and LCD TVs.
 * 
 * Notice that borders can also be *negative*: this obtains the effect of
 * actually enlarging the output, growing from 640x480. Doing so will very
 * likely create problems with most TV grabbers and upscalers, but it might
 * work correctly on most CRTs (though the added pixels will surely be
 * part of the overscan so not really visible). Horizontally, the maximum display
 * output will probably be ~700-ish on CRTs, after which the sync will be lost.
 * Vertically, any negative number will likely create immediate syncing problems,
 */
typedef struct vi_borders_s {
    int16_t left, right, up, down;
} vi_borders_t;

/** 
 * @brief VI AA Mode
 * 
 * This setting configures the resampling and AA filters performed
 * by VI during the framebuffer display.
 * 
 * Notice that VI seems a bit incomplete / buggy in this area, so
 * not all filter-related settings are guaranteed to work in all
 * contexts. 
 */
typedef enum vi_aa_mode_e {
    /** 
     * @brief Disable the resampling and AA filter.
     * 
     * @note Because of a hardware bug on NTSC units, don't use
     *       this mode in 16 bpp mode and framebuffer widths lower or
     *       equal to 320 pixels.
     */
    VI_AA_MODE_NONE                  = (0b11 << 8),

    /**
     * @brief Enable bilinear filtering during resampling
     * 
     * With this setting, the VI will perform true 2x2 bilinear filtering
     * while resampling the framebuffer into the display output.
     * 
     * @note This mode is not compatible with dedithering
     *       (#vi_set_dedither).
     */
    VI_AA_MODE_RESAMPLE              = (0b10 << 8),

    /**
     * @brief Enable AA filter (in addition to bilinear resampling).
     * 
     * This filter is useful to reduce aliasing artifacts. It works by
     * smoothing pixels that represent external edges of polygons/meshes.
     * Since VI works on the final framebuffer picture, it has no knowledge
     * of the 3D scene that was drawn over it, so it uses pixel coverage
     * information (left by RDP in the framebuffer), to identify which
     * pixels to smooth.
     * 
     * Doing AA filter will have a performance impact (compared to pure 
     * bilinear resampling, which is #VI_AA_MODE_RESAMPLE) because of the
     * extra memory bandwidth required.
     * 
     * @note This setting seems to cause image corruption in high-bandwidth
     *       scenarios, like high-resolution framebuffers. Use
     *       #VI_AA_MODE_RESAMPLE_FETCH_ALWAYS in those cases (though it will 
     *       impact general performance more).
     * 
     * @note This setting is not compatible with dedithering
     *       (#vi_set_dedither).
     */
    VI_AA_MODE_RESAMPLE_FETCH_NEEDED = (0b01 << 8),

    /**
     * @brief Enable AA filter (in addition to bilinear resampling).
     * 
     * This is similar to #VI_AA_MODE_RESAMPLE_FETCH_NEEDED. The exact
     * difference is not known at this time: it seems to impact the way
     * internally VI fetches pixels from memory. The AA filter itself is
     * identical, so the picture will look the same on the screen.
     * 
     * The known outcomes of setting this mode are:
     * 
     *  * It seems completely broken in 32-bpp mode. It's not clear if it's
     *    a bandwidth issue or it's just a bug in the hardware.
     *  * It seems to fix screen artifacts that appear with #VI_AA_MODE_RESAMPLE_FETCH_NEEDED
     *    in high-bandwidth scenarios, like high-resolution framebuffers.
     *  * It causes a bit more performance impact.
     */
    VI_AA_MODE_RESAMPLE_FETCH_ALWAYS = (0b00 << 8),
} vi_aa_mode_t;


/** @brief Initialize the VI module */
void vi_init(void);

/** 
 * @name Low-level register access 
 * 
 * These functions allow to read/write VI hardware registers directly, in a safe
 * way. Consider that most timing-related registers can cause a hard-crash of VI
 * if they are changed during the active display area, so it is generally good
 * practice to only change them during vblank. Thus, we provide a way to delay
 * writing registers until next vblank.
 * 
 * @{
 */

/**
 * @brief Read the current state of a register, including pending changes.
 * 
 * This function is similar to directly sampling the hardware register,
 * but also takes into account any pending write that might have issued
 * before but was still not applied. This helps achieving consistency during
 * multi-register writes.
 * 
 * @param reg               Register to read
 * @return uint32_t         Current value of the register (or pending value if any)
 */
inline uint32_t vi_read(volatile uint32_t *reg) {
    return __vi_cfg[reg - VI_REGISTERS];
}

/**
 * @brief Partially write a VI register at next vblank
 * 
 * This is similar to #vi_write, but only write some bits of the register.
 * The specified \p wmask specifies which bits will actually be changed
 * using the \p value, while the other bits will be left untouched.
 *  
 * @param reg               Register to write
 * @param wmask             Mask of bits to write
 * @param value             Value to write
 * 
 * @see #vi_write
 * @see #vi_write_begin
 */
void vi_write_masked(volatile uint32_t *reg, uint32_t wmask, uint32_t value);

/**
 * @brief Write a VI register at next vblank
 * 
 * The write will be pending until the vblank, but #vi_read will immediately
 * start returning the new value.
 * 
 * Notice that if the function is called while the VI is currently in vblank
 * period, the batched changes will be applied immediately.
 * 
 * If you want to atomically change multiple registers, use #vi_write_begin
 * and #vi_write_end to group the changes.
 * 
 * @param reg               Register to write
 * @param value             Value to write
 * 
 * @see #vi_write_begin
 * @see #vi_write_masked
 */
inline void vi_write(volatile uint32_t *reg, uint32_t value)
{
    vi_write_masked(reg, 0xFFFFFFFF, value);
}

/**
 * @brief Begin a batch of register writes
 * 
 * This function starts a batch of register writes, so that they will be
 * applied atomically at the next vblank. This is useful when you want to
 * change multiple registers at once, to avoid display artifacts.
 * 
 * @see #vi_write_end
 */
void vi_write_begin(void);

/**
 * @brief End a batch of register writes
 * 
 * This function ends a batch of register writes started with #vi_write_begin.
 * It doesn't block until vblank, so when the function returns, the registers
 * are still pending to be written (though #vi_read will return the new values).
 * 
 * Notice that if the function is called while the VI is currently in vblank
 * period, the batched changes will be applied immediately.
 * 
 * @see #vi_write_begin
 */
void vi_write_end(void);

/**
 * @brief Wait for the beginning of next vblank period
 * 
 * This function waits for the beginning of the next vblank period, and
 * returns immediately after that.
 * 
 * If the VI is not active, this function will return immediately.
 */
void vi_wait_vblank(void);

/**
 * @brief Dump the current status of all VI registers
 * 
 * @param verbose      Verbosity mode of dump
 *                     0=just hex values, 1=decoded values
 */
void vi_debug_dump(int verbose);

/**
 * @brief Stabilize the value of a VI register by rewriting it at vblank
 * 
 * @note This is an advanced function, which is normally not needed.
 * 
 * This function forces a certain register to be rewritten with its programmed
 * value (last value written via #vi_write functions) at every vblank.
 * 
 * This can be useful in cases where you are playing with a certain register
 * mid-frame, and you want to ensure that the original value is reset at the
 * beginning of each frame.
 * 
 * @param reg           Register to stabilize
 * @param enable        Whether to enable or disable the stabilization
 */
void vi_stabilize(volatile uint32_t *reg, bool enable);

/** @} */

/**
 * @name Functions to query VI state
 * 
 * These functions allow to query the current state of VI, by also providing
 * simple processing of raw register values.
 * 
 * @{
 */

/**
 * @brief Get the current refresh rate of the video output in Hz
 * 
 * The refresh rate is normally 50 for PAL and 60 for NTSC, but this function
 * returns the hardware-accurate number which is close to those but not quite
 * exact. Moreover, this will also account for advanced VI configurations
 * affecting the refresh rate, like PAL60.
 * 
 * @return float        Refresh rate in Hz (frames per second)
 */
float vi_get_refresh_rate(void);

/**
 * @brief Get the current active display output area
 * 
 * This function returns the current VI display output area, which is the
 * area on the output screen within which the framebuffer is displayed.
 * The area is expressed in screen dots.
 * 
 * The theoretical total size of the screen is 773x525 on NTSC and M-PAL,
 * and 794x625 on PAL, but some of that range won't actually be available for
 * display as it would conflict with various sync signals. The maximum allowed
 * output area can be queried at runtime using #vi_get_output_bounds.
 * 
 * These are the default display areas in the various TV standards:
 * 
 *  * NTSC and M-PAL: (108,35) - (748,515)
 *  * PAL: (128,45) - (768,621)
 * 
 * @note The output area has nothing to do with the framebuffer size. It just
 *       describes which part of the (virtual) TV screen is not black.
 *       The framebuffer can have any size, and normally it will resampled to
 *       fit the output area.
 * 
 * @param[out] x0       Horizontal start of the output area
 * @param[out] y0       Vertical start of the output area
 * @param[out] x1       Horizontal end of the output area (exclusive)
 * @param[out] y1       Vertical end of the output area (exclusive)
 * 
 * @see #vi_get_output_bounds
 * @see #vi_set_output
 * @see #vi_move_output
 */
void vi_get_output(int *x0, int *y0, int *x1, int *y1);

/**
 * @brief Get the bounds for the output area
 * 
 * The output area is the area on the screen where the framebuffer is displayed.
 * 
 * The theoretical total size of the screen is 773x525 on NTSC and M-PAL,
 * and 794x625 on PAL, but some of that range won't actually be available for
 * display as it would conflict with various sync signals. If the output area
 * was configure to overlap those signals, the picture would desync.
 * 
 * This function returns the actual bounds which the outut area can be
 * configured. The bounds depend on the TV standard and whether interlacing
 * is on or off. 
 * 
 * @param[out] x0       Minimum X coordinate that can be used for output.
 * @param[out] y0       Minimum Y coordinate that can be used for output.
 * @param[out] x1       Maximum X coordinate that can be used for output (exclusive)
 * @param[out] y1       Maximum Y coordinate that can be used for output (exclusive)
 */
void vi_get_output_bounds(int *x0, int *y0, int *x1, int *y1);

/**
 * @brief Get the output area as a border structure
 * 
 * This function returns the current output area as a border structure. The
 * output area is normally queried using #vi_get_output, but this function
 * returns a different representation of the same area, in terms of size
 * of the "borders".
 * 
 * The borders are expressed in dots, and they represent the offset from the
 * default output area. For instance, on NTSC, the default output area is
 * be (108,35) - (748,515). If you call this function, you will see that
 * borders are 0. This means that the output area is the default one.
 * 
 * If you change the display area to (140,51) - (716,499), the borders will
 * be (left=32, right=32, up=16, down=16), as they represent the offset
 * from the default area. Positive values mean that the output area is
 * smaller than the default one, while negative values mean that the output
 * area is larger.
 * 
 * @return vi_borders_t         Size of the borders in the output area, with
 *                              respect to the default output area.
 */
vi_borders_t vi_get_borders(void);

/**
 * @brief Return the current scanline, and optionally the current field
 * 
 * This function returns the current scanline being displayed on the screen.
 * This counter is sometimes called "half-line", it is a *even* value between
 * 0 and 524 (or 624 on PAL), which can be doubt as two times the actual TV
 * scanline (which is 262 or 312 on PAL).
 * 
 * This scanline number is in the same coordinate system as the one used by
 * other VI registers and functions such as #vi_set_output.
 * 
 * If the \p field parameter is not NULL, it will be filled with the current
 * field being displayed. The field is 0 for the first field, and 1 for the
 * second field, in interlaced mode. In progressive mode, the field is
 * undefined (normally it is 0, but it's not guaranteed).
 * 
 * @param[out] field            Field number (0 or 1) in interlaced mode (undefined in progressive mode)
 * @return int                  Current scanline (always an even number)
 */
inline int vi_get_scanline(int *field) {
    uint32_t v_current = *VI_V_CURRENT;
    if (field) *field = v_current & 1;
    return v_current & ~1;
}

/** @} */

/**
 * @name Functions to configure VI
 * 
 * These functions allow to configure VI in various ways, like setting the
 * framebuffer, the borders, the scaling factors, and more. The functions 
 * provide a "middle-level" interface to VI, which is easier to use than
 * just writing raw registers.
 * 
 * @{
 */

/**
 * @brief Configure VI to display the specified framebuffer
 * 
 * @note You can use #vi_show as a shortcut to do everything required to
 *       display a framebuffer.
 * 
 * This is the most basic function to display a buffer on the screen.
 * It just updates the buffer pointer in RDRAM and its properties
 * (width and bitdepth).
 * 
 * Notice that VI doesn't know the actual width and height of the framebuffer:
 * it will always display contents within the active display area. Instead,
 * you should call #vi_set_xscale and #vi_set_yscale to configure the scaling
 * factors, so that the framebuffer is fits the display area.
 * 
 * @param buffer        Pointer to the framebuffer to display
 * @param pixel_stride  Distance between two consecutive lines in the framebuffer,
 *                      measured in pixels.
 * @param bpp           Bit depth of the framebuffer. Only allowed values are 16 and 32.
 * 
 * @see #vi_show
 * @see #vi_set_xscale
 * @see #vi_set_yscale
 */
void vi_set_origin(void *buffer, int pixel_stride, int bpp);

/**
 * @brief Configure the horizontal scale factor to display the specified framebuffer width
 * 
 * This function calculates and configures the horizontal scale factor
 * needed to display the specified framebuffer width on the screen, so
 * that it fits exactly the active display area.
 * 
 * If OUT_WIDTH is the width of the output area (as returned by #vi_get_output),
 * this function is equivalent to calling `vi_set_xscale_factor(fb_width / OUT_WIDTH)`.
 *  
 * @param fb_width      Width of the framebuffer in pixels
 * 
 * @ßee #vi_set_xscale_factor
 */
void vi_set_xscale(float fb_width);

/**
 * @brief Configure the vertical scale factor to display the specified framebuffer height.
 * 
 * This function calculates and configures the vertical scale factor
 * needed to display the specified framebuffer width on the screen, so
 * that it fits exactly the active display area.
 *  
 * @param fb_height      Height of the framebuffer in pixels
 */
void vi_set_yscale(float fb_height);

/**
 * @brief Set the horizontal scaling factor.
 * 
 * Set the scale factor applied to the framebuffer width to display it on the screen.
 * The \p xfactor term describes how many framebuffer pixels advance for each
 * output dot. For instance, a factor of 0.5 means that each framebuffer pixel
 * will be repeated twice horizontally on the screen, onto two output dots.
 * 
 * You can use #vi_set_xscale to automatically calculate the factor needed
 * to display the framebuffer width on the screen.
 * 
 * @param xfactor        Horizontal scale factor to set
 * 
 * @see #vi_set_xscale
 */
void vi_set_xscale_factor(float xfactor);

/**
 * @brief Set the vertical scaling factor.
 * 
 * Set the scale factor applied to the framebuffer height to display it on the screen.
 * The \p yfactor term describes how many framebuffer pixels advance for each
 * output scanline. For instance, a factor of 0.5 means that each framebuffer pixel
 * will be repeated twice vertically on the screen, onto two scanlines.
 * 
 * @param yfactor        Horizontal scale factor to set
 */
void vi_set_yscale_factor(float yfactor);

/**
 * @brief Enable or disable the interlaced mode
 * 
 * @param interlaced     If true, the VI will display the framebuffer in
 *                       interlaced mode.
 */
void vi_set_interlaced(bool interlaced);

/** 
 * @brief Enable / disable AA mode for the VI
 * 
 * The AA mode specifies a set of filters to apply to the framebuffer
 * during the resampling process. See #vi_aa_mode_t for more information.
 * 
 * @param aa_mode       AA mode to set
 */
void vi_set_aa_mode(vi_aa_mode_t aa_mode);

/**
 * @brief Enable / disable the divot filter
 * 
 * Divot filter is a post-processing filter, on top of anti-alias filters,
 * that is designed to remove a few single-point artifacts that can appear
 * in the framebuffer.
 * 
 * Divot only makes sense if you are using an AA filter, that is, the AA mode
 * is set to either #VI_AA_MODE_RESAMPLE_FETCH_NEEDED or
 * #VI_AA_MODE_RESAMPLE_FETCH_ALWAYS.
 * 
 * @param divot         If true, the divot filter will be enabled
 */
void vi_set_divot(bool divot);

/**
 * @brief Enable / disable dedithering
 * 
 * Dedithering (or "dither filter") is a filter that tries to apply an
 * error correction on top of a dithered 16-bit framebuffer, trying to 
 * restore part of the original 32-bit color information. It works best
 * when the framebuffer was drawn with the RDP "magic square" dithering
 * pattern, as it's designed to reverse exactly that.
 * 
 * Dedithering only makes sense for 16-bit, dithered framebuffers.
 * 
 * @note Dedithering is not compatible with all AA filters. In particular, it
 *       only works with #VI_AA_MODE_NONE and #VI_AA_MODE_RESAMPLE_FETCH_ALWAYS.
 */
void vi_set_dedither(bool dedither);

/**
 * @brief Enable / disable gamma correction
 * 
 * VI is able to apply a gamma correction filter to the framebuffer. 
 * 
 * Gamma correction is meant to convert a framebuffer with pixels in linear
 * RGB space, into the non-linear sRGB space expected by most displays.
 * Normally, this is not required because all assets are already provided in
 * sRGB color space, which is how most authoring tools save PNGs nowadays. 
 * 
 * Drawing in linear space would in theory be useful to produce more accurate
 * lighting and blending effects, though testing showed that this is really
 * only visible with 32 bpp framebuffers, which are normally not used. If you
 * want to experiment with working in linear space, make sure to provide
 * assets in that format (which normally means using the --gamma option to
 * mksprite to convert sRGB PNG pixels into linear space).
 */
void vi_set_gamma(bool gamma);

/**
 * @brief Set the active display output area
 * 
 * This function sets the active display output area to the specified
 * coordinates. The area is expressed in screen dots.
 * 
 * Notice that not all positions are valid for the output, so the
 * function will clamp it to make sure to keep the display stable (and
 * avoid crashing the VI, which is not very tolerant to invalid values).
 * The bounds for the output area can be queried using #vi_get_output_bounds.
 * 
 * @param x0        Horizontal start of the output area 
 * @param y0        Vertical start of the output area
 * @param x1        Horizontal end of the output area (exclusive)
 * @param y1        Vertical end of the output area (exclusive)
 */
void vi_set_output(int x0, int y0, int x1, int y1);

/**
 * @brief Set the specified start position for the active display output area
 * 
 * This function sets the start position for the active display area (without 
 * changing the size). The position is expressed in VI dots.
 * 
 * Notice that not all positions are valid for the output, so the
 * function will clamp it to make sure to keep the display stable (and
 * avoid crashing the VI, which is not very tolerant to invalid values).
 * 
 * To query the bounds for the output area, use #vi_get_output_bounds. Notice
 * that you must ensure that the *full* output area fits the bounds; for instance
 * if the horizontal bounds are 100-800 and output width is 500, the start
 * position must be within 100 and 300. If you set it to 400, for instance,
 * it will be automatically clamped to 300.
 * 
 * @param x             Horizontal scroll offset
 * @param y             Vertical scroll offset
 * 
 * @see #vi_get_output_bounds
 * @see #vi_get_output
 * @see #vi_scroll_output
 */
void vi_move_output(int x, int y);

/**
 * @brief Scroll the active display area by the specified amount
 * 
 * This function is similar to #vi_move_output, but scrolls the display area
 * by the specified relative amount, instead of setting the absolute position.
 * 
 * Like #vi_move_output, the function will clamp the position to make
 * sure it is within the allowed bounds.
 * 
 * @param deltax        Horizontal scroll amount
 * @param deltay        Vertical scroll amount
 * 
 * @see #vi_move_output
 * @see #vi_get_output_bounds
 */
void vi_scroll_output(int deltax, int deltay);

/**
 * @brief Calculate correct VI borders for a target aspect ratio.
 * 
 * This function calculates the appropriate VI borders to obtain the specified
 * aspect ratio, and optionally adding a margin to make the picture CRT-safe.
 * 
 * The margin is expressed as a percentage relative to the virtual VI display
 * output (640x480). A good default for this margin for most CRTs is
 * #VI_CRT_MARGIN (5%).
 * 
 * For instance, to create a 16:9 resolution, you can do:
 * 
 * \code{.c}
 *      vi_borders_t borders = vi_calc_borders(16./9, false, 0, 0);
 * \endcode
 * 
 * @param aspect_ratio      Target aspect ratio
 * @param overscan_margin   Margin to add to compensate for TV overscan. Use 0
 *                          to use full picture (eg: for emulators), and something
 *                          like #VI_CRT_MARGIN to get a good CRT default.
 * @return vi_borders_t The requested border settings
 */
vi_borders_t vi_calc_borders(float aspect_ratio, float overscan_margin);

/**
 * @brief Configure the output area via the specified borders
 * 
 * Configures the VI to the specified border size. This function is an
 * alterantive to #vi_set_output: instead of specifying the output area
 * directly, you can specify it in terms of borders with respect to the
 * default output area. See #vi_get_borders for more information.
 * 
 * @param b                 Size of the borders to apply
 */
void vi_set_borders(vi_borders_t b);

/**
 * @brief Show the specified surface (at next vblank)
 * 
 * This function is a shortcut to do the following:
 * 
 *  * Call #vi_set_origin to specify the new buffer, stride and bpp
 *  * Call #vi_set_xscale to set the horizontal scale factor
 *  * Call #vi_set_yscale to set the vertical scale factor
 * 
 * If the specified surface is NULL, the VI will be turned off altogether,
 * and will stop issuing any video signal.
 * 
 * @param fb      Surface to show
 */
void vi_show(surface_t *fb);

/** @} */

#ifdef __cplusplus
}
#endif

#endif