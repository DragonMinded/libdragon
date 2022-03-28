#ifndef __RSPQ_INTERNAL
#define __RSPQ_INTERNAL

#include <stdint.h>


typedef struct {
    volatile uint32_t *cur;             ///< Current write pointer within the active buffer
    volatile uint32_t *sentinel;        ///< Current write sentinel within the active buffer
} rspq_write_ctx_t;

/**
 * @brief RSP queue building context
 * 
 * This structure contains the state of a RSP queue as it is built by the CPU.
 * It is instantiated two times: one for the lwopri queue, and one for the
 * highpri queue. It contains the two buffers used in the double buffering
 * scheme, and some metadata about the queue.
 * 
 * The current write pointer is stored in the "cur" field. The "sentinel" field
 * contains the pointer to the last byte at which a new command can start,
 * before overflowing the buffer (given #RSPQ_MAX_COMMAND_SIZE). This is used
 * for efficiently check when it is time to switch to the other buffer: basically,
 * it is sufficient to check whether "cur > sentinel".
 * 
 * The current queue is stored in 3 global pointers: #rspq_ctx, #rspq_cur_pointer
 * and #rspq_cur_sentinel. #rspq_cur_pointer and #rspq_cur_sentinel are 
 * external copies of the "cur" and "sentinel" pointer of the
 * current context, but they are kept as separate global variables for
 * maximum performance of the hottest code path: #rspq_write. In fact, it is
 * much faster to access a global 32-bit pointer (via gp-relative offset) than
 * dereferencing a member of a global structure pointer.
 * 
 * rspq_switch_context is called to switch between lowpri and highpri,
 * updating the three global pointers.
 * 
 * When building a block, #rspq_ctx is set to NULL, while the other two
 * pointers point inside the block memory.
 */
typedef struct {
    void *buffers[2];                   ///< The two buffers used to build the RSP queue
    int buf_size;                       ///< Size of each buffer in 32-bit words
    int buf_idx;                        ///< Index of the buffer currently being written to.
    uint32_t sp_status_bufdone;         ///< SP status bit to signal that one buffer has been run by RSP
    uint32_t sp_wstatus_set_bufdone;    ///< SP mask to set the bufdone bit
    uint32_t sp_wstatus_clear_bufdone;  ///< SP mask to clear the bufdone bit
    rspq_write_ctx_t write_ctx;
} rspq_ctx_t;

#endif
