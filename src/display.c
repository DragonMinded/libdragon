/**
 * @file display.c
 * @author Jennifer Taylor <dragonminded@dragonminded.com>
 * @author Giovanni Bajo <giovannibajo@gmail.com>
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
#include "display.h"
#include "interrupt.h"
#include "utils.h"
#include "debug.h"
#include "surface.h"
#include "rsp.h"
#include "kirq.h"
#include "accounting_internal.h"

/** @brief Maximum number of video backbuffers */
#define NUM_BUFFERS         32
/** @brief Number of past frames used to evaluate FPS */
#define FPS_WINDOW          32
/** @brief How many times per second we should update the FPS value */
#define FPS_UPDATE_FREQ      4

static surface_t *surfaces;
/** @brief Currently allocated Z-buffer (allocated for __alloc_width x __alloc_height) */
static surface_t surf_zbuf;
/** @brief View of zbuf with current getter dimensions (__width x __height); returned by display_get_zbuf */
static surface_t surf_zbuf_view;
/** @brief Record whether the Z-buffer was allocated via sbrk_top */
static bool zbuf_sbrk_top = false;
/** @brief True if the vblank handler is installed */
static bool handler_installed = false;
/** @brief Currently active bit depth */
static uint32_t __bitdepth;
/** @brief Currently active video width (calculated) */
static uint32_t __width;
/** @brief Currently active video height (calculated) */
static uint32_t __height;
/** @brief Currently active video interlace mode */
static interlace_mode_t __interlace_mode = INTERLACE_OFF;
/** @brief Number of active buffers (returned by display_get_num_buffers); also used for the assignable ring in display_try_get. */
static int __num_buffers = 0;
/** @brief Allocated framebuffer dimensions (from display_init; used for display_change validation and zbuf) */
static uint32_t __alloc_width = 0;
static uint32_t __alloc_height = 0;
static uint32_t __alloc_bitdepth = 0;
static uint32_t __alloc_buffers = 0;
/** @brief Stored params for apply_display_vi_config when a display_change() is pending */
static resolution_t pending_res;
static bitdepth_t pending_bit;
static gamma_t pending_gamma;
static filter_options_t pending_filters;
static interlace_mode_t pending_interlace_mode;
/** @brief Pending display_change countdown: -1 means no pending config, >=0 means frames left before apply */
static int pending_vi_frames_left = -1;
/** @brief Currently displayed buffer */
static int now_showing = -1;
/** @brief FIFO queue of buffer indices in the order they were obtained via display_get/display_try_get */
static uint8_t display_queue[NUM_BUFFERS];
/** @brief Queue head (next index that should be shown, once ready) */
static uint8_t display_queue_head = 0;
/** @brief Queue tail (where next obtained index is appended) */
static uint8_t display_queue_tail = 0;
/** @brief Number of queued buffers that must be shown in FIFO order */
static uint8_t display_queue_count = 0;
/** @brief Bitmask of surfaces that are currently being drawn */
static uint32_t drawing_mask = 0;
/** @brief Bitmask of surfaces that are ready to be shown */
static volatile uint32_t ready_mask = 0;
/** @brief Actual display refresh rate */
static float refresh_rate;
/** @brief Actual display refresh period */
static float refresh_period;
/** @brief Currently set FPS limit */
static float fps_limit;
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
/** @brief True if we are applying the workaround for the VI bug on 320x16-bit unfiltered mode */
static bool vi_bug_workaround = false;

static void apply_display_vi_config(resolution_t res, bitdepth_t bit, gamma_t gamma, filter_options_t filters);

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

/** @brief Get the next buffer index (with wraparound) for the configured ring; used by display_try_get. */
static inline int buffer_next(int idx) {
    idx += 1;
    if (idx >= __num_buffers)
        idx = 0;
    return idx;
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
        // Update the refresh rate in case it changed (eg: switch PAL50/PAL60)
        refresh_rate = vi_get_refresh_rate();
        refresh_period = 1.0f / refresh_rate;    
        display_set_fps_limit(fps_limit);
    }

    last_frame_counter = 0;
}

/**
 * @brief Interrupt handler for vertical blank
 *
 * If there is another frame to display, display the frame
 */
static void __display_callback(void *arg)
{
    // If a reset has occured and this is almost the last VI interrupt
    // before RESET_TIME_LENGTH grace period, stop all work and exit
    uint32_t next_time = TICKS_FROM_MS(refresh_period*1000);
    if(exception_reset_time() + next_time*3 >= RESET_TIME_LENGTH) die();

    /* Least significant bit of the current line register indicates
       if the currently displayed field is odd or even. */
    bool field = (*VI_V_CURRENT) & 1;

    bool apply_pending_vi_config = false;

    /* Show frames in strict FIFO order (order of display_get/display_try_get).
       If full interlace mode is selected then don't update the buffer until two fields were displayed. */
    if (!(__interlace_mode == INTERLACE_FULL && field) && fps_limit_ok()) {
        bool newframe = false;
        if (display_queue_count > 0) {
            int next = display_queue[display_queue_head];
            if (ready_mask & (1U << next)) {
                if (pending_vi_frames_left >= 0) {
                    if (pending_vi_frames_left > 0) {
                        pending_vi_frames_left--;
                    } else {
                        apply_pending_vi_config = true;
                        pending_vi_frames_left = -1;
                    }
                }
                now_showing = next;
                ready_mask &= ~(1U << next);
                display_queue_head = (display_queue_head + 1) % NUM_BUFFERS;
                display_queue_count--;
                newframe = true;
            }
        }
        update_fps(newframe);
    }

    vi_write_begin();
    if (apply_pending_vi_config) {
        __interlace_mode = pending_interlace_mode;
        apply_display_vi_config(pending_res, pending_bit, pending_gamma, pending_filters);
    }
    vi_show(&surfaces[now_showing]);
    if ( vi_bug_workaround ) vi_write(VI_X_SCALE, 0x201);
    vi_write_end();
}

/**
 * @brief Apply VI configuration (interlace, gamma, filters, borders, vi_bug_workaround).
 * Caller must hold vi_write_begin() / vi_write_end() around this.
 */
static void apply_display_vi_config(resolution_t res, bitdepth_t bit, gamma_t gamma, filter_options_t filters)
{
    vi_set_interlaced(res.interlaced != INTERLACE_OFF);
    vi_set_gamma((vi_gamma_t)gamma);

    switch (filters)
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

               For the very common case of width=320 exactly, we can do a workaround,
               which is setting a slightly higher VI XSCALE (0x201 instead of 0x200)
               which workarounds the bug without any artifact. See below where the
               fix is applied.

               For people that absolutely need this on PAL consoles, call display_init()
               with FILTERS_RESAMPLE, and then call vi_set_aa_mode(VI_AA_MODE_NONE); */
            if (bit == DEPTH_16_BPP) {
                assertf(res.width >= 320,
                    "FILTERS_DISABLED is not supported by the hardware for widths <= 320.\n"
                    "Please use FILTERS_RESAMPLE instead.");
            }
            vi_set_aa_mode(VI_AA_MODE_NONE);
            vi_set_divot(false);
            vi_set_dedither(false);
            break;
        case FILTERS_RESAMPLE:
            /* Set AA on resample */
            vi_set_aa_mode(VI_AA_MODE_RESAMPLE);

            /* Dither filter should not be enabled with this AA mode
               as it will cause ugly vertical streaks */
            vi_set_divot(false);
            vi_set_dedither(false);
            break;
        case FILTERS_DEDITHER:
            /* Set AA off flag and dedither on
               (only on 16bpp mode, act as FILTERS_DISABLED on 32bpp) */
            if (bit == DEPTH_16_BPP) {
                /* Assert on width (see FILTERS_DISABLED) */
                assertf(res.width > 320,
                    "FILTERS_DEDITHER is not supported by the hardware for widths <= 320.\n"
                    "Please use FILTERS_RESAMPLE instead.");
                vi_set_aa_mode(VI_AA_MODE_NONE);
                vi_set_divot(false);
                vi_set_dedither(true);
            } else {
                vi_set_aa_mode(VI_AA_MODE_NONE);
                vi_set_divot(false);
                vi_set_dedither(false);
                }
            break;
        case FILTERS_RESAMPLE_ANTIALIAS:
            /* Set AA on resample and fetch as well as divot on.
             * FETCH_ALWAYS seems to change the way VI operates on the bus
             * to work better when there is bandwidth saturation; so
             * even if the RDRAM bus is very busy, the VI will still get the
             * data it needs.
             *
             * Note that FETCH_ALWAYS appears to be broken in 32bpp modes, so
             * we cannot use it there; this means that it'll be much easier
             * to get image corruption for VI bandwidth saturation in 32bpp modes.
             */
            if (bit == DEPTH_16_BPP)
                vi_set_aa_mode(VI_AA_MODE_RESAMPLE_FETCH_ALWAYS);
            else
                vi_set_aa_mode(VI_AA_MODE_RESAMPLE_FETCH_NEEDED);
            vi_set_divot(true);
            vi_set_dedither(false);
            break;
        case FILTERS_RESAMPLE_ANTIALIAS_DEDITHER:
            /* Set AA on resample always and fetch as well as dedither on
               (only on 16bpp mode, act as FILTERS_RESAMPLE_ANTIALIAS on 32bpp) */
            if (bit == DEPTH_16_BPP) {
                vi_set_aa_mode(VI_AA_MODE_RESAMPLE_FETCH_ALWAYS);
                vi_set_dedither(true);
                vi_set_divot(true);
            } else {
                vi_set_aa_mode(VI_AA_MODE_RESAMPLE_FETCH_NEEDED);
                vi_set_dedither(false);
                vi_set_divot(true);
            }
            break;
    }

    float aspect_ratio = res.aspect_ratio ? res.aspect_ratio : 4.0f / 3.0f;
    vi_set_borders(vi_calc_borders(aspect_ratio, res.overscan_margin));

    /* Workaround for VI bug */
    vi_bug_workaround = (res.width == 320 && bit == DEPTH_16_BPP && filters == FILTERS_DISABLED);
}

#define RDRAM_BANK_SHIFT  20

static void allocate_surfaces_in_different_memory_banks( tex_format_t format )
{
    bool three_buffers = __num_buffers == 3;
    assertf(three_buffers || __num_buffers == 2, "Invalid num buffers: %d", __num_buffers);

    uint32_t width = __width;
    uint32_t height = __height;
    uint32_t last_available_bank = __boot_memsize >> RDRAM_BANK_SHIFT;

    surface_t first_surface;
    uint32_t first_bank = 0;

    surface_t second_surface;
    uint32_t second_bank = 0;

    surface_t third_surface;
    uint32_t third_bank = 0;

    surface_t last_surface;
    uint32_t last_bank = 0;

    #define MAX_INVALID_SURFACES 128

    surface_t invalid_surfaces[MAX_INVALID_SURFACES];
    uint32_t num_invalid_surfaces = 0;

    while (true)
    {
        surface_t surface = surface_alloc(format, width, height);

        if (surface.buffer) {
            phys_addr_t address = PhysicalAddr(surface.buffer);
            uint32_t bank = (address >> RDRAM_BANK_SHIFT) + 1;

            if (bank == last_available_bank) {
                /* Last bank is usually used by the Z-Buffer, let's avoid it if we can */
                if (! last_bank) {
                    last_surface = surface;
                    last_bank = bank;
                }
                else {
                    invalid_surfaces[num_invalid_surfaces] = surface;
                    ++num_invalid_surfaces;

                    if (num_invalid_surfaces == MAX_INVALID_SURFACES) {
                        break;
                    }
                }
            }
            else if (! first_bank) {
                first_surface = surface;
                first_bank = bank;
            }
            else if (! second_bank && bank != first_bank) {
                second_surface = surface;
                second_bank = bank;

                if (! three_buffers) {
                    break;
                }
            }
            else if (three_buffers && bank != first_bank && bank != second_bank) {
                third_surface = surface;
                third_bank = bank;
                break;
            }
            else {
                invalid_surfaces[num_invalid_surfaces] = surface;
                ++num_invalid_surfaces;

                if (num_invalid_surfaces == MAX_INVALID_SURFACES) {
                    break;
                }
            }
        }
        else {
            break;
        }
    }

    for (int i = 0; i < __num_buffers; i++)
    {
        if (first_bank) {
            surfaces[i] = first_surface;
            first_bank = 0;
        }
        else if (second_bank) {
            surfaces[i] = second_surface;
            second_bank = 0;
        }
        else if (third_bank) {
            surfaces[i] = third_surface;
            third_bank = 0;
        }
        else if (last_bank) {
            surfaces[i] = last_surface;
            last_bank = 0;
        }
        else {
            assertf(num_invalid_surfaces, "Out of memory");

            --num_invalid_surfaces;
            surfaces[i] = invalid_surfaces[num_invalid_surfaces];
        }
    }

    if (last_bank) {
        surface_free(&last_surface);
    }

    for (uint32_t i = 0; i < num_invalid_surfaces; i++)
    {
        surface_free(&invalid_surfaces[i]);
    }

    #undef MAX_INVALID_SURFACES
}

void display_init( resolution_t res, bitdepth_t bit, uint32_t num_buffers, gamma_t gamma, filter_options_t filters )
{
    assertf(__num_buffers == 0, "display_init() called while the display is already initialized.\nPlease close the current display with display_close() first.");

    /* Calculate width and scale registers */
    assertf(res.width > 0, "nonpositive width");
    assertf(res.height > 0, "nonpositive height");
    assertf(res.width <= 800, "invalid width");
    assertf(res.height <= 720, "heights > 720 are buggy on hardware");
    if (bit == DEPTH_16_BPP)
        assertf(res.width % 4 == 0, "width must be divisible by 4 for 16-bit depth");
    else if (bit == DEPTH_32_BPP)
        assertf(res.width % 2 == 0, "width must be divisible by 2 for 32-bit depth");

    vi_init();
    vi_write_begin();

    /* Reset VI configuration to default, before proceeding to configure the
       current display mode. */
    vi_reset();

    /* Minimum is at least one buffer. */
    uint32_t nb = MAX(1, MIN(NUM_BUFFERS, num_buffers));
    __num_buffers = (int)nb;
    __alloc_buffers = nb;

    /* Set up the display */
    __width = res.width;
    __height = res.height;
    __bitdepth = (bit == DEPTH_16_BPP) ? 2 : 4;
    __interlace_mode = res.interlaced;
    __alloc_width = res.width;
    __alloc_height = res.height;
    __alloc_bitdepth = __bitdepth;

    /* Set up pending config variables to affect display.h getters */
    pending_res = res;
    pending_bit = bit;
    pending_gamma = gamma;
    pending_filters = filters;
    pending_interlace_mode = res.interlaced;
    pending_vi_frames_left = -1;

    apply_display_vi_config(res, bit, gamma, filters);

    surfaces = malloc(sizeof(surface_t) * __num_buffers);
    assertf(surfaces, "Out of memory");

    /* Initialize buffers and set parameters */
    tex_format_t format = bit == DEPTH_16_BPP ? FMT_RGBA16 : FMT_RGBA32;
    uint32_t surface_length = __width * __height * __bitdepth;
    uint32_t bank_size = 1 << RDRAM_BANK_SHIFT;

    if ((__num_buffers == 2 || __num_buffers == 3) && surface_length <= bank_size) {
        allocate_surfaces_in_different_memory_banks( format );
    }
    else {
        for (int i = 0; i < __num_buffers; i++)
        {
            surfaces[i] = surface_alloc(format, __width, __height);
            assertf(surfaces[i].buffer, "Out of memory");
        }
    }

    for (int i = 0; i < __num_buffers; i++)
    {
        /* Baseline is blank */
        sys_hw_memset(surfaces[i].buffer, 0, surface_length);
    }

    /* Set the first buffer as the displaying buffer */
    now_showing = 0;
    drawing_mask = 0;
    ready_mask = 0;
    display_queue_head = 0;
    display_queue_tail = 0;
    display_queue_count = 0;
    vi_show(&surfaces[0]);
    if (vi_bug_workaround) {
        /* VI hits a rendering bug when HSTART < 128 && 16-bpp && X_SCALE <= 0x200,
           and resampling is disabled (see vi.c for this). HSTART < 128 is the
           default border configuration on NTSC. Since X_SCALE=0x200 means
           width=320 which happens to be the most common resolution, let's apply
           a simple workaround.
           A X_SCALE of 0x201 will behave exactly like 0x200 would if it worked,
           and introduce zero rendering artifacts (without resampling, that is). */
        vi_write(VI_X_SCALE, 0x201);
    }

    /* Calculate actual refresh rate for this configuration */
    refresh_rate = vi_get_refresh_rate();
    refresh_period = 1.0f / refresh_rate;
    frame_rate_snapshot = refresh_rate;
    display_set_fps_limit(0);
    kalman_init(&k_fps, 1.0f, 0.01f);
    kalman_init(&k_delta, 1.0f, 1.0f);

    vi_write_end();

    vi_install_vblank_handler(__display_callback, NULL);
    handler_installed = true;
}

void display_change(resolution_t res, bitdepth_t bit, uint32_t num_buffers, gamma_t gamma, filter_options_t filters)
{
    assertf(__alloc_buffers != 0, "display_change() called with display not initialized.");

    assertf(res.width > 0, "nonpositive width");
    assertf(res.height > 0, "nonpositive height");
    assertf(res.width <= 800, "invalid width");
    assertf(res.height <= 720, "heights > 720 are buggy on hardware");
    if (bit == DEPTH_16_BPP)
        assertf(res.width % 4 == 0, "width must be divisible by 4 for 16-bit depth");
    else if (bit == DEPTH_32_BPP)
        assertf(res.width % 2 == 0, "width must be divisible by 2 for 32-bit depth");

    assertf(num_buffers <= __alloc_buffers,
        "display_change() num_buffers (%u) exceeds originally allocated (%u).", (unsigned)num_buffers, (unsigned)__alloc_buffers);

    uint32_t new_bpp = (bit == DEPTH_16_BPP) ? 2 : 4;
    uint32_t new_size = (uint32_t)res.width * (uint32_t)res.height * new_bpp;
    uint32_t alloc_size = __alloc_width * __alloc_height * __alloc_bitdepth;
    assertf(new_size <= alloc_size,
        "display_change() framebuffer size (%ux%u %s) exceeds allocated size.", (unsigned)res.width, (unsigned)res.height,
        bit == DEPTH_16_BPP ? "16bpp" : "32bpp");

    uint32_t nb = MAX(1, MIN(NUM_BUFFERS, num_buffers));

    disable_interrupts();
    __num_buffers = (int)nb;
    pending_res = res;
    pending_bit = bit;
    pending_gamma = gamma;
    pending_filters = filters;
    pending_interlace_mode = res.interlaced;
    pending_vi_frames_left = display_queue_count;

    /* Update getters immediately so they return the new configuration. */
    __width = (uint32_t)res.width;
    __height = (uint32_t)res.height;
    __bitdepth = new_bpp;
    enable_interrupts();
}

void display_close()
{
    // Contrary to most other subsystems, for display we want to handle
    // correctly failed display_init() calls followed by
    // display_close(), because this is a common pattern during eg
    // exception handling. So we need to be a bit more careful than usual
    // to make sure to deinitialize piece-wise.

    if ( handler_installed )
    {
        vi_uninstall_vblank_handler(__display_callback, NULL);
        handler_installed = false;
    }

    now_showing = -1;
    drawing_mask = 0;
    ready_mask = 0;
    display_queue_head = 0;
    display_queue_tail = 0;
    display_queue_count = 0;
    pending_vi_frames_left = -1;

    if (surf_zbuf.buffer)
    {
        surface_free(&surf_zbuf);
        memset(&surf_zbuf_view, 0, sizeof(surf_zbuf_view));
        if (zbuf_sbrk_top) {
            sbrk_top(-(int)(__alloc_width * __alloc_height * 2));
            zbuf_sbrk_top = false;
        }
    }

    // Blank the image and wait until it actually happens, before
    // freeing the buffers.
    vi_show(NULL);
    vi_wait_vblank();

    if (surfaces)
    {
        for (uint32_t i = 0; i < __alloc_buffers; i++)
        {
            /* Free framebuffer memory */
            surface_free(&surfaces[i]);
        }
        free(surfaces);
        surfaces = NULL;
    }

    __width = 0;
    __height = 0;
    __num_buffers = 0;
    __alloc_width = 0;
    __alloc_height = 0;
    __alloc_bitdepth = 0;
    __alloc_buffers = 0;
}

surface_t* display_try_get(void)
{
    surface_t* retval = NULL;
    int next, start;

    assertf(__num_buffers != 0, "Display not initialized.");

    /* Can't have the video interrupt happening here */
    disable_interrupts();

    /* Calculate index of next display context to draw on. We need
       to find the first buffer which is not being drawn upon nor
       being ready to be displayed.

       Notice that the loop is always executed once, so it also works
       in the case of a single display buffer, though it at least
       wait for that buffer to be shown. */
    start = now_showing;
    if (start < 0 || start >= __num_buffers)
        start = __num_buffers - 1;

    next = buffer_next(start);
    do {
        if (((drawing_mask | ready_mask) & (1U << next)) == 0) {
            void *buf = surfaces[next].buffer;
            tex_format_t fmt = __bitdepth == 2 ? FMT_RGBA16 : FMT_RGBA32;
            uint16_t stride = (uint16_t)(__width * __bitdepth);
            surfaces[next] = surface_make(buf, fmt, (uint16_t)__width, (uint16_t)__height, stride);
            surfaces[next].flags |= SURFACE_FLAGS_OWNEDBUFFER;
            retval = &surfaces[next];
            drawing_mask |= 1U << next;
            assertf(display_queue_count < NUM_BUFFERS, "Display queue overflow");
            display_queue[display_queue_tail] = (uint8_t)next;
            display_queue_tail = (display_queue_tail + 1) % NUM_BUFFERS;
            display_queue_count++;
            break;
        }
        next = buffer_next(next);
    } while (next != start);

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

    assertf(__num_buffers != 0, "Display not initialized.");

    kirq_wait_t kirq = kirq_begin_wait_vi();
    ACCT_SCOPE(ACCT_CAT_DISPLAY) RSP_WAIT_LOOP(200) {
         if ((disp = display_try_get())) {
             break;
         }
         kirq_wait(&kirq);
    }
    return disp;
}

surface_t* display_get_zbuf(void)
{
    if (surf_zbuf.buffer == NULL) {
        /* Try to allocate the Z-Buffer from the top of the heap (near the stack).
           This basically puts it in the last memory bank, hopefully separating it
           from framebuffers, which provides a nice speed gain. */
        uint32_t alloc_size = __alloc_width * __alloc_height * 2;
        void *buf = sbrk_top(alloc_size);
        if (buf != (void*)-1) {
            data_cache_hit_invalidate(buf, alloc_size);
            surf_zbuf = surface_make(UncachedAddr(buf), FMT_RGBA16, (uint16_t)__alloc_width, (uint16_t)__alloc_height, (uint16_t)(__alloc_width * 2));
            zbuf_sbrk_top = true;
        } else {
            surf_zbuf = surface_alloc(FMT_RGBA16, (uint16_t)__alloc_width, (uint16_t)__alloc_height);
            zbuf_sbrk_top = false;
        }
    }
    surf_zbuf_view = surface_make(surf_zbuf.buffer, FMT_RGBA16, (uint16_t)__width, (uint16_t)__height, (uint16_t)(__width * 2));
    return &surf_zbuf_view;
}

void display_show( surface_t* surf )
{
    /* They tried drawing on a bad context */
    if (surf == NULL)
        return;

    /* Can't have the video interrupt screwing this up */
    disable_interrupts();

    /* Correct to ensure we are handling the right screen (any allocated buffer is valid). */
    int i = surf - surfaces;

    assertf(i >= 0 && i < (int)__alloc_buffers, "Display context is not valid!");

    /* Check we have not unlocked this display already and is pending drawn. */
    assertf(!(ready_mask & (1U << i)), "display_show called again on the same display %d (mask: %lx)", i, ready_mask);

    /* This should match, or something went awry */
    assertf(drawing_mask & (1U << i), "display_show called on non-locked display %d (mask: %lx)", i, drawing_mask);

    drawing_mask &= ~(1U << i);
    ready_mask |= 1U << i;

    enable_interrupts();
}

uint32_t display_get_width(void)
{
    return pending_res.width;
}

uint32_t display_get_height(void)
{
    return pending_res.height;
}

uint32_t display_get_bitdepth(void)
{
    return pending_bit == DEPTH_16_BPP ? 2 : 4;
}

uint32_t display_get_num_buffers(void)
{
    return (uint32_t)__num_buffers;
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
    assert(fps >= 0.0f);

    disable_interrupts();

    min_refresh_period = 1.0f / (fps ? MIN(fps, refresh_rate) : refresh_rate);
    frame_skip = refresh_period / min_refresh_period;
    delta_time = min_refresh_period;

    // Calculate also the minimum period using a rounded refresh rate
    // This will be used only for display purposes, so that FPS are capped
    // to 60 Hz rather than 59.83 Hz, which would be the hw-accurate value.
    min_refresh_period_rounded = 1.0f / (fps ? MIN(fps, roundf(refresh_rate)) : roundf(refresh_rate));

    fps_limit = fps;

    enable_interrupts();
}

surface_t display_get_current_framebuffer(void)
{
    return surface_make_linear(
        VirtualUncachedAddr(*VI_ORIGIN), 
        display_get_bitdepth() == 2 ? FMT_RGBA16 : FMT_RGBA32,
        display_get_width(),
        display_get_height());
}
