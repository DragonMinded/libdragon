/**
 * @file rsp.c
 * @brief Low-level RSP hardware library
 * @ingroup rsp
 */

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rsp.h"
#include "rdp.h"
#include "debug.h"
#include "console.h"
#include "regsinternal.h"
#include "n64sys.h"
#include "interrupt.h"
#include "rdpq/rdpq_debug_internal.h"

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

static void rsp_interrupt(void)
{
    #ifndef NDEBUG
    __rsp_check_assert(__FILE__, __LINE__, __func__);
    #endif
}

void rsp_init(void)
{
    /* Make sure RSP is halted */
    *SP_PC = 0x1000;
    SP_regs->status = SP_WSTATUS_SET_HALT;
    set_SP_interrupt(1);
    register_SP_handler(rsp_interrupt);
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

    if (cur_ucode != NULL) {
        cur_ucode = NULL;
    }

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

/** @brief Internal implementation of #rsp_run_async */
void __rsp_run_async(uint32_t status_flags)
{
    // set RSP program counter
    *SP_PC = cur_ucode ? cur_ucode->start_pc : 0;
    MEMORY_BARRIER();
    *SP_STATUS = SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_CLEAR_BROKE | status_flags;
}

void rsp_wait(void)
{
    RSP_WAIT_LOOP(500) {
        // Wait for the RSP to halt and the DMA engine to be idle.
        uint32_t status = *SP_STATUS;
        if (status & SP_STATUS_HALTED && 
            !(status & (SP_STATUS_DMA_BUSY|SP_STATUS_DMA_FULL)))
            break;
    }
}

void rsp_run(void)
{
    rsp_run_async();
    rsp_wait();
}

#ifndef NDEBUG
/// @cond
// Check if the RSP has hit an internal assert, and call rsp_crash if so.
// This function is invoked by #RSP_WAIT_LOOP while waiting for the RSP
// to finish a task, so that we immediately show a crash screen if the RSP
// has hit an assert.
void __rsp_check_assert(const char *file, int line, const char *func)
{
    // If it's running, it has not asserted
    if (!(*SP_STATUS & (SP_STATUS_HALTED | SP_STATUS_BROKE)))
        return;

    // We need to check if the RSP has reached the assert loop. We do
    // this by inspecting IMEM, which cannot be done while a DMA is in
    // progress. Since this is a best-effort fast-path to a RSP crash,
    // we can simply punt if a DMA is in progress.
    // TODO: figure out a better way to know the PC address of the RSP
    // assert loop.
    if (*SP_STATUS & (SP_STATUS_DMA_BUSY | SP_STATUS_IO_BUSY))
        return;

    // Detect infinite break loop
    if (SP_IMEM[(*SP_PC >> 2) + 1] == 0x00BA000D) {
        __rsp_crash(file, line, func, NULL);
    }
}
/// @endcond
#endif /* NDEBUG */

#ifndef NDEBUG
/// @cond
// RSP crash handler implementation
__attribute__((noreturn, format(printf, 4, 5)))
void __rsp_crash(const char *file, int line, const char *func, const char *msg, ...)
{
    volatile uint32_t *DP_REGS = (volatile uint32_t*)0xA4100000;
    volatile uint32_t *SP_REGS = (volatile uint32_t*)0xA4040000;

    rsp_snapshot_t state __attribute__((aligned(8)));
    rsp_ucode_t *uc = cur_ucode;

    // Disable interrupts right away. We're going to abort soon, so let's
    // avoid being preempted for any reason.
    disable_interrupts();

    // Read the status registers right away. Its value can mutate at any time
    // so the earlier the better.
    uint32_t sp_status = *SP_STATUS;
    uint32_t dp_status = *DP_STATUS;
    MEMORY_BARRIER();

    // Now read all SP registers. Most of them are DMA-related so the earlier
    // we read them the better. We can't freeze the DMA transfer so they might
    // be slightly incoherent.
    uint32_t sp_regs[8], dp_regs[8];
    for (int i=0;i<8;i++) {
        sp_regs[i] = i==4 ? sp_status : SP_REGS[i];
        dp_regs[i] = i==3 ? dp_status : DP_REGS[i];
    }
    MEMORY_BARRIER();

    // Forcibly halt the RSP, and wait also for the DMA engine to be idle
    *SP_STATUS = SP_WSTATUS_SET_HALT;
    while (!(*SP_STATUS & SP_STATUS_HALTED)) {}
    while (*SP_STATUS & (SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL)) {}
    MEMORY_BARRIER();

    // We now need to check whether the RDP has crashed. We need to send a
    // DMA transfer (unless one is already going)
    uint64_t dummy_rdp_command = 0x2700000000000000ull; // sync pipe
    if (!(dp_status & (DP_STATUS_DMA_BUSY | DP_STATUS_START_VALID | DP_STATUS_END_VALID))) {
        data_cache_hit_writeback_invalidate(&dummy_rdp_command, sizeof(dummy_rdp_command));
        *DP_START = PhysicalAddr(&dummy_rdp_command);
        *DP_END = PhysicalAddr(&dummy_rdp_command + 1);
    }
    // Check if there are any progresses in DP_CURRENT
    for (int i=0; i<20 && *DP_CURRENT == dp_regs[2]; i++)
        wait_ms(5);
    bool rdp_crashed = *DP_CURRENT == dp_regs[2];

    // Freeze the RDP
    *DP_STATUS = 1<<3;

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
 
    // Overwrite the status register information with the reads we did at
    // the beginning of the handler.
    // FIXME: maybe not read these anymore from the RSP?
    for (int i=0;i<8;i++) {
        state.cop0[i+0] = sp_regs[i];
        state.cop0[i+8] = dp_regs[i];
    }

    // Write the PC now so it doesn't get overwritten by the DMA
    state.pc = pc;

    // Initialize the console
    console_init();
    console_set_debug(true);
    console_set_render_mode(RENDER_MANUAL);

    // If the validator is active, this is a good moment to flush its buffered
    // output. This could also trigger a RDP crash (which might be the
    // underlying cause for the RSP crash), so better try that before start
    // filling the output buffer.
    if (rdpq_trace) rdpq_trace();

    // Dump information on the current ucode name and CPU point of crash
    const char *uc_name = uc ? uc->name : "???";
    char pcpos[120];
    snprintf(pcpos, 120, "%s (%s:%d)", func, file, line);
    pcpos[119] = 0;

    printf("RSP CRASH | %s | %.*s\n", uc_name, 48-strlen(uc_name), pcpos);

    // Display the optional message coming from the C code
    if (msg)
    {
        printf("Crash symptom: ");
        va_list args;
        va_start(args, msg);
        vprintf(msg, args);
        va_end(args);
        printf("\n");
    }

    // Check if the RDP crashed. If the RDP crashed while the validator was active,
    // theoretically it should have caught it before (in the rdpq_trace above),
    // so we shouldn't event get here.
    // Still, there are a few cases where this can happen:
    //   * the validator could be disabled
    //   * some unknown RDP crash conditions not yet handled by the validator
    //   * some race condition for which the validator missed the command that
    //     triggered the crash
    //   * validator asserts could be disabled, in which case we dumped the crash
    //     condition in the debug output, but we still get here.
    // NOTE: unfortunately, RDP doesn't always sets the FREEZE bit when it crashes
    // (it is unknown why sometimes it doesn't). So this is just a best effort to
    // highlight the presence of the important FREEZE bit in DP STATUS that could
    // otherwise go unnoticed.
    if (rdp_crashed) {
        printf("RDP CRASHED: the code triggered a RDP hardware bug.\n");
        printf("Use the rdpq validator (rdpq_debug_start()) to analyze.\n");
    }

    // Check if a RSP assert triggered. We check that we reached an
    // infinite loop with the break instruction in the delay slot.
    if (*(uint32_t*)(&state.imem[pc+4]) == 0x00BA000D) {
        // The at register ($1) contains the assert code in the top 16 bits.
        uint16_t code = state.gpr[1] >> 16;
        printf("RSP ASSERTION FAILED (0x%x)", code);

        if (uc && uc->assert_handler) {
            printf(" - ");
            uc->assert_handler(&state, code);
        } else {
            printf("\n");
        }
    }

    printf("PC:%03lx | STATUS:%4lx [", state.pc, sp_status);
    if (sp_status & (1<<0))  printf("halt ");
    if (sp_status & (1<<1))  printf("broke ");
    if (sp_status & (1<<2))  printf("dma_busy ");
    if (sp_status & (1<<3))  printf("dma_full ");
    if (sp_status & (1<<4))  printf("io_full ");
    if (sp_status & (1<<5))  printf("sstep ");
    if (sp_status & (1<<6))  printf("irqbreak ");
    if (sp_status & (1<<7))  printf("sig0 ");
    if (sp_status & (1<<8))  printf("sig1 ");
    if (sp_status & (1<<9))  printf("sig2 ");
    if (sp_status & (1<<10)) printf("sig3 ");
    if (sp_status & (1<<11)) printf("sig4 ");
    if (sp_status & (1<<12)) printf("sig5 ");
    if (sp_status & (1<<13)) printf("sig6 ");
    if (sp_status & (1<<14)) printf("sig7 ");
    printf("] | DP_STATUS:%4lx [", dp_status);
    if (dp_status & (1<<0))  printf("xbus ");
    if (dp_status & (1<<1))  printf("freeze ");
    if (dp_status & (1<<2))  printf("flush ");
    if (dp_status & (1<<3))  printf("gclk ");
    if (dp_status & (1<<4))  printf("tmem ");
    if (dp_status & (1<<5))  printf("pipe ");
    if (dp_status & (1<<6))  printf("busy ");
    if (dp_status & (1<<7))  printf("ready ");
    if (dp_status & (1<<8))  printf("dma ");
    if (dp_status & (1<<9))  printf("start ");
    if (dp_status & (1<<10)) printf("end ");
    printf("]\n");

    // Dump GPRs
    printf("-------------------------------------------------GP Registers--\n");
    printf("zr:%08lX ",  state.gpr[0]);  printf("at:%08lX ",  state.gpr[1]);
    printf("v0:%08lX ",  state.gpr[2]);  printf("v1:%08lX ",  state.gpr[3]);
    printf("a0:%08lX\n", state.gpr[4]);  printf("a1:%08lX ",  state.gpr[5]);
    printf("a2:%08lX ",  state.gpr[6]);  printf("a3:%08lX ",  state.gpr[7]);
    printf("t0:%08lX ",  state.gpr[8]);  printf("t1:%08lX\n", state.gpr[9]);
    printf("t2:%08lX ",  state.gpr[10]); printf("t3:%08lX ",  state.gpr[11]);
    printf("t4:%08lX ",  state.gpr[12]); printf("t5:%08lX ",  state.gpr[13]);
    printf("t6:%08lX\n", state.gpr[14]); printf("t7:%08lX ",  state.gpr[15]);
    printf("t8:%08lX ",  state.gpr[24]); printf("t9:%08lX ",  state.gpr[25]);
    printf("s0:%08lX ",  state.gpr[16]); printf("s1:%08lX\n", state.gpr[17]);
    printf("s2:%08lX ",  state.gpr[18]); printf("s3:%08lX ",  state.gpr[19]);
    printf("s4:%08lX ",  state.gpr[20]); printf("s5:%08lX ",  state.gpr[21]);
    printf("s6:%08lX\n", state.gpr[22]); printf("s7:%08lX ",  state.gpr[23]);
    printf("gp:%08lX ",  state.gpr[28]); printf("sp:%08lX ",  state.gpr[29]);
    printf("fp:%08lX ",  state.gpr[30]); printf("ra:%08lX\n", state.gpr[31]);

    // Dump VPRs, only to the debug log (no space on screen)
    debugf("-------------------------------------------------VP Registers--\n");
    for (int i=0;i<16;i++) {
        uint16_t *r = state.vpr[i];
        debugf("$v%02d:%04x %04x %04x %04x %04x %04x %04x %04x   ",
            i, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
        r += 16*8;
        debugf("$v%02d:%04x %04x %04x %04x %04x %04x %04x %04x\n",
            i+16, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
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
    printf("$c0  DMA_SPADDR    %08lx    |  ", state.cop0[0]);
    printf("$c8  DP_START      %08lx\n",      state.cop0[8]);
    printf("$c1  DMA_RAMADDR   %08lx    |  ", state.cop0[1]);
    printf("$c9  DP_END        %08lx\n",      state.cop0[9]);
    printf("$c2  DMA_READ      %08lx    |  ", state.cop0[2]);
    printf("$c10 DP_CURRENT    %08lx\n",      state.cop0[10]);
    printf("$c3  DMA_WRITE     %08lx    |  ", state.cop0[3]);
    printf("$c11 DP_STATUS     %08lx\n",      state.cop0[11]);
    printf("$c4  SP_STATUS     %08lx    |  ", state.cop0[4]);
    printf("$c12 DP_CLOCK      %08lx\n",      state.cop0[12]);
    printf("$c5  DMA_FULL      %08lx    |  ", state.cop0[5]);
    printf("$c13 DP_BUSY       %08lx\n",      state.cop0[13]);
    printf("$c6  DMA_BUSY      %08lx    |  ", state.cop0[6]);
    printf("$c14 DP_PIPE_BUSY  %08lx\n",      state.cop0[14]);
    printf("$c7  SEMAPHORE     %08lx    |  ", state.cop0[7]);
    printf("$c15 DP_TMEM_BUSY  %08lx\n",      state.cop0[15]);

    // Invoke ucode-specific crash handler, if defined. This will dump ucode-specific
    // information (possibly decoded from DMEM).
    if (uc && uc->crash_handler) {
        printf("-----------------------------------------------Ucode data------\n");
        uc->crash_handler(&state);
    }

    // Backtrace
    debug_backtrace();

    // Full dump of DMEM into the debug log.
    debugf("DMEM:\n");
    debug_hexdump(state.dmem, 4096);

    // OK we're done. Render on the screen and abort
    console_render();
    abort();
}
/// @endcond
#endif /* NDEBUG */

extern inline void rsp_run_async(void);
