#ifndef LIBDRAGON_TOOLS_UTILS_H
#define LIBDRAGON_TOOLS_UTILS_H

#include "../../src/utils.h"

#include <stdlib.h>
#include <string.h>

static char* path_remove_trailing_slash(char *path)
{
    path = strdup(path);
    int n = strlen(path);
    if (path[n-1] == '/' || path[n-1] == '\\')
        path[n-1] = 0;
    return path;
}

// Find the directory where the libdragon toolchain is installed.
// This is where you can find GCC, the linker, etc.
const char *n64_toolchain_dir(void)
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

// Find the directory where the libdragon tools are installed.
// This is where you can find mksprite, mkfont, etc.
const char *n64_tools_dir(void)
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
