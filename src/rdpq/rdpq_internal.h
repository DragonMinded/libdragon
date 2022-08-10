#ifndef __LIBDRAGON_RDPQ_INTERNAL_H
#define __LIBDRAGON_RDPQ_INTERNAL_H

#include "pputils.h"
#include "../rspq/rspq_internal.h"

#define RDPQ_OVL_ID (0xC << 28)

extern bool __rdpq_inited;
extern bool __rdpq_zero_blocks;

typedef struct rdpq_block_s rdpq_block_t;

typedef struct rdpq_block_s {
    rdpq_block_t *next;
    uint32_t autosync_state;
    uint32_t cmds[] __attribute__((aligned(8)));
} rdpq_block_t;

void __rdpq_reset_buffer();
void __rdpq_block_begin();
rdpq_block_t* __rdpq_block_end();
void __rdpq_block_free(rdpq_block_t *block);
void __rdpq_block_run(rdpq_block_t *block);
void __rdpq_block_check(void);
void __rdpq_block_next_buffer(void);
void __rdpq_block_update(uint32_t* old, uint32_t *new);
void __rdpq_block_update_reset(void);

void __rdpq_autosync_use(uint32_t res);
void __rdpq_autosync_change(uint32_t res);

void __rdpq_write8(uint32_t cmd_id, uint32_t arg0, uint32_t arg1);
void __rdpq_write16(uint32_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

/* Helpers for rdpq_write / rdpq_fixup_write */
#define __rdpcmd_count_words2(rdp_cmd_id, arg0, ...)  nwords += __COUNT_VARARGS(__VA_ARGS__) + 1;
#define __rdpcmd_count_words(arg)                    __rdpcmd_count_words2 arg

#define __rdpcmd_write_arg(arg)                      *ptr++ = arg;
#define __rdpcmd_write2(rdp_cmd_id, arg0, ...)        \
        *ptr++ = (RDPQ_OVL_ID + ((rdp_cmd_id)<<24)) | (arg0); \
        __CALL_FOREACH_BIS(__rdpcmd_write_arg, ##__VA_ARGS__);
#define __rdpcmd_write(arg)                          __rdpcmd_write2 arg

#define __rspcmd_write(...)                          ({ rspq_write(RDPQ_OVL_ID, __VA_ARGS__ ); })

/**
 * @brief Write a passthrough RDP command into the rspq queue
 * 
 * This macro handles writing a single RDP command into the rspq queue. It must be
 * used only with raw commands aka passthroughs, that is commands that are not
 * intercepted by RSP in any way, but just forwarded to RDP.
 * 
 * In block mode, the RDP command will be written to the static RDP buffer instead,
 * so that it will be sent directly to RDP without going through RSP at all.
 * 
 * Example syntax (notice the double parenthesis, required for uniformity 
 * with #rdpq_fixup_write):
 * 
 *    rdpq_write((RDPQ_CMD_SYNC_PIPE, 0, 0));
 */
#define rdpq_write(rdp_cmd) ({ \
    if (rspq_in_block()) { \
        extern volatile uint32_t *rdpq_block_ptr, *rdpq_block_end; \
        int nwords = 0; __rdpcmd_count_words(rdp_cmd); \
        if (__builtin_expect(rdpq_block_ptr + nwords > rdpq_block_end, 0)) \
            __rdpq_block_next_buffer(); \
        volatile uint32_t *ptr = rdpq_block_ptr, *old = ptr; \
        __rdpcmd_write(rdp_cmd); \
        rdpq_block_ptr = ptr; \
        __rdpq_block_update((uint32_t*)old, (uint32_t*)ptr); \
    } else { \
        __rspcmd_write rdp_cmd; \
    } \
})

/**
 * @brief Write a fixup RDP command into the rspq queue.
 * 
 * Fixup commands are similar to standard RDP commands, but they are intercepted
 * by RSP which (optionally) manipulates them before sending them to the RDP buffer.
 * In blocks, the final modified RDP command is written to the RDP static buffer,
 * intermixed with other commands, so there needs to be an empty slot for it.
 * 
 * This macro accepts the RSP command as first mandatory argument, and a list
 * of RDP commands that will be used as placeholder in the static RDP buffer.
 * For instance:
 * 
 *      rdpq_fixup_write(
 *          (RDPQ_CMD_MODIFY_OTHER_MODES, 0, 0),                               // RSP buffer
 *          (RDPQ_CMD_SET_OTHER_MODES, 0, 0), (RDPQ_CMD_SET_SCISSOR, 0, 0),    // RDP buffer
 *      );
 * 
 * This will generate a rdpq command "modify other modes" which is a RSP-only fixup;
 * when this fixup will run, it will generate two RDP commands: a SET_OTHER_MODES,
 * and a SET_SCISSOR. When the function above runs in block mode, the macro reserves
 * two slots in the RDP static buffer for the two RDP commands, and even initializes
 * the slots with the provided commands (in case this reduces the work the
 * fixup will have to do), and then writes the RSP command as usual. When running
 * outside block mode, instead, only the RSP command is emitted as usual, and the
 * RDP commands are ignored: in fact, the passthrough will simply push them into the
 * standard RDP dynamic buffers, so no reservation is required.
 */
#define rdpq_fixup_write(rsp_cmd, ...) ({ \
    if (__COUNT_VARARGS(__VA_ARGS__) != 0 && rspq_in_block()) { \
        extern volatile uint32_t *rdpq_block_ptr, *rdpq_block_end; \
        int nwords = 0; __CALL_FOREACH(__rdpcmd_count_words, ##__VA_ARGS__) \
        if (__builtin_expect(rdpq_block_ptr + nwords > rdpq_block_end, 0)) \
            __rdpq_block_next_buffer(); \
        volatile uint32_t *ptr = rdpq_block_ptr; \
        __CALL_FOREACH(__rdpcmd_write, ##__VA_ARGS__); \
        __rdpq_block_update_reset(); \
        rdpq_block_ptr = ptr; \
    } \
    __rspcmd_write rsp_cmd; \
})

#endif
