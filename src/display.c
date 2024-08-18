/**
 * @file display.c
 * @brief Display Subsystem
 * @ingroup display
 */
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>
#include <string.h>
#include <math.h>
#include "regsinternal.h"
#include "system_internal.h"
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
/** @brief How many times per second we should update the FPS value */
#define FPS_UPDATE_FREQ      4

static surface_t *surfaces;
/** @brief Currently allocated Z-buffer */
static surface_t surf_zbuf;
/** @brief Record whehter the Z-buffer as allocated via sbrk_top */
bool zbuf_sbrk_top = false;
/** @brief Currently active bit depth */
static uint32_t __bitdepth;
/** @brief Currently active video width (calculated) */
static uint32_t __width;
/** @brief Currently active video height (calculated) */
static uint32_t __height;
/** @brief Currently active video interlace mode */
static interlace_mode_t __interlace_mode = INTERLACE_OFF;
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
/** @brief Auto detected TV region for display */
static uint32_t __tv_type;
/** @brief Actual display refresh rate */
static float refresh_rate;
/** @brief Actual display refresh period */
static float refresh_period;
/** @brief Currently calculated delta time estimation */
static volatile float delta_time;
/** @brief Snapshot of frame rate for display purposes (avoid changing it too fast) */
static volatile float frame_rate_snapshot;
/** @brief Factor between TV refresh rate and requested virtual refresh rate (#display_set_fps_limit) */
static float frame_skip;
/** @brief Minimum refresh period as requested by #display_set_fps_limit */
static float min_refresh_period;
/** @brief Rounded minimum refresh period as requested by #display_set_fps_limit */
static float min_refresh_period_rounded;

/** @brief State for the Kalman filter */
typedef struct {
    float P;            ///< Process noise covariance
    float Q;            ///< Measurement noise covariance
    float R;            ///< Estimation error covariance
    float p_estimate;   ///> Last estimated value
} kalman_state_t;

/** @brief State for kalman filter used for FPS estimation */
static kalman_state_t k_fps;
/** @brief State for kalman filter used for delta-time estimation */
static kalman_state_t k_delta;

/** @brief Initalize Kalman's filter  */
static void kalman_init(kalman_state_t *s, float x, float Q)
{
    s->P = 1.0f;
    s->Q = Q;
    s->R = 1.0f;
    s->p_estimate = x;
}

/** @brief Run kalman's filter */
static float kalman(kalman_state_t *s, float x)
{
    float p_pred = s->p_estimate;
    float P_pred = s->P + s->Q;

    float K = P_pred / (P_pred + s->R);
    s->p_estimate = p_pred + K * (x - p_pred);
    s->P = (1 - K) * P_pred;

    return s->p_estimate;
}

/** @brief Get the next buffer index (with wraparound) */
static inline int buffer_next(int idx) {
    idx += 1;
    if (idx == __buffers)
        idx = 0;
    return idx;
}

/** Calculate the actual refresh rate of the display given the current VI configuration */
static float calc_refresh_rate(void)
{
    int clock;
    switch (__tv_type) {
        case TV_PAL:    clock = 49656530; break;
        case TV_MPAL:   clock = 48628322; break;
        default:        clock = 48681818; break;
    }
    uint32_t HSYNC = *VI_H_SYNC;
    uint32_t VSYNC = *VI_V_SYNC;
    uint32_t HSYNC_LEAP = *VI_H_SYNC_LEAP;

    int h_sync = (HSYNC & 0xFFF) + 1;
    int v_sync = (VSYNC & 0x3FF) + 1;
    int h_sync_leap_b = (HSYNC_LEAP >>  0) & 0xFFF;
    int h_sync_leap_a = (HSYNC_LEAP >> 16) & 0xFFF;
    int leap_bitcount = 0;
    int mask = 1<<16;
    for (int i=0; i<5; i++) {
        if (HSYNC & mask) leap_bitcount++;
        mask <<= 1;
    }
    int h_sync_leap_avg = (h_sync_leap_a * leap_bitcount + h_sync_leap_b * (5 - leap_bitcount)) / 5;

    return (float)clock / ((h_sync * (v_sync - 2) / 2 + h_sync_leap_avg));
}

/** 
 * @brief Check if we should use this vblank interrupt or not, depending on fps limit 
 * 
 * FPS limit is implemented simply by pretending the hardware is slower at generating
 * video interrupts, which in turn means skipping an interrupt every now and then
 * to keep the frame rate within the desired limits.
 */
static bool fps_limit_ok(void)
{
    static float frame_skip_accum = 0.0f;
    frame_skip_accum += frame_skip;
    if (frame_skip_accum < 0.0f) return false;
    frame_skip_accum -= 1.0f;
    return true;
}

/** 
 * @brief Update FPS estimation. 
 * 
 * This function is on every "virtual" vblank (that is, only on vblank interrupts
 * which are not ignored by #fps_limit_ok). It updates the estimation of the
 * frame rate using a Kalman filter, based on the number of frames that were
 * actually displayed.
 * 
 * @param newframe      True if a new frame was displayed in this vblank, false otherwise
 */
static void update_fps(bool newframe)
{
    static int last_frame_counter = 0;
    ++last_frame_counter;
    if (!newframe) return;

    // Calculate updated delta_time and frame_rate. Technically one is just the
    // reciprocal of the other, but we prefer a more reactive kalman filter (Q=1)
    // for delta_time, and a more stable one (Q=0.01) for frame_rate for display purposes.
    delta_time = kalman(&k_delta, last_frame_counter) * min_refresh_period;
    float kk_fps = kalman(&k_fps, last_frame_counter);

    // Take a few snapshots of the framerate for display purposes.
    static uint32_t last_update = 0;
    uint32_t now = TICKS_READ();
    if (TICKS_DISTANCE(last_update, now) > TICKS_PER_SECOND / FPS_UPDATE_FREQ) {
        last_update = now;        
        frame_rate_snapshot = 1.0f / (kk_fps * min_refresh_period_rounded);
    }

    last_frame_counter = 0;
}

/**
 * @brief Interrupt handler for vertical blank
 *
 * If there is another frame to display, display the frame
 */
static void __display_callback()
{
    // If a reset has occured and its the last VI interrupt before RESET_TIME_LENGTH grace period, stop all work and exit
    uint32_t next_call = __tv_type == TV_PAL? 
        TICKS_FROM_MS(VI_PERIOD_PAL) : 
        TICKS_FROM_MS(VI_PERIOD_NTSC_MPAL);

    if(exception_reset_time() + next_call >= RESET_TIME_LENGTH) die();

    /* Least significant bit of the current line register indicates
       if the currently displayed field is odd or even. */
    bool field = (*VI_V_CURRENT) & 1;
    bool interlaced = (*VI_CTRL) & (VI_CTRL_SERRATE);

    /* Check if the next buffer is ready to be displayed, otherwise just
       leave up the current frame. If full interlace mode is selected
       then don't update the buffer until two fields were displayed. */
    if (!(__interlace_mode == INTERLACE_FULL && field) && fps_limit_ok()) {
        bool newframe = false;
        int next = buffer_next(now_showing);
        if (ready_mask & (1 << next)) {
            now_showing = next;
            ready_mask &= ~(1 << next);
            newframe = true;
        }
        update_fps(newframe);
    }

    vi_write_dram_register(__safe_buffer[now_showing] + (interlaced && !field ? __width * __bitdepth : 0));

    // FIXME: PAL-M on old boards like NUS-CPU-02 requires changing V_BURST every field, otherwise
    // the image seems garbled at the top. It is probably a bug in old revisions of the VI chip,
    // since the problem doesn't exist on newer boards.
    if (__tv_type == TV_MPAL && interlaced) {
        if (field == 0) {
            *VI_V_BURST = 0x000b0202;
        } else {
            *VI_V_BURST = 0x000e0204;
        }
    }
}

void display_init( resolution_t res, bitdepth_t bit, uint32_t num_buffers, gamma_t gamma, filter_options_t filters )
{
    __tv_type = get_tv_type();
    uint32_t control = !sys_bbplayer()? VI_PIXEL_ADVANCE_DEFAULT : VI_PIXEL_ADVANCE_BBPLAYER;

    /* Can't have the video interrupt happening here */
    disable_interrupts();

    /* Minimum is two buffers. */
    __buffers = MAX(1, MIN(NUM_BUFFERS, num_buffers));

    bool serrate = res.interlaced != INTERLACE_OFF;
    /* Serrate on to stop vertical jitter */
    if(serrate) control |= VI_CTRL_SERRATE;

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

    /* Set basic preset */
    vi_write_config(&vi_config_presets[serrate][__tv_type]);

    if( __tv_type == TV_PAL && res.pal60 )
    {
        /* 60Hz PAL is a regular PAL video mode with NTSC-like V_SYNC and V_VIDEO */

        /* NOTE: Ideally V_SYNC would be 524/525, matching NTSC, however in practice this appears
                to cause too-slow retrace intervals. Instead we use 518/519 half-lines which is
                only a 1.14% deviation, the expectation is that this is within the tolerance ranges
                of almost all devices.
                Alternatively we could have elected to shorten H_SYNC, however H_SYNC is expected
                to be less tolerant than V_SYNC so we opt to leave it alone at the nominal value. */
        vi_write_safe(VI_V_SYNC, VI_V_SYNC_SET(525 - 6 - serrate));
        vi_write_safe(VI_V_VIDEO, (serrate) ? vi_ntsc_i.regs[VI_TO_INDEX(VI_V_VIDEO)] : vi_ntsc_p.regs[VI_TO_INDEX(VI_V_VIDEO)]);
    }

    /* Configure other VI registers */
    vi_write_safe(VI_ORIGIN, PhysicalAddr(__safe_buffer[0]));
    vi_write_safe(VI_WIDTH, res.width);
    vi_write_safe(VI_X_SCALE, VI_X_SCALE_SET(res.width));
    if (__tv_type == TV_PAL)
    {
        vi_write_safe(VI_Y_SCALE, VI_Y_SCALE_SET_288_LINES(res.height));
    }
    else
    {
        vi_write_safe(VI_Y_SCALE, VI_Y_SCALE_SET_240_LINES(res.height));
    }
    vi_write_safe(VI_CTRL, control);

    /* Calculate actual refresh rate for this configuration */
    refresh_rate = calc_refresh_rate();
    refresh_period = 1.0f / refresh_rate;
    frame_rate_snapshot = refresh_rate;
    display_set_fps_limit(0);
    kalman_init(&k_fps, 1.0f, 0.01f);
    kalman_init(&k_delta, 1.0f, 1.0f);

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

    if ( surf_zbuf.buffer )
    {
        surface_free(&surf_zbuf);
        
        if (zbuf_sbrk_top) {
            sbrk_top(-__width * __height * 2);
            zbuf_sbrk_top = false;
        }
    }

    __width = 0;
    __height = 0;

    /* If display is active, wait for vblank before touching the registers */
    if( vi_is_active() ) { vi_wait_for_vblank(); }

    vi_set_blank_image();
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

surface_t* display_get_zbuf(void)
{
    if (surf_zbuf.buffer == NULL) {
        /* Try to allocate the Z-Buffer from the top of the heap (near the stack).
           This basically puts it in the last memory bank, hopefully separating it
           from framebuffers, which provides a nice speed gain. */
        void *buf = sbrk_top(__width * __height * 2);
        if (buf != (void*)-1) {
            data_cache_hit_invalidate(buf, __width * __height * 2);
            surf_zbuf = surface_make(UncachedAddr(buf), FMT_RGBA16, __width, __height, __width*2);
            zbuf_sbrk_top = true;
        } else {
            surf_zbuf = surface_alloc(FMT_RGBA16, __width, __height);
            zbuf_sbrk_top = false;
        }
    }
 
    return &surf_zbuf;
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
    return frame_rate_snapshot;
}

float display_get_refresh_rate(void)
{
    return refresh_rate;
}

float display_get_delta_time(void)
{
    return delta_time;
}

void display_set_fps_limit(float fps)
{
    disable_interrupts();

    min_refresh_period = 1.0f / (fps ? fps : refresh_rate);
    frame_skip = refresh_period / min_refresh_period;
    delta_time = min_refresh_period;

    // Calculate also the minimum period using a rounded refresh rate
    // This will be used only for display purposes, so that FPS are capped
    // to 60 Hz rather than 59.83 Hz, which would be the hw-accurate value.
    min_refresh_period_rounded = 1.0f / (fps ? fps : roundf(refresh_rate));

    enable_interrupts();
}

extern inline void vi_write_config(const vi_config_t* config);
