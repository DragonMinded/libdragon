/**
 * @file regsinternal.h
 * @brief Register definitions for various hardware in the N64
 * @ingroup lowlevel
 */
#ifndef __LIBDRAGON_REGSINTERNAL_H
#define __LIBDRAGON_REGSINTERNAL_H

#include <stdint.h>

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
    volatile void * address;
    /** @brief Size in bytes of the buffer to be played.  Should be
     *         number of stereo samples * 2 * sizeof( uint16_t ) 
     */
    uint32_t length;
    /** @brief DMA start register.  Write a 1 to this register to start
     *         playing back an audio sample. */
    uint32_t control;
    /** @brief AI status register.  Bit 31 is the full bit, bit 30 is the busy bit. */
    uint32_t status;
    /** @brief Rate at which the buffer should be played.
     *
     * Use the following formula to calculate the value: ((2 * clockrate / frequency) + 1) / 2 - 1
     */
    uint32_t dacrate;
    /** @brief Half-rate at which each single bit of a sample is shifted into the DAC.
     * 
     * Allowed values are 0..15, with "0" turning off the audio output. Values
     * 1 and 2 are a valid hardware configuration for the DAC, but result in
     * audio corruption because AI isn't able to shift bits that fast.
     * 
     * The maximum value that still allows samples to play correctly is dacrate / 66
     * (consider this is a half-rate and there are 2 16-bit samples). Lower values
     * will work too, though.
     */
    uint32_t bitrate;
} AI_regs_t;

/**
 * @brief Register definition for the VI interface
 * @ingroup lowlevel
 */
typedef struct VI_regs_s {
    /** @brief VI control register.  Sets up various rasterization modes */
    uint32_t control;
    /** @brief Pointer to uncached buffer in memory to rasterize */
    void * framebuffer;
    /** @brief Width of the buffer in pixels */
    uint32_t width;
    /** @brief Vertical interrupt control register.  Controls which horizontal
     *         line must be hit to generate a VI interrupt
     */
    uint32_t v_int;
    /** @brief Current vertical line counter. */
    uint32_t cur_line;
    /** @brief Timing generation register for PAL/NTSC signals */
    uint32_t timing;
    /** @brief Number of lines per frame */
    uint32_t v_sync;
    /** @brief Number of pixels in line and leap pattern */
    uint32_t h_sync;
    /** @brief Number of pixels in line, set identically to h_sync */
    uint32_t h_sync2;
    /** @brief Beginning and end of video horizontally */
    uint32_t h_limits;
    /** @brief Beginning and end of video vertically */
    uint32_t v_limits;
    /** @brief Beginning and end of color burst in vertical lines */
    uint32_t color_burst;
    /** @brief Horizontal scaling factor from buffer to screen */
    uint32_t h_scale;
    /** @brief Vertical scaling factor from buffer to screen */
    uint32_t v_scale;
} VI_regs_t;

/**
 * @brief Register definition for the PI interface
 * @ingroup lowlevel
 */
typedef struct PI_regs_s {
    /** @brief Uncached address in RAM where data should be found */
    volatile void * ram_address;
    /** @brief Address of data on peripheral */
    uint32_t pi_address;
    /** @brief How much data to read from RAM into the peripheral */
    uint32_t read_length;
    /** @brief How much data to write to RAM from the peripheral */
    uint32_t write_length;
    /** @brief Status of the PI, including DMA busy */
    uint32_t status;
    /** @brief Cartridge domain 1 latency in RCP clock cycles. Requires DMA status bit guards to work reliably */
    uint32_t dom1_latency;
    /** @brief Cartridge domain 1 pulse width in RCP clock cycles. Requires DMA status bit guards to work reliably */
    uint32_t dom1_pulse_width;
    // TODO: add remaining registers
} PI_regs_t;

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
    uint32_t reserved1;
    /** @brief Reserved word */
    uint32_t reserved2;
    /** @brief Address to write when copying to PIF RAM */
    volatile void * PIF_addr_write;
    /** @brief Reserved word */
    uint32_t reserved3;
    /** @brief SI status, including DMA busy and IO busy */
    uint32_t status;
} SI_regs_t;

/**
 * @brief Register definition for the SP interface
 * @ingroup lowlevel
 */
typedef struct SP_regs_s {
    /** @brief RSP memory address (IMEM/DMEM) */
    volatile void * RSP_addr;
    /** @brief RDRAM memory address */
    volatile void * DRAM_addr;
    /** @brief RDRAM->RSP DMA length */
    uint32_t rsp_read_length;
    /** @brief RDP->RDRAM DMA length */
    uint32_t rsp_write_length;
    /** @brief RSP status */
    uint32_t status;
    /** @brief RSP DMA full */
    uint32_t rsp_dma_full;
    /** @brief RSP DMA busy */
    uint32_t rsp_dma_busy;
    /** @brief RSP Semaphore */
    uint32_t rsp_semaphore;
} SP_regs_t;

#endif
