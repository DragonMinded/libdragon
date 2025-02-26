/**
 * @file dir.h
 * @brief Directory handling
 * @ingroup system
 */
#ifndef __LIBDRAGON_DIR_H
#define __LIBDRAGON_DIR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * @addtogroup system
 * @{
 */

/**
 * @name Directory entry type definitions
 * @{
 */
/** @brief Regular file */
#define DT_REG 1
/** @brief Directory */
#define DT_DIR 2
/** @} */

/**
 * @brief Directory entry structure
 */
typedef struct
{
    /** @brief The name of the directory entry (relative to the directory path) */
    char d_name[256];
    /** @brief The type of the directory entry.  See #DT_REG and #DT_DIR. */
    int d_type;
    /** @brief Size of the file.
     * 
     * This value is well defined for files. For directories, the value
     * is filesystem-dependent.
     * 
     * If negative, the filesystem does not report the size during directory
     * walking.
     */
    int64_t d_size;
    /** @brief Opaque cookie used to continue walking. */
    uint32_t d_cookie;
} dir_t;

/** @} */

/**
 * @brief Find the first file in a directory
 *
 * This function should be called to start enumerating a directory or whenever
 * a directory enumeration should be restarted.
 *
 * @param[in]  path
 *             Path to the directory structure
 * @param[out] dir
 *             Directory entry structure to populate with first entry
 *
 * @return 0 on successful lookup, -1 if the directory existed and is empty,
 *         or a different negative value on error (in which case, errno will be set).
 */
int dir_findfirst( const char * const path, dir_t *dir );

/**
 * @brief Find the next file in a directory
 *
 * After finding the first file in a directory using #dir_findfirst, call this to retrieve
 * the rest of the directory entries.  Call this repeatedly until a negative error is returned
 * signifying that there are no more directory entries in the directory.
 *
 * @param[in]  path
 *             Path to the directory structure
 * @param[out] dir
 *             Directory entry structure to populate with next entry
 *
 * @return 0 on successful lookup, -1 if there are no more files in the directory,
 *         or a different negative value on error (in which case, errno will be set).
 */
int dir_findnext( const char * const path, dir_t *dir );

/** @brief Supported return values for #dir_walk_callback_t */
enum {
    DIR_WALK_ABORT = -2,    ///< Abort walking and exit immediately
    DIR_WALK_ERROR = -1,    ///< Error walking the directory (errno will be set)
    DIR_WALK_CONTINUE = 0,  ///< Continue walking
    DIR_WALK_SKIPDIR = 1,   ///< Do not recurse within the current directory
    DIR_WALK_GOUP = 2,      ///< Stop walking the current directory, return up one level
};

/**
 * @brief Callback function for directory walking
 * 
 * This function is called for each file and each directory found during the directory walk.
 * 
 * The return value of this function determines the behavior of the directory walk. The possible
 * return values are:
 * 
 * - #DIR_WALK_CONTINUE: Continue walking the directory. This is the default expected return value.
 * - #DIR_WALK_SKIPDIR: Skip the current directory. If the current entry is a directory, the directory
 *   will not be recursed into.
 * - #DIR_WALK_GOUP: Stop walking the current directory and return up one level.
 * - #DIR_WALK_ABORT: Abort the directory walk immediately. #dir_walk will return -2, and errno
 *   will not be set.
 * - #DIR_WALK_ERROR: An error occurred while walking the directory. The callback is responsible
 *   for setting errno to the appropriate value. #dir_walk will return -1.
 * 
 * @param fn    The full absolute filename of the file or directory
 * @param dir   The directory entry structure with information about the file or directory
 * @param data  User data passed to #dir_walk
 * @return int  The return value determines the behavior of the directory walk (one of DIR_WALK constants)
 */
typedef int (*dir_walk_callback_t)(const char *fn, dir_t *dir, void *data);

/**
 * @brief Walk a directory tree
 * 
 * This function walks a directory tree, calling the callback for each file and directory found.
 * 
 * The callback is of type #dir_walk_callback_t, and its return value determines the behavior
 * of the walk. In fact, the callback can request to abort the walk (#DIR_WALK_ABORT), skip the
 * current directory (#DIR_WALK_SKIPDIR), or stop walking the current directory and return up one
 * level (#DIR_WALK_GOUP). See #dir_walk_callback_t for more information.
 * 
 * @param path      The path to the directory to walk
 * @param cb        The callback function to call for each file and directory
 * @param data      User data to pass to the callback
 * @return 0        on success
 * @return -1       on error (errno will be set)
 * @return -2       if abort was requested by the callback via DIR_WALK_ABORT
 */
int dir_walk(const char *path, dir_walk_callback_t cb, void *data);

///
/// @brief Check if a filename matches a pattern
/// 
/// This function is a simplified version of fnmatch that only supports the following
/// special characters:
/// 
///  * `?` - Matches any single character
///  * `*` - Matches any sequence of characters, except '/'. It can be used to match
///         multiple files in a single directory.
///  * `**` - Matches any sequence of characters, including '/'. It can be used to match
///         files within directory trees
///
/// Example of patterns:
///
/// @code
///   *.txt              - Matches all files with a .txt extension
///   **/*.txt           - Matches all files with a .txt extension in all directories
///                        under the starting directory
///   hero/**/*.sprite   - Matches all files with a .sprite extension in the
///                        hero directory and all its subdirectories
///   catalog?.dat       - Matches catalog1.dat, catalog2.dat, etc.
///   *w*/*.txt          - Matches all files with a .txt extension in directories
///                        that contain the letter 'w'
/// @endcode
///
/// @param pattern       The pattern to match against
/// @param fullpath      The full path to match
/// @return true         The filename matches the pattern
/// @return false        The filename does not match the pattern
///
bool dir_fnmatch(const char *pattern, const char *fullpath);

///
/// @brief Glob a directory tree using a pattern
/// 
/// This function walks a directory tree searching for files and directories that match a pattern.
/// The pattern is a simplified version of fnmatch; see #dir_fnmatch for more information
/// about the supported special characters.
/// 
/// The callback function is called for each file and directory that matches the pattern. The callback
/// can then decide how to proceed using its return value (see #dir_walk_callback_t for more information).
/// 
/// @code{.c}
///   int mycb(const char *fn, dir_t *dir, void *data) {
///      debugf("Found sprite file: %s\n", fn);
///      return DIR_WALK_CONTINUE;
///   }
/// 
///   // Search for all files with a .sprite extension in all directories under rom:/
///   dir_glob("**/*.sprite", "rom:/", mycb, NULL);
/// @endcode
/// 
/// @note the glob pattern is matched against pathnames **relative** to the starting directory.
///       For example, if you start the search at "rom:/sprites", the pattern "hero/*.sprite"
///       will match all files with a .sprite extension in the "rom:/sprites/hero" directory.
///
/// @param pattern       The pattern to match against (see #dir_fnmatch)
/// @param path          The path to the directory to start the search
/// @param cb            The callback function to call for each file and directory
/// @param data          User data to pass to the callback
/// @return 0 on success, 
/// @return -1 on error (errno will be set)
/// @return -2 if abort was requested by the callback via DIR_WALK_ABORT
///
int dir_glob(const char *pattern, const char *path, dir_walk_callback_t cb, void *data);


#ifdef __cplusplus
}
#endif

#endif
