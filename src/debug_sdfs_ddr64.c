/*********************************************************************
 * FAT backend: Dreamdrive64
 *********************************************************************/

#include <libdragon.h>

// Dreamdrive64 Address space
#ifndef DDR64_BASE_ADDRESS_START
// [READ/WRITE]: Scratch memory used for various functions
#define DDR64_BASE_ADDRESS_START     	(0x1FFE0000)
#define DDR64_BASE_ADDRESS_LENGTH    	(0x00001000)
#define DDR64_BASE_ADDRESS_END       	(DDR64_BASE_ADDRESS_START + DDR64_BASE_ADDRESS_LENGTH - 1)

// [READ/WRITE]: Command address space. See register definitions below for details.
#define DDR64_CIBASE_ADDRESS_START   	(DDR64_BASE_ADDRESS_END + 1)
#define DDR64_CIBASE_ADDRESS_LENGTH  	(0x00000800)
#define DDR64_CIBASE_ADDRESS_END     	(DDR64_CIBASE_ADDRESS_START + DDR64_CIBASE_ADDRESS_LENGTH - 1)

/* *** SD CARD *** */
// [READ]: Signals dreamdrive to start data read from SD Card
#define DDR64_COMMAND_SD_READ				(0xC)

// [READ]: Load selected rom into memory and boot,
#define DDR64_COMMAND_SD_ROM_SELECT 		(DDR64_COMMAND_SD_READ + 0x4)

// [READ] 1 while sd card is busy, 0 once the CI is free
#define DDR64_REGISTER_SD_BUSY 				(DDR64_COMMAND_SD_ROM_SELECT + 0x4)

// [WRITE] Sector to read from SD Card, 8 bytes
#define DDR64_REGISTER_SD_READ_SECTOR0 		(DDR64_REGISTER_SD_BUSY + 0x4)
#define DDR64_REGISTER_SD_READ_SECTOR1 		(DDR64_REGISTER_SD_READ_SECTOR0 + 0x4)

// [WRITE] number of sectors to read from the sd card, 4 bytes
#define DDR64_REGISTER_SD_READ_NUM_SECTORS 	(DDR64_REGISTER_SD_READ_SECTOR1 + 0x4)

// [WRITE] write the selected file name that should be loaded into memory
// 255 bytes
#define DDR64_REGISTER_SD_SELECT_ROM 		(DDR64_REGISTER_SD_READ_NUM_SECTORS + 0x4)

// [WRITE] Register to define the cic type and save type.
// 0xFF00 == Cic
// 0x00FF == save
#define DDR64_REGISTER_SELECTED_ROM_META 	(DDR64_REGISTER_SD_SELECT_ROM + 0x4)

#endif

static DSTATUS fat_disk_initialize_ddr64(void) { return RES_OK; }

static uint8_t ddr64_sd_wait()
{
    uint32_t timeout = 0;
    uint32_t isBusy;

	// Wait until the cartridge interface is ready
	do {
		// returns 1 while sd card is busy
		isBusy = io_read(DDR64_CIBASE_ADDRESS_START + DDR64_REGISTER_SD_BUSY);
		// Took too long, abort
		if((timeout++) > 10000000) {
			return -1;
		}
	} while(isBusy != 0);
	(void) timeout; // Needed to stop unused variable warning

	// Success
	return 0;
}

static DRESULT fat_disk_read_ddr64(BYTE* buff, LBA_t sector, UINT count)
{
	_Static_assert(FF_MIN_SS == 512, "this function assumes sector size == 512");
	_Static_assert(FF_MAX_SS == 512, "this function assumes sector size == 512");

	int retryCount = 2;
	int i = 0;
	do {
		uint64_t current_sector = sector + i;

		uint32_t part0 __attribute__((aligned(8)));
		part0 = (current_sector & 0xFFFFFFFF00000000LL) >> 32;

		uint32_t part1 __attribute__((aligned(8)));
		part1 = (current_sector & 0x00000000FFFFFFFFLL);

		// send sector
		io_write(DDR64_CIBASE_ADDRESS_START + DDR64_REGISTER_SD_READ_SECTOR0, part0);

		io_write(DDR64_CIBASE_ADDRESS_START + DDR64_REGISTER_SD_READ_SECTOR1, part1);

		// send num sectors to read
		io_write(DDR64_CIBASE_ADDRESS_START + DDR64_REGISTER_SD_READ_NUM_SECTORS, 1);

		// start the load
		io_write(DDR64_CIBASE_ADDRESS_START + DDR64_COMMAND_SD_READ, 1);

		// wait for the sd card to finish
		if(ddr64_sd_wait() == 0) {
			data_cache_hit_writeback_invalidate(buff, 512);
			dma_read(buff, DDR64_BASE_ADDRESS_START, 512);
			buff += 512;
			i++;
		} else {
			retryCount--;
		}

		if (retryCount <= 0) {
			return RES_ERROR;
		}

	} while (i < count);

	return RES_OK;
}

static DRESULT fat_disk_write_ddr64(const BYTE* buff, LBA_t sector, UINT count)
{
	assertf(0, "Picocart64 does not currently support SD card writes.");
    return RES_OK;
}
