/**
 * @file eia608.h
 * @brief Library to generate NTSC EIA-608 closed captions
 * @ingroup display
 *
 * This module provides a simple way to generate EIA-608 closed captions
 * for NTSC video signals. The EIA-608 standard defines a way to transmit
 * text data over the vertical blanking interval of an NTSC video signal.
 * 
 * The N64 VI is powerful enough that it can be programmed to emit a
 * EIA-608 compliant signal, while still displaying a framebuffer. This module
 * offers a simple API to implement that.
 * 
 * To generate caption, first initialize the module at boot via #eia608_init.
 * 
 * Once the video mode has been fully configured via either #display_init
 * or via the VI library, you can start generating captions by calling
 * #eia608_start. This function will perform calculations to inject the
 * EIA-608 signal into the video signal, so it is important to call it
 * after all VI configurations have been performed. If you need to perform
 * *any* change to VI configuration, make sure to call #eia608_stop first,
 * and then call #eia608_start again.
 * 
 * To generate captions, you can use #eia608_caption_prepare and #eia608_caption_show
 * to display a UTF-8 string on the screen. These functions will take care of
 * encoding the string into EIA-608 format, doing some basic formatting (word-wrap,
 * and centering), and hiding it after the specified number of seconds.
 * 
 * This module also provides a way to display raw EIA-608 data, via
 * #eia608_write_raw. One possible use case for this function is to
 * display captions saved into the Scenarist SCC format, which is a very
 * simple text format that encodes captions as sequence of raw 16-bit
 * EIA-608 words.
 * 
 * @note The runtime performance impact of this module is around ~100 µs per
 *       VI frame (so 60 Hz), irrespective of the actual game speed.
 * 
 * @note EIA-608 is a standard for NTSC video signals. It is not compatible
 *       with PAL or MPAL TVs.
 * 
 * @see Wikipedia article on EIA-608: https://en.wikipedia.org/wiki/EIA-608
 * @see EIA-608 specs: https://www.govinfo.gov/content/pkg/CFR-2007-title47-vol1/pdf/CFR-2007-title47-vol1-sec15-119.pdf
 */

#ifndef LIBDRAGON_EIA608_H
#define LIBDRAGON_EIA608_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Caption channel */
typedef enum {
    EIA608_CC1 = 1,
    EIA608_CC2 = 2,
} eia608_channel_t;

/** @brief Caption display parameters */
typedef struct {
    int row;                     ///< The row to display the caption on, range 1-15 (default: 11)
    bool underline;              ///< Enable underline for the caption (default: false)
} eia608_captionparms_t;

/** @brief Calculate EIA608 parity for 7-bit value */
#define EIA608_PARITY(b)       ((((1 ^ ((b)>>0) ^ ((b)>>1) ^ ((b)>>2) ^ ((b)>>3) ^ ((b)>>4) ^ ((b)>>5) ^ ((b)>>6)) & 1) << 7) | (b))

///@cond
#define EIA608_CTRL(c)         ((EIA608_PARITY((c) & 0xFF)) | (EIA608_PARITY((c) >> 8) << 8))
///@endcond

/** 
 * @brief EIA-608 control words
 * 
 * This is a list of EIA-608 control words as defined by the spec. Notice
 * that the semantic of these control words is a bit complex, so make sure
 * to refer to the spec for more information.
 */
typedef enum {
    EIA608_NOP = EIA608_CTRL(0x0000),       // No operation
    EAI608_CC1_TS = EIA608_CTRL(0x1139),    // CC1: Transparent space
    EAI608_CC2_TS = EIA608_CTRL(0x1939),    // CC2: Transparent space

    EIA608_CC1_RCL = EIA608_CTRL(0x1420),   // CC1: Resume caption loading
    EIA608_CC2_RCL = EIA608_CTRL(0x1C20),   // CC2: Resume caption loading
    EIA608_CC1_BS  = EIA608_CTRL(0x1421),   // CC1: Backspace
    EIA608_CC2_BS  = EIA608_CTRL(0x1C21),   // CC2: Backspace
    EIA608_CC1_AOF = EIA608_CTRL(0x1422),   // CC1: Alarm off
    EIA608_CC2_AOF = EIA608_CTRL(0x1C22),   // CC2: Alarm off
    EIA608_CC1_AON = EIA608_CTRL(0x1423),   // CC1: Alarm on
    EIA608_CC2_AON = EIA608_CTRL(0x1C23),   // CC2: Alarm on
    EIA608_CC1_DER = EIA608_CTRL(0x1424),   // CC1: Delete to end of row
    EIA608_CC2_DER = EIA608_CTRL(0x1C24),   // CC1: Delete to end of row
    EIA608_CC1_RU2 = EIA608_CTRL(0x1425),   // CC1: Roll-up 2 rows
    EIA608_CC2_RU2 = EIA608_CTRL(0x1C25),   // CC2: Roll-up 2 rows
    EIA608_CC1_RU3 = EIA608_CTRL(0x1426),   // CC1: Roll-up 3 rows
    EIA608_CC2_RU3 = EIA608_CTRL(0x1C26),   // CC2: Roll-up 3 rows
    EIA608_CC1_RU4 = EIA608_CTRL(0x1427),   // CC1: Roll-up 4 rows
    EIA608_CC2_RU4 = EIA608_CTRL(0x1C27),   // CC2: Roll-up 4 rows
    EIA608_CC1_FON = EIA608_CTRL(0x1428),   // CC1: Flash on
    EIA608_CC2_FON = EIA608_CTRL(0x1C28),   // CC2: Flash on
    EIA608_CC1_RDC = EIA608_CTRL(0x1429),   // CC1: Resume direct captioning
    EIA608_CC2_RDC = EIA608_CTRL(0x1C29),   // CC2: Resume direct captioning
    EIA608_CC1_TR  = EIA608_CTRL(0x142A),   // CC1: Text restart
    EIA608_CC2_TR  = EIA608_CTRL(0x1C2A),   // CC2: Text restart
    EIA608_CC1_RTD = EIA608_CTRL(0x142B),   // CC1: Resume text display
    EIA608_CC2_RTD = EIA608_CTRL(0x1C2B),   // CC2: Resume text display
    EIA608_CC1_EDM = EIA608_CTRL(0x142C),   // CC1: Erase displayed memory
    EIA608_CC2_EDM = EIA608_CTRL(0x1C2C),   // CC2: Erase displayed memory
    EIA608_CC1_CR  = EIA608_CTRL(0x142D),   // CC1: Carriage return
    EIA608_CC2_CR  = EIA608_CTRL(0x1C2D),   // CC2: Carriage return
    EIA608_CC1_ENM = EIA608_CTRL(0x142E),   // CC1: Erase non-displayed memory
    EIA608_CC2_ENM = EIA608_CTRL(0x1C2E),   // CC2: Erase non-displayed memory
    EIA608_CC1_EOC = EIA608_CTRL(0x142F),   // CC1: End of caption
    EIA608_CC2_EOC = EIA608_CTRL(0x1C2F),   // CC2: End of caption
    EIA608_CC1_TO1 = EIA608_CTRL(0x1721),   // CC1: Tab offset 1
    EIA608_CC2_TO1 = EIA608_CTRL(0x1F21),   // CC2: Tab offset 1
    EIA608_CC1_TO2 = EIA608_CTRL(0x1722),   // CC1: Tab offset 2
    EIA608_CC2_TO2 = EIA608_CTRL(0x1F22),   // CC2: Tab offset 2
    EIA608_CC1_TO3 = EIA608_CTRL(0x1723),   // CC1: Tab offset 3
    EIA608_CC2_TO3 = EIA608_CTRL(0x1F23),   // CC2: Tab offset 3
} eia608_ctrl_t;

/**
 * @brief Initialize the eia608 library
 */
void eia608_init(void);

/**
 * @brief Deinitialize the eia608 library
 */
void eia608_close(void);

/**
 * @brief Start emitting a EIA-608 compliant signal on the video output.
 * 
 * This function configures the module to start emitting EIA-608 on the current
 * configured resolution, and starts the signal generation.
 * 
 * Notice that internal parameters will be configured depending on the current
 * video mode, so make sure to call this function after the video mode has been
 * set (eg: after #display_init).
 * 
 * If you need to do *any* changes to the VI configuration, make sure to stop
 * the EIA-608 signal first by calling #eia608_stop.
 * 
 * @note TVs might require up to one second to lock both the NTSC signal *and*
 *       the EIA-608 signal. Make sure to call this function at least one second
 *       before you need the first caption to be displayed.
 * 
 * @see #eia608_stop
 */
void eia608_start(void);

/**
 * @brief Stop emitting a EIA-608 compliant signal on the video output.
 * 
 * This function stops the EIA-608 signal generation, and resets the internal
 * state of the module. It should be called before making any changes to the
 * VI configuration, and then #eia608_start should be called again to resume
 * the EIA-608 signal (if needed).
 * 
 * @see #eia608_start
 */
void eia608_stop(void);

/**
 * @brief Emit raw data to the EIA-608 signal.
 * 
 * This is an advanced function that allows you to emit raw data to the
 * EIA-608 signal. It is mainly useful for testing or for emitting special
 * commands that are currently not supported by the higher-level functions.
 * 
 * Data is stored into an internal buffer that is then flushed as frames are
 * displayed. Normally this buffer will never be full, but if it is, the
 * function will return early and return the number of bytes emitted.
 * 
 * @param data              The data to emit (a 16-bit word)
 * @param calc_parity       If true, the parity bits will be calculated on the data.
 *                          If false, the data will be sent as-is and thus probably
 *                          be invalid if parity bits are not correct.
 * 
 * @return bool             True if the data was emitted, false if the buffer is full
 */
bool eia608_write_raw(uint16_t data, bool calc_parity);

/**
 * @brief Write a control code to the EIA-608 signal.
 * 
 * @note Control codes are quite low level and their exact semantic is non-trivial,
 *       as it depends on the current state of the EIA-608 signal. Please 
 *       check the EIA-608 standard for more information.
 * 
 * @param ctrl        The control code to write
 */
void eia608_write_ctrl_raw(eia608_ctrl_t ctrl);

/**
 * @brief Emit a caption, with automatic positioning on the screen.
 * 
 * EIA-608 captions are divided into maximum 4 lines of 32 characters each.
 * This function will automatically word-wrap the caption into multiple lines if
 * necessary, but also respects any embedded newlines in the caption.
 * 
 * The caption will not be displayed but just prepared. You need
 * to call #eia608_caption_show to actually display it. This allows for perfect
 * syncing as preparing a caption can take a non trivial amount of time
 * (about 1 second every 60 characters), so to avoid avoid the captions always
 * a bit late, you can prepare the next sentence in advance and then show it
 * when needed.
 * 
 * By default, the caption will be displayed centered, at the bottom of the
 * screen, using white on black colors. You can change these parameters by
 * setting fields in the \p parms structure.
 * 
 * If the caption is too long to fit in the 4 lines, it will be truncated.
 * 
 * The input string must be provided in UTF-8 format, but please notice
 * that the EIA-608 standard only supports a limited set of characters,
 * even though some extensions are available to cover at least western
 * languages. Check the Wikipedia page for more information.
 * 
 * @note This function emits the caption using the EIA-608 "POP-ON" mode,
 *       where the caption is accumulated into a back buffer and then displayed
 *       all at once. This is the most common mode for captions. If you call
 *       this function to display a caption before the previous one has been
 *       displayed, the new caption will replace the old one.
 * 
 * @param cc                Channel in which the caption must be prepared.
 * @param utf8_str          The UTF-8 string to display
 * @param parms             Optiona parameters to control the caption
 *                          (use NULL for default values)
 * 
 * @see Wikipedia article on EIA-608: https://en.wikipedia.org/wiki/EIA-608
 */
void eia608_caption_prepare(eia608_channel_t cc, const char *utf8_str, eia608_captionparms_t *parms);

/**
 * @brief Show a caption that was previously prepared with #eia608_caption_prepare
 * 
 * This shows a caption that was prepared by #eia608_caption_prepare. The caption
 * will be displayed on the screen for the specified duration, and then hidden.
 * 
 * @param cc                Channel in which the caption was prepared.
 * @param duration_secs     The duration in seconds to display the caption
 */
void eia608_caption_show(eia608_channel_t cc, float duration_secs);

#ifdef __cplusplus
}
#endif

#endif