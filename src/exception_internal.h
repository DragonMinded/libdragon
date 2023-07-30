#ifndef __LIBDRAGON_EXCEPTION_INTERNAL_H
#define __LIBDRAGON_EXCEPTION_INTERNAL_H

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include "exception.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const char *__mips_gpr[34];
extern const char *__mips_fpreg[32];

void __exception_dump_header(FILE *out, exception_t* ex);
void __exception_dump_gpr(exception_t* ex, void (*cb)(void *arg, const char *regname, char* value), void *arg);
void __exception_dump_fpr(exception_t* ex, void (*cb)(void *arg, const char *regname, char* hexvalue, char *singlevalue, char *doublevalue), void *arg);

__attribute__((noreturn))
void __inspector_exception(exception_t* ex);

__attribute__((noreturn))
void __inspector_assertion(const char *failedexpr, const char *msg, va_list args);

__attribute__((noreturn))
void __inspector_cppexception(const char *exctype, const char *what);

#ifdef __cplusplus
}
#endif

#endif
