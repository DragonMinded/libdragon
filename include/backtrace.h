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
 */

#ifndef __LIBDRAGON_BACKTRACE_H
#define __LIBDRAGON_BACKTRACE_H

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
 * @param buffer    Array of return addresses, populated by #backtrace
 * @param size      Size of the provided buffer, in number of pointers.
 * @return          Array of strings, one for each call frame. The array
 *                  must be freed by the caller with a single free() call.
 */
char** backtrace_symbols(void **buffer, int size);

#endif
