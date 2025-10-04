/**
 * @file joybus.c
 * @author Christopher Bonhage <me@christopherbonhage.com>
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Joybus Subsystem
 * @ingroup joybus
 */

#include <assert.h>
#include <string.h>

#include "debug.h"
#include "timer.h"
#include "interrupt.h"
#include "joybus.h"
#include "joybus_commands.h"
#include "n64sys.h"
#include "mi.h"
#include "kernel/kernel_internal.h"
#include "regsinternal.h"
#include "kirq.h"

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
    joybus_callback_t callback;                                        ///< callback for completion
    void *context;                                                     ///< callback context
} joybus_msg_t;

#define MAX_JOYBUS_MSGS                   8    ///< Maximum number of pending joybus messages
#define MAX_JOYBUS_DETECTION_CALLBACKS    8    ///< Maximum number of registered detection callbacks

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
/** @brief Init counter */
static int joybus_init_count = 0;
/** @brief Are we in the middle of identifying the devices? */
static volatile bool joybus_identify_pending = false;
/** @brief Is the cached Joybus input block for identifying Joypads valid? */
static volatile uint8_t joybus_identify_input_valid = false;
/** @brief Cached Joybus input block for identifying all Joypads. */
static volatile uint8_t joybus_identify_input[JOYBUS_BLOCK_SIZE] = {0};
/** @brief Joybus identifiers for each port. */
volatile joybus_identifier_t joybus_identifiers_hot[JOYBUS_PORT_COUNT] = {0};
/** @brief Joybus identify status bytes for each port. */
volatile uint8_t joybus_identify_status_hot[JOYBUS_PORT_COUNT] = {0};
/** @brief Timer used to periodically identify the devices. */
static timer_link_t *identify_timer = NULL;

/**
 * @brief Number of ticks between Joybus identify commands.
 *
 * During VI interrupt, the Joypad subsystem will periodically re-identify
 * the connected devices to check if the identifier has changed or if any
 * accessories have been connected/disconnected.
 */
#define JOYBUS_IDENTIFY_INTERVAL_TICKS      (1 * TICKS_PER_SECOND)

/** @brief Detection callback entry */
typedef struct {
    joybus_detection_callback_t callback;       ///< callback function
    void *ctx;                                  ///< callback context
} detection_entry_t;

/** @brief Callbacks for detection events */
static detection_entry_t detection_callbacks[MAX_JOYBUS_DETECTION_CALLBACKS];

static void si_interrupt(void);

/**
 * @brief Initialize the joybus subsystem
 * 
 * NOTE: historically, the low-level part of the joybus subsystem (eg: joybus_exec)
 * has always worked without an init function. So even if a proper #joybus_init
 * exists now, we still need to initialize the low-level part of the subsystem
 * automatically before the first use. This is done via a constructor function.
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

void joybus_exec_async(const void * input, joybus_callback_t callback, void *ctx)
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

    kirq_wait_t w = kirq_begin_wait_si();

    joybus_exec_async(input, callback, NULL);
    while (!done) {
        if (__kernel) {
            kirq_wait(&w);
        } else {
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
}

static void send_detection_event(joybus_identifier_t identifier, joybus_detection_event_t event, int port, uint8_t device_status)
{
    for (int i = 0; i < MAX_JOYBUS_DETECTION_CALLBACKS; i++) {
        if (detection_callbacks[i].callback) {
            detection_callbacks[i].callback(identifier, event, port, device_status, detection_callbacks[i].ctx);
        }
    }
}

void joybus_register_detection_callback(joybus_detection_callback_t callback, void *ctx)
{
    disable_interrupts();
    for (int i = 0; i < MAX_JOYBUS_DETECTION_CALLBACKS; i++) {
        if (detection_callbacks[i].callback == NULL) {
            detection_callbacks[i].callback = callback;
            detection_callbacks[i].ctx = ctx;

            // Notify the callback of all currently connected devices
            for (int port = 0; port < JOYBUS_PORT_COUNT; port++) {
                joybus_identifier_t identifier = joybus_identifiers_hot[port];
                uint8_t device_status = joybus_identify_status_hot[port];
                if (identifier != JOYBUS_IDENTIFIER_NONE) {
                    callback(identifier, JOYBUS_DETECT_CONNECTED, port, device_status, ctx);
                }
            }

            enable_interrupts();
            return;
        }
    }
    assertf(false, "Too many detection callbacks registered");
    enable_interrupts();
}

void joybus_unregister_detection_callback(joybus_detection_callback_t callback, void *ctx)
{
    disable_interrupts();
    for (int i = 0; i < MAX_JOYBUS_DETECTION_CALLBACKS; i++) {
        if (detection_callbacks[i].callback == callback && detection_callbacks[i].ctx == ctx) {
            detection_callbacks[i].callback = NULL;
            detection_callbacks[i].ctx = NULL;
            enable_interrupts();
            return;
        }
    }
    assertf(false, "Callback was not registered");
    enable_interrupts();
}

static void joybus_input_identify( uint8_t input[JOYBUS_BLOCK_SIZE], bool reset )
{
    const joybus_cmd_identify_port_t cmd = { .send = {
        .command = reset ? JOYBUS_COMMAND_ID_RESET : JOYBUS_COMMAND_ID_IDENTIFY,
    } };
    const size_t recv_offset = offsetof(typeof(cmd), recv);
    size_t i = 0;

    // Populate the Joybus commands on each port
    memset(input, 0x00, JOYBUS_BLOCK_SIZE);
    for (int port = 0; port < JOYBUS_PORT_COUNT-1; port++)
    {
        // iQue PIF requires a NOP (0xFF) before each command
        if (sys_bbplayer()) input[i++] = 0xFF;
        // Set the command metadata
        input[i++] = sizeof(cmd.send);
        input[i++] = sizeof(cmd.recv);
        // Micro-optimization: Minimize copy length
        memcpy(&input[i], &cmd, recv_offset);
        i += sizeof(cmd);
        // iQue PIF requires commands to be 8-byte aligned
        if (sys_bbplayer()) while (i & 7) input[i++] = 0xFF;
    }

    // Close out the Joybus operation block
    input[i] = 0xFE;
    input[JOYBUS_BLOCK_SIZE - 1] = 0x01;
}

/**
 * @brief Callback for identifying Joypads.
 * 
 * @param[in] out_dwords Joybus output block.
 * @param[in,out] ctx Not used.
 */
static void joybus_identify_callback(uint64_t *out_dwords, void *ctx)
{
    const uint8_t *out_bytes = (void *)out_dwords;
    const joybus_cmd_identify_port_t *cmd;
    size_t i = 0;

    for (int port = 0; port < JOYBUS_PORT_COUNT; port++)
    {
        if (sys_bbplayer()) {
            // iQue has a very fixed layout for commands, and it also tends
            // to corrupt other parts of PIF-RAM. So better jump to fixed positions
            // while parsing.
            if (port == 4) break;
            i = (port * 8) + 1;
        }

        volatile joybus_identifier_t *old_identifier = &joybus_identifiers_hot[port];
        volatile uint8_t *old_status = &joybus_identify_status_hot[port];

        cmd = (void *)&out_bytes[i + JOYBUS_COMMAND_METADATA_SIZE];

        joybus_identifier_t new_identifier = cmd->recv.identifier;
        uint8_t new_status = cmd->recv.status;
        if (out_bytes[i+1] & 0x80) {
            // If the error flag is set, no device is connected here
            new_identifier = JOYBUS_IDENTIFIER_NONE;
            new_status = 0;
        }

        // Now check if the identifier has changed since last time
        if (new_identifier != *old_identifier)
        {
            if (*old_identifier != JOYBUS_IDENTIFIER_NONE)
                send_detection_event(*old_identifier, JOYBUS_DETECT_DISCONNECTED, port, *old_status);
            if (new_identifier != JOYBUS_IDENTIFIER_NONE)
                send_detection_event(new_identifier, JOYBUS_DETECT_CONNECTED, port, new_status);
        }
        else if (new_identifier != JOYBUS_IDENTIFIER_NONE)
        {
            send_detection_event(new_identifier, JOYBUS_DETECT_POLLED, port, new_status);
        }
        
        *old_identifier = new_identifier;
        *old_status = new_status;
        i += JOYBUS_COMMAND_METADATA_SIZE + sizeof(*cmd);
    }

    joybus_identify_pending = false;
}

/**
 * @brief Identify Joypads asynchronously.
 * 
 * @param reset Whether to reset the devices.
 */
static void joybus_identify_async(bool reset)
{
    // Async operations are disabled during reset
    if( exception_reset_time() > 0 ) { return; }

    // Bail if this operation is already in-progress
    if (joybus_identify_pending) { return; }
    joybus_identify_pending = true;

    uint8_t * const input = (void *)joybus_identify_input;
    // Reset invalidates the cached input block
    if (!joybus_identify_input_valid || reset)
    {
        joybus_input_identify(input, reset);
        // Identify is more common than reset, so don't cache resets
        joybus_identify_input_valid = !reset;
    }

    joybus_exec_async(input, joybus_identify_callback, NULL);
}

static void joybus_identify_wait(void)
{
    kirq_wait_t w = kirq_begin_wait_si();
    while (joybus_identify_pending) {
        if (__kernel)
            kirq_wait(&w);
    }
}

static void timer_callback(int ovfl, void *ctx)
{
    bool reset = (bool)ctx;
    joybus_identify_async(reset);
}

void joybus_init(void)
{
    // NOTE: the actual SI/interrupt initialization is done in __joybus_init()
    // which is a constructor function that runs before main().
    // This function effectively just initializes the periodic polling and
    // background detection system.
    if (joybus_init_count++ > 0) { return; }

    // Start a timer (period 1 second) to periodically identify the devices
    timer_init();
    identify_timer = new_timer_context(JOYBUS_IDENTIFY_INTERVAL_TICKS, TF_CONTINUOUS, timer_callback, (void*)false);

    // Run a first detection pass immediately, so that after joybus_init(),
    // at least we have a first snapshot of the connected devices.
    joybus_identify_async(true);
    joybus_identify_wait();
}

void joybus_close(void)
{
    if (--joybus_init_count > 0) { return; }

    // Stop the identify timer, and deinitialize the timer subsystem
    delete_timer(identify_timer); identify_timer = NULL;
    timer_close();
    
    // Make sure we are not in the middle of an identify operation, otherwise
    // we must wait until it's finished
    joybus_identify_wait();

    // Clear all registered detection callbacks.
    memset(detection_callbacks, 0, sizeof(detection_callbacks));
}

void joybus_detect_now(void)
{
    joybus_identify_async(false);
    joybus_identify_wait();
}

joybus_identifier_t joybus_get_identifier(int port, uint8_t *status)
{
    assert((port >= 0) && (port < JOYBUS_PORT_COUNT));
    joybus_identifier_t identifier;

    // Return a consistent snapshot of identifier and status
    disable_interrupts();
    if (status) *status = joybus_identify_status_hot[port];
    identifier = joybus_identifiers_hot[port];
    enable_interrupts();

    return identifier;
}

extern inline void joybus_exec_cmd(int port, size_t send_len, size_t recv_len, const void *send_data, void *recv_data);
