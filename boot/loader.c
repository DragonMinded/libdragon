#include "loader.h"
#include "minidragon.h"
#include "debug.h"
#include <stdbool.h>

#define ENTROPY_FAR
#include "entropy.h"

// Like alloca(), but return a cache-aligned address so that it can
// be safely invalidated without false sharing with stack variables.
#define alloca_aligned(size)  ({ \
    void *ptr = __builtin_alloca((size)+16); \
    ptr += -(uint32_t)ptr & 15; \
})

#define ELF_MAGIC           0x7F454C46
#define PT_LOAD             0x1
#define PT_N64_DECOMP       0x64E36341
#define PF_N64_COMPRESSED   0x1000

static void pi_read_async(void *dram_addr, uint32_t cart_addr, uint32_t len)
{
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

static void rsp_clear_dmem_async(void)
{
    *SP_RSP_ADDR = 0x0000+0x10; // IMEM (skip first 16 bytes which contain bootflags)
    *SP_DRAM_ADDR = 8*1024*1024;
    *SP_RD_LEN = 4096-16-1;
}


static void fast_bzero(void *mem, int size)
{
    uint32_t *ptr = UncachedAddr(mem);
    while (size >= 128) {
        MEMORY_BARRIER();
        *MI_MODE = MI_WMODE_SET_REPEAT_MODE | MI_WMODE_REPEAT_LENGTH(128);
        *ptr = 0;
        MEMORY_BARRIER();
        size -= 128;
        ptr += 128/4;
    }
    if (size) {
        MEMORY_BARRIER();
        *MI_MODE = MI_WMODE_SET_REPEAT_MODE | MI_WMODE_REPEAT_LENGTH(size);
        *ptr = 0;
        MEMORY_BARRIER();
    }
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

__attribute__((used))
void loader(void)
{
    debugf("Hello from RDRAM ", __builtin_frame_address(0));

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
        uint32_t memsize = phdr[5];
        uint32_t flags = phdr[6];

        if (phdr[0] == PT_N64_DECOMP) {
            // If this segment contains the decompression function, load it
            // in RDRAM (allocating memory via alloca()).
            decomp = alloca_aligned(size);
            data_cache_hit_writeback_invalidate(decomp, size);
            vaddr = (uint32_t)decomp;
        } else if (phdr[0] != PT_LOAD) {
            continue;
        }

        // Make sure we can do PI DMA
        assertf((vaddr % 8) == 0, "segment %d: vaddr is not 8-byte aligned: %08x", i, vaddr);
        assertf((offset % 2) == 0, "segment %d: file offset is not 2-byte aligned: %08x", i, offset);

        debugf("Segment ", i, phdr[0], offset, vaddr, size, memsize, flags);
        if (size)
            pi_read_async((void*)vaddr, elf_header + offset, size);

        if (flags & PF_N64_COMPRESSED) {
            //if (!decomp) fatal("INVALID COMPRESSED ELF");

            // Decompress the segment. paddr contains the output pointer, where
            // decompressed data must be written. Notice that we can do this while
            // the DMA is running because decompression functions are supposed to
            // do DMA racing (either that, or just wait for the DMA to finish themselves).
            int dec_size = decomp((void*)vaddr, size, (void*)paddr);

            // Adjust variables for the bzero below, now pointing to the
            // decompressed data.
            vaddr = paddr;
            size = dec_size;

            // Invalidate the cache for the decompressed data. This is required
            // because of the absence of coherency with instruction cache --
            // if we jump there before the cache is flushed, the processor won't
            // see the data.
            data_cache_hit_writeback_invalidate((void*)vaddr, size);
        }

        // Clear the rest of the segment in RDRAM (if any).
        if (memsize)
            fast_bzero((void*)(vaddr + size), memsize - size);
        
        // Wait for the DMA to finish. If we decompressed the data, it's already
        if (size)
            pi_wait();
    }
    void *entrypoint = (void*)io_read32(elf_header + 0x18);

    // Reset the RCP hardware
    rcp_reset();

    // Write the accumulated entropy to the pool
    *(uint32_t*)0xA4000004 = entropy_get();
    debugf("Boot flags: ", *(uint32_t*)0xA4000000, *(uint32_t*)0xA4000004, *(uint32_t*)0xA4000008, *(uint32_t*)0xA400000C);

    // Clear DMEM (leave only the boot flags area intact). Notice that we can't
    // call debugf anymore after this, because a small piece of debugging code
    // (io_write) is in DMEM, so it can't be used anymore.
    rsp_clear_dmem_async();
    #undef debugf

    // Notify the PIF that the boot process is finished
    pif_terminate_boot();

    // Jump to the entry point
    goto *entrypoint;
}
