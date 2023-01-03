#ifndef LIBDRAGON_RDPQ_DEBUG_INTERNAL_H
#define LIBDRAGON_RDPQ_DEBUG_INTERNAL_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Log all the commands run by RDP until the time of this call.
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
 * 
 * @param new_buffer    If true, we know for sure that the RDP is about to switch buffer.
 *                      If false, this is an optimistic reading (eg: done in idle time),
 *                      so the contents might match previous readings.
 */
extern void (*rdpq_trace_fetch)(bool new_buffer);

/**
 * @brief Validate the next RDP command, given the RDP current state 
 * 
 * @param       buf     Pointer to the RDP command
 * @param       flags   Flags that configure the validation 
 * @param[out]  errs    If provided, this variable will contain the number of
 *                      validation errors that were found.
 * @param[out]  warns   If provided, this variable will contain the number of
 *                      validation warnings that were found.
 */
void rdpq_validate(uint64_t *buf, uint32_t flags, int *errs, int *warns);

/** @brief Disable echo of commands triggering validation errors */
#define RDPQ_VALIDATE_FLAG_NOECHO    0x00000001

/** @brief Show all triangles in logging (default: off) */
#define RDPQ_LOG_FLAG_SHOWTRIS       0x00000001

/** @brief Flags that configure the logging */
extern int __rdpq_debug_log_flags;

#endif /* LIBDRAGON_RDPQ_DEBUG_INTERNAL_H */
