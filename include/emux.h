/**
 * @file emux.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Implementation of the N64 emulator extensions
 * @ingroup emux
 * @defgroup emux emux
 * 
 * EMUX is an open spec for emulators to provide extensions to the emulated
 * N64 system, allowing advanced profiling and debugging features.
 * https://hackmd.io/SOLBoD6XSn-_WRdwtlMs9A
 * 
 */

#ifndef LIBDRAGON_EMUX_H
#define LIBDRAGON_EMUX_H

///@cond
#ifndef __ASSEMBLER__
#include <stdint.h>
#include <stddef.h>
#define cast64(x) (uint64_t)(x)
#else
#define cast64(x) x
#endif
///@endcond


/** @brief Assemble a single emux opcode */
#define EMUX_OP(ext, rd, rt, code) \
    ((0b010000 << 26) | (1 << 25) | ((ext) & 0x3F) | \
    (((rd) & 0x1F) << 20) | (((rt) & 0x1F) << 15) | (((code) & 0x1FF) << 6))

/** 
 * @name EMUX opcodes
 * @{
 */
#define EMUX_XDETECT(rd, code)          EMUX_OP(0x20,   rd,      0,  code)  ///< Detect EMUX presence
#define EMUX_XBREAK()                   EMUX_OP(0x21,    0,      0, 0x000)  ///< Trigger a breakpoint
#define EMUX_XBREAKPOINT(addr, op)      EMUX_OP(0x22, addr,      0,    op)  ///< Configure a breakpoint/watchpoint
#define EMUX_XTRACESTART(count)         EMUX_OP(0x23,    0,      0, count)  ///< Start tracing
#define EMUX_XTRACESTOP()               EMUX_OP(0x24,    0,      0, 0x000)  ///< Stop tracing
#define EMUX_XLOG(addr, len, code)      EMUX_OP(0x25, addr,    len,  code)  ///< Log a string
#define EMUX_XLOGREGS(mask, code)       EMUX_OP(0x26, mask,      0,  code)  ///< Log registers
#define EMUX_XHEXDUMP(addr, len)        EMUX_OP(0x27, addr,    len, 0x000)  ///< Hexdump memory region
#define EMUX_XPROF(slot, code)          EMUX_OP(0x28, slot,      0,  code)  ///< Control profiler
#define EMUX_XPROFREAD(slot, metric)    EMUX_OP(0x29, slot, metric, 0x000)  ///< Read profiler metric
#define EMUX_XEXCEPTION(mask)           EMUX_OP(0x2A,    0,   mask, 0x000)  ///< Set exception mask
#define EMUX_XASAN(rd, rt, code)        EMUX_OP(0x2B,   rd,     rt,  code)  ///< XASAN memory sanitizer
#define EMUX_XIOCTL(code)               EMUX_OP(0x2C,    0,      0,  code)  ///< Modify emulator behavior
/** @} */

#define EMUX_FEAT1_DETECT                       (1 << 0x0)    ///< EMUX detection support
#define EMUX_FEAT1_BREAK                        (1 << 0x1)    ///< Immediate breakpoint support
#define EMUX_FEAT1_BREAKPOINTS                  (1 << 0x2)    ///< Breakpoint configuration support
#define EMUX_FEAT1_TRACE                        (1 << 0x3)    ///< Tracing support
#define EMUX_FEAT1_LOG                          (1 << 0x5)    ///< Logging support
#define EMUX_FEAT1_LOGREGS                      (1 << 0x6)    ///< Register logging support
#define EMUX_FEAT1_HEXDUMP                      (1 << 0x7)    ///< Hexdump support
#define EMUX_FEAT1_PROFILER                     (1 << 0x8)    ///< Profiling support
#define EMUX_FEAT1_EXCEPTION                    (1 << 0xA)    ///< Exception support
#define EMUX_FEAT1_XASAN                        (1 << 0xB)    ///< XASAN memory sanitizer support
#define EMUX_FEAT1_IOCTL                        (1 << 0xC)    ///< Emulator behavior support

#define EMUX_XASAN_DISABLE                      0x0           ///< Disable XASAN checking (refcount--)
#define EMUX_XASAN_ENABLE                       0x1           ///< Enable XASAN checking (refcount++)
#define EMUX_XASAN_POISON                       0x2           ///< Poison a memory region
#define EMUX_XASAN_UNPOISON                     0x3           ///< Unpoison a memory region

#define EMUX_XASAN_TAG_ACCESSIBLE               0             ///< Accessible region
#define EMUX_XASAN_TAG_LEFT                     1             ///< Left redzone
#define EMUX_XASAN_TAG_RIGHT                    2             ///< Right redzone
#define EMUX_XASAN_TAG_FREED                    3             ///< Freed memory
#define EMUX_XASAN_TAG_GLOBAL                   4             ///< Global redzone
#define EMUX_XASAN_TAG_USER                     5             ///< User-poisoned region
#define EMUX_XASAN_TAG_UNALLOC                  6             ///< Unallocated heap memory

#define EMUX_XASAN_ACCESS_UNKNOWN               0x00
#define EMUX_XASAN_ACCESS_CPU_READ              0x01
#define EMUX_XASAN_ACCESS_CPU_WRITE             0x02
#define EMUX_XASAN_ACCESS_CPU_EXEC              0x03
#define EMUX_XASAN_ACCESS_STACK                 0x04
#define EMUX_XASAN_ACCESS_RSP_DMA_READ          0x05
#define EMUX_XASAN_ACCESS_RSP_DMA_WRITE         0x06

#define EMUX_XASAN_FAULT_UNKNOWN                0x00
#define EMUX_XASAN_FAULT_PERMISSION             0x01
#define EMUX_XASAN_FAULT_POISON                 0x02
#define EMUX_XASAN_FAULT_TAIL                   0x03

#define EMUX_LOG_ASCIIZ                         0x000      ///< Log a zero-terminated string
#define EMUX_LOG_LENGTH                         0x001      ///< Log a non-zero-terminated string

#define EMUX_BREAKPOINT_ADD                     0x001      ///< Add instruction breakpoint
#define EMUX_BREAKPOINT_REMOVE                  0x002      ///< Remove instruction breakpoint
#define EMUX_WATCHPOINT_READ_ADD                0x003      ///< Add read watchpoint
#define EMUX_WATCHPOINT_READ_REMOVE             0x004      ///< Remove read watchpoint
#define EMUX_WATCHPOINT_WRITE_ADD               0x005      ///< Add write watchpoint
#define EMUX_WATCHPOINT_WRITE_REMOVE            0x006      ///< Remove write watchpoint

#define EMUX_LOGREGS_COP0                       0x000      ///< Dump COP0 registers
#define EMUX_LOGREGS_COP1                       0x001      ///< Dump COP1 registers
#define EMUX_LOGREGS_COP2                       0x002      ///< Dump COP2 registers
#define EMUX_LOGREGS_GPR                        0x003      ///< Dump GPR registers
#define EMUX_LOGREGS_DECIMAL                    0x004      ///< Prefer decimal formatting
#define EMUX_LOGREGS_DOUBLE                     0x008      ///< Interpret as 64-bit floating point (COP1)
#define EMUX_LOGREGS_EXTRA                      0x010      ///< Include extra registers

#define EMUX_IOCTL_EXIT                         0x001      ///< Exit the emulator
#define EMUX_IOCTL_FAST                         0x002      ///< Fast mode (go uncapped)
#define EMUX_IOCTL_SLOW                         0x003      ///< Slow mode (go 1x)
#define EMUX_IOCTL_PAUSE                        0x004      ///< Pause the emulator
        
#define EMUX_PROF_START                         0x001      ///< Start profiling on a slot
#define EMUX_PROF_STOP                          0x002      ///< Stop profiling on a slot
#define EMUX_PROF_CLEAR                         0x003      ///< Clear profiling data on a slot
#define EMUX_PROF_RESET                         0x004      ///< Reset profiling data (clear all slots)

#define EMUX_PROF_CYCLES                       0x0000      ///< Total CPU cycles
#define EMUX_PROF_CYCLES_EXC                   0x0001      ///< CPU cycles within exception
#define EMUX_PROF_INSNS                        0x0002      ///< CPU instructions executed
#define EMUX_PROF_INSNS_EXC                    0x0003      ///< CPU instructions executed within exception
#define EMUX_PROF_ICACHE_HITS                  0x0010      ///< Instruction cache hits
#define EMUX_PROF_ICACHE_MISSES                0x0011      ///< Instruction cache misses
#define EMUX_PROF_ICACHE_WBS                   0x0012      ///< Instruction cache writebacks
#define EMUX_PROF_DCACHE_HITS                  0x0020      ///< Data cache hits
#define EMUX_PROF_DCACHE_MISSES                0x0021      ///< Data cache misses
#define EMUX_PROF_DCACHE_WBS                   0x0022      ///< Data cache writebacks
#define EMUX_PROF_TLB_HITS                     0x0100      ///< TLB hits
#define EMUX_PROF_TLB_MISSES                   0x0101      ///< TLB misses
#define EMUX_PROF_MTLB_HITS                    0x0110      ///< MiniTLB hits
#define EMUX_PROF_MTLB_MISSES                  0x0111      ///< MiniTLB misses
#define EMUX_PROF_RSP_CYCLES                   0x0200      ///< RSP cycles
#define EMUX_PROF_RSP_IDLE                     0x0201      ///< RSP cycles in idle mode
#define EMUX_PROF_RSP_STALLS                   0x0210      ///< RSP stalls
#define EMUX_PROF_RSP_STALLS_V                 0x0211      ///< RSP stalls due to vector unit
#define EMUX_PROF_RSP_STALLS_S                 0x0212      ///< RSP stalls due to scalar unit
#define EMUX_PROF_RAM_BYTES                    0x0300      ///< RDRAM bytes transferred (r/w)
#define EMUX_PROF_RAM_BYTES_R                  0x0301      ///< RDRAM bytes transferred (read)
#define EMUX_PROF_RAM_BYTES_W                  0x0302      ///< RDRAM bytes transferred (write)
#define EMUX_PROF_RAM_ICACHE_BYTES             0x0310      ///< RDRAM bytes transferred for I-cache (r/w)
#define EMUX_PROF_RAM_ICACHE_BYTES_R           0x0311      ///< RDRAM bytes transferred for I-cache (read)
#define EMUX_PROF_RAM_ICACHE_BYTES_W           0x0312      ///< RDRAM bytes transferred for I-cache (write)
#define EMUX_PROF_RAM_DCACHE_BYTES             0x0320      ///< RDRAM bytes transferred for D-cache (r/w)
#define EMUX_PROF_RAM_DCACHE_BYTES_R           0x0321      ///< RDRAM bytes transferred for D-cache (read)
#define EMUX_PROF_RAM_DCACHE_BYTES_W           0x0322      ///< RDRAM bytes transferred for D-cache (write)
#define EMUX_PROF_RAM_UNCACHED_BYTES           0x0330      ///< RDRAM bytes transferred for uncached accesses (r/w)
#define EMUX_PROF_RAM_UNCACHED_BYTES_R         0x0331      ///< RDRAM bytes transferred for uncached accesses (read)
#define EMUX_PROF_RAM_UNCACHED_BYTES_W         0x0332      ///< RDRAM bytes transferred for uncached accesses (write)
#define EMUX_PROF_RAM_RSPDMA_BYTES             0x0340      ///< RDRAM bytes transferred via RSP DMA (r/w)
#define EMUX_PROF_RAM_RSPDMA_BYTES_R           0x0341      ///< RDRAM bytes transferred via RSP DMA (read)
#define EMUX_PROF_RAM_RSPDMA_BYTES_W           0x0342      ///< RDRAM bytes transferred via RSP DMA (write)
#define EMUX_PROF_RAM_PIDMA_BYTES              0x0350      ///< RDRAM bytes transferred via PI DMA (r/w)
#define EMUX_PROF_RAM_PIDMA_BYTES_R            0x0351      ///< RDRAM bytes transferred via PI DMA (read)    
#define EMUX_PROF_RAM_PIDMA_BYTES_W            0x0352      ///< RDRAM bytes transferred via PI DMA (write)
#define EMUX_PROF_RAM_SIDMA_BYTES              0x0360      ///< RDRAM bytes transferred via SI DMA (r/w)
#define EMUX_PROF_RAM_SIDMA_BYTES_R            0x0361      ///< RDRAM bytes transferred via SI DMA (read)
#define EMUX_PROF_RAM_SIDMA_BYTES_W            0x0362      ///< RDRAM bytes transferred via SI DMA (write)
#define EMUX_PROF_RAM_RDPDRAW_BYTES            0x0370      ///< RDRAM bytes transferred via RDP during draw (r/w)
#define EMUX_PROF_RAM_RDPDRAW_BYTES_R          0x0371      ///< RDRAM bytes transferred via RDP during draw (read)
#define EMUX_PROF_RAM_RDPDRAW_BYTES_W          0x0372      ///< RDRAM bytes transferred via RDP during draw (write)
#define EMUX_PROF_RAM_AIDMA_BYTES              0x0380      ///< RDRAM bytes transferred via AI DMA (r/w)
#define EMUX_PROF_RAM_AIDMA_BYTES_R            0x0381      ///< RDRAM bytes transferred via AI DMA (read)
#define EMUX_PROF_RAM_AIDMA_BYTES_W            0x0382      ///< RDRAM bytes transferred via AI DMA (write) (always 0)
#define EMUX_PROF_RAM_VI_BYTES                 0x0390      ///< RDRAM bytes transferred via VI (r/w)
#define EMUX_PROF_RAM_VI_BYTES_R               0x0391      ///< RDRAM bytes transferred via VI (read)
#define EMUX_PROF_RAM_VI_BYTES_W               0x0392      ///< RDRAM bytes transferred via VI (write) (always 0)
#define EMUX_PROF_RAM_RDPDMA_BYTES             0x03A0      ///< RDRAM bytes transferred via RDP DMA (r/w)
#define EMUX_PROF_RAM_RDPDMA_BYTES_R           0x03A1      ///< RDRAM bytes transferred via RDP DMA (read)
#define EMUX_PROF_RAM_RDPDMA_BYTES_W           0x03A2      ///< RDRAM bytes transferred via RDP DMA (write) (always 0)

#define EMUX_EXCEPTION_ERR_MASK                0x00000000FFFFFFFFULL     ///< Mask of exceptions related to hardware errors
#define EMUX_EXCEPTION_WARN_MASK               0xFFFFFFFF00000000ULL     ///< Mask of exceptions related to software warnings (resumable exceptions)

#define EMUX_EXCEPTION_CPU_CACHED_ACCESS       (cast64(1) << 1)  ///< CPU cached access to non-RDRAM area
#define EMUX_EXCEPTION_CPU_64BIT_READ          (cast64(1) << 2)  ///< CPU 64-bit read from non-RDRAM area
#define EMUX_EXCEPTION_CPU_UNMAPPED_ACCESS     (cast64(1) << 3)  ///< CPU access to RCP unmapped area
#define EMUX_EXCEPTION_XASAN                   (cast64(1) << 4)  ///< XASAN memory access violation

#ifndef __ASSEMBLER__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Detect if EMUX is present and which features are supported
 * 
 * If this function returns zero, EMUX is not present. Otherwise, the
 * returned bitmask contains the supported features (see EMUX_FEAT_* defines).
 * 
 * @param subcode       Index of the detection bitmask to read
 * @return              Bitmask of supported EMUX features (see EMUX_FEAT_*)
 */
inline uint32_t emux_detect(int subcode)
{
    /*
     * GCC can't easily feed the destination register number into a raw `.word`
     * encoding. A simple MIPS64-friendly pattern is:
     *  1) encode the instruction to write into a fixed scratch GPR (here: t0)
     *  2) copy the scratch GPR into a GCC output register
     */
    const int REG_T0 = 8;
    register uint64_t result asm("t0") = 0;

    __asm__ __volatile__(
        " .word %1\n"
        : "+r"(result) : "i"(EMUX_XDETECT(REG_T0, subcode)) : "memory");

    return result;
}

/**
 * @brief Trigger an immediate breakpoint
 * 
 * When this function is called, if an EMUX-compatible emulator is running,
 * execution will break immediately, allowing to inspect the current state
 * of the emulated system.
 */
inline void emux_break(void)
{
    __asm__ __volatile__(" .word %0\n" :: "i"(EMUX_XBREAK()): "memory");
}

/**
 * @brief Log an UTF-8 string to the emulator
 * 
 * This function sends the provided UTF-8 string to the emulator logging
 * system. The string must be 0-terminated.
 * 
 * Notice that the emulator is allowed to perform line-buffering, so the
 * logged string may not appear immediately in the logging output if it
 * does not end with a newline character.
 * 
 * @param ut8_str           UTF-8 encoded string to log
 */
inline void emux_log(const char *ut8_str)
{
    const int REG_T0 = 8;
    register const char *__ut8_str asm("t0") = ut8_str;
    __asm__ __volatile__(
        " .word %1\n"
        :: "r"(__ut8_str), "i"(EMUX_XLOG(REG_T0, 0, EMUX_LOG_ASCIIZ)) : "memory");
}

/**
 * @brief Log a non-zero-terminated UTF-8 string to the emulator
 * 
 * This function sends the provided UTF-8 string to the emulator logging
 * system. The string is not required to be 0-terminated; its length
 * is provided as a separate parameter.
 * 
 * Notice that the emulator is allowed to perform line-buffering, so the
 * logged string may not appear immediately in the logging output if it
 * does not end with a newline character.
 * 
 * @param utf8_str        UTF-8 encoded string to log
 * @param len             Length of the string
 */
inline void emux_logn(const char *utf8_str, int len)
{
    const int REG_T0 = 8;
    const int REG_T1 = 9;
    register const char *__utf8_str asm("t0") = utf8_str;
    register int __len asm("t1") = len;
    __asm__ __volatile__(
        " .word %2\n"
        :: "r"(__utf8_str), "r"(__len), "i"(EMUX_XLOG(REG_T0, REG_T1, EMUX_LOG_LENGTH)) : "memory");
}

/**
 * @brief Do a hexdump log to the emulator
 * 
 * The emulator will log the provided string as a hexdump, showing the
 * hexadecimal values of each byte in the string (and possibly also its
 * ASCII representation).
 * 
 * @param buffer          UTF-8 encoded string to log
 * @param len             Length of the string
 */
inline void emux_hexdump(const uint8_t *buffer, int len)
{
    const int REG_T0 = 8;
    const int REG_T1 = 9;
    register const uint8_t *__buffer asm("t0") = buffer;
    register int __len asm("t1") = len;
    __asm__ __volatile__(
        " .word %2\n"
        :: "r"(__buffer), "r"(__len), "i"(EMUX_XHEXDUMP(REG_T0, REG_T1)) : "memory");
}

/** 
 * @brief Ask the emulator to exit
 * 
 * When this function is called, if an EMUX-compatible emulator is running,
 * it will exit the emulation immediately.
 * 
 * This is useful mainly for automated test ROMs, to save the user the hassle of
 * closing the emulator window manually after the ROM has finished running.
 */
inline void emux_ioctl_exit(void)
{
    __asm__ __volatile__(" .word %0\n" :: "i"(EMUX_XIOCTL(EMUX_IOCTL_EXIT)) : "memory");
}

/**
 * @brief Set the emulator to fast mode
 * 
 * When this function is called, if an EMUX-compatible emulator is running,
 * it will switch to fast mode, where the emulation runs as fast as possible
 * (unlimited framerate).
 * 
 * This is useful to quickly skip to the point that you want to test/debug.
 */
inline void emux_ioctl_fast(void)
{
    __asm__ __volatile__(" .word %0\n" :: "i"(EMUX_XIOCTL(EMUX_IOCTL_FAST)) : "memory");
}

/**
 * @brief Set the emulator to slow mode
 * 
 * When this function is called, if an EMUX-compatible emulator is running,
 * it will switch to slow mode, where the emulation runs at real N64 speed
 * (capped framerate).
 */
inline void emux_ioctl_slow(void)
{
    __asm__ __volatile__(" .word %0\n" :: "i"(EMUX_XIOCTL(EMUX_IOCTL_SLOW)) : "memory");
}

/**
 * @brief Pause the emulator
 * 
 * When this function is called, if an EMUX-compatible emulator is running,
 * it will pause the emulation until resumed by the user.
 * 
 * This can be useful to show the user a specific state/frame of the
 * emulation for inspection.
 */
inline void emux_ioctl_pause(void)
{
    __asm__ __volatile__(" .word %0\n" :: "i"(EMUX_XIOCTL(EMUX_IOCTL_PAUSE)) : "memory");
}

/**
 * @brief Start profiling on a slot
 * 
 * When this function is called, if an EMUX-compatible emulator is running,
 * it will start profiling on the specified slot.
 * 
 * @param slot       Profiler slot to start
 */
inline void emux_prof_start(int slot)
{
    const int REG_T0 = 8;
    register int __slot asm("t0") = slot;
    __asm__ __volatile__(" .word %1\n" :: 
        "r"(__slot), "i"(EMUX_XPROF(REG_T0, EMUX_PROF_START)) : "memory");
}

/**
 * @brief Stop profiling on a slot
 * 
 * When this function is called, if an EMUX-compatible emulator is running,
 * it will stop profiling on the specified slot.
 * 
 * @param slot      Profiler slot to stop
 */
inline void emux_prof_stop(int slot)
{
    const int REG_T0 = 8;
    register int __slot asm("t0") = slot;
    __asm__ __volatile__(" .word %1\n" :: 
        "r"(__slot), "i"(EMUX_XPROF(REG_T0, EMUX_PROF_STOP)) : "memory");
}

/**
 * @brief Clear profiling data on a slot
 * 
 * When this function is called, if an EMUX-compatible emulator is running,
 * it will clear profiling data on the specified slot.
 * 
 * @param slot    Profiler slot to clear
 */
inline void emux_prof_clear(int slot)
{
    const int REG_T0 = 8;
    register int __slot asm("t0") = slot;
    __asm__ __volatile__(" .word %1\n" :: 
        "r"(__slot), "i"(EMUX_XPROF(REG_T0, EMUX_PROF_CLEAR)) : "memory");
}

/**
 * @brief Reset all profiling data
 * 
 * When this function is called, if an EMUX-compatible emulator is running,
 * it will reset all profiling data on all slots.
 */
inline void emux_prof_reset(void)
{
    __asm__ __volatile__(" .word %0\n" :: "i"(EMUX_XPROF(0, EMUX_PROF_RESET)) : "memory");
}

/**
 * @brief Read a profiling metric from a slot
 * 
 * When this function is called, if an EMUX-compatible emulator is running,
 * it will read the specified profiling metric from the specified slot.
 * 
 * This is useful to extract profiling data programmatically from within
 * the emulated program. Emulators can also expose profiling data via their
 * own user interface.
 * 
 * @param slot          Profiler slot to read from
 * @param metric        Metric to read (see EMUX_PROF_* defines)
 * @return              Value of the requested metric in the slot
 */
inline uint64_t emux_prof_read(int slot, uint32_t metric)
{
    const int REG_T0 = 8;
    const int REG_T1 = 9;
    register int __slot asm("t0") = slot;
    register uint64_t __metric asm("t1") = metric;

    __asm__ __volatile__(
        " .word %2\n"
        : "+r"(__metric) : "r"(__slot), "i"(EMUX_XPROFREAD(REG_T0, REG_T1)) : "memory"
    );

    return __metric;
}

/**
 * @brief Activate emux exceptions
 *
 * This function allows to configure the emux exception mask, that tells the
 * emulator which special exceptions should be generated.
 *
 * Emux exceptions are generated by the emulator as an alternative of properly
 * emulating hardware freezes, to provide a better development experience.
 *
 * @param mask      Mask of exceptions to activate/deactivate (see EMUX_EXCEPTION_* defines)
 */
inline void emux_exception_set_mask(uint64_t mask)
{
    const int REG_T0 = 8;
    register uint64_t __mask asm("t0") = mask;
    __asm__ __volatile__(" .word %1\n" :: "r"(__mask), "i"(EMUX_XEXCEPTION(REG_T0)) : "memory");
}

/** @brief Run an XASAN EMUX opcode with address and size operands */
#define EMUX_XASAN_RUN(addr, size, code) do { \
    const int REG_T0 = 8; \
    const int REG_T1 = 9; \
    register const void *__xasan_addr asm("t0") = (const void *)(addr); \
    register size_t __xasan_size asm("t1") = (size_t)(size); \
    __asm__ __volatile__(" .word %2\n" \
        :: "r"(__xasan_addr), "r"(__xasan_size), "i"(EMUX_XASAN(REG_T0, REG_T1, code)) \
        : "memory"); \
} while(0)

/** @brief Enable XASAN checking in the emulator */
inline void emux_xasan_enable(void)
{
    __asm__ __volatile__(" .word %0\n" :: "i"(EMUX_XASAN(0, 0, EMUX_XASAN_ENABLE)) : "memory");
}

/** @brief Disable XASAN checking in the emulator */
inline void emux_xasan_disable(void)
{
    __asm__ __volatile__(" .word %0\n" :: "i"(EMUX_XASAN(0, 0, EMUX_XASAN_DISABLE)) : "memory");
}

/** @brief Mark a memory region as accessible */
inline void emux_xasan_unpoison(const void *addr, size_t size)
{
    EMUX_XASAN_RUN(addr, size, EMUX_XASAN_UNPOISON);
}

/** @brief Mark a memory region as user-poisoned */
inline void emux_xasan_poison_user(const void *addr, size_t size)
{
    EMUX_XASAN_RUN(addr, size, EMUX_XASAN_POISON | (EMUX_XASAN_TAG_USER << 4));
}

/** @brief Mark a memory region as left redzone */
inline void emux_xasan_poison_left(const void *addr, size_t size)
{
    EMUX_XASAN_RUN(addr, size, EMUX_XASAN_POISON | (EMUX_XASAN_TAG_LEFT << 4));
}

/** @brief Mark a memory region as right redzone */
inline void emux_xasan_poison_right(const void *addr, size_t size)
{
    EMUX_XASAN_RUN(addr, size, EMUX_XASAN_POISON | (EMUX_XASAN_TAG_RIGHT << 4));
}

/** @brief Mark a memory region as freed */
inline void emux_xasan_poison_freed(const void *addr, size_t size)
{
    EMUX_XASAN_RUN(addr, size, EMUX_XASAN_POISON | (EMUX_XASAN_TAG_FREED << 4));
}

/** @brief Mark a memory region as unallocated heap memory */
inline void emux_xasan_poison_unalloc(const void *addr, size_t size)
{
    EMUX_XASAN_RUN(addr, size, EMUX_XASAN_POISON | (EMUX_XASAN_TAG_UNALLOC << 4));
}

#ifdef __cplusplus
}
#endif

#endif // __ASSEMBLER__

#endif  // LIBDRAGON_EMUX_H
