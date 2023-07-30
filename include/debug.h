/**
 * @file debug.h
 * @brief Debugging Support
 */

#ifndef __LIBDRAGON_DEBUG_H
#define __LIBDRAGON_DEBUG_H

#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * @brief Flag to activate the USB logging channel.
 *
 * The USB logging channel is supported on several different development
 * cartridges, using a compatible tool to acquire data on the PC side.
 *
 * Logging can be done by writing to stderr. You can use the debugf()
 * macro as a simple wrapper over fprintf(stderr) that can be
 * disabled when building the final ROM through the NDEBUG macro.
 *
 * Supported development cartridges:
 *
 *   * 64drive (rev 1 or 2)
 *   * EverDrive64
 *   * SC64
 *
 * Compatible PC tools:
 *
 *   * UNFLoader (https://github.com/buu342/N64-UNFLoader)
 *   * g64drive (https://github.com/rasky/g64drive)
 *   * ed64 (https://github.com/anacierdem/ed64)
 *
 */ 
#define DEBUG_FEATURE_LOG_USB       (1 << 0)

/** 
 * @brief Flag to activate the ISViewer logging channel.
 *
 * ISViewer was a real development cartridge that was used in the 90s
 * to debug N64 development. It is emulated by a few emulators to ease
 * the work of homebrew developers.
 *
 * Logging can be done by writing to stderr. You can use the debugf()
 * macro as a simple wrapper over fprintf(stderr) that can be
 * disabled when building the final ROM through the NDEBUG macro.
 *
 * Supported emulators:
 *
 *   * cen64 (https://github.com/n64dev/cen64) - run with -is-viewer command line flag
 *   * Ares (https://ares-emulator.github.io)
 *   * simple64 (https://simple64.github.io)
 *   * dgb-n64 (https://github.com/Dillonb/n64)
 *
 */
#define DEBUG_FEATURE_LOG_ISVIEWER  (1 << 1)

/** 
 * @brief Flag to activate the logging on CompactFlash/SD card.
 *
 * When this feature is activated, and assuming a SD card is
 * inserted into the cartridge slot and its read-only physical
 * switch is disabled, libdragon will write the logging stream
 * to a file called "libdragon.log" on the root of the card.
 * If the file already exists, logging will be appended to it.
 *
 * The SD card must be formatted in either FAT16, FAT32 or ExFat.
 *
 * Logging can be done by writing to stderr. You can use the debugf()
 * macro as a simple wrapper over fprintf(stderr) that can be
 * disabled when building the final ROM through the NDEBUG macro.
 *
 * Supported development cartridges:
 *
 *   * 64Drive HW1 and HW2
 *   * EverDrive-64 V1, V2, V2.5, V3, X7 and X5
 *   * ED64Plus / Super 64
 *   * SC64
 *
 * @note This feature works only if DEBUG_FEATURE_FILE_SD is also
 * activated.
 *
 * @note Because of a 64drive firmware bug, the 64drive USB stack
 * will crash if a SD command is sent while no SD card is inserted,
 * and there is no way to detect whether a SD card is in the slot.
 * So activating this feature without a SD card means that the USB
 * logging will stop working.
 */
#define DEBUG_FEATURE_LOG_SD        (1 << 2)

/**
 * @brief Flag to activate filesystem access to files on CompactFlash/SD.
 *
 * This flag activates a feature that allows direct read/write
 * access to the SD card / CompactFlash filesystem, available
 * on a development cartridge.
 *
 * To access the files on the SD card, simply open them
 * prefixing their name using the "sd:/" prefix in front of filenames.
 *
 * The SD card must be formatted in either FAT16, FAT32 or ExFat.
 * Long filenames are supported.
 *
 * Supported development cartridges:
 *
 *   * 64Drive HW1 and HW2
 *   * EverDrive-64 V1, V2, V2.5, V3, X7 and X5
 *   * ED64Plus / Super 64
 *   * SC64
 *
 */
#define DEBUG_FEATURE_FILE_SD       (1 << 3)


/**
 * @brief Flag to activate all supported debugging features.
 *
 * This flag can be passed to debug_init() to activate all
 * supported debugging features. It is a good default for
 * development, and should be used unless there are strong
 * constraints on ROM size. In fact, disabling unused 
 * debugging features will decrease the ROM size because
 * the unused code will not be linked.
 */
#define DEBUG_FEATURE_ALL           0xFF


#ifndef NDEBUG
	/** @brief Initialize USB logging. */
	bool debug_init_usblog(void);
	/** @brief Initialize ISViewer logging. */
	bool debug_init_isviewer(void);
	/** @brief Initialize SD logging. */
	bool debug_init_sdlog(const char *fn, const char *openfmt);
	/** @brief Initialize SD filesystem */
	bool debug_init_sdfs(const char *prefix, int npart);

	/** @brief Shutdown SD filesystem. */
	void debug_close_sdfs(void);

	/**
	 * @brief Initialize debugging features of libdragon.
	 *
	 * This function should be called at the beginning of main to request
	 * activation of debugging features. Passing DEBUG_FEATURE_ALL will
	 * try to activate all features.
	 *
	 * @param channels a bitmask of debugging features to activate.
	 *
	 * @return true if at least a feature was activated, false otherwise.
	 */
	static inline bool debug_init(int features)
	{
		bool ok = false; 
		if (features & DEBUG_FEATURE_LOG_USB)
			ok = debug_init_usblog() || ok;
		if (features & DEBUG_FEATURE_LOG_ISVIEWER)
			ok = debug_init_isviewer() || ok;
		if (features & DEBUG_FEATURE_FILE_SD)
			ok = debug_init_sdfs("sd:/", -1) || ok;
		if (features & DEBUG_FEATURE_LOG_SD)
			ok = debug_init_sdlog("sd:/libdragon.log", "a");
		return ok;
	}

	/** 
	 * @brief Write a message to the debugging channel.
	 *
	 * This macro is a simple wrapper over fprintf(stderr), to write
	 * a debugging message through all the activated debugging channels.
	 *
	 * Writing directly to stderr is perfectly supported; this macro
	 * only simplifies disabling all debugging features, because it
	 * is disabled when compiling with NDEBUG.
	 */
	#define debugf(msg, ...)           fprintf(stderr, msg, ##__VA_ARGS__)

	/** 
	 * @brief assertf() is like assert() with an attached printf().
	 *
	 * assertf() behaves exactly like assert(), but allows for a better
	 * debugging experience because it is possible to attach a formatted
	 * string that will be displayed in case the assert triggers.
	 *
	 * Assertion in general are supported by libdragon even without this
	 * debugging library: they abort execution displaying a console screen
	 * with the error message. Moreover, the assertion is also printed on
	 * stderr, so when using this debug library, it can be read on PC using
	 * one of the supported debugging channel.
	 */
	#define assertf(expr, msg, ...)   ({ \
		if (!(expr)) debug_assert_func_f(__FILE__, __LINE__, __func__, #expr, msg, ##__VA_ARGS__); \
	})

#else
	#define debug_init(ch)             ({ false; })
	#define debug_init_usblog()        ({ false; })
	#define debug_init_isviewer()      ({ false; })
	#define debug_init_sdlog(fn,fmt)   ({ false; })
	#define debug_init_sdfs(prefix,np) ({ false; })
	#define debugf(msg, ...)           ({ })
	#define assertf(expr, msg, ...)    ({ })
#endif

/**
 * @brief Do a hexdump of the specified buffer via #debugf
 * 
 * This is useful to dump a binary buffer for debugging purposes. The hexdump shown
 * contains both the hexadecimal and ASCII values, similar to what hex editors do.
 * 
 * Sample output:
 * 
 * <pre>
 * 0000  80 80 80 80 80 80 80 80  80 80 80 80 80 80 80 80   |................|
 * 0010  45 67 cd ef aa aa aa aa  aa aa aa aa aa aa aa aa   |Eg..............| 
 * 0020  9a bc 12 34 80 80 80 80  80 80 80 80 80 80 80 80   |...4............|
 * 0030  aa aa aa aa aa aa aa aa  ef 01 67 89 aa aa aa aa   |..........g.....|
 * 0040  80 80 80 80 80 80 80 80  00 00 00 00 80 80 80 80   |................|
 * </pre>
 * 
 * @param[in] buffer 	Buffer to dump
 * @param[in] size 		Size of the buffer in bytes
 */
void debug_hexdump(const void *buffer, int size);

/**
 * @brief Dump a backtrace (call stack) via #debugf
 * 
 * This function will dump the current call stack to the debugging channel. It is
 * useful to understand where the program is currently executing, and to understand
 * the context of an error.
 * 
 * The implementation of this function relies on the lower level #backtrace and
 * #backtrace_symbols functions, which are implemented in libdragon itself via
 * a symbol table embedded in the ROM. See #backtrace_symbols for more information.
 * 
 * @see #backtrace
 * @see #backtrace_symbols
 */
void debug_backtrace(void);

/** @brief Underlying implementation function for assert() and #assertf. */ 
void debug_assert_func_f(const char *file, int line, const char *func, const char *failedexpr, const char *msg, ...)
   __attribute__((noreturn, format(printf, 5, 6)));

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
