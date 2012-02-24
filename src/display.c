/**
 * @file display.c
 * @brief Display Subsystem
 * @ingroup display
 */
#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include "libdragon.h"

/**
 * @defgroup display Display Subsystem
 * @ingroup libdragon
 * @brief Video interface system for configuring video output modes and displaying rendered
 *        graphics.
 *
 * The display subsystem handles interfacing with the video interface (VI)
 * and the hardware rasterizer (RDP) to allow software and hardware graphics
 * operations.  It consists of the @ref display, the @ref graphics and the
 * @ref rdp modules.  A separate module, the @ref console, provides a rudimentary
 * console for developers.  Only the display subsystem or the console can be
 * used at the same time.  However, commands to draw console text to the display
 * subsystem are available.
 *
 * The display subsystem module is responsible for initializing the proper video
 * mode for displaying 2D, 3D and softward graphics.  To set up video on the N64,
 * code should call #display_init with the appropriate options.  Once the display
 * has been set, a display context can be requested from the display subsystem using
 * #display_lock.  To draw to the acquired display context, code should use functions
 * present in the @ref graphics and the @ref rdp modules.  Once drawing to a display
 * context is complete, the rendered graphic can be displayed to the screen using 
 * #display_show.  Once code has finished rendering all graphics, #display_close can 
 * be used to shut down the display subsystem.
 *
 * @{
 */

/** @brief Maximum number of video backbuffers */
#define NUM_BUFFERS         3

/** @brief Register location in memory of VI */
#define REGISTER_BASE       0xA4400000
/** @brief Number of 32-bit registers at the register base */
#define REGISTER_COUNT      14

/** @brief Memory location to read which determines the TV type. */
#define TV_TYPE_LOC         0x80000300

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
 * @brief Align a memory address to 16 byte offset
 * 
 * @param[in] x
 *            Unaligned memory address
 *
 * @return An aligned address guaranteed to be >= the unaligned address
 */
#define ALIGN_16BYTE(x)     ((void *)(((uint32_t)(x)+15) & 0xFFFFFFF0))

/**
 * @name Video Mode Register Presets
 * @brief Presets to use when setting a particular video mode
 * @{
 */
static const uint32_t ntsc_320[] = {
    0x00000000, 0x00000000, 0x00000140, 0x00000200,
    0x00000000, 0x03e52239, 0x0000020d, 0x00000c15,
    0x0c150c15, 0x006c02ec, 0x002501ff, 0x000e0204,
    0x00000200, 0x00000400 };
static const uint32_t pal_320[] = {
    0x00000000, 0x00000000, 0x00000140, 0x00000200,
    0x00000000, 0x0404233a, 0x00000271, 0x00150c69,
    0x0c6f0c6e, 0x00800300, 0x005f0239, 0x0009026b,
    0x00000200, 0x00000400 };
static const uint32_t mpal_320[] = {
    0x00000000, 0x00000000, 0x00000140, 0x00000200,
    0x00000000, 0x04651e39, 0x0000020d, 0x00040c11,
    0x0c190c1a, 0x006c02ec, 0x002501ff, 0x000e0204,
    0x00000200, 0x00000400 };
static const uint32_t ntsc_640[] = {
    0x00000000, 0x00000000, 0x00000280, 0x00000200,
    0x00000000, 0x03e52239, 0x0000020c, 0x00000c15,
    0x0c150c15, 0x006c02ec, 0x002301fd, 0x000e0204,
    0x00000400, 0x02000800 };
static const uint32_t pal_640[] = {
    0x00000000, 0x00000000, 0x00000280, 0x00000200,
    0x00000000, 0x0404233a, 0x00000270, 0x00150c69,
    0x0c6f0c6e, 0x00800300, 0x005d0237, 0x0009026b,
    0x00000400, 0x02000800 };
static const uint32_t mpal_640[] = {
    0x00000000, 0x00000000, 0x00000280, 0x00000200,
    0x00000000, 0x04651e39, 0x0000020c, 0x00000c10,
    0x0c1c0c1c, 0x006c02ec, 0x002301fd, 0x000b0202,
    0x00000400, 0x02000800 };
static const uint32_t ntsc_256[] = {
    0x00000000, 0x00000000, 0x00000100, 0x00000200,
    0x00000000, 0x03e52239, 0x0000020d, 0x00000c15,
    0x0c150c15, 0x006c02ec, 0x002501ff, 0x000e0204,
    0x0000019A, 0x00000400 };
static const uint32_t pal_256[] = {
    0x00000000, 0x00000000, 0x00000100, 0x00000200,
    0x00000000, 0x0404233a, 0x00000271, 0x00150c69,
    0x0c6f0c6e, 0x00800300, 0x005f0239, 0x0009026b,
    0x0000019A, 0x00000400 };
static const uint32_t mpal_256[] = {
    0x00000000, 0x00000000, 0x00000100, 0x00000200,
    0x00000000, 0x04651e39, 0x0000020d, 0x00040c11,
    0x0c190c1a, 0x006c02ec, 0x002501ff, 0x000e0204,
    0x0000019A, 0x00000400 };
static const uint32_t ntsc_512[] = {
    0x00000000, 0x00000000, 0x00000200, 0x00000200,
    0x00000000, 0x03e52239, 0x0000020c, 0x00000c15,
    0x0c150c15, 0x006c02ec, 0x002301fd, 0x000e0204,
    0x00000334, 0x02000800 };
static const uint32_t pal_512[] = {
    0x00000000, 0x00000000, 0x00000200, 0x00000200,
    0x00000000, 0x0404233a, 0x00000270, 0x00150c69,
    0x0c6f0c6e, 0x00800300, 0x005d0237, 0x0009026b,
    0x00000334, 0x02000800 };
static const uint32_t mpal_512[] = {
    0x00000000, 0x00000000, 0x00000200, 0x00000200,
    0x00000000, 0x04651e39, 0x0000020c, 0x00000c10,
    0x0c1c0c1c, 0x006c02ec, 0x002301fd, 0x000b0202,
    0x00000334, 0x02000800 };
/** @} */

/** @brief Register initial value array */
static const uint32_t * const reg_values[] = { pal_320, ntsc_320, mpal_320, pal_640, ntsc_640, mpal_640, pal_256, ntsc_256, mpal_256, pal_512, ntsc_512, mpal_512 };
/** @brief Video buffer pointers */
static void *buffer[NUM_BUFFERS];
/** @brief Currently active bit depth */
uint32_t __bitdepth;
/** @brief Currently active video width (calculated) */
uint32_t __width;
/** @brief Currently active video height (calculated) */
uint32_t __height;
/** @brief Number of active buffers */
uint32_t __buffers = NUM_BUFFERS;
/** @brief Pointer to uncached 16-bit aligned version of buffers */
void *__safe_buffer[NUM_BUFFERS];

/** @brief Currently displayed buffer */
static int now_showing = -1;

/** @brief Complete drawn buffer to display next */
static int show_next = -1;

/** @brief Buffer currently being drawn on */
static int now_drawing = -1;

/**
 * @brief Write a set of video registers to the VI
 *
 * @param[in] registers
 *            A pointer to a set of register values to be written
 */
static void __write_registers( uint32_t const * const registers )
{
    uint32_t *reg_base = (uint32_t *)REGISTER_BASE;

    /* This should never happen */
    if( !registers ) { return; }

    /* Just straight copy */
    for( int i = 0; i < REGISTER_COUNT; i++ )
    {
        /* Don't clear interrupts */
        if( i == 3 ) { continue; }
        if( i == 4 ) { continue; }

        reg_base[i] = registers[i];
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
    uint32_t *reg_base = (uint32_t *)REGISTER_BASE;

    reg_base[1] = (uint32_t)dram_val;
}

/**
 * @brief Interrupt handler for vertical blank
 *
 * If there is another frame to display, display the frame
 */
static void __display_callback()
{
    /* Only swap frames if we have a new frame to swap, otherwise just
       leave up the current frame */
    if(show_next >= 0 && show_next != now_drawing)
    {
        __write_dram_register( __safe_buffer[show_next] );

        now_showing = show_next;
        show_next = -1;
    }
}

/**
 * @brief Initialize the display to a particular resolution and bit depth
 *
 * Initialize video system.  This sets up a double or triple buffered drawing surface which can
 * be blitted or rendered to using software or hardware.
 *
 * @param[in] res
 *            The requested resolution
 * @param[in] bit
 *            The requested bit depth
 * @param[in] num_buffers
 *            Number of buffers (2 or 3)
 * @param[in] gamma
 *            The requested gamma setting
 * @param[in] aa
 *            The requested anti-aliasing setting
 */
void display_init( resolution_t res, bitdepth_t bit, uint32_t num_buffers, gamma_t gamma, antialias_t aa )
{
    uint32_t registers[REGISTER_COUNT];
    uint32_t tv_type = *((uint32_t *)TV_TYPE_LOC);
    uint32_t control = 0x3000;

    /* Can't have the video interrupt happening here */
    disable_interrupts();

    /* Ensure that buffering is either double or twiple */
    if( num_buffers != 2 && num_buffers != 3 )
    {
        __buffers = NUM_BUFFERS;
    }
    else
    {
        __buffers = num_buffers;
    }

	switch( res )
	{
		case RESOLUTION_640x480:
			/* Serrate on to stop vertical jitter */
			control |= 0x40;
			tv_type += 3;
			break;
		case RESOLUTION_256x240:
			tv_type += 6;
			break;
		case RESOLUTION_512x480:
			/* Serrate on to stop vertical jitter */
			control |= 0x40;
			tv_type += 9;
			break;
		case RESOLUTION_320x240:
		default:
			break;
    }

    /* Copy over to temporary for extra initializations */
    memcpy( registers, reg_values[tv_type], sizeof( uint32_t ) * REGISTER_COUNT );

    /* Figure out control register based on input given */
    switch( bit )
    {
        case DEPTH_16_BPP:
            /* We enable divot filter in 16bpp mode to give our gradients
               a slightly smoother look */
            control |= 0x10002;
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
            /* Set AA off flag */
            control |= 0x300;
            break;
        case ANTIALIAS_RESAMPLE:
            /* Set AA on resample as well as divot on */
            control |= 0x210;
            break;
        case ANTIALIAS_RESAMPLE_FETCH_NEEDED:
            /* Set AA on resample and fetch as well as divot on */
            control |= 0x110;
            break;
        case ANTIALIAS_RESAMPLE_FETCH_ALWAYS:
            /* Set AA on resample always and fetch as well as divot on */
            control |= 0x010;
            break;
    }

    /* Set the control register in our template */
    registers[0] = control;

    /* Set up initial registers */
    __write_registers( registers );

    /* Set up the display */
	switch( res )
	{
		case RESOLUTION_320x240:
			__width = 320;
			__height = 240;
			break;
		case RESOLUTION_640x480:
			__width = 640;
			__height = 480;
			break;
		case RESOLUTION_256x240:
			__width = 256;
			__height = 240;
			break;
		case RESOLUTION_512x480:
			__width = 512;
			__height = 480;
			break;
	}
    __bitdepth = ( bit == DEPTH_16_BPP ) ? 2 : 4;

    /* Initialize buffers and set parameters */
    for( int i = 0; i < __buffers; i++ )
    {
        /* Set parameters necessary for drawing */
        /* Grab a location to render to */
        buffer[i] = malloc( __width * __height * __bitdepth + 15 );
        __safe_buffer[i] = ALIGN_16BYTE( UNCACHED_ADDR( buffer[i] ) );

        /* Baseline is blank */
        memset( __safe_buffer[i], 0, __width * __height * __bitdepth );
    }

    /* Set the first buffer as the displaying buffer */
    __write_dram_register( __safe_buffer[0] );

    now_showing = 0;
    now_drawing = -1;
    show_next = -1;

    enable_interrupts();

    /* Set which line to call back on in order to flip screens */
    register_VI_handler( __display_callback );
    set_VI_interrupt( 1, 0x200 );
}

/**
 * @brief Close the display
 *
 * Close a display and free buffer memory associated with it.
 */
void display_close()
{
    /* Can't have the video interrupt happening here */
    disable_interrupts();

    set_VI_interrupt( 0, 0 );
    unregister_VI_handler( __display_callback );

    now_showing = -1;
    now_drawing = -1;
    show_next = -1;

    __width = 0;
    __height = 0;

    __write_dram_register( 0 );

    for( int i = 0; i < __buffers; i++ )
    {
        /* Free framebuffer memory */
        if( buffer[i] )
        {
            free( buffer[i]);
        }

        buffer[i] = 0;
        __safe_buffer[i] = 0;
    }

    enable_interrupts();
}

/**
 * @brief Lock a display buffer for rendering
 *
 * Grab a display context that is safe for drawing.  If none is available
 * then this will return 0.  Do not check out more than one display
 * context at a time.
 *
 * @return A valid display context to render to or 0 if none is available.
 */
display_context_t display_lock()
{
    display_context_t retval = 0;

    /* Can't have the video interrupt happening here */
    disable_interrupts();

    for( int i = 0; i < __buffers; i++ )
    {
        if( i != now_showing && i != now_drawing && i != show_next )
        {
            /* This screen should be returned */
            now_drawing = i;
            retval = i + 1;

            break;
        }
    }

    enable_interrupts();

    /* Possibility of returning nothing, or a valid display context */
    return retval;
}

/**
 * @brief Display a previously locked buffer
 *
 * Display a valid display context to the screen on the next vblank.  Display
 * contexts should be locked via #display_lock.
 *
 * @param[in] disp
 *            A display context retrieved using #display_lock
 */
void display_show( display_context_t disp )
{
    /* They tried drawing on a bad context */
    if( disp == 0 ) { return; }

    /* Can't have the video interrupt screwing this up */
    disable_interrupts();

    /* Correct to ensure we are handling the right screen */
    int i = disp - 1;

    /* This should match, or something went awry */
    if( i == now_drawing )
    {
        /* Ensure we display this next time */
        now_drawing = -1;
        show_next = i;
    }

    enable_interrupts();
}

/** @} */ /* display */
