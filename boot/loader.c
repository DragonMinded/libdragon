/**
 * @file loader.c
 * @author Giovanni Bajo (giovannibajo@gmail.com)
 * @brief IPL3: Stage 2 (ELF loader)
 * 
 * This module implements the second stage of the loader, which is responsible
 * of searching and loading the ELF file embedded in the ROM, and jumping
 * to the entrypoint.
 * 
 * This stage runs from "high RDRAM", that is, it is placed at the end of RDRAM.
 * The code is compiled to be relocatable via a trick in the Makefile, so that
 * it can be placed at dynamic addresses (though normally only two would be
 * possible: either near 4 MiB or 8 MiB).
 * 
 * The tasks performed by this stage are:
 * 
 *  * Find the ELF file in ROM.
 *  * Load the ELF file in memory (PT_LOAD segments).
 *  * Optionally decompress the ELF file (using the decompression function
 *    stored in the ELF file itself).
 *  * Reset the RCP hardware (SP, DP, MI, PI, SI, AI).
 *  * Finalize the entropy accumulator and store it in the boot flags.
 *  * Notify the PIF that the boot process is finished.
 *  * Clear DMEM (except the boot flags area).
 *  * Jump to the entrypoint.
 */
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
extern void rsp_bzero_async(uint32_t rdram, int size);
__attribute__((far))
extern void cop0_clear_cache(void);

__attribute__((far, noreturn))
void stage3(uint32_t entrypoint);

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

static void pif_terminate_boot(void)
{
    // Inform PIF that the boot process is finished. If this is not written,
    // the PIF will halt the CPU after 5 seconds. This is not done by official
    // IPL3 but rather left to the game to do, but for our open source IPL3,
    // it seems better to leave it to the IPL3.
    si_write(0x7FC, 0x8);
}

static const unsigned char font[] = {
    0x00, 0x00, 0x00, 0x00, 0x00,
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
    0x28, 0xc7, 0x07, 0x08, 0xf0, 0x08, 0x07, 0xc1, 0xa1, 0x99, 0x85, 0x83,
};


#define _(x)    ((x) >= '0' && (x) <= '9' ? (x) - '0' + 2 : \
                 (x) >= 'A' && (x) <= 'Z' ? (x) - 'A' + 12 : \
                 (x) == ' ' ? 1 : 0)

// "ELF HEADER NOT FOUND"
__attribute__((aligned(1)))
static const char MSG_ELF_NOT_FOUND[] = {
    _('E'), _('L'), _('F'), _(' '),
    _('H'), _('E'), _('A'), _('D'), _('E'), _('R'), _(' '),
    _('N'), _('O'), _('T'), _(' '),
    _('F'), _('O'), _('U'), _('N'), _('D'), 0
};

// "ELF LITTLE ENDIAN NOT SUPPORTED"
__attribute__((aligned(1)))
static const char MSG_ELF_LITTLE_ENDIAN[] = {
    _('E'), _('L'), _('F'), _(' '),
    _('L'), _('I'), _('T'), _('T'), _('L'), _('E'), _(' '),
    _('E'), _('N'), _('D'), _('I'), _('A'), _('N'), _(' '),
    _('N'), _('O'), _('T'), _(' '),
    _('S'), _('U'), _('P'), _('P'), _('O'), _('R'), _('T'), _('E'), _('D'), 0
};

// "ELF VADDR NOT 8-BYTE ALIGNED"
__attribute__((aligned(1)))
static const char MSG_ELF_VADDR_NOT_ALIGNED[] = {
    _('E'), _('L'), _('F'), _(' '),
    _('V'), _('A'), _('D'), _('D'), _('R'), _(' '),
    _('N'), _('O'), _('T'), _(' '),
    _('8'), _(' '), _('B'), _('Y'), _('T'), _('E'), _(' '),
    _('A'), _('L'), _('I'), _('G'), _('N'), _('E'), _('D'), 0
};

// "ELF OFFSET NOT 2-BYTE ALIGNED"
__attribute__((aligned(1)))
static const char MSG_ELF_OFFSET_NOT_ALIGNED[] = {
    _('E'), _('L'), _('F'), _(' '),
    _('O'), _('F'), _('F'), _('S'), _('E'), _('T'), _(' '),
    _('N'), _('O'), _('T'), _(' '),
    _('2'), _(' '), _('B'), _('Y'), _('T'), _('E'), _(' '),
    _('A'), _('L'), _('I'), _('G'), _('N'), _('E'), _('D'), 0
};

#undef _

__attribute__((noreturn))
static void fatal(const char *str)
{
#if 1
#define FATAL_TEXT  1
#if FATAL_TEXT
#endif
    static const uint32_t vi_regs_p[3][7] =  {
    {   /* PAL */   0x0404233a, 0x00000271, 0x00150c69,
        0x0c6f0c6e, 0x00800300, 0x005f0239, 0x0009026b },
    {   /* NTSC */  0x03e52239, 0x0000020d, 0x00000c15,
        0x0c150c15, 0x006c02ec, 0x002501ff, 0x000e0204 },
    {   /* MPAL */  0x04651e39, 0x0000020d, 0x00040c11,
        0x0c190c1a, 0x006c02ec, 0x002501ff, 0x000e0204 },
    };

    #define RGBA(r,g,b,a)   (((r)<<11) | ((g)<<6) | ((b)<<1) | (a))
    #define RGBA32(rgba32)  RGBA((((rgba32)>>19) & 0x1F), (((rgba32)>>11) & 0x1F), ((rgba32>>3) & 0x1F), (((rgba32)>>31) & 1))

    uint16_t *fb_base = (uint16_t*)0xA0100000;
    volatile uint32_t* regs = (uint32_t*)0xA4400000;
    regs[1] = (uint32_t)fb_base;
    for (int i=0; i<320*240; i++) 
        fb_base[i] = RGBA32(0xCB2B40);

    regs[2] = 320;
    regs[12] = 0x200;
    regs[13] = 0x400;

#if FATAL_TEXT
    const int RES_WIDTH = 320;
    const int X = 40;
    const int Y = 40;
    const uint16_t COLOR = RGBA32(0xF0F0C9);

    fb_base += Y*RES_WIDTH + X;
    uint16_t *fb = fb_base;
    while (1) {
        char ch = io_read8((uint32_t)str); str++;
        if (!ch) break;
        const uint8_t *glyph = font + (ch-1)*5;
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
    int tv_type = io_read8(0xA4000009);
    bool ique = io_read8(0xA400000B);
    #pragma GCC unroll 0
    for (int reg=0; reg<7; reg++)
        regs[reg+5] = vi_regs_p[tv_type][reg];
    regs[0] = ique ? 0x1202 : 0x3202;
#endif
    abort();
}

__attribute__((used))
void stage2(void)
{
    debugf("Hello from RDRAM ", __builtin_frame_address(0));

    // Invalidate the stack1 area, where the first stage put its stack.
    // We don't need it anymore, and we don't want it to be flushed to RDRAM
    // that will be cleared anyway.
    data_cache_hit_invalidate(STACK1_BASE, STACK1_SIZE);

    // Search for the ELF header. We search for a 256-byte aligned header
    // starting at offset 0x1000 in the ROM area (after the IPL3).
    // We search for 64 MiB of ROM space (takes only a couple of seconds)
    uint32_t elf_header = 0x10001000;
    bool found_elf = false;
    for (int i=0; i<64*1024*1024/256; i++) {
        if (io_read32(elf_header) == ELF_MAGIC) {
            found_elf = true;
            break;
        }
        elf_header += 0x100;
    }
    if (!found_elf) {
        debugf("ELF header not found: make sure it is 256-byte aligned");
        fatal(MSG_ELF_NOT_FOUND);
        abort();
    }

    // Store the ELF offset in the boot flags
    *(uint32_t*)0xA400000C = elf_header << 8;

    // Check if the ELF is 32/64 bit, and if it's big/little endian
    uint32_t elf_type = io_read32(elf_header + 0x4);
    bool elf64 = (elf_type >> 24) == 2;
    if (((elf_type >> 16) & 0xff) == 1) {
        debugf("ELF: little endian ELFs are not supported");
        fatal(MSG_ELF_LITTLE_ENDIAN);
    }

    // Read program headers offset and number. Allocate space in the stack
    // for them.
    const int PHDR_SIZE = elf64 ? 0x38 : 0x20;
    uint32_t phoff = io_read32(elf_header + (elf64 ? 0x20+4 : 0x1C));
    int phnum = io_read16(elf_header + (elf64 ? 0x38 : 0x2C));
    uint32_t entrypoint = io_read32(elf_header + (elf64 ? 0x18+4 : 0x18));
    uint32_t *phdr = alloca_aligned(PHDR_SIZE * phnum);
    data_cache_hit_writeback_invalidate(phdr, PHDR_SIZE * phnum);

    // Load all the program headers
    pi_read_async((void*)phdr, elf_header + phoff, PHDR_SIZE * phnum);
    pi_wait();

    // Decompression function (if any)
    int (*decomp)(void *inbuf, int size, void *outbuf) = 0;

    // Load the program segments
    for (int i=0; i<phnum; i++, phdr+=PHDR_SIZE/4) {
        uint32_t offset = phdr[elf64 ? 3 : 1];
        uint32_t vaddr = phdr[elf64 ? 5 : 2];
        uint32_t paddr = phdr[elf64 ? 7 : 3];
        uint32_t size = phdr[elf64 ? 9 : 4];
        uint32_t flags = phdr[elf64 ? 1 : 6];

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
        if ((vaddr % 8) != 0) {
            debugf("ELF: vaddr is not 8-byte aligned in segment");
            fatal(MSG_ELF_VADDR_NOT_ALIGNED);
        }
        if ((offset % 2) != 0) {
            debugf("ELF: file offset is not 2-byte aligned in segment");
            fatal(MSG_ELF_OFFSET_NOT_ALIGNED);
        }

        debugf("Segment ", i, phdr[0], offset, vaddr, size, flags);

        // Load the segment into RDRAM. Notice that we don't need to clear
        // extra size at the end of the segment (as specified by phdr[5])
        // as the whole RDRAM has been cleared already.
        // Handle odd sizes by loading one byte more; this can happen with
        // compressed segments, where sometimes padding cannot be added 
        // without corrupting the decompression.
        int dma_size = size & 1 ? size+1 : size;
        pi_read_async((void*)vaddr, elf_header + offset, dma_size);

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
            uint32_t paddr_end = paddr + dec_size;
            uint32_t vaddr_end = vaddr + size;
            uint32_t a, b; bool clear = false;
            if (vaddr < paddr) {
                a=vaddr; b=MIN(vaddr_end, paddr);
                clear = true;
            } else if (vaddr_end > paddr_end) {
                a=MAX(vaddr, paddr_end); b=vaddr_end;
                clear = true;
            }
            if (clear) fast_bzero_range((void*)a, (void*)b);
        }

        // Wait for the DMA to finish.
        pi_wait();
    }

    // Reset the RCP hardware
    rcp_reset();

    // Write the accumulated entropy to the pool
    *(uint32_t*)0xA4000004 = entropy_get();
    debugf("Boot flags: ", *(uint32_t*)0xA4000000, *(uint32_t*)0xA4000004, *(uint32_t*)0xA4000008, *(uint32_t*)0xA400000C);

    // Jump to the ROM finish function
    stage3(entrypoint);
}

// This is the last stage of IPL3. It runs directly from ROM so that we are
// free of cleaning up our breadcrumbs in both DMEM and RDRAM.
__attribute__((far, noreturn))
void stage3(uint32_t entrypoint)
{
    // Notify the PIF that the boot process is finished. This will take a while
    // so start it in background.
    pif_terminate_boot();

    // Reset the CPU cache, so that the application starts from a pristine state
    cop0_clear_cache();

    // Read memory size from boot flags
    int memsize = *(volatile uint32_t*)0xA4000000;

    // Clear the reserved portion of RDRAM. To create a SP_WR_LEN value that works,
    // we assume the reserved size is a multiple of 1024. It can be made to work
    // also with other sizes, but this code will need to be adjusted.
    while (*SP_DMA_FULL) {}
    *SP_RSP_ADDR = 0xA4001000;
    *SP_DRAM_ADDR = memsize - TOTAL_RESERVED_SIZE;
    _Static_assert((TOTAL_RESERVED_SIZE % 1024) == 0, "TOTAL_RESERVED_SIZE must be multiple of 1024");
    *SP_WR_LEN = (((TOTAL_RESERVED_SIZE >> 10) - 1) << 12) | (1024-1);

    // Clear DMEM (leave only the boot flags area intact). Notice that we can't
    // call debugf anymore after this, because a small piece of debugging code
    // (io_write) is in DMEM, so it can't be used anymore.
    while (*SP_DMA_FULL) {}
    *SP_RSP_ADDR = 0xA4000010;
    *SP_DRAM_ADDR = 0x00802000;  // Area > 8 MiB which is guaranteed to be empty
    *SP_RD_LEN = 4096-16-1;

    // Wait until the PIF is done. This will also clear the interrupt, so that
    // we don't leave the interrupt pending when we go to the entrypoint.
    si_wait();

    // RSP DMA is guaranteed to be finished by now because stage3 is running from
    // ROM and it's very slow. Anyway, let's just wait to avoid bugs in the future,
    // because we don't want to begin using the stack (at the end of RDRAM) before it's finished.
    while (*SP_DMA_BUSY) {}

    // Configure SP at the end of RDRAM. This is a good default in general,
    // then of course userspace code is free to reconfigure it.
    asm ("move $sp, %0" : : "r" (0x80000000 + memsize - 0x10));

    goto *(void*)entrypoint;
}
