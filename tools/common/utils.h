/*
    utils: utility functions for tools
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
#ifndef LIBDRAGON_TOOLS_UTILS_H
#define LIBDRAGON_TOOLS_UTILS_H

#include "polyfill.h"
#include "../../src/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

__attribute__((used))
static char* path_remove_trailing_slash(char *path)
{
    path = strdup(path);
    int n = strlen(path);
    if (path[n-1] == '/' || path[n-1] == '\\')
        path[n-1] = 0;
    return path;
}

__attribute__((used))
static char *change_ext(const char *fn, const char *ext)
{
    char *out = strdup(fn);
    char *dot = strrchr(out, '.');
    if (dot) *dot = 0;
    strcat(out, ext);
    return out;
}

__attribute__((used))
static bool file_exists(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (f) fclose(f);
    return f != NULL;
}

// Find the directory where the libdragon toolchain is installed.
// This is where you can find GCC, the linker, etc.
__attribute__((used))
static const char *n64_toolchain_dir(void)
{
    static char *n64_inst = NULL;
    if (n64_inst)
        return n64_inst;

    // Find the toolchain installation directory.
    // n64.mk supports having a separate installation for the toolchain and
    // libdragon. So first check if N64_GCCPREFIX is set; if so the toolchain
    // is there. Otherwise, fallback to N64_INST which is where we expect
    // the toolchain to reside.
    n64_inst = getenv("N64_GCCPREFIX");
    if (!n64_inst)
        n64_inst = getenv("N64_INST");
    if (!n64_inst)
        return NULL;

    // Remove the trailing backslash if any. On some system, running
    // popen with a path containing double backslashes will fail, so
    // we normalize it here.
    n64_inst = path_remove_trailing_slash(n64_inst);
    return n64_inst;
}

// Find the prefix to be prepended for GCC commands.
// This is everything before gcc, ld, objdump, etc
__attribute__((used))
static const char *n64_gccprefix_triplet(void)
{
    static char *n64_gccprefix_triplet = NULL;
    if (n64_gccprefix_triplet)
        return n64_gccprefix_triplet;

    const char *inst = n64_toolchain_dir();
    if (!inst)
        return NULL;
    const char *target = getenv("N64_TARGET");
    if (!target)
        target = "mips64-elf";

    int ret;
    if (target[0])
        ret = asprintf(&n64_gccprefix_triplet, "%s/bin/%s-", inst, target);
    else
        ret = asprintf(&n64_gccprefix_triplet, "%s/bin/", inst);

    if (ret < 0) {
        perror("asprintf");
        exit(1);
    }
    return n64_gccprefix_triplet;
}

// Find the directory where the libdragon tools are installed.
// This is where you can find mksprite, mkfont, etc.
__attribute__((used))
static const char *n64_tools_dir(void)
{
    static char *n64_inst = NULL;
    if (n64_inst)
        return n64_inst;

    // Find the tools installation directory.
    n64_inst = getenv("N64_INST");
    if (!n64_inst)
        return NULL;

    // Remove the trailing backslash if any. On some system, running
    // popen with a path containing double backslashes will fail, so
    // we normalize it here.
    n64_inst = path_remove_trailing_slash(n64_inst);
    return n64_inst;
}
#endif
