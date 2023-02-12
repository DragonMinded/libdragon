/**
 * @file backtrace.h
 * @brief Backtrace (call stack) support
 * @ingroup backtrace
 */

/**
 * @defgroup backtrace Backtrace (call stack) support
 * @ingroup lowlevel
 * @brief Implementation of functions to walk the stack and dump a backtrace
 * 
 * This module implements two POSIX/GNU standard functions to help walking
 * the stack and providing the current execution context: backtrace() and
 * backtrace_symbols().
 * 
 * The functions have an API fully compatible with the standard ones. The
 * implementation is however optimized for the MIPS/N64 case, and with
 * standard compilation settings. See the documentation in backtrace.c
 * for implementation details.
 * 
 * You can call the functions to inspect the current call stack. For
 * a higher level function that just prints the current call stack
 * on the debug channels, see #debug_backtrace.
 * 
 * @{
 */

#ifndef __LIBDRAGON_BACKTRACE_H
#define __LIBDRAGON_BACKTRACE_H

#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * @brief A stack frame, part of a backtrace
 */
typedef struct {
    uint32_t addr;              ///< PC address of the frame (MIPS virtual address)

    const char *func;           ///< Name of the function (this should always be present)
    uint32_t func_offset;       ///< Byte offset of the address within the function

    const char *source_file;    ///< Name of the source file (if known, or "???" otherwise)
    int source_line;            ///< Line number in the source file (if known, or 0 otherwise)

    bool is_inline;             ///< True if this frame refers to an inlined function
} backtrace_frame_t;


/**
 * @brief Print a single frame of a backtrace
 * 
 * Print all the information about a single frame of a backtrace, with
 * the following format:
 * 
 * ```
 *    <func>+<offset> (<source_file>:<source_line>) [<address>]
 * ```
 * 
 * for instance:
 * 
 * ```
 *    debug_assert_func_f+0x9c (/home/user/src/libdragon/src/debug.c:537) [0x80010c5c]
 * ```
 * 
 * @param out       File to print to
 * @param frame     Frame to print
 */
void backtrace_frame_print(backtrace_frame_t *frame, FILE *out);

/**
 * @brief Print a single frame of a backtrace, in a compact format
 * 
 * Print a frame of a backtrace in a compact format, with a limited width in number
 * of characters. This is the format:
 * 
 * ```
 *    <func> (<source_file>:<source_line>)
 * ```
 * 
 * but the source file will be truncated to fit the width, showing only its final
 * part. For instance, if the width is 40 characters, the following frame:
 * 
 * ```
 *   debug_assert_func_f+0x9c (/home/user/src/libdragon/src/debug.c:537) [0x80010c5c]
 * ```
 * 
 * will be printed as:
 * 
 * ```
 *    debug_assert_func_f (.../src/debug.c:537)
 * ```
 * 
 * @param out       File to print to
 * @param frame     Frame to print
 * @param width     Width in characters to fit the frame information to
 */
void backtrace_frame_print_compact(backtrace_frame_t *frame, FILE *out, int width);

/**
 * @brief Walk the stack and return the current call stack
 * 
 * This function will analyze the current execution context,
 * walking the stack and returning informations on the active
 * call frames.
 * 
 * This function adheres to POSIX specification. It does not
 * allocate memory so it is safe to be called even in the
 * context of low memory conditions or possibly corrupted heap.
 * 
 * If called within an interrupt or exception handler, the function
 * is able to correctly walk backward the interrupt handler and
 * show the context even before the exception was triggered.
 * 
 * @param buffer    Empty array of pointers. This will be populated with pointers
 *                  to the return addresses for each call frame.
 * @param size      Size of the buffer, that is, maximum number of call frames
 *                  that will be walked by the function.
 * @return          Number of call frames walked (at most, size).
 */
int backtrace(void **buffer, int size);

/**
 * @brief Translate the buffer returned by #backtrace into a list of strings
 * 
 * This function symbolizes the buffer returned by #backtrace, translating
 * return addresses into function names and source code locations.
 * 
 * The user-readable strings are allocated on the heap and must be freed by
 * the caller (via a single free() call). There is no need to free each
 * of the returned strings: a single free() call is enough, as they are
 * allocated in a single contiguous block.
 * 
 * This function adheres to POSIX specification.
 * 
 * This function also handles inlined functions. In general, inlined function
 * do not have a real stack frame because they are expanded in place; so for
 * instance a single stack frame (as returned by #backtrace) can correspond
 * to multiple symbolized stack frames, one per each inlined function. Since
 * the POSIX API requires this function to return an array of the same size
 * of the input array, all inlined functions are collapsed into a single
 * string, separated by newlines.
 * 
 * @param buffer    Array of return addresses, populated by #backtrace
 * @param size      Size of the provided buffer, in number of pointers.
 * @return          Array of strings, one for each call frame. The array
 *                  must be freed by the caller with a single free() call.
 * 
 * @see #backtrace_symbols_cb
 */
char** backtrace_symbols(void **buffer, int size);

/**
 * @brief Symbolize the buffer returned by #backtrace, calling a callback for each frame
 * 
 * This function is similar to #backtrace_symbols, but instead of formatting strings
 * into a heap-allocated buffer, it invokes a callback for each symbolized stack
 * frame. This allows to skip the memory allocation if not required, and also allows
 * for custom processing / formatting of the backtrace by the caller.
 * 
 * The callback will receive an opaque argument (cb_arg) and a pointer to a
 * stack frame descriptor (#backtrace_frame_t). The descriptor and all its
 * contents (including strings) is valid only for the duration of the call,
 * so the callback must (deep-)copy any data it needs to keep.
 * 
 * The callback implementation might find useful to call #backtrace_frame_print
 * or #backtrace_frame_print_compact to print the frame information.
 * 
 * @param buffer    Array of return addresses, populated by #backtrace
 * @param size      Size of the provided buffer, in number of pointers.
 * @param flags     Flags to control the symbolization process. Use 0.
 * @param cb        Callback function to invoke for each symbolized frame
 * @param cb_arg    Opaque argument to pass to the callback function
 * @return True if the symbolization was successful, false otherwise.
 *         Notice that the function returns true even if some frames
 *         were not symbolized; false is only used when the function
 *         had to abort before even calling the callback once (eg:
 *         no symbol table was found).
 * 
 * @see #backtrace_symbols
 */
bool backtrace_symbols_cb(void **buffer, int size, uint32_t flags,
    void (*cb)(void *, backtrace_frame_t*), void *cb_arg);

#ifdef __cplusplus
}
#endif

/** @} */

#endif
