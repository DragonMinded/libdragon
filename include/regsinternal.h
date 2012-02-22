/**
 * @file regsinternal.h
 * @brief Register definitions for various hardware in the N64
 * @ingroup lowlevel
 */
#ifndef __LIBDRAGON_REGSINTERNAL_H
#define __LIBDRAGON_REGSINTERNAL_H

/**
 * @defgroup lowlevel Low Level Hardware Interfaces
 * @ingroup libdragon
 * @brief Low level hardware interface descriptions and functionality
 *
 * The low level hardware interfaces handle several functions in the N64 that
 * would otherwise be handled by a kernel or RTOS.  This includes the @ref dma,
 * the @ref exceptions, the @ref interrupt and the @ref n64sys.  The DMA controller
 * handles DMA requests between the cartridge and the N64 RDRAM.  Other systems
 * in the N64 have their own DMA controllers that are handled in the relevant
 * subsystems.  The exception handler traps any exceptions raised by the N64,
 * including the reset exception.  The interrupt handler sets up the MIPS
 * interface (MI) which handles low level interrupt functionality for all other
 * systems in the N64.  The N64 system interface provides the ability for code to
 * manipulate cache and boot options.
 */

/**
 * @brief Register definition for the AI interface
 * @ingroup lowlevel
 */
typedef struct AI_regs_s {
    /** @brief Pointer to uncached memory buffer of samples to play */
    short * address;
    /** @brief Size in bytes of the buffer to be played.  Should be
     *         number of stereo samples * 2 * sizeof( uint16_t ) 
     */
    unsigned long length;
    /** @brief DMA start register.  Write a 1 to this register to start
     *         playing back an audio sample. */
    unsigned long control;
    /** @brief AI status register.  Bit 31 is the full bit, bit 30 is the busy bit. */
    unsigned long status;
    /** @brief Rate at which the buffer should be played.
     *
     * Use the following formula to calculate the value: ((2 * clockrate / frequency) + 1) / 2 - 1
     */
    unsigned long dacrate;
    /** @brief The size of a single sample in bits. */
    unsigned long samplesize;
} _AI_regs_s;

/**
 * @brief Register definition for the MI interface
 * @ingroup lowlevel
 */
typedef struct MI_regs_s {
    /** @brief Mode register */
    unsigned long mode;
    /** @brief Version register */
    unsigned long version;
    /** @brief Current interrupts on the system */
    unsigned long intr;
    /** @brief Interrupt mask */
    unsigned long mask;
} _MI_regs_s;

/**
 * @brief Register definition for the VI interface
 * @ingroup lowlevel
 */
typedef struct VI_regs_s {
    /** @brief VI control register.  Sets up various rasterization modes */
    unsigned long control;
    /** @brief Pointer to uncached buffer in memory to rasterize */
    unsigned short int * framebuffer;
    /** @brief Width of the buffer in pixels */
    unsigned long width;
    /** @brief Vertical interrupt control register.  Controls which horizontal
     *         line must be hit to generate a VI interrupt
     */
    unsigned long v_int;
    /** @brief Current vertical line counter. */
    unsigned long cur_line;
    /** @brief Timing generation register for PAL/NTSC signals */
    unsigned long timing;
    /** @brief Number of lines per frame */
    unsigned long v_sync;
    /** @brief Number of pixels in line and leap pattern */
    unsigned long h_sync;
    /** @brief Number of pixels in line, set identically to h_sync */
    unsigned long h_sync2;
    /** @brief Beginning and end of video horizontally */
    unsigned long h_limits;
    /** @brief Beginning and end of video vertically */
    unsigned long v_limits;
    /** @brief Beginning and end of color burst in vertical lines */
    unsigned long color_burst;
    /** @brief Horizontal scaling factor from buffer to screen */
    unsigned long h_scale;
    /** @brief Vertical scaling factor from buffer to screen */
    unsigned long v_scale;
} _VI_regs_s;

/**
 * @brief Register definition for the PI interface
 * @ingroup lowlevel
 */
typedef struct PI_regs_s {
    /** @brief Uncached address in RAM where data should be found */
    void * ram_address;
    /** @brief Address of data on peripheral */
    unsigned long pi_address;
    /** @brief How much data to read from RAM into the peripheral */
    unsigned long read_length;
    /** @brief How much data to write to RAM from the peripheral */
    unsigned long write_length;
    /** @brief Status of the PI, including DMA busy */
    unsigned long status;
} _PI_regs_s;

/** 
 * @brief Register definition for the SI interface
 * @ingroup lowlevel
 */
typedef struct SI_regs_s {
    /** @brief Uncached address in RAM where data should be found */
    volatile void * DRAM_addr;
    /** @brief Address to read when copying from PIF RAM */
    volatile void * PIF_addr_read;
    /** @brief Reserved word */
    unsigned long reserved1;
    /** @brief Reserved word */
    unsigned long reserved2;
    /** @brief Address to write when copying to PIF RAM */
    volatile void * PIF_addr_write;
    /** @brief Reserved word */
    unsigned long reserved3;
    /** @brief SI status, including DMA busy and IO busy */
    unsigned long status;
} _SI_regs_s;

#endif
