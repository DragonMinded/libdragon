/**
 * @file string_hash.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Fast string hashing function with compile-time support
 */
#ifndef LIBDRAGON_STRING_HASH_H
#define LIBDRAGON_STRING_HASH_H

#include <stdint.h>
#include "debug.h"

#ifdef __cplusplus
extern "C" {
#endif

///@cond
extern uint32_t __string_hash(const char *str);

__attribute__((always_inline))
inline uint32_t __string_hash_literal(const char *str)
{
    uint32_t hash = 5381; const uint32_t prime = 33;

#define NEXT_CHAR() \
    if (*str == '\0') return hash; \
    hash = hash * prime + (*str++);

    NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR();
    NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR();
    NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR();
    NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR();
    NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR();
    NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR();
    NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR();
    NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR(); NEXT_CHAR();

    assertf(0, "String too long for hash_literal_string");
}

///@endcond

/**
 * @brief Hash a string at compile-time or run-time.
 *
 * This function allows for a quick hash of a string, either at compile-time
 * (for literals) or at run-time (for variables). It is using the djb2 hash.
 * 
 * @hideinitializer
 */
#define string_hash(s)  (__builtin_constant_p(s) ? __string_hash_literal(s) : __string_hash(s))

#ifdef __cplusplus
}
#endif

#endif // LIBDRAGON_STRING_HASH_H
