/**
 * @file display.c
 * @brief Display Subsystem
 * @ingroup display
 */
#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include "regsinternal.h"
#include "n64sys.h"
#include "display.h"
#include "interrupt.h"
#include "utils.h"
#include "debug.h"

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
static const uint32_t ntsc_512p[] = {
    0x00000000, 0x00000000, 0x00000200, 0x00000200,
    0x00000000, 0x03e52239, 0x0000020d, 0x00000c15,
    0x0c150c15, 0x006c02ec, 0x002501ff, 0x000e0204,
    0x00000333, 0x00000400 };
static const uint32_t pal_512p[] = {
    0x00000000, 0x00000000, 0x00000200, 0x00000200,
    0x00000000, 0x0404233a, 0x00000271, 0x00150c69,
    0x0c6f0c6e, 0x00800300, 0x005f0239, 0x0009026b,
    0x00000333, 0x00000400 };
static const uint32_t mpal_512p[] = {
    0x00000000, 0x00000000, 0x00000200, 0x00000200,
    0x00000000, 0x04651e39, 0x0000020d, 0x00040c11,
    0x0c190c1a, 0x006c02ec, 0x002501ff, 0x000e0204,
    0x00000333, 0x00000400 };
static const uint32_t ntsc_640p[] = {
    0x00000000, 0x00000000, 0x00000280, 0x00000200,
    0x00000000, 0x03e52239, 0x0000020d, 0x00000c15,
    0x0c150c15, 0x006c02ec, 0x002501ff, 0x000e0204,
    0x00000400, 0x00000400 };
static const uint32_t pal_640p[] = {
    0x00000000, 0x00000000, 0x00000280, 0x00000200,
    0x00000000, 0x0404233a, 0x00000271, 0x00150c69,
    0x0c6f0c6e, 0x00800300, 0x005f0239, 0x0009026b,
    0x00000400, 0x00000400 };
static const uint32_t mpal_640p[] = {
    0x00000000, 0x00000000, 0x00000280, 0x00000200,
    0x00000000, 0x04651e39, 0x0000020d, 0x00040c11,
    0x0c190c1a, 0x006c02ec, 0x002501ff, 0x000e0204,
    0x00000400, 0x00000400 };
/** @} */

/** @brief Register initial value array */
static const uint32_t * const reg_values[] = {
    pal_320, ntsc_320, mpal_320,
    pal_640, ntsc_640, mpal_640,
    pal_256, ntsc_256, mpal_256,
    pal_512, ntsc_512, mpal_512,
    pal_512p, ntsc_512p, mpal_512p,
    pal_640p, ntsc_640p, mpal_640p,
};

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

/** @brief True if the buffer indexed by now_drawing is currently locked */
static uint32_t drawing_mask = 0;

static volatile uint32_t ready_mask = 0;

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

/**
 * @brief Initialize the display to a particular resolution and bit depth
 *
 * Initialize video system.  This sets up a double, triple, or multiple
 * buffered drawing surface which can be blitted or rendered to using
 * software or hardware.
 *
 * @param[in] res
 *            The requested resolution
 * @param[in] bit
 *            The requested bit depth
 * @param[in] num_buffers
 *            Number of buffers, usually 2 or 3, but can be more.
 * @param[in] gamma
 *            The requested gamma setting
 * @param[in] aa
 *            The requested anti-aliasing setting
 */
void display_init( resolution_t res, bitdepth_t bit, uint32_t num_buffers, gamma_t gamma, antialias_t aa )
{
    uint32_t registers[REGISTER_COUNT];
    uint32_t tv_type = get_tv_type();
    uint32_t control = !sys_bbplayer() ? 0x3000 : 0x1000;

    /* Can't have the video interrupt happening here */
    disable_interrupts();

    /* Minimum is two buffers. */
    __buffers = MAX(2, MIN(32, num_buffers));

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
		case RESOLUTION_512x240:
			tv_type += 12;
			break;
		case RESOLUTION_640x240:
			tv_type += 15;
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
            if (bit == DEPTH_16_BPP)
                assertf(res != RESOLUTION_256x240 && res != RESOLUTION_320x240,
                    "ANTIALIAS_OFF is not supported by the hardware on 256x240x16 and 320x240x16.\n"
                    "Please use ANTIALIAS_RESAMPLE instead.");

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

    /* Black screen please, the clearing takes a couple frames, and
       garbage would be visible. */
    registers[9] = 0;

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
		case RESOLUTION_512x240:
			__width = 512;
			__height = 240;
			break;
		case RESOLUTION_640x240:
			__width = 640;
			__height = 240;
			break;
	}
    __bitdepth = ( bit == DEPTH_16_BPP ) ? 2 : 4;

    /* Initialize buffers and set parameters */
    for( int i = 0; i < __buffers; i++ )
    {
        /* Set parameters necessary for drawing */
        /* Grab a location to render to */
        buffer[i] = memalign( 64, __width * __height * __bitdepth );
        __safe_buffer[i] = UNCACHED_ADDR( buffer[i] );

        /* Baseline is blank */
        memset( __safe_buffer[i], 0, __width * __height * __bitdepth );
    }

    /* Set the first buffer as the displaying buffer */
    now_showing = 0;
    drawing_mask = 0;
    ready_mask = 0;

    /* Show our screen normally */
    registers[1] = (uintptr_t) __safe_buffer[0];
    registers[9] = reg_values[tv_type][9];
    __write_registers( registers );

    enable_interrupts();

    /* Set which line to call back on in order to flip screens */
    register_VI_handler( __display_callback );
    set_VI_interrupt( 1, 0x2 );
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
    drawing_mask = 0;
    ready_mask = 0;

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
 * then this will return 0, without blocking. 
 * 
 * When you are done drawing on the buffer, use #display_show to unlock
 * the context and schedule the buffer to be displayed on the screen during
 * next vblank.
 * 
 * It is possible to lock more than a display buffer at the same time, for
 * instance to begin working on a new frame while the previous one is still
 * being rendered in parallel through RDP. It is important to notice that
 * display contexts will always be shown on the screen in locking order,
 * irrespective of the order #display_show is called.
 *
 * @return A valid display context to render to or 0 if none is available.
 */
display_context_t display_lock(void)
{
    display_context_t retval = 0;
    int next;

    /* Can't have the video interrupt happening here */
    disable_interrupts();

    /* Calculate index of next display context to draw on. We need
       to find the first buffer which is not being drawn upon nor
       being ready to be displayed. */
    for (next = buffer_next(now_showing); next != now_showing; next = buffer_next(next)) {
        if (((drawing_mask | ready_mask) & (1 << next)) == 0)  {
            retval = next+1;
            drawing_mask |= 1 << next;
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

/**
 * @brief Get the currently configured width of the display in pixels
 */
uint32_t display_get_width()
{
    return __width;
}

/**
 * @brief Get the currently configured height of the display in pixels
 */
uint32_t display_get_height()
{
    return __height;
}

/**
 * @brief Get the currently configured bitdepth of the display
 */
bitdepth_t display_get_bitdepth()
{
    return __bitdepth == 2 ? DEPTH_16_BPP : DEPTH_32_BPP;
}

/**
 * @brief Get the currently configured number of buffers
 */
uint32_t display_get_num_buffers()
{
    return __buffers;
}

/**
 * @brief Get the pointer to the buffer at the specified index
 *
 * @param[in] index
 *            The index of the buffer for which to return the pointer.
 *            To get the buffer pointer for a previously aqcuired display context,
 *            pass the display context minus 1.
 */
void * display_get_buffer(uint32_t index)
{
    return __safe_buffer[index];
}

/** @} */ /* display */
