/**
 * @file yuv.h
 * @brief Hardware accelerated YUV conversion
 * @ingroup video
 */

/**
 * @defgroup video Video Subsystem
 * @ingroup libdragon
 * @brief Compressed video playback libraries (for FMVs, etc.)
 */

#ifndef __LIBDRAGON_YUV_H
#define __LIBDRAGON_YUV_H

/**
 * @brief Convert YUV frames to RGB.
 * 
 * This library contains hardware-accelerated function to convert a YUV image
 * into a RGB image. The task is mainly performed using RDP, but the RSP can
 * also be used to handle parts of it.
 * 
 * It is possible to specify the exact colorspace to use for the conversions.
 * Colorspaces are represented using the #yuv_colorspace_t. A few standard
 * colorspaces are pre-defined as constants and can be used as-is:
 * 
 * - #YUV_BT601_TV: BT.601 colorspace, limited range (16-235) for CRT TVs.
 * - #YUV_BT601_FULL: BT.601 colorspace, full range (0-255)
 * - #YUV_BT709_TV: BT.709 colorspace, limited range (16-235) for CRT TVs.
 * - #YUV_BT709_FULL: BT.709 colorspace, full range (0-255)
 * 
 * Normally, most encoders default to #YUV_BT601_TV for videos at Nintendo 64
 * resolutions, while #YUV_BT709_FULL is typically the defaults for modern
 * HD or 4K videos.
 * 
 * If you have some very specific use case, you can define your own colorspace
 * using #yuv_new_colorspace. For testing purposes, #yuv_to_rgb can be used
 * to convert a single YUV pixel to RGB using a specified colorspace.
 * 
 * To blit a full frame, you can use #yuv_tex_blit, which is similar to 
 * #rdpq_tex_blit as it allows to copy an arbitrary sized frame and apply
 * transformations to it (typically, scaling or flipping).
 * 
 * To playback a video at maximum performance, it is recommended to use
 * #yuv_blitter_t instead. A blitter is an object that can be used to 
 * perform multiple frame conversions with the same parameters (same input
 * size, same output size, same scaling and alignment). It is similar to
 * #rdpq_tex_blit in concept, but it precalculates most of the computations
 * using a rspq block (see #rspq_block_t for more information), so that
 * any time a conversion is needed, it is completely offloaded to the RSP+RDP
 * with almost zero CPU overhead.
 * 
 * You can create a #yuv_blitter_t using #yuv_blitter_new (which accepts
 * parameters identical to #yuv_tex_blit), or the most handy #yuv_blitter_new_fmv
 * which accepts more high-level parameters more optimized for the use case
 * of a full-screen full motion video player.
 * 
 */

#include <stdint.h>
#include "graphics.h"
#include "rdpq_tex.h"

#ifdef __cplusplus
extern "C" {
#endif

///@cond
struct rspq_block_s;
typedef struct rspq_block_s rspq_block_t;
///@endcond

/**
 * @brief Initialize the YUV conversion library.
 */
void yuv_init(void);

/**
 * @brief Shutdown the YUV conversion library
 */
void yuv_close(void);

/**
 * @brief A YUV colorspace
 * 
 * This structure contains the parameters that define a YUV colorspace
 * for conversion to and from the RGB space. The "c" parameters are
 * used when doing a CPU-based conversion (using #yuv_to_rgb), while
 * the "k" parameters are used when doing a RDP-based conversion
 * (see all the yuv_draw_frame_* functions).
 * 
 * Most users can simply use one of the predefined colorspaces: #YUV_BT601_TV,
 * #YUV_BT601_FULL, #YUV_BT709_TV, #YUV_BT709_FULL. To simplify creating a custom
 * colorspace, #yuv_new_colorspace can be used.
 * 
 * When playing back a video, you should specify the colorspace that was used
 * to encode the video. Normally, this is available in the video header or 
 * stream as metadata information. Notice that most video encoders such as
 * ffmpeg default to ITU-R BT601 TV Range when encoding low resolution movies
 * (non-HD resolutions), so using #YUV_BT601_TV is a good default if the exact
 * colorspace is not known.
 * 
 * When encoding a video, it is suggested to configure the encoder to use
 * ITU-R BT601 TV Range. In general, colorspaces created in the "TV Range"
 * have more color fidelity when displayed on a CRT TV. For PC-only playback
 * through emulators, "Full Range" colorspaces offer more color precision.
 */
typedef struct {
    float c0, c1, c2, c3, c4; int y0;
    int k0, k1, k2, k3, k4, k5;
} yuv_colorspace_t;

/** @brief ITU-R BT.601 TV Range colorspace (DEFAULT). 
 * 
 * This is the standard colorspace used by low resolution videos such as those
 * normally encoded for Nintendo 64. If in doubt, try first always with this
 * colorspace.
 * 
 * Created via #yuv_new_colorspace using the following parameters:
 *    
 *  * Kr=0.299, Kb=0.114
 *  * Y0=16, yrange=219, crange=224
 */ 
extern const yuv_colorspace_t YUV_BT601_TV;

/** @brief ITU-R BT.601 Full Range colorspace. 
 *
 * Created via #yuv_new_colorspace using the following parameters:
 *    
 *  * Kr=0.299, Kb=0.114
 *  * Y0=0, yrange=256, crange=256
 */
extern const yuv_colorspace_t YUV_BT601_FULL;

/** @brief ITU-R BT.709 TV Range colorspace. 
 * 
 * Created via #yuv_new_colorspace using the following parameters:
 *    
 *  * Kr=0.0722, Kb=0.2126
 *  * Y0=16, yrange=219, crange=224
 */
extern const yuv_colorspace_t YUV_BT709_TV;

/** 
 * @brief ITU-R BT.709 Full Range colorspace.
 * 
 * Created via #yuv_new_colorspace using the following parameters:
 *    
 *  * Kr=0.0722, Kb=0.2126
 *  * Y0=0, yrange=256, crange=256
 */
extern const yuv_colorspace_t YUV_BT709_FULL;


/**
 * @brief Calculate coefficients for a new YUV colorspace.
 * 
 * This function is mostly for documentation purposes. It can be used to create
 * a new colorspace, by calculating its coefficients (stored into #yuv_colorspace_t)
 * from the mathematical definition of the colorspace. Most users will not need
 * to call this function and just use one of the predefined colorspaces such as
 * #YUV_BT601_TV.
 * 
 * A YUV colorspace is defined by three constants normally called Kr, Kg, Kb.
 * Since the sum of these three constants must be 1.0, normally only two are
 * provided (by convention, Kr and Kb).
 * 
 * Because of technical issues with old analog TVs, it was not possible to 
 * display the full 8-bit range of the Y,U,V components, so during the conversion
 * the range was often restricted a bit. The range for the Y component (as an
 * 8-bit unsigned integer) is defined via the minimum allowed value @p y0 and the
 * number of possible values @p yrange -- so the last allowed value is y0+yrange-1.
 * The range for the U,V components (as 8-bit signed integers) is specified in
 * @p crange and is always assumed to be centered around 0 (so the allowed values
 * are -crange/2 to crange/2-1).
 * 
 * For old TVs, colorspaces should use the "TV Range" which is defined as
 * y0=19, yrange=219, crange=224.
 * 
 * Videos meant to be played back on PC are probably coded using the "Full Range",
 * which is y0=0, yrange=256, crange=256.
 *
 * @param[in]  Kr      The colorspace coefficient Kr
 * @param[in]  Kb      The colorspace coefficient Kb
 * @param[in]  y0      The minimum allowed value for the Y component
 * @param[in]  yrange  The number of allowed values for the Y component
 * @param[in]  crange  The number of allowed values for the U,V component
 * @return      	   The new colorspace structure
 * 
 * @see #YUV_BT601_TV
 * @see #YUV_BT601_FULL
 * @see #YUV_BT709_TV
 * @see #YUV_BT709_FULL
 * 
 */
yuv_colorspace_t yuv_new_colorspace(float Kr, float Kb, int y0, int yrange, int crange);


/**
 * @brief Convert a single YUV pixel into RGB.
 * 
 * Convert a single YUV pixel to RGB, using the CPU. This function
 * should be used only for non-performance critical tasks. For high-performance
 * conversions, see the yuv_draw_frame_* functions that are hardware
 * accelerated via the RDP.
 * 
 * Notice that this function is not meant to be bit-exact with the RDP hardware
 * accelerated version, but it will return values which are very close.
 *
 * @param[in]  y     Y component
 * @param[in]  u     U component
 * @param[in]  v     V component
 * @param      cs    The colorspace to use for the conversion
 *
 * @return     The converted pixel in RGBA format (A is forced to 255).
 */
color_t yuv_to_rgb(uint8_t y, uint8_t u, uint8_t v, const yuv_colorspace_t *cs);

/** @brief A YUV frame, made of three distinct planes */
typedef struct {
	surface_t y;           ///< Luminance plane (Y)
    surface_t u;           ///< Chrominance plane (U)
    surface_t v;           ///< Chrominance plane (V)
} yuv_frame_t;

/** @brief YUV blitter zoom configuration */
typedef enum {
    YUV_ZOOM_KEEP_ASPECT,  ///< Zoom the frame, keeping frame aspect ratio
    YUV_ZOOM_FULL,         ///< Zoom the frame, irrespective of aspect ratio
    YUV_ZOOM_NONE,         ///< Do not zoom the frame to fit the output buffer
} yuv_zoom_t;

/** @brief YUV blitter output buffer alignment */
typedef enum {
    YUV_ALIGN_CENTER,   ///< Align to center of the output buffer
    YUV_ALIGN_MIN,      ///< Align to left/top of the output buffer
    YUV_ALIGN_MAX,      ///< Align to right/bottom of the output buffer
} yuv_align_t;


/** 
 * @brief YUV full motion video blitter configuration.
 * 
 * These are the parameters that can be used to configure a YUV blitter via
 * #yuv_blitter_new_fmv. They are designed for the use case of a full-screen
 * full motion video player, where the video is optionally scaled to fit the screen.
 */
typedef struct yuv_fmv_parms_s {
    const yuv_colorspace_t *cs;		///< Color space to use during conversion (default: #YUV_BT601_TV)
    yuv_align_t halign;			    ///< Frame horizontal alignment to the output buffer (default: centered)
    yuv_align_t valign;				///< Frame vertical alignment to the output buffer (default: centered)
    yuv_zoom_t zoom;				///< Frame zooming algorithm to use (default: keep aspect ratio)
    color_t bkg_color;              ///< Color to use to clear the reset of the output buffer
} yuv_fmv_parms_t;

/** 
 * @brief An optimized YUV blitter, meant for drawing multiple frames.
 * 
 * This structure represents a YUV blitter, which is an engine capable of
 * drawing multiple YUV frames onto a RGB target surface.
 * 
 * The blitter is created by #yuv_blitter_new or #yuv_blitter_new_fmv,
 * providing all parameters that describe how to perform the blitting. At
 * creation time, the blitting operation is recorded into a rspq block, so
 * that the blitting itself (performed by #yuv_blitter_run) uses almost zero
 * CPU time.
 * 
 * Once a blitter is not used anymore, remember to call #yuv_blitter_free to
 * release the memory.
 */
typedef struct yuv_blitter_s {
    rspq_block_t *block;            ///< RSPQ block containing the blitting operation
} yuv_blitter_t;


/**
 * @brief Create a YUV blitter optimized for rendering multiple frames with 
 *        some possible transformation.
 * 
 * This function is similar to #yuv_blitter_new_fmv but initializes the
 * blitter using the same interface of #yuv_tex_blit or #rdpq_tex_blit. The
 * interface allows to handle on-the-fly arbitrary transformations of the
 * blitter (including scaling and rotations) and also cropping. It is indeed
 * a superset of what is possible through #yuv_blitter_new_fmv, but its API
 * might be a bit harder to use for people that just want to do a full-motion
 * video player.
 * 
 * In general, refer to #rdpq_tex_blit for more in-depth documentation
 * related to @p x0 , @p y0 , and @p parms .
 * 
 * The blitter initialized by this function must be freed with #yuv_blitter_free
 * to release all allocated memory.
 * 
 * @param video_width           Width of the video in pixels
 * @param video_height          Height of the video in pixels
 * @param x0                    X coordinate on the framebuffer where to draw the surface
 * @param y0                    Y coordinate on the framebuffer where to draw the surface
 * @param parms                 Parameters for the blit operation (or NULL for default)
 * @param cs                    Colorspace to use for the conversion (or NULL for #YUV_BT601_TV)
 * @return An initialized blitter instance.
 *
 * @see #yuv_blitter_new_fmv
 * @see #yuv_blitter_run
 */
yuv_blitter_t yuv_blitter_new(int video_width, int video_height,
    float x0, float y0, const rdpq_blitparms_t *parms, const yuv_colorspace_t *cs);

/**
 * @brief Create a YUV blitter optimized for FMV drawing (full screen movie player)
 * 
 * This function creates a YUV blitter, using a configuration that is suited
 * for full motion videos. By default (passing NULL as @p parms ), the blitter
 * will draw each frame centered on the screen, and zooming it while maintaining
 * its aspect ratio. Moreover, areas outside of the video will be filled with
 * the black color. This is a good default for a full screen video player.
 * 
 * By configuring @p parms , it is possible to tune the behavior of the player
 * in several details: color space, alignment of the frame, type of zoom,
 * and fill color.
 * 
 * The blitter initialized by this function must be freed with #yuv_blitter_free
 * to release all allocated memory.
 * 
 * @param video_width           Width of the video in pixels
 * @param video_height          Height of the video in pixels
 * @param screen_width          Width of the screen in pixels
 * @param screen_height         Height of the screen in pixels
 * @param parms                 Optional parameters (can be NULL)
 * @return                      An initialized blitter instance.
 * 
 * @see #yuv_blitter_new
 * @see #yuv_blitter_run
 */
yuv_blitter_t yuv_blitter_new_fmv(int video_width, int video_height,
    int screen_width, int screen_height, const yuv_fmv_parms_t *parms);


/**
 * @brief Perform a YUV blit using a blitter, with the specified surfaces
 * 
 * This function performs blitting of a YUV frame (converting it into RGB).
 * The source frame is expected to be split into 3 planes. The conversion
 * will be performed by a mix of RSP and RDP, and will be drawn to the currently
 * attached surface (see #rdpq_attach).
 * 
 * The blitter is configured at creation time with parameters that describe
 * where to draw into the buffer, whether to perform a zoom, etc.
 * 
 * @param blitter 		Blitter created by #yuv_blitter_new_fmv or #yuv_blitter_new
 * @param frame         YUV frame to blit
 */
void yuv_blitter_run(yuv_blitter_t *blitter, yuv_frame_t *frame);

/**
 * @brief Free the memory allocated by a blitter
 * 
 * This function release the memory allocated on a #yuv_blitter_t instance.
 * After calling this function, the blitter instance cannot be used anymore
 * and must be initialized again.
 * 
 * @param blitter 		Blitter to free
 */
void yuv_blitter_free(yuv_blitter_t *blitter);

/**
 * @brief Blit a 3-planes YUV frame into the current RDP framebuffer.
 * 
 * This function is similar to #rdpq_tex_blit, but it allows to blit
 * a YUV frame split into 3 planes. This is faster than first merging the
 * 3 planes into a single buffer (as required by #FMT_YUV16) and then blit it.
 * 
 * This is an all-in-one function that avoids creating a #yuv_blitter_t instance,
 * using it and then freeing it. On the other hand, it performs a lot of work
 * on the CPU which the blitter does only one time (at creation time). Unless you
 * only need to convert one frame, you should consider using the blitter
 * for improved speed.
 * 
 * For more information on how to use this function, see #rdpq_tex_blit.
 * 
 * @param frame         YUV frame to blit
 * @param x0 		    X coordinate where to blit the frame
 * @param y0            Y coordinate where to blit the frame
 * @param parms         Optional blitting parameters (see #rdpq_blitparms_t)
 * @param cs            Optional colorspace to use for the conversion. If NULL,
 * 						the default is #YUV_BT601_TV.
 * 
 * @see #rdpq_tex_blit
 * @see #yuv_blitter_t
 * @see #yuv_blitter_new
 * @see #yuv_blitter_new_fmv
 */
void yuv_tex_blit(yuv_frame_t *frame, float x0, float y0,
    const rdpq_blitparms_t *parms, const yuv_colorspace_t *cs);


#ifdef __cplusplus
}
#endif

#endif
