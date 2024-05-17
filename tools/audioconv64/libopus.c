
#pragma GCC diagnostic push

// Compiling libopus seems to trigger these warnings that we can safely ignore.
#pragma GCC diagnostic ignored "-Wtautological-pointer-compare"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

// Activate support for CELT custom modes. We are going to always use the
// custom mode API even for standard modes (like 48 Khz), so we can safely
// define CUSTOM_MODES_ONLY.
#define CUSTOM_MODES
#define CUSTOM_MODES_ONLY

// Some config macros, since we don't run configure
#define USE_ALLOCA
#define HAVE_LRINTF
#define HAVE_LRINT

// Bring opus in
#include "opus/opus_custom.h"
#include "opus/celt_encoder.c"
#include "opus/celt_decoder.c"
#include "opus/bands.c"
#include "opus/celt.c"
#include "opus/cwrs.c"
#include "opus/celt_lpc.c"
#include "opus/laplace.c"
#include "opus/entcode.c"
#include "opus/entenc.c"
#include "opus/entdec.c"
#include "opus/pitch.c"
#include "opus/rate.c"
#include "opus/mathops.c"
#include "opus/modes.c"
#include "opus/mdct.c"
#include "opus/kiss_fft.c"
#include "opus/vq.c"
#include "opus/quant_bands.c"

#pragma GCC diagnostic pop
#undef MIN
#undef MAX
