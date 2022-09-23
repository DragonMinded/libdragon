/**
 * @file debug.c
 * @brief Debugging Support
 */

#include <libdragon.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "regsinternal.h"
#include "system.h"
#include "usb.h"
#include "utils.h"
#include "fatfs/ff.h"
#include "fatfs/ffconf.h"
#include "fatfs/diskio.h"

// SD implementations
#include "debug_sdfs_ed64.c"
#include "debug_sdfs_64drive.c"
#include "debug_sdfs_sc64.c"

/**
 * @defgroup debug Debugging Support
 * @brief Debugging support through development cartridges and emulators.
 *
 * The debugging library is a collection of different features
 * that can be used to simplify development of N64 applications.
 * Each feature can be independently activated and used.
 *
 * Features currently fall into two groups:
 *
 *  * Logging. These features allow creating a logging channel for the
 *    developer to inspect the application while it is running and
 *    afterwards. Logging can be performed through the USB channel
 *    of a supported cartridge (#DEBUG_FEATURE_LOG_USB), or through
 *    N64 emulators on PC (#DEBUG_FEATURE_LOG_ISVIEWER), or even
 *    redirected to a file on a SD card inserted into the development
 *    cartridge (#DEBUG_FEATURE_LOG_SD).
 *    On N64, logging can simply be performed by writing to stderr,
 *    for instance through the #debugf macro.
 *
 *  * External filesystems. In addition to the read-only filesystem
 *    stored within the ROM image (dragonfs), these debugging features
 *    allow access to an external filesystem in both read and write mode.
 *    Currently, this is possibly on SD cards (#DEBUG_FEATURE_FILE_SD).
 *
 * All the debugging features can be disabled at compile-time using
 * the standard "NDEBUG" macro. This is suggested when building
 * the final ROM, so that it's not required to manually remove
 * calls to debugging functions from the code. All the debugging code
 * and supporting libraries is also automatically stripped by the linker
 * from the ROM.
 */

/** @brief bitmask of features that have been enabled. */
static int enabled_features = 0;

/** @brief open log file to SD */
static FILE *sdlog_file = NULL;

/** @brief prefix used to address SD filesystem */
static char sdfs_prefix[16];
static char sdfs_logic_drive[3] = { 0 };

/** @brief debug writer functions (USB, SD, IS64) */
static void (*debug_writer[3])(const uint8_t *buf, int size) = { 0 };


/*********************************************************************
 * Log writers
 *********************************************************************/

/** ISViewer register for buffer write length */
#define ISVIEWER_WRITE_LEN       ((volatile uint32_t *)0xB3FF0014)
/** ISViewer buffer */
#define ISVIEWER_BUFFER          ((volatile uint32_t *)0xB3FF0020)
/** ISViewer buffer length */
#define ISVIEWER_BUFFER_LEN      0x00000200

static bool isviewer_init(void)
{
	// To check whether an ISViewer is present (probably emulated),
	// write some data to the buffer address. If we can read it
	// back, it means that there's some memory there and we can
	// hopefully use it.
	ISVIEWER_BUFFER[0] = 0x12345678;
	MEMORY_BARRIER();
	return ISVIEWER_BUFFER[0] == 0x12345678;
}

static void isviewer_write(const uint8_t *data, int len)
{
	while (len > 0)
	{
		uint32_t l = len < ISVIEWER_BUFFER_LEN ? len : ISVIEWER_BUFFER_LEN;

		// Write 32-bit aligned words to copy the buffer. Notice that
		// we might overflow the input buffer if it's not a multiple
		// of 4 bytes but it doesn't matter because we are going to
		// write the exact number of bytes later.
		for (int i=0; i < l; i+=4)
		{
			ISVIEWER_BUFFER[i/4] = ((uint32_t)data[0] << 24) |
								   ((uint32_t)data[1] << 16) |
								   ((uint32_t)data[2] <<  8) |
								   ((uint32_t)data[3] <<  0);
			data += 4;
		}

		// Flush the data into the ISViewer
		*ISVIEWER_WRITE_LEN = l;
		len -= l;
	}
}

static void usblog_write(const uint8_t *data, int len)
{
	usb_write(DATATYPE_TEXT, data, len);
}

static void sdlog_write(const uint8_t *data, int len)
{
	fwrite(data, 1, len, sdlog_file);
}

/*********************************************************************
 * FAT backend
 *********************************************************************/
/** @cond */

static FATFS sd_fat;
#define FAT_VOLUME_SD    0

typedef struct
{
	DSTATUS (*disk_initialize)(void);
	DSTATUS (*disk_status)(void);
	DRESULT (*disk_read)(BYTE* buff, LBA_t sector, UINT count);
	DRESULT (*disk_write)(const BYTE* buff, LBA_t sector, UINT count);
	DRESULT (*disk_ioctl)(BYTE cmd, void* buff);
} fat_disk_t;

static fat_disk_t fat_disks[FF_VOLUMES] = {0};

DSTATUS disk_initialize(BYTE pdrv)
{
	if (fat_disks[pdrv].disk_initialize)
		return fat_disks[pdrv].disk_initialize();
	return STA_NOINIT;
}

DSTATUS disk_status(BYTE pdrv)
{
	if (fat_disks[pdrv].disk_status)
		return fat_disks[pdrv].disk_status();
	return STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
	if (fat_disks[pdrv].disk_read)
		return fat_disks[pdrv].disk_read(buff, sector, count);
	return RES_PARERR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
	if (fat_disks[pdrv].disk_write)
		return fat_disks[pdrv].disk_write(buff, sector, count);
	return RES_PARERR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
	if (fat_disks[pdrv].disk_ioctl)
		return fat_disks[pdrv].disk_ioctl(cmd, buff);
	return RES_PARERR;
}

/** @endcond */

/*********************************************************************
 * Helpers
 *********************************************************************/

static DSTATUS fat_disk_status_default(void) { return 0; }

static DRESULT fat_disk_ioctl_default(BYTE cmd, void* buff)
{
	switch (cmd)
	{
		case CTRL_SYNC: return RES_OK;
		default:        return RES_PARERR;
	}
}

static fat_disk_t fat_disk_everdrive =
{
	fat_disk_initialize_everdrive,
	fat_disk_status_default,
	fat_disk_read_everdrive,
	fat_disk_write_everdrive,
	fat_disk_ioctl_default
};

static fat_disk_t fat_disk_64drive =
{
	fat_disk_initialize_64drive,
	fat_disk_status_default,
	fat_disk_read_64drive,
	fat_disk_write_64drive,
	fat_disk_ioctl_default
};

static fat_disk_t fat_disk_sc64 =
{
	fat_disk_initialize_sc64,
	fat_disk_status_default,
	fat_disk_read_sc64,
	fat_disk_write_sc64,
	fat_disk_ioctl_default
};

/*********************************************************************
 * FAT newlib wrappers
 *********************************************************************/

/** Maximum number of FAT files that can be concurrently opened */
#define MAX_FAT_FILES 4
static FIL fat_files[MAX_FAT_FILES] = {0};
static DIR find_dir;

static void *__fat_open(char *name, int flags)
{
	int i;
	for (i=0;i<MAX_FAT_FILES;i++)
		if (fat_files[i].obj.fs == NULL)
			break;
	if (i == MAX_FAT_FILES)
		return NULL;

	int fatfs_flags = 0;
	if ((flags & O_ACCMODE) == O_RDONLY)
		fatfs_flags |= FA_READ;
	if ((flags & O_ACCMODE) == O_WRONLY)
		fatfs_flags |= FA_WRITE;
	if ((flags & O_ACCMODE) == O_RDWR)
		fatfs_flags |= FA_READ | FA_WRITE;
	if ((flags & O_APPEND) == O_APPEND)
		fatfs_flags |= FA_OPEN_APPEND;
	if ((flags & O_TRUNC) == O_TRUNC)
		fatfs_flags |= FA_CREATE_ALWAYS;
	if ((flags & O_CREAT) == O_CREAT) {
		if ((flags & O_EXCL) == O_EXCL)
			fatfs_flags |= FA_CREATE_NEW;
		else
			fatfs_flags |= FA_OPEN_ALWAYS;
	} else
		 fatfs_flags |= FA_OPEN_EXISTING;

	FRESULT res = f_open(&fat_files[i], name, fatfs_flags);
	if (res != FR_OK)
	{
		fat_files[i].obj.fs = NULL;
		return NULL;
	}
	return &fat_files[i];
}

static int __fat_fstat(void *file, struct stat *st)
{
	FIL *f = file;

	memset(st, 0, sizeof(struct stat));
	st->st_size = f_size(f);
	if (f->obj.attr & AM_RDO)
		st->st_mode |= 0444;
	else
		st->st_mode |= 0666;
	if (f->obj.attr & AM_DIR)
		st->st_mode |= S_IFDIR;

	return 0;
}

static int __fat_read(void *file, uint8_t *ptr, int len)
{
	UINT read;
	FRESULT res = f_read(file, ptr, len, &read);
	if (res != FR_OK)
		debugf("[debug] fat: error reading file: %d\n", res);
	return read;
}

static int __fat_write(void *file, uint8_t *ptr, int len)
{
	UINT written;
	FRESULT res = f_write(file, ptr, len, &written);
	if (res != FR_OK)
		debugf("[debug] fat: error writing file: %d\n", res);
	return written;
}

static int __fat_close(void *file)
{
	FRESULT res = f_close(file);
	if (res != FR_OK)
		return -1;
	return 0;
}

static int __fat_lseek(void *file, int offset, int whence)
{
	FRESULT res;
	FIL *f = file;
	switch (whence)
	{
	case SEEK_SET: res = f_lseek(f, offset); break;
	case SEEK_CUR: res = f_lseek(f, f_tell(f) + offset); break;
	case SEEK_END: res = f_lseek(f, f_size(f) + offset); break;
	default: return -1;
	}
	if (res != FR_OK)
		return -1;
	return f_tell(f);
}

static int __fat_unlink(char *name)
{
	FRESULT res = f_unlink(name);
	if (res != FR_OK) {
		debugf("[debug] fat: unlink failed: %d\n", res);
		return -1;
	}
	return 0;
}

static int __fat_findnext(dir_t *dir)
{
	FILINFO info;
	FRESULT res = f_readdir(&find_dir, &info);
	if (res != FR_OK)
		return -1;

	strlcpy(dir->d_name, info.fname, sizeof(dir->d_name));
	if (info.fattrib & AM_DIR)
		dir->d_type = DT_DIR;
	else
		dir->d_type = DT_REG;
	return 0;
}

static int __fat_findfirst(char *name, dir_t *dir)
{
	FRESULT res = f_opendir(&find_dir, name);
	if (res != FR_OK)
		return -1;
	return __fat_findnext(dir);
}

static filesystem_t fat_fs = {
	__fat_open,
	__fat_fstat,
	__fat_lseek,
	__fat_read,
	__fat_write,
	__fat_close,
	__fat_unlink,
	__fat_findfirst,
	__fat_findnext
};



/** Initialize the USB stack just once */
static bool usb_initialize_once(void) {
	static bool once = false;
	static bool ok = false;
	if (!once)
	{
		once = true;
		if (!sys_bbplayer())
			ok = usb_initialize();
		else
			/* 64drive autodetection makes iQue player crash; disable USB
			   support altogether for now. */
			ok = false;
	}
	return ok;
}

static int __stderr_write(char *buf, unsigned int len)
{
	for (int i=0; i<sizeof(debug_writer) / sizeof(debug_writer[0]); i++)
		if (debug_writer[i])
			debug_writer[i]((uint8_t*)buf, len);

	// Pretend stderr is written correctly even if it isn't. 
	// There's really no benefit in bubbling up I/O errors
	// for the log spew. Also there could be multiple channels
	// and we wouldn't know how to report errors only on
	// some of them.
	return len;
}

static void hook_init_once(void)
{
	static bool once = false;
	if (!once)
	{
		// Connect debug spew to stderr
		stdio_t debug_calls = { 0, 0, __stderr_write };
		hook_stdio_calls(&debug_calls);

		// Configure stderr channel to be line-buffered, so that we avoid
		// too much overhead
		setvbuf(stderr, NULL, _IOLBF, BUFSIZ);

		once = true;
	}
}

bool debug_init_usblog(void)
{
	if (!usb_initialize_once())
		return false;

	hook_init_once();
	debug_writer[0] = usblog_write;
	enabled_features |= DEBUG_FEATURE_LOG_USB;
	return true;
}

bool debug_init_isviewer(void)
{
	if (!isviewer_init())
		return false;

	hook_init_once();
	debug_writer[1] = isviewer_write;
	return true;
}

bool debug_init_sdlog(const char *fn, const char *openfmt)
{
	sdlog_file = fopen(fn, openfmt);
	if (!sdlog_file)
		return false;

	hook_init_once();
	debug_writer[2] = sdlog_write;
	return true;
}

bool debug_init_sdfs(const char *prefix, int npart)
{
	if (!usb_initialize_once())
		return false;

	switch (usb_getcart())
	{
	case CART_64DRIVE:
		fat_disks[FAT_VOLUME_SD] = fat_disk_64drive;
		break;
	case CART_EVERDRIVE:
		fat_disks[FAT_VOLUME_SD] = fat_disk_everdrive;
		break;
	case CART_SC64:
		fat_disks[FAT_VOLUME_SD] = fat_disk_sc64;
		break;
	default:
		return false;
	}

	if (npart >= 0) {
		sdfs_logic_drive[0] = '0' + npart;
		sdfs_logic_drive[1] = ':';
		sdfs_logic_drive[2] = '\0';
	} else {
		sdfs_logic_drive[0] = '\0';
	}

	FRESULT res = f_mount(&sd_fat, sdfs_logic_drive, 1);
	if (res != FR_OK)
	{
		debugf("Cannot mount SD FAT filesystem: %d\n", res);
		return false;
	}

	strlcpy(sdfs_prefix, prefix, sizeof(sdfs_prefix));
	attach_filesystem(sdfs_prefix, &fat_fs);
	enabled_features |= DEBUG_FEATURE_FILE_SD;
	return true;
}

void debug_close_sdfs(void)
{
	if (enabled_features & DEBUG_FEATURE_FILE_SD)
	{
		detach_filesystem(sdfs_prefix);
		f_mount(NULL, sdfs_logic_drive, 0);
	}
}

void debug_assert_func_f(const char *file, int line, const char *func, const char *failedexpr, const char *msg, ...)
{
	// As first step, immediately print the assertion on stderr. This is
	// very likely to succeed as it should not cause any further allocations
	// and we would display the assertion immediately on logs.
	fprintf(stderr,
		"ASSERTION FAILED: %s\n"
		"file \"%s\", line %d%s%s\n",
		failedexpr, file, line,
		func ? ", function: " : "", func ? func : "");

	if (msg)
	{
		va_list args;

		va_start(args, msg);
		vfprintf(stderr, msg, args);
		va_end(args);

		fprintf(stderr, "\n");
	}

	// Now try to initialize the console. This might fail in extreme conditions
	// like memory full (display_init might fail), which will create an
	// endless loop of assertions / crashes. It would be nice to introduce
	// an "emergency console" to use in these cases that displays on a fixed
	// framebuffer at a fixed memory address without using malloc.
	console_close();
	console_init();
	console_set_debug(false);
	console_set_render_mode(RENDER_MANUAL);

	// Print the assertion again to the console.
	fprintf(stdout,
		"ASSERTION FAILED: %s\n"
		"file \"%s\", line %d%s%s\n",
		failedexpr, file, line,
		func ? ", function: " : "", func ? func : "");

	if (msg)
	{
		va_list args;

		va_start(args, msg);
		vfprintf(stdout, msg, args);
		va_end(args);

		fprintf(stdout, "\n");
	}

	console_render();
	abort();
}

/** @brief Assertion function that is registered into system.c at startup */
void debug_assert_func(const char *file, int line, const char *func, const char *failedexpr)
{
	debug_assert_func_f(file, line, func, failedexpr, NULL);
}

void debug_hexdump(const void *vbuf, int size)
{
	const uint8_t *buf = vbuf;
    bool lineskip = false;
    for (int i = 0; i < size; i+=16) {
        const uint8_t *d = buf + i;
        // If the current line of data is identical to the previous one,
        // just dump one "*" and skip all other similar lines
        if (i!=0 && memcmp(d, d-16, 16) == 0) {
            if (!lineskip) debugf("*\n");
            lineskip = true;
        } else {
            lineskip = false;
            debugf("%04x  ", i);
            for (int j=0;j<16;j++) {
				if (i+j < size)
					debugf("%02x ", d[j]);
				else
					debugf("   ");
                if (j==7) debugf(" ");
            }
            debugf("  |");
            for (int j=0;j<16;j++) {
				if (i+j < size)
					debugf("%c", d[j] >= 32 && d[j] < 127 ? d[j] : '.');
				else
					debugf(" ");
			}
            debugf("|\n");
        }
    }
}
