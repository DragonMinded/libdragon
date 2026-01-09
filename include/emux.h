/**
 * @file emux.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Implementation of the N64 emulator extensions
 * 
 * EMUX is an open spec for emulators to provide extensions to the emulated
 * N64 system, allowing advanced profiling and debugging features.
 * https://hackmd.io/SOLBoD6XSn-_WRdwtlMs9A
 * 
 */

#ifndef LIBDRAGON_EMUX_H
#define LIBDRAGON_EMUX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Assemble a single emux opcode */
#define EMUX_OP(ext, rd, rt, code) \
    ((0b010000 << 26) | (1 << 25) | ((ext) & 0x3F) | \
    (((rd) & 0x1F) << 20) | (((rt) & 0x1F) << 15) | (((code) & 0x1FF) << 6))

/** 
 * @name EMUX opcodes
 * @{
 */
#define EMUX_XDETECT(rd)                EMUX_OP(0x20,   rd,      0, 0x000)  ///< Detect EMUX presence
#define EMUX_XBREAK()                   EMUX_OP(0x21,    0,      0, 0x000)  ///< Trigger a breakpoint
#define EMUX_XLOG(addr, len)            EMUX_OP(0x22, addr,    len, 0x000)  ///< Log a string
#define EMUX_XPROF(slot, code)          EMUX_OP(0x28, slot,      0,  code)  ///< Control profiler
#define EMUX_XPROF_READ(slot, metric)   EMUX_OP(0x29, slot, metric, 0x000)  ///< Read profiler metric
#define EMUX_XIOCTL(code)               EMUX_OP(0x2C,    0,      0,  code)  ///< Modify emulator behavior
/** @} */

#define EMUX_FEAT_DETECT                        (1 << 0)    ///< EMUX detection support
#define EMUX_FEAT_BREAK                         (1 << 1)    ///< Immediate breakpoint support
#define EMUX_FEAT_BREAKPOINTS                   (1 << 2)    ///< Breakpoint configuration support
#define EMUX_FEAT_TRACE                         (1 << 3)    ///< Tracing support
#define EMUX_FEAT_LOG                           (1 << 5)    ///< Logging support
#define EMUX_FEAT_LOGREGS                       (1 << 6)    ///< Register logging support
#define EMUX_FEAT_HEXDUMP                       (1 << 7)    ///< Hexdump support
#define EMUX_FEAT_PROFILER                      (1 << 8)    ///< Profiling support
#define EMUX_FEAT_IOCTL                         (1 << 12)   ///< Emulator behavior support

#define EMUX_XIOCTL_EXIT                         0x001      ///< Exit the emulator
#define EMUX_XIOCTL_FAST                         0x002      ///< Fast mode (go uncapped)
#define EMUX_XIOCTL_SLOW                         0x003      ///< Slow mode (go 1x)
#define EMUX_XIOCTL_PAUSE                        0x004      ///< Pause the emulator
        
#define EMUX_XPROF_START                         0x001      ///< Start profiling on a slot
#define EMUX_XPROF_STOP                          0x002      ///< Stop profiling on a slot
#define EMUX_XPROF_CLEAR                         0x003      ///< Clear profiling data on a slot
#define EMUX_XPROF_RESET                         0x004      ///< Reset profiling data (clear all slots)

#define EMUX_XPROF_CYCLES                       0x0000      ///< Total CPU cycles
#define EMUX_XPROF_CYCLES_EXC                   0x0001      ///< CPU cycles within exception
#define EMUX_XPROF_ICACHE_HITS                  0x0010      ///< Instruction cache hits
#define EMUX_XPROF_ICACHE_MISSES                0x0011      ///< Instruction cache misses
#define EMUX_XPROF_ICACHE_WBS                   0x0012      ///< Instruction cache writebacks
#define EMUX_XPROF_DCACHE_HITS                  0x0020      ///< Data cache hits
#define EMUX_XPROF_DCACHE_MISSES                0x0021      ///< Data cache misses
#define EMUX_XPROF_DCACHE_WBS                   0x0022      ///< Data cache writebacks
#define EMUX_XPROF_TLB_HITS                     0x0100      ///< TLB hits
#define EMUX_XPROF_TLB_MISSES                   0x0101      ///< TLB misses
#define EMUX_XPROF_MTLB_HITS                    0x0110      ///< MiniTLB hits
#define EMUX_XPROF_MTLB_MISSES                  0x0111      ///< MiniTLB misses
#define EMUX_XPROF_RSP_CYCLES                   0x0200      ///< RSP cycles
#define EMUX_XPROF_RSP_IDLE                     0x0201      ///< RSP cycles in idle mode
#define EMUX_XPROF_RSP_STALLS                   0x0210      ///< RSP stalls
#define EMUX_XPROF_RSP_STALLS_V                 0x0211      ///< RSP stalls due to vector unit
#define EMUX_XPROF_RSP_STALLS_S                 0x0212      ///< RSP stalls due to scalar unit
#define EMUX_XPROF_RAM_BYTES                    0x0300      ///< RDRAM bytes transferred (r/w)
#define EMUX_XPROF_RAM_BYTES_R                  0x0301      ///< RDRAM bytes transferred (read)
#define EMUX_XPROF_RAM_BYTES_W                  0x0302      ///< RDRAM bytes transferred (write)
#define EMUX_XPROF_RAM_ICACHE_BYTES             0x0310      ///< RDRAM bytes transferred for I-cache (r/w)
#define EMUX_XPROF_RAM_ICACHE_BYTES_R           0x0311      ///< RDRAM bytes transferred for I-cache (read)
#define EMUX_XPROF_RAM_ICACHE_BYTES_W           0x0312      ///< RDRAM bytes transferred for I-cache (write)
#define EMUX_XPROF_RAM_DCACHE_BYTES             0x0320      ///< RDRAM bytes transferred for D-cache (r/w)
#define EMUX_XPROF_RAM_DCACHE_BYTES_R           0x0321      ///< RDRAM bytes transferred for D-cache (read)
#define EMUX_XPROF_RAM_DCACHE_BYTES_W           0x0322      ///< RDRAM bytes transferred for D-cache (write)
#define EMUX_XPROF_RAM_UNCACHED_BYTES           0x0330      ///< RDRAM bytes transferred for uncached accesses (r/w)
#define EMUX_XPROF_RAM_UNCACHED_BYTES_R         0x0331      ///< RDRAM bytes transferred for uncached accesses (read)
#define EMUX_XPROF_RAM_UNCACHED_BYTES_W         0x0332      ///< RDRAM bytes transferred for uncached accesses (write)
#define EMUX_XPROF_RAM_RSPDMA_BYTES             0x0340      ///< RDRAM bytes transferred via RSP DMA (r/w)
#define EMUX_XPROF_RAM_RSPDMA_BYTES_R           0x0341      ///< RDRAM bytes transferred via RSP DMA (read)
#define EMUX_XPROF_RAM_RSPDMA_BYTES_W           0x0342      ///< RDRAM bytes transferred via RSP DMA (write)
#define EMUX_XPROF_RAM_PIDMA_BYTES              0x0350      ///< RDRAM bytes transferred via PI DMA (r/w)
#define EMUX_XPROF_RAM_PIDMA_BYTES_R            0x0351      ///< RDRAM bytes transferred via PI DMA (read)    
#define EMUX_XPROF_RAM_PIDMA_BYTES_W            0x0352      ///< RDRAM bytes transferred via PI DMA (write)
#define EMUX_XPROF_RAM_SIDMA_BYTES              0x0360      ///< RDRAM bytes transferred via SI DMA (r/w)
#define EMUX_XPROF_RAM_SIDMA_BYTES_R            0x0361      ///< RDRAM bytes transferred via SI DMA (read)
#define EMUX_XPROF_RAM_SIDMA_BYTES_W            0x0362      ///< RDRAM bytes transferred via SI DMA (write)
#define EMUX_XPROF_RAM_RDPDRAW_BYTES            0x0370      ///< RDRAM bytes transferred via RDP during draw (r/w)
#define EMUX_XPROF_RAM_RDPDRAW_BYTES_R          0x0371      ///< RDRAM bytes transferred via RDP during draw (read)
#define EMUX_XPROF_RAM_RDPDRAW_BYTES_W          0x0372      ///< RDRAM bytes transferred via RDP during draw (write)
#define EMUX_XPROF_RAM_AIDMA_BYTES              0x0380      ///< RDRAM bytes transferred via AI DMA (r/w)
#define EMUX_XPROF_RAM_AIDMA_BYTES_R            0x0381      ///< RDRAM bytes transferred via AI DMA (read)
#define EMUX_XPROF_RAM_AIDMA_BYTES_W            0x0382      ///< RDRAM bytes transferred via AI DMA (write) (always 0)
#define EMUX_XPROF_RAM_VI_BYTES                 0x0390      ///< RDRAM bytes transferred via VI (r/w)
#define EMUX_XPROF_RAM_VI_BYTES_R               0x0391      ///< RDRAM bytes transferred via VI (read)
#define EMUX_XPROF_RAM_VI_BYTES_W               0x0392      ///< RDRAM bytes transferred via VI (write) (always 0)
#define EMUX_XPROF_RAM_RDPDMA_BYTES             0x03A0      ///< RDRAM bytes transferred via RDP DMA (r/w)
#define EMUX_XPROF_RAM_RDPDMA_BYTES_R           0x03A1      ///< RDRAM bytes transferred via RDP DMA (read)
#define EMUX_XPROF_RAM_RDPDMA_BYTES_W           0x03A2      ///< RDRAM bytes transferred via RDP DMA (write) (always 0)

/**
 * @brief Detect if EMUX is present and which features are supported
 * 
 * If this function returns zero, EMUX is not present. Otherwise, the
 * returned bitmask contains the supported features (see EMUX_FEAT_* defines).
 * 
 * @return uint64_t     Bitmask of supported EMUX features (see EMUX_FEAT_*)
 */
inline uint64_t emux_detect(void)
{
    uint64_t result;

    /*
     * GCC can't easily feed the destination register number into a raw `.word`
     * encoding. A simple MIPS64-friendly pattern is:
     *  1) encode the instruction to write into a fixed scratch GPR (here: $t0)
     *  2) copy the scratch GPR into a GCC output register
     */
    const int REG_T0 = 8;

    __asm__ __volatile__(
        " li $t0, 0\n"
        " .word %1\n"
        " move %0, $t0\n"
        : "=r"(result) : "i"(EMUX_XDETECT(REG_T0)) : "memory", "t0");

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
    register const char *__ut8_str asm("$t0") = ut8_str;
    __asm__ __volatile__(
        " .word %1\n"
        :: "r"(__ut8_str), "i"(EMUX_XLOG(REG_T0, 0)) : "memory");
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
    register const char *__utf8_str asm("$t0") = utf8_str;
    register int __len asm("$t1") = len;
    __asm__ __volatile__(
        " .word %2\n"
        :: "r"(__utf8_str), "r"(__len), "i"(EMUX_XLOG(REG_T0, REG_T1)) : "memory");
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
    __asm__ __volatile__(" .word %0\n" :: "i"(EMUX_XIOCTL(EMUX_XIOCTL_EXIT)) : "memory");
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
    __asm__ __volatile__(" .word %0\n" :: "i"(EMUX_XIOCTL(EMUX_XIOCTL_FAST)) : "memory");
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
    __asm__ __volatile__(" .word %0\n" :: "i"(EMUX_XIOCTL(EMUX_XIOCTL_SLOW)) : "memory");
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
    __asm__ __volatile__(" .word %0\n" :: "i"(EMUX_XIOCTL(EMUX_XIOCTL_PAUSE)) : "memory");
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
    register int __slot asm("$t0") = slot;
    __asm__ __volatile__(" .word %2\n" :: 
        "r"(__slot), "i"(EMUX_XPROF(REG_T0, EMUX_XPROF_START)) : "memory");
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
    register int __slot asm("$t0") = slot;
    __asm__ __volatile__(" .word %2\n" :: 
        "r"(__slot), "i"(EMUX_XPROF(REG_T0, EMUX_XPROF_STOP)) : "memory");
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
    register int __slot asm("$t0") = slot;
    __asm__ __volatile__(" .word %2\n" :: 
        "r"(__slot), "i"(EMUX_XPROF(REG_T0, EMUX_XPROF_CLEAR)) : "memory");
}

/**
 * @brief Reset all profiling data
 * 
 * When this function is called, if an EMUX-compatible emulator is running,
 * it will reset all profiling data on all slots.
 */
inline void emux_prof_reset(void)
{
    __asm__ __volatile__(" .word %2\n" :: "i"(EMUX_XPROF(0, EMUX_XPROF_RESET)) : "memory");
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
 * @param metric        Metric to read (see EMUX_XPROF_* defines)
 * @return uint64_t     Value of the requested metric in the slot
 */
inline uint64_t emux_prof_read(int slot, uint32_t metric)
{
    uint64_t result;
    const int REG_T0 = 8;
    const int REG_T1 = 9;
    register int __slot asm("$t0") = slot;
    register int __metric asm("$t1") = metric;

    __asm__ __volatile__(
        " .word %2\n"
        " move %0, $t1\n"
        : "=r"(result) : "r"(__slot), "r"(__metric), "i"(EMUX_XPROF_READ(REG_T0, REG_T1)) : "memory", "t1");

    return result;
}

#ifdef __cplusplus
}
#endif

#endif  // LIBDRAGON_EMUX_H
