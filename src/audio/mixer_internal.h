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
 * Round IDs are issued by mixer_exec; when round R is done, every highpri
 * command enqueued before that mix (including VADPCM/Opus decode) is done too.
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
 * @brief Round ID that covers highpri commands enqueued right now.
 *
 * Returns the ID of the mix round whose completion guarantees that any
 * highpri command enqueued at this moment (VADPCM decodes, compaction
 * memmoves) has executed: the round being prepared if inside mixer_exec,
 * or the next round to be issued otherwise. samplebuffer uses this to tag
 * dirty tails and pending compactions.
 */
uint32_t __mixer_round_producer(void);

/**
 * @brief Enqueue an RSP-side backward memmove (dst < src) in the highpri queue.
 *
 * Used by samplebuffer_discard to compact sample buffers in-order with the
 * decode/mix commands that reference them, without CPU waits. dst/src must be
 * 8-byte aligned, len a multiple of 8.
 *
 * @return false if the mixer is not initialized (caller must fall back to
 *         a synchronous CPU copy).
 */
bool __mixer_memmove_async(void *dst, void *src, int len);

/**
 * @brief Wait until the producers of a dirty tail tagged with round_id are done.
 *
 * Like #__mixer_round_wait, but also handles the case where round_id is the
 * round currently being prepared (whose mix command is not enqueued yet):
 * in that case it drains the highpri queue, which already contains the
 * producer commands (eg: VADPCM decodes).
 */
void __mixer_dirty_wait(uint32_t round_id);

/**
 * @brief Wait until every issued mix round has completed.
 *
 * Cold-path helper for free/realloc of uncached buffers that in-flight
 * rounds may still reference.
 */
void __mixer_wait_idle(void);

#endif
