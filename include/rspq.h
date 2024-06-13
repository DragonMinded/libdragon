/**
 * @file rspq.h
 * @brief RSP Command queue
 * @ingroup rsp
 * 
 * The RSP command queue library provides the basic infrastructure to allow
 * a very efficient use of the RSP coprocessor. On the CPU side, it implements
 * an API to enqueue "commands" to be executed by RSP into a ring buffer, that
 * is concurrently consumed by RSP in background. On the RSP side, it provides the
 * core loop that reads and execute the queue prepared by the CPU, and an
 * infrastructure to write "RSP overlays", that is libraries that plug upon
 * the RSP command queue to perform actual RSP jobs (eg: 3D graphics, audio, etc.).
 * 
 * The library is extremely efficient. It is designed for very high throughput
 * and low latency, as the RSP pulls by the queue concurrently as the CPU 
 * fills it. Through some complex synchronization paradigms, both CPU and RSP
 * run fully lockless, that is never need to explicitly synchronize with
 * each other (unless requested by the user). The CPU can keep filling the
 * queue and must only wait for RSP in case the queue becomes full; on the
 * other side, the RSP can keep processing the queue without ever talking to
 * the CPU.
 * 
 * The library has been designed to be able to enqueue thousands of RSP commands
 * per frame without its overhead to be measurable, which should be more than
 * enough for most use cases.
 * 
 * This documentation describes the API of the queue, and how to use it. For
 * details on how it is implemented, see the comments in rspq.c.
 * 
 * ## Commands
 * 
 * Each command in the queue is made by one or more 32-bit words (up to
 * #RSPQ_MAX_COMMAND_SIZE). The MSB of the first word is the command ID. The
 * higher 4 bits are called the "overlay ID" and identify the overlay that is
 * able to execute the command; the lower 4 bits are the command index, which
 * identify the command within the overlay. For instance, command ID 0x37 is
 * command index 7 in overlay 3.
 * 
 * As the RSP executes the queue, it will parse the command ID and dispatch
 * it for execution. When required, the RSP will automatically load the
 * RSP overlay needed to execute a command. In the previous example, the RSP
 * will load into IMEM/DMEM overlay 3 (unless it was already loaded) and then
 * dispatch command 7 to it.
 * 
 * ## Higher-level libraries and overlays
 * 
 * Higher-level libraries that come with their RSP ucode can be designed to
 * use the RSP command queue to efficiently coexist with all other RSP libraries
 * provided by libdragon. In fact, by using the overlay mechanism, each library
 * can obtain its own overlay ID, and enqueue commands to be executed by the
 * RSP through the same unique queue. Overlay IDs are allocated dynamically by
 * rspq in registration order, to avoid conflicts between libraries.
 * 
 * End-users can then use all these libraries at the same time, without having
 * to arrange for complex RSP synchronization, asynchronous execution or plan for
 * efficient context switching. In fact, they don't even need to be aware that
 * the libraries are using the RSP. Through the unified command queue,
 * the RSP can be used efficiently and effortlessly without idle time, nor
 * wasting CPU cycles waiting for completion of a task before switching to 
 * another one.
 * 
 * Higher-level libraries that are designed to use the RSP command queue must:
 * 
 *   * Call #rspq_init at initialization. The function can be called multiple
 *     times by different libraries, with no side-effect.
 *   * Call #rspq_overlay_register to register a #rsp_ucode_t as RSP command
 *     queue overlay, obtaining an overlay ID to use.
 *   * Provide higher-level APIs that, when required, call #rspq_write
 *     and #rspq_flush to enqueue commands for the RSP. For
 *     instance, a matrix library might provide a "matrix_mult" function that
 *     internally calls #rspq_write to enqueue a command
 *     for the RSP to perform the calculation.
 * 
 * Normally, end-users will not need to manually enqueue commands in the RSP
 * queue: they should only call higher-level APIs which internally do that.
 * 
 * ## Blocks
 * 
 * A block (#rspq_block_t) is a prerecorded sequence of RSP commands that can
 * be played back. Blocks can be created via #rspq_block_begin / #rspq_block_end,
 * and then executed by #rspq_block_run. It is also possible to do nested
 * calls (a block can call another block), up to 8 levels deep.
 * 
 * A block is very efficient to run because it is played back by the RSP itself.
 * The CPU just enqueues a single command that "calls" the block. It is thus
 * much faster than enqueuing the same commands every frame.
 * 
 * Blocks can be used by higher-level libraries as an internal tool to efficiently
 * drive the RSP (without having to repeat common sequence of commands), or
 * they can be used by end-users to record and replay batch of commands, similar
 * to OpenGL 1.x display lists.
 * 
 * Notice that this library does not support static (compile-time) blocks.
 * Blocks must always be created at runtime once (eg: at init time) before
 * being used.
 * 
 * ## Syncpoints
 * 
 * The RSP command queue is designed to be fully lockless, but sometimes it is
 * required to know when the RSP has actually executed an enqueued command or
 * not (eg: to use its result). To do so, this library offers a synchronization
 * primitive called "syncpoint" (#rspq_syncpoint_t). A syncpoint can be
 * created via #rspq_syncpoint_new and records the current writing position in the
 * queue. It is then possible to call #rspq_syncpoint_check to check whether
 * the RSP has reached that position, or #rspq_syncpoint_wait to wait for
 * the RSP to reach that position.
 * 
 * Syncpoints are implemented using RSP interrupts, so their overhead is small
 * but still measurable. They should not be abused.
 * 
 * ## High-priority queue
 * 
 * This library offers a mechanism to preempt the execution of RSP to give
 * priority to very urgent tasks: the high-priority queue. Since the
 * moment a high-priority queue is created via #rspq_highpri_begin, the RSP
 * immediately suspends execution of the command queue, and switches to
 * the high-priority queue, waiting for commands. All commands added via
 * standard APIs (#rspq_write) are then directed
 * to the high-priority queue, until #rspq_highpri_end is called. Once the
 * RSP has finished executing all the commands enqueue in the high-priority
 * queue, it resumes execution of the standard queue.
 * 
 * The net effect is that commands enqueued in the high-priority queue are
 * executed right away by the RSP, irrespective to whatever was currently
 * enqueued in the standard queue. This can be useful for running tasks that
 * require immediate execution, like for instance audio processing.
 * 
 * If required, it is possible to call #rspq_highpri_sync to wait for the
 * high-priority queue to be fully executed.
 * 
 * Notice that the RSP cannot be fully preempted, so switching to the high-priority
 * queue can only happen after a command has finished execution (before starting
 * the following one). This can have an effect on latency if a single command
 * has a very long execution time; RSP overlays should in general prefer
 * providing smaller, faster commands.
 * 
 * This feature should normally not be used by end-users, but by libraries
 * in which a very low latency of RSP execution is paramount to their workings.
 * 
 * ## RDP support
 * 
 * RSPQ contains a basic support for sending commands to RDP. It is meant
 * to collaborate with the RDPQ module for full RDP usage (see rdpq.h),
 * but it does provide some barebone support on its own.
 * 
 * In particulare, it allocates and handle two buffers (used with double
 * buffering) that hold RDP commands generated by RSPQ overlays, where 
 * commands are stored to be sent to RDP via DMA.
 * 
 * Overlays that generate RDP commands as part of their duty can call
 * the assembly API RSPQ_RdpSend that will take care of sending the
 * RDP commands via DMA into the RDRAM buffers (possibly swapping them
 * when they are full) and also tell the RDP to run them.
 * 
 * Notice that, while the RSP would allow also to send commands to RDP
 * directly via DMEM, this is deemed as inefficient in the grand picture:
 * DMEM in general is too small and would thus cause frequent stalls
 * (RSP waiting for the RDP to run the commands and buffers to flush);
 * at the same time, it is also hard to efficiently mix and match 
 * RDP buffers in DMEM and RDRAM, as that again can cause excessive
 * stalling. So for the time being, this mode of working is unsupported
 * by RSPQ.
 * 
 */

#ifndef __LIBDRAGON_RSPQ_H
#define __LIBDRAGON_RSPQ_H

#include <stdint.h>
#include <rsp.h>
#include <debug.h>
#include <n64sys.h>
#include <pputils.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum size of a command (in 32-bit words). */
#define RSPQ_MAX_COMMAND_SIZE          62

/** @brief Maximum size of a command that it is writable with #rspq_write
 *         (in 32-bit words).
 *
 * For larger commands, use #rspq_write_begin + #rspq_write_arg + #rspq_write_end.
 */
#define RSPQ_MAX_SHORT_COMMAND_SIZE    16

/**
 * @brief A preconstructed block of commands
 * 
 * To improve performance of execution of sequences of commands, it is possible
 * to create a "block". A block is a fixed set of commands that is created
 * once and executed multiple times.
 * 
 * To create a block, use #rspq_block_begin and #rspq_block_end. After creation,
 * you can use #rspq_block_run at any point to run it. If you do not need the
 * block anymore, use #rspq_block_free to dispose it.
 */
typedef struct rspq_block_s rspq_block_t;

/**
 * @brief A syncpoint in the queue
 * 
 * A syncpoint can be thought of as a pointer to a position in the command queue.
 * After creation, it is possible to later check whether the RSP has reached
 * that position or not.
 * 
 * To create a syncpoint, use #rspq_syncpoint_new that returns a syncpoint that
 * references the current position. Call #rspq_syncpoint_check or #rspq_syncpoint_wait
 * to respectively do a single check or block waiting for the syncpoint to be
 * reached by RSP.
 * 
 * Syncpoints are implemented using interrupts, so they have a light but non
 * trivial overhead. Do not abuse them. For instance, it is reasonable to use
 * tens of syncpoints per frame, but not hundreds or thousands of them.
 * 
 * @note A valid syncpoint is an integer greater than 0.
 */
typedef int rspq_syncpoint_t;

/**
 * @brief Initialize the RSPQ library.
 * 
 * This should be called by the initialization functions of the higher-level
 * libraries using the RSP command queue. It can be safely called multiple
 * times without side effects.
 * 
 * It is not required by applications to call this explicitly in the main
 * function.
 */
void rspq_init(void);

/**
 * @brief Shut down the RSPQ library.
 * 
 * This is mainly used for testing.
 */
void rspq_close(void);


/**
 * @brief Register a rspq overlay into the RSP queue engine.
 * 
 * This function registers a rspq overlay into the queue engine.
 * An overlay is a RSP ucode that has been written to be compatible with the
 * queue engine (see rsp_queue.inc for instructions) and is thus able to 
 * execute commands that are enqueued in the queue. An overlay doesn't have 
 * a single entry point: it exposes multiple functions bound to different 
 * commands, that will be called by the queue engine when the commands are enqueued.
 * 
 * The function returns the overlay ID, which is the ID to use to enqueue
 * commands for this overlay. The overlay ID must be passed to #rspq_write
 * when adding new commands. rspq allows up to 16 overlays to be registered
 * simultaneously, as the overlay ID occupies the top 4 bits of each command.
 * The lower 4 bits specify the command ID, so in theory each overlay could
 * offer a maximum of 16 commands. To overcome this limitation, this function 
 * will reserve multiple consecutive IDs in case an overlay with more than 16
 * commands is registered. These additional IDs are silently occupied and
 * never need to be specified explicitly when queueing commands.
 * 
 * For example if an overlay with 32 commands were registered, this function
 * could return ID 0x60, and ID 0x70 would implicitly be reserved as well.
 * To queue the twenty first command of this overlay, you would write
 * `rspq_write(ovl_id, 0x14, ...)`, where `ovl_id` is the value that was returned
 * by this function.
 *             
 * @param      overlay_ucode  The overlay to register
 *
 * @return     The overlay ID that has been assigned to the overlay. Note that
 *             this value will be preshifted by 28 (eg: 0x60000000 for ID 6),
 *             as this is the expected format used by #rspq_write.
 */
uint32_t rspq_overlay_register(rsp_ucode_t *overlay_ucode);

/**
 * @brief Register an overlay into the RSP queue engine assigning a static ID to it
 * 
 * This function works similar to #rspq_overlay_register, except it will
 * attempt to assign the specified ID to the overlay instead of automatically
 * choosing one. Note that if the ID (or a consecutive IDs) is already used
 * by another overlay, this function will assert, so careful usage is advised.
 * 
 * Assigning a static ID can mostly be useful for debugging purposes.
 * 
 * @param      overlay_ucode  The ucode to register
 * @param      overlay_id     The ID to register the overlay with. This ID
 *                            must be preshifted by 28 (eg: 0x40000000).
 * 
 * @see #rspq_overlay_register
 */
void rspq_overlay_register_static(rsp_ucode_t *overlay_ucode, uint32_t overlay_id);

/**
 * @brief Unregister a ucode overlay from the RSP queue engine.
 * 
 * This function removes an overlay that has previously been registered
 * with #rspq_overlay_register or #rspq_overlay_register_static from the 
 * queue engine. After calling this function, the specified overlay ID 
 * (and consecutive IDs in case the overlay has more than 16 commands) 
 * is no longer valid and must not be used to write new commands into the queue.
 * 
 * Note that when new overlays are registered, the queue engine may recycle 
 * IDs from previously unregistered overlays.
 *             
 * @param      overlay_id  The ID of the ucode (as returned by
 *                         #rspq_overlay_register) to unregister.
 */
void rspq_overlay_unregister(uint32_t overlay_id);

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
 * To avoid race conditions between overlay state access by CPU and RSP, this
 * function first calls #rspq_wait to force a full sync and make sure the RSP is
 * idle. As such, it should be treated as a debugging function.
 *
 * @param      overlay_ucode  The ucode overlay for which the state pointer will be returned.
 *
 * @return     Pointer to the overlay state (in RDRAM). The pointer is returned in
 *             the cached segment, so make sure to handle cache coherency appropriately.
 */
void* rspq_overlay_get_state(rsp_ucode_t *overlay_ucode);

/**
 * @brief Write a new command into the RSP queue.
 * 
 * This macro is the main entry point to add a command to the RSP queue. It can
 * be used as a variadic argument function, in which the first argument is
 * the overlay ID, the second argument is the command index within the overlay,
 * and the other arguments are the command arguments (additional 32-bit words).
 * 
 * @code{.c}
 *      // This example adds to the queue a command called CMD_SPRITE with 
 *      // index 0xA, with its arguments, for a total of three words. The overlay
 *      // was previously registered via #rspq_overlay_register.
 *      
 *      #define CMD_SPRITE  0xA
 *      
 *      rspq_write(gfx_overlay_id, CMD_SPRITE,
 *                 sprite_num, 
 *                 (x0 << 16) | y0,
 *                 (x1 << 16) | y1);
 * @endcode
 * 
 * As explained in the top-level documentation, the overlay ID (4 bits) and the
 * command index (4 bits) are composed to form the command ID, and are stored
 * in the most significant byte of the first word. So, the first command argument
 * word, if provided, must have the upper MSB empty, to leave space
 * for the command ID itself.
 * 
 * For instance, assuming the overlay ID for an overlay called "gfx" is 1, 
 * `rspq_write(gfx_id, 0x2, 0x00FF2233)` is a correct call, which
 * writes `0x12FF2233` into the RSP queue; it will invoke command #2 in
 * overlay with ID 0x1, and the first word contains other 24 bits of arguments
 * that will be parsed by the RSP code. Instead, `rspq_write(gfx_id, 0x2, 0x11FF2233)`
 * is an invalid call because the MSB of the first word is non-zero, and thus
 * the highest byte "0x11" would clash with the overlay ID and command index.
 * 
 * `rspq_write(gfx_id, 0x2)` is a valid call, and equivalent to
 * `rspq_write(gfx_id, 0x2, 0x0)`. It will write `0x12000000` in the queue.
 * 
 * Notice that after a call to #rspq_write, the command might or might not
 * get executed by the RSP, depending on timing. If you want to make sure that
 * the command will be executed, use #rspq_flush. You can call #rspq_flush
 * after you have finished writing a batch of related commands. See #rspq_flush
 * documentation for more information.
 * 
 * #rspq_write allows to write a full command with a single call, which is
 * normally the easiest way to do it; it supports up to 16 argument words.
 * In case it is needed to assemble larger commands, see #rspq_write_begin
 * for an alternative API.
 *
 * @note Each command can be up to #RSPQ_MAX_SHORT_COMMAND_SIZE 32-bit words.
 * 
 * @param      ovl_id    The overlay ID of the command to enqueue. Notice that
 *                       this must be a value preshifted by 28, as returned
 *                       by #rspq_overlay_register.
 * @param      cmd_id    Index of the command to call, within the overlay.
 * @param      ...       Optional arguments for the command
 *
 * @see #rspq_overlay_register
 * @see #rspq_flush
 * @see #rspq_write_begin
 * 
 * @hideinitializer
 */

#define rspq_write(ovl_id, cmd_id, ...) \
    __PPCAT(_rspq_write, __HAS_VARARGS(__VA_ARGS__)) (ovl_id, cmd_id, ##__VA_ARGS__)

/// @cond

// Helpers used to implement rspq_write
#define _rspq_write_prolog() \
    extern volatile uint32_t *rspq_cur_pointer, *rspq_cur_sentinel; \
    extern void rspq_next_buffer(void); \
    volatile uint32_t *ptr = rspq_cur_pointer+1; \
    (void)ptr;

#define _rspq_write_epilog() ({ \
    if (__builtin_expect(rspq_cur_pointer > rspq_cur_sentinel, 0)) \
        rspq_next_buffer(); \
})

#define _rspq_write_arg(arg) \
    *ptr++ = (arg);

#define _rspq_write0(ovl_id, cmd_id) ({ \
    _rspq_write_prolog(); \
    rspq_cur_pointer[0] = (ovl_id) + ((cmd_id)<<24); \
    rspq_cur_pointer += 1; \
    _rspq_write_epilog(); \
})

#define _rspq_write1(ovl_id, cmd_id, arg0, ...) ({ \
    _Static_assert(__COUNT_VARARGS(__VA_ARGS__) <= RSPQ_MAX_SHORT_COMMAND_SIZE, "too many arguments to rspq_write, please use rspq_write_begin/arg/end instead"); \
    _rspq_write_prolog(); \
    __CALL_FOREACH(_rspq_write_arg, ##__VA_ARGS__); \
    rspq_cur_pointer[0] = ((ovl_id) + ((cmd_id)<<24)) | (arg0); \
    rspq_cur_pointer += 1 + __COUNT_VARARGS(__VA_ARGS__); \
    _rspq_write_epilog(); \
})

/// @endcond

/** @brief A write cursor, returned by #rspq_write_begin. */
typedef struct {
    uint32_t first_word;                  ///< value that will be written as first word
    volatile uint32_t *pointer;           ///< current pointer into the RSP queue
    volatile uint32_t *first;             ///< pointer to the first word of the command
    bool is_first;                        ///< true if we are waiting for the first argument word
} rspq_write_t;

/**
 * @brief Begin writing a new command into the RSP queue.
 * 
 * This command initiates a sequence to enqueue a new command into the RSP
 * queue. Call this command passing the overlay ID and command ID of the command
 * to create. Then, call #rspq_write_arg once per each argument word that
 * composes the command. Finally, call #rspq_write_end to finalize and enqueue
 * the command.
 * 
 * A sequence made by #rspq_write_begin, #rspq_write_arg, #rspq_write_end is
 * functionally equivalent to a call to #rspq_write, but it allows to
 * create bigger commands, and might better fit some situations where arguments
 * are calculated on the fly. Performance-wise, the code generated by
 * #rspq_write_begin + #rspq_write_arg + #rspq_write_end should be very similar
 * to a single call to #rspq_write, though just a bit slower. It is advisable
 * to use #rspq_write whenever possible.
 * 
 * Make sure to read the documentation of #rspq_write as well for further
 * details.
 * 
 * @param      ovl_id    The overlay ID of the command to enqueue. Notice that
 *                       this must be a value preshifted by 28, as returned
 *                       by #rspq_overlay_register.
 * @param      cmd_id    Index of the command to call, within the overlay.
 * @param      size      The size of the commands in 32-bit words
 * @returns              A write cursor, that must be passed to #rspq_write_arg
 *                       and #rspq_write_end
 * 
 * @see #rspq_write_arg
 * @see #rspq_write_end
 * @see #rspq_write
 */
inline rspq_write_t rspq_write_begin(uint32_t ovl_id, uint32_t cmd_id, int size) {
    extern volatile uint32_t *rspq_cur_pointer, *rspq_cur_sentinel;
    extern void rspq_next_buffer(void);

    assertf(size <= RSPQ_MAX_COMMAND_SIZE, "The maximum command size is %d!", RSPQ_MAX_COMMAND_SIZE);

    if (__builtin_expect(rspq_cur_pointer > rspq_cur_sentinel - size, 0))
        rspq_next_buffer();

    volatile uint32_t *cur = rspq_cur_pointer;
    rspq_cur_pointer += size;

    return (rspq_write_t){
        .first_word = ovl_id + (cmd_id<<24),
        .pointer = cur + 1,
        .first = cur,
        .is_first = 1
    };
}

/**
 * @brief Add one argument to the command being enqueued.
 * 
 * This function adds one more argument to the command currently being
 * enqueued. This function must be called after #rspq_write_begin; it should
 * be called multiple times (one per argument word), and then #rspq_write_end
 * should be called to terminate enqueuing the command.
 * 
 * See also #rspq_write for a more straightforward API for command enqueuing.
 * 
 * @param       w       The write cursor (returned by #rspq_write_begin)
 * @param       value   New 32-bit argument word to add to the command.
 * 
 * @note The first argument must have its MSB set to 0, to leave space for
 *       the command ID. See #rspq_write documentation for a more complete
 *       explanation.
 * 
 * @see #rspq_write_begin
 * @see #rspq_write_end
 * @see #rspq_write
 */
inline void rspq_write_arg(rspq_write_t *w, uint32_t value) {
    if (w->is_first) {
        w->first_word |= value;
        w->is_first = 0;
    } else {
        *w->pointer++ = value;
    }
}

/**
 * @brief Finish enqueuing a command into the queue.
 * 
 * This function should be called to terminate a sequence for command
 * enqueuing, after #rspq_write_begin and (multiple) calls to #rspq_write_arg.
 * 
 * After calling this command, the write cursor cannot be used anymore.
 * 
 * @param       w       The write cursor (returned by #rspq_write_begin)
 * 
 * @see #rspq_write_begin
 * @see #rspq_write_arg
 * @see #rspq_write
 */
inline void rspq_write_end(rspq_write_t *w) {
    *w->first = w->first_word;
    w->first = 0;
    w->pointer = 0;
}

/**
 * @brief Make sure that RSP starts executing up to the last written command.
 * 
 * RSP processes the command queue asynchronously as it is being written.
 * If it catches up with the CPU, it halts itself and waits for the CPU to
 * notify that more commands are available. On the contrary, if the RSP lags
 * behind it might keep executing commands as they are written without ever
 * sleeping. So in general, at any given moment the RSP could be crunching
 * commands or sleeping waiting to be notified that more commands are available.
 * 
 * This means that writing a command via #rspq_write is not enough to make sure
 * it is executed; depending on timing and batching performed
 * by RSP, it might either be executed automatically or not. #rspq_flush makes
 * sure that the RSP will see it and execute it.
 * 
 * This function does not block: it just make sure that the RSP will run the 
 * full command queue written until now. If you need to actively wait until the
 * last written command has been executed, use #rspq_wait.
 * 
 * It is suggested to call rspq_flush every time a new "batch" of commands
 * has been written. In general, it is not a problem to call it often because
 * it is very very fast (takes only ~20 cycles). For instance, it can be called
 * after every rspq_write without many worries, but if you know that you are
 * going to write a number of subsequent commands in straight line code, you
 * can postpone the call to #rspq_flush after the whole sequence has been written.
 * 
 * @code{.c}
 * 		// This example shows some code configuring the lights for a scene.
 * 		// The command in this sample is called CMD_SET_LIGHT and requires
 * 		// a light index and the RGB colors for the list to update.
 *      uint32_t gfx_overlay_id;   
 *
 * 		#define CMD_SET_LIGHT  0x7
 *
 * 		for (int i=0; i<MAX_LIGHTS; i++) {
 * 			rspq_write(gfx_overlay_id, CMD_SET_LIGHT, i,
 * 			    (lights[i].r << 16) | (lights[i].g << 8) | lights[i].b);
 * 		}
 * 		
 * 		// After enqueuing multiple commands, it is sufficient
 * 		// to call rspq_flush once to make sure the RSP runs them (in case
 * 		// it was idling).
 * 		rspq_flush();
 * @endcode
 *
 * @note This is an experimental API. In the future, it might become 
 *       a no-op, and flushing could happen automatically at every #rspq_write.
 *       We are keeping it separate from #rspq_write while experimenting more
 *       with the RSPQ API.
 * 
 * @note This function is a no-op if it is called while a block is being recorded
 *       (see #rspq_block_begin / #rspq_block_end). This means calling this function
 *       in a block recording context will not guarantee the execution of commands
 *       that were queued prior to starting the block.
 *       
 */
void rspq_flush(void);

/**
 * @brief Wait until all commands in the queue have been executed by RSP.
 *
 * This function blocks until all commands present in the queue have
 * been executed by the RSP and the RSP is idle. If the queue contained also
 * RDP commands, it also waits for those commands to finish drawing.
 * 
 * This function exists mostly for debugging purposes. Calling this function
 * is not necessary, as the CPU can continue adding commands to the queue
 * while the RSP is running them. If you need to synchronize between RSP and CPU
 * (eg: to access data that was processed by RSP) prefer using #rspq_syncpoint_new /
 * #rspq_syncpoint_wait which allows for more granular synchronization.
 */
void rspq_wait(void);

/**
 * @brief Create a syncpoint in the queue.
 * 
 * This function creates a new "syncpoint" referencing the current position
 * in the queue. It is possible to later check when the syncpoint
 * is reached by the RSP via #rspq_syncpoint_check and #rspq_syncpoint_wait.
 *
 * @return     ID of the just-created syncpoint.
 * 
 * @note It is not possible to create a syncpoint within a block because it
 *       is meant to be a one-time event. Otherwise the same syncpoint would
 *       potentially be triggered multiple times, which is not supported.
 * 
 * @note It is not possible to create a syncpoint from the high-priority queue
 *       due to the implementation requiring syncpoints to be triggered
 *       in the same order they have been created.
 * 
 * @see #rspq_syncpoint_t
 * @see #rspq_syncpoint_new_cb
 */
rspq_syncpoint_t rspq_syncpoint_new(void);

/**
 * @brief Create a syncpoint in the queue that triggers a callback on the CPU.
 * 
 * This function is similar to #rspq_syncpoint_new: it creates a new "syncpoint"
 * that references the current position in the queue. When the RSP reaches
 * the syncpoint, it notifies the CPU, that will invoke the provided callback
 * function.
 * 
 * The callback function will be called *outside* of the interrupt context, so
 * that it is safe for instance to call into most the standard library.
 * 
 * The callback function is guaranteed to be called after the RSP has reached
 * the syncpoint, but there is no guarantee on "how much" after. In general
 * the callbacks will be treated as "lower priority" by rspq, so they will
 * be called in best effort.
 * 
 * @param func          Callback function to call when the syncpoint is reached
 * @param arg           Argument to pass to the callback function
 * @return rspq_syncpoint_t     ID of the just-created syncpoint.
 * 
 * @see #rspq_syncpoint_t
 * @see #rspq_syncpoint_new
 */
rspq_syncpoint_t rspq_syncpoint_new_cb(void (*func)(void *), void *arg);

/**
 * @brief Check whether a syncpoint was reached by RSP or not.
 * 
 * This function checks whether a syncpoint was reached. It never blocks.
 * If you need to wait for a syncpoint to be reached, use #rspq_syncpoint_wait
 * instead of polling this function.
 *
 * @param[in]  sync_id  ID of the syncpoint to check
 *
 * @return true if the RSP has reached the syncpoint, false otherwise
 * 
 * @see #rspq_syncpoint_t
 */
bool rspq_syncpoint_check(rspq_syncpoint_t sync_id);

/**
 * @brief Wait until a syncpoint is reached by RSP.
 * 
 * This function blocks waiting for the RSP to reach the specified syncpoint.
 * If the syncpoint was already called at the moment of call, the function
 * exits immediately.
 * 
 * @param[in]  sync_id  ID of the syncpoint to wait for
 * 
 * @see #rspq_syncpoint_t
 */
void rspq_syncpoint_wait(rspq_syncpoint_t sync_id);

/**
 * @brief Enqueue a callback to be called by the CPU
 * 
 * This function enqueues a callback that will be called by the CPU when
 * the RSP has finished all commands put in the queue until now.
 * 
 * An example of a use case for this function is to free resources such as
 * rspq blocks that are no longer needed, but that you want to make sure that
 * are not referenced anymore by the RSP.
 * 
 * See also #rdpq_call_deferred that, in addition to waiting for RSP, it also
 * waits for RDP to process all pending commands before calling the callback.
 * 
 * @note DO NOT CALL RSPQ FUNCTIONS INSIDE THE CALLBACK (including enqueueing
 *       new rspq commands). This might cause a deadlock or corruption, and it
 *       is not supported.
 * 
 * @param func      Callback function
 * @param arg       Argument to pass to the callback
 * 
 * @see #rdpq_call_deferred
 */
inline void rspq_call_deferred(void (*func)(void *), void *arg) {
    rspq_syncpoint_new_cb(func, arg);
    rspq_flush();
}

/**
 * @brief Begin creating a new block.
 * 
 * This function begins writing a command block (see #rspq_block_t).
 * While a block is being written, all calls to #rspq_write
 * will record the commands into the block, without actually scheduling them for
 * execution. Use #rspq_block_end to close the block and get a reference to it.
 * 
 * Only one block at a time can be created. Calling #rspq_block_begin
 * twice (without any intervening #rspq_block_end) will cause an assert.
 *
 * During block creation, the RSP will keep running as usual and
 * execute commands that have been already added to the queue.
 *       
 * @note Calls to #rspq_flush are ignored during block creation, as the RSP
 *       is not going to execute the block commands anyway.
 */
void rspq_block_begin(void);

/**
 * @brief Finish creating a block.
 * 
 * This function completes a block and returns a reference to it (see #rspq_block_t).
 * After this function is called, all subsequent #rspq_write
 * will resume working as usual: they will add commands to the queue
 * for immediate RSP execution.
 * 
 * To run the created block, use #rspq_block_run. 
 *
 * @return A reference to the just created block
 * 
 * @see rspq_block_begin
 * @see rspq_block_run 
 */
rspq_block_t* rspq_block_end(void);

/**
 * @brief Add to the RSP queue a command that runs a block.
 * 
 * This function runs a block that was previously created via #rspq_block_begin
 * and #rspq_block_end. It schedules a special command in the queue
 * that will run the block, so that execution of the block will happen in
 * order relative to other commands in the queue.
 * 
 * Blocks can call other blocks. For instance, if a block A has been fully
 * created, it is possible to call `rspq_block_run(A)` at any point during the
 * creation of a second block B; this means that B will contain the special
 * command that will call A.
 *
 * @param block The block that must be run
 * 
 * @note The maximum depth of nested block calls is 8.
 */
void rspq_block_run(rspq_block_t *block);

/**
 * @brief Free a block that is not needed any more.
 * 
 * After calling this function, the block is invalid and must not be called
 * anymore. Notice that a block that was recently run via #rspq_block_run
 * might still be referenced in the RSP queue, and in that case it is invalid
 * to free it before the RSP has processed it. 
 * 
 * In this case, you must free it once you are absolutely sure that the RSP
 * has processed it (eg: at the end of a frame), or use #rspq_call_deferred 
 * or #rdpq_call_deferred, that handle the synchronization for you. 
 * 
 * @param  block  The block
 * 
 * @note If the block was being called by other blocks, these other blocks
 *       become invalid and will make the RSP crash if called. Make sure
 *       that freeing a block is only done when no other blocks reference it.
 */
void rspq_block_free(rspq_block_t *block);

/**
 * @brief Start building a high-priority queue.
 * 
 * This function enters a special mode in which a high-priority queue is
 * activated and can be filled with commands. After this function has been
 * called, all commands will be put in the high-priority queue, until
 * #rspq_highpri_end is called.
 * 
 * The RSP will start processing the high-priority queue almost instantly
 * (as soon as the current command is done), pausing the normal queue. This will
 * also happen while the high-priority queue is being built, to achieve the
 * lowest possible latency. When the RSP finishes processing the high priority
 * queue (after #rspq_highpri_end closes it), it resumes processing the normal
 * queue from the exact point that was left.
 * 
 * The goal of the high-priority queue is to either schedule latency-sensitive
 * commands like audio processing, or to schedule immediate RSP calculations
 * that should be performed right away, just like they were preempting what
 * the RSP is currently doing.
 * 
 * It is possible to create multiple high-priority queues by calling
 * #rspq_highpri_begin / #rspq_highpri_end multiple times with short
 * delays in-between. The RSP will process them in order. Notice that
 * there is a overhead in doing so, so it might be advisable to keep
 * the high-priority mode active for a longer period if possible. On the
 * other hand, a shorter high-priority queue allows for the RSP to
 * switch back to processing the normal queue before the next one
 * is created.
 * 
 * @note It is not possible to create a block while the high-priority queue is
 *       active. Arrange for constructing blocks beforehand.
 *       
 * @note It is currently not possible to call a block from the
 *       high-priority queue. (FIXME: to be implemented)
 *       
 */
void rspq_highpri_begin(void);

/**
 * @brief Finish building the high-priority queue and close it.
 * 
 * This function terminates and closes the high-priority queue. After this
 * command is called, all following commands will be added to the normal queue.
 * 
 * Notice that the RSP does not wait for this function to be called: it will
 * start running the high-priority queue as soon as possible, even while it is
 * being built.
 */
void rspq_highpri_end(void);

/**
 * @brief Wait for the RSP to finish processing all high-priority queues.
 * 
 * This function will spin-lock waiting for the RSP to finish processing
 * all high-priority queues. It is meant for debugging purposes or for situations
 * in which the high-priority queue is known to be very short and fast to run.
 * Also note that it is not possible to create syncpoints in the high-priority queue.
 */
void rspq_highpri_sync(void);

/**
 * @brief Enqueue a no-op command in the queue.
 * 
 * This function enqueues a command that does nothing. This is mostly
 * useful for debugging purposes.
 */
void rspq_noop(void);

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
 *       this function is always asynchronous as it just adds a command
 *       to the queue.
 */
void rspq_dma_to_rdram(void *rdram_addr, uint32_t dmem_addr, uint32_t len, bool is_async);

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
 *       this function is always asynchronous as it just adds a command
 *       to the queue.
 */
void rspq_dma_to_dmem(uint32_t dmem_addr, void *rdram_addr, uint32_t len, bool is_async);

/** @cond */
__attribute__((deprecated("may not work anymore. use rspq_syncpoint_new/rspq_syncpoint_check instead")))
void rspq_signal(uint32_t signal);
/** @endcond */

#ifdef __cplusplus
}
#endif

#endif
