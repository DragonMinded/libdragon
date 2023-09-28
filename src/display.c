/**
 * @file display.c
 * @brief Display Subsystem
 * @ingroup display
 */
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>
#include <string.h>
#include "regsinternal.h"
#include "n64sys.h"
#include "vi.h"
#include "display.h"
#include "interrupt.h"
#include "utils.h"
#include "debug.h"
#include "surface.h"
#include "rsp.h"

/** @brief Maximum number of video backbuffers */
#define NUM_BUFFERS         32
/** @brief Number of past frames used to evaluate FPS */
#define FPS_WINDOW          32

static surface_t *surfaces;
/** @brief Currently active bit depth */
static uint32_t __bitdepth;
/** @brief Currently active video width (calculated) */
static uint32_t __width;
/** @brief Currently active video height (calculated) */
static uint32_t __height;
/** @brief Currently active video interlace mode */
static interlace_mode_t __interlace_mode = RES_INTERLACE_OFF;
/** @brief Current VI display borders of the output */
static vi_borders_t __borders;
/** @brief Number of active buffers */
static uint32_t __buffers = NUM_BUFFERS;
/** @brief Pointer to uncached 16-bit aligned version of buffers */
static void *__safe_buffer[NUM_BUFFERS];
/** @brief Currently displayed buffer */
static int now_showing = -1;
/** @brief Bitmask of surfaces that are currently being drawn */
static uint32_t drawing_mask = 0;
/** @brief Bitmask of surfaces that are ready to be shown */
static volatile uint32_t ready_mask = 0;
/** @brief Window of absolute times at which previous frames were shown */
static uint32_t frame_times[FPS_WINDOW];
/** @brief Current index into the frame times window */
static int frame_times_index = 0;
/** @brief Current duration of the frame window (time elapsed for FPS_WINDOW frames) */
static uint32_t frame_times_duration;

/** @brief Get the next buffer index (with wraparound) */
static inline int buffer_next(int idx) {
    idx += 1;
    if (idx == __buffers)
        idx = 0;
    return idx;
}

/**
 * @brief Interrupt handler for vertical blank
 *
 * If there is another frame to display, display the frame
 */
static void __display_callback()
{
    /* Least significant bit of the current line register indicates
       if the currently displayed field is odd or even. */
    bool field = (*VI_V_CURRENT) & 1;
    bool interlaced = (*VI_CTRL) & (VI_CTRL_SERRATE);

    /* Check if the next buffer is ready to be displayed, otherwise just
       leave up the current frame. If full interlace mode is selected
       then don't update the buffer until two fields were displayed. */
    if (!(__interlace_mode == RES_INTERLACE_FULL && field)) {
        int next = buffer_next(now_showing);
        if (ready_mask & (1 << next)) {
            now_showing = next;
            ready_mask &= ~(1 << next);
        }
    }

    uint32_t vi_h_video = *VI_H_VIDEO;
    int h_end = vi_h_video & 0xFFFF;
    int h_start = vi_h_video >> 16;
    vi_resparms_t resparms = vi_calc_resparms(__width, h_start, h_end);
    vi_write_dram_register(__safe_buffer[now_showing] + (interlaced && !field ? __width * __bitdepth : 0) - resparms.vi_origin_offset * __bitdepth);
}

void display_init( resolution_t res, bitdepth_t bit, uint32_t num_buffers, gamma_t gamma, filter_options_t filters )
{
    uint32_t tv_type = get_tv_type();
    uint32_t control = !sys_bbplayer()? VI_PIXEL_ADVANCE_DEFAULT : VI_PIXEL_ADVANCE_BBPLAYER;

    /* Can't have the video interrupt happening here */
    disable_interrupts();

    /* Minimum is two buffers. */
    __buffers = MAX(1, MIN(NUM_BUFFERS, num_buffers));

    bool serrate = res.interlaced != RES_INTERLACE_OFF;
    /* Serrate on to stop vertical jitter */
    if(serrate) control |= VI_CTRL_SERRATE;

    /* Copy over extra initializations */
    vi_write_config(&vi_config_presets[serrate][tv_type]);

    /* Figure out control register based on input given */
    switch( bit )
    {
        case DEPTH_16_BPP:
            control |= VI_CTRL_TYPE_16_BPP;
            break;
        case DEPTH_32_BPP:
            control |= VI_CTRL_TYPE_32_BPP;
            break;
    }

    switch( gamma )
    {
        case GAMMA_NONE:
            /* Nothing to set here */
            break;
        case GAMMA_CORRECT:
            control |= VI_GAMMA_ENABLE;
            break;
        case GAMMA_CORRECT_DITHER:
            control |= VI_GAMMA_ENABLE | VI_GAMMA_DITHER_ENABLE;
            break;  
    }

    switch( filters )
    {
        /* Libdragon uses preconfigured modes for enabling certain 
        combinations of VI filters due to a large number of wrong/invalid configurations 
        with very strict conditions, and to simplify the options for the user.
        Like for example antialiasing requiring resampling; dedithering not working with 
        resampling, unless always fetching; always enabling divot filter under AA etc.
        The cases below provide all possible configurations that are deemed useful. */

        case FILTERS_DISABLED:
            /* Set AA off flag */
            control |= VI_AA_MODE_NONE;

            break;
        case FILTERS_RESAMPLE:
            /* Set AA on resample */
            control |= VI_AA_MODE_RESAMPLE;

            /* Dither filter should not be enabled with this AA mode
               as it will cause ugly vertical streaks */
            break;
        case FILTERS_DEDITHER:
            /* Set AA off flag and dedither on 
            (only on 16bpp mode, act as FILTERS_DISABLED on 32bpp) */
            if ( bit == DEPTH_16_BPP ) {
                control |= VI_AA_MODE_NONE | VI_DEDITHER_FILTER_ENABLE;
            }
            else control |= VI_AA_MODE_NONE;

            break;
        case FILTERS_RESAMPLE_ANTIALIAS:
            /* Set AA on resample and fetch as well as divot on */
            control |= VI_AA_MODE_RESAMPLE_FETCH_NEEDED | VI_DIVOT_ENABLE;

            break;
        case FILTERS_RESAMPLE_ANTIALIAS_DEDITHER:
            /* Set AA on resample always and fetch as well as dedither on 
            (only on 16bpp mode, act as FILTERS_RESAMPLE_ANTIALIAS on 32bpp) */

            /* Enable dither filter in 16bpp mode to give gradients
               a slightly smoother look */
            if ( bit == DEPTH_16_BPP ) 
                 control |= VI_AA_MODE_RESAMPLE_FETCH_ALWAYS | VI_DEDITHER_FILTER_ENABLE | VI_DIVOT_ENABLE; 
            else control |= VI_AA_MODE_RESAMPLE_FETCH_NEEDED | VI_DIVOT_ENABLE;
            break;
    }

    /* Set the control register in our template */
    vi_write(VI_CTRL, control);

    /* Calculate width and scale registers */
    assertf(res.width > 0, "nonpositive width");
    assertf(res.height > 0, "nonpositive height");
    assertf(res.width <= 800, "invalid width");
    assertf(res.height <= 720, "heights > 720 are buggy on hardware");
    if( bit == DEPTH_16_BPP )
    {
        assertf(res.width % 4 == 0, "width must be divisible by 4 for 16-bit depth");
    }
    else if ( bit == DEPTH_32_BPP )
    {
        assertf(res.width % 2 == 0, "width must be divisible by 2 for 32-bit depth");
    }

    vi_borders_t         borders = VI_BORDERS_NONE;
    if(res.crt_borders)  borders = VI_BORDERS_CRT;

    // Fix this specific case so that the most common resolution of 320x240 16bpp wouldn't crash without a resampling filter
    // Users can obtain a sharper image by disabling that filter
    //if(bit == DEPTH_16_BPP && res.width == 320 && (FILTERS_DISABLED || FILTERS_DEDITHER) && !res.crt_borders)
    //    borders.right++;

    __borders = borders;

    vi_write(VI_WIDTH, res.width);
    vi_write_display(res.width, res.height, serrate, res.aspect_ratio, borders);

    /* Disabling resampling (AA_MODE = 0x3) on 16bpp hits a hardware bug on NTSC 
       consoles when the X_SCALE is 0x200 or lower (see issue #66).
       It would work on PAL consoles, but we think users are better
       served by prohibiting it altogether.

       For people that absolutely need this on PAL consoles, it can
       be enabled with *(volatile uint32_t*)0xA4400000 |= 0x300 just
       after the display_init call. */
    if ( bit == DEPTH_16_BPP ){
        if(filters == FILTERS_DISABLED)
            assertf((*VI_X_SCALE & 0xFFF) > 0x200,
            "FILTERS_DISABLED is not supported by the hardware for widths <= 320 without borders.\n"
            "Please use FILTERS_RESAMPLE instead.");
        if(filters == FILTERS_DEDITHER)
            assertf((*VI_X_SCALE & 0xFFF) > 0x200,
            "FILTERS_DEDITHER is not supported by the hardware for widths <= 320 without borders.\n"
            "Please use FILTERS_RESAMPLE instead.");
    }

    /* Set up the display */
    __width = res.width;
    __height = res.height;
    __bitdepth = ( bit == DEPTH_16_BPP ) ? 2 : 4;
    __interlace_mode = res.interlaced;

    surfaces = malloc(sizeof(surface_t) * __buffers);

    /* Initialize buffers and set parameters */
    for( int i = 0; i < __buffers; i++ )
    {
        /* Set parameters necessary for drawing */
        /* Grab a location to render to */
        tex_format_t format = bit == DEPTH_16_BPP ? FMT_RGBA16 : FMT_RGBA32;
        surfaces[i] = surface_alloc(format, __width, __height + 2); // Also add some redundancy for the VI output, so that it doesn't display garbage at the end
        surfaces[i].height = __height; // For the frontend, use correct resolution values;
        __safe_buffer[i] = surfaces[i].buffer;
        assert(__safe_buffer[i] != NULL);

        /* Baseline is blank */
        memset( __safe_buffer[i], 0, __width * (__height+2) * __bitdepth );
    }

    /* Set the first buffer as the displaying buffer */
    now_showing = 0;
    drawing_mask = 0;
    ready_mask = 0;

    /* Show our screen normally. If display is already active, do that during vblank
       to avoid confusing the VI chip with in-frame modifications. */
    if ( vi_is_active() ) { vi_wait_for_vblank(); }

    vi_write(VI_ORIGIN, PhysicalAddr(__safe_buffer[0]));

    enable_interrupts();

    /* Set which line to call back on in order to flip screens */
    register_VI_handler( __display_callback );
    set_VI_interrupt( 1, VI_V_CURRENT_VBLANK );
}

void display_close()
{
    /* Can't have the video interrupt happening here */
    disable_interrupts();

    set_VI_interrupt( 0, 0 );
    unregister_VI_handler( __display_callback );

    now_showing = -1;
    drawing_mask = 0;
    ready_mask = 0;

    __width = 0;
    __height = 0;

    /* If display is active, wait for vblank before touching the registers */
    if( vi_is_active() ) { vi_wait_for_vblank(); }

    vi_write_image_is_blank();
    vi_write_dram_register( 0 );

    if( surfaces )
    {
        for( int i = 0; i < __buffers; i++ )
        {
            /* Free framebuffer memory */
            surface_free(&surfaces[i]);
            __safe_buffer[i] = NULL;
        }
        free(surfaces);
        surfaces = NULL;
    }

    enable_interrupts();
}

surface_t* display_try_get(void)
{
    surface_t* retval = NULL;
    int next;

    /* Can't have the video interrupt happening here */
    disable_interrupts();

    /* Calculate index of next display context to draw on. We need
       to find the first buffer which is not being drawn upon nor
       being ready to be displayed.

       Notice that the loop is always executed once, so it also works
       in the case of a single display buffer, though it at least
       wait for that buffer to be shown. */
    next = buffer_next(now_showing);
    do {
        if (((drawing_mask | ready_mask) & (1 << next)) == 0)  {
            retval = &surfaces[next];
            drawing_mask |= 1 << next;
            break;
        }
        next = buffer_next(next);
    } while (next != now_showing);

    enable_interrupts();

    /* Possibility of returning nothing, or a valid display context */
    return retval;
}

surface_t* display_get(void)
{
    // Wait until a buffer is available. We use a RSP_WAIT_LOOP as
    // it is common for display to become ready again after RSP+RDP
    // have finished processing the previous frame's commands.
    surface_t* disp;
    RSP_WAIT_LOOP(200) {
         if ((disp = display_try_get())) {
             break;
         }
    }
    return disp;
}

void display_show( surface_t* surf )
{
    /* They tried drawing on a bad context */
    if( surf == NULL ) { return; }

    /* Can't have the video interrupt screwing this up */
    disable_interrupts();

    /* Correct to ensure we are handling the right screen */
    int i = surf - surfaces;

    assertf(i >= 0 && i < __buffers, "Display context is not valid!");

    /* Check we have not unlocked this display already and is pending drawn. */
    assertf(!(ready_mask & (1 << i)), "display_show called again on the same display %d (mask: %lx)", i, ready_mask);

    /* This should match, or something went awry */
    assertf(drawing_mask & (1 << i), "display_show called on non-locked display %d (mask: %lx)", i, drawing_mask);

    drawing_mask &= ~(1 << i);
    ready_mask |= 1 << i;

    /* Record the time at which this frame was (asked to be) shown */
    uint32_t old_ticks = frame_times[frame_times_index];
    uint32_t now = get_ticks();
    if (old_ticks)
        frame_times_duration = TICKS_DISTANCE(old_ticks, now);
    frame_times[frame_times_index] = now;
    frame_times_index++;
    if (frame_times_index == FPS_WINDOW)
        frame_times_index = 0;

    enable_interrupts();
}

/**
 * @brief Force-display a previously locked buffer
 *
 * Display a valid display context to the screen right away, without waiting
 * for vblank interrupt. This function works also with interrupts disabled.
 *
 * NOTE: this is currently not part of the public API as we use it only
 * internally.
 *
 * @param[in] disp
 *            A display context retrieved using #display_get
 */
void display_show_force( display_context_t disp )
{
    /* Can't have the video interrupt screwing this up */
    disable_interrupts();
    display_show(disp);
    __display_callback();
    enable_interrupts();
}

uint32_t display_get_width(void)
{
    return __width;
}

uint32_t display_get_height(void)
{
    return __height;
}

uint32_t display_get_bitdepth(void)
{
    return __bitdepth;
}

uint32_t display_get_num_buffers(void)
{
    return __buffers;
}

float display_get_fps(void)
{
    if (!frame_times_duration) return 0;
    return (float)(FPS_WINDOW * TICKS_PER_SECOND) / frame_times_duration;
}

extern inline vi_resparms_t vi_calc_resparms(int fb_width, int h_start, int h_end);