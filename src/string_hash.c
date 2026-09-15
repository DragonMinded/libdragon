/**
 * @file string_hash.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Fast string hashing function with compile-time support
 */

#include "string_hash.h"

/** @brief Out of line string hash function. */
uint32_t __string_hash(const char *str)
{
    uint32_t hash = 5381; const uint32_t prime = 33;

    while (*str) {
        hash = hash * prime + (*str++);
    }
    return hash;
}
