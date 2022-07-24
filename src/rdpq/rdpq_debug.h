#ifndef LIBDRAGON_RDPQ_DEBUG_H
#define LIBDRAGON_RDPQ_DEBUG_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Log all the commmands run by RDP until the time of this call.
 * 
 * Given that RDP buffers get reused as circular buffers, it is important
 * to call this function often enough.
 */
extern void (*rdpq_trace)(void);

/**
 * @brief Notify the trace engine that RDP is about to change buffer.
 * 
 * Calling this function notifies the trace engine that the RDP buffer is possibly
 * going to be switched soon, and the current pointers should be fetched and stored
 * away for later dump.
 * 
 * Notice that this function does not create a copy of the memory contents, but just
 * saves the DP_START/DP_END pointers. It is up to the client to make sure to call
 * rdpq_trace() at least once before the same buffer gets overwritten in the future.
 */
extern void (*rdpq_trace_fetch)(void);

/**
 * @brief Validate the next RDP command, given the RDP current state 
 * 
 * @param       buf     Pointer to the RDP command 
 * @param[out]  errs    If provided, this variable will contain the number of
 *                      validation errors that were found.
 * @param[out]  warns   If provided, this variable will contain the number of
 *                      validation warnings that were found.
 */
void rdpq_validate(uint64_t *buf, int *errs, int *warns);

/**
 * @brief Return the size of the next RDP commands
 * 
 * @param       buf     Pointer to RDP command
 * @return      Number of 64-bit words the command is composed of
 */
int rdpq_disasm_size(uint64_t *out);

/**
 * @brief Disassemble a RDP command
 * 
 * @param   buf     Pointer to the RDP command 
 * @param   out     Ouput stream where to write the disassembled string   
 * @return  Number of 64-bit words the command is composed of
 */
void rdpq_disasm(uint64_t *buf, FILE *out);

#endif /* LIBDRAGON_RDPQ_DEBUG_H */
