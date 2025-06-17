#ifndef __MINGW32__
// Enable usage of visibility attributes for GCC and Clang, except when using MinGW
#define HAVE_VISIBILITY
#endif

#include "libsamplerate.h"

#include "libsamplerate/samplerate.c"
#include "libsamplerate/src_sinc.c"
#include "libsamplerate/src_zoh.c"
#include "libsamplerate/src_linear.c"

