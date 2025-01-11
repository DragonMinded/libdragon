#include "libopus_internal.h"

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wtautological-compare"
#else
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wnonnull-compare"
    #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

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
