#ifndef __MINGW32__
// Enable usage of visibility attributes for GCC and Clang, except when using MinGW
#define HAVE_VISIBILITY
#endif

#include "libulc.h"

#include "libulc/libfourier/Fourier_ApplyAmplitude.c"
#include "libulc/libfourier/Fourier_ApplyWindow.c"
#include "libulc/libfourier/Fourier_CenterFFT.c"
#include "libulc/libfourier/Fourier_DCT2.c"
#include "libulc/libfourier/Fourier_DCT4.c"
#include "libulc/libfourier/Fourier_IMDCT.c"
#include "libulc/libfourier/Fourier_MDCT.c"

#include "libulc/ulcDecoder.c"
#include "libulc/ulcEncoder.c"
#include "libulc/ulcEncoder_BlockTransform.c"
#include "libulc/ulcEncoder_Encode.c"
#include "libulc/ulcEncoder_NoiseFill.c"
#include "libulc/ulcEncoder_Psyopt.c"
#include "libulc/ulcEncoder_WindowControl.c"
