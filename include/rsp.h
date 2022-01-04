/**
 * @file rsp.h
 * @brief RSP - Programmable vector coprocessor
 * @ingroup rsp
 */
#ifndef __LIBDRAGON_RSP_H
#define __LIBDRAGON_RSP_H

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

/** @brief SP halted */
#define SP_STATUS_HALTED                (1 << 0)
/** @brief SP DMA busy */
#define SP_STATUS_DMA_BUSY              (1 << 2)
/** @brief SP IO busy */
#define SP_STATUS_IO_BUSY               (1 << 4)
/** @brief SP generate interrupt when hit a break instruction */
#define SP_STATUS_INTERRUPT_ON_BREAK    (1 << 6)

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
} rsp_ucode_t;

/**
 * @brief Define one RSP ucode compiled via libdragon's build system (n64.mk).
 * 
 * If you're using libdragon's build system (n64.mk), use DEFINE_RSP_UCODE() to
 * define one ucode coming from a rsp_*.S file. For instance, if you wrote
 * and compiled a ucode called rsp_math.S, you can use DEFINE_RSP_UCODE(rsp_math)
 * to define it at the global level. You can then use rsp_load(&rsp_math) 
 * to load it.
 */
#define DEFINE_RSP_UCODE(ucode_name) \
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

/** @brief Wait until RSP has finished processing. */
void rsp_wait(void);

/** @brief Do a DMA transfer to load a piece of code into RSP IMEM.
 * 
 * This is a lower-level function that actually executes a DMA transfer
 * from RDRAM to IMEM. Prefer using #rsp_load instead.
 * 
 * @note in order for this function to be interoperable with #rsp_load, it
 * will reset the last loaded ucode cache.
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

#ifdef __cplusplus
}
#endif

#endif
