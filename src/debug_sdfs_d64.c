/*********************************************************************
 * FAT backend: 64drive
 *********************************************************************/

#define D64_CIBASE_ADDRESS   0xB8000000
#define D64_BUFFER           0x00000000
#define D64_REGISTER_STATUS  0x00000200
#define D64_REGISTER_COMMAND 0x00000208
#define D64_REGISTER_LBA     0x00000210
#define D64_REGISTER_LENGTH  0x00000218
#define D64_REGISTER_RESULT  0x00000220

#define D64_CI_IDLE  0x00
#define D64_CI_BUSY  0x10
#define D64_CI_WRITE 0x20

#define D64_COMMAND_SD_READ  0x01
#define D64_COMMAND_SD_WRITE 0x10
#define D64_COMMAND_SD_RESET 0x1F
#define D64_COMMAND_ABORT    0xFF

// Utility functions for 64drive communication, defined in usb.c
extern int8_t usb_64drive_wait(void);
extern void usb_64drive_setwritable(int8_t enable);

static DRESULT fat_disk_read_64drive(BYTE* buff, LBA_t sector, UINT count)
{
	_Static_assert(FF_MIN_SS == 512, "this function assumes sector size == 512");
	_Static_assert(FF_MAX_SS == 512, "this function assumes sector size == 512");

	for (int i=0;i<count;i++)
	{
		usb_64drive_wait();
		io_write(D64_CIBASE_ADDRESS + D64_REGISTER_LBA, sector+i);
		usb_64drive_wait();
		io_write(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, D64_COMMAND_SD_READ);
		if (usb_64drive_wait() != 0)
		{
			debugf("[debug] fat_disk_read_64drive: wait timeout\n");
			// Operation is taking too long. Probably SD was not inserted.
			// Send a COMMAND_ABORT and SD_RESET, and return I/O error.
			// Note that because of a 64drive firmware bug, this is not
			// sufficient to unblock the 64drive. The USB channel will stay
			// unresponsive. We don't currently have a workaround for this.
			io_write(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, D64_COMMAND_ABORT);
			usb_64drive_wait();
			io_write(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, D64_COMMAND_SD_RESET);
			usb_64drive_wait();
			return FR_DISK_ERR;
		}

		data_cache_hit_writeback_invalidate(buff, 512);
		dma_read(buff, D64_CIBASE_ADDRESS + D64_BUFFER, 512);
		buff += 512;
	}
	return RES_OK;
}

static DRESULT fat_disk_write_64drive(const BYTE* buff, LBA_t sector, UINT count)
{
	_Static_assert(FF_MIN_SS == 512, "this function assumes sector size == 512");
	_Static_assert(FF_MAX_SS == 512, "this function assumes sector size == 512");

	for (int i=0;i<count;i++)
	{
		if (((uint32_t)buff & 7) == 0)
		{
			data_cache_hit_writeback(buff, 512);
			dma_write(buff, D64_CIBASE_ADDRESS + D64_BUFFER, 512);
		}
		else
		{
			typedef uint32_t u_uint32_t __attribute__((aligned(1)));

			uint32_t* dst = (uint32_t*)(D64_CIBASE_ADDRESS + D64_BUFFER);
			u_uint32_t* src = (u_uint32_t*)buff;
			for (int i = 0; i < 512/16; i++)
			{
				uint32_t a = *src++; uint32_t b = *src++; uint32_t c = *src++; uint32_t d = *src++;
				*dst++ = a;          *dst++ = b;          *dst++ = c;          *dst++ = d;
			}
		}

		usb_64drive_wait();
		io_write(D64_CIBASE_ADDRESS + D64_REGISTER_LBA, sector+i);
		usb_64drive_wait();
		io_write(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, D64_COMMAND_SD_WRITE);
		if (usb_64drive_wait() != 0)
		{
			debugf("[debug] fat_disk_write_64drive: wait timeout\n");
			// Operation is taking too long. Probably SD was not inserted.
			// Send a COMMAND_ABORT and SD_RESET, and return I/O error.
			// Note that because of a 64drive firmware bug, this is not
			// sufficient to unblock the 64drive. The USB channel will stay
			// unresponsive. We don't currently have a workaround for this.
			io_write(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, D64_COMMAND_ABORT);
			usb_64drive_wait();
			io_write(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, D64_COMMAND_SD_RESET);
			usb_64drive_wait();
			return FR_DISK_ERR;
		}

		buff += 512;
	}

	return RES_OK;
}

static DSTATUS fat_disk_initialize_64drive(void) { return 0; }