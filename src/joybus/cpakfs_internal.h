/**
 * @file cpakfs_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Internal Controller Pak Filesystem Routines
 */
#ifndef LIBDRAGON_CPAKFS_INTERNAL_H
#define LIBDRAGON_CPAKFS_INTERNAL_H

#include "joypad.h"
#include "joypad_accessory.h"
#include "joybus_accessory.h"
#include <assert.h>

/// @cond
#ifndef N64
#define be16(x)   __builtin_bswap16(x)
#define be16i(x)  (int16_t)__builtin_bswap16(x)
#define be32(x)   __builtin_bswap32(x)
#else
#define be16(x)   (x)
#define be16i(x)  (x)
#define be32(x)   (x)
#endif
/// @endcond

/** @brief Set to 1 to activate debug logs */
#ifndef CPAKFS_TRACE
#define CPAKFS_TRACE   0
#endif

/** @brief like tracef(), but writes only if #CPAKFS_TRACE is not 0 */
#define tracef(fmt, ...)  ({ if (CPAKFS_TRACE) fprintf(stderr, fmt, ##__VA_ARGS__); })

#define MAX_NOTES       16      ///< Maximum number of notes in a controller pak
#define MAX_BANKS       62      ///< Maximum number of banks in a controller pak
#define NUM_PAGES       128     ///< Number of pages in a bank (128 pages = 32KB)
#define BANK_SIZE       32768   ///< Size of a bank in bytes (32KB)
#define PAGE_SIZE       256     ///< Page size in bytes
#define BLOCK_SIZE      32      ///< Block size in bytes

#define NOTE_STATUS_OCCUPIED     (1<<1)     ///< Flag to mark a note as occupied (not empty)

#define FSFLAGS_DEFAULT          0x0000     ///< Default flags for a filesystem

/** @brief FAT entry (File Allocation Table) */
typedef struct {
    uint8_t bank;                   ///< Bank number (0-61)
    uint8_t page;                   ///< Page number (0-127)
} cpakfs_fat_entry_t;

static_assert(sizeof(cpakfs_fat_entry_t) == 2, "cpakfs_fat_entry_t must be 2 bytes");

#define FAT_LINEAR(e)               ((e).bank * NUM_PAGES + (e).page)       ///< Convert a FAT entry to linear FAT index
#define FAT_NEXT(fat, e)            ((fat)[(e).bank][(e).page])                  ///< Get the next FAT entry for a given entry
#define FAT_IS_VALID(e, reserved)   ((e).page > 0 && (e).page < 0x80 && ((e).bank != 0 || (e).page >= reserved))      ///< Check if a FAT entry is valid (point to a valid page)
#define FAT_IS_RESERVED(e)          ((e).page == 0 && (e).bank == 0)        ///< Check if the FAT entry marks a reserved page
#define FAT_IS_TERMINATOR(e)        ((e).page == 1 && (e).bank == 0)        ///< Check if a FAT entry marks the terminator of a file
#define FAT_IS_UNUSED(e)            ((e).page == 3 && (e).bank == 0)        ///< Check if a FAT entry marks an unused page

#define FAT_RESERVED                ((cpakfs_fat_entry_t){0, 0})                   ///< Reserved FAT entry
#define FAT_TERMINATOR              ((cpakfs_fat_entry_t){0, 1})                   ///< Terminator FAT entry
#define FAT_UNUSED                  ((cpakfs_fat_entry_t){0, 3})                   ///< Unused FAT entry

/** @brief ID sector */
typedef union {
    struct {
        uint8_t serial[24];         ///< Serial number
        uint16_t device_id_lsb;     ///< Device ID (in the LSB). Bit 0 should be 1
        uint16_t bank_size_msb;     ///< Bank size (in the MSB): number of FAT entries
        uint16_t checksum1;         ///< Checksum of the first 14 words
        uint16_t checksum2;         ///< Reversed checksum of the first 14 words
    };
    uint8_t data8[32];              ///< Raw data (8-bit access)
    uint16_t data16[16];            ///< Raw data (16-bit access)
} cpakfs_id_t;

/** @brief A note (similar to inode) */
typedef union {
    struct {
        char gamecode[4];               ///< Game code (ASCII, 4 chars)
        char pubcode[2];                ///< Publisher code (ASCII, 2 chars)
        cpakfs_fat_entry_t first_page;  ///< First page where data is stored
        uint8_t status;                 ///< Status flags (bit 1: occupied)
        uint8_t ext_padding_size;       ///< Size of the padding in the last page
        uint16_t ext_crc16;             ///< CRC16 checksum (libdragon extension)
        uint8_t ext[4];                 ///< File extension (custom codepage)
        uint8_t filename[16];           ///< Filename (custom codepage)
    };
    uint8_t data8[32];                  ///< Raw data (8-bit access)
} cpakfs_note_t;

static_assert(sizeof(cpakfs_note_t) == 32, "cpakfs_note_t must be 32 bytes");

/** @brief Calculate the checksum of a cpak sector ID */
void __cpakfs_fsid_checksum(cpakfs_id_t *id, uint16_t *checksum1, uint16_t *checksum2);

/** @brief  Compute the checksum of a FAT page, starting from a given entry index. */
uint8_t __cpakfs_fat_checksum(cpakfs_fat_entry_t *fat_page, int start_idx);

/** @brief Check whether a character is valid in the N64 codepage */
bool __cpakfs_n64_string_valid(uint8_t ch);

/** @brief Sanitize a string in the N64 codepage by replacing all invalid characters with NUL */
void __cpakfs_n64_string_sanitize(uint8_t *buf, int len);

#endif
