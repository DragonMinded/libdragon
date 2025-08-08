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
 * @note This function is just a stub and doesn't really work for now. Please
 *       use the old #validate_mempak for now.
 * 
 * This function checks the integrity of a controller pak. It is useful to
 * check if the controller pak is corrupted or if the filesystem is corrupted.
 * 
 * @param port          The controller pak to check the integrity of
 * @param fix_errors    Whether to fix the errors found
 * @return 0            if the integrity check was successful
 * @return negative     if an error occurred (eg: no cpak on the specified port),
 *                      and errno is set accordingly.
 */
int cpak_fsck(joypad_port_t port, bool fix_errors);

#ifdef __cplusplus
}
#endif

#endif