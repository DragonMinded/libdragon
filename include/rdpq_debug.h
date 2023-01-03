/**
 * @file rdpq_debug.h
 * @brief RDP Command queue: debugging helpers
 * @ingroup rdp
 */

#ifndef LIBDRAGON_RDPQ_DEBUG_H
#define LIBDRAGON_RDPQ_DEBUG_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct surface_s surface_t;
///@endcond

/**
 * @brief Initialize the RDPQ debugging engine
 * 
 * This function initializes the RDP debugging engine. After calling this function,
 * all RDP commands sent via the rspq/rdpq libraries and overlays will be analyzed
 * and validated, providing insights in case of programming errors that trigger
 * hardware undefined behaviors or corrupt graphics. The validation errors
 * and warnings are emitted via #debugf, so make sure to initialize the debugging
 * library to see it.
 * 
 * This is especially important with RDP because the chips is very hard to program
 * correctly, and it is common to do mistakes. While rdpq tries to shield the
 * programmer from most common mistakes via the fixups, it is still possible
 * to do mistakes (eg: creating non-working color combiners) that the debugging
 * engine can help spotting.
 * 
 * Notice that the validator needs to maintain a representation of the RDP state,
 * as it is not possible to query the RDP about it. So it is better to call
 * #rdpq_debug_start immediately after #rdpq_init when required, so that it can
 * track all commands from the start. Otherwise, some spurious validation error
 * could be emitted.
 * 
 * @note The validator does cause a measurable overhead. It is advised to enable
 *       it only in debugging builds.
 */
void rdpq_debug_start(void);

/**
 * @brief Stop the rdpq debugging engine.
 */
void rdpq_debug_stop(void);

/**
 * @brief Show a full log of all the RDP commands
 * 
 * This function configures the debugging engine to also log all RDP commands
 * to the debugging channel (via #debugf). This is extremely verbose and should
 * be used sparingly to debug specific issues.
 * 
 * This function does enqueue a command in the rspq queue, so it is executed
 * in order with respect to all rspq/rdpq commands. You can thus delimit
 * specific portions of your code with `rdpq_debug_log(true)` /
 * `rdpq_debug_log(false)`, to see only the RDP log produced by those
 * code lines.
 * 
 * @param show_log    true/false to enable/disable the RDP log.
 */
void rdpq_debug_log(bool show_log);

/**
 * @brief Add a custom message in the RDP logging
 * 
 * If the debug log is active, this function adds a custom message to the log.
 * It can be useful to annotate different portions of the disassembly.
 * 
 * For instance, the following code:
 * 
 * @code{.c}
 *      rdpq_debug_log(true);
 * 
 *      rdpq_debug_log_msg("Black rectangle");
 *      rdpq_set_mode_fill(RGBA32(0,0,0,0));
 *      rdpq_fill_rectangle(0, 0, 320, 120);
 * 
 *      rdpq_debug_log_msg("Red rectangle");
 *      rdpq_set_fill_color(RGBA32(255,0,0,0));
 *      rdpq_fill_rectangle(0, 120, 320, 240);
 * 
 *      rdpq_debug_log(false);
 * @endcode
 * 
 * produces this output:
 * 
 *      [0xa00e7128] f1020000000332a8    RDPQ_MESSAGE     Black rectangle
 *      [0xa00e7130] ef30000000000000    SET_OTHER_MODES  fill
 *      [0xa00e7138] ed00000000000000    SET_SCISSOR      xy=(0.00,0.00)-(0.00,0.00)
 *      [0xa00e7140] f700000000000000    SET_FILL_COLOR   rgba16=(0,0,0,0) rgba32=(0,0,0,0)
 *      [0xa00e7148] f65001e000000000    FILL_RECT        xy=(0.00,0.00)-(320.00,120.00)
 *      [0xa00e7150] f1020000000332b8    RDPQ_MESSAGE     Red rectangle
 *      [0xa00e7158] e700000000000000    SYNC_PIPE
 *      [0xa00e7160] f7000000f800f800    SET_FILL_COLOR   rgba16=(31,0,0,0) rgba32=(248,0,248,0)
 *      [0xa00e7168] f65003c0000001e0    FILL_RECT        xy=(0.00,120.00)-(320.00,240.00)
 *      [0xa00e7170] f101000000000000    RDPQ_SHOWLOG     show=0
 * 
 * where you can see the `RDPQ_MESSAGE` lines which helps isolate portion of commands with
 * respect to the source lines that generated them.
 * 
 * @param str           message to display
 */
void rdpq_debug_log_msg(const char *str);

/**
 * @brief Acquire a dump of the current contents of TMEM
 * 
 * Inspecting TMEM can be useful for debugging purposes, so this function
 * dumps it to RDRAM for inspection. It returns a surface that contains the
 * contents of TMEM as a 32x64 FMT_RGBA16 (4K) buffer, but obviously the
 * contents can vary and have nothing to do with this layout.
 * 
 * The function will do a full sync (via #rspq_wait) to make sure the
 * surface data has been fully written by RDP when the function returns.
 * 
 * For the debugging, you can easily dump the contents of the surface calling
 * #debug_hexdump.
 * 
 * The surface must be freed via #surface_free when it is not useful anymore.
 * 
 * @code
 *      // Get the TMEM contents
 *      surface_t surf = rdpq_debug_get_tmem();
 * 
 *      // Dump TMEM in the debug spew
 *      debug_hexdump(surf.buffer, 4096);
 * 
 *      surface_free(&surf);
 * @endcode
 * 
 * @return    A surface with TMEM contents, that must be freed via #surface_free.
 */
surface_t rdpq_debug_get_tmem(void);

/**
 * @brief Install a custom hook that will be called every time a RDP command is processed.
 * 
 * This function can be used to perform custom analysis on the RDP stream. It allows
 * you to register a callback that will be called any time a RDP command is processed
 * by the debugging engine.
 * 
 * @param   hook    Hook function that will be called for each RDP command
 * @param   ctx     Context passed to the hook function
 * 
 * @note You can currently install only one hook
 */
void rdpq_debug_install_hook(void (*hook)(void *ctx, uint64_t* cmd, int cmd_size), void* ctx);

/**
 * @brief Disassemble a RDP command
 * 
 * This function allows to access directly the disassembler which is part
 * of the rdpq debugging log. Normally, you don't need to use this function:
 * just call #rdpq_debug_log to see all RDP commands in disassembled format.
 * 
 * This function can be useful for writing tools or manually debugging a
 * RDP stream.
 * 
 * @param   buf     Pointer to the RDP command 
 * @param   out     Ouput stream where to write the disassembled string   
 * @return  true if the command was disassembled, false if the command is being
 *          held in a buffer waiting for more commands to be appended.
 * 
 * @see #rdpq_debug_disasm_size
 */
bool rdpq_debug_disasm(uint64_t *buf, FILE *out);

/**
 * @brief Return the size of the next RDP commands
 * 
 * @param       buf     Pointer to RDP command
 * @return      Number of 64-bit words the command is composed of
 */
int rdpq_debug_disasm_size(uint64_t *buf);


#ifdef __cplusplus
}
#endif

#endif
