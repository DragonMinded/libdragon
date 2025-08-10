/**
 * @file cpak.h
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
 * Call #cpak_mount to mount the controller pak associated to a certain
 * joypad port, and then use standard C functions to access the notes as
 * files in the mounted filesystem. The filesystem has a virtual structure
 * that is similar to a directory tree: the root contains only subdirectories,
 * named after the game code and publisher code.
 * Then within each subdirectory, you can find the notes as files.
 * 
 * For instance, to read the note for the game "Doubutsu no Mori" (Animal Crossing),
 * you can do the following:
 * 
 * \code{.c}
 *     if (cpak_mount(JOYPAD_PORT_1, "cpak1:/") < 0) {
 *        // handle errors, by inspecting errno [...]
 *       return;
 *     }
 * 
 *     // Read the following note:
 *     //   Game code: NAFJ
 *     //   Publisher code: 01
 *     //   Filename: DOUBUTSUNOMORI
 *     //   File extension: A
 *     FILE *f = fopen("cpak1:/NAFJ.01/DOUBUTSUNOMORI.A", "rb");
 * \endcode
 * 
 * To iterate over the notes in a controller pak, you can use the directory
 * functions defined in dir.h (eg: #dir_findfirst, #dir_walk, #dir_glob).
 * 
 * To write a note, create a file in the mounted filesystem using fopen,
 * and then write the data to it using fwrite.
 * 
 * To unmount the controller pak filesystem, call #cpak_unmount.
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
 *    it was corrupted. Use #cpak_fsck to try recovering the contents. Also
 *    returned when no controller pak is present on the specified port.
 *  * ENOSPC: No space left on the controller pak to allocate new pages for
 *    writing to a file.
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
#ifndef LIBDRAGON_CPAK_H
#define LIBDRAGON_CPAK_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "joypad.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Usage statistics for a Controller Pak
 * 
 * This structure is returned by #cpak_get_stats and contains the usage
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
} cpak_stats_t;

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
    CPAKFS_ISSUE_FAT_INVALID_RESERVED = 6,          ///< Reserved page has invalid FAT entry
    CPAKFS_ISSUE_FAT_INVALID_ENTRY = 7,             ///< Invalid FAT entry

    CPAKFS_ISSUE_NOTE_INVALID_GAMECODE = 8,         ///< Note has invalid gamecode
    CPAKFS_ISSUE_NOTE_INVALID_PUBCODE = 9,          ///< Note has invalid publisher code
} cpakfs_issue_t;

/** @brief Cpak filesystem report callback */
typedef void (*cpakfs_report_fn)(cpakfs_issue_t issue, cpakfs_issue_level_t level, const char *fmt, ...);


/**
 * @brief Mount the controller pak as filesystem
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
 *    it was corrupted. Use #cpak_fsck to try recovering the contents.
 * 
 * @param port              Cpak to mount, identified by the joypad port
 * @param prefix            Filesystem prefix to use for mounting. Suggested
 *                          name is "cpakN:/" where "N" is the controller
 *                          port (1..4).
 * @return 0 if success, negative value in case of error (and errno is set)
 */
int cpak_mount(joypad_port_t port, const char *prefix);

/**
 * @brief Unmount the controller pak filesystem
 * 
 * This function unmounts the controller pak filesystem, waiting for all
 * pending operations to complete.
 * 
 * @param port              The controller pak to unmount
 * @return 0 if success, negative value in case of error (and errno is set)
 */
int cpak_unmount(joypad_port_t port);

/**
 * @brief Read the serial number of a controller pak
 * 
 * This function reads the 20-byte serial number of a controller pak.
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
int cpak_get_serial(joypad_port_t port, uint8_t serial[24]);

/**
 * @brief Read the usage state of a controller pak
 * 
 * @param port          The controller pak to read the usage state from
 * @param stats         The structure where to store the usage statistics
 * @return 0            if the serial was successfully read
 * @return negative     if an error occurred (eg: no cpak on the specified port),
 *                      and errno is set accordingly.
 */
int cpak_get_stats(joypad_port_t port, cpak_stats_t *stats);

/**
 * @brief Check the integrity of a controller pak
 * 
 * This function checks the integrity of a controller pak filesystem, and
 * optionally tries also to repair it if unreadable, and recover as much as
 * possible.
 * 
 * Found issues can be reported via the report callback, which is called
 * for each issue found.
 * 
 * @param port          The controller pak to check the integrity of
 * @param fix_errors    Whether to fix the errors found
 * @param report        Callback to report issues found during the check
 * @return 0            if the integrity check was successful
 * @return negative     if an error occurred (eg: no cpak on the specified port),
 *                      and errno is set accordingly.
 */
int cpak_fsck(joypad_port_t port, bool fix_errors, cpakfs_report_fn report);

/**
 * @brief Format a controller pak
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
 * @return int      0 on success, negative on error
 */
int cpak_format(joypad_port_t port, bool erase);

#ifdef __cplusplus
}
#endif

#endif