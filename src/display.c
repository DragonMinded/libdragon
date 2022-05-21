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

/** @brief Maximum number of video backbuffers */
#define NUM_BUFFERS         32

/** @brief Register location in memory of VI */
#define REGISTER_BASE       0xA4400000
/** @brief Number of 32-bit registers at the register base */
#define REGISTER_COUNT      14

/** 
 * @brief Return the uncached memory address of a cached address
 *
 * @param[in] x 
 *            The cached address
 *
 * @return The uncached address
 */
#define UNCACHED_ADDR(x)    ((void *)(((uint32_t)(x)) | 0xA0000000))

/**
 * @name Video Mode Register Presets
 * @brief Presets to use when setting a particular video mode
 * @{
 */
static const uint32_t ntsc_p[] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x03e52239, 0x0000020d, 0x00000c15,
    0x0c150c15, 0x006c02ec, 0x002501ff, 0x000e0204,
    0x00000000, 0x00000000 };
static const uint32_t pal_p[] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x0404233a, 0x00000271, 0x00150c69,
    0x0c6f0c6e, 0x00800300, 0x005f0239, 0x0009026b,
    0x00000000, 0x00000000 };
static const uint32_t mpal_p[] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x04651e39, 0x0000020d, 0x00040c11,
    0x0c190c1a, 0x006c02ec, 0x002501ff, 0x000e0204,
    0x00000000, 0x00000000 };
static const uint32_t ntsc_i[] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x03e52239, 0x0000020c, 0x00000c15,
    0x0c150c15, 0x006c02ec, 0x002301fd, 0x000e0204,
    0x00000000, 0x00000000 };
static const uint32_t pal_i[] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x0404233a, 0x00000270, 0x00150c69,
    0x0c6f0c6e, 0x00800300, 0x005d0237, 0x0009026b,
    0x00000000, 0x00000000 };
static const uint32_t mpal_i[] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x04651e39, 0x0000020c, 0x00000c10,
    0x0c1c0c1c, 0x006c02ec, 0x002301fd, 0x000b0202,
    0x00000000, 0x00000000 };
/** @} */

/** @brief Register initial value array */
static const uint32_t * const reg_values[] = {
    pal_p, ntsc_p, mpal_p,
    pal_i, ntsc_i, mpal_i,
};

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
 * @brief Write a set of video registers to the VI
 *
 * @param[in] registers
 *            A pointer to a set of register values to be written
 */
static void __write_registers( uint32_t const * const registers )
{
    volatile uint32_t *reg_base = (uint32_t *)REGISTER_BASE;

    /* This should never happen */
    if( !registers ) { return; }

    /* Just straight copy */
    for( int i = 0; i < REGISTER_COUNT; i++ )
    {
        /* Don't clear interrupts */
        if( i == 3 ) { continue; }
        if( i == 4 ) { continue; }

        reg_base[i] = registers[i];
        MEMORY_BARRIER();
    }
}

/**
 * @brief Update the framebuffer pointer in the VI
 *
 * @param[in] dram_val
 *            The new framebuffer to use for display.  Should be aligned and uncached.
 */
static void __write_dram_register( void const * const dram_val )
{
    volatile uint32_t *reg_base = (uint32_t *)REGISTER_BASE;

    reg_base[1] = (uint32_t)dram_val;
    MEMORY_BARRIER();
}

/** @brief Wait until entering the vblank period */
static void __wait_for_vblank()
{
    volatile uint32_t *reg_base = (uint32_t *)REGISTER_BASE;

    while( reg_base[4] != 2 ) {  }
}

/** @brief Return true if VI is active (16 or 32-bit color set) */
static inline bool __is_vi_active()
{
    volatile uint32_t *reg_base = (uint32_t *)REGISTER_BASE;

    return (reg_base[0] & 0x03) >= 2;
}

/**
 * @brief Interrupt handler for vertical blank
 *
 * If there is another frame to display, display the frame
 */
static void __display_callback()
{
    volatile uint32_t *reg_base = (uint32_t *)REGISTER_BASE;

    /* Least significant bit of the current line register indicates
       if the currently displayed field is odd or even. */
    bool field = reg_base[4] & 1;

    /* Check if the next buffer is ready to be displayed, otherwise just
       leave up the current frame */
    int next = buffer_next(now_showing);
    if (ready_mask & (1 << next)) {
        now_showing = next;
        ready_mask &= ~(1 << next);
    }

    __write_dram_register(__safe_buffer[now_showing] + (!field ? __width * __bitdepth : 0));
}

void display_init( resolution_t res, bitdepth_t bit, uint32_t num_buffers, gamma_t gamma, antialias_t aa )
{
    uint32_t registers[REGISTER_COUNT];
    uint32_t tv_type = get_tv_type();
    uint32_t control = !sys_bbplayer() ? 0x3000 : 0x1000;

    /* Can't have the video interrupt happening here */
    disable_interrupts();

    /* Minimum is two buffers. */
    __buffers = MAX(2, MIN(NUM_BUFFERS, num_buffers));


    if( res.interlaced )
    {
        /* Serrate on to stop vertical jitter */
        control |= 0x40;
        tv_type += 3;
    }

    /* Copy over to temporary for extra initializations */
    memcpy( registers, reg_values[tv_type], sizeof( uint32_t ) * REGISTER_COUNT );

    /* Figure out control register based on input given */
    switch( bit )
    {
        case DEPTH_16_BPP:
            control |= 0x2;
            break;
        case DEPTH_32_BPP:
            control |= 0x3;
            break;
    }

    switch( gamma )
    {
        case GAMMA_NONE:
            /* Nothing to set here */
            break;
        case GAMMA_CORRECT:
            control |= 0x8;
            break;
        case GAMMA_CORRECT_DITHER:
            control |= 0xC;
            break;
    }

    switch( aa )
    {
        case ANTIALIAS_OFF:
            /* Disabling antialias hits a hardware bug on NTSC consoles on
               low resolutions (see issue #66). We do not know the exact
               horizontal scale minimum, but among libdragon's supported
               resolutions the bug appears on 256x240x16 and 320x240x16. It would
               work on PAL consoles, but we think users are better served by
               prohibiting it altogether. 

               For people that absolutely need this on PAL consoles, it can
               be enabled with *(volatile uint32_t*)0xA4400000 |= 0x300 just
               after the display_init call. */
            if ( bit == DEPTH_16_BPP )
            {
                assertf(res.width > 320,
                    "ANTIALIAS_OFF is not supported by the hardware for widths <= 320.\n"
                    "Please use ANTIALIAS_RESAMPLE instead.");
            }

            /* Set AA off flag */
            control |= 0x300;

            /* Dither filter should not be enabled with this AA mode
               as it will cause ugly vertical streaks */
            break;
        case ANTIALIAS_RESAMPLE:
            /* Set AA on resample as well as divot on */
            control |= 0x210;

            /* Dither filter should not be enabled with this AA mode
               as it will cause ugly vertical streaks */
            break;
        case ANTIALIAS_RESAMPLE_FETCH_NEEDED:
            /* Set AA on resample and fetch as well as divot on */
            control |= 0x110;

            /* Enable dither filter in 16bpp mode to give gradients
               a slightly smoother look */
            if ( bit == DEPTH_16_BPP ) { control |= 0x10000; }
            break;
        case ANTIALIAS_RESAMPLE_FETCH_ALWAYS:
            /* Set AA on resample always and fetch as well as divot on */
            control |= 0x010;

            /* Enable dither filter in 16bpp mode to give gradients
               a slightly smoother look */
            if ( bit == DEPTH_16_BPP ) { control |= 0x10000; }
            break;
    }

    /* Set the control register in our template */
    registers[0] = control;

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
    registers[2] = res.width;
    registers[12] = ( 1024*res.width + 320 ) / 640;
    registers[13] = ( 1024*res.height + 120 ) / 240;

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

    if ( __is_vi_active() ) { __wait_for_vblank(); }

    /* Show our screen normally */
    registers[1] = (uintptr_t) __safe_buffer[0];
    __write_registers( registers );

    enable_interrupts();

    /* Set which line to call back on in order to flip screens */
    register_VI_handler( __display_callback );
    set_VI_interrupt( 1, 0x2 );
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

    if( __is_vi_active() ) { __wait_for_vblank(); }

    volatile uint32_t *reg_base = (uint32_t *)REGISTER_BASE;
    reg_base[9] = 0;
    __write_dram_register( 0 );

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
