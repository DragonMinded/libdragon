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
 * @brief 
 * 
 * @param port 
 * @return int 
 */
int cpak_fsck(joypad_port_t port, bool fix_errors);


#ifdef __cplusplus
}
#endif

#endif