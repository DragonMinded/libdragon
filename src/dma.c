/**
 * @file dma.c
 * @author Jennifer Taylor <dragonminded@dragonminded.com>
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief DMA Controller
 * @ingroup dma
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "dma.h"
#include "n64types.h"
#include "n64sys.h"
#include "vaddr64.h"
#include "interrupt.h"
#include "debug.h"
#include "utils.h"
#include "regsinternal.h"
#include "interrupt_internal.h"
#include "kernel/kernel_internal.h"
#include "kirq.h"
#include "mi.h"
#include "accounting_internal.h"

/** @brief Structure used to interact with the PI registers */
static volatile struct PI_regs_s * const PI_regs = (struct PI_regs_s *)0xa4600000;

/** @brief Write this to PI_STATUS to acknowledge the PI interrupt */
#define PI_CLEAR_INTERRUPT          0x02

#define MAX_PI_MSGS                 16    ///< Maximum number of pending PI DMA messages

#define PI_STATE_IDLE               0    ///< PI queue state: idle (no DMA in progress)
#define PI_STATE_RUNNING            1    ///< PI queue state: DMA in progress

#define PI_OP_READ_RAW              0    ///< Read raw bytes from PI bus
#define PI_OP_WRITE_RAW             1    ///< Write raw bytes to PI bus
#define PI_OP_READ_MISALIGNED       2    ///< Read misaligned bytes from PI bus

#define PI_DMA_MAX_LEN              0x1000000UL ///< Maximum length of a PI DMA transfer
#define PI_TICKET_LEN_BITS          24         ///< Number of bits used to encode the transfer length
#define PI_TICKET_LEN_MASK          ((1ULL << PI_TICKET_LEN_BITS) - 1) ///< Mask to extract the transfer length from a ticket
#define PI_TICKET_ID(ticket)        ((ticket) >> PI_TICKET_LEN_BITS) ///< Extract the transfer ID from a ticket
#define PI_TICKET_LEN(ticket)       (((ticket) & PI_TICKET_LEN_MASK) + 1) ///< Extract the transfer length from a ticket

/** @brief A PI DMA message in the pending queue */
typedef struct {
    void *ram;              ///< RDRAM address
    pi_addr_t pi_addr;      ///< PI bus address
    uint32_t len;           ///< Transfer length in bytes
    uint8_t type;           ///< Operation type (#PI_OP_READ_RAW, etc.)
} pi_msg_t;

/** @brief Pending PI DMA messages (ring buffer) */
static pi_msg_t pi_msgs[MAX_PI_MSGS];
/** @brief Pending messages write index */
static volatile int pi_msgs_widx;
/** @brief Pending messages read index */
static volatile int pi_msgs_ridx;
/** @brief PI queue state (#PI_STATE_IDLE or #PI_STATE_RUNNING) */
static volatile int pi_state;
/** @brief Monotonic ticket counter (assigned at enqueue) */
static volatile uint64_t pi_ticket_issued;
/** @brief Ticket counter advanced when a transfer starts */
static volatile uint64_t pi_ticket_started;
/** @brief Ticket counter advanced when a transfer completes */
static volatile uint64_t pi_ticket_done;

static void pi_interrupt(void);
static void pi_poll(void);

static volatile int __dma_busy(void)
{
    return PI_regs->status & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY);
}

static bool __pi_queue_empty(void)
{
    return pi_msgs_widx == pi_msgs_ridx;
}

static bool __pi_queue_full(void)
{
    return (pi_msgs_widx + 1) % MAX_PI_MSGS == pi_msgs_ridx;
}

static void pi_manual_interrupt_poll(void)
{
    // Poll PI interrupts manually. This is required when interrupts are
    // disabled (eg: during dma_wait spin loops), as otherwise the queue
    // would never make progress.
    unsigned long status = *MI_INTERRUPT & *MI_MASK;
    if (status & MI_INTERRUPT_PI) {
        PI_regs->status = PI_CLEAR_INTERRUPT;
        pi_interrupt();
    }
}

static void pi_start_read_raw(void *ram_address, pi_addr_t pi_address, unsigned long len)
{
    assert(len > 0);
    *PI_DRAM_ADDR = PhysicalAddr(ram_address);
    *PI_CART_ADDR = pi_address;
    *PI_WR_LEN = len - 1;
}

static void pi_start_write_raw(const void *ram_address, pi_addr_t pi_address, unsigned long len)
{
    assert(len > 0);
    *PI_DRAM_ADDR = PhysicalAddr(ram_address);
    *PI_CART_ADDR = pi_address;
    *PI_RD_LEN = len - 1;
}

/**
 * @brief Read arbitrary bytes from the PI bus using aligned 32-bit CPU reads.
 *
 * Sub-word CPU accesses to the PI bus are not reliable. Read aligned words
 * through the 64-bit uncached address space and copy out the requested bytes.
 *
 * @note This function must be called with interrupts disabled and PI idle.
 */
static void pi_cpu_read(void *ram_address, pi_addr_t pi_address, unsigned long len)
{
    uint8_t *ram = ram_address;

    while (len) {
        unsigned int offset = pi_address & 3;
        unsigned int count = MIN(len, 4 - offset);
        uint32_t word = sys_vaddr_read32(VirtualUncachedAddr64(pi_address ^ offset));
        const uint8_t *bytes = (const uint8_t*)&word;
        memcpy(ram, bytes + offset, count);

        pi_address += count;
        len -= count;
        ram += count;
    }
}

/**
 * @brief Execute a misaligned read: CPU fixups for the borders, then DMA.
 *
 * Transfers the misaligned head (up to the next 8-byte aligned RAM address)
 * and the trailing odd byte (when the DMA would not handle it) via CPU
 * reads, then starts the DMA transfer of the aligned portion, if any.
 *
 * @note This function must be called with interrupts disabled and PI idle.
 *
 * @return The length of the DMA portion (0 if the whole transfer was done
 *         via CPU and no DMA was started).
 */
static unsigned long pi_exec_read_misaligned(const pi_msg_t *msg)
{
    uint8_t *ram = UncachedAddr(msg->ram);
    pi_addr_t pi_address = msg->pi_addr;
    unsigned long len = msg->len;

    assert(io_accessible(pi_address));

    // Transfer the first bytes manually up until the next 8-byte aligned
    // address. Make sure to not transfer more than requested.
    unsigned long head = MIN(len, (-(uintptr_t)ram) & 7);
    if (head > 0) {
        pi_cpu_read(ram, pi_address, head);
        ram += head;
        pi_address += head;
        len -= head;
    }

    // If there's an odd number of bytes left to transfer, check if the DMA
    // will do that correctly. This happens only if the transfer fits the
    // first DMA block, which is either 127 bytes or up to the end of the
    // current RDRAM row (0x800 bytes).
    int first_block_len = MIN(127, 0x800 - ((uint32_t)ram & 0x7ff));
    if ((len & 1) && len >= first_block_len) {
        // Odd transfers would not work correctly. Transfer the last byte
        // manually.
        pi_cpu_read(ram+len-1, pi_address+len-1, 1);
        len -= 1;
    }

    // Start the actual DMA transfer, if still needed.
    if (len)
        pi_start_read_raw(ram, pi_address, len);
    return len;
}

/**
 * @brief Mark the current transfer as complete and advance the queue.
 *
 * @note This function must be called with interrupts disabled.
 */
static void pi_complete_current(void)
{
    pi_ticket_done++;
    pi_msgs_ridx = (pi_msgs_ridx + 1) % MAX_PI_MSGS;
    pi_state = PI_STATE_IDLE;
}

/**
 * @brief Start executing a queued PI DMA transfer.
 *
 * @note This function must be called with interrupts disabled and PI idle.
 */
static void pi_exec_entry(pi_msg_t *msg)
{
    assert((PI_regs->status & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY)) == 0);

    pi_state = PI_STATE_RUNNING;

    switch (msg->type) {
    case PI_OP_READ_RAW:
        pi_start_read_raw(msg->ram, msg->pi_addr, msg->len);
        break;

    case PI_OP_WRITE_RAW:
        pi_start_write_raw(msg->ram, msg->pi_addr, msg->len);
        break;

    case PI_OP_READ_MISALIGNED:
        // Run the CPU fixups for the borders, then start the DMA of the
        // aligned portion. If the whole transfer was done via CPU, complete
        // the entry inline (no interrupt will come).
        if (pi_exec_read_misaligned(msg) == 0) {
            pi_ticket_started++;
            pi_complete_current();
            return;
        }
        break;

    default:
        assertf(false, "Internal error: unknown PI queue entry type");
    }

    // Publish the started state only after all synchronous CPU fixups have
    // completed and the hardware DMA has been launched.
    pi_ticket_started++;
}

/**
 * @brief Check whether there are new messages to send.
 *
 * @note This function must be called with interrupts disabled.
 */
static void pi_poll(void)
{
    // Execute entries until one starts an actual DMA. Entries which are
    // fully executed via CPU (tiny misaligned reads) complete inline, so
    // we must keep going to the next one.
    while (pi_state == PI_STATE_IDLE && !__pi_queue_empty() && !__dma_busy())
        pi_exec_entry(&pi_msgs[pi_msgs_ridx]);
}

/**
 * @brief PI interrupt handler.
 */
static void pi_interrupt(void)
{
    if (pi_state == PI_STATE_RUNNING) {
        pi_complete_current();
        pi_poll();
        return;
    }

    // External PI DMA (eg: iQue NAND). Poll the queue in case a transfer
    // was enqueued while the PI was busy.
    pi_poll();
}

static uint64_t pi_enqueue(uint8_t type, void *ram, pi_addr_t pi_addr, unsigned long len)
{
    assert(len > 0 && len <= PI_DMA_MAX_LEN);
    assert(pi_ticket_issued < (1ULL << (64 - PI_TICKET_LEN_BITS)) - 1);

    kirq_wait_t w = kirq_begin_wait_pi();

    disable_interrupts();

    // If the queue is full, wait until a slot is freed by the interrupt
    // handler. Poll the PI interrupt manually so that this also works when
    // the caller has interrupts disabled.
    if (UNLIKELY(__pi_queue_full())) {
        ACCT_SCOPE(ACCT_CAT_PI) while (UNLIKELY(__pi_queue_full())) {
            pi_manual_interrupt_poll();
            if (!__pi_queue_full())
                break;
            enable_interrupts();
            if (__kernel)
                kirq_wait(&w);
            disable_interrupts();
        }
    }

    // Write the new message into the ring buffer.
    pi_msg_t *msg = &pi_msgs[pi_msgs_widx];
    msg->ram = ram;
    msg->pi_addr = pi_addr;
    msg->len = len;
    msg->type = type;

    pi_msgs_widx = (pi_msgs_widx + 1) % MAX_PI_MSGS;
    uint64_t ticket = (++pi_ticket_issued << PI_TICKET_LEN_BITS) | (len - 1);

    // If the PI is idle, start the transfer immediately. For misaligned
    // reads, this also runs the CPU fixups right now, in caller context.
    if (pi_state == PI_STATE_IDLE && !__dma_busy())
        pi_poll();

    enable_interrupts();
    return ticket;
}

bool io_accessible(pi_addr_t pi_address)
{
    // Below 0x0500_0000, there is RDRAM and RCP registers.
    if (pi_address < 0x05000000)
        return false;

    // The SI bus is partially covering the PI range in the CPU memory map
    if (pi_address >= 0x1FC00000 && pi_address <= 0x1FCFFFFF)
        return false;

    // Upper half of the PI range is not memory mapped
    if (pi_address >= 0x80000000)
        return false;

    // All other addresses are memory mapped and can be accessed via CPU.
    return true;
}

/**
 * @brief Return whether the DMA controller is currently busy
 *
 * @return nonzero if the DMA controller is busy or 0 otherwise
 */
volatile int dma_busy(void)
{
    disable_interrupts();
    int busy = !__pi_queue_empty() || __dma_busy();
    enable_interrupts();
    return busy;
}

uint64_t dma_read_raw_async(void *ram_address, pi_addr_t pi_address, unsigned long len)
{
    return pi_enqueue(PI_OP_READ_RAW, ram_address, pi_address, len);
}

uint64_t dma_write_raw_async(const void *ram_address, pi_addr_t pi_address, unsigned long len)
{
    return pi_enqueue(PI_OP_WRITE_RAW, (void*)ram_address, pi_address, len);
}

uint64_t dma_read_async(void *ram_pointer, pi_addr_t pi_address, unsigned long len)
{
    void *ram = UncachedAddr(ram_pointer);
    uint32_t ram_address = (uint32_t)ram;

    assert(len > 0);
    assert(((ram_address ^ pi_address) & 1) == 0);

    if (!io_accessible(pi_address)) {
        // Check if the PI address can be accessed with CPU.
        // If not, we cannot perform a misaligned transfer.
        assertf((pi_address & 2) == 0 && (ram_address & 7) == 0,
            "misaligned transfer not supported at this PI address");
        return dma_read_raw_async(ram_pointer, pi_address, len);
    }

    // Misaligned reads are executed at dequeue time: the CPU fixups for the
    // borders run either right now in caller context (if the queue is empty
    // and PI is idle) or in the interrupt handler when the entry is dequeued.
    if ((ram_address & 7) || ((len & 1) && len >= MIN(127, 0x800 - (ram_address & 0x7ff))))
        return pi_enqueue(PI_OP_READ_MISALIGNED, ram_pointer, pi_address, len);

    return dma_read_raw_async(ram_pointer, pi_address, len);
}

bool dma_finished(uint64_t ticket)
{
    disable_interrupts();
    // Poll the PI interrupt manually, so that this function makes progress
    // even when called with interrupts disabled.
    pi_manual_interrupt_poll();
    bool done = ticket && PI_TICKET_ID(ticket) <= pi_ticket_done;
    enable_interrupts();
    return done;
}

static bool dma_started(uint64_t ticket)
{
    disable_interrupts();
    // Poll the PI interrupt manually, so that this function makes progress
    // even when called with interrupts disabled.
    pi_manual_interrupt_poll();
    bool started = ticket && PI_TICKET_ID(ticket) <= pi_ticket_started;
    enable_interrupts();
    return started;
}

unsigned long dma_get_progress(uint64_t ticket)
{
    if (!ticket)
        return 0;

    disable_interrupts();
    // Poll the PI interrupt manually, so that this function makes progress
    // even when called with interrupts disabled.
    pi_manual_interrupt_poll();

    uint64_t id = PI_TICKET_ID(ticket);
    unsigned long len = PI_TICKET_LEN(ticket);
    unsigned long progress = 0;

    if (id <= pi_ticket_done) {
        // The PI registers might already refer to a later transfer.
        progress = len;
    } else if (id == pi_ticket_started) {
        // This is the transfer currently running. PI_DRAM_ADDR tracks the
        // current RDRAM pointer for both reads and writes.
        pi_msg_t *msg = &pi_msgs[pi_msgs_ridx];
        uint32_t start = PhysicalAddr(msg->ram);
        uint32_t current = *PI_DRAM_ADDR;
        if (current >= start)
            progress = MIN(current - start, len);
    }

    enable_interrupts();
    return progress;
}

void dma_wait_started(uint64_t ticket)
{
    assert(ticket);

    kirq_wait_t w = kirq_begin_wait_pi();

    // Keep the common case outside the accounting scope.
    if (LIKELY(dma_started(ticket)))
        return;

    ACCT_SCOPE(ACCT_CAT_PI) while (!dma_started(ticket)) {
        if (__kernel)
            kirq_wait(&w);
    }
}

void dma_wait_finished(uint64_t ticket)
{
    assert(ticket);

    kirq_wait_t w = kirq_begin_wait_pi();

    // Keep the common case outside the accounting scope.
    if (LIKELY(dma_finished(ticket)))
        return;

    ACCT_SCOPE(ACCT_CAT_PI) while (!dma_finished(ticket)) {
        if (__kernel)
            kirq_wait(&w);
    }
}

__attribute__((noinline, warn_unused_result))
static uint32_t dma_wait_and_disable_interrupts(void)
{
    kirq_wait_t w = kirq_begin_wait_pi();
    uint32_t sr = __disable_interrupts();

    // Poll the PI interrupt manually, so that this function makes progress
    // even when called with interrupts disabled.
    pi_manual_interrupt_poll();
    if (LIKELY(__pi_queue_empty() && !__dma_busy()))
        return sr;
    __enable_interrupts(sr);

    ACCT_SCOPE(ACCT_CAT_PI) while (1) {
        if (__kernel)
            kirq_wait(&w);

        sr = __disable_interrupts();
        pi_manual_interrupt_poll();
        if (__pi_queue_empty() && !__dma_busy())
            break;
        __enable_interrupts(sr);
    }
    return sr;
}

void dma_wait(void)
{
    uint32_t sr = dma_wait_and_disable_interrupts();
    __enable_interrupts(sr);
}

void dma_read(void *ram_address, pi_addr_t pi_address, unsigned long len)
{
    // HORROR: this code makes no sense, but it's always been here. The original
    // goal was to convert virtual addresses to PI addresses, but it is also
    // preventing a large span of the PI address space from being used.
    pi_address = (pi_address | 0x10000000) & 0x1FFFFFFF;
    dma_read_async(ram_address, pi_address, len);
    dma_wait();
}

void dma_write(const void * ram_address, pi_addr_t rom_address, unsigned long len)
{
    // HORROR: this code makes no sense, but it's always been here. The original
    // goal was to make virtual addresses to PI addresses, but it is also
    // preventing a large span of the PI address space from being used.
    rom_address = (rom_address | 0x10000000) & 0x1FFFFFFF;
    dma_write_raw_async(ram_address, rom_address, len);
    dma_wait();
}

uint32_t io_read(pi_addr_t pi_address)
{
    // HACK: to maintain backward compatibility, this function used to accept
    // also CPU virtual addresses. To still allow for that, we need to convert
    // them to physical addresses first.
    if (UNLIKELY(pi_address >= 0x80000000 && pi_address <= 0xBFFFFFFF)) {
        debugf("io_read: WARNING: deprecated usage of virtual address: %08lX\n", pi_address);
        pi_address = PhysicalAddr((void*)pi_address);
    }

    // Convert the PI address into a 64-bit virtual address, which allows a wider
    // range of PI addresses to be accessed.
    vaddr64_t va64 = VirtualUncachedAddr64(pi_address);

    uint32_t sr = dma_wait_and_disable_interrupts();
    uint32_t retval = sys_vaddr_read32(va64);
    __enable_interrupts(sr);

    return retval;
}

void io_write(pi_addr_t pi_address, uint32_t data)
{
    // HACK: to maintain backward compatibility, this function used to accept
    // also CPU virtual addresses. To still allow for that, we need to convert
    // them to physical addresses first. Keep this undocumented though, as we
    // want to deprecate this behavior.
    if (UNLIKELY(pi_address >= 0x80000000 && pi_address <= 0xBFFFFFFF)) {
        debugf("io_write: WARNING: deprecated usage of virtual address: %08lX\n", pi_address);
        pi_address = PhysicalAddr((void*)pi_address);
    }

    // Convert the PI address into a 64-bit virtual address, which allows a wider
    // range of PI addresses to be accessed.
    vaddr64_t va64 = VirtualUncachedAddr64(pi_address);

    uint32_t sr = dma_wait_and_disable_interrupts();
    sys_vaddr_write32(va64, data);
    __enable_interrupts(sr);
}

/**
 * @brief Initialize the PI DMA queue subsystem.
 *
 * NOTE: the DMA subsystem requires no explicit init function from the user.
 * This constructor runs before main().
 */
__attribute__((constructor))
void __dma_init(void)
{
    pi_msgs_widx = 0;
    pi_msgs_ridx = 0;
    pi_state = PI_STATE_IDLE;
    pi_ticket_issued = 0;
    pi_ticket_started = 0;
    pi_ticket_done = 0;

    register_PI_handler(pi_interrupt);
    set_PI_interrupt(1);
}