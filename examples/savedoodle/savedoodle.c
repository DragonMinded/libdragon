/**
 * @file savedoodle.c
 * @brief Save-backed doodle canvas
 *
 * A tiny "etch-a-sketch" that persists across power cycles using whatever save
 * chip the cartridge (or emulator/flashcart) provides. The same program is
 * built into one ROM per save type (see the Makefile); at runtime it probes the
 * hardware to discover which backend is actually present and drives it through
 * a uniform read/write interface.
 *
 * Detection is by hardware probing, not by reading the ROM header: the
 * N64_ROM_SAVETYPE build flag only tells emulators/flashcarts which backing
 * store to mount, it is not exposed to the running program. SRAM and FlashRAM
 * share the same PI window (0x08000000), so probe order matters to avoid one
 * probe corrupting the other's chip:
 *
 *   1. EEPROM   -- lives on the separate Joybus/SI bus; probing can't disturb
 *                  the PI save window, so it is always safe to check first.
 *   2. SRAM     -- sram_detect() writes one word and restores it; that write is
 *                  ignored by a FlashRAM chip in its power-on read mode, so this
 *                  probe is harmless to a FlashRAM cart.
 *   3. FlashRAM -- detection writes to the command register at
 *                  FLASHRAM_ADDRESS|0x10000, which on a real SRAM cart aliases
 *                  back into the 32 KiB SRAM window and corrupts it. So we only
 *                  probe FlashRAM once SRAM has been ruled out.
 *
 * Controls: D-Pad move cursor, hold A paint, hold B erase, L clear the canvas,
 * START save, Z reload the saved doodle.
 */

#include <libdragon.h>
#include <sram.h>  // not part of the <libdragon.h> umbrella header
#include <string.h>

/// Canvas dimensions in cells. 48*32 = 1536 cells stored 1 bit each = 192 bytes,
/// so the whole save fits even the smallest supported chip (EEPROM 4Kbit = 512 B).
#define CANVAS_W    48
#define CANVAS_H    32
#define CANVAS_BITS (CANVAS_W * CANVAS_H)
#define CANVAS_BYTES (CANVAS_BITS / 8)

#define STICK_DEADZONE 8       ///< Ignore small analog jitter near the stick center.
#define STICK_SPEED    0.004f  ///< Cells/frame per unit of stick deflection (-127..127).

#define SAVE_MAGIC  0x444F4F44u  ///< 'DOOD' -- marks a valid save in storage.

/// On-disk (and in-RAM) save layout. 200 bytes, even-sized so it satisfies
/// flashram_read()'s even-offset/length preference and fits every backend.
typedef struct
{
    uint32_t magic;                 ///< SAVE_MAGIC when the slot holds a doodle.
    uint32_t writes;                ///< How many times this doodle has been saved.
    uint8_t  pixels[CANVAS_BYTES];  ///< 1 bit per cell, row-major.
} save_t;

_Static_assert((sizeof(save_t) & 1) == 0, "save_t must be even-sized for flashram");

/// Which save backend we detected and are driving.
typedef enum
{
    BACKEND_NONE = 0,
    BACKEND_EEPROM,
    BACKEND_SRAM,
    BACKEND_FLASHRAM,
} backend_t;

static backend_t   g_backend = BACKEND_NONE;
static const char* g_backend_name = "none detected";
static size_t      g_capacity = 0;  ///< Detected usable capacity in bytes.

// --- Storage abstraction -----------------------------------------------------

/// Probe the hardware and record which save chip is present. See file header
/// for why the probe order matters.
static void storage_detect(void)
{
    eeprom_type_t et = eeprom_present();
    if (et != EEPROM_NONE)
    {
        g_backend = BACKEND_EEPROM;
        g_capacity = eeprom_total_blocks() * EEPROM_BLOCK_SIZE;
        g_backend_name = (et == EEPROM_16K) ? "EEPROM 16Kbit" : "EEPROM 4Kbit";
        return;
    }

    sram_init();
    int sz = sram_detect();
    if (sz > 0)
    {
        g_backend = BACKEND_SRAM;
        g_capacity = (size_t) sz;
        g_backend_name = "SRAM";
        return;
    }

    flashram_init();
    flashram_info_t fi;
    if (flashram_detect(&fi))
    {
        g_backend = BACKEND_FLASHRAM;
        g_capacity = fi.total_size;
        g_backend_name = fi.name;  // e.g. "MX29L1101_C" or "unknown"
        return;
    }

    g_backend = BACKEND_NONE;
}

/// Read @p len bytes at @p offset from the detected backend into @p dst.
static void storage_read(void* dst, size_t offset, size_t len)
{
    switch (g_backend)
    {
        case BACKEND_EEPROM:   eeprom_read_bytes(dst, offset, len); break;
        case BACKEND_SRAM:     sram_read(dst, offset, len); break;
        case BACKEND_FLASHRAM: flashram_read(dst, offset, len); break;
        default:               memset(dst, 0xFF, len); break;
    }
}

/// Write @p len bytes of @p src to @p offset on the detected backend, then block
/// until the data has actually reached the chip.
static void storage_write(const void* src, size_t offset, size_t len)
{
    switch (g_backend)
    {
        case BACKEND_EEPROM:
            eeprom_write_bytes(src, offset, len);
            eeprom_wait_idle();  // let the background flush finish before we claim "saved"
            break;
        case BACKEND_SRAM:     sram_write(src, offset, len); break;
        case BACKEND_FLASHRAM: flashram_write(src, offset, len); break;
        default:               break;
    }
}

// --- Canvas ------------------------------------------------------------------

/// 16-byte aligned so it is a valid PI-DMA source/destination for the SRAM and
/// FlashRAM backends.
static save_t g_save __attribute__((aligned(16)));
static bool   g_dirty = false;  ///< Canvas changed since the last save.

static inline bool cell_get(int x, int y)
{
    int i = y * CANVAS_W + x;
    return (g_save.pixels[i >> 3] >> (i & 7)) & 1;
}

static inline void cell_set(int x, int y, bool on)
{
    int i = y * CANVAS_W + x;
    uint8_t mask = 1 << (i & 7);
    if (on) g_save.pixels[i >> 3] |= mask;
    else    g_save.pixels[i >> 3] &= ~mask;
}

/// Load the saved doodle. Returns true if a valid save was found; otherwise the
/// canvas is left blank and initialised as a fresh (unwritten) save.
static bool canvas_load(void)
{
    storage_read(&g_save, 0, sizeof(g_save));
    if (g_save.magic != SAVE_MAGIC)
    {
        memset(&g_save, 0, sizeof(g_save));
        g_save.magic = SAVE_MAGIC;
        g_save.writes = 0;
        return false;
    }
    return true;
}

static void canvas_save(void)
{
    g_save.magic = SAVE_MAGIC;
    g_save.writes++;
    storage_write(&g_save, 0, sizeof(g_save));
    g_dirty = false;
}

// --- Rendering ---------------------------------------------------------------

#define CELL_PX 5                                  ///< On-screen size of one cell.
#define GRID_X  ((320 - CANVAS_W * CELL_PX) / 2)   ///< Centered horizontally.
#define GRID_Y  64                                 ///< Below the header text.
#define GRID_BOTTOM (GRID_Y + CANVAS_H * CELL_PX)

#define FONT_ID      1  ///< Our registered id for the builtin debug font.
#define STYLE_STATUS 1  ///< Highlighted style used for the status line.

#define COL_BG     RGBA32(20, 20, 32, 255)
#define COL_OFF    RGBA32(46, 46, 60, 255)
#define COL_ON     RGBA32(235, 235, 245, 255)
#define COL_CURSOR RGBA32(255, 210, 0, 255)

/// Draw a filled rectangle in the current fill color from (x,y) sized w*h.
static inline void fill_box(int x, int y, int w, int h)
{
    rdpq_fill_rectangle(x, y, x + w, y + h);
}

static void render(int cx, int cy, const char* status)
{
    surface_t* disp = display_get();
    rdpq_attach(disp, NULL);

    // Background and canvas backdrop (RDP fill mode -- one solid color at a time).
    rdpq_set_mode_fill(COL_BG);
    fill_box(0, 0, 320, 240);
    rdpq_set_fill_color(COL_OFF);
    fill_box(GRID_X, GRID_Y, CANVAS_W * CELL_PX, CANVAS_H * CELL_PX);

    // Lit cells, inset by 1px so the backdrop shows as a subtle grid gutter.
    rdpq_set_fill_color(COL_ON);
    for (int y = 0; y < CANVAS_H; y++)
    {
        for (int x = 0; x < CANVAS_W; x++)
        {
            if (cell_get(x, y))
                fill_box(GRID_X + x * CELL_PX, GRID_Y + y * CELL_PX, CELL_PX - 1, CELL_PX - 1);
        }
    }

    // Cursor: a cell-sized box in the cursor color with the cell state inset.
    int cpx = GRID_X + cx * CELL_PX;
    int cpy = GRID_Y + cy * CELL_PX;
    rdpq_set_fill_color(COL_CURSOR);
    fill_box(cpx, cpy, CELL_PX - 1, CELL_PX - 1);
    rdpq_set_fill_color(cell_get(cx, cy) ? COL_ON : COL_OFF);
    fill_box(cpx + 1, cpy + 1, CELL_PX - 3, CELL_PX - 3);

    // Text (rdpq_text sets up its own render mode).
    char cap[24];
    if (g_capacity >= 1024)
        sprintf(cap, "%u KiB", (unsigned)(g_capacity / 1024));
    else
        sprintf(cap, "%u bytes", (unsigned) g_capacity);

    rdpq_text_printf(NULL, FONT_ID, 16, 16, "Backend: %s  (%s)", g_backend_name, cap);
    rdpq_text_printf(NULL, FONT_ID, 16, 28, "Saves: %u   Save size: %u B   %s",
                     (unsigned) g_save.writes, (unsigned) sizeof(save_t),
                     g_dirty ? "[unsaved]" : "[saved]");
    rdpq_text_print(NULL, FONT_ID, 16, 40, "D-Pad/Stick move   A paint   B erase");
    rdpq_text_print(NULL, FONT_ID, 16, 52, "L clear   START save   Z reload");

    if (status)
        rdpq_text_printf(&(rdpq_textparms_t){ .style_id = STYLE_STATUS },
                         FONT_ID, 16, GRID_BOTTOM + 12, "%s", status);

    rdpq_detach_show();
}

// --- Main --------------------------------------------------------------------

int main(void)
{
    debug_init_isviewer();
    debug_init_usblog();

    timer_init();       // EEPROM's background flush is timer-driven.
    joypad_init();
    display_init(RESOLUTION_320x240, DEPTH_32_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();

    // Register the builtin debug font, with a highlighted style for the status line.
    rdpq_font_t* font = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO);
    rdpq_font_style(font, STYLE_STATUS, &(rdpq_fontstyle_t){ .color = COL_CURSOR });
    rdpq_text_register_font(FONT_ID, font);

    storage_detect();

    const char* status;
    if (g_backend == BACKEND_NONE)
        status = "No save chip detected!";
    else if (canvas_load())
        status = "Loaded your saved doodle.";
    else
        status = "Blank canvas - draw and press START.";

    // Cursor position is kept in fractional cells so the analog stick can move
    // it with sub-cell precision; cx/cy are the resolved active cell.
    float fx = CANVAS_W / 2;
    float fy = CANVAS_H / 2;
    int   cx = (int) fx;
    int   cy = (int) fy;
    int   move_delay = 0;

    while (1)
    {
        joypad_poll();
        joypad_buttons_t pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
        joypad_buttons_t held    = joypad_get_buttons_held(JOYPAD_PORT_1);
        joypad_inputs_t  in      = joypad_get_inputs(JOYPAD_PORT_1);

        // D-Pad: discrete single-cell steps, repeating while held.
        if (move_delay > 0)
            move_delay--;
        if (move_delay == 0)
        {
            bool moved = true;
            if      (held.d_up)    fy -= 1.0f;
            else if (held.d_down)  fy += 1.0f;
            else if (held.d_left)  fx -= 1.0f;
            else if (held.d_right) fx += 1.0f;
            else                   moved = false;
            if (moved)
                move_delay = 4;
        }

        // Analog stick: continuous movement whose speed tracks deflection, so
        // small nudges give fine, precise control near the stick's center.
        if (in.stick_x > STICK_DEADZONE || in.stick_x < -STICK_DEADZONE)
            fx += in.stick_x * STICK_SPEED;
        if (in.stick_y > STICK_DEADZONE || in.stick_y < -STICK_DEADZONE)
            fy -= in.stick_y * STICK_SPEED;  // stick up (+y) moves toward the top row

        // Clamp to the canvas and resolve the active cell.
        if (fx < 0.0f) fx = 0.0f; else if (fx > CANVAS_W - 1) fx = CANVAS_W - 1;
        if (fy < 0.0f) fy = 0.0f; else if (fy > CANVAS_H - 1) fy = CANVAS_H - 1;
        cx = (int) (fx + 0.5f);
        cy = (int) (fy + 0.5f);

        // Painting: hold A to draw, B to erase the cell under the cursor.
        if (g_backend != BACKEND_NONE)
        {
            if (held.a && !cell_get(cx, cy)) { cell_set(cx, cy, true);  g_dirty = true; }
            if (held.b &&  cell_get(cx, cy)) { cell_set(cx, cy, false); g_dirty = true; }

            if (pressed.l)
            {
                memset(g_save.pixels, 0, sizeof(g_save.pixels));
                g_dirty = true;
                status = "Canvas cleared (not yet saved).";
            }
            if (pressed.start)
            {
                canvas_save();
                status = "Saved! Power-cycle to prove it persists.";
            }
            if (pressed.z)
            {
                status = canvas_load() ? "Reloaded the saved doodle."
                                       : "Nothing saved yet.";
                g_dirty = false;
            }
        }

        render(cx, cy, status);
    }
}
