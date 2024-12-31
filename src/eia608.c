/**
 * @file eia608.c
 * @brief Library to generate NTSC EIA-608 closed captions
 * @ingroup display
 */

#include "eia608.h"
#include "surface.h"
#include "vi.h"
#include "vi_internal.h"
#include "n64sys.h"
#include "n64types.h"
#include "debug.h"
#include "utils.h"
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/**
 * @brief Activate debugging mode for EIA608 signal
 * 
 * This macro will do a few things:
 * - Allow PAL
 * - Stick the EIA608 line to the top of the output window
 * - Make the EAI608 line 8 lines big so that it's more visible
 * 
 * This would allow to visually debug the implementation by configuring large
 * borders, so that EIA608 becomes visible. Obviously it will likely break
 * *actual* captions.
 */
#define DEBUG_EIA608            0

#define EIA608_IRQ_SAFEMARGIN   4     ///< How many halflines to trigger the IRQ before the actual line

#define SIG_ON      0x8421            ///< IRE 50 (50% intensity RGBA16)
#define SIG_OFF     0x0000            ///< IRE 0

#define SIGW_SCALE    4               ///< Scale factor for the signal wrt linebuffer pixels
#define SIGW_BLANK    7               ///< Blank before/after the actual eia608 signal
#define SIGW_LEADIN   19              ///< From spec: 19 bits of clock leadin
#define SIGW_BIT      2               ///< From spec: Encode each bit in 2 pixels, to make it take twice the time of the leadin (per bit)
#define SIGW_DATA     (16*SIGW_BIT)   ///< From spec: 16 bits of data for each packet
#define SIGW_LEN      (SIGW_BLANK + (SIGW_LEADIN + SIGW_DATA) * SIGW_SCALE + SIGW_BLANK)   ///< Total length of EIA608 signal in pixels

#define RING_BUFFER_SIZE    512       ///< Size of the ring buffer for EIA608 data

static uint16_t *ring_buffer;
static volatile int rb_wpos, rb_rpos;
static surface_t linebuffer;
static int force_clear_timer;
static int irq_errors;

static struct {
    bool interlaced;                        ///< Whether the parameters refer to interlaced mode or not
    uint32_t reg_ctrl;                      ///< Value to set for VI_CTRL
    uint32_t reg_h_video;                   ///< Value to set for VI_H_VIDEO
    uint32_t reg_xscale;                    ///< Value to set for VI_X_SCALE
    int v_line_start;                       ///< Lines to draw
    int v_line_end;                         ///< Lines to draw
    int out_x0, out_y0, out_x1, out_y1;     ///< Previous output window
} sigparms;

static uint16_t* linebuffer_write_bits(uint16_t* buffer, bool on, int nbits)
{
    uint16_t val16 = on ? SIG_ON : SIG_OFF;
    uint32_t val32 = (val16 << 16) | val16;
    uint64_t val64 = (uint64_t)val32 << 32 | val32;
    int size = SIGW_SCALE * nbits;
    while (size >= 8) {
        *(u_uint64_t*)buffer = val64;
        buffer += 8;
        size -= 8;
    }
    if (size >= 4) {
        *(u_uint32_t*)buffer = val32;
        buffer += 4;
        size -= 4;
    }
    if (size >= 2) {
        *(uint16_t*)buffer = val16;
        buffer += 2;
        size -= 2;
    }
    return buffer;
}

static void linebuffer_init(surface_t *lb)
{
    uint16_t *buffer = lb->buffer;

    for (int i=0; i<SIGW_BLANK; i++)
        *buffer++ = SIG_OFF;
    
    int clock = 0x61555;
    for (int i=0; i<SIGW_LEADIN; i++) {
        buffer = linebuffer_write_bits(buffer, clock & 1, 1);
        clock >>= 1;
    }

    buffer += SIGW_DATA * SIGW_SCALE;   // skip the data part

    for (int i=0; i<SIGW_BLANK; i++)
        *buffer++ = SIG_OFF;
}

static void linebuffer_write(surface_t *lb, uint16_t data)
{
    uint16_t *buffer = lb->buffer;
    buffer += SIGW_BLANK;                // skip the blank part
    buffer += SIGW_LEADIN * SIGW_SCALE;  // skip the leadin part
    data = (data << 8) | (data >> 8);
    for (int i=0; i<16; i++) {
        buffer = linebuffer_write_bits(buffer, data & 1, SIGW_BIT);
        data >>= 1;
    }
}

static void recalc_parms(void)
{
    const float eia_databit_us = 1.986f;       // from spec: duration of a data bit in us
    const float ntsc_pixel_us = 0.082166f;     // fixed for N64 NTSC
    float xscale = (SIGW_BIT * SIGW_SCALE) / (eia_databit_us / ntsc_pixel_us);
    uint32_t xscale_fx = (uint32_t)(xscale * 1024.0f + 0.5f);

    uint32_t ctrl = 0;
    ctrl |= !sys_bbplayer() ? VI_PIXEL_ADVANCE_DEFAULT : VI_PIXEL_ADVANCE_BBPLAYER;
    ctrl |= VI_CTRL_TYPE_16_BPP;   // 16-bit color
    ctrl |= VI_AA_MODE_RESAMPLE;   // resample AA mode
    ctrl |= vi_read(VI_CTRL) & VI_CTRL_SERRATE;   // keep interlace mode if active

    bool interlaced = ctrl & VI_CTRL_SERRATE;

    // Calculate vertical line on which to draw. The spec says to draw on
    // NTSC line 21. NTSC counts from the start of the vsync pulse, while
    // the N64 starts counting after it. Moreover, in interlace mode, we want
    const int LINE_21 = 21;                    // from spec: draw on NTSC line 21
    const int vsync_height = vi_read(VI_BURST) >> 16 & 0xF;
    const int v_line = LINE_21 - vsync_height;
    int v_halfline = v_line * 2 + 1;
    if (!interlaced) v_halfline++;

    const int h_start = 96;                                       // earliest possible start of active video on NTSC
    const int h_end = h_start + 640;                              // standard 640 pixel output width
    const float clock_target_us = 10.5f;                          // EIA-608-B spec: clock lead in starts at 10.5 us after hsync
    const float clock_target = clock_target_us / ntsc_pixel_us;   // clock target in NTSC dots
    float blank = (clock_target - h_start) * xscale;      // framebuffer pixel of blank before clock leadin
    blank -= 3;             // compensate for VI bug that delays the start by 3 pixels
    assert(blank >= 7);     // VI blanks the first 7 pixels, so we need at least that much
                            // If this triggers, try playing with h_start or SIGW_SCALE
    // We now need to make sure we have "blank" fractional pixels of blank before the clock leadin
    // The fraction part can be adjusted using VI's XOFFSET. For the integer part,
    // we could just use it as SIGW_BLANK, but instead we keep SIGW_BLANK hardcoded,
    // and just adjust xoffset accordingly.
    float xoffset = blank - SIGW_BLANK;     // adjustement required on top of SIGW_BLANK
    assert(xoffset >= 0 && xoffset < 3);    // xoffset is max 3; if this triggers, change SIGW_BLANK
    uint32_t xoffset_fx = (uint32_t)(xoffset * 1024.0f + 0.5f);

    sigparms.interlaced = interlaced;
    sigparms.reg_ctrl = ctrl;
    sigparms.reg_h_video = (h_start << 16) | h_end;
    sigparms.reg_xscale = (xoffset_fx << 16) | xscale_fx;
    sigparms.v_line_start = v_halfline;
    sigparms.v_line_end = v_halfline + (interlaced ? 2 : 1);

    // For now, we only support default NTSC output where the real output
    // begins on line 23, just one line after the EIA-608 signal.
    vi_get_output(&sigparms.out_x0, &sigparms.out_y0, &sigparms.out_x1, &sigparms.out_y1);
    #if DEBUG_EIA608
    sigparms.v_line_start = sigparms.out_y0 - 16;
    sigparms.v_line_end = sigparms.out_y0;
    #endif
    assertf(sigparms.v_line_end == sigparms.out_y0,
        "EIA-608: unimplemented support for borders");
}

static void __eia608_interrupt(void)
{
    static int framecounter = 0; ++framecounter;

    // We need to update the linebuffer at 30 Hz, so every
    // other frame. Even if the display is interlaced, it doesn't
    // really matter *which* field we do the update, as we emit
    // the signal on both anyway.
    if (framecounter & 1) {
        // Fetch the data from the ring buffer (or the NOP filler if empty)
        uint16_t data;

        if (force_clear_timer && --force_clear_timer < 2) {
            data = EIA608_CC1_EDM;
        } else if (rb_rpos == rb_wpos) {
            data = EIA608_NOP;
        } else {
            data = ring_buffer[rb_rpos];
            rb_rpos = (rb_rpos + 1) % RING_BUFFER_SIZE;
        }

        // Draw the data into the linebuffer
        linebuffer_write(&linebuffer, data);
    }

    // The VI should still be drawing the line *before* the one that we want
    // to draw on. If we're not there, it means that the interrupt was late,
    // so we'll just skip this frame.
    if ((*VI_V_CURRENT|1) >= (sigparms.v_line_start|1)-2) {
        #if DEBUG_EIA608
        debugf("EIA-608: IRQ error %ld %d\n", *VI_V_CURRENT, sigparms.v_line_start);
        #endif
        ++irq_errors;
        return;
    }

    // Fetch the current VI configuration
    uint32_t old_origin = *VI_ORIGIN;
    uint32_t old_ctrl = *VI_CTRL;
    uint32_t old_h_video = *VI_H_VIDEO;
    uint32_t old_x_scale = *VI_X_SCALE;
    uint32_t old_y_scale = *VI_Y_SCALE;

    uint32_t new_origin = (uint32_t)(linebuffer.buffer);
    uint32_t new_ctrl = sigparms.reg_ctrl;
    uint32_t new_h_video = sigparms.reg_h_video;
    uint32_t new_x_scale = sigparms.reg_xscale;
    uint32_t new_y_scale = 0; // FIXME: this seems to revert the interlacing fix?

    // Wait for the line to match the one we're expecting
    while ((*VI_V_CURRENT|1) < (sigparms.v_line_start|1)-2) {}
    
    // We are now in hblank. Quickly switch VI configuration to draw the
    // linebuffer. Notice that we don't need to set VI_WIDTH because Y scale is 0 anyway.
    *VI_ORIGIN = new_origin;
    *VI_CTRL = new_ctrl;
    *VI_H_VIDEO = new_h_video;
    *VI_X_SCALE = new_x_scale;
    *VI_Y_SCALE = new_y_scale;
    MEMORY_BARRIER();

    // Wait for the EIA-608 signal to be displayed
    // Displaying one line takes less than 60us, so we realistically don't
    // have time to do much else, and it would be a waste to retrigger another
    // interrupt.
    while ((*VI_V_CURRENT|1) < (sigparms.v_line_end|1)) {}

    // Restore the original VI configuration
    *VI_ORIGIN = old_origin;
    *VI_CTRL = old_ctrl;
    *VI_H_VIDEO = old_h_video;
    *VI_X_SCALE = old_x_scale;
    *VI_Y_SCALE = old_y_scale;
    MEMORY_BARRIER();
}

void eia608_init(void)
{
    #if !DEBUG_EIA608
    assertf(get_tv_type() == TV_NTSC, "EIA-608 is only supported on NTSC");
    #endif
    assert(ring_buffer == NULL);
    ring_buffer = malloc(RING_BUFFER_SIZE * 2);
    rb_rpos = rb_wpos = 0;

    // Allocate and initialize the linebuffer
    linebuffer = surface_alloc(FMT_RGBA16, SIGW_LEN, 1);
    linebuffer_init(&linebuffer);
}

void eia608_close(void)
{
    if (ring_buffer) free(ring_buffer);
    ring_buffer = NULL;
    surface_free(&linebuffer);
}

void eia608_start(void)
{
    recalc_parms();

    // Enqueue one second of NOP fillers, so that we give time to the TV
    // to stabilize before we start sending the actual data
    rb_rpos = rb_wpos = 0;
    for (int i=0; i<30; i++)
        ring_buffer[rb_wpos++] = EIA608_NOP;

    vi_write_begin();
        // Enable the line interrupt
        vi_set_line_interrupt(sigparms.v_line_start-EIA608_IRQ_SAFEMARGIN, __eia608_interrupt);
        // Increase the output window to include the EIA-608 signal
        vi_set_output(sigparms.out_x0, sigparms.v_line_start, sigparms.out_x1, sigparms.out_y1);
    vi_write_end();
}

void eia608_stop(void)
{
    vi_write_begin();
        vi_set_output(sigparms.out_x0, sigparms.out_y0, sigparms.out_x1, sigparms.out_y1);
        vi_set_line_interrupt(sigparms.v_line_start-EIA608_IRQ_SAFEMARGIN, NULL);
    vi_write_end();
}

static uint8_t odd_parity(uint8_t value)
{
    uint8_t v = value;
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return value | ((v ^ 1) << 7);
}

bool eia608_write_raw(uint16_t data, bool calc_parity)
{
    debugf("EIA-608: %04X\n", data);
    int next = (rb_wpos + 1) % RING_BUFFER_SIZE;
    if (next == rb_rpos) 
        return false;

    if (calc_parity)
        data = odd_parity(data) | (odd_parity(data >> 8) << 8);

    ring_buffer[rb_wpos] = data;
    rb_wpos = next;
    return true;
}

void eia608_write_ctrl_raw(eia608_ctrl_t ctrl)
{
    // EIA-608 control codes are 16-bit values, but the spec suggests to always
    // send them twice for reliability.
    eia608_write_raw(ctrl, false);
    eia608_write_raw(ctrl, false);
}

static void eia608_write_indent(eia608_channel_t cc, int row, int indent, bool underline)
{
    assert(cc == EIA608_CC1 || cc == EIA608_CC2);
    assert(row >= 1 && row <= 15);
    assert(indent >= 0 && indent <= 31);
    static const uint16_t pac_code[15] = {
        0x1150, 0x1170, 0x1250, 0x1270, 0x1550, 0x1570, 0x1650, 0x1670, 
        0x1750, 0x1770, 0x1050, 0x1350, 0x1370, 0x1450, 0x1470
    };

    uint16_t data = pac_code[row-1];
    data |= (indent / 4) << 1;
    if (underline) data |= 0x1;

    eia608_write_raw(data, true);
    eia608_write_raw(data, true);
}

/**
 * @brief Encode a UTF8-character from a string into EIA-608 format
 * 
 * This function takes a pointer to a UTF8 string, and encodes the next character
 * into EIA-608 format. The pointer is updated to point to the next character in
 * the string.
 * 
 * The EIA-608 format is a 32-bit value as follows:
 *  * 0 means that the character is not supported by EIA-608
 *  * 1-byte values are standard 1-byte EIA-608 characters
 *  * 2-byte values are characters from the North American set, that must be
 *    transmitted in a single packet.
 *  * 3-byte values are extended characters made by 1 byte value (MSB) which is
 *    the alternate representation (eg: à => a), and a 2-byte value which must
 *    be transmitted in a whole packet. Notice that the semantic of the 2-value
 *    character also encodes a backspace, so the single-byte alternative value
 *    must always be transmitted.
 */
static uint32_t eia608_encode_char(const char **utf8_str)
{
    uint8_t ch0 = *(*utf8_str)++;
    if (ch0 < 0x80) {
        // Map unicode codepoints into EIA-608 equivalents
        switch (ch0) {
        case 0x27: return 0x1229 | '\''<<16;   // APOSTROPHE -> RIGHT_SINGLE_QUOTATION_MARK
        case 0x2A: return 0x1228 | '#'<<16;   // ASTERISK
        case 0x5C: return 0x132B | '/'<<16;   // REVERSE SOLIDUS
        case 0x5E: return 0x132C | '/'<<16;   // CIRCUMFLEX ACCENT
        case 0x5F: return 0x132D | '-'<<16;   // LOW LINE
        case 0x60: return 0x1226 | '\''<<16;  // GRAVE ACCENT
        case 0x7B: return 0x1329 | '['<<16;   // LEFT CURLY BRACKET
        case 0x7C: return 0x132E | '-'<<16;   // VERTICAL LINE
        case 0x7D: return 0x132A | ']'<<16;   // RIGHT CURLY BRACKET
        case 0x7E: return 0x132F | '-'<<16;   // TILDE
        default:   return ch0;      // ASCII default
        }
    }
    if (ch0 == 0xC2) switch ((uint8_t)*(*utf8_str)++) {
        // Special North American character set
        case 0xAE: return 0x1130;             // REGISTERED SIGN
        case 0xB0: return 0x1131;             // DEGREE SIGN
        case 0xBD: return 0x1132;             // VULGAR_FRACTION_ONE_HALF
        case 0xBF: return 0x1133;             // INVERTED_QUESTION_MARK
        case 0xA2: return 0x1135;             // CENT SIGN
        case 0xA3: return 0x1136;             // POUND SIGN
        // Extended Spanish/Miscellaneous
        case 0xA1: return 0x1227 | '!'<<16;   // INVERTED_EXCLAMATION_MARK
        case 0xA9: return 0x122B | 'c'<<16;   // COPYRIGHT SIGN
        // Extended French
        case 0xAB: return 0x123E | '<'<<16;   // LEFT_POINTING_DOUBLE_ANGLE_QUOTATION_MARK
        case 0xBB: return 0x123F | '>'<<16;   // RIGHT_POINTING_DOUBLE_ANGLE_QUOTATION_MARK
        // German/Danish
        case 0xA5: return 0x1335 | 'Y'<<16;   // YEN SIGN
        case 0xA4: return 0x1336 | 'C'<<16;   // CURRENCY SIGN
        case 0xA6: return 0x1337 | '|'<<16;   // BROKEN BAR
        default:   return 0;
    }
    if (ch0 == 0xC3) switch ((uint8_t)*(*utf8_str)++) {
        // ASCII exceptions
        case 0xA1: return 0x2A;               // LATIN_SMALL_LETTER_A_WITH_ACUTE
        case 0xA9: return 0x5C;               // LATIN_SMALL_LETTER_E_WITH_ACUTE
        case 0xAD: return 0x5E;               // LATIN_SMALL_LETTER_I_WITH_ACUTE
        case 0xB3: return 0x5F;               // LATIN_SMALL_LETTER_O_WITH_ACUTE
        case 0xBA: return 0x60;               // LATIN_SMALL_LETTER_U_WITH_ACUTE
        case 0xA7: return 0x7B;               // LATIN_SMALL_LETTER_C_WITH_CEDILLA
        case 0xB7: return 0x7C;               // DIVISION_SIGN
        case 0x91: return 0x7D;               // LATIN_CAPITAL_LETTER_N_WITH_TILDE
        case 0xB1: return 0x7E;               // LATIN_SMALL_LETTER_N_WITH_TILDE
        // Special North American character set
        case 0xA0: return 0x1138;             // LATIN_SMALL_LETTER_A_WITH_GRAVE
        case 0xA8: return 0x113A;             // LATIN_SMALL_LETTER_E_WITH_GRAVE
        case 0xA2: return 0x113B;             // LATIN_SMALL_LETTER_A_WITH_CIRCUMFLEX
        case 0xAA: return 0x113C;             // LATIN_SMALL_LETTER_E_WITH_CIRCUMFLEX
        case 0xAE: return 0x113D;             // LATIN_SMALL_LETTER_I_WITH_CIRCUMFLEX
        case 0xB4: return 0x113E;             // LATIN_SMALL_LETTER_O_WITH_CIRCUMFLEX
        case 0xBB: return 0x113F;             // LATIN_SMALL_LETTER_U_WITH_CIRCUMFLEX
        // Extended Spanish/Miscellaneous characters
        case 0x81: return 0x1220 | 'A'<<16;   // LATIN_CAPITAL_LETTER_A_WITH_ACUTE
        case 0x89: return 0x1221 | 'E'<<16;   // LATIN_CAPITAL_LETTER_E_WITH_ACUTE
        case 0x93: return 0x1222 | 'O'<<16;   // LATIN_CAPITAL_LETTER_O_WITH_ACUTE
        case 0x9A: return 0x1223 | 'U'<<16;   // LATIN_CAPITAL_LETTER_U_WITH_ACUTE
        case 0x9C: return 0x1224 | 'U'<<16;   // LATIN_CAPITAL_LETTER_U_WITH_DIAERESIS
        case 0xBC: return 0x1225 | 'u'<<16;   // LATIN_SMALL_LETTER_U_WITH_DIAERESIS
        // Extended French
        case 0x80: return 0x1230 | 'A'<<16;   // LATIN_CAPITAL_LETTER_A_WITH_GRAVE
        case 0x82: return 0x1231 | 'A'<<16;   // LATIN_CAPITAL_LETTER_A_WITH_CIRCUMFLEX
        case 0x87: return 0x1232 | 'C'<<16;   // LATIN_CAPITAL_LETTER_C_WITH_CEDILLA
        case 0x88: return 0x1233 | 'E'<<16;   // LATIN_CAPITAL_LETTER_E_WITH_GRAVE
        case 0x8A: return 0x1234 | 'E'<<16;   // LATIN_CAPITAL_LETTER_E_WITH_CIRCUMFLEX
        case 0x8B: return 0x1235 | 'E'<<16;   // LATIN_CAPITAL_LETTER_E_WITH_DIAERESIS
        case 0xAB: return 0x1236 | 'e'<<16;   // LATIN_SMALL_LETTER_E_WITH_DIAERESIS
        case 0x8E: return 0x1237 | 'I'<<16;   // LATIN_CAPITAL_LETTER_I_WITH_CIRCUMFLEX
        case 0x8F: return 0x1238 | 'I'<<16;   // LATIN_CAPITAL_LETTER_I_WITH_DIAERESIS
        case 0xAF: return 0x1239 | 'i'<<16;   // LATIN_SMALL_LETTER_I_WITH_DIAERESIS
        case 0x94: return 0x123A | 'O'<<16;   // LATIN_CAPITAL_LETTER_O_WITH_CIRCUMFLEX
        case 0x99: return 0x123B | 'U'<<16;   // LATIN_CAPITAL_LETTER_U_WITH_GRAVE
        case 0xB9: return 0x123C | 'u'<<16;   // LATIN_SMALL_LETTER_U_WITH_GRAVE
        case 0x9B: return 0x123D | 'U'<<16;   // LATIN_CAPITAL_LETTER_U_WITH_CIRCUMFLEX
        // Portuguese
        case 0x83: return 0x1320 | 'A'<<16;   // LATIN_CAPITAL_LETTER_A_WITH_TILDE
        case 0xA3: return 0x1321 | 'a'<<16;   // LATIN_SMALL_LETTER_A_WITH_TILDE
        case 0x8D: return 0x1322 | 'I'<<16;   // LATIN_CAPITAL_LETTER_I_WITH_ACUTE
        case 0x8C: return 0x1323 | 'I'<<16;   // LATIN_CAPITAL_LETTER_I_WITH_GRAVE
        case 0xAC: return 0x1324 | 'i'<<16;   // LATIN_SMALL_LETTER_I_WITH_GRAVE
        case 0x92: return 0x1325 | 'O'<<16;   // LATIN_CAPITAL_LETTER_O_WITH_GRAVE
        case 0xB2: return 0x1326 | 'o'<<16;   // LATIN_SMALL_LETTER_O_WITH_GRAVE
        case 0x95: return 0x1327 | 'O'<<16;   // LATIN_CAPITAL_LETTER_O_WITH_TILDE
        case 0xB5: return 0x1328 | 'o'<<16;   // LATIN_SMALL_LETTER_O_WITH_TILDE
        // German/Danish
        case 0x84: return 0x1330 | 'A'<<16;   // LATIN_CAPITAL_LETTER_A_WITH_DIAERESIS
        case 0xA4: return 0x1331 | 'a'<<16;   // LATIN_SMALL_LETTER_A_WITH_DIAERESIS
        case 0x96: return 0x1332 | 'O'<<16;   // LATIN_CAPITAL_LETTER_O_WITH_DIAERESIS
        case 0xB6: return 0x1333 | 'o'<<16;   // LATIN_SMALL_LETTER_O_WITH_DIAERESIS
        case 0x9F: return 0x1334 | 'S'<<16;   // LATIN_CAPITAL_LETTER_SHARP_S
        case 0x85: return 0x1338 | 'A'<<16;   // LATIN_CAPITAL_LETTER_A_WITH_RING_ABOVE
        case 0xA5: return 0x1339 | 'a'<<16;   // LATIN_SMALL_LETTER_A_WITH_RING_ABOVE
        case 0x98: return 0x133A | 'O'<<16;   // LATIN_CAPITAL_LETTER_O_WITH_STROKE
        case 0xB8: return 0x133B | 'o'<<16;   // LATIN_SMALL_LETTER_O_WITH_STROKE
        default:   return 0;
    }
    if (ch0 == 0xE2) {
        uint16_t ch1 = *(*utf8_str)++ << 8; ch1 |= *(*utf8_str)++;
        switch (ch1) {
            // ASCII exceptions
            case 0x9688: return 0x7F;               // FULL_BLOCK
            // Special North American character set
            case 0x84A2: return 0x1134;             // TRADE_MARK_SIGN
            case 0x99AA: return 0x1137;             // EIGHTH_NOTE
            // Extended Spanish/Miscellaneous
            case 0x8098: return 0x1226 | '\''<<16;  // LEFT_SINGLE_QUOTATION_MARK
            case 0x8099: return 0x27;               // RIGHT_SINGLE_QUOTATION_MARK -> APOSTROPHE
            case 0x8094: return 0x122A | '-'<<16;   // EM_DASH
            case 0x84A0: return 0x122C | 's'<<16;   // SERVICE_MARK
            case 0x80A2: return 0x122D | '.'<<16;   // BULLET
            case 0x809C: return 0x122E | '"'<<16;   // LEFT_DOUBLE_QUOTATION_MARK
            case 0x809D: return 0x122F | '"'<<16;   // RIGHT_DOUBLE_QUOTATION_MARK
            // German/Danish
            case 0x948C: return 0x133C | '+'<<16;   // BOX_DRAWINGS_LIGHT_DOWN_AND_RIGHT
            case 0x9490: return 0x133D | '+'<<16;   // BOX_DRAWINGS_LIGHT_DOWN_AND_LEFT
            case 0x9494: return 0x133E | '+'<<16;   // BOX_DRAWINGS_LIGHT_UP_AND_RIGHT
            case 0x9498: return 0x133F | '+'<<16;   // BOX_DRAWINGS_LIGHT_UP_AND_LEFT
            default:     return 0;
        }
    }
    if (ch0 < 0xE0) { utf8_str+=1; return 0; }
    if (ch0 < 0xF0) { utf8_str+=2; return 0; }
    if (ch0 < 0xF8) { utf8_str+=3; return 0; }
    return 0;
}

void eia608_caption_prepare(eia608_channel_t cc, const char *utf8_str, eia608_captionparms_t *parms)
{
    // EIA-608 POP-ON mode can only hold up to three lines of 32 characters each
    enum { MAX_ROWS = 4 };

    eia608_captionparms_t _default = {0};
    if (parms == NULL) parms = &_default;

    int first_row = parms->row ? parms->row : 11;

    uint32_t buffer[32*MAX_ROWS+8];
    int buf_len = 0;

    // Emit start caption command (in pop-on mode)
    eia608_write_ctrl_raw(cc == EIA608_CC1 ? EIA608_CC1_RCL : EIA608_CC2_RCL);

    // Convert the input string from UTF-8 into EIA-608 16-bit characters.
    for (int i = 0; i < sizeof(buffer)/2; i++) {
        if (*utf8_str == 0) break;
        uint32_t ch = eia608_encode_char(&utf8_str);
        if (ch > 0x100 && cc == EIA608_CC2) ch |= 0x8000;
        if (ch == 0) continue;
        buffer[buf_len++] = ch;
    }

    // Now split in lines of 32 characters max
    int buf_idx = 0;
    for (int row = 0; row < MAX_ROWS; row++) {
        // Go through the buffer and find the next line break
        // Notice that each character in buffer is one visual glyph
        // (irrespective of how many raw bytes it requires).
        int wrap = -1; int ch;
        for (ch = 0; ch < 32; ch++) {
            if (buf_idx+ch >= buf_len)
                break;

            if (buffer[buf_idx+ch] == '\n') {
                wrap = ch;
                break;
            }

            if (buffer[buf_idx+ch] == ' ')
                wrap = ch;
        }

        // Empty line, just skip it (or stop if we're done)
        if (ch == 0) {
            if (buf_idx >= buf_len) break;
            buf_idx++;
            continue;
        }

        // We need to wrap if we reached 32 chars and there are more
        if (ch == 32 && buf_idx+ch < buf_len)
            ch = wrap;

        // Calculate indentation to center the line
        // rounding it to the nearest multiple of 4
        int indent = (32 - ch) / 2;
        eia608_write_indent(cc, first_row, indent & ~3, parms->underline);
        for (int i=0; i<(indent & 3); i++)
            eia608_write_raw(cc == EIA608_CC1 ? EAI608_CC1_TS : EAI608_CC2_TS, false);

        // Now emit the characters.
        uint16_t accum = 0;
        for (int i = 0; i < ch; i++) {
            uint32_t token = buffer[buf_idx++];

            // Split the token into 1, 2, or 1+2 byte values
            uint16_t single = 0, pair = 0;
            if (token >> 16) {
                single = token >> 16;
                pair = token & 0xffff;
            } else if (token >> 8) {
                pair = token;
            }  else {
                single = token;
            }

            // Emit single bytes, accumulating them into pairs
            // if possible (otherwise use 0 to pad the pair)
            if (single) {
                if (accum) {
                    accum |= single;
                    eia608_write_raw(accum, true);
                    accum = 0;
                } else {
                    accum = single << 8;
                }
            }
            
            // Emit the pair
            if (pair) {
                if (accum) {
                    eia608_write_raw(accum, true);
                    accum = 0;
                } 
                eia608_write_raw(pair, true);
            }
        }

        // Emit any pending single byte value, since we are going
        // to emit a control code next.
        if (accum) {
            eia608_write_raw(accum, true);
        }

        // Move to the next line
        first_row++;

        // Skip wrapping character
        buf_idx++;
    }
}

void eia608_caption_show(eia608_channel_t cc, float duration_secs)
{
    eia608_write_ctrl_raw(cc == EIA608_CC1 ? EIA608_CC1_EOC : EIA608_CC2_EOC);
    force_clear_timer = duration_secs * 30;
}
