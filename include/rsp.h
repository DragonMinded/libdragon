/**
 * @file rsp.h
 * @brief Low-level RSP hardware library
 * @ingroup rsp
 */

/**
 * @defgroup rsp RSP interface
 * @ingroup lowlevel
 * @brief RSP basic library and command queue
 * 
 * This module is made of two libraries:
 * 
 *  * rsp.h and rsp.c: low-level routines to manipulate the RSP
 *  * rspq.h and rspq.c: RSP command queue for efficient task processing by
 *    multiple libraries
 * 
 */

#ifndef __LIBDRAGON_RSP_H
#define __LIBDRAGON_RSP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief RSP DMEM: 4K of data memory */
#define SP_DMEM       ((volatile uint32_t*)0xA4000000)

/** @brief RSP IMEM: 4K of instruction memory */
#define SP_IMEM       ((volatile uint32_t*)0xA4001000)

/** @brief Current SP program counter */
#define SP_PC         ((volatile uint32_t*)0xA4080000)

/** @brief SP status register */
#define SP_STATUS     ((volatile uint32_t*)0xA4040010)

/** @brief SP DMA full register */
#define SP_DMA_FULL   ((volatile uint32_t*)0xA4040014)

/** @brief SP DMA busy register */
#define SP_DMA_BUSY   ((volatile uint32_t*)0xA4040018)

/** @brief SP semaphore register */
#define SP_SEMAPHORE  ((volatile uint32_t*)0xA404001C)

/** @brief SP halted */
#define SP_STATUS_HALTED                (1 << 0)
/** @brief SP executed a break instruction */
#define SP_STATUS_BROKE                 (1 << 1)
/** @brief SP DMA busy */
#define SP_STATUS_DMA_BUSY              (1 << 2)
/** @brief SP DMA full */
#define SP_STATUS_DMA_FULL              (1 << 3)
/** @brief SP IO busy */
#define SP_STATUS_IO_BUSY               (1 << 4)
/** @brief SP is in single step mode */
#define SP_STATUS_SSTEP                 (1 << 5)
/** @brief SP generate interrupt when hit a break instruction */
#define SP_STATUS_INTERRUPT_ON_BREAK    (1 << 6)
/** @brief SP signal 0 is set */
#define SP_STATUS_SIG0                  (1 << 7)
/** @brief SP signal 1 is set */
#define SP_STATUS_SIG1                  (1 << 8)
/** @brief SP signal 2 is set */
#define SP_STATUS_SIG2                  (1 << 9)
/** @brief SP signal 3 is set */
#define SP_STATUS_SIG3                  (1 << 10)
/** @brief SP signal 4 is set */
#define SP_STATUS_SIG4                  (1 << 11)
/** @brief SP signal 5 is set */
#define SP_STATUS_SIG5                  (1 << 12)
/** @brief SP signal 6 is set */
#define SP_STATUS_SIG6                  (1 << 13)
/** @brief SP signal 7 is set */
#define SP_STATUS_SIG7                  (1 << 14)

#define SP_WSTATUS_CLEAR_HALT        0x00001   ///< SP_STATUS write mask: clear #SP_STATUS_HALTED bit
#define SP_WSTATUS_SET_HALT          0x00002   ///< SP_STATUS write mask: set #SP_STATUS_HALTED bit
#define SP_WSTATUS_CLEAR_BROKE       0x00004   ///< SP_STATUS write mask: clear BROKE bit
#define SP_WSTATUS_CLEAR_INTR        0x00008   ///< SP_STATUS write mask: clear INTR bit
#define SP_WSTATUS_SET_INTR          0x00010   ///< SP_STATUS write mask: set HALT bit
#define SP_WSTATUS_CLEAR_SSTEP       0x00020   ///< SP_STATUS write mask: clear SSTEP bit
#define SP_WSTATUS_SET_SSTEP         0x00040   ///< SP_STATUS write mask: set SSTEP bit
#define SP_WSTATUS_CLEAR_INTR_BREAK  0x00080   ///< SP_STATUS write mask: clear #SP_STATUS_INTERRUPT_ON_BREAK bit
#define SP_WSTATUS_SET_INTR_BREAK    0x00100   ///< SP_STATUS write mask: set SSTEP bit
#define SP_WSTATUS_CLEAR_SIG0        0x00200   ///< SP_STATUS write mask: clear SIG0 bit
#define SP_WSTATUS_SET_SIG0          0x00400   ///< SP_STATUS write mask: set SIG0 bit
#define SP_WSTATUS_CLEAR_SIG1        0x00800   ///< SP_STATUS write mask: clear SIG1 bit
#define SP_WSTATUS_SET_SIG1          0x01000   ///< SP_STATUS write mask: set SIG1 bit
#define SP_WSTATUS_CLEAR_SIG2        0x02000   ///< SP_STATUS write mask: clear SIG2 bit
#define SP_WSTATUS_SET_SIG2          0x04000   ///< SP_STATUS write mask: set SIG2 bit
#define SP_WSTATUS_CLEAR_SIG3        0x08000   ///< SP_STATUS write mask: clear SIG3 bit
#define SP_WSTATUS_SET_SIG3          0x10000   ///< SP_STATUS write mask: set SIG3 bit
#define SP_WSTATUS_CLEAR_SIG4        0x20000   ///< SP_STATUS write mask: clear SIG4 bit
#define SP_WSTATUS_SET_SIG4          0x40000   ///< SP_STATUS write mask: set SIG4 bit
#define SP_WSTATUS_CLEAR_SIG5        0x80000   ///< SP_STATUS write mask: clear SIG5 bit
#define SP_WSTATUS_SET_SIG5          0x100000  ///< SP_STATUS write mask: set SIG5 bit
#define SP_WSTATUS_CLEAR_SIG6        0x200000  ///< SP_STATUS write mask: clear SIG6 bit
#define SP_WSTATUS_SET_SIG6          0x400000  ///< SP_STATUS write mask: set SIG6 bit
#define SP_WSTATUS_CLEAR_SIG7        0x800000  ///< SP_STATUS write mask: clear SIG7 bit
#define SP_WSTATUS_SET_SIG7          0x1000000 ///< SP_STATUS write mask: set SIG7 bit

/**
 * @brief Snapshot of the register status of the RSP.
 * 
 * This structure is used in the crash handler.
 */
typedef struct {
    uint32_t gpr[32];           ///< General purpose registers
    uint16_t vpr[32][8];        ///< Vector registers
    uint16_t vaccum[3][8];      ///< Vector accumulator
    uint32_t cop0[16];          ///< COP0 registers (note: reg 4 is SP_STATUS)
    uint32_t cop2[3];           ///< COP2 control registers
    uint32_t pc;                ///< Program counter
    uint8_t dmem[4096] __attribute__((aligned(8)));  ///< Contents of DMEM
    uint8_t imem[4096] __attribute__((aligned(8)));  ///< Contents of IMEM
} rsp_snapshot_t;

/**
 * @brief RSP ucode definition.
 * 
 * This small structure holds the text/data pointers to a RSP ucode program
 * in RDRAM. It also contains the name (for the debugging purposes) and
 * the initial PC (usually 0).
 * 
 * If you're using libdragon's build system (n64.mk), use DEFINE_RSP_UCODE()
 * to initialize one of these.
 */
typedef struct {
    uint8_t *code;         ///< Pointer to the code segment
    void    *code_end;     ///< Pointer past the end of the code segment
    uint8_t *data;         ///< Pointer to the data segment
    void    *data_end;     ///< Pointer past the end of the data segment

    const char *name;      ///< Name of the ucode
    uint32_t start_pc;     ///< Initial RSP PC

    /**
     * @brief Custom crash handler. 
     * 
     * If specified, this function is invoked when a RSP crash happens,
     * while filling the information screen. It can be used to dump
     * custom ucode-specific information.
     */
    void (*crash_handler)(rsp_snapshot_t *state);

    /**
     * @brief Custom assert handler.
     * 
     * If specified, this function is invoked when a RSP crash caused
     * by an assert is triggered. This function should display information
     * related to the assert using `printf` (max 2 lines).
     * 
     * Normally, the first line will be the assert message associated with
     * the code (eg: "Invalid buffer pointer"), while the optional second line 
     * can contain a dump of a few important variables, maybe extracted from
     * the register state (eg: "bufptr=0x00000000 prevptr=0x8003F780").
     * The assert handler will now which registers to inspect to extract
     * information, given the exact position of the assert in the code.
     * 
     * @note The crash handler, if specified, is called for all crashes,
     * including asserts. That is the correct place where dump information
     * on the ucode state in general.
     */
    void (*assert_handler)(rsp_snapshot_t *state, uint16_t assert_code);
} rsp_ucode_t;

/**
 * @brief Define one RSP ucode compiled via libdragon's build system (n64.mk).
 * 
 * If you're using libdragon's build system (n64.mk), use DEFINE_RSP_UCODE() to
 * define one ucode coming from a rsp_*.S file. For instance, if you wrote
 * and compiled a ucode called rsp_math.S, you can use DEFINE_RSP_UCODE(rsp_math)
 * to define it at the global level. You can then use rsp_load(&rsp_math) 
 * to load it.
 * 
 * To statically define attributes of the ucode, you can use the C designated
 * initializer syntax.
 * 
* @code{.c}
 *      // Define the RSP ucode stored in file rsp_math.S.
 *      // For the sake of this example, we also show how to set the member
 *      // start_pc at definition time. You normally don't need to change this
 *      // as most ucode will start at 0x0 anyway (which is the default).
 *      DEFINE_RSP_UCODE(&rsp_math, .start_pc = 0x100);
 * @endcode
  * 
 */
#define DEFINE_RSP_UCODE(ucode_name, ...) \
    extern uint8_t ucode_name ## _text_start[]; \
    extern uint8_t ucode_name ## _data_start[]; \
    extern uint8_t ucode_name ## _text_end[0]; \
    extern uint8_t ucode_name ## _data_end[0]; \
    rsp_ucode_t ucode_name = (rsp_ucode_t){ \
        .code = ucode_name ## _text_start, \
        .data = ucode_name ## _data_start, \
        .code_end = ucode_name ## _text_end, \
        .data_end = ucode_name ## _data_end, \
        .name = #ucode_name, .start_pc = 0, \
        .crash_handler = 0, .assert_handler = 0, \
        __VA_ARGS__ \
    }

/** @brief Initialize the RSP subsytem. */
void rsp_init(void);

/** @brief Load a RSP ucode.
 * 
 * This function allows to load a RSP ucode into the RSP internal memory.
 * The function executes the transfer right away, so it is responsibility
 * of the caller making sure that it's a good time to do it.
 * 
 * The function internally keeps a pointer to the last loaded ucode. If the
 * ucode passed is the same, it does nothing. This makes it easier to write
 * code that optimistically switches between different ucodes, but without
 * forcing transfers every time.
 * 
 * @param[in]     ucode       Ucode to load into RSP
 **/
void rsp_load(rsp_ucode_t *ucode);

/** @brief Run RSP ucode.
 * 
 * This function starts running the RSP, and wait until the ucode is finished.
 */
void rsp_run(void);

/** @brief Run RSP async.
 * 
 * This function starts running the RSP in background. Use rsp_wait() to
 * synchronize later.
 */
void rsp_run_async(void);

/** 
 * @brief Wait until RSP has finished processing. 
 *
 * This function will wait until the RSP is halted. It contains a fixed
 * timeout of 500 ms, after which #rsp_crash is invoked to abort the program.
 */
void rsp_wait(void);

/** @brief Do a DMA transfer to load a piece of code into RSP IMEM.
 * 
 * This is a lower-level function that actually executes a DMA transfer
 * from RDRAM to IMEM. Prefer using rsp_load instead.
 * 
 * @param[in]     code          Pointer to buffer in RDRAM containing code. 
 *                              Must be aligned to 8 bytes.
 * @param[in]     size          Size of the code to load. Must be a multiple of 8.
 * @param[in]     imem_offset   Byte offset in IMEM where to load the code.
 *                              Must be a multiple of 8.
 */
void rsp_load_code(void* code, unsigned long size, unsigned int imem_offset);

/** @brief Do a DMA transfer to load a piece of data into RSP DMEM.
 * 
 * This is a lower-level function that actually executes a DMA transfer
 * from RDRAM to DMEM. Prefer using rsp_load instead.
 * 
 * @param[in]     data          Pointer to buffer in RDRAM containing data. 
 *                              Must be aligned to 8 bytes.
 * @param[in]     size          Size of the data to load. Must be a multiple of 8.
 * @param[in]     dmem_offset   Offset in DMEM where to load the code.
 *                              Must be a multiple of 8.
 */
void rsp_load_data(void* data, unsigned long size, unsigned int dmem_offset);


/** @brief Do a DMA transfer to load a piece of code from RSP IMEM to RDRAM.
 * 
 * This is a lower-level function that actually executes a DMA transfer
 * from IMEM to RDRAM.
 * 
 * @param[in]     code          Pointer to buffer in RDRAM where to write code. 
 *                              Must be aligned to 8 bytes.
 * @param[in]     size          Size of the code to load. Must be a multiple of 8.
 * @param[in]     imem_offset   Byte offset in IMEM where where the code will
 *                              be loaded from. Must be a multiple of 8.
 */
void rsp_read_code(void* code, unsigned long size, unsigned int imem_offset);

/** @brief Do a DMA transfer to load a piece of data from RSP DMEM to RDRAM.
 * 
 * This is a lower-level function that actually executes a DMA transfer
 * from DMEM to RDRAM.
 * 
 * @param[in]     data          Pointer to buffer in RDRAM where to write data. 
 *                              Must be aligned to 8 bytes.
 * @param[in]     size          Size of the data to load. Must be a multiple of 8.
 * @param[in]     dmem_offset   Byte offset in IMEM where where the data will
 *                              be loaded from. Must be a multiple of 8.
 */
void rsp_read_data(void* data, unsigned long size, unsigned int dmem_offset);

/**
 * @brief Pause RSP execution.
 * 
 * This function pauses the RSP. It also waits until any current SP DMA
 * is finished so that the RSP unit is fully idle when this function returns
 * and is then possible to run SP DMA or access IMEM/DMEM without any bus
 * conflict.
 *
 * @param[in]  pause  If true, RSP will be paused. If false, it will resume execution.
 */
void rsp_pause(bool pause);

/**
 * @brief Abort the program showing a RSP crash screen
 * 
 * This function aborts the execution of the program, and shows an exception
 * screen which contains the RSP status. 
 * 
 * This function (and its sibling #rsp_crashf) should be invoked whenever the
 * CPU realizes that the RSP is severely misbehaving, as it provides useful
 * information on the RSP status that can help tracking down the bug. It is
 * invoked automatically by this library (and others RSP libraries that build upon)
 * whenever internal consistency checks fail. It is also invoked as part
 * of `RSP_WAIT_LOOP` when the timeout is reached, which is the most common
 * way of detecting RSP misbehavior.
 * 
 * If the RSP has hit an assert, the crash screen will display the assert-
 * specific information (like assert code and assert message).
 * 
 * To display ucode-specific information (like structural decoding of DMEM data),
 * this function will call the function crash_handler in the current #rsp_ucode_t,
 * if it is defined. 
 * 
 * @see #rsp_crashf
 */
#define rsp_crash()  ({ \
    __rsp_crash(__FILE__, __LINE__, __func__, NULL); \
}) 

/**
 * @brief Abort the program showing a RSP crash screen with a symptom message.
 * 
 * This function is similar to #rsp_crash, but also allows to specify a message
 * that will be displayed in the crash screen. Since the CPU is normally
 * unaware of the exact reason why the RSP has crashed, the message is
 * possibly just a symptom as observed by the CPU (eg: "timeout reached",
 * "signal was not set"), and is in fact referred as "symptom" in the RSP crash
 * screen.
 * 
 * See #rsp_crash for more information on when to call this function and how
 * it can be useful.
 * 
 * @see #rsp_crash
 */
#define rsp_crashf(msg, ...) ({ \
    __rsp_crash(__FILE__, __LINE__, __func__, msg, ##__VA_ARGS__); \
})

/**
 * @brief Create a loop that waits for some condition that is related to RSP,
 *        aborting with a RSP crash after a timeout.
 *
 * This macro simplifies the creation of a loop that busy-waits for operations
 * performed by the RSP. If the condition is not reached within a timeout,
 * it is assumed that the RSP has crashed or otherwise stalled and
 * #rsp_crash is invoked to abort the program showing a debugging screen.
 * 
 * @code{.c}
 *      // This example shows a loop that waits for the RSP to set signal 2
 *      // in the status register. It is just an example on how to use the
 *      // macro.
 *      
 *      RSP_WAIT_LOOP(150) {
 *          if (*SP_STATUS & SP_STATUS_SIG_2)
 *              break;
 *      }
 * @endcode
 *
 * @param[in]  timeout_ms  Allowed timeout in milliseconds. Normally a value
 *                         like 150 is good enough because it is unlikely that
 *                         the application should wait for such a long time.
 * 
 */
#define RSP_WAIT_LOOP(timeout_ms) \
    for (uint32_t __t = TICKS_READ() + TICKS_FROM_MS(timeout_ms); \
         TICKS_BEFORE(TICKS_READ(), __t) || (rsp_crashf("wait loop timed out (%d ms)", timeout_ms), false); \
         __rsp_check_assert(__FILE__, __LINE__, __func__))

static inline __attribute__((deprecated("use rsp_load_code instead")))
void load_ucode(void * start, unsigned long size) {
    rsp_load_code(start, size, 0);
}

static inline __attribute__((deprecated("use rsp_read_code instead")))
void read_ucode(void * start, unsigned long size) {    
    rsp_read_code(start, size, 0);
}

static inline __attribute__((deprecated("use rsp_load_data instead"))) 
void load_data(void * start, unsigned long size) {
    rsp_load_data(start, size, 0);
}

static inline __attribute__((deprecated("use rsp_read_data instead"))) 
void read_data(void * start, unsigned long size) {
    rsp_read_data(start, size, 0);
}

static inline __attribute__((deprecated("use rsp_run_async instead"))) 
void run_ucode(void) {
    rsp_run_async();
}

static inline void rsp_semaphore_wait()
{
    while (*SP_SEMAPHORE);
}

static inline void rsp_semaphore_release()
{
    *SP_SEMAPHORE = 0;
}

// Internal function used by rsp_crash and rsp_crashf
/// @cond
void __rsp_crash(const char *file, int line, const char *func, const char *msg, ...)
   __attribute__((noreturn, format(printf, 4, 5)));
void __rsp_check_assert(const char *file, int line, const char *func);
/// @endcond

#ifdef __cplusplus
}
#endif

#endif
