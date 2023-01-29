/*********************************************************************
 * FAT backend: SC64
 *********************************************************************/

// SC64 internal 8 KiB general use buffer
#define SC64_BUFFER_ADDRESS		0xBFFE0000
#define SC64_BUFFER_SIZE		8192

// SC64 SD card related commands
#define SC64_CMD_SD_CARD_OP		'i'
#define SC64_CMD_SD_SECTOR_SET	'I'
#define SC64_CMD_SD_READ		's'
#define SC64_CMD_SD_WRITE		'S'

// SD card operation IDs
#define SC64_SD_CARD_OP_INIT	1

// Utility functions for SC64 communication, defined in usb.c
extern uint32_t usb_sc64_execute_cmd(uint8_t cmd, uint32_t *args, uint32_t *result);

static bool sc64_sd_card_init(void)
{
	uint32_t args[2] = { 0, SC64_SD_CARD_OP_INIT };
	return usb_sc64_execute_cmd(SC64_CMD_SD_CARD_OP, args, NULL) != 0;
}

static bool sc64_sd_read_sectors(uint32_t address, LBA_t sector, UINT count)
{
	uint32_t sector_set_args[2] = { sector, 0 };
	uint32_t read_args[2] = { address, count };
	if (usb_sc64_execute_cmd(SC64_CMD_SD_SECTOR_SET, sector_set_args, NULL))
		return true;
	return usb_sc64_execute_cmd(SC64_CMD_SD_READ, read_args, NULL) != 0;
}

static bool sc64_sd_write_sectors(uint32_t address, LBA_t sector, UINT count)
{
	uint32_t sector_set_args[2] = { sector, 0 };
	uint32_t write_args[2] = { address, count };
	if (usb_sc64_execute_cmd(SC64_CMD_SD_SECTOR_SET, sector_set_args, NULL))
		return true;
	return usb_sc64_execute_cmd(SC64_CMD_SD_WRITE, write_args, NULL) != 0;
}

static DSTATUS fat_disk_initialize_sc64(void)
{
	if (sc64_sd_card_init())
		return STA_NODISK;
	return 0;
}

static DRESULT fat_disk_read_sc64(BYTE* buff, LBA_t sector, UINT count)
{
	_Static_assert(FF_MIN_SS == 512, "this function assumes sector size == 512");
	_Static_assert(FF_MAX_SS == 512, "this function assumes sector size == 512");
	while (count > 0)
	{
		UINT sectors_to_process = MIN(count, SC64_BUFFER_SIZE/512);
		if (sc64_sd_read_sectors(SC64_BUFFER_ADDRESS, sector, sectors_to_process))
			return FR_DISK_ERR;
		data_cache_hit_writeback_invalidate(buff, sectors_to_process*512);
		dma_read(buff, SC64_BUFFER_ADDRESS, sectors_to_process*512);
		buff += sectors_to_process*512;
		sector += sectors_to_process;
		count -= sectors_to_process;
	}
	return RES_OK;
}

static DRESULT fat_disk_write_sc64(const BYTE* buff, LBA_t sector, UINT count)
{
	_Static_assert(FF_MIN_SS == 512, "this function assumes sector size == 512");
	_Static_assert(FF_MAX_SS == 512, "this function assumes sector size == 512");
	while (count > 0)
	{
		UINT sectors_to_process = MIN(count, SC64_BUFFER_SIZE/512);
		if (((uint32_t)buff & 7) == 0)
		{
			data_cache_hit_writeback(buff, sectors_to_process*512);
			dma_write(buff, SC64_BUFFER_ADDRESS, sectors_to_process*512);
		}
		else
		{
			typedef uint32_t u_uint32_t __attribute__((aligned(1)));

			uint32_t* dst = (uint32_t*)(SC64_BUFFER_ADDRESS);
			u_uint32_t* src = (u_uint32_t*)buff;
			for (int i = 0; i < (sectors_to_process*512)/16; i++)
			{
				uint32_t a = *src++; uint32_t b = *src++; uint32_t c = *src++; uint32_t d = *src++;
				*dst++ = a;          *dst++ = b;          *dst++ = c;          *dst++ = d;
			}
		}
		if (sc64_sd_write_sectors(SC64_BUFFER_ADDRESS, sector, sectors_to_process))
			return FR_DISK_ERR;
		buff += sectors_to_process*512;
		sector += sectors_to_process;
		count -= sectors_to_process;
	}
	return RES_OK;
}
