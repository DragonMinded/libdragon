#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include <libn64.h>
#include "display.h"

/* Constants for easier code management */
#define NUM_BUFFERS         3

#define REGISTER_BASE       0xA4400000
#define REGISTER_COUNT      14

#define TV_TYPE_LOC         0x80000300

#define UNCACHED_ADDR(x)    ((void *)(((uint32_t)(x)) | 0xA0000000))
#define ALIGN_16BYTE(x)     ((void *)(((uint32_t)(x)+15) & 0xFFFFFFF0))

/* Presets for video modes */
static uint32_t ntsc_320[] = {
    0x00000000, 0x00000000, 0x00000140, 0x00000200,
    0x00000000, 0x03e52239, 0x0000020d, 0x00000c15,
    0x0c150c15, 0x006c02ec, 0x002501ff, 0x000e0204,
    0x00000200, 0x00000400 };
static uint32_t pal_320[] = {
    0x00000000, 0x00000000, 0x00000140, 0x00000200,
    0x00000000, 0x0404233a, 0x00000271, 0x00150c69,
    0x0c6f0c6e, 0x00800300, 0x005f0239, 0x0009026b,
    0x00000200, 0x00000400 };
static uint32_t mpal_320[] = {
    0x00000000, 0x00000000, 0x00000140, 0x00000200,
    0x00000000, 0x04651e39, 0x0000020d, 0x00040c11,
    0x0c190c1a, 0x006c02ec, 0x002501ff, 0x000e0204,
    0x00000200, 0x00000400 };
static uint32_t ntsc_640[] = {
    0x00000000, 0x00000000, 0x00000280, 0x00000200,
    0x00000000, 0x03e52239, 0x0000020c, 0x00000c15,
    0x0c150c15, 0x006c02ec, 0x002301fd, 0x000e0204,
    0x00000400, 0x00000800 };
static uint32_t pal_640[] = {
    0x00000000, 0x00000000, 0x00000280, 0x00000200,
    0x00000000, 0x0404233a, 0x00000270, 0x00150c69,
    0x0c6f0c6e, 0x00800300, 0x005d0237, 0x0009026b,
    0x00000400, 0x00000800 };
static uint32_t mpal_640[] = {
    0x00000000, 0x00000000, 0x00000280, 0x00000200,
    0x00000000, 0x04651e39, 0x0000020c, 0x00000c10,
    0x0c1c0c1c, 0x006c02ec, 0x002301fd, 0x000b0202,
    0x00000400, 0x00000800 };

static uint32_t *reg_values[] = { pal_320, ntsc_320, mpal_320, pal_640, ntsc_640, mpal_640 };
static void *buffer[NUM_BUFFERS];
uint32_t __bitdepth;
uint32_t __width;
uint32_t __height;
uint32_t __buffers = NUM_BUFFERS;
void *__safe_buffer[NUM_BUFFERS];

/* currently displayed buffer */
static int now_showing = -1;

/* complete drawn buffer to display next */
static int show_next = -1;

/* buffer currently being drawn on */
static int now_drawing = -1;

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

static void __write_dram_register( void const * const dram_val )
{
    uint32_t *reg_base = (uint32_t *)REGISTER_BASE;

    reg_base[1] = (uint32_t)dram_val;
}

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

void display_init( resolution_t res, bitdepth_t bit, uint32_t num_buffers, gamma_t gamma, antialias_t aa )
{
    uint32_t registers[REGISTER_COUNT];
    uint32_t tv_type = *((uint32_t *)TV_TYPE_LOC);
    uint32_t control = 0x3000;

    /* Ensure that buffering is either double or twiple */
    if( num_buffers != 2 && num_buffers != 3 )
    {
        __buffers = NUM_BUFFERS;
    }
    else
    {
        __buffers = num_buffers;
    }
    
    /* Can't have the video interrupt happening here */
    disable_interrupts();

    /* Switch to high resolution type if needed */
    if( res == RESOLUTION_640x480 ) 
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
    __width = ( res == RESOLUTION_320x240 ) ? 320 : 640;
    __height = ( res == RESOLUTION_320x240) ? 240 : 480;
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
    registerVIhandler( __display_callback );
    set_VI_interrupt( 1, 0x200 );
}

void display_close()
{
    /* Can't have the video interrupt happening here */
    disable_interrupts();
    
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

