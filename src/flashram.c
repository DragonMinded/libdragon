/**
 * @file flashram.c
 * @brief FlashRAM access functions for N64 cartridges
 *
 * FlashRAM is a 1 Mibit (128 KiB) Macronix-family NOR flash used as save
 * storage on some N64 cartridges. It shares the PI DOM2 window with cartridge
 * SRAM (a cart has one or the other, never both), but is driven through a
 * command state machine rather than being flat memory:
 *
 *   - Command register at #FLASHRAM_ADDRESS | 0x10000: 32-bit command words are
 *     written here as (opcode << 24) | argument, where the argument (for
 *     erase/program) is a *page number*, not a byte offset.
 *   - Data window at #FLASHRAM_ADDRESS: the status/ID readback and the array
 *     read/write buffer. Array data can only be moved via 16-bit PI DMA; a CPU
 *     read of this window returns the status/ID latch, never array data.
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
#include <errno.h>
#include <malloc.h>
#include <string.h>

/// @cond
#define PI_BSD_DOM2_LAT ((volatile uint32_t*) 0xA4600024)
#define PI_BSD_DOM2_PWD ((volatile uint32_t*) 0xA4600028)
#define PI_BSD_DOM2_PGS ((volatile uint32_t*) 0xA460002C)
#define PI_BSD_DOM2_RLS ((volatile uint32_t*) 0xA4600030)

/// Command register address (offset 0x10000 into the FlashRAM window).
#define FLASHRAM_COMMAND_ADDRESS (FLASHRAM_ADDRESS | 0x00010000)

/// Silicon ID word reported by FlashRAM in identify mode.
#define FLASHRAM_IDENTIFIER      0x11118001

// Command opcodes (written as (opcode) or (opcode | page_number)).
#define FLASHRAM_CMD_STATUS_MODE     0xD2000000  ///< Enter status mode.
#define FLASHRAM_CMD_IDENTIFY_MODE   0xE1000000  ///< Enter identify (silicon ID) mode.
#define FLASHRAM_CMD_READ_MODE       0xF0000000  ///< Enter array read mode.
#define FLASHRAM_CMD_SECTOR_ERASE    0x4B000000  ///< Arm sector erase at page (| page).
#define FLASHRAM_CMD_EXECUTE_ERASE   0x78000000  ///< Execute the armed erase.
#define FLASHRAM_CMD_PAGE_PROGRAM    0xB4000000  ///< Enter page-program mode.
#define FLASHRAM_CMD_EXECUTE_PROGRAM 0xA5000000  ///< Execute page program at page (| page).

// Status register bits.
#define FLASHRAM_STATUS_PROGRAM_BUSY 0x01  ///< A page program is in progress.
#define FLASHRAM_STATUS_ERASE_BUSY   0x02  ///< A sector/chip erase is in progress.
#define FLASHRAM_STATUS_PROGRAM_OK   0x04  ///< The last page program completed.
#define FLASHRAM_STATUS_ERASE_OK     0x08  ///< The last erase completed.

// Blocking poll timeouts (milliseconds).
#define FLASHRAM_PROGRAM_TIMEOUT_MS  1000
#define FLASHRAM_ERASE_TIMEOUT_MS    3000

#define FLASHRAM_PAGES_PER_SECTOR (FLASHRAM_SECTOR_SIZE / FLASHRAM_PAGE_SIZE)
/// @endcond

/// True if flashram_init() has been called.
static bool __flashram_inited = false;

/// Write a command word to the FlashRAM command register.
static inline void flashram_command(uint32_t command)
{
    io_write(FLASHRAM_COMMAND_ADDRESS, command);
}

uint32_t flashram_status(void)
{
    assertf(__flashram_inited, "flashram accessed, but flashram_init() hasn't been called yet");
    flashram_command(FLASHRAM_CMD_STATUS_MODE);
    return io_read(FLASHRAM_ADDRESS);
}

/// Poll the status register until @p busy_mask clears; returns whether @p ok_mask is then set.
static bool flashram_wait_ready(uint32_t busy_mask, uint32_t ok_mask, uint32_t timeout_ms)
{
    uint64_t start_ms = get_ticks_ms();
    while (true)
    {
        uint32_t status = flashram_status();
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

    // Configure PI DOM2 registers to enable access to FlashRAM (same timing as SRAM).
    disable_interrupts();
    *PI_BSD_DOM2_LAT = 0x5;
    *PI_BSD_DOM2_PWD = 0xc;
    *PI_BSD_DOM2_PGS = 0xd;
    *PI_BSD_DOM2_RLS = 0x2;
    enable_interrupts();

    __flashram_inited = true;

    // Leave the chip in a known (read) state.
    flashram_command(FLASHRAM_CMD_READ_MODE);
}

int flashram_detect(void)
{
    assertf(__flashram_inited, "flashram accessed, but flashram_init() hasn't been called yet");

    // Enter identify mode and DMA out the two silicon-ID words.
    uint32_t id[2] __attribute__((aligned(16))) = {0};
    flashram_command(FLASHRAM_CMD_IDENTIFY_MODE);
    data_cache_hit_writeback_invalidate(id, sizeof(id));
    dma_read_raw_async(id, FLASHRAM_ADDRESS, sizeof(id));
    dma_wait();

    // Restore read mode so a later CPU/DMA read returns array data.
    flashram_command(FLASHRAM_CMD_READ_MODE);

    return (id[0] == FLASHRAM_IDENTIFIER) ? FLASHRAM_SIZE : 0;
}

int flashram_read(void* dst, size_t offset, size_t len)
{
    assertf(__flashram_inited, "flashram accessed, but flashram_init() hasn't been called yet");

    if (offset + len > FLASHRAM_SIZE)
    {
        errno = EINVAL;
        return -1;
    }
    if (len == 0)
        return 0;
    // PI DMA requires a 2-byte-aligned source (FlashRAM) address.
    if (offset & 1)
    {
        errno = EINVAL;
        return -1;
    }

    flashram_command(FLASHRAM_CMD_READ_MODE);

    // Fast path: destination and length are aligned for a direct DMA.
    if ((((uintptr_t) dst & 7) == 0) && ((len & 1) == 0))
    {
        data_cache_hit_writeback_invalidate(dst, len);
        dma_read_raw_async(dst, FLASHRAM_ADDRESS + offset, len);
        dma_wait();
        return (int) len;
    }

    // Slow path: bounce through an aligned buffer. Array data must be moved with
    // pure PI DMA (16-bit bus cycles) -- CPU I/O of the FlashRAM window returns
    // the status latch, not array bytes -- so we never fall back to io_read here.
    uint8_t bounce[512] __attribute__((aligned(16)));
    uint8_t* out = (uint8_t*) dst;
    size_t done = 0;
    while (done < len)
    {
        size_t chunk = len - done;
        if (chunk > sizeof(bounce))
            chunk = sizeof(bounce);

        // DMA a 2-byte-rounded span, clamped to the end of the chip.
        size_t dma_len = (chunk + 1) & ~(size_t) 1;
        if (offset + done + dma_len > FLASHRAM_SIZE)
            dma_len = FLASHRAM_SIZE - (offset + done);

        data_cache_hit_writeback_invalidate(bounce, dma_len);
        dma_read_raw_async(bounce, FLASHRAM_ADDRESS + offset + done, dma_len);
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

    if (sector >= FLASHRAM_NUM_SECTORS)
    {
        errno = EINVAL;
        return false;
    }

    unsigned int page = sector * FLASHRAM_PAGES_PER_SECTOR;
    flashram_command(FLASHRAM_CMD_SECTOR_ERASE | page);
    flashram_command(FLASHRAM_CMD_EXECUTE_ERASE);
    return flashram_wait_ready(FLASHRAM_STATUS_ERASE_BUSY, FLASHRAM_STATUS_ERASE_OK,
                               FLASHRAM_ERASE_TIMEOUT_MS);
}

bool flashram_program_page(unsigned int page, const void* data)
{
    assertf(__flashram_inited, "flashram accessed, but flashram_init() hasn't been called yet");

    if (page >= FLASHRAM_NUM_PAGES)
    {
        errno = EINVAL;
        return false;
    }

    // Copy into an aligned bounce so callers can pass any alignment.
    uint8_t buffer[FLASHRAM_PAGE_SIZE] __attribute__((aligned(16)));
    memcpy(buffer, data, FLASHRAM_PAGE_SIZE);

    flashram_command(FLASHRAM_CMD_PAGE_PROGRAM);
    data_cache_hit_writeback(buffer, FLASHRAM_PAGE_SIZE);
    dma_write_raw_async(buffer, FLASHRAM_ADDRESS, FLASHRAM_PAGE_SIZE);
    dma_wait();
    flashram_command(FLASHRAM_CMD_EXECUTE_PROGRAM | page);
    return flashram_wait_ready(FLASHRAM_STATUS_PROGRAM_BUSY, FLASHRAM_STATUS_PROGRAM_OK,
                               FLASHRAM_PROGRAM_TIMEOUT_MS);
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

    if (offset + len > FLASHRAM_SIZE)
    {
        errno = EINVAL;
        return -1;
    }
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
                if (sector_buf == NULL)
                {
                    errno = ENOMEM;
                    result = -1;
                    break;
                }
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
