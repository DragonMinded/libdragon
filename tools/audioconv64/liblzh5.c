#include <assert.h>
#define assertf(x, ...) assert(x)
#ifndef assertf
#define assertf(x, ...) assert(x)
#endif
#define memalign(a, b) malloc(b)

#include "../../src/compress/lzh5.c"
#include "lzh5_compress.c"
