/**
 * @file rsp.c
 * @brief Low-level RSP hardware library
 * @ingroup rsp
 */

#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rsp.h"
#include "debug.h"
#include "console.h"
#include "regsinternal.h"
#include "n64sys.h"
#include "interrupt.h"

/**
 * RSP crash handler ucode (rsp_crash.S)
 */
DEFINE_RSP_UCODE(rsp_crash);

/** @brief Static structure to address SP registers */
static volatile struct SP_regs_s * const SP_regs = (struct SP_regs_s *)0xa4040000;

/** @brief Current ucode being loaded */
static rsp_ucode_t *cur_ucode = NULL;

/**
 * @brief Wait until the SI is finished with a DMA request
 */
static void __SP_DMA_wait(void)
{
    while (SP_regs->status & (SP_STATUS_DMA_BUSY | SP_STATUS_IO_BUSY)) ;
}

void rsp_init(void)
{
    /* Make sure RSP is halted */
    *SP_PC = 0x1000;
    SP_regs->status = SP_WSTATUS_SET_HALT;
}

void rsp_load(rsp_ucode_t *ucode) {
    if (cur_ucode != ucode) {
        rsp_load_code(ucode->code, (uint8_t*)ucode->code_end - ucode->code, 0);
        rsp_load_data(ucode->data, (uint8_t*)ucode->data_end - ucode->data, 0);
        cur_ucode = ucode;
    }
}

void rsp_load_code(void* start, unsigned long size, unsigned int imem_offset)
{
    assert(((uint32_t)start % 8) == 0);
    assert((imem_offset % 8) == 0);

    disable_interrupts();
    __SP_DMA_wait();

    SP_regs->DRAM_addr = start;
    MEMORY_BARRIER();
    SP_regs->RSP_addr = SP_IMEM + imem_offset/4;
    MEMORY_BARRIER();
    SP_regs->rsp_read_length = size - 1;
    MEMORY_BARRIER();

    __SP_DMA_wait();
    enable_interrupts();
}

void rsp_load_data(void* start, unsigned long size, unsigned int dmem_offset)
{
    assert(((uint32_t)start % 8) == 0);
    assert((dmem_offset % 8) == 0);

    disable_interrupts();
    __SP_DMA_wait();

    SP_regs->DRAM_addr = start;
    MEMORY_BARRIER();
    SP_regs->RSP_addr = SP_DMEM + dmem_offset/4;
    MEMORY_BARRIER();
    SP_regs->rsp_read_length = size - 1;
    MEMORY_BARRIER();

    __SP_DMA_wait();
    enable_interrupts();
}

void rsp_read_code(void* start, unsigned long size, unsigned int imem_offset)
{
    assert(((uint32_t)start % 8) == 0);
    assert((imem_offset % 8) == 0);
    data_cache_hit_writeback_invalidate(start, size);

    disable_interrupts();
    __SP_DMA_wait();

    SP_regs->DRAM_addr = start;
    MEMORY_BARRIER();
    SP_regs->RSP_addr = SP_IMEM + imem_offset/4;
    MEMORY_BARRIER();
    SP_regs->rsp_write_length = size - 1;
    MEMORY_BARRIER();
    __SP_DMA_wait();

    enable_interrupts();
}


void rsp_read_data(void* start, unsigned long size, unsigned int dmem_offset)
{
    assert(((uint32_t)start % 8) == 0);
    assert((dmem_offset % 8) == 0);
    data_cache_hit_writeback_invalidate(start, size);

    disable_interrupts();
    __SP_DMA_wait();

    SP_regs->DRAM_addr = start;
    MEMORY_BARRIER();
    SP_regs->RSP_addr = SP_DMEM + dmem_offset/4;
    MEMORY_BARRIER();
    SP_regs->rsp_write_length = size - 1;
    MEMORY_BARRIER();
    __SP_DMA_wait();

    enable_interrupts();
}

void rsp_run_async(void)
{
    // set RSP program counter
    *SP_PC = cur_ucode ? cur_ucode->start_pc : 0;
    MEMORY_BARRIER();
    *SP_STATUS = SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_CLEAR_BROKE;
}

void rsp_wait(void)
{
    RSP_WAIT_LOOP(500) {
        if (*SP_STATUS & SP_STATUS_HALTED)
            break;
    }
    while (*SP_STATUS & (SP_STATUS_DMA_BUSY|SP_STATUS_DMA_FULL)) {}
}

void rsp_run(void)
{
    rsp_run_async();
    rsp_wait();
}

void rsp_pause(bool pause)
{
    if (pause) {
        // disable_interrupts();
        // do {
        //     // while ((*SP_STATUS & (SP_STATUS_DMA_BUSY|SP_STATUS_DMA_FULL))) {}
        //     MEMORY_BARRIER();
        //     *SP_STATUS = SP_WSTATUS_SET_HALT;
        //     MEMORY_BARRIER();
        //     while (!(*SP_STATUS & SP_STATUS_HALTED)) { }
        //     MEMORY_BARRIER();

        //     if (!(*SP_STATUS & (SP_STATUS_DMA_BUSY|SP_STATUS_DMA_FULL)))
        //         break;

        //     MEMORY_BARRIER();
        //     debugf("PANIC: RSP HALTED DURING DMA (PC: %lx)\n", *SP_PC);
        //     MEMORY_BARRIER();
        //     *SP_STATUS = SP_WSTATUS_CLEAR_HALT;
        // } while(1);
        // enable_interrupts();

        *SP_STATUS = SP_WSTATUS_SET_HALT;
        MEMORY_BARRIER();
        while (*SP_STATUS & (SP_STATUS_DMA_BUSY|SP_STATUS_DMA_FULL)) {}


        // // Wait until the DMA engine is idle. It's not allowed for CPU
        // // touch SP DMEM/IMEM while a DMA is in progress, so it's better to
        // // play safe here.
        // while (*SP_STATUS & (SP_STATUS_DMA_BUSY|SP_STATUS_DMA_FULL)) {
        //     MEMORY_BARRIER();
        //     while (*SP_STATUS & (SP_STATUS_DMA_BUSY|SP_STATUS_DMA_FULL))
        //     { /* spin-wait */ }
        //     MEMORY_BARRIER();
        //     wait_ticks(100);
        //     debugf("halt during DMA\n");
        // }
    } else {
        *SP_STATUS = SP_WSTATUS_CLEAR_SSTEP|SP_WSTATUS_CLEAR_HALT;
    }
}

__attribute__((noreturn))
void __rsp_crash(const char *file, int line, const char *func)
{
    rsp_snapshot_t state __attribute__((aligned(8)));
    rsp_ucode_t *uc = cur_ucode;

    // Disable interrupts right away. We're going to abort soon, so let's
    // avoid being preempted for any reason.
    disable_interrupts();

    // Read the status register right away. Its value can mutate at any time
    // so the earlier the better.
    uint32_t status = *SP_STATUS;
    MEMORY_BARRIER();

    // Initialize the console
    console_init();
    console_set_debug(true);
    console_set_render_mode(RENDER_MANUAL);

    // Forcibly halt the RSP, and wait also for the DMA engine to be idle
    *SP_STATUS = SP_WSTATUS_SET_HALT;
    while (!(*SP_STATUS & SP_STATUS_HALTED)) {}
    while (*SP_STATUS & (SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL)) {}
    MEMORY_BARRIER();

    // Read the current PC. This can only be read after the RSP is halted.
    uint32_t pc = *SP_PC;
    MEMORY_BARRIER();

    // Fetch DMEM, as we are going to modify it to read the register contents
    rsp_read_code(state.imem, 4096, 0);
    rsp_read_data(state.dmem, 4096, 0);

    // Load the crash handler into RSP, and run it. It will read all the
    // registers and save them into DMEM.
    rsp_load(&rsp_crash);
    rsp_run();
    rsp_read_data(&state, 764, 0);

    // Overwrite the status register information with the read we did at
    // the beginning of the handler
    state.cop0[4] = status;

    // Write the PC now so it doesn't get overwritten by the DMA
    state.pc = pc;

    // Dump information on the current ucode name and CPU point of crash
    const char *uc_name = uc ? uc->name : "???";
    char pcpos[120];
    snprintf(pcpos, 120, "%s (%s:%d)", func, file, line);
    pcpos[119] = 0;

    printf("RSP CRASH | %s | %.*s\n", uc_name, 49-strlen(uc_name), pcpos);

    printf("PC:%03lx | STATUS:%04lx [", state.pc, status);
    if (status & (1<<0)) printf("halt ");
    if (status & (1<<1)) printf("broke ");
    if (status & (1<<2)) printf("dma_busy ");
    if (status & (1<<3)) printf("dma_full ");
    if (status & (1<<4)) printf("io_full ");
    if (status & (1<<5)) printf("sstep ");
    if (status & (1<<6)) printf("irqbreak ");
    if (status & (1<<7)) printf("sig0 ");
    if (status & (1<<8)) printf("sig1 ");
    if (status & (1<<9)) printf("sig2 ");
    if (status & (1<<10)) printf("sig3 ");
    if (status & (1<<11)) printf("sig4 ");
    if (status & (1<<12)) printf("sig5 ");
    if (status & (1<<13)) printf("sig6 ");
    if (status & (1<<14)) printf("sig7 ");
    printf("]\n");

    // Dump GPRs
    printf("-------------------------------------------------GP Registers--\n");
    printf("zr:%08lX ",  state.gpr[0]);
    printf("at:%08lX ",  state.gpr[1]);
    printf("v0:%08lX ",  state.gpr[2]);
    printf("v1:%08lX ",  state.gpr[3]);
    printf("a0:%08lX\n", state.gpr[4]);
    printf("a1:%08lX ",  state.gpr[5]);
    printf("a2:%08lX ",  state.gpr[6]);
    printf("a3:%08lX ",  state.gpr[7]);
    printf("t0:%08lX ",  state.gpr[8]);
    printf("t1:%08lX\n", state.gpr[9]);
    printf("t2:%08lX ",  state.gpr[10]);
    printf("t3:%08lX ",  state.gpr[11]);
    printf("t4:%08lX ",  state.gpr[12]);
    printf("t5:%08lX ",  state.gpr[13]);
    printf("t6:%08lX\n", state.gpr[14]);
    printf("t7:%08lX ",  state.gpr[15]);
    printf("t8:%08lX ",  state.gpr[24]);
    printf("t9:%08lX ",  state.gpr[25]);
    printf("s0:%08lX ",  state.gpr[16]);
    printf("s1:%08lX\n", state.gpr[17]);
    printf("s2:%08lX ",  state.gpr[18]);
    printf("s3:%08lX ",  state.gpr[19]);
    printf("s4:%08lX ",  state.gpr[20]);
    printf("s5:%08lX ",  state.gpr[21]);
    printf("s6:%08lX\n", state.gpr[22]);
    printf("s7:%08lX ",  state.gpr[23]);
    printf("gp:%08lX ",  state.gpr[28]);
    printf("sp:%08lX ",  state.gpr[29]);
    printf("fp:%08lX ",  state.gpr[30]);
    printf("ra:%08lX \n", state.gpr[31]);

    // Dump VPRs, only to the debug log (no space on screen)
    debugf("-------------------------------------------------VP Registers--\n");
    for (int i=0;i<16;i++) {
        uint16_t *r = state.vpr[i*2];
        debugf("$v%02d:%04x %04x %04x %04x %04x %04x %04x %04x   ",
            i*2+0, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
        r += 8;
        debugf("$v%02d:%04x %04x %04x %04x %04x %04x %04x %04x\n",
            i*2+1, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
    }
    {        
        uint16_t *r = state.vaccum[0];
        debugf("acc_hi:%04x %04x %04x %04x %04x %04x %04x %04x\n",
            r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
        r += 8;
        debugf("acc_md:%04x %04x %04x %04x %04x %04x %04x %04x\n",
            r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
        r += 8;
        debugf("acc_lo:%04x %04x %04x %04x %04x %04x %04x %04x\n",
            r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
    }

    // Dump COP0 registers
    printf("-----------------------------------------------COP0 Registers--\n");
    printf("$c0  DMA_SPADDR    %08lx    |  ", *((volatile uint32_t*)0xA4040000));
    printf("$c8  DP_START      %08lx\n", *((volatile uint32_t*)0xA4100000));
    printf("$c1  DMA_RAMADDR   %08lx    |  ", *((volatile uint32_t*)0xA4040004));
    printf("$c9  DP_END        %08lx\n", *((volatile uint32_t*)0xA4100004));
    printf("$c2  DMA_READ      %08lx    |  ", *((volatile uint32_t*)0xA4040008));
    printf("$c10 DP_CURRENT    %08lx\n", *((volatile uint32_t*)0xA4100008));
    printf("$c3  DMA_WRITE     %08lx    |  ", *((volatile uint32_t*)0xA404000C));
    printf("$c11 DP_STATUS     %08lx\n", *((volatile uint32_t*)0xA410000C));
    printf("$c4  SP_STATUS     %08lx    |  ", *((volatile uint32_t*)0xA4040010));
    printf("$c12 DP_CLOCK      %08lx\n", *((volatile uint32_t*)0xA4100010));
    printf("$c5  DMA_FULL      %08lx    |  ", *((volatile uint32_t*)0xA4040014));
    printf("$c13 DP_BUSY       %08lx\n", *((volatile uint32_t*)0xA4100014));
    printf("$c6  DMA_BUSY      %08lx    |  ", *((volatile uint32_t*)0xA4040018));
    printf("$c14 DP_PIPE_BUSY  %08lx\n", *((volatile uint32_t*)0xA4100018));
    printf("$c7  SEMAPHORE     %08lx    |  ", *((volatile uint32_t*)0xA404001C));
    printf("$c15 DP_TMEM_BUSY  %08lx\n", *((volatile uint32_t*)0xA410001C));

    // Invoke ucode-specific crash handler, if defined. This will dump ucode-specific
    // information (possibly decoded from DMEM).
    if (uc->crash_handler) {
        printf("-----------------------------------------------Ucode data------\n");
        uc->crash_handler(&state);
    }

    // Full dump of DMEM into the debug log.
    debugf("DMEM:\n");
    for (int i = 0; i < 4096/16; i++) {
        uint8_t *d = state.dmem + i*16;
        debugf("%04x  ", i*16);
        for (int j=0;j<16;j++) {
            debugf("%02x ", d[j]);
            if (j==7) debugf(" ");
        }
        debugf("  |");
        for (int j=0;j<16;j++) debugf("%c", d[j] >= 32 && d[j] < 127 ? d[j] : '.');
        debugf("|\n");
    }

    // OK we're done. Render on the screen and abort
    console_render();
    abort();
}
