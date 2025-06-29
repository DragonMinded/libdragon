/*
    utils: utility functions for tools
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
#ifndef LIBDRAGON_TOOLS_UTILS_H
#define LIBDRAGON_TOOLS_UTILS_H

#include "../../src/utils.h"

#include <stdlib.h>
#include <string.h>

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
    n64_inst = strdup(n64_inst);
    int n = strlen(n64_inst);
    if (n64_inst[n-1] == '/' || n64_inst[n-1] == '\\')
        n64_inst[n-1] = 0;

    return n64_inst;
}

#endif
