/**
 * @file joybus.c
 * @brief Joybus Subsystem
 * @ingroup joybus
 */

#include <assert.h>
#include <string.h>

#include "debug.h"
#include "interrupt.h"
#include "joybus.h"
#include "joybus_internal.h"
#include "n64sys.h"
#include "mi.h"
#include "regsinternal.h"

/**
 * @name SI status register bit definitions
 * @{
 */

/** @brief SI DMA busy */
#define SI_STATUS_DMA_BUSY  ( 1 << 0 )
/** @brief SI IO busy */
#define SI_STATUS_IO_BUSY   ( 1 << 1 )
/** @} */

/**
 * @brief Structure used to interact with SI registers.
 */
static volatile struct SI_regs_s * const SI_regs = (struct SI_regs_s *)0xa4800000;

/**
 * @brief Pointer to the memory-mapped location of the PIF RAM.
 */
static void * const PIF_RAM = (void *)0x1fc007c0;

/**
 * @brief A message to be sent to JoyBus, with its completion callback. 
 */
typedef struct {
    uint64_t input[JOYBUS_BLOCK_DWORDS] __attribute__((aligned(16)));  ///< input message
    void (*callback)(uint64_t *output, void *ctx);                     ///< callback for completion
    void *context;                                                     ///< callback context
} joybus_msg_t;

#define MAX_JOYBUS_MSGS            8    ///< Maximum number of pending joybus messages

/**
 * @anchor JOYBUS_STATE
 * @name Joybus internal state machine values
 * 
 * @{
 */
#define JOYBUS_STATE_IDLE          0    ///< Joybus state: idle (no pending messages)
#define JOYBUS_STATE_SENDING       1    ///< JoyBus state: sending a message to PIF
#define JOYBUS_STATE_RECEIVING     2    ///< JoyBus state: receiving a reply from PIF
/** @} */ /* JOYBUS_STATE */

/** @brief Joybus temporary output buffer */
static uint64_t joybus_outbuf[JOYBUS_BLOCK_DWORDS] __attribute__((aligned(16)));
/** @brief Joybus current state (either #JOYBUS_STATE_IDLE, #JOYBUS_STATE_SENDING or #JOYBUS_STATE_RECEIVING) */
static volatile int joybus_state;
/** @brief Joybus pending messages (ring buffer) */
static joybus_msg_t joybus_msgs[MAX_JOYBUS_MSGS];
/** @brief Pending messages write index */
static volatile int msgs_widx;
/** @brief Pending messages read index */
static volatile int msgs_ridx;

static void si_interrupt(void);

/**
 * @brief Initialize the joybus subsystem
 */
__attribute__((constructor))
void __joybus_init(void)
{
    // FIXME: this constructor requires the __init_interrupts constructor to be
    // already run. Since we are not 100% sure of how GCC handles constructor
    // ordering, we call it explicitly here (there's no harm in calling it
    // multiple times anyway). Revisit this after gathering more information
    // on constructor ordering.
    extern void __init_interrupts(void);
    __init_interrupts();

    // Initialize the message ring buffer
    msgs_widx = 0;
    msgs_ridx = 0;
    joybus_state = JOYBUS_STATE_IDLE;

    // Wait for any pending SI write. This can happen mainly because of the
    // write made by entrypoint.S to complete the PIF boot. If the write is still
    // pending, it would trigger a SI interrupt later and cause a crash.
    while (SI_regs->status & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY)) {}

    // Acknowledge any pending SI interrupt
    SI_regs->status = 0;

    // Register our internal interrupt handler
    register_SI_handler(si_interrupt);
    set_SI_interrupt(1);
}


/**
 * @brief Send a joybus messages to the PIF
 * 
 * @note This function must be called with interrupts disabled and SI must be idle
 *
 * @param      msg    Message to send
 */
static void joybus_msg_send(joybus_msg_t *msg) {
    assert((SI_regs->status & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY)) == 0);

    data_cache_hit_writeback(msg->input, JOYBUS_BLOCK_SIZE);
    SI_regs->DRAM_addr = msg->input;
    MEMORY_BARRIER();
    SI_regs->PIF_addr_write = PIF_RAM;
    MEMORY_BARRIER();
    joybus_state = JOYBUS_STATE_SENDING;
}

/**
 * @brief Receive a joybus reply from the PIF
 * 
 * @note This function must be called with interrupts disabled and SI must be idle
 *
 * @param      msg    Message for which a reply is pending
 */
static void joybus_msg_recv(joybus_msg_t *msg) {
    assert((SI_regs->status & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY)) == 0);

    // Start a DMA transfer into the global temporary buffer. We just need
    // one buffer for all messages as there can be only one ongoing joybus
    // message at a time (it is a master/slave protocol).
    data_cache_hit_invalidate(joybus_outbuf, JOYBUS_BLOCK_SIZE);
    SI_regs->DRAM_addr = joybus_outbuf;
    MEMORY_BARRIER();
    SI_regs->PIF_addr_read = PIF_RAM;
    MEMORY_BARRIER();
    joybus_state = JOYBUS_STATE_RECEIVING;
}

/**
 * @brief Check where there are new messages to send
 * 
 * @note This function must be called with interrupts disabled.
 */
static void joybus_poll(void) {
    // If the ring buffer is not empty, fetch the first message and send it
    if (msgs_ridx != msgs_widx) {
        joybus_msg_t *msg = &joybus_msgs[msgs_ridx];
        joybus_msg_send(msg);
        return;
    }

    // Queue is empty, switch to idle state
    joybus_state = JOYBUS_STATE_IDLE;
}

/**
 * @brief SI interrupt handler
 */
static void si_interrupt(void) {
    joybus_msg_t *msg;

    switch (joybus_state) {
    case JOYBUS_STATE_SENDING:
        // Message sending complete. Start receiving the reply
        msg = &joybus_msgs[msgs_ridx];
        joybus_msg_recv(msg);
        return;

    case JOYBUS_STATE_RECEIVING:
        // Reply received. Call the callback
        msg = &joybus_msgs[msgs_ridx];
        if (msg->callback)
            msg->callback(joybus_outbuf, msg->context);

        // Increment read pointer and poll for new messages
        msgs_ridx = (msgs_ridx + 1) % MAX_JOYBUS_MSGS;
        joybus_poll();
        return;

    case JOYBUS_STATE_IDLE:
        // This should never happen! It's a bug in our state machine, or
        // somebody is using the SI interface bypassing the joybus message list,
        // which can cause havoc because they can intermix commands with us
        assertf(false, "Internal error: SI interrupt while joybus state is idle");
        return;
    }
}

/**
 * @brief Execute an asynchronous joybus message.
 * 
 * This function executes an asynchronous joybus protocol exchange, sending
 * a message block (input), and receiving a reply (output). The message is
 * sent in background and a completion function "callback" is called when
 * the output is ready to be processed.
 * 
 * It is possible to schedule multiple joybus messages by calling this
 * function multiple times. They will be automatically executed in order.
 * The maximum number of pending messages at any given time is #MAX_JOYBUS_MSGS.
 * 
 * @note The callback function will be called under interrupt. 
 * 
 * @note This function is not part of the public API yet because we want
 *       to evaluate the use of threading rather than asynchronous programming.
 *       It should only be used internally without exposing a asynchronous
 *       API externally.
 * 
 * @param[in]   input       The input block (must be of JOYBUS_BLOCK_SIZE bytes).
 *                          No specific alignment is required for this data block.
 * @param[in]   callback    A callback completion function that will be called
 *                          when the joybus command is finished. The function
 *                          will receive a pointer to the output buffer and the
 *                          opaque pointer to the callback's context.
 *                          Can be NULL if no callback is required.
 * @param[in]   ctx         Context opaque pointer to pass to the callback.
 *                          Can be NULL if no context is required.
 */
void joybus_exec_async(const void * input, void (*callback)(uint64_t *output, void *ctx), void *ctx)
{
    // Make sure that the task queue is not full. If it is, just assert for now.
    // It is not easy to understand what we should do when the queue is full;
    // blocking would be an option, but if we are under interrupt, we would be
    // deadlocking. So punt for now: we can revisit this later.
    assertf((msgs_widx + 1) % MAX_JOYBUS_MSGS != msgs_ridx,
        "joybus task queue is full");

    disable_interrupts();

    // Write the new task into the ring buffer.
    joybus_msg_t *msg = &joybus_msgs[msgs_widx];
    memcpy(msg->input, input, JOYBUS_BLOCK_SIZE);
    msg->callback = callback;
    msg->context = ctx;

    // Increment the write index. If the joybus subsystem is idle, poll immediately
    // so that we can begin sending the message.
    msgs_widx = (msgs_widx + 1) % MAX_JOYBUS_MSGS;
    if (joybus_state == JOYBUS_STATE_IDLE)
        joybus_poll();

    enable_interrupts();
}

void joybus_exec( const void * input, void * output )
{
    volatile bool done = false;

    void callback(uint64_t *out, void *ctx) {
        memcpy(output, out, JOYBUS_BLOCK_SIZE);
        done = true;
    }

    joybus_exec_async(input, callback, NULL);
    while (!done) {
        // We want the blocking function to also work with interrupts disabled.
        // So while we spin loop, poll SI interrupts manually in case they
        // are disabled.
        disable_interrupts();
        unsigned long status = *MI_INTERRUPT & *MI_MASK;
        if (status & MI_INTERRUPT_SI) {
            SI_regs->status = 0;    // clear interrupt
            si_interrupt();
        }
        enable_interrupts();
    }
}
