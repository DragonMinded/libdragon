#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wtautological-compare"
#else
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wnonnull-compare"
#endif

// Activate support for CELT custom modes. We have pregenerated the two common
// custom modes in static_modes_fixed.h, but we are going to allow for runtime
// mode generation as well for now.
#define CUSTOM_MODES
//#define CUSTOM_MODES_ONLY          /* Disable runtime custom mode generation */

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

#ifdef __clang__
    #pragma clang diagnostic pop
#else
    #pragma GCC diagnostic pop
#endif
#undef MIN
#undef MAX