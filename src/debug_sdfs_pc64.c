/*
NOTE:
This code needs to live in libdragon and will also need to modify the debug.c file to include definitions for picocart64
to use functions in here
*/

/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 * 
 */

#include <libdragon.h>
// This lives in the picocart codebase copy of libdragon

// PicoCart64 Address space

// [READ/WRITE]: Scratch memory used for various functions
#ifndef PC64_BASE_ADDRESS_START
// PicoCart64 Address space
// [READ/WRITE]: Scratch memory used for various functions
// #define PC64_BASE_ADDRESS_START     0x81000000
#define PC64_BASE_ADDRESS_START     (0x1FFE0000)
#define PC64_BASE_ADDRESS_LENGTH    (0x00000800)
#define PC64_BASE_ADDRESS_END       (PC64_BASE_ADDRESS_START + PC64_BASE_ADDRESS_LENGTH - 1)

// [READ/WRITE]: Command address space. See register definitions below for details.
#define PC64_CIBASE_ADDRESS_START   (PC64_BASE_ADDRESS_END + 1)
#define PC64_CIBASE_ADDRESS_LENGTH  0x00000800
#define PC64_CIBASE_ADDRESS_END     (PC64_CIBASE_ADDRESS_START + PC64_CIBASE_ADDRESS_LENGTH - 1)

// [READ]: Returns pseudo-random values.
//         Address does not matter.
//         Each returned 16-bit word generates a new random value.
//         PC64_REGISTER_RESET_RAND resets the random seed.
#define PC64_RAND_ADDRESS_START     0x82000000
#define PC64_RAND_ADDRESS_LENGTH    0x01000000
#define PC64_RAND_ADDRESS_END       (PC64_RAND_ADDRESS_START + PC64_RAND_ADDRESS_LENGTH - 1)

// [Read]: Returns PC64_MAGIC
#define PC64_REGISTER_MAGIC         0x00000000
#define PC64_MAGIC                  0xDEAD6400

// [WRITE]: Write number of bytes to print from TX buffer
#define PC64_REGISTER_UART_TX       0x00000004

// [WRITE]: Set the random seed to a 32-bit value
#define PC64_REGISTER_RAND_SEED     0x00000008

/* *** SD CARD *** */
// [READ]: Signals pico to start data read from SD Card
#define PC64_COMMAND_SD_READ       (PC64_REGISTER_RAND_SEED + 0x4)

// [READ]: Load selected rom into memory and boot, 
#define PC64_COMMAND_SD_ROM_SELECT (PC64_COMMAND_SD_READ + 0x4)

// [READ] 1 while sd card is busy, 0 once the CI is free
#define PC64_REGISTER_SD_BUSY (PC64_COMMAND_SD_ROM_SELECT + 0x4)

// [WRITE] Sector to read from SD Card, 8 bytes
#define PC64_REGISTER_SD_READ_SECTOR0 (PC64_REGISTER_SD_BUSY + 0x4)
#define PC64_REGISTER_SD_READ_SECTOR1 (PC64_REGISTER_SD_READ_SECTOR0 + 0x4)

// [WRITE] number of sectors to read from the sd card, 4 bytes
#define PC64_REGISTER_SD_READ_NUM_SECTORS (PC64_REGISTER_SD_READ_SECTOR1 + 0x4)

// [WRITE] write the selected file name that should be loaded into memory
// 255 bytes
#define PC64_REGISTER_SD_SELECT_ROM (PC64_REGISTER_SD_READ_NUM_SECTORS + 0x4)

#endif

// void pc_pi_read_raw(void *dest, uint32_t base, uint32_t offset, uint32_t len)
// {
// 	assert(dest != NULL);

// 	disable_interrupts();
// 	dma_wait();

// 	MEMORY_BARRIER();
// 	PI_regs->ram_address = UncachedAddr(dest);
// 	MEMORY_BARRIER();
// 	PI_regs->pi_address = offset | base;
// 	MEMORY_BARRIER();
// 	PI_regs->write_length = len - 1;
// 	MEMORY_BARRIER();

// 	enable_interrupts();
// 	dma_wait();
// }

// void pc_pi_write_raw(const void *src, uint32_t base, uint32_t offset, uint32_t len)
// {
// 	assert(src != NULL);

// 	disable_interrupts();
// 	dma_wait();

// 	MEMORY_BARRIER();
// 	PI_regs->ram_address = UncachedAddr(src);
// 	MEMORY_BARRIER();
// 	PI_regs->pi_address = offset | base;
// 	MEMORY_BARRIER();
// 	PI_regs->read_length = len - 1;
// 	MEMORY_BARRIER();

// 	enable_interrupts();
// 	dma_wait();
// }

void pc64_debug_print() {
	fprintf(stdout, "PC64_CIBASE_ADDRESS_START: %08x\n", PC64_CIBASE_ADDRESS_START);
	
}

static uint8_t pc64_sd_wait() {
    uint32_t timeout = 0;
    uint32_t isBusy __attribute__((aligned(8)));
	isBusy = 1;

    // Wait until the cartridge interface is ready
    do {
		
        // returns 1 while sd card is busy
		isBusy = io_read(PC64_CIBASE_ADDRESS_START + PC64_REGISTER_SD_BUSY);
        
        // Took too long, abort
        if((timeout++) > 10000000) {
			fprintf(stdout, "SD_WAIT timed out. isBusy: %ld\n", isBusy);
			return -1;
		}
    }
    while(isBusy != 0);
    (void) timeout; // Needed to stop unused variable warning

    // Success
    return 0;
}

static DRESULT fat_disk_read_pc64(BYTE* buff, LBA_t sector, UINT count) {
	_Static_assert(FF_MIN_SS == 512, "this function assumes sector size == 512");
	_Static_assert(FF_MAX_SS == 512, "this function assumes sector size == 512");

	// fprintf(stdout, "read sector: %llu, count: %d\n", sector, count);

	int retryCount = 2;
	int i = 0;
	do {
		uint64_t current_sector = sector + i;
		
		uint32_t part0 __attribute__((aligned(8)));
		part0 = (current_sector & 0xFFFFFFFF00000000LL) >> 32;

		uint32_t part1 __attribute__((aligned(8)));
		part1 = (current_sector & 0x00000000FFFFFFFFLL);

		// send sector
		io_write(PC64_CIBASE_ADDRESS_START + PC64_REGISTER_SD_READ_SECTOR0, part0);

		io_write(PC64_CIBASE_ADDRESS_START + PC64_REGISTER_SD_READ_SECTOR1, part1);

		// send num sectors to read
		io_write(PC64_CIBASE_ADDRESS_START + PC64_REGISTER_SD_READ_NUM_SECTORS, 1);

		// start the load
		io_write(PC64_CIBASE_ADDRESS_START + PC64_COMMAND_SD_READ, 1);

        // wait for the sd card to finish
        if(pc64_sd_wait() == 0) {
			data_cache_hit_writeback_invalidate(buff, 512);
			dma_read(buff, PC64_BASE_ADDRESS_START, 512);
			buff += 512;
			i++;
		} else {
			retryCount--;
			fprintf(stderr, "wait timeout\n");
		}

		if (retryCount <= 0) {
			break;
		}
		
	} while (i < count);

	return RES_OK;
}

static DRESULT fat_disk_write_pc64(const BYTE* buff, LBA_t sector, UINT count)
{
	fprintf(stderr, "Picocart64 does not currently support SD card writes.\n");
    return RES_OK;
}

static DSTATUS fat_disk_initialize_pc64(void) { return RES_OK; }