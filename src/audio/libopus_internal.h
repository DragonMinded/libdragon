/**
 * @file libopus_internal.h
 * @author Giovanni Bajo (giovannibajo@gmail.com)
 * @brief C-level glue for RSP ucode for Opus decoding
 */

#ifndef LIBDRAGON_AUDIO_LIBOPUS_INTERNAL_H
#define LIBDRAGON_AUDIO_LIBOPUS_INTERNAL_H

// Activate fixed point mode on N64. Floating point works as well in its
// reference implementation, but our RSP-accelerated version requires
// fixed point.
#define FIXED_POINT

// Activate support for CELT custom modes. We have pregenerated the two common
// custom modes in static_modes_fixed.h, but we are going to allow for runtime
// mode generation as well for now.
#define CUSTOM_MODES
//#define CUSTOM_MODES_ONLY          /* Disable runtime custom mode generation */

// Some config macros, since we don't run configure
#define USE_ALLOCA
#define HAVE_LRINTF
#define HAVE_LRINT

#include "opus/arch.h"
#include "opus/os_support.h"
#include "opus/opus_custom.h"
#include "opus/mdct.h"

#ifdef N64

// Activate RSP-specific optimizations.
#define RSP_IMDCT           1
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

void rsp_clt_mdct_backward(const mdct_lookup *l, kiss_fft_scalar *in, kiss_fft_scalar * OPUS_RESTRICT out,
      const opus_val16 * OPUS_RESTRICT window, int overlap, int shift, int stride, int B, int NB, int arch);

void rsp_opus_memmove(celt_sig *dst, celt_sig *src, opus_int32 len);
void rsp_opus_clear(celt_sig *dst, opus_int32 len);

#endif

#endif
