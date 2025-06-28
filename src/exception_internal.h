/**
 * @file exception_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef __LIBDRAGON_EXCEPTION_INTERNAL_H
#define __LIBDRAGON_EXCEPTION_INTERNAL_H

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include "exception.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Names of the 34 MIPS general-purpose registers.
 */
extern const char *__mips_gpr[34];
/**
 * @brief Names of the 32 MIPS floating-point registers.
 */
extern const char *__mips_fpreg[32];

void __exception_dump_header(FILE *out, exception_t* ex);
void __exception_dump_gpr(exception_t* ex, void (*cb)(void *arg, const char *regname, char* value), void *arg);
void __exception_dump_fpr(exception_t* ex, void (*cb)(void *arg, const char *regname, char* hexvalue, char *singlevalue, char *doublevalue), void *arg);

/**
 * @brief Trigger the inspector for an exception.
 *
 * @param ex Pointer to the exception structure.
 */
__attribute__((noreturn))
void __inspector_exception(exception_t* ex);

/**
 * @brief Trigger the inspector for an assertion failure.
 *
 * @param failedexpr The failed expression string.
 * @param msg The assertion message.
 * @param args Variable argument list for the message.
 */
__attribute__((noreturn))
void __inspector_assertion(const char *failedexpr, const char *msg, va_list args);

/**
 * @brief Trigger the inspector for a C++ exception.
 *
 * @param exctype The exception type string.
 * @param what The exception message.
 */
__attribute__((noreturn))
void __inspector_cppexception(const char *exctype, const char *what);

#ifdef __cplusplus
}
#endif

#endif
