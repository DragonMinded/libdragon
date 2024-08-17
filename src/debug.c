/**
 * @file debug.c
 * @brief Debugging Support
 */
#ifndef NDEBUG
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/errno.h>
#include "console.h"
#include "debug.h"
#include "regsinternal.h"
#include "system.h"
#include "n64types.h"
#include "n64sys.h"
#include "dma.h"
#include "backtrace.h"
#include "usb.h"
#include "utils.h"
#include "libcart/cart.h"
#include "interrupt.h"
#include "backtrace.h"
#include "exception_internal.h"
#include "fatfs/ff.h"
#include "fatfs/ffconf.h"
#include "fatfs/diskio.h"

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

/** @brief internal backtrace printing function */
void __debug_backtrace(FILE *out, bool skip_exception);

/*********************************************************************
 * Log writers
 *********************************************************************/

/** ISViewer register for magic value (to check ISViewer presence) */
#define ISVIEWER_MAGIC			0x13FF0000
/** ISViewer register for circular buffer write pointer */
#define ISVIEWER_WRITE_POINTER	0x13FF0014
/** ISViewer buffer */
#define ISVIEWER_BUFFER         0x13FF0020
/** ISViewer buffer length */
#define ISVIEWER_BUFFER_LEN     0x00000200 // Buffer size is configurable on real ISViewer, it's usually 64kB - 0x20

static bool isviewer_init(void)
{
	// No ISViewer on iQue Player (and touching PI addresses outside ROM
	// trigger a bus error, so better avoid it)
	if (sys_bbplayer())
		return false;

	// To check whether an ISViewer is present (probably emulated),
	// write some data to the "magic" register. If we can read it
	// back, it means that there's some memory there and we can
	// hopefully use it.

	// Magic value is different than what official ISViewer code used, but since
	// libdragon doesn't implement correct access pattern (circular buffer)
	// we want to differentiate our implementation from the real thing
	const uint32_t magic = 0x12345678;

	// Write inverted magic value to check if the memory is truly writable,
	// and to make sure there's no residual value that's equal to our magic value
	io_write(ISVIEWER_MAGIC, ~magic);
	if (io_read(ISVIEWER_MAGIC) != ~magic) {
		return false;
	}

	io_write(ISVIEWER_MAGIC, magic);
	return io_read(ISVIEWER_MAGIC) == magic;
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
			uint32_t value = ((uint32_t)data[0] << 24) |
							 ((uint32_t)data[1] << 16) |
							 ((uint32_t)data[2] <<  8) |
							 ((uint32_t)data[3] <<  0);
			io_write(ISVIEWER_BUFFER + i, value);
			data += 4;
		}

		// Flush the data into the ISViewer
		// We use write pointer register as length register,
		// but that's fine for emulators that usually doesn't
		// update the read and write pointers anyways.
		io_write(ISVIEWER_WRITE_POINTER, l);
		len -= l;
	}
}

static void usblog_write(const uint8_t *data, int len)
{
	usb_write(DATATYPE_TEXT, data, len);
}

static void sdlog_write(const uint8_t *data, int len)
{
	// Avoid reentrant calls. If the SD card code for any reason generates
	// an exception, the exception handler will try to log more, which would
	// cause reentrant calls, that might corrupt the filesystem.
	static bool in_write = false;
	if (in_write) return;

	in_write = true;
	fwrite(data, 1, len, sdlog_file);
	in_write = false;
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
	DRESULT (*disk_read_sdram)(BYTE* buff, LBA_t sector, UINT count);
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
	_Static_assert(FF_MIN_SS == 512, "this function assumes sector size == 512");
	_Static_assert(FF_MAX_SS == 512, "this function assumes sector size == 512");
	if (fat_disks[pdrv].disk_read && PhysicalAddr(buff) < 0x00800000)
		return fat_disks[pdrv].disk_read(buff, sector, count);
	if (fat_disks[pdrv].disk_read_sdram && io_accessible(PhysicalAddr(buff)))
		return fat_disks[pdrv].disk_read_sdram(buff, sector, count);
	return RES_PARERR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
	_Static_assert(FF_MIN_SS == 512, "this function assumes sector size == 512");
	_Static_assert(FF_MAX_SS == 512, "this function assumes sector size == 512");
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

DWORD get_fattime(void)
{
	time_t t = time(NULL);
	if (t == -1) {
		return (DWORD)(
			(FF_NORTC_YEAR - 1980) << 25 |
			FF_NORTC_MON << 21 |
			FF_NORTC_MDAY << 16
		);
	}
  	struct tm tm = *localtime(&t);
	return (DWORD)(
		(tm.tm_year - 80) << 25 |
		(tm.tm_mon + 1) << 21 |
		tm.tm_mday << 16 |
		tm.tm_hour << 11 |
		tm.tm_min << 5 |
		(tm.tm_sec >> 1)
	);
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

static DSTATUS fat_disk_initialize_sd(void)
{
	return cart_card_init() ? STA_NOINIT : 0;
}

static DRESULT fat_disk_read_sd(BYTE* buff, LBA_t sector, UINT count)
{
	return cart_card_rd_dram(buff, sector, count) ? RES_ERROR : RES_OK;
}

static DRESULT fat_disk_read_sdram_sd(BYTE* buff, LBA_t sector, UINT count)
{
	return cart_card_rd_cart(PhysicalAddr(buff), sector, count) ? RES_ERROR : RES_OK;
}

static DRESULT fat_disk_write_sd(const BYTE* buff, LBA_t sector, UINT count)
{
	return cart_card_wr_dram(buff, sector, count) ? RES_ERROR : RES_OK;
}

static fat_disk_t fat_disk_sd =
{
	fat_disk_initialize_sd,
	fat_disk_status_default,
	fat_disk_read_sd,
	fat_disk_read_sdram_sd,
	fat_disk_write_sd,
	fat_disk_ioctl_default
};

/*********************************************************************
 * FAT newlib wrappers
 *********************************************************************/

/** Maximum number of FAT files that can be concurrently opened */
#define MAX_FAT_FILES 4
static FIL fat_files[MAX_FAT_FILES] = {0};
static DIR find_dir;

static void __fresult_set_errno(FRESULT err)
{
	assertf(err != FR_INT_ERR, "FatFS assertion error");
	switch (err) {
	case FR_OK: return;
	case FR_DISK_ERR: 			errno = EIO; return;
	case FR_NOT_READY: 			errno = EBUSY; return;
	case FR_NO_FILE: 			errno = ENOENT; return;
	case FR_NO_PATH: 			errno = ENOENT; return;
	case FR_INVALID_NAME: 		errno = EINVAL; return;
	case FR_DENIED: 			errno = EACCES; return;
	case FR_EXIST: 				errno = EEXIST; return;
	case FR_INVALID_OBJECT: 	errno = EINVAL; return;
	case FR_WRITE_PROTECTED: 	errno = EROFS; return;
	case FR_INVALID_DRIVE: 		errno = ENODEV; return;
	case FR_NOT_ENABLED: 		errno = ENODEV; return;
	case FR_NO_FILESYSTEM: 		errno = ENODEV; return;
	case FR_MKFS_ABORTED: 		errno = EIO; return;
	case FR_TIMEOUT: 			errno = ETIMEDOUT; return;
	case FR_LOCKED: 			errno = EBUSY; return;
	case FR_NOT_ENOUGH_CORE: 	errno = ENOMEM; return;
	case FR_TOO_MANY_OPEN_FILES: errno = EMFILE; return;
	case FR_INVALID_PARAMETER: 	errno = EINVAL; return;
	default: 					errno = EIO; return;
	}
}

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
		__fresult_set_errno(res);
		fat_files[i].obj.fs = NULL;
		return NULL;
	}
	return &fat_files[i];
}

static void __fat_stat_fill(FSIZE_t size, BYTE attr, struct stat *st)
{
	memset(st, 0, sizeof(struct stat));
	st->st_size = size;
	if (attr & AM_RDO)
		st->st_mode |= 0444;
	else
		st->st_mode |= 0666;
	if (attr & AM_DIR)
		st->st_mode |= S_IFDIR;
	else
		st->st_mode |= S_IFREG;
}

static int __fat_stat(char *name, struct stat *st)
{
	FILINFO fno;
	FRESULT res = f_stat(name, &fno);
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -1;
	}
	__fat_stat_fill(fno.fsize, fno.fattrib, st);
	struct tm tm;
	memset(&tm, 0, sizeof(struct tm));
	tm.tm_min = (fno.ftime >> 5) & 0x3F;
	tm.tm_hour = fno.ftime >> 11;
	tm.tm_mday = fno.fdate & 0x1F;
	tm.tm_mon = ((fno.fdate >> 5) & 0x0F) - 1;
	tm.tm_year = (fno.fdate >> 9) + 80;
	st->st_mtim.tv_sec = mktime(&tm);
	st->st_mtim.tv_nsec = 0;
	return 0;
}

static int __fat_fstat(void *file, struct stat *st)
{
	FIL *f = file;
	__fat_stat_fill(f_size(f), f->obj.attr, st);
	return 0;
}

static int __fat_read(void *file, uint8_t *ptr, int len)
{
	UINT read;
	FRESULT res = f_read(file, ptr, len, &read);
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -1;
	}
	return read;
}

static int __fat_write(void *file, uint8_t *ptr, int len)
{
	UINT written;
	FRESULT res = f_write(file, ptr, len, &written);
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -1;
	}
	return written;
}

static int __fat_close(void *file)
{
	FRESULT res = f_close(file);
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -1;
	}
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
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -1;
	}
	return f_tell(f);
}

static int __fat_unlink(char *name)
{
	FRESULT res = f_unlink(name);
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -1;
	}
	return 0;
}

static int __fat_findnext(dir_t *dir)
{
	FILINFO info;
	FRESULT res = f_readdir(&find_dir, &info);
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -2;
	}

	// Check if we reached the end of the directory
	if (info.fname[0] == 0) {
		res = f_closedir(&find_dir);
		if (res != FR_OK) {
			__fresult_set_errno(res);
			return -2;
		}
		return -1;
	}

	strlcpy(dir->d_name, info.fname, sizeof(dir->d_name));
	if (info.fattrib & AM_DIR)
		dir->d_type = DT_DIR;
	else
		dir->d_type = DT_REG;
	dir->d_size = info.fsize;
	return 0;
}

static int __fat_findfirst(char *name, dir_t *dir)
{
	FRESULT res = f_opendir(&find_dir, name);
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -2;
	}
	return __fat_findnext(dir);
}

static int __fat_mkdir(char *path, mode_t mode)
{
	FRESULT res = f_mkdir(path);
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -1;
	}
	return 0;
}

static filesystem_t fat_fs = {
	.open = __fat_open,
	.stat = __fat_stat,
	.fstat = __fat_fstat,
	.lseek = __fat_lseek,
	.read = __fat_read,
	.write = __fat_write,
	.close = __fat_close,
	.unlink = __fat_unlink,
	.findfirst = __fat_findfirst,
	.findnext = __fat_findnext,
	.mkdir = __fat_mkdir,
};



/** Initialize the SD stack just once */
static bool sd_initialize_once(void) {
	static bool once = false;
	static bool ok = false;
	if (!once)
	{
		once = true;
		if (!sys_bbplayer())
			ok = cart_init() >= 0;
		else
			/* Don't call cart_init() on iQue, as touching PI addresses outside ROM
			   triggers a bus error. */
			ok = false;
	}
	return ok;
}

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
			/* Don't call usb_initialize() on iQue, as touching PI addresses outside ROM
			   triggers a bus error. */
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
	if (!sd_initialize_once())
		return false;

	fat_disks[FAT_VOLUME_SD] = fat_disk_sd;

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
	disable_interrupts();

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

	fprintf(stderr, "\n");

	va_list args;
	va_start(args, msg);
	__inspector_assertion(failedexpr, msg, args);
	va_end(args);
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

void __debug_backtrace(FILE *out, bool skip_exception)
{
	void *bt[32];
	int n = backtrace(bt, 32);

	fprintf(out, "Backtrace:\n");
	void cb(void *data, backtrace_frame_t *frame)
	{
		if (skip_exception) {
			skip_exception = strstr(frame->func, "<EXCEPTION HANDLER>") == NULL;
			return;
		}
		FILE *out = (FILE *)data;
		fprintf(out, "    ");
		backtrace_frame_print(frame, out);
		fprintf(out, "\n");
	}
	backtrace_symbols_cb(bt, n, 0, cb, out);
}

void debug_backtrace(void)
{
	__debug_backtrace(stderr, false);
}
#else

#include <stdlib.h>

void debug_assert_func(...) {
	abort();
}

#endif
