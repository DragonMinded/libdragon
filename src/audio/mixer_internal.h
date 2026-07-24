/**
 * @file mixer_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_MIXER_INTERNAL_H
#define LIBDRAGON_MIXER_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

/** @brief RSPQ overlay ID assigned to the mixer ucode */
extern uint32_t __mixer_overlay_id;

/**
 * @brief Return true if the RSP has fully executed the given mix round.
 *
 * Round IDs are issued by mixer_exec; when round R is done, every command
 * enqueued before its mix is done too.
 */
bool __mixer_round_done(uint32_t round_id);

/**
 * @brief Wait until the given mix round has completed (bounded spinwait).
 *
 * Used as a rare fallback when CPU needs to reclaim memory still referenced
 * by in-flight RSP work. Prefer arranging for the wait to be a no-op.
 */
void __mixer_round_wait(uint32_t round_id);

/**
 * @brief Wait until every issued mix round has completed.
 *
 * Cold-path helper for free/realloc of uncached buffers that in-flight
 * rounds may still reference.
 */
void __mixer_wait_idle(void);

#endif
