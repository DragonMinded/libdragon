/**
 * @file cpakfs.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Controller Pak filesystem library
 * @ingroup controllerpak
 * 
 * This module implements filesystem access to the information stored in the
 * controller pak, called "notes".
 * 
 * To check whether a controller pak is present, you can use #joypad_get_accessory_type,
 * from the joypad module.
 * 
 * ## Filesystem usage
 *
 * Call #cpakfs_mount to mount the controller pak associated to a certain
 * joypad port, and then use standard C functions to access the notes as
 * files in the mounted filesystem. The filesystem exposes a flat structure
 * where all files are located at the root level using the format:
 * "GAME.PU-filename.ext", where:
 * - GAME is a 4-character game code (or 8-character hex for non-alphanumeric)
 * - PU is a 2-character publisher code (or 4-character hex for non-alphanumeric)  
 * - filename is the file name
 * - ext is the optional file extension
 * 
 * For instance, to read the note for the game "Doubutsu no Mori" (Animal Crossing),
 * you can do the following:
 * 
 * \code{.c}
 *     if (cpakfs_mount(JOYPAD_PORT_1, "cpak1:/") < 0) {
 *        // handle errors, by inspecting errno [...]
 *       return;
 *     }
 * 
 *     // Read the following note:
 *     //   Game code: NAFJ
 *     //   Publisher code: 01
 *     //   Filename: DOUBUTSUNOMORI
 *     //   File extension: A
 *     FILE *f = fopen("cpak1:/NAFJ.01-DOUBUTSUNOMORI.A", "rb");
 * \endcode
 * 
 * To iterate over the notes in a controller pak, you can use the directory
 * functions defined in dir.h (eg: #dir_findfirst, #dir_walk, #dir_glob).
 * 
 * To write a note, create a file in the mounted filesystem using fopen,
 * and then write the data to it using fwrite.
 * 
 * To unmount the controller pak filesystem, call #cpakfs_unmount.
 * 
 * ## Codepage of filenames and extensions
 * 
 * Filenames and extensions in the controller pak filesystem are encoded
 * using a special codepage, where only a subset of ASCII characters are
 * allowed, plus some Kanji characters. Notably, lowercase letters are not
 * allowed.
 * 
 * In cpaks, you must specify filenames as UTF-8 strings. If the filename cannot
 * be represented in the codepage, you will get an error (with errno EINVAL).
 * The only exception is that lowercase letters are automatically converted
 * to uppercase, just like a non-case-sensitive filesystem would do.
 * 
 * The special codepage allows also for embedded NULs in filenames. Since these
 * cannot be directly represented in C strings, you can use the special character
 * `\001` as replacement for NUL. For instance, to create a file with an embedded
 * NUL, you can do the following:
 * 
 * \code{.c}
 *    // Create a file whose filename is "FOO<NUL>BAR"
 *    FILE *f = fopen("cpak1:/GAME.01-FOO\001BAR.TXT", "wb");
 * \endcode
 * 
 * ## Error codes
 * 
 * The filesystem tries to be POSIX compliant, so make sure to check error
 * codes via errno if an operation fails. In particular, the following error
 * codes can be returned.
 * 
 * ### Controller Pak specific errors
 * 
 *  * EIO: Input/output error on the wire. The serial connection is faulty,
 *    so either the cable is damaged or the cpak is electrically unstable.
 *  * ENXIO: The controller pak or the whole joypad has been abruptly disconnected
 *    during the operation.
 *  * ENODEV: the controller pak appears not to contain a valid filesystem, or
 *    it was corrupted. Use #cpakfs_fsck to try recovering the contents. Also
 *    returned when no controller pak is present on the specified port.
 *  * ENOSPC: No space left on the controller pak to allocate new pages for
 *    writing to a file.
 *  * EMFILE: Too many files. The note table is full (maximum 16 notes per pak).
 *  * EFTYPE: Inappropriate file type or format. The filesystem structure is
 *    corrupted, with invalid FAT entries or file chains.
 *  * EEXIST: File exists. Attempting to create a file with O_CREAT|O_EXCL
 *    when a file with the same name already exists.
 *  * ENOENT: No such file or directory. The requested file does not exist,
 *    or no more directory entries are available during directory iteration.
 *  * EINVAL: Invalid argument. For directory operations, the path is not "/"
 *    (the root directory). For file operations, the filename format is invalid.
 *    Also returned for invalid file descriptors or null pointers.
 * 
 * ### Generic filesystem errors
 * 
 *  * EBADF: Bad file descriptor. The file was not opened for the attempted
 *    operation (e.g., trying to read from a file opened only for writing).
 *  * ENOMEM: Not enough memory available for filesystem operations (e.g., when
 *    mounting a filesystem or allocating file handles).
 *  * EPERM: Operation not permitted. Returned when trying to mount a filesystem
 *    with a prefix that's already in use, or when trying to unmount a filesystem
 *    that doesn't exist.
 *  * ENFILE: Too many open files in the system. The file handle table is full.
 *  * ENOSYS: Function not implemented. Returned for unsupported operations
 *    on filesystems that don't implement certain features (e.g., some file
 *    operations if the filesystem doesn't support them).
 */
#ifndef LIBDRAGON_CPAKFS_H
#define LIBDRAGON_CPAKFS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "preview.h"
#include "ioctl.h"
#include "joypad.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Get the current bank/page of an open file in cpakfs
 *
 * This ioctl allows you to retrieve the current position of an open file
 * in terms of bank and page, packed into a single uint16_t value.
 * The high byte contains the bank number, the low byte contains the page number.
 *
 * Example usage:
 *
 *   uint16_t bankpage;
 *   ioctl(fileno(f), IOCPAKFS_GET_PAGE, &bankpage);
 *   int bank = bankpage >> 8;
 *   int page = bankpage & 0xFF;
 *
 * If the file is not valid, the ioctl returns -1 and errno is set.
 */
#define IOCPAKFS_GET_PAGE   _IO('K', 0)

/**
 * @brief Usage statistics for a Controller Pak
 * 
 * This structure is returned by #cpakfs_get_stats and contains the usage
 * statistics for a controller pak.
 */
typedef struct {
    struct {
        int total;      ///< Total number of pages in the controller pak
        int used;       ///< Number of pages used in the controller pak
    } pages;            ///< Statistics on pages in the controller pak
    struct {
        int total;      ///< Total number of notes in the controller pak
        int used;       ///< Number of notes used in the controller pak
    } notes;            ///< Statistics on notes in the controller pak
    int num_banks;      ///< Number of banks in the controller pak (as recorded in the filesystem)
} cpakfs_stats_t;

/** @brief Cpak filesystem issue levels */
typedef enum {
    /** The reported issue does not prevent a filesystem from mounting or otherwise working. 
     *  It's just advisory. 
     *  Example: a backup copy isn't synchronized with the main copy; a reserved sector
     *  isn't correctly marked as such, etc.
     */
    CPAKFS_LEVEL_INFO = 0,
    
    /**
     * The issue can prevent correct interpretation of the filesystem, but can be
     * recovered automatically.
     */
    CPAKFS_LEVEL_WARNING = 1,

    /**
     * The issue is critical; integrity of the filesystem is potentially compromised
     * and data may be lost after recovery.
     */
    CPAKFS_LEVEL_ERROR = 2,
} cpakfs_issue_level_t;

/** @brief Cpak filesystem issues */
typedef enum {
    CPAKFS_ISSUE_FSID_CHECKSUM_FAILURE = 1,         ///< Cannot find an ID sector with correct checksum
    CPAKFS_ISSUE_FSID_DEXDRIVE = 2,                 ///< ID sector has a known-corrupted checksum generated by DexDrive
    CPAKFS_ISSUE_FSID_CORRUPTED_BACKUP = 3,         ///< ID sector backup is corrupted (can be recovered from main)
    CPAKFS_ISSUE_FSID_WRONG_DEVICE_ID = 4,          ///< Device ID is not valid; this is just advisory so it can be ignored

    CPAKFS_ISSUE_FAT_CHECKSUM_FAILURE = 5,          ///< FAT page has invalid checksum
    CPAKFS_ISSUE_FAT_UNSYNCHRONIZED = 6,            ///< FAT page has unsynchronized copies
    CPAKFS_ISSUE_FAT_INVALID_RESERVED = 7,          ///< Reserved page has invalid FAT entry
    CPAKFS_ISSUE_FAT_INVALID_ENTRY = 8,             ///< Invalid FAT entry
    CPAKFS_ISSUE_FAT_INVALID_BANK = 9,              ///< FAT entry has invalid bank number

    CPAKFS_ISSUE_NOTE_INVALID_GAMECODE = 10,        ///< Note has invalid gamecode
    CPAKFS_ISSUE_NOTE_INVALID_PUBCODE = 11,         ///< Note has invalid publisher code
    CPAKFS_ISSUE_NOTE_INVALID_FIRST_PAGE = 12,      ///< Note has invalid first page
    CPAKFS_ISSUE_NOTE_INVALID_CHARSET = 13,         ///< Note has invalid characters in filename or extension
    CPAKFS_ISSUE_NOTE_NOT_OCCUPTED = 14,            ///< Note is not correctly marked as occupied

    CPAKFS_ISSUE_CHAIN_INFINITE_LOOP = 15,          ///< Chain has infinite loop
    CPAKFS_ISSUE_CHAIN_NO_TERMINATOR = 16,          ///< Chain does not end with terminator
    CPAKFS_ISSUE_CHAIN_COLLISION = 17,              ///< Chain collides with another chain
    CPAKFS_ISSUE_CHAIN_ORPHANED = 18,               ///< Orphaned chain found
} cpakfs_issue_t;

/** @brief Cpak filesystem report callback */
typedef void (*cpakfs_report_fn)(void *ctx, cpakfs_issue_t issue, cpakfs_issue_level_t level, const char *fmt, ...);

/**
 * @brief Mount the controller pak as filesystem
 * @preview
 * 
 * This function mounts the contents of a controller pak as a virtual
 * filesystem, with the specified prefix. After this function successfully
 * return, it is possible to access the notes in the cpak using standard
 * C functions like fopen.
 * 
 * \code{.c}
 *      if (cpak_mount(JOYPAD_PORT_1, "cpak1:/") < 0) {
 *         // handle errors, by inspecting errno [...]
 *         return;
 *      }
 * 
 *      // Read the following note:
 *      //   Game code: NAFJ
 *      //   Publisher code: 01
 *      //   Filename: DOUBUTSUNOMORI
 *      //   File extension: A
 *      FILE *f = fopen("cpak1:/NAFJ.01/DOUBUTSUNOMORI.A");
 * \endcode
 * 
 * The virtual filesystem structure is as follows:
 *   * Root directory contains no files, only subdirectories. The name of the
 *     subdirectories is a 4.2 ASCII string that encode the game code and
 *     publisher code (eg: "NSME.01")
 *   * Within each subdirectory, you can find one or multiple files that are
 *     the notes found in the cpak. The filenames are UTF-8 strings that must
 *     adhere to the special cpak charset.
 *   * Empty directories do not exist.
 * 
 * In case of error while mounting the filesystem, errno is set as follows:
 * 
 *  * EIO: Input/output error on the wire. The serial connection is faulty,
 *    so either the cable is damaged or the cpak is electrically unstable.
 *  * ENXIO: The controller pak or the whole joypad has been abruptly disconnected
 *    during the operation.
 *  * ENODEV: the controller pak appears not to contain a valid filesystem, or
 *    it was corrupted. Use #cpakfs_fsck to try recovering the contents.
 * 
 * @param port              Cpak to mount, identified by the joypad port
 * @param prefix            Filesystem prefix to use for mounting. Suggested
 *                          name is "cpakN:/" where "N" is the controller
 *                          port (1..4).
 * @return 0 if success, negative value in case of error (and errno is set)
 */
LIBDRAGON_PREVIEW_API
int cpakfs_mount(joypad_port_t port, const char *prefix);

/**
 * @brief Unmount the controller pak filesystem
 * @preview
 * 
 * This function unmounts the controller pak filesystem, waiting for all
 * pending operations to complete.
 * 
 * @param port              The controller pak to unmount
 * @return 0 if success, negative value in case of error (and errno is set)
 */
LIBDRAGON_PREVIEW_API
int cpakfs_unmount(joypad_port_t port);

/**
 * @brief Read the serial number of a controller pak
 * @preview
 * 
 * This function reads the 24-byte serial number of a controller pak.
 * This is a unique identifier that can be used to distinguish between
 * different controller paks. It is normally generated with random data
 * when the controller pak is formatted, so it does not contain printable
 * characters.
 * 
 * @param port          The controller pak to read the serial from
 * @param serial        The buffer where to store the serial number (24 bytes)
 * @return 0            if the serial was successfully read
 * @return negative     if an error occurred (eg: no cpak on the specified port),
 *                      and errno is set accordingly.
 */
LIBDRAGON_PREVIEW_API
int cpakfs_get_serial(joypad_port_t port, uint8_t serial[24]);

/**
 * @brief Read the usage state of a controller pak
 * @preview
 * 
 * @note The filesystem must be mounted before calling this function.
 * 
 * @param port          The controller pak to read the usage state from
 * @param stats         The structure where to store the usage statistics
 * @return 0            if the serial was successfully read
 * @return negative     if an error occurred (eg: no cpak on the specified port),
 *                      and errno is set accordingly.
 */
LIBDRAGON_PREVIEW_API
int cpakfs_get_stats(joypad_port_t port, cpakfs_stats_t *stats);

/**
 * @brief Check the integrity of a controller pak
 * @preview
 * 
 * This function checks the integrity of a controller pak filesystem, and
 * optionally tries also to repair it if unreadable, and recover as much as
 * possible.
 * 
 * Found issues can be reported via the report callback, which is called
 * for each issue found.
 * 
 * The return value will report the number of issues found whose level is at least
 * CPAKFS_LEVEL_WARNING. In fact, issues of kind CPAKFS_LEVEL_INFO, while fixable,
 * do not impact filesystem integrity in any way (for instance, a backup copy
 * of some metadata might be corrupted, but the main copy is still valid).
 * 
 * @param port          The controller pak to check the integrity of
 * @param fix_errors    Whether to fix the errors found
 * @param report        Optional callback to report issues found during the check
 * @param report_ctx    Optional context to pass to the report callback
 * @return positive     Number of issues found (and possibly fixed if @p fix_errors is true).
 * @return 0            No issues found.
 * @return negative     If a physical error occurred (eg: no cpak on the specified port),
 *                      and errno will be set accordingly. 
 */
LIBDRAGON_PREVIEW_API
int cpakfs_fsck(joypad_port_t port, bool fix_errors,
        cpakfs_report_fn report, void *report_ctx);

/**
 * @brief Format a controller pak
 * @preview
 * 
 * This function formats a controller pak, resetting to a pristine, valid
 * empty state. By default, the function only erases the filesystem
 * metadata but does not purge content from all the pages. This is OK for
 * most use cases.
 * 
 * If @p erase is true, all pages are written with zeros, effectively erasing
 * all the content from the controller pak.
 * 
 * @param port      The controller pak to format
 * @param erase     Whether to erase all content or just the metadata
 * @return          0 on success, negative on error
 */
LIBDRAGON_PREVIEW_API
int cpakfs_format(joypad_port_t port, bool erase);


/** 
 * @brief Cpakfs path components
 * 
 * A file path on the cpak filesystem is composed of four components:
 * 
 * -  Game code: this would normally match the game ID in the ROM header, so
 *    it is a 4-character string made of uppercase letters and digits. In some
 *    cases (eg: Datel Gameshark), it is made of non-printable characters.
 * -  Publisher code: this is a 2-character string that identifies the publisher
 *    of the game. It was a global registry assigned by Nintendo. Like the game
 *    code, this is normally just uppercase letters and digits.
 * -  Filename: this is a character string in a custom codepage. The codepage
 *    is a subset of ASCII, notably excluding lowercase letters and common
 *    characters like the underscore, but including some Katana characters.
 *    Notice also that the string might contain embedded NULs. The actual
 *    filename length is determined by the position of the last non-zero byte.
 * -  Extension: an optional 4-character string to further identify the file.
 *    Sometimes games use the same filename for all saves and just put an
 *    index in the extension, like "SAVE.0", "SAVE.1", etc. The actual
 *    extension length is determined by the position of the last non-zero byte.
 * 
 * Cpakfs in libdragon offers a simplified interface to manipulate paths
 * using what it called a "full name". A full name is a UTF-8 string that
 * contains all the components, composed as follows:
 * 
 *    GAME.PU-filename.ext
 * 
 * The full name is also the string that you can use when manipulating notes
 * via the filesystem interface (eg: fopen, #dir_walk, etc).
 * 
 * You can use #cpakfs_path_parse and #cpakfs_path_format to convert from
 * and to the UTF-8 full name and the #cpakfs_path_t structure. Normally,
 * users of cpakfs will only need to use the full name, but some advanced
 * uses might require accessing the individual components.
 */
typedef struct {
    uint8_t gamecode[4];              ///< Game code (4 chars, normally ASCII)
    uint8_t pubcode[2];               ///< Publisher code (2 chars, normally ASCII)
    uint8_t filename[16];             ///< Filename in N64 codepage (zero-padded)
    uint8_t ext[4];                   ///< Extension in N64 codepage (zero-padded)
} cpakfs_path_t;

/** @brief #cpakfs_path_parse result codes */
typedef enum {
    CPAKFS_PARSE_OK = 0,                       ///< Path parsed successfully
    CPAKFS_PARSE_ERR_GAMECODE_TOO_SHORT = -1,  ///< Game code too short in the path
    CPAKFS_PARSE_ERR_GAMECODE_TOO_LONG = -2,   ///< Game code too long in the path
    CPAKFS_PARSE_ERR_GAMECODE_CHAR = -3,       ///< Invalid game code character
    CPAKFS_PARSE_ERR_PUBCODE_TOO_SHORT = -4,   ///< Publisher code too short in the path
    CPAKFS_PARSE_ERR_PUBCODE_TOO_LONG = -5,    ///< Publisher code too long in the path
    CPAKFS_PARSE_ERR_PUBCODE_CHAR = -6,        ///< Invalid publisher code character in the path
    CPAKFS_PARSE_ERR_FILENAME_TOO_SHORT = -7,  ///< Filename too short (empty) in the path
    CPAKFS_PARSE_ERR_FILENAME_TOO_LONG = -8,   ///< Filename too long in the path
    CPAKFS_PARSE_ERR_FILENAME_CHAR = -9,       ///< Invalid filename character in the path
    CPAKFS_PARSE_ERR_EXTENSION_TOO_LONG = -10, ///< Extension too long in the path
    CPAKFS_PARSE_ERR_EXTENSION_CHAR = -11,     ///< Invalid extension character in the path
} cpakfs_parse_err_t;

/**
 * @brief Parse a cpakfs fullpath into its components
 * @preview
 * 
 * This function parses a full path in the format "GAMECODE.PU-filename.ext"
 * and extracts the game code, publisher code, filename, and extension into
 * the provided #cpakfs_path_t structure.
 * 
 * Parsing a path might fail if the input full path is not in the expected
 * format (eg: too long components), or if the filename or extension contains
 * unsupported characters. If @p error_pos is provided, it will be set to point
 * to the first character that caused the error.
 * 
 * @param utf8_fullname         Input full path to parse (UTF-8 string)
 * @param path                  Output path structure to populate
 * @param error_pos             Optional output parameter to indicate the position
 *                              of the first error in the input string, if parsing fails.
 * @result 0 on success, or a negative value indicating the type of error. See
 *         #cpakfs_parse_err_t for details.
 */
LIBDRAGON_PREVIEW_API
cpakfs_parse_err_t cpakfs_path_parse(const char *utf8_fullname, cpakfs_path_t *path, const char **error_pos);

/**
 * @brief Format a cpakfs path structure into a full path string
 * @preview
 * 
 * This function will always succeed, as it will find a way to format any
 * combination of game code, publisher code, filename, and extension, including
 * invalid byte sequences according to the expected codepages and rules, falling
 * back to hex encoding if necessary.
 * 
 * For instance, if the gamecode is not a valid ASCII string but a random sequence
 * of bytes like eg: `"\x04\x05\x06\x07"`, the function will format it as 
 * `"04050607.PU-filename.ext"`.
 * 
 * So it is guaranteed that any input path structure can be formatted into a valid
 * UTF-8 string as full name, and that the resulting string will always be
 * parsable back to the same path structure using #cpakfs_path_parse.
 *
 * @param path                  Input path structure to format
 * @param utf8_fullname         Output buffer to receive the formatted full path (UTF-8 string)
 * @param buflen                Size of the output buffer (including space for null terminator)
 * @return                      0 on success, -1 if the buffer was too small
 */
LIBDRAGON_PREVIEW_API
int cpakfs_path_format(const cpakfs_path_t *path, char *utf8_fullname, int buflen);


#ifdef __cplusplus
}
#endif

#endif