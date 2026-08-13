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
 * property of the silicon; flashram_init() looks it up from the silicon ID
 * and caches it, and the read path adapts accordingly. Because PGS cannot
 * express the word-indexed "divide by 2", we set PGS to its maximum (never
 * auto-split) and split every DMA manually at the 256-page boundary.
 *
 * The command protocol implemented here matches real hardware (as exercised by
 * libultra and homebrew such as N64-SwapDumper) and is compatible with both
 * SC64 and the ares emulator.
 */

#include "flashram.h"
#include "flashram_internal.h"
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
#define FLASHRAM_CMD_CHIP_ERASE      0x3C000000  ///< Arm whole-chip erase.
#define FLASHRAM_CMD_EXECUTE_ERASE   0x78000000  ///< Execute the armed (sector or chip) erase.
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

/// @endcond

// Canonical layouts for the two known 1 Mibit geometries (128-byte pages,
// 128 pages/sector, 8 sectors, 256-page read boundary), differing only in the
// addressable unit: byte-indexed vs 16-bit-word-indexed.
#define FLASHRAM_LAYOUT_BYTE \
    { .unit_bits = 0, .offset_bits = 7, .page_bits = 7, .sector_bits = 3, .read_page_bits = 8 }
#define FLASHRAM_LAYOUT_WORD \
    { .unit_bits = 1, .offset_bits = 6, .page_bits = 7, .sector_bits = 3, .read_page_bits = 8 }

/// One row of the silicon-ID lookup table.
typedef struct
{
    uint16_t          manufacturer_id;
    uint16_t          device_id;
    const char*       name;
    flashram_layout_t layout;
} flashram_model_t;

/// Known FlashRAM parts. The layout (in particular byte- vs word-addressing) is
/// a fixed property we cannot probe at runtime, so it must be looked up by
/// manufacturer/device ID.
static const flashram_model_t FLASHRAM_MODELS[] = {
    { 0x00C2, 0x0000, "MX29L0000",   FLASHRAM_LAYOUT_WORD },  // Macronix
    { 0x00C2, 0x0001, "MX29L0001",   FLASHRAM_LAYOUT_WORD },
    { 0x00C2, 0x001E, "MX29L1100",   FLASHRAM_LAYOUT_WORD },
    { 0x00C2, 0x001D, "MX29L1101_A", FLASHRAM_LAYOUT_BYTE },
    { 0x00C2, 0x0084, "MX29L1101_B", FLASHRAM_LAYOUT_BYTE },
    { 0x00C2, 0x008E, "MX29L1101_C", FLASHRAM_LAYOUT_BYTE },
    { 0x0032, 0x00F1, "MN63F8MPN",   FLASHRAM_LAYOUT_BYTE },  // Matsushita
};

/// Fill the byte-size geometry fields of @p info from its bit-encoded layout.
static void flashram_derive_geometry(flashram_info_t* info)
{
    const flashram_layout_t* l = &info->layout;
    info->page_size   = (size_t) 1 << (l->unit_bits + l->offset_bits);
    info->sector_size = (size_t) 1 << (l->unit_bits + l->offset_bits + l->page_bits);
    info->total_size  = (size_t) 1 << (l->unit_bits + l->offset_bits + l->page_bits + l->sector_bits);
    info->num_sectors = 1u << l->sector_bits;
    info->num_pages   = 1u << (l->page_bits + l->sector_bits);
}

/// True if flashram_init() has been called.
static bool __flashram_inited = false;

/// True once flashram_init() has probed and found a FlashRAM chip present.
static bool __flashram_present = false;

/// Assert that flashram_init() ran and found a chip -- otherwise the cached
/// layout is only a default guess and reads/writes cannot address the array.
#define flashram_assert_ready() do { \
    assertf(__flashram_inited, "flashram accessed before flashram_init() was called"); \
    assertf(__flashram_present, "flashram accessed, but no FlashRAM chip was detected"); \
} while (0)

/// Cached identity/layout of the detected chip. Defaults to a byte-indexed
/// 1 Mibit part so the read path is safe even before flashram_init() runs.
static flashram_info_t __flashram_info = {
    .type_id         = 0,
    .manufacturer_id = 0,
    .device_id       = 0,
    .layout          = FLASHRAM_LAYOUT_BYTE,
    .name            = "unknown",
    .total_size      = FLASHRAM_SIZE,
    .sector_size     = FLASHRAM_SECTOR_SIZE,
    .page_size       = FLASHRAM_PAGE_SIZE,
    .num_sectors     = FLASHRAM_NUM_SECTORS,
    .num_pages       = FLASHRAM_NUM_PAGES,
};

/// Logical-byte span a single DMA must not cross, derived from the current
/// layout's read boundary. For every known part this is 0x8000 logical bytes.
static inline size_t flashram_dma_boundary(void)
{
    const flashram_layout_t* l = &__flashram_info.layout;
    return (size_t) 1 << (l->unit_bits + l->offset_bits + l->read_page_bits);
}

/// Number of program pages per erase sector for the detected layout.
static inline unsigned int flashram_pages_per_sector(void)
{
    return __flashram_info.num_pages / __flashram_info.num_sectors;
}

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
/// unit_bits is the log2 of the unit size, so it is exactly the shift that maps
/// a byte offset onto a unit (word) address: 0 for byte-indexed, 1 for word.
static inline uint32_t flashram_pi_address(size_t offset)
{
    return FLASHRAM_ADDRESS + (uint32_t) (offset >> __flashram_info.layout.unit_bits);
}

uint8_t flashram_status(void)
{
    flashram_assert_ready();
    flashram_command_twice(FLASHRAM_CMD_STATUS_MODE);
    // The window returns the pattern 00 <status>; the upper bits are meaningless
    // (and on MX29L1100 hold leftovers from the previous mode), so keep the low byte.
    return (uint8_t) io_read(FLASHRAM_ADDRESS);
}

void flashram_clear_status(void)
{
    flashram_assert_ready();
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

/// Standard PI DOM2 bus timings for FlashRAM (https://n64brew.dev/wiki/Flash).
/// PGS is left at its maximum so the PI never auto-splits DMAs -- the driver
/// splits them manually, which is required for word-indexed parts.
static const pi_dom_timings_t FLASHRAM_TIMINGS_DEFAULT = {
    .latency     = 0x05,
    .pulse_width = 0x0C,
    .page_size   = 0x0F,
    .release     = 0x02,
};

/// Read the silicon ID, identify the chip, and cache its layout/geometry.
/// Returns whether a known FlashRAM identifier was found. Leaves read mode.
static bool flashram_probe(void)
{
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

    // Look up the model to learn its layout. Default to a byte-indexed 1 Mibit
    // part (the common flashcart/emulator convention) for parts not in the table.
    const char* name = "unknown";
    flashram_layout_t layout = (flashram_layout_t) FLASHRAM_LAYOUT_BYTE;
    for (size_t i = 0; i < sizeof(FLASHRAM_MODELS) / sizeof(FLASHRAM_MODELS[0]); i++)
    {
        if (FLASHRAM_MODELS[i].manufacturer_id == manufacturer_id &&
            FLASHRAM_MODELS[i].device_id == device_id)
        {
            name = FLASHRAM_MODELS[i].name;
            layout = FLASHRAM_MODELS[i].layout;
            break;
        }
    }

    __flashram_info.type_id         = type_id;
    __flashram_info.manufacturer_id = manufacturer_id;
    __flashram_info.device_id       = device_id;
    __flashram_info.layout          = layout;
    __flashram_info.name            = name;
    flashram_derive_geometry(&__flashram_info);
    return true;
}

bool flashram_init(const pi_dom_timings_t* timings, flashram_info_t* info)
{
    if (!__flashram_inited)
    {
        if (timings == NULL)
            timings = &FLASHRAM_TIMINGS_DEFAULT;

        // Configure PI DOM2 registers to enable access to FlashRAM.
        disable_interrupts();
        *PI_BSD_DOM2_LAT = timings->latency;
        *PI_BSD_DOM2_PWD = timings->pulse_width;
        *PI_BSD_DOM2_PGS = timings->page_size;
        *PI_BSD_DOM2_RLS = timings->release;
        enable_interrupts();

        __flashram_inited = true;

        // Probe the chip so its layout is cached before any read/write. Leaves
        // the chip in read mode whether or not a chip was found.
        __flashram_present = flashram_probe();
    }

    if (info && __flashram_present)
        *info = __flashram_info;
    return __flashram_present;
}

int flashram_read(void* dst, size_t offset, size_t len)
{
    flashram_assert_ready();

    size_t total_size = __flashram_info.total_size;
    assertf(offset + len <= total_size,
            "flashram_read out of range: offset=0x%X len=0x%X (size=0x%X)",
            (unsigned) offset, (unsigned) len, (unsigned) total_size);
    if (len == 0)
        return 0;
    // PI DMA moves the array over the 2-byte bus, so the offset must be even.
    assertf((offset & 1) == 0, "flashram_read requires an even offset, got 0x%X", (unsigned) offset);

    flashram_command(FLASHRAM_CMD_READ_MODE);

    uint8_t bounce[512] __attribute__((aligned(16)));
    uint8_t* out = (uint8_t*) dst;
    size_t done = 0;
    size_t boundary = flashram_dma_boundary();  // 0x8000 logical bytes for known parts

    while (done < len)
    {
        size_t abs_off = offset + done;
        // Clamp this DMA so it neither exceeds the request nor crosses the
        // 256-page (0x8000 logical byte) boundary that a single DMA cannot span.
        size_t to_boundary = boundary - (abs_off & (boundary - 1));
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
        if (abs_off + dma_len > total_size)
            dma_len = total_size - abs_off;

        data_cache_hit_writeback_invalidate(bounce, dma_len);
        dma_read_raw_async(bounce, pi, dma_len);
        dma_wait();

        memcpy(out + done, bounce, chunk);
        done += chunk;
    }
    return (int) len;
}

/// True if all @p page_size bytes at @p page are 0xFF (already erased).
static bool flashram_page_is_erased(const uint8_t* page, size_t page_size)
{
    for (size_t i = 0; i < page_size; i++)
    {
        if (page[i] != 0xFF)
            return false;
    }
    return true;
}

bool flashram_erase_sector_at_page(unsigned int page)
{
    flashram_assert_ready();

    assertf(page < __flashram_info.num_pages,
            "flashram_erase_sector_at_page: page %u out of range (0..%u)",
            page, __flashram_info.num_pages - 1);

    // The sector-erase command is addressed by page number; the chip derives the
    // enclosing sector from it.
    flashram_command(FLASHRAM_CMD_SECTOR_ERASE | page);
    flashram_command(FLASHRAM_CMD_EXECUTE_ERASE);
    bool ok = flashram_wait_ready(FLASHRAM_STATUS_ERASE_BUSY, FLASHRAM_STATUS_ERASE_OK,
                                  FLASHRAM_ERASE_TIMEOUT_MS);
    // Clear the OK/error latch so the next operation's result is not masked.
    flashram_clear_status();
    return ok;
}

bool flashram_erase_chip(void)
{
    flashram_assert_ready();

    flashram_command(FLASHRAM_CMD_CHIP_ERASE);
    flashram_command(FLASHRAM_CMD_EXECUTE_ERASE);
    bool ok = flashram_wait_ready(FLASHRAM_STATUS_ERASE_BUSY, FLASHRAM_STATUS_ERASE_OK,
                                  FLASHRAM_ERASE_TIMEOUT_MS);
    // Clear the OK/error latch so the next operation's result is not masked.
    flashram_clear_status();
    return ok;
}

bool flashram_program_page(unsigned int page, const void* data)
{
    flashram_assert_ready();

    assertf(page < __flashram_info.num_pages,
            "flashram_program_page: page %u out of range (0..%u)",
            page, __flashram_info.num_pages - 1);

    // Copy into an aligned bounce so callers can pass any alignment. The bounce
    // is sized to the standard page (128 B), which caps the layouts we support.
    size_t page_size = __flashram_info.page_size;
    uint8_t buffer[FLASHRAM_PAGE_SIZE] __attribute__((aligned(16)));
    assertf(page_size <= sizeof(buffer),
            "flashram_program_page: page size 0x%X exceeds the buffer", (unsigned) page_size);
    memcpy(buffer, data, page_size);

    // Load the page buffer, then program it into the target page.
    flashram_command(FLASHRAM_CMD_LOAD_BYTE_PAGE);
    data_cache_hit_writeback(buffer, page_size);
    dma_write_raw_async(buffer, FLASHRAM_ADDRESS, page_size);
    dma_wait();
    flashram_command(FLASHRAM_CMD_PROGRAM_PAGE | page);
    bool ok = flashram_wait_ready(FLASHRAM_STATUS_PROGRAM_BUSY, FLASHRAM_STATUS_PROGRAM_OK,
                                  FLASHRAM_PROGRAM_TIMEOUT_MS);
    // Clear the OK/error latch so the next operation's result is not masked.
    flashram_clear_status();
    return ok;
}

/// Erase the sector starting at @p base_page, then program its pages from the
/// one-sector @p content buffer. Works in page numbers throughout.
static bool flashram_write_sector(unsigned int base_page, const uint8_t* content)
{
    if (!flashram_erase_sector_at_page(base_page))
        return false;

    size_t page_size = __flashram_info.page_size;
    unsigned int pages_per_sector = flashram_pages_per_sector();
    for (unsigned int i = 0; i < pages_per_sector; i++)
    {
        const uint8_t* page = content + (i * page_size);
        // Pages that end up fully erased need no programming; the erase set them to 0xFF.
        if (flashram_page_is_erased(page, page_size))
            continue;
        if (!flashram_program_page(base_page + i, page))
            return false;
    }
    return true;
}

int flashram_write(const void* src, size_t offset, size_t len)
{
    flashram_assert_ready();

    size_t total_size = __flashram_info.total_size;
    size_t sector_size = __flashram_info.sector_size;
    unsigned int pages_per_sector = flashram_pages_per_sector();
    assertf(offset + len <= total_size,
            "flashram_write out of range: offset=0x%X len=0x%X (size=0x%X)",
            (unsigned) offset, (unsigned) len, (unsigned) total_size);
    if (len == 0)
        return 0;

    const uint8_t* in = (const uint8_t*) src;
    size_t pos = offset;
    size_t remaining = len;
    uint8_t* sector_buf = NULL;  // one-sector scratch, allocated lazily for partial sectors only
    int result = (int) len;

    while (remaining > 0)
    {
        unsigned int sector = pos / sector_size;
        unsigned int base_page = sector * pages_per_sector;
        size_t sector_start = (size_t) sector * sector_size;
        size_t in_sector = pos - sector_start;
        size_t n = sector_size - in_sector;
        if (n > remaining)
            n = remaining;

        if (in_sector == 0 && n == sector_size)
        {
            // Whole sector overwritten: erase and program straight from the source.
            if (!flashram_write_sector(base_page, in))
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
                sector_buf = memalign(16, sector_size);
                assertf(sector_buf != NULL,
                        "flashram_write: out of memory for the 0x%X-byte sector buffer",
                        (unsigned) sector_size);
            }
            flashram_read(sector_buf, sector_start, sector_size);
            memcpy(sector_buf + in_sector, in, n);
            if (!flashram_write_sector(base_page, sector_buf))
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
