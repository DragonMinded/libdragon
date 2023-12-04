#include "loader.h"
#include "minidragon.h"
#include "debug.h"
#include <stdbool.h>

#define ENTROPY_FAR
#include "entropy.h"

#define MIN(a, b)   ((a) < (b) ? (a) : (b))
#define MAX(a, b)   ((a) > (b) ? (a) : (b))

// Like alloca(), but return a cache-aligned address so that it can
// be safely invalidated without false sharing with stack variables.
#define alloca_aligned(size)  ({ \
    void *ptr = __builtin_alloca((size)+16); \
    ptr += -(uint32_t)ptr & 15; \
})

#define ELF_MAGIC               0x7F454C46
#define PT_LOAD                 0x1
#define PT_N64_DECOMP           0x64E36341
#define PF_N64_COMPRESSED       0x1000

// Stage 1 functions we want to reuse
__attribute__((far))
extern void rsp_clear_mem(uint32_t mem, int size);
__attribute__((far))
extern void rsp_bzero_async(uint32_t rdram, int size);

static void pi_read_async(void *dram_addr, uint32_t cart_addr, uint32_t len)
{
    while (*PI_STATUS & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY)) {}
    *PI_DRAM_ADDR = (uint32_t)dram_addr;
    *PI_CART_ADDR = cart_addr;
    *PI_WR_LEN = len-1;
}

static void pi_wait(void)
{
    while (*PI_STATUS & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY)) {}
    // PI timings are subject to small oscillations. We consider those as
    // entropy, so fetching C0_COUNT after a PI DMA is finished is a good
    // way to add more entropy.
    entropy_add(C0_COUNT());
}

static uint32_t io_read32(uint32_t vaddrx)
{
    vaddrx |= 0xA0000000;
    volatile uint32_t *vaddr = (uint32_t *)vaddrx;
    return *vaddr;
}

static uint16_t io_read16(uint32_t vaddrx)
{
    uint32_t value = io_read32(vaddrx & ~3);
    if (!(vaddrx & 2))
        value >>= 16;
    return value;
}

__attribute__((noinline))
static uint8_t io_read8(uint32_t vaddrx)
{
    uint32_t value = io_read32(vaddrx & ~3);
    return value >> ((~vaddrx & 3)*8);
}

static void fast_bzero_range(void *mem, void *mem_end)
{
    int size = mem_end - mem;
    data_cache_hit_writeback_invalidate(mem, size);
    rsp_bzero_async((uint32_t)mem, size);
}

static void rcp_reset(void)
{
    *SP_STATUS = SP_WSTATUS_CLEAR_BROKE | SP_WSTATUS_SET_HALT | SP_WSTATUS_CLEAR_INTR | 
                 SP_WSTATUS_CLEAR_SIG0 | SP_WSTATUS_CLEAR_SIG1 | 
                 SP_WSTATUS_CLEAR_SIG2 | SP_WSTATUS_CLEAR_SIG3 | 
                 SP_WSTATUS_CLEAR_SIG4 | SP_WSTATUS_CLEAR_SIG5 | 
                 SP_WSTATUS_CLEAR_SIG6 | SP_WSTATUS_CLEAR_SIG7;
    *SP_PC = 0;
    *SP_SEMAPHORE = 0;
    
    *MI_INTERRUPT = MI_WINTERRUPT_CLR_SP | MI_WINTERRUPT_CLR_SI | MI_WINTERRUPT_CLR_AI | MI_WINTERRUPT_CLR_VI | MI_WINTERRUPT_CLR_PI | MI_WINTERRUPT_CLR_DP;
    *PI_STATUS = PI_CLEAR_INTERRUPT;
    *SI_STATUS = SI_CLEAR_INTERRUPT;
    *AI_STATUS = AI_CLEAR_INTERRUPT;
    *MI_MODE = DP_CLEAR_INTERRUPT;
}

static void pif_terminate_boot(void)
{
    // Inform PIF that the boot process is finished. If this is not written,
    // the PIF will halt the CPU after 5 seconds. This is not done by official
    // IPL3 but rather left to the game to do, but for our open source IPL3,
    // it seems better to leave it to the IPL3.
    si_write(0x7FC, 0x8);
}

static void fatal(const char *str)
{
#if 0
#define FATAL_TEXT  0
#if FATAL_TEXT
    static const unsigned char font[] = {
        0x7e, 0xa1, 0x99, 0x85, 0x7e, 0x84, 0x82, 0xff, 0x80, 0x80, 0xc1, 0xa1,
        0x91, 0x89, 0x86, 0x89, 0x89, 0x89, 0x89, 0x76, 0x18, 0x14, 0x12, 0xff,
        0x10, 0x8f, 0x89, 0x89, 0x89, 0x71, 0x7e, 0x89, 0x89, 0x89, 0x72, 0x01,
        0x81, 0x61, 0x19, 0x07, 0x62, 0x95, 0x89, 0x95, 0x62, 0x4e, 0x91, 0x91,
        0x91, 0x7e, 0xfe, 0x11, 0x11, 0x11, 0xfe, 0xff, 0x89, 0x89, 0x89, 0x76,
        0x7e, 0x81, 0x81, 0x81, 0x81, 0xff, 0x81, 0x81, 0x81, 0x7e, 0xff, 0x89,
        0x89, 0x89, 0x89, 0xff, 0x09, 0x09, 0x09, 0x09, 0x7e, 0x81, 0x91, 0x51,
        0xf1, 0xff, 0x08, 0x08, 0x08, 0xff, 0x00, 0x81, 0xff, 0x81, 0x00, 0x40,
        0x80, 0x80, 0x80, 0x7f, 0xff, 0x08, 0x14, 0x22, 0xc1, 0xff, 0x80, 0x80,
        0x80, 0x80, 0xff, 0x02, 0x04, 0x02, 0xff, 0xff, 0x06, 0x18, 0x60, 0xff,
        0x7e, 0x81, 0x81, 0x81, 0x7e, 0xff, 0x11, 0x11, 0x11, 0x0e, 0x7e, 0x81,
        0xa1, 0xc1, 0xfe, 0xff, 0x11, 0x11, 0x11, 0xee, 0x86, 0x89, 0x89, 0x89,
        0x71, 0x01, 0x01, 0xff, 0x01, 0x01, 0x7f, 0x80, 0x80, 0x80, 0x7f, 0x1f,
        0x60, 0x80, 0x60, 0x1f, 0xff, 0x40, 0x20, 0x40, 0xff, 0xc7, 0x28, 0x10,
        0x28, 0xc7, 0x07, 0x08, 0xf0, 0x08, 0x07, 0xc1, 0xa1, 0x99, 0x85, 0x83
    };
#endif
    static const uint32_t vi_pal_p[] =  {
        0x00003202, 0xa0100000, 0x00000140, 0x00000002,
        0x00000008, 0x0404233a, 0x00000271, 0x00150c69,
        0x0c6f0c6e, 0x00800300, 0x005f0239, 0x0009026b,
        0x00000200, 0x00000400 };
    static volatile uint32_t *VI_REGS = (uint32_t*)0xA4400000;

#if !FATAL_TEXT
    uint16_t *fb = (uint16_t*)0xA0100000;
    for (int i=0; i<320*240; i++) fb[i] = (0x1f)<<11;
#else
    const int RES_WIDTH = 320;
    const int X = 80;
    const int Y = 40;
    const uint16_t COLOR = 0xFFFF;

    uint16_t *fb = (uint16_t*)0xA0100000;
    fb += Y*RES_WIDTH + X;
    while (1) {
        char ch = io_read8((uint32_t)str); str++;
        if (!ch) break;
        int idx;
        if (ch >= '0' && ch <= '9') idx = ch - '0';
        else if (ch >= 'A' && ch <= 'Z') idx = ch - 'A' + 10;
        else {
            fb += 5;
            continue;
        }

        const uint8_t *glyph = font + idx*5;
        for (int x=0; x<5; x++) {
            uint8_t g = io_read8((uint32_t)glyph);
            for (int y=0; y<8; y++) {
                if (g & (1 << y))
                    fb[RES_WIDTH*y] = COLOR;
            }
            fb++;
            glyph++;
        }

        fb += 2; // spacing
    }

#endif
    for (int reg=13; reg>=0; reg--)
        VI_REGS[reg] = vi_pal_p[reg];
#endif
    abort();
}

__attribute__((used))
void loader(void)
{
    debugf("Hello from RDRAM ", __builtin_frame_address(0));

    // Invalidate the stack1 area, where the first stage put its stack.
    // We don't need it anymore, and we don't want it to be flushed to RDRAM
    // that will be cleared anyway.
    data_cache_hit_invalidate(STACK1_BASE, STACK1_SIZE);

    // Search for the ELF header. We search for a 256-byte aligned header
    // starting at offset 0x1000 in the ROM area (after the IPL3).
    // We search for a bit but not forever -- it doesn't help anyone a ROM
    // that starts after 2 minutes.
    uint32_t elf_header = 0x10001000;
    bool found_elf = false;
    for (int i=0; i<1024; i++) {
        if (io_read32(elf_header) == ELF_MAGIC) {
            found_elf = true;
            break;
        }
        elf_header += 0x100;
    }
    if (!found_elf) {
        debugf("ELF header not found: make sure it is 256-byte aligned");
        fatal("ELF HEADER NOT FOUND");
        abort();
    }

    // Store the ELF offset in the boot flags
    *(uint32_t*)0xA400000C = elf_header << 8;

    // Read program headers offset and number. Allocate space in the stack
    // for them.
    uint32_t phoff = io_read32(elf_header + 0x1C);
    int phnum = io_read16(elf_header + 0x2C);
    uint32_t *phdr = alloca_aligned(0x20 * phnum);
    data_cache_hit_writeback_invalidate(phdr, 0x20 * phnum);

    // Load all the program headers
    pi_read_async((void*)phdr, elf_header + phoff, 0x20 * phnum);
    pi_wait();

    // Decompression function (if any)
    int (*decomp)(void *inbuf, int size, void *outbuf) = 0;

    // Load the program segments
    for (int i=0; i<phnum; i++, phdr+=0x20/4) {
        uint32_t offset = phdr[1];
        uint32_t vaddr = phdr[2];
        uint32_t paddr = phdr[3];
        uint32_t size = phdr[4];
        uint32_t flags = phdr[6];

        if (phdr[0] == PT_N64_DECOMP) {
            // If this segment contains the decompression function, load it
            // in RDRAM (to the specified address, or allocating memory via alloca()).
            if (vaddr == 0) {
                decomp = alloca_aligned(size);
                data_cache_hit_writeback_invalidate(decomp, size);
                vaddr = (uint32_t)decomp;
            } else {
                decomp = (void*)vaddr;
            }
        } else if (phdr[0] != PT_LOAD) {
            continue;
        }

        if (!size) continue;

        // Make sure we can do PI DMA
        assertf((vaddr % 8) == 0, "segment %d: vaddr is not 8-byte aligned: %08x", i, vaddr);
        assertf((offset % 2) == 0, "segment %d: file offset is not 2-byte aligned: %08x", i, offset);

        debugf("Segment ", i, phdr[0], offset, vaddr, size, flags);

        // Load the segment into RDRAM. Notice that we don't need to clear
        // extra size at the end of the segment (as specified by phdr[5])
        // as the whole RDRAM has been cleared already.
        pi_read_async((void*)vaddr, elf_header + offset, size);

        if (flags & PF_N64_COMPRESSED) {
            // Decompress the segment. paddr contains the output pointer, where
            // decompressed data must be written. Notice that we can do this while
            // the DMA is running because decompression functions are supposed to
            // do DMA racing (either that, or just wait for the DMA to finish themselves).
            int dec_size = decomp((void*)vaddr, size, (void*)paddr);

            // Flush the cache for the decompressed data.
            data_cache_hit_writeback_invalidate((void*)paddr, dec_size);

            // Clear the range of memory [vaddr, vaddr+size] which lies outside
            // of the range [paddr, paddr+dec_size]. That is, any compressed
            // data leftover (which was not overwritten by decompressed data).
            // uint32_t paddr_end = paddr + dec_size;
            // uint32_t vaddr_end = vaddr + size;
            // if (vaddr < paddr)
            //     fast_bzero_range((void*)vaddr, (void*)MIN(vaddr_end, paddr));
            // else if (vaddr_end > paddr_end)
            //     fast_bzero_range((void*)MAX(vaddr, paddr_end), (void*)vaddr_end);
        }

        // Wait for the DMA to finish.
        pi_wait();
    }
    void *entrypoint = (void*)io_read32(elf_header + 0x18);

    // Reset the RCP hardware
    rcp_reset();

    // Write the accumulated entropy to the pool
    *(uint32_t*)0xA4000004 = entropy_get();
    debugf("Boot flags: ", *(uint32_t*)0xA4000000, *(uint32_t*)0xA4000004, *(uint32_t*)0xA4000008, *(uint32_t*)0xA400000C);

    // Notify the PIF that the boot process is finished. This will take a while
    // so start it in background.
    pif_terminate_boot();

    // Clear DMEM (leave only the boot flags area intact). Notice that we can't
    // call debugf anymore after this, because a small piece of debugging code
    // (io_write) is in DMEM, so it can't be used anymore.
    rsp_clear_mem(0xA4000010, 4096-16);
    #undef debugf

    // Wait until the PIF is done. This will also clear the interrupt, so that
    // we don't leave the interrupt pending when we go to the entrypoint.
    si_wait();

    // Jump to the entry point
    goto *entrypoint;
}
