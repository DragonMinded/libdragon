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
#include "display.h"
#include "interrupt.h"
#include "utils.h"
#include "debug.h"
#include "surface.h"
#include "vi.h"

/** @brief Maximum number of video backbuffers */
#define NUM_BUFFERS         32


static surface_t *surfaces;
/** @brief Currently active bit depth */
static uint32_t __bitdepth;
/** @brief Currently active video width (calculated) */
static uint32_t __width;
/** @brief Currently active video height (calculated) */
static uint32_t __height;
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
    bool field = (*vi_register(VI_V_CURRENT)) & 1;
    bool interlaced = (*vi_register(VI_CTRL)) & (VI_CTRL_SERRATE);

    /* Check if the next buffer is ready to be displayed, otherwise just
       leave up the current frame */
    int next = buffer_next(now_showing);
    if (ready_mask & (1 << next)) {
        now_showing = next;
        ready_mask &= ~(1 << next);
    }

    vi_write_dram_register(__safe_buffer[now_showing] + (interlaced && !field ? __width * __bitdepth : 0));
}

void display_init( resolution_t res, bitdepth_t bit, uint32_t num_buffers, gamma_t gamma, filter_options_t filters )
{
    vi_config_t config;
    uint32_t tv_type = get_tv_type();
    uint32_t control = !sys_bbplayer()? VI_PIXEL_ADVANCE_DEFAULT : VI_PIXEL_ADVANCE_BBPLAYER;

    /* Can't have the video interrupt happening here */
    disable_interrupts();

    /* Minimum is two buffers. */
    __buffers = MAX(2, MIN(NUM_BUFFERS, num_buffers));


    if( res.interlaced )
    {
        /* Serrate on to stop vertical jitter */
        control |= VI_CTRL_SERRATE;
    }

    /* Copy over to temporary for extra initializations */
    config = vi_config_presets[res.interlaced][tv_type];

    /* Figure out control register based on input given */
    switch( bit )
    {
        case DEPTH_16_BPP:
            control |= VI_CTRL_TYPE_16_BIT;
            break;
        case DEPTH_32_BPP:
            control |= VI_CTRL_TYPE_32_BIT;
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
            /* Disabling resampling (AA_MODE = 0x3) on 16bpp hits a hardware bug on NTSC 
               consoles when the horizontal resolution is 320 or lower (see issue #66).
               It would work on PAL consoles, but we think users are better
               served by prohibiting it altogether.

               For people that absolutely need this on PAL consoles, it can
               be enabled with *(volatile uint32_t*)0xA4400000 |= 0x300 just
               after the display_init call. */
            if ( bit == DEPTH_16_BPP )
            {
                assertf(res.width > 320,
                    "FILTERS_DISABLED is not supported by the hardware for widths <= 320.\n"
                    "Please use FILTERS_RESAMPLE instead.");
            }

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
                // Assert on width (see FILTERS_DISABLED)
                assertf(res.width > 320,
                    "FILTERS_DEDITHER is not supported by the hardware for widths <= 320.\n"
                    "Please use FILTERS_RESAMPLE instead.");
                control |= VI_AA_MODE_NONE | VI_DEDITHER_FILTER_ENABLE;
            }
            else control |= VI_AA_MODE_NONE;

            break;
        case FILTERS_RESAMPLE_ANTIALIAS:
            /* Set AA on resample and fetch as well as divot on */
            control |= VI_AA_MODE_RESAMPLE_FETCH_NEEDED;

            break;
        case FILTERS_RESAMPLE_ANTIALIAS_DEDITHER:
            /* Set AA on resample always and fetch as well as dedither on 
            (only on 16bpp mode, act as FILTERS_RESAMPLE_ANTIALIAS on 32bpp) */

            /* Enable dither filter in 16bpp mode to give gradients
               a slightly smoother look */
            if ( bit == DEPTH_16_BPP ) 
                 control |= VI_AA_MODE_RESAMPLE_FETCH_ALWAYS | VI_DEDITHER_FILTER_ENABLE; 
            else control |= VI_AA_MODE_RESAMPLE_FETCH_NEEDED;
            break;
    }

    /* Set the control register in our template */
    config.regs[VI_CTRL] = control;

    /* Calculate width and scale registers */
    assertf(res.width > 0 && res.width <= 800, "invalid width");
    assertf(res.height > 0 && res.height <= 600, "invalid height");
    if( bit == DEPTH_16_BPP )
    {
        assertf(res.width % 4 == 0, "width must be divisible by 4 for 16-bit depth");
    }
    else if ( bit == DEPTH_32_BPP )
    {
        assertf(res.width % 2 == 0, "width must be divisible by 2 for 32-bit depth");
    }
    config.regs[VI_WIDTH] = res.width;
    config.regs[VI_X_SCALE] = ( 1024*res.width + 320 ) / 640;
    config.regs[VI_Y_SCALE] = ( 1024*res.height + 120 ) / 240;

    /* Set up the display */
    __width = res.width;
    __height = res.height;
    __bitdepth = ( bit == DEPTH_16_BPP ) ? 2 : 4;

    surfaces = malloc(sizeof(surface_t) * __buffers);

    /* Initialize buffers and set parameters */
    for( int i = 0; i < __buffers; i++ )
    {
        /* Set parameters necessary for drawing */
        /* Grab a location to render to */
        tex_format_t format = bit == DEPTH_16_BPP ? FMT_RGBA16 : FMT_RGBA32;
        surfaces[i] = surface_alloc(format, __width, __height);
        __safe_buffer[i] = surfaces[i].buffer;
        assert(__safe_buffer[i] != NULL);

        /* Baseline is blank */
        memset( __safe_buffer[i], 0, __width * __height * __bitdepth );
    }

    /* Set the first buffer as the displaying buffer */
    now_showing = 0;
    drawing_mask = 0;
    ready_mask = 0;

    /* Show our screen normally. If display is already active, do that during vblank
       to avoid confusing the VI chip with in-frame modifications. */
    if ( vi_is_active() ) { vi_wait_for_vblank(); }

    config.regs[VI_ORIGIN] = PhysicalAddr(__safe_buffer[0]);
    vi_configure_registers(&config);

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

    // If display is active, wait for vblank before touching the registers
    if( vi_is_active() ) { vi_wait_for_vblank(); }

    *vi_register(VI_H_VIDEO) = 0;
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

surface_t* display_lock(void)
{
    surface_t* retval = NULL;
    int next;

    /* Can't have the video interrupt happening here */
    disable_interrupts();

    /* Calculate index of next display context to draw on. We need
       to find the first buffer which is not being drawn upon nor
       being ready to be displayed. */
    for (next = buffer_next(now_showing); next != now_showing; next = buffer_next(next)) {
        if (((drawing_mask | ready_mask) & (1 << next)) == 0)  {
            retval = &surfaces[next];
            drawing_mask |= 1 << next;
            break;
        }
    }

    enable_interrupts();

    /* Possibility of returning nothing, or a valid display context */
    return retval;
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
 *            A display context retrieved using #display_lock
 */
void display_show_force( display_context_t disp )
{
    /* Can't have the video interrupt screwing this up */
    disable_interrupts();
    display_show(disp);
    __display_callback();
    enable_interrupts();
}

uint32_t display_get_width()
{
    return __width;
}

uint32_t display_get_height()
{
    return __height;
}

uint32_t display_get_bitdepth()
{
    return __bitdepth;
}

uint32_t display_get_num_buffers()
{
    return __buffers;
}
