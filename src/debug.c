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
#include "fat.h"
#include "fatfs/ff.h"
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

static int fat_disk_status_default(void) { return 0; }

static int fat_disk_ioctl_default(uint8_t cmd, void* buff)
{
	switch (cmd)
	{
		case CTRL_SYNC: return RES_OK;
		default:        return RES_PARERR;
	}
}

static int fat_disk_initialize_sd(void)
{
	return cart_card_init() ? STA_NOINIT : 0;
}

static int fat_disk_read_sd(uint8_t* buff, int64_t sector, int count)
{
	_Static_assert(FF_MIN_SS == 512, "this function assumes sector size == 512");
	_Static_assert(FF_MAX_SS == 512, "this function assumes sector size == 512");
	assertf((uint32_t)sector == sector, "unsupported access to SD card > 2 TiB");

	// Check whether the user is requesting a read into RDRAM, or into
	// the cartridge's SDRAM (that is, a PI-accessible address). We try
	// to be generic here and not hardcode specific addresses; we assume
	// libcart will know better than us where the SDRAM is located.
	if (PhysicalAddr(buff) < 0x00800000)
		return cart_card_rd_dram(buff, sector, count) ? RES_ERROR : RES_OK;
	if (io_accessible(PhysicalAddr(buff)))
		return cart_card_rd_cart(PhysicalAddr(buff), sector, count) ? RES_ERROR : RES_OK;

	return RES_PARERR;
}


static int fat_disk_write_sd(const uint8_t* buff, int64_t sector, int count)
{
	_Static_assert(FF_MIN_SS == 512, "this function assumes sector size == 512");
	_Static_assert(FF_MAX_SS == 512, "this function assumes sector size == 512");
	assertf((uint32_t)sector == sector, "unsupported access to SD card > 2 TiB");
	return cart_card_wr_dram(buff, sector, count) ? RES_ERROR : RES_OK;
}

static fat_disk_t fat_disk_sd =
{
	.disk_initialize = fat_disk_initialize_sd,
	.disk_status = fat_disk_status_default,
	.disk_read = fat_disk_read_sd,
	.disk_write = fat_disk_write_sd,
	.disk_ioctl = fat_disk_ioctl_default,
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

	strlcpy(sdfs_prefix, prefix, sizeof(sdfs_prefix));
	fat_mount(sdfs_prefix, &fat_disk_sd, FAT_MOUNT_DEFERRED);
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
            debugf("%08lx %04x  ", (uint32_t)d, i);
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
