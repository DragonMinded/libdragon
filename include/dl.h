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
void dl_start(void);

uint8_t dl_overlay_add(rsp_ucode_t *overlay_ucode);
void* dl_overlay_get_state(rsp_ucode_t *overlay_ucode);
void dl_overlay_register_id(uint8_t overlay_index, uint8_t id);

void dl_close(void);

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
	extern uint32_t *dl_cur_pointer; \
    dl_cur_pointer; \
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
	extern uint32_t *dl_cur_pointer; \
	extern uint32_t *dl_cur_sentinel; \
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
    dl_cur_pointer = __dl; \
    if (dl_cur_pointer > dl_cur_sentinel) { \
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
 * 		dl_flush(dl);
 * @endcode
 *
 * @note This is an experimental API. In the future, it might become 
 *       a no-op, and flushing could happen automatically at every dl_write_end().
 *       We are keeping it separate from dl_write_end() while experimenting more
 *       with the DL API.
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
 * This function returns a "syncpoint" from the command list. This can be
 * thought as a pointer to the current position of the list. A syncpoint
 * allows to later check whether the RSP has reached it or not: this
 * allows for granular synchronization between CPU and RSP.
 * 
 * Syncpoints are implemented using interrupts, so they have a little but 
 * non-trivial overhead. They should not be abused but used sparingly.
 *
 * @return     ID of the just-created syncpoint.
 * 
 * @note It is not possible to create a syncpoint within a block
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
 * @see #dl_syncpoint
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
 * @see #dl_syncpoint
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
 * @note The maximum number of nested block calls is 8.
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


void dl_queue_u8(uint8_t cmd);
void dl_queue_u16(uint16_t cmd);
void dl_queue_u32(uint32_t cmd);
void dl_queue_u64(uint64_t cmd);

void dl_noop();
void dl_signal(uint32_t signal);

#endif
