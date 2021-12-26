/**
 * @file dl.h
 * @brief RSP Command list
 * @ingroup rsp
 */

#ifndef __LIBDRAGON_DL_H
#define __LIBDRAGON_DL_H

#include <stdint.h>
#include <rsp.h>

// This is not a hard limit. Adjust this value when bigger commands are added.
#define DL_MAX_COMMAND_SIZE   16

typedef struct {
    void *buffers[2];
    int buf_size;
    int buf_idx;
    uint32_t *cur;
    uint32_t *sentinel;
    uint32_t sp_status_bufdone, sp_wstatus_set_bufdone, sp_wstatus_clear_bufdone;
} dl_ctx_t;

/**
 * @brief A preconstructed block of commands
 * 
 * To improve performance of execution of sequences of commands, it is possible
 * to create a "block". A block is a fixed set of commands that is created
 * once and executed multiple times.
 * 
 * To create a block, use #dl_block_begin and #dl_block_end. After creation,
 * you can use #dl_block_run at any point to run it. If you do not need the
 * block anymore, use #dl_block_free to dispose it.
 */
typedef struct dl_block_s dl_block_t;

/**
 * @brief A syncpoint in the command list
 * 
 * A syncpoint can be thought of as a pointer to a position in the command list.
 * After creation, it is possible to later check whether the RSP has reached it
 * or not.
 * 
 * To create a syncpoint, use #dl_syncpoint that returns a syncpoint that
 * references the current position. Call #dl_check_syncpoint or #dl_wait_syncpoint
 * to respectively do a single check or block waiting for the syncpoint to be
 * reached by RSP.
 * 
 * Syncpoints are implemented using interrupts, so they have a light but non
 * trivial overhead. Do not abuse them. For instance, it is reasonable to use
 * tens of syncpoints per frame, but not hundreds or thousands of them.
 * 
 * @note A valid syncpoint is an integer greater than 0.
 */
typedef int dl_syncpoint_t;

/**
 * @brief Initialize the RSP command list.
 */
void dl_init(void);

/**
 * @brief Shut down the RSP command list.
 */
void dl_close(void);


/**
 * @brief Register a ucode overlay into the command list engine.
 * 
 * This function registers a ucode overlay into the command list engine.
 * An overlay is a ucode that has been written to be compatible with the
 * command list engine (see rsp_dl.inc) and is thus able to executed commands
 * that are enqueued in the command list.
 * 
 * Each command in the command list starts with a 8-bit ID, in which the
 * upper 4 bits are the overlay ID and the lower 4 bits are the command ID.
 * The ID specified with this function is the overlay ID to associated with
 * the ucode. For instance, calling this function with ID 0x3 means that 
 * the overlay will be associated with commands 0x30 - 0x3F. The overlay ID
 * 0 is reserved to the command list engine.
 * 
 * Notice that it is possible to call this function multiple times with the
 * same ucode in case the ucode exposes more than 16 commands. For instance,
 * an ucode that handles up to 32 commands could be registered twice with
 * IDs 0x6 and 0x7, so that the whole range 0x60-0x7F is assigned to it.
 * When calling multiple times, consecutive IDs must be used.
 *             
 * @param      overlay_ucode  The ucode to register
 * @param[in]  id             The overlay ID that will be associated to this ucode.
 */
void dl_overlay_register(rsp_ucode_t *overlay_ucode, uint8_t id);

/**
 * @brief Return a pointer to the overlay state (in RDRAM)
 * 
 * Overlays can define a section of DMEM as persistent state. This area will be
 * preserved across overlay switching, by reading back into RDRAM the DMEM
 * contents when the overlay is switched away.
 * 
 * This function returns a pointer to the state area in RDRAM (not DMEM). It is
 * meant to modify the state on the CPU side while the overlay is not loaded.
 * The layout of the state and its size should be known to the caller.
 *
 * @param      overlay_ucode  The ucode overlay for which the state pointer will be returned.
 *
 * @return     Pointer to the overlay state (in RDRAM)
 */
void* dl_overlay_get_state(rsp_ucode_t *overlay_ucode);

/**
 * @brief Begin writing a command to the current RSP command list.
 * 
 * This function must be called when a new command must be written to
 * the command list. It returns a pointer where the command can be written.
 * Call #dl_write_end to terminate the command.
 * 
 * @return A pointer where the next command can be written.
 *
 * @code{.c}
 * 		// This example adds to the command list a sample command called
 * 		// CMD_SPRITE with code 0x3A (overlay 3, command A), with its arguments,
 * 		// for a total of three words.
 * 		
 * 		#define CMD_SPRITE  0x3A000000
 * 		
 * 		uint32_t *dl = dl_write_begin();
 * 		*dl++ = CMD_SPRITE | sprite_num;
 * 		*dl++ = (x0 << 16) | y0;
 * 		*dl++ = (x1 << 16) | y1;
 * 		dl_write_end(dl);
 * @endcode
 *
 * @note Each command can be up to DL_MAX_COMMAND_SIZE 32-bit words. Make
 * sure not to write more than that size without calling #dl_write_end.
 * 
 * @hideinitializer
 */
#define dl_write_begin() ({ \
	extern dl_ctx_t ctx; \
    ctx.cur; \
})

/**
 * @brief Finish writing a command to the current RSP command list.
 * 
 * This function terminates a command that was written to the command list.
 * 
 * @note Writing a command is not enough to make sure that the RSP will execute
 * it, as it might be idle. If you want to make sure that the RSP is running,
 * using #dl_flush.
 *
 * @param      dl_   Address pointing after the last word of the command.
 * 
 * @see #dl_write_begin
 * @see #dl_flush
 * 
 * @hideinitializer
 */
#define dl_write_end(dl_) ({ \
    extern dl_ctx_t ctx; \
	extern void dl_next_buffer(void); \
    \
	uint32_t *__dl = (dl_); \
    \
    /* Terminate the buffer (so that the RSP will sleep in case \
     * it catches up with us). \
     * NOTE: this is an inlined version of the internal dl_terminator() macro. */ \
    MEMORY_BARRIER(); \
    *(uint8_t*)(__dl) = 0x01; \
    \
    /* Update the pointer and check if we went past the sentinel, \
     * in which case it's time to switch to the next buffer. */ \
    ctx.cur = __dl; \
    if (ctx.cur > ctx.sentinel) { \
        dl_next_buffer(); \
    } \
})

/**
 * @brief Make sure that RSP starts executing up to the last written command.
 * 
 * RSP processes the current command list asynchronously as it is being written.
 * If it catches up with the CPU, it halts itself and waits for the CPU to
 * notify that more commands are available. On the contrary, if the RSP lags
 * behind it might keep executing commands as they are written without ever
 * sleeping. So in general, at any given moment the RSP could be crunching
 * commands or sleeping waiting to be notified that more commands are available.
 * 
 * This means that writing a command (#dl_write_begin / #dl_write_end) is not
 * enough to make sure it is executed; depending on timing and batching performed
 * by RSP, it might either be executed automatically or not. #dl_flush makes
 * sure that the RSP will see it and execute it.
 * 
 * This function does not block: it just make sure that the RSP will run the 
 * full command list written until now. If you need to actively wait until the
 * last written command has been executed, use #dl_sync.
 * 
 * It is suggested to call dl_flush every time a new "batch" of commands
 * has been written. In general, it is not a problem to call it often because
 * it is very very fast (takes only ~20 cycles). For instance, it can be called
 * after every dl_write_end without many worries, but if you know that you are
 * going to write a number of subsequent commands in straight line code, you
 * can postpone the call to #dl_flush after the whole sequence has been written.
 * 
 * @code{.c}
 * 		// This example shows some code configuring the lights for a scene.
 * 		// The command in this sample is called CMD_SET_LIGHT and requires
 * 		// a light index and the RGB colors for the list to update.
 *
 * 		#define CMD_SET_LIGHT  0x47000000
 *
 * 		for (int i=0; i<MAX_LIGHTS; i++) {
 * 			uint32_t *dl = dl_write_begin();
 * 			*dl++ = CMD_SET_LIGHT | i;
 * 			*dl++ = (lights[i].r << 16) | (lights[i].g << 8) | lights[i].b;
 * 			dl_write_end(dl);
 * 		}
 * 		
 * 		// After enqueuing multiple commands, it is sufficient
 * 		// to call dl_flush once to make sure the RSP runs them (in case
 * 		// it was idling).
 * 		dl_flush();
 * @endcode
 *
 * @note This is an experimental API. In the future, it might become 
 *       a no-op, and flushing could happen automatically at every dl_write_end().
 *       We are keeping it separate from dl_write_end() while experimenting more
 *       with the DL API.
 * 
 * @note This function is a no-op if it is called while a block is being recorded
 *       (see #dl_block_begin / #dl_block_end). This means calling this function
 *       in a block recording context will not guarantee the execution of commands
 *       that were queued prior to starting the block.
 *       
 */
void dl_flush(void);

/**
 * @brief Wait until all commands in command list have been executed by RSP.
 *
 * This function blocks until all commands present in the command list have
 * been executed by the RSP and the RSP is idle.
 * 
 * This function exists mostly for debugging purposes. Calling this function
 * is not necessary, as the CPU can continue enqueuing commands in the list
 * while the RSP is running them. If you need to synchronize between RSP and CPU
 * (eg: to access data that was processed by RSP) prefer using #dl_syncpoint /
 * #dl_wait_syncpoint which allows for more granular synchronization.
 */
#define dl_sync() ({ \
    dl_wait_syncpoint(dl_syncpoint()); \
})

/**
 * @brief Create a syncpoint in the command list.
 * 
 * This function creates a new "syncpoint" referencing the current position
 * in the command list. It is possible to later check when the syncpoint
 * is reached by RSP via #dl_check_syncpoint and #dl_wait_syncpoint.
 *
 * @return     ID of the just-created syncpoint.
 * 
 * @note It is not possible to create a syncpoint within a block
 * 
 * @see #dl_syncpoint_t
 */
dl_syncpoint_t dl_syncpoint(void);

/**
 * @brief Check whether a syncpoint was reached by RSP or not.
 * 
 * This function checks whether a syncpoint was reached. It never blocks.
 * If you need to wait for a syncpoint to be reached, use #dl_wait_syncpoint
 * instead of polling this function.
 *
 * @param[in]  sync_id  ID of the syncpoint to check
 *
 * @return true if the RSP has reached the syncpoint, false otherwise
 * 
 * @see #dl_syncpoint_t
 */
bool dl_check_syncpoint(dl_syncpoint_t sync_id);

/**
 * @brief Wait until a syncpoint is reached by RSP.
 * 
 * This function blocks waiting for the RSP to reach the specified syncpoint.
 * If the syncpoint was already called at the moment of call, the function
 * exits immediately.
 * 
 * @param[in]  sync_id  ID of the syncpoint to wait for
 * 
 * @see #dl_syncpoint_t
 */
void dl_wait_syncpoint(dl_syncpoint_t sync_id);


/**
 * @brief Begin creating a new block.
 * 
 * This function initiates writing a command list block (see #dl_block_t).
 * While a block is being written, all calls to #dl_write_begin / #dl_write_end
 * will record the commands into the block, without actually scheduling them for
 * execution. Use #dl_block_end to close the block and get a reference to it.
 * 
 * Only one block at a time can be created. Calling #dl_block_begin
 * twice (without any intervening #dl_block_end) will cause an assert.
 *
 * During block creation, the RSP will keep running as usual and
 * execute commands that have been already enqueue in the command list.
 *       
 * @note Calls to #dl_flush are ignored during block creation, as the RSP
 *       is not going to execute the block commands anyway.
 */
void dl_block_begin(void);

/**
 * @brief Finish creating a block.
 * 
 * This function completes a block and returns a reference to it (see #dl_block_t).
 * After this function is called, all subsequent #dl_write_begin / #dl_write_end
 * will resume working as usual: they will enqueue commands in the command list
 * for immediate RSP execution.
 * 
 * To run the created block, use #dl_block_run. 
 *
 * @return A reference to the just created block
 * 
 * @see dl_block_begin
 * @see dl_block_run 
 */
dl_block_t* dl_block_end(void);

/**
 * @brief Add to the RSP command list a command that runs a block.
 * 
 * This function runs a block that was previously created via #dl_block_begin
 * and #dl_block_end. It schedules a special command in the command list
 * that will run the block, so that execution of the block will happen in
 * order relative to other commands in the command list.
 * 
 * Blocks can call other blocks. For instance, if a block A has been fully
 * created, it is possible to call `dl_block_run(A)` at any point during the
 * creation of a second block B; this means that B will contain the special
 * command that will call A.
 *
 * @param block The block that must be run
 * 
 * @note The maximum depth of nested block calls is 8.
 */
void dl_block_run(dl_block_t *block);

/**
 * @brief Free a block that is not needed any more.
 * 
 * After calling this function, the block is invalid and must not be called
 * anymore.
 * 
 * @param  block  The block
 * 
 * @note If the block was being called by other blocks, these other blocks
 *       become invalid and will make the RSP crash if called. Make sure
 *       that freeing a block is only done when no other blocks reference it.
 */
void dl_block_free(dl_block_t *block);

/**
 * @brief Start building a high-priority queue.
 * 
 * This function enters a special mode in which a high-priority queue is
 * activated and can be filled with commands. After this command has been
 * called, all commands will be put in the high-priority queue, until
 * #dl_highpri_end is called.
 * 
 * The RSP will start processing the high-priority queue almost instantly
 * (as soon as the current command is done), pausing the normal queue. This will
 * also happen while the high-priority queue is being built, to achieve the
 * lowest possible latency. When the RSP finishes processing the high priority
 * queue (after #dl_highpri_end closes it), it resumes processing the normal
 * queue from the exact point that was left.
 * 
 * The goal of the high-priority queue is to either schedule latency-sensitive
 * commands like audio processing, or to schedule immediate RSP calculations
 * that should be performed right away, just like they were preempting what
 * the RSP is currently doing.
 * 
 * @note It is possible to create multiple high-priority queues by calling
 *       #dl_highpri_begin / #dl_highpri_end multiples time with short
 *       delays in-between. The RSP will process them in order. Notice that
 *       there is a overhead in doing so, so it might be advisable to keep
 *       the high-priority mode active for a longer period if possible. On the
 *       other hand, a shorter high-priority queue allows for the RSP to
 *       switch back to processing the normal queue before the next one
 *       is created.
 * 
 * @note It is not possible to create a block while the high-priority queue is
 *       active. Arrange for constructing blocks beforehand.
 *       
 * @note It is currently not possible to call a block from the
 *       high-priority queue. (FIXME: to be implemented)
 *       
 */
void dl_highpri_begin(void);

/**
 * @brief Finish building the high-priority queue and close it.
 * 
 * This function terminates and closes the high-priority queue. After this
 * command is called, all commands will be added to the normal queue.
 * 
 * Notice that the RSP does not wait for this function to be called: it will
 * start running the high-priority queue as soon as possible, even while it is
 * being built.
 */
void dl_highpri_end(void);

/**
 * @brief Wait for the RSP to finish processing all high-priority queues.
 * 
 * This function will spin-lock waiting for the RSP to finish processing
 * all high-priority queues. It is meant for debugging purposes or for situations
 * in which the high-priority queue is known to be very short and fast to run,
 * so that the overhead of a syncpoint would be too high.
 * 
 * For longer/slower high-priority queues, it is advisable to use a #dl_syncpoint_t
 * to synchronize (thought it has a higher overhead).
 */
void dl_highpri_sync(void);


void dl_queue_u8(uint8_t cmd);
void dl_queue_u16(uint16_t cmd);
void dl_queue_u32(uint32_t cmd);
void dl_queue_u64(uint64_t cmd);

void dl_noop();

/**
 * @brief Enqueue a command that sets a signal in SP status
 * 
 * The SP status register has 8 bits called "signals" that can be
 * atomically set or cleared by both the CPU and the RSP. They can be used
 * to provide asynchronous communication.
 * 
 * This function allows to enqueue a command in the list that will set and/or
 * clear a combination of the above bits.
 * 
 * Notice that signal bits 3-7 are used by the command list engine itself, so this
 * function must only be used for bits 0-2.
 * 
 * @param[in]  signal  A signal set/clear mask created by composing SP_WSTATUS_* 
 *                     defines.
 *                     
 * @note This is an advanced function that should be used rarely. Most
 * synchronization requirements should be fulfilled via #dl_syncpoint which is
 * easier to use.
 */
void dl_signal(uint32_t signal);

/**
 * @brief Enqueue a command to do a DMA transfer from DMEM to RDRAM
 *
 * @param      rdram_addr  The RDRAM address (destination, must be aligned to 8)
 * @param[in]  dmem_addr   The DMEM address (source, must be aligned to 8)
 * @param[in]  len         Number of bytes to transfer (must be multiple of 8)
 * @param[in]  is_async    If true, the RSP does not wait for DMA completion
 *                         and processes the next command as the DMA is in progress.
 *                         If false, the RSP waits until the transfer is finished
 *                         before processing the next command.
 *                         
 * @note The argument is_async refers to the RSP only. From the CPU standpoint,
 *       this function is always asynchronous as it just enqueues a command
 *       in the list. 
 */
void dl_dma_to_rdram(void *rdram_addr, uint32_t dmem_addr, uint32_t len, bool is_async);

/**
 * @brief Enqueue a command to do a DMA transfer from RDRAM to DMEM
 *
 * @param[in]  dmem_addr   The DMEM address (destination, must be aligned to 8)
 * @param      rdram_addr  The RDRAM address (source, must be aligned to 8)
 * @param[in]  len         Number of bytes to transfer (must be multiple of 8)
 * @param[in]  is_async    If true, the RSP does not wait for DMA completion
 *                         and processes the next command as the DMA is in progress.
 *                         If false, the RSP waits until the transfer is finished
 *                         before processing the next command.
 *                         
 * @note The argument is_async refers to the RSP only. From the CPU standpoint,
 *       this function is always asynchronous as it just enqueues a command
 *       in the list. 
 */
void dl_dma_to_dmem(uint32_t dmem_addr, void *rdram_addr, uint32_t len, bool is_async);

#endif
