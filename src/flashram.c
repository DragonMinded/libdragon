/**
 * @file flashram.c
 * @brief FlashRAM access functions for N64 cartridges
 *
 * FlashRAM is a 1 Mibit (128 KiB) Macronix- or Matsushita-family NOR flash used
 * as save storage on some N64 cartridges. It shares the PI DOM2 window with
 * cartridge SRAM (a cart has one or the other, never both), but is driven
 * through a command state machine rather than being flat memory:
 *
 *   - Command register at #FLASHRAM_ADDRESS | 0x10000: 32-bit command words are
 *     written here as (opcode << 24) | argument, where the argument (for
 *     erase/program) is a *page number*, not a byte offset.
 *   - Data window at #FLASHRAM_ADDRESS: reads back the status/ID latch or the
 *     array, depending on the current mode -- a CPU read returns the latch in
 *     status/identify mode and array data in read-array mode. This driver moves
 *     array data with DMA throughout, which uniformly handles the word-index
 *     address halving and the 2-byte transfer unit.
 *
 * Two families of chip exist and differ in how they address their array:
 * "byte-indexed" parts address individual bytes (like SRAM), while "older"
 * word-indexed parts (e.g. MX29L1100) address 16-bit words, so a logical byte
 * offset must be halved to reach the right word. The addressing mode is a fixed
 * property of the silicon; flashram_detect() looks it up from the silicon ID
 * and caches it, and the read path adapts accordingly. Because PGS cannot
 * express the word-indexed "divide by 2", we set PGS to its maximum (never
 * auto-split) and split every DMA manually at the 256-page boundary.
 *
 * The command protocol implemented here matches real hardware (as exercised by
 * libultra and homebrew such as N64-SwapDumper) and is compatible with both
 * SC64 and the ares emulator.
 */

#include "flashram.h"
#include "debug.h"
#include "dma.h"
#include "interrupt.h"
#include "n64sys.h"
#include <malloc.h>
#include <string.h>

/// @cond
#define PI_BSD_DOM2_LAT ((volatile uint32_t*) 0xA4600024)
#define PI_BSD_DOM2_PWD ((volatile uint32_t*) 0xA4600028)
#define PI_BSD_DOM2_PGS ((volatile uint32_t*) 0xA460002C)
#define PI_BSD_DOM2_RLS ((volatile uint32_t*) 0xA4600030)

/// Command register address (offset 0x10000 into the FlashRAM window).
#define FLASHRAM_COMMAND_ADDRESS (FLASHRAM_ADDRESS | 0x00010000)

/// FLASH_TYPE_ID word (silicon ID bits [63:32]) expected on all known FlashRAM parts.
#define FLASHRAM_IDENTIFIER      0x11118001

// Command opcodes (written as (opcode) or (opcode | page_number)).
#define FLASHRAM_CMD_STATUS_MODE     0xD2000000  ///< Enter status mode.
#define FLASHRAM_CMD_IDENTIFY_MODE   0xE1000000  ///< Enter identify (silicon ID) mode.
#define FLASHRAM_CMD_READ_MODE       0xF0000000  ///< Enter array read mode.
#define FLASHRAM_CMD_SECTOR_ERASE    0x4B000000  ///< Arm sector erase at page (| page).
#define FLASHRAM_CMD_EXECUTE_ERASE   0x78000000  ///< Execute the armed erase.
#define FLASHRAM_CMD_LOAD_BYTE_PAGE  0xB4000000  ///< Enter load-byte-page mode (fill the page buffer).
#define FLASHRAM_CMD_PROGRAM_PAGE    0xA5000000  ///< Program the page buffer into page (| page).

// Status register bits (the register is 8-bit; the window returns 00 <status>).
#define FLASHRAM_STATUS_PROGRAM_BUSY 0x01  ///< A page program is in progress.
#define FLASHRAM_STATUS_ERASE_BUSY   0x02  ///< A sector/chip erase is in progress.
#define FLASHRAM_STATUS_PROGRAM_OK   0x04  ///< The last page program succeeded.
#define FLASHRAM_STATUS_ERASE_OK     0x08  ///< The last erase succeeded.

// Blocking poll timeouts (milliseconds).
#define FLASHRAM_PROGRAM_TIMEOUT_MS  1000
#define FLASHRAM_ERASE_TIMEOUT_MS    3000

#define FLASHRAM_PAGES_PER_SECTOR (FLASHRAM_SECTOR_SIZE / FLASHRAM_PAGE_SIZE)

/// A single DMA cannot cross this many logical bytes (the 256-page wrap
/// boundary). It is 0x8000 logical bytes for both addressing modes: a
/// word-indexed part's 0x4000 PI-address boundary maps to 0x8000 logical.
#define FLASHRAM_DMA_BOUNDARY 0x8000
/// @endcond

/// One row of the silicon-ID lookup table.
typedef struct
{
    uint16_t              manufacturer_id;
    uint16_t              device_id;
    const char*           name;
    flashram_addressing_t addressing;
} flashram_model_t;

/// Known FlashRAM parts. Byte- vs word-addressing is a fixed property we cannot
/// probe at runtime, so it must be looked up by manufacturer/device ID.
static const flashram_model_t FLASHRAM_MODELS[] = {
    { 0x00C2, 0x0000, "MX29L0000",   FLASHRAM_ADDRESSING_WORD },  // Macronix
    { 0x00C2, 0x0001, "MX29L0001",   FLASHRAM_ADDRESSING_WORD },
    { 0x00C2, 0x001E, "MX29L1100",   FLASHRAM_ADDRESSING_WORD },
    { 0x00C2, 0x001D, "MX29L1101_A", FLASHRAM_ADDRESSING_BYTE },
    { 0x00C2, 0x0084, "MX29L1101_B", FLASHRAM_ADDRESSING_BYTE },
    { 0x00C2, 0x008E, "MX29L1101_C", FLASHRAM_ADDRESSING_BYTE },
    { 0x0032, 0x00F1, "MN63F8MPN",   FLASHRAM_ADDRESSING_BYTE },  // Matsushita
};

/// True if flashram_init() has been called.
static bool __flashram_inited = false;

/// Cached identity/layout of the detected chip. Defaults to a byte-indexed
/// 1 Mibit part so the read path is safe even before flashram_detect() runs.
static flashram_info_t __flashram_info = {
    .type_id         = 0,
    .manufacturer_id = 0,
    .device_id       = 0,
    .addressing      = FLASHRAM_ADDRESSING_BYTE,
    .name            = "unknown",
    .total_size      = FLASHRAM_SIZE,
    .sector_size     = FLASHRAM_SECTOR_SIZE,
    .page_size       = FLASHRAM_PAGE_SIZE,
    .num_sectors     = FLASHRAM_NUM_SECTORS,
    .num_pages       = FLASHRAM_NUM_PAGES,
};

/// Write a single command word to the FlashRAM command register.
static inline void flashram_command(uint32_t command)
{
    io_write(FLASHRAM_COMMAND_ADDRESS, command);
}

/// Write a command word twice. Some parts (notably MX29L1100) need two CIR
/// writes to actually switch to status or silicon-ID mode; a single write can
/// leave the previous mode latched. Read-array/erase/program need only one.
static inline void flashram_command_twice(uint32_t command)
{
    io_write(FLASHRAM_COMMAND_ADDRESS, command);
    io_write(FLASHRAM_COMMAND_ADDRESS, command);
}

/// PI-bus address of logical byte @p offset for the detected addressing mode.
static inline uint32_t flashram_pi_address(size_t offset)
{
    if (__flashram_info.addressing == FLASHRAM_ADDRESSING_WORD)
        return FLASHRAM_ADDRESS + (uint32_t) (offset >> 1);
    return FLASHRAM_ADDRESS + (uint32_t) offset;
}

uint8_t flashram_status(void)
{
    assertf(__flashram_inited, "flashram accessed, but flashram_init() hasn't been called yet");
    flashram_command_twice(FLASHRAM_CMD_STATUS_MODE);
    // The window returns the pattern 00 <status>; the upper bits are meaningless
    // (and on MX29L1100 hold leftovers from the previous mode), so keep the low byte.
    return (uint8_t) io_read(FLASHRAM_ADDRESS);
}

void flashram_clear_status(void)
{
    assertf(__flashram_inited, "flashram accessed, but flashram_init() hasn't been called yet");
    flashram_command_twice(FLASHRAM_CMD_STATUS_MODE);
    // Reset the ERASE_OK / PROGRAM_OK latch by writing 0 at the array origin.
    io_write(FLASHRAM_ADDRESS, 0);
}

/// Poll the status byte until @p busy_mask clears; returns whether @p ok_mask is then set.
static bool flashram_wait_ready(uint8_t busy_mask, uint8_t ok_mask, uint32_t timeout_ms)
{
    // The erase/program that preceded this call left the chip in status mode
    // automatically, so read the status byte directly. Re-issuing the status
    // command each poll is unnecessary and can clear the OK latch on some parts.
    uint64_t start_ms = get_ticks_ms();
    while (true)
    {
        uint8_t status = (uint8_t) io_read(FLASHRAM_ADDRESS);
        if ((status & busy_mask) == 0)
        {
            return (status & ok_mask) == ok_mask;
        }
        if ((get_ticks_ms() - start_ms) > timeout_ms)
        {
            return false;
        }
        wait_ms(1);
    }
}

void flashram_init(void)
{
    if (__flashram_inited)
        return;

    // Configure PI DOM2 registers to enable access to FlashRAM.
    disable_interrupts();
    *PI_BSD_DOM2_LAT = 0x40;  // latch: FlashRAM needs a longer latch than SRAM (0x40-0x50)
    *PI_BSD_DOM2_PWD = 0x0c;  // pulse width
    *PI_BSD_DOM2_PGS = 0x0f;  // max page size: never auto-split -- we split every DMA manually
    *PI_BSD_DOM2_RLS = 0x02;  // release
    enable_interrupts();

    __flashram_inited = true;

    // Leave the chip in a known (read) state.
    flashram_command(FLASHRAM_CMD_READ_MODE);
}

bool flashram_detect(flashram_info_t* info)
{
    assertf(__flashram_inited, "flashram accessed, but flashram_init() hasn't been called yet");

    // Enter identify mode and DMA out the two silicon-ID words. DMA is mandatory
    // here: a CPU read of the ID window returns only the first 32-bit word twice.
    uint32_t id[2] __attribute__((aligned(16))) = {0};
    flashram_command_twice(FLASHRAM_CMD_IDENTIFY_MODE);
    data_cache_hit_writeback_invalidate(id, sizeof(id));
    dma_read_raw_async(id, FLASHRAM_ADDRESS, sizeof(id));
    dma_wait();

    // Restore read mode so a later CPU/DMA read returns array data.
    flashram_command(FLASHRAM_CMD_READ_MODE);

    // SILICON_ID: [63:32] FLASH_TYPE_ID, [31:16] MANUFACTURER_ID, [15:0] DEVICE_ID.
    uint32_t type_id = id[0];
    if (type_id != FLASHRAM_IDENTIFIER)
        return false;

    uint16_t manufacturer_id = (uint16_t) (id[1] >> 16);
    uint16_t device_id       = (uint16_t) (id[1] & 0xFFFF);

    // Look up the model to learn its addressing mode. Default to byte-indexed
    // (the common flashcart/emulator convention) for parts not in the table.
    const char* name = "unknown";
    flashram_addressing_t addressing = FLASHRAM_ADDRESSING_BYTE;
    for (size_t i = 0; i < sizeof(FLASHRAM_MODELS) / sizeof(FLASHRAM_MODELS[0]); i++)
    {
        if (FLASHRAM_MODELS[i].manufacturer_id == manufacturer_id &&
            FLASHRAM_MODELS[i].device_id == device_id)
        {
            name = FLASHRAM_MODELS[i].name;
            addressing = FLASHRAM_MODELS[i].addressing;
            break;
        }
    }

    __flashram_info.type_id         = type_id;
    __flashram_info.manufacturer_id = manufacturer_id;
    __flashram_info.device_id       = device_id;
    __flashram_info.addressing      = addressing;
    __flashram_info.name            = name;
    // Geometry fields keep their static defaults (fixed 1 Mibit layout).

    if (info)
        *info = __flashram_info;
    return true;
}

int flashram_read(void* dst, size_t offset, size_t len)
{
    assertf(__flashram_inited, "flashram accessed, but flashram_init() hasn't been called yet");

    assertf(offset + len <= FLASHRAM_SIZE,
            "flashram_read out of range: offset=0x%X len=0x%X (size=0x%X)",
            (unsigned) offset, (unsigned) len, (unsigned) FLASHRAM_SIZE);
    if (len == 0)
        return 0;
    // PI DMA moves the array over the 2-byte bus, so the offset must be even.
    assertf((offset & 1) == 0, "flashram_read requires an even offset, got 0x%X", (unsigned) offset);

    flashram_command(FLASHRAM_CMD_READ_MODE);

    uint8_t bounce[512] __attribute__((aligned(16)));
    uint8_t* out = (uint8_t*) dst;
    size_t done = 0;

    while (done < len)
    {
        size_t abs_off = offset + done;
        // Clamp this DMA so it neither exceeds the request nor crosses the
        // 256-page (0x8000 logical byte) boundary that a single DMA cannot span.
        size_t to_boundary = FLASHRAM_DMA_BOUNDARY - (abs_off & (FLASHRAM_DMA_BOUNDARY - 1));
        size_t chunk = len - done;
        if (chunk > to_boundary)
            chunk = to_boundary;

        uint32_t pi = flashram_pi_address(abs_off);

        // Fast path: aligned destination and even length -> DMA straight in.
        if ((((uintptr_t) (out + done) & 7) == 0) && ((chunk & 1) == 0))
        {
            data_cache_hit_writeback_invalidate(out + done, chunk);
            dma_read_raw_async(out + done, pi, chunk);
            dma_wait();
            done += chunk;
            continue;
        }

        // Slow path (unaligned dst or odd length): DMA into an aligned bounce,
        // then copy out. We keep the whole array path on DMA -- a CPU read would
        // return a big-endian 32-bit word needing manual byte assembly here.
        if (chunk > sizeof(bounce))
            chunk = sizeof(bounce);
        size_t dma_len = (chunk + 1) & ~(size_t) 1;  // round up to the 2-byte DMA unit
        if (abs_off + dma_len > FLASHRAM_SIZE)
            dma_len = FLASHRAM_SIZE - abs_off;

        data_cache_hit_writeback_invalidate(bounce, dma_len);
        dma_read_raw_async(bounce, pi, dma_len);
        dma_wait();

        memcpy(out + done, bounce, chunk);
        done += chunk;
    }
    return (int) len;
}

/// True if all #FLASHRAM_PAGE_SIZE bytes at @p page are 0xFF (already erased).
static bool flashram_page_is_erased(const uint8_t* page)
{
    for (size_t i = 0; i < FLASHRAM_PAGE_SIZE; i++)
    {
        if (page[i] != 0xFF)
            return false;
    }
    return true;
}

bool flashram_erase_sector(unsigned int sector)
{
    assertf(__flashram_inited, "flashram accessed, but flashram_init() hasn't been called yet");

    assertf(sector < FLASHRAM_NUM_SECTORS,
            "flashram_erase_sector: sector %u out of range (0..%u)",
            sector, (unsigned) (FLASHRAM_NUM_SECTORS - 1));

    unsigned int page = sector * FLASHRAM_PAGES_PER_SECTOR;
    flashram_command(FLASHRAM_CMD_SECTOR_ERASE | page);
    flashram_command(FLASHRAM_CMD_EXECUTE_ERASE);
    bool ok = flashram_wait_ready(FLASHRAM_STATUS_ERASE_BUSY, FLASHRAM_STATUS_ERASE_OK,
                                  FLASHRAM_ERASE_TIMEOUT_MS);
    // Clear the OK/error latch so the next operation's result is not masked.
    flashram_clear_status();
    return ok;
}

bool flashram_program_page(unsigned int page, const void* data)
{
    assertf(__flashram_inited, "flashram accessed, but flashram_init() hasn't been called yet");

    assertf(page < FLASHRAM_NUM_PAGES,
            "flashram_program_page: page %u out of range (0..%u)",
            page, (unsigned) (FLASHRAM_NUM_PAGES - 1));

    // Copy into an aligned bounce so callers can pass any alignment.
    uint8_t buffer[FLASHRAM_PAGE_SIZE] __attribute__((aligned(16)));
    memcpy(buffer, data, FLASHRAM_PAGE_SIZE);

    // Load the 128-byte page buffer, then program it into the target page.
    flashram_command(FLASHRAM_CMD_LOAD_BYTE_PAGE);
    data_cache_hit_writeback(buffer, FLASHRAM_PAGE_SIZE);
    dma_write_raw_async(buffer, FLASHRAM_ADDRESS, FLASHRAM_PAGE_SIZE);
    dma_wait();
    flashram_command(FLASHRAM_CMD_PROGRAM_PAGE | page);
    bool ok = flashram_wait_ready(FLASHRAM_STATUS_PROGRAM_BUSY, FLASHRAM_STATUS_PROGRAM_OK,
                                  FLASHRAM_PROGRAM_TIMEOUT_MS);
    // Clear the OK/error latch so the next operation's result is not masked.
    flashram_clear_status();
    return ok;
}

/// Erase @p sector then program its pages from the 16 KiB @p content buffer.
static bool flashram_write_sector(unsigned int sector, const uint8_t* content)
{
    if (!flashram_erase_sector(sector))
        return false;

    unsigned int base_page = sector * FLASHRAM_PAGES_PER_SECTOR;
    for (unsigned int i = 0; i < FLASHRAM_PAGES_PER_SECTOR; i++)
    {
        const uint8_t* page = content + (i * FLASHRAM_PAGE_SIZE);
        // Pages that end up fully erased need no programming; the erase set them to 0xFF.
        if (flashram_page_is_erased(page))
            continue;
        if (!flashram_program_page(base_page + i, page))
            return false;
    }
    return true;
}

int flashram_write(const void* src, size_t offset, size_t len)
{
    assertf(__flashram_inited, "flashram accessed, but flashram_init() hasn't been called yet");

    assertf(offset + len <= FLASHRAM_SIZE,
            "flashram_write out of range: offset=0x%X len=0x%X (size=0x%X)",
            (unsigned) offset, (unsigned) len, (unsigned) FLASHRAM_SIZE);
    if (len == 0)
        return 0;

    const uint8_t* in = (const uint8_t*) src;
    size_t pos = offset;
    size_t remaining = len;
    uint8_t* sector_buf = NULL;  // 16 KiB scratch, allocated lazily for partial sectors only
    int result = (int) len;

    while (remaining > 0)
    {
        unsigned int sector = pos / FLASHRAM_SECTOR_SIZE;
        size_t sector_start = (size_t) sector * FLASHRAM_SECTOR_SIZE;
        size_t in_sector = pos - sector_start;
        size_t n = FLASHRAM_SECTOR_SIZE - in_sector;
        if (n > remaining)
            n = remaining;

        if (in_sector == 0 && n == FLASHRAM_SECTOR_SIZE)
        {
            // Whole sector overwritten: erase and program straight from the source.
            if (!flashram_write_sector(sector, in))
            {
                result = -1;
                break;
            }
        }
        else
        {
            // Partial sector: read-modify-write to preserve untouched bytes.
            if (sector_buf == NULL)
            {
                sector_buf = memalign(16, FLASHRAM_SECTOR_SIZE);
                assertf(sector_buf != NULL,
                        "flashram_write: out of memory for the 0x%X-byte sector buffer",
                        (unsigned) FLASHRAM_SECTOR_SIZE);
            }
            flashram_read(sector_buf, sector_start, FLASHRAM_SECTOR_SIZE);
            memcpy(sector_buf + in_sector, in, n);
            if (!flashram_write_sector(sector, sector_buf))
            {
                result = -1;
                break;
            }
        }

        in += n;
        pos += n;
        remaining -= n;
    }

    if (sector_buf != NULL)
        free(sector_buf);

    // Return the chip to read mode for subsequent reads.
    flashram_command(FLASHRAM_CMD_READ_MODE);
    return result;
}
