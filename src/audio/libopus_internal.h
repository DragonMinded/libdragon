#ifndef LIBDRAGON_AUDIO_LIBOPUS_INTERNAL_H
#define LIBDRAGON_AUDIO_LIBOPUS_INTERNAL_H

// Activate fixed point mode on N64. Floating point works as well in its
// reference implementation, but our RSP-accelerated version requires
// fixed point.
#define FIXED_POINT

#include "opus/arch.h"
#include "opus/os_support.h"

// Activate RSP-specific optimizations.
#define RSP_IMDCT           0
#define RSP_COMB_FILTER     1
#define RSP_DEEMPHASIS      1

void rsp_opus_init(void);

void rsp_opus_comb_filter_const(opus_val32 *y, opus_val32 *x, int T, int N,
      opus_val16 g10, opus_val16 g11, opus_val16 g12, int arch);

void rsp_opus_comb_filter_dual(opus_val32 *y, opus_val32 *x, int T0, int T1, int N,
      opus_val16 g00, opus_val16 g01, opus_val16 g02,
      opus_val16 g10, opus_val16 g11, opus_val16 g12,
      const opus_val16 *window);

void rsp_opus_deemphasis(celt_sig *in[], opus_val16 *pcm, int N, int C, int downsample, const opus_val16 *coef,
      celt_sig *mem, int accum);


#endif
