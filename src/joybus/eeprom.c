/**
 * @file eeprom.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @author Christopher Bonhage <christopher.bonhage@meeq.tech>
 * @brief EEPROM support
 * @ingroup eeprom
 *
 *
 * EEPROM implementation overview
 * ==============================
 *
 * This module implements EEPROM access using a RAM write-back cache. EEPROM
 * writes in fact tend to be extremely slow, while reads are quite fast.
 * 
 * Public API remains synchronous and unchanged from the historical one, but
 * persistence to hardware is decoupled from logical writes. This achieves what
 * is normally called "eventual consistency".
 *
 * High-level model
 * ----------------
 * The RAM cache is the authoritative copy seen by callers, while EEPROM
 * hardware acts as a backing store flushed in background. The cache is
 * allocated lazily on first EEPROM access and then populated lazily per block,
 * so blocks are fetched from hardware only when they are actually needed.
 *
 * Read path
 * ---------
 * eeprom_read and eeprom_read_bytes serve data from RAM cache. When a cache
 * miss happens, the module performs a synchronous single-block fetch from
 * hardware. If the flusher is currently writing one block, read-miss logic
 * waits for that single in-flight write to complete before fetching.
 *
 * Write path
 * ----------
 * eeprom_write and eeprom_write_bytes immediately update cache contents and
 * mark touched blocks as dirty in a bitmap. For partial writes, required
 * blocks are validated first so read-modify-write semantics are preserved.
 *
 * Background flush
 * ----------------
 * A single async state machine persists one dirty block at a time. On each
 * completion, it schedules the next dirty block. Dirty selection starts from
 * a randomized index to avoid always favoring low-index blocks. If a write
 * fails, the block is marked dirty again and retried later.
 *
 * Read priority over flush
 * ------------------------
 * When a read is requested while background writes are active, this module
 * tries to keep blocking time low. A read miss requests temporary flush yield,
 * so the write callback stops chaining while the miss is being resolved. Once
 * the missing block has been fetched, flush resumes if it was active before.
 *
 * Busy semantics
 * --------------
 * eeprom_is_busy reports whether a flush write is currently in progress.
 * Hardware busy is still honored for direct synchronous read-block fetches.
 */

#include <string.h>
#include <stdlib.h>
#include "debug.h"
#include "interrupt.h"
#include "kernel/kernel_internal.h"
#include "kirq.h"
#include "../rand_internal.h"
#include "eeprom.h"
#include "joybus.h"
#include "joybus_commands.h"

/** @brief Joybus port for the cartridge connector */
#define EEPROM_PORT 4

static bool eeprom_maybe_busy = false;

/** @brief EEPROM background flushing state machine */
typedef enum {
    EEPROM_FLUSH_IDLE = 0,    ///< No background flush is in progress
    EEPROM_FLUSH_WRITE_BLOCK, ///< A block is being flushed asynchronously
} eeprom_flush_state_t;

static volatile eeprom_flush_state_t eeprom_flush_state = EEPROM_FLUSH_IDLE;

static uint8_t eeprom_flush_block = 0;     ///< The block currently being flushed
static uint8_t eeprom_last_hw_status = 0x00; ///< The last hardware status byte received
static volatile bool eeprom_read_miss_pending = false; ///< Whether a read miss is pending

static eeprom_type_t eeprom_cached_type = EEPROM_NONE;
static size_t eeprom_num_blocks = 0;
static uint8_t *eeprom_cache;
static uint8_t *eeprom_valid_bitmap;
static uint8_t *eeprom_dirty_bitmap;

static void eeprom_flush_write_callback(uint64_t *out_dwords, void *ctx);

/** Return one bit from a compact bitmap. */
static inline bool bitmap_get(const uint8_t *bitmap, size_t idx)
{
    return (bitmap[idx >> 3] & (1u << (idx & 7))) != 0;
}

/** Set one bit in a compact bitmap. */
static inline void bitmap_set(uint8_t *bitmap, size_t idx)
{
    bitmap[idx >> 3] |= (1u << (idx & 7));
}

/** Clear one bit in a compact bitmap. */
static inline void bitmap_clear(uint8_t *bitmap, size_t idx)
{
    bitmap[idx >> 3] &= ~(1u << (idx & 7));
}

/** Send one asynchronous EEPROM block write on Joybus. */
static void eeprom_write_block_async_hw(uint8_t block, const uint8_t *src, joybus_callback_t callback)
{
    joybus_cmd_eeprom_write_block_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_EEPROM_WRITE_BLOCK,
        .block = block,
    } };
    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    size_t i = EEPROM_PORT;
    input[i++] = sizeof(cmd.send);
    input[i++] = sizeof(cmd.recv);
    const size_t data_offset = offsetof(typeof(cmd.send), data);
    memcpy(&input[i], (void *)&cmd.send, data_offset);
    memcpy(&input[i + data_offset], src, sizeof(cmd.send.data));
    i += sizeof(cmd.send) + sizeof(cmd.recv);
    input[i] = 0xFE;
    input[sizeof(input) - 1] = 0x01;
    joybus_exec_async(input, callback, NULL);
}

/** Block until EEPROM busy bit is cleared, if a write happened before. */
static void eeprom_wait_ready(void)
{
    if (!eeprom_maybe_busy) return;

    joybus_cmd_identify_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_IDENTIFY,
    } };
    do { joybus_exec_cmd_struct(EEPROM_PORT, cmd); }
    while (cmd.recv.status & JOYBUS_IDENTIFY_STATUS_EEPROM_BUSY);
    eeprom_maybe_busy = false;
}

/** Read one EEPROM block synchronously from hardware. */
static void eeprom_read_block_hw(uint8_t block, uint8_t *dest)
{
    eeprom_wait_ready();

    joybus_cmd_eeprom_read_block_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_EEPROM_READ_BLOCK,
        .block = block,
    } };
    joybus_exec_cmd_struct(EEPROM_PORT, cmd);
    memcpy(dest, cmd.recv.data, EEPROM_BLOCK_SIZE);
}

/** Probe EEPROM type/capacity once and cache the result. */
static void eeprom_probe_capacity_if_needed(void)
{
    if (eeprom_num_blocks) return;

    joybus_cmd_identify_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_IDENTIFY,
    } };
    joybus_exec_cmd_struct(EEPROM_PORT, cmd);
    switch (cmd.recv.identifier)
    {
        case JOYBUS_IDENTIFIER_CART_EEPROM_16KBIT:
            eeprom_cached_type = EEPROM_16K;
            eeprom_num_blocks = 256;
            break;
        case JOYBUS_IDENTIFIER_CART_EEPROM_4KBIT:
            eeprom_cached_type = EEPROM_4K;
            eeprom_num_blocks = 64;
            break;
        default:
            eeprom_cached_type = EEPROM_NONE;
            eeprom_num_blocks = 0;
            break;
    }
}

/** Lazily allocate cache and bitmaps on first EEPROM use. */
static void eeprom_cache_alloc_if_needed(void)
{
    if (eeprom_cache) return;

    eeprom_probe_capacity_if_needed();
    if (!eeprom_num_blocks) return;

    size_t bitmap_bytes = (eeprom_num_blocks + 7) / 8;
    eeprom_cache = malloc(eeprom_num_blocks * EEPROM_BLOCK_SIZE);
    assertf(eeprom_cache, "Out of memory");
    eeprom_valid_bitmap = calloc(bitmap_bytes, 1);
    assertf(eeprom_valid_bitmap, "Out of memory");
    eeprom_dirty_bitmap = calloc(bitmap_bytes, 1);
    assertf(eeprom_dirty_bitmap, "Out of memory");

}

/** Try to schedule the next dirty block flush. */
static void eeprom_flush_kick(void);

/** Find a dirty block using randomized scan start to avoid bias. */
static int eeprom_find_next_dirty_block(void)
{
    if (!eeprom_num_blocks) return -1;

    size_t block = __randn(eeprom_num_blocks);
    for (size_t i = 0; i < eeprom_num_blocks; i++)
    {
        block++;
        if (block == eeprom_num_blocks) block = 0;
        if (bitmap_get(eeprom_dirty_bitmap, block))
            return block;
    }
    return -1;
}

/** Handle completion of one async write and chain next flush step. */
static void eeprom_flush_write_callback(uint64_t *out_dwords, void *ctx)
{
    (void)ctx;
    if (eeprom_flush_state != EEPROM_FLUSH_WRITE_BLOCK) return;

    const uint8_t *out_bytes = (void *)out_dwords;
    const joybus_cmd_eeprom_write_block_t *cmd = (void *)&out_bytes[EEPROM_PORT + JOYBUS_COMMAND_METADATA_SIZE];
    eeprom_last_hw_status = cmd->recv.status;
    eeprom_maybe_busy = true;

    if (cmd->recv.status != 0x00)
    {
        // Keep data pending so a later kick can retry persistence.
        bitmap_set(eeprom_dirty_bitmap, eeprom_flush_block);
    }

    eeprom_flush_state = EEPROM_FLUSH_IDLE;
    // Yield to an outstanding read miss before continuing the flush chain.
    if (eeprom_read_miss_pending) return;
    eeprom_flush_kick();
}

/** Start one async write for a dirty block (if flusher is idle). */
static void eeprom_flush_kick(void)
{
    uint8_t block;
    const uint8_t *src;

    // Atomically claim one dirty block and transition the flusher state.
    disable_interrupts();
    if (eeprom_flush_state != EEPROM_FLUSH_IDLE)
    {
        enable_interrupts();
        return;
    }

    int next_block = eeprom_find_next_dirty_block();
    if (next_block < 0)
    {
        enable_interrupts();
        return;
    }

    block = (uint8_t)next_block;
    eeprom_flush_block = block;
    bitmap_clear(eeprom_dirty_bitmap, block);
    eeprom_flush_state = EEPROM_FLUSH_WRITE_BLOCK;
    src = &eeprom_cache[(size_t)block * EEPROM_BLOCK_SIZE];
    enable_interrupts();

    eeprom_write_block_async_hw(
        block,
        src,
        eeprom_flush_write_callback
    );
}

/** Wait until there is no async write currently in-flight. */
static void eeprom_flush_wait_idle(void)
{
    kirq_wait_t w = kirq_begin_wait_si();
    while (eeprom_flush_state != EEPROM_FLUSH_IDLE)
    {
        if (__kernel) kirq_wait(&w);
    }
}

/**
 * Pause flush chaining while serving foreground cache misses.
 *
 * Returns true if a flush write was already active and had to be drained first.
 */
static bool eeprom_flush_pause(void)
{
    bool had_flush_in_progress;
    disable_interrupts();
    had_flush_in_progress = (eeprom_flush_state != EEPROM_FLUSH_IDLE);
    eeprom_read_miss_pending = true;
    enable_interrupts();

    // At most one block can be in-flight, wait for it once.
    if (had_flush_in_progress) eeprom_flush_wait_idle();
    return had_flush_in_progress;
}

/** Resume flush chaining after foreground cache-miss handling. */
static void eeprom_flush_resume(bool had_flush_in_progress)
{
    disable_interrupts();
    eeprom_read_miss_pending = false;
    enable_interrupts();

    if (had_flush_in_progress) eeprom_flush_kick();
}

/** Ensure one block is valid in cache, fetching it lazily if needed. */
static void eeprom_cache_ensure_block_valid(size_t block)
{
    if (bitmap_get(eeprom_valid_bitmap, block) || bitmap_get(eeprom_dirty_bitmap, block))
        return;

    eeprom_read_block_hw((uint8_t)block, &eeprom_cache[block * EEPROM_BLOCK_SIZE]);
    bitmap_set(eeprom_valid_bitmap, block);
}

eeprom_type_t eeprom_present( void )
{
    eeprom_probe_capacity_if_needed();
    return eeprom_cached_type;
}

size_t eeprom_total_blocks( void )
{
    eeprom_probe_capacity_if_needed();
    return eeprom_num_blocks;
}

void eeprom_read( uint8_t block, void * dest )
{
    eeprom_read_bytes(dest, (size_t)block * EEPROM_BLOCK_SIZE, EEPROM_BLOCK_SIZE);
}

uint8_t eeprom_write( uint8_t block, const void * src )
{
    eeprom_write_bytes(src, (size_t)block * EEPROM_BLOCK_SIZE, EEPROM_BLOCK_SIZE);
    return eeprom_last_hw_status;
}

void eeprom_read_bytes( void * dest, size_t start, size_t len )
{
    eeprom_cache_alloc_if_needed();
    assertf(eeprom_num_blocks > 0, "EEPROM not present");
    if (len == 0) return;
    assertf((start + len) <= (eeprom_num_blocks * EEPROM_BLOCK_SIZE), 
            "EEPROM read out of bounds: start=%zu, len=%zu, total_size=%zu",
            start, len, eeprom_num_blocks * EEPROM_BLOCK_SIZE);

    size_t first_block = start / EEPROM_BLOCK_SIZE;
    size_t last_block = (start + len - 1) / EEPROM_BLOCK_SIZE;
    bool had_flush_in_progress = eeprom_flush_pause();
    for (size_t block = first_block; block <= last_block; block++)
        eeprom_cache_ensure_block_valid(block);
    eeprom_flush_resume(had_flush_in_progress);

    memcpy(dest, &eeprom_cache[start], len);
}

void eeprom_write_bytes( const void * src, size_t start, size_t len )
{
    eeprom_cache_alloc_if_needed();
    assertf(eeprom_num_blocks > 0, "EEPROM not present");
    if (len == 0) return;
    assertf((start + len) <= (eeprom_num_blocks * EEPROM_BLOCK_SIZE), 
            "EEPROM write out of bounds: start=%zu, len=%zu, total_size=%zu",
            start, len, eeprom_num_blocks * EEPROM_BLOCK_SIZE);

    // For partial writes we need existing data to preserve untouched bytes.
    size_t first_block = start / EEPROM_BLOCK_SIZE;
    size_t last_block = (start + len - 1) / EEPROM_BLOCK_SIZE;
    size_t first_offset = start % EEPROM_BLOCK_SIZE;
    size_t end_offset = (start + len) % EEPROM_BLOCK_SIZE;
    bool need_rmw_prefix = first_offset || (last_block != first_block && end_offset);
    if (need_rmw_prefix) {
        bool had_flush_in_progress = eeprom_flush_pause();
        if (first_offset)
            eeprom_cache_ensure_block_valid(first_block);
        if (last_block != first_block && end_offset)
            eeprom_cache_ensure_block_valid(last_block);
        eeprom_flush_resume(had_flush_in_progress);
    }

    // Keep cache write + dirty/valid bitmap updates coherent vs flush callback.
    disable_interrupts();
    memcpy(&eeprom_cache[start], src, len);
    for (size_t block = first_block; block <= last_block; block++)
    {
        bitmap_set(eeprom_valid_bitmap, block);
        bitmap_set(eeprom_dirty_bitmap, block);
    }
    enable_interrupts();

    eeprom_flush_kick();
}

bool eeprom_is_busy(void)
{
    bool busy;
    disable_interrupts();
    busy = (eeprom_flush_state != EEPROM_FLUSH_IDLE);
    enable_interrupts();
    return busy;
}

void eeprom_wait_idle(void)
{
    kirq_wait_t w = kirq_begin_wait_si();
    while (eeprom_is_busy())
    {
        if (__kernel) kirq_wait(&w);
    }
}
