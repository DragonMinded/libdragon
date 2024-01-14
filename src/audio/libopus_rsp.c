/**
 * @file libopus_rsp.c
 * @author Giovanni Bajo (giovannibajo@gmail.com)
 * @brief C-level glue for RSP ucode for Opus decoding
 */

#include "libopus_internal.h"
#include "rspq.h"
#include "debug.h"
#include "../utils.h"
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

DEFINE_RSP_UCODE(rsp_opus_dsp);         ///< RSP ucode for DSP functions
DEFINE_RSP_UCODE(rsp_opus_imdct);       ///< RSP ucode for IMDCT

static void fft_init(void);

/** @brief Initialize Opus RSP acceleration */
void rsp_opus_init(void)
{
    static bool init = false;
    if (!init) {
        rspq_init();
        rspq_overlay_register_static(&rsp_opus_dsp, 0x8<<28);
        rspq_overlay_register_static(&rsp_opus_imdct, 0x9<<28);
        fft_init();
        init = true;
    }
}

static void rsp_cmd_deemphasis(int32_t *inch0, int32_t *inch1, int16_t *out, int32_t state[2], int nn, int downsample)
{
    rspq_write(0x8<<28, 0x0, 
        PhysicalAddr(inch0),
        PhysicalAddr(inch1) | (downsample<<24),
        PhysicalAddr(out)   | ((nn/4-1) << 24),
        PhysicalAddr(state)
    );
}

static void rsp_cmd_comb_fetch(opus_val32 *x, int dmem_idx, int nsamples)
{
   rspq_write(0x8<<28, 0x1,
      PhysicalAddr(x), 
      (dmem_idx<<16) | nsamples);
}

static void rsp_cmd_comb_single(int nsamples, int i_idx, int t0_idx,
    int16_t g10, int16_t g11, int16_t g12)
{
    rspq_write(0x8<<28, 0x2,
        (nsamples/8-1)<<8 | (i_idx/4)<<0,
        (uint16_t)g10 | ((uint16_t)g11 << 16),
        ((uint16_t)g12),
        (t0_idx << 16));
}

static void rsp_cmd_comb_dual(int nsamples, int i_idx, int t0_idx, int t1_idx, 
    int16_t g00, int16_t g01, int16_t g02,
    int16_t g10, int16_t g11, int16_t g12)
{
   rspq_write(0x8<<28, 0x4,
      (nsamples/8-1)<<8 | (i_idx/4)<<0,
      (uint16_t)g00 | ((uint16_t)g01 << 16),
      ((uint16_t)g02) | ((uint16_t)g10 << 16),
      (t0_idx << 16) | (t1_idx),
      ((uint16_t)g11) | ((uint16_t)g12 << 16));
}

static void rsp_cmd_comb_result(opus_val32 *x, int i_idx, int nsamples)
{
   rspq_write(0x8<<28, 0x3,
      PhysicalAddr(x),
      (i_idx<<16) | nsamples);
}

static void rsp_cmd_memmove(int32_t *dst, int32_t *src, int nsamples)
{
   rspq_write(0x9<<28, 0x2,
      PhysicalAddr(dst),
      PhysicalAddr(src),
      nsamples * sizeof(int32_t));
}

static void rsp_cmd_clear(int32_t *dst, int nsamples)
{
   rspq_write(0x9<<28, 0x3,
      PhysicalAddr(dst),
      nsamples * sizeof(int32_t));
}

/*******************************************************************************
 * Memmove
 *******************************************************************************/

/** @brief Do a memmove with RSP to move back samples in the output buffer */
void rsp_opus_memmove(celt_sig *dst, celt_sig *src, opus_int32 len) {
    rsp_cmd_memmove(dst, src, len);
    rspq_flush();
}

/** @brief Clear output buffer with RSP */
void rsp_opus_clear(celt_sig *dst, opus_int32 len) {
    rsp_cmd_clear(dst, len);
    rspq_flush();
}

/*******************************************************************************
 * IMDCT (and FFT)
 *******************************************************************************/

DEFINE_RSP_UCODE(rsp_opus_fft_prerot);      ///< RSP ucode for IMDCT pre-rotation
DEFINE_RSP_UCODE(rsp_opus_fft_bfly2);       ///< RSP ucode for FFT butterfly 2
DEFINE_RSP_UCODE(rsp_opus_fft_bfly3);       ///< RSP ucode for FFT butterfly 3
DEFINE_RSP_UCODE(rsp_opus_fft_bfly4);       ///< RSP ucode for FFT butterfly 4
DEFINE_RSP_UCODE(rsp_opus_fft_bfly4m1);     ///< RSP ucode for FFT butterfly 4 (multiplicity 1)
DEFINE_RSP_UCODE(rsp_opus_fft_bfly5);       ///< RSP ucode for FFT butterfly 5
DEFINE_RSP_UCODE(rsp_opus_fft_postrot);     ///< RSP ucode for IMDCT post-rotation

/// @brief Description of a single pass of the FFT
typedef struct {
    uint16_t consts[8][8];      ///< Up to 8 vector constants
    uint32_t next_pass_rdram;   ///< Pointer to next pass in RDRAM (or 0 if it's the last)
    uint32_t func_rdram;        ///< Address of the butterfly function in RDRAM (overlay)
    uint32_t stride, m, n, mm;  ///< Parameters for the butterfly function
} opus_fft_pass_t;

/// @cond
#define __KF_ANGLE16_COS(i, N)             (((i) * (65536-1) / (N)) & 0xFFFF)
#define __KF_ANGLE16_SIN(i, N)             ((__KF_ANGLE16_COS(i, N) + 0x4000) & 0xFFFF)
#define __KF_BFLY_FSTRIDE_CPX(stride, N)   __KF_ANGLE16_COS(stride, N), __KF_ANGLE16_SIN(stride, N)

#define KF_BFLY2_CONST1  \
          { 0x7fff, 0x0000, 0x5a82, 0xa57e, 0x0000, 0x8000, 0xa57e, 0xa57e }
#define KF_BFLY2_CONST2  \
          { 0x0000, 0x7fff, 0x5a82, 0x5a82, 0x7fff, 0x0000, 0x5a82, 0xa57e }

#define KF_BFLY3_TWIDDLE1(stride, N)  \
          { __KF_BFLY_FSTRIDE_CPX(stride*0, N), \
            __KF_BFLY_FSTRIDE_CPX(stride*1, N), \
            __KF_BFLY_FSTRIDE_CPX(stride*2, N), \
            __KF_BFLY_FSTRIDE_CPX(stride*3, N) }

#define KF_BFLY3_TWIDDLE2(stride, N)  \
          { __KF_BFLY_FSTRIDE_CPX(stride*0, N), \
            __KF_BFLY_FSTRIDE_CPX(stride*2, N), \
            __KF_BFLY_FSTRIDE_CPX(stride*4, N), \
            __KF_BFLY_FSTRIDE_CPX(stride*6, N) }

#define KF_BFLY3_TWINCR1(stride, N)  \
          { __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_COS(stride*4, N), \
            __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_COS(stride*4, N), \
            __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_COS(stride*4, N), \
            __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_COS(stride*4, N) }

#define KF_BFLY3_TWINCR2(stride, N)  \
          { __KF_ANGLE16_COS(stride*8, N), __KF_ANGLE16_COS(stride*8, N), \
            __KF_ANGLE16_COS(stride*8, N), __KF_ANGLE16_COS(stride*8, N), \
            __KF_ANGLE16_COS(stride*8, N), __KF_ANGLE16_COS(stride*8, N), \
            __KF_ANGLE16_COS(stride*8, N), __KF_ANGLE16_COS(stride*8, N) }

#define KF_BFLY4_TWIDDLE1(stride, N)  \
          { 0, 0, 0, 0, \
            __KF_BFLY_FSTRIDE_CPX(stride*0, N), \
            __KF_BFLY_FSTRIDE_CPX(stride*1, N) }
#define KF_BFLY4_TWIDDLE2(stride, N)  \
          { __KF_BFLY_FSTRIDE_CPX(stride*0, N), \
            __KF_BFLY_FSTRIDE_CPX(stride*2, N), \
            __KF_BFLY_FSTRIDE_CPX(stride*0, N), \
            __KF_BFLY_FSTRIDE_CPX(stride*3, N) }
#define KF_BFLY4_TWINCR1(stride, N)  \
          { 0, 0, 0, 0, \
            __KF_ANGLE16_COS(stride*2, N), __KF_ANGLE16_COS(stride*2, N), \
            __KF_ANGLE16_COS(stride*2, N), __KF_ANGLE16_COS(stride*2, N) }
#define KF_BFLY4_TWINCR2(stride, N)  \
          { __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_COS(stride*4, N), \
            __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_COS(stride*4, N), \
            __KF_ANGLE16_COS(stride*6, N), __KF_ANGLE16_COS(stride*6, N), \
            __KF_ANGLE16_COS(stride*6, N), __KF_ANGLE16_COS(stride*6, N) }

#define KF_BFLY5_TWIDDLE1(stride, N)  \
          {   __KF_ANGLE16_COS(stride*0, N), __KF_ANGLE16_SIN(stride*0, N), \
            __KF_ANGLE16_COS(stride*1, N), __KF_ANGLE16_SIN(stride*1, N), \
            __KF_ANGLE16_SIN(stride*0, N), __KF_ANGLE16_COS(stride*0, N), \
            __KF_ANGLE16_SIN(stride*2, N), __KF_ANGLE16_COS(stride*2, N) }

#define KF_BFLY5_TWIDDLE2(stride, N)  \
          {   __KF_ANGLE16_COS(stride*0, N), __KF_ANGLE16_SIN(stride*0, N), \
            __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_SIN(stride*4, N), \
            __KF_ANGLE16_SIN(stride*0, N), __KF_ANGLE16_COS(stride*0, N), \
            __KF_ANGLE16_SIN(stride*3, N), __KF_ANGLE16_COS(stride*3, N) }

#define KF_BFLY5_TWINCR1(stride, N)  \
          { __KF_ANGLE16_COS(stride*2, N), __KF_ANGLE16_COS(stride*2, N), \
            __KF_ANGLE16_COS(stride*2, N), __KF_ANGLE16_COS(stride*2, N), \
            __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_COS(stride*4, N), \
            __KF_ANGLE16_COS(stride*4, N), __KF_ANGLE16_COS(stride*4, N) }

#define KF_BFLY5_TWINCR2(stride, N)  \
          { __KF_ANGLE16_COS(stride*8, N), __KF_ANGLE16_COS(stride*8, N), \
            __KF_ANGLE16_COS(stride*8, N), __KF_ANGLE16_COS(stride*8, N), \
            __KF_ANGLE16_COS(stride*6, N), __KF_ANGLE16_COS(stride*6, N), \
            __KF_ANGLE16_COS(stride*6, N), __KF_ANGLE16_COS(stride*6, N) }

#define KF_BFLY5_YAR  (10126)
#define KF_BFLY5_YAI (-31164)
#define KF_BFLY5_YBR (-26510)
#define KF_BFLY5_YBI (-19261)

#define KF_BFLY5_CONST1 \
          { KF_BFLY5_YAR, KF_BFLY5_YBR, KF_BFLY5_YAR, KF_BFLY5_YBR, \
            KF_BFLY5_YAR, KF_BFLY5_YBR, KF_BFLY5_YAR, KF_BFLY5_YBR }
#define KF_BFLY5_CONST2 \
          { KF_BFLY5_YBR, KF_BFLY5_YAR, KF_BFLY5_YBR, KF_BFLY5_YAR, \
            KF_BFLY5_YBR, KF_BFLY5_YAR, KF_BFLY5_YBR, KF_BFLY5_YAR }
#define KF_BFLY5_CONST3 \
          { KF_BFLY5_YAI, -KF_BFLY5_YBI, KF_BFLY5_YAI, -KF_BFLY5_YBI, \
            -KF_BFLY5_YAI, -KF_BFLY5_YBI, -KF_BFLY5_YAI, -KF_BFLY5_YBI }
#define KF_BFLY5_CONST4 \
          { KF_BFLY5_YBI, KF_BFLY5_YAI, KF_BFLY5_YBI, KF_BFLY5_YAI, \
            KF_BFLY5_YBI, -KF_BFLY5_YAI, KF_BFLY5_YBI, -KF_BFLY5_YAI }


#define KF_BFLY2(stride, N) \
    .func_rdram = PhysicalAddr(rsp_opus_fft_bfly2.code), \
    .consts = { \
        KF_BFLY2_CONST1, \
        KF_BFLY2_CONST2, \
    }

#define KF_BFLY3(stride, N) \
    .func_rdram = PhysicalAddr(rsp_opus_fft_bfly3.code), \
    .consts = { \
        KF_BFLY3_TWIDDLE1(stride, N), \
        KF_BFLY3_TWIDDLE2(stride, N), \
        KF_BFLY3_TWINCR1(stride, N), \
        KF_BFLY3_TWINCR2(stride, N), \
    }

#define KF_BFLY4M1(stride, N)  \
    .func_rdram = PhysicalAddr(rsp_opus_fft_bfly4m1.code), \
    .consts = {}

#define KF_BFLY4(stride, N) \
    .func_rdram = PhysicalAddr(rsp_opus_fft_bfly4.code), \
    .consts = { \
        KF_BFLY4_TWIDDLE1(stride, N), \
        KF_BFLY4_TWIDDLE2(stride, N), \
        KF_BFLY4_TWINCR1(stride, N), \
        KF_BFLY4_TWINCR2(stride, N), \
    }

#define KF_BFLY5(stride, N) \
    .func_rdram = PhysicalAddr(rsp_opus_fft_bfly5.code), \
    .consts = { \
        KF_BFLY5_TWIDDLE1(stride, N), \
        KF_BFLY5_TWIDDLE2(stride, N), \
        KF_BFLY5_TWINCR1(stride, N), \
        KF_BFLY5_TWINCR2(stride, N), \
        KF_BFLY5_CONST1, \
        KF_BFLY5_CONST2, \
        KF_BFLY5_CONST3, \
        KF_BFLY5_CONST4, \
    }

#define KF_POSTROT_TWIDDLE1(N)  \
          { __KF_BFLY_FSTRIDE_CPX(0, N), \
            __KF_BFLY_FSTRIDE_CPX(1, N), \
            __KF_BFLY_FSTRIDE_CPX(2, N), \
            __KF_BFLY_FSTRIDE_CPX(3, N) }

#define KF_POSTROT_TWIDDLE2(N)  \
          { __KF_BFLY_FSTRIDE_CPX(N/4-1, N), \
            __KF_BFLY_FSTRIDE_CPX(N/4-2, N), \
            __KF_BFLY_FSTRIDE_CPX(N/4-3, N), \
            __KF_BFLY_FSTRIDE_CPX(N/4-4, N) }

#define KF_POSTROT_TWINCR1(N)  \
          { __KF_ANGLE16_COS(4, N), __KF_ANGLE16_COS(4, N), \
            __KF_ANGLE16_COS(4, N), __KF_ANGLE16_COS(4, N), \
            __KF_ANGLE16_COS(4, N), __KF_ANGLE16_COS(4, N), \
            __KF_ANGLE16_COS(4, N), __KF_ANGLE16_COS(4, N) }

#define KF_POSTROT_TWINCR2(N)  \
          { -__KF_ANGLE16_COS(4, N), -__KF_ANGLE16_COS(4, N), \
            -__KF_ANGLE16_COS(4, N), -__KF_ANGLE16_COS(4, N), \
            -__KF_ANGLE16_COS(4, N), -__KF_ANGLE16_COS(4, N), \
            -__KF_ANGLE16_COS(4, N), -__KF_ANGLE16_COS(4, N) }

#define KF_PREROT_TWIDDLE_480() \
    { 0x0009, 0x004d, 0x0091, 0x00d5, 0x011a, 0x015e, 0x01a2, 0x01e6 }
#define KF_PREROT_TWINCR_480() \
    { 0x222 }

#define KF_PREROT_TWIDDLE_60() \
    { 0x0044, 0x0266, 0x0489, 0x06ab, 0x08cd, 0x0aef, 0x0d11, 0x0f33 }
#define KF_PREROT_TWINCR_60() \
    { 0x1111 }
/// @endcond

static opus_fft_pass_t fft_60[5];
static opus_fft_pass_t fft_480[7];

static void fft_init(void) {
    const int MAX_FFT_OVERLAY_SIZE = 0x400;

    assert(rsp_opus_fft_bfly2.code_end   - (void*)rsp_opus_fft_bfly2.code   <= MAX_FFT_OVERLAY_SIZE);
    assert(rsp_opus_fft_bfly3.code_end   - (void*)rsp_opus_fft_bfly3.code   <= MAX_FFT_OVERLAY_SIZE);
    assert(rsp_opus_fft_bfly4m1.code_end - (void*)rsp_opus_fft_bfly4m1.code <= MAX_FFT_OVERLAY_SIZE);
    assert(rsp_opus_fft_bfly5.code_end   - (void*)rsp_opus_fft_bfly5.code   <= MAX_FFT_OVERLAY_SIZE);
    assert(rsp_opus_fft_postrot.code_end - (void*)rsp_opus_fft_postrot.code <= MAX_FFT_OVERLAY_SIZE);

    fft_60[0] = (opus_fft_pass_t){
        .consts = {
            KF_PREROT_TWIDDLE_60(),
            KF_PREROT_TWINCR_60(),
        },
        .func_rdram = PhysicalAddr(rsp_opus_fft_prerot.code),
        .next_pass_rdram = PhysicalAddr(&fft_60[1]),
    };
    fft_60[1] = (opus_fft_pass_t){
        KF_BFLY4M1(120, 480),
        .stride = 120, .m = 1, .n = 15, .mm = 4,
        .next_pass_rdram = PhysicalAddr(&fft_60[2]),
    };
    fft_60[2] = (opus_fft_pass_t){
        KF_BFLY3(40, 480),
        .stride = 40, .m = 4, .n = 5, .mm = 12,
        .next_pass_rdram = PhysicalAddr(&fft_60[3]),
    };
    fft_60[3] = (opus_fft_pass_t){
        KF_BFLY5(8, 480),
        .stride = 8, .m = 12, .n = 1, .mm = 1,
        .next_pass_rdram = PhysicalAddr(&fft_60[4]),
    };
    fft_60[4] = (opus_fft_pass_t){
        .consts = {
            KF_POSTROT_TWIDDLE1(240),
            KF_POSTROT_TWIDDLE2(240),
            KF_POSTROT_TWINCR1(240),
            KF_POSTROT_TWINCR2(240),
        },
        .func_rdram = PhysicalAddr(rsp_opus_fft_postrot.code),
        .n = 120,
        .next_pass_rdram = 0,
    };

    fft_480[0] = (opus_fft_pass_t){
        .consts = {
            KF_PREROT_TWIDDLE_480(),
            KF_PREROT_TWINCR_480(),
        },
        .func_rdram = PhysicalAddr(rsp_opus_fft_prerot.code),
        .next_pass_rdram = PhysicalAddr(&fft_480[1]),
    };
    fft_480[1] = (opus_fft_pass_t){
        KF_BFLY4M1(120, 480),
        .stride = 120, .m = 1, .n = 120, .mm = 4,
        .next_pass_rdram = PhysicalAddr(&fft_480[2]),
    };
    fft_480[2] = (opus_fft_pass_t){
        KF_BFLY2(4, 480),
        .stride = 0, .m = 4, .n = 60, .mm = 0,
        .next_pass_rdram = PhysicalAddr(&fft_480[3]),
    };
    fft_480[3] = (opus_fft_pass_t){
        KF_BFLY4(15, 480),
        .stride = 15, .m = 8, .n = 15, .mm = 32,
        .next_pass_rdram = PhysicalAddr(&fft_480[4]),
    };
    fft_480[4] = (opus_fft_pass_t){
        KF_BFLY3(5, 480),
        .stride = 5, .m = 32, .n = 5, .mm = 96,
        .next_pass_rdram = PhysicalAddr(&fft_480[5]),
    };
    fft_480[5] = (opus_fft_pass_t){
        KF_BFLY5(1, 480),
        .stride = 1, .m = 96, .n = 1, .mm = 1,
        .next_pass_rdram = PhysicalAddr(&fft_480[6]),
    };
    fft_480[6] = (opus_fft_pass_t){
        .consts = {
            KF_POSTROT_TWIDDLE1(1920),
            KF_POSTROT_TWIDDLE2(1920),
            KF_POSTROT_TWINCR1(1920),
            KF_POSTROT_TWINCR2(1920),
        },
        .func_rdram = PhysicalAddr(rsp_opus_fft_postrot.code),
        .n = 960,
        .next_pass_rdram = 0,
    };

    data_cache_hit_writeback_invalidate(fft_60, sizeof(fft_60));
    data_cache_hit_writeback_invalidate(fft_480, sizeof(fft_480));
}

/** @brief Compare RSP and C implementation of IMDCT */
#define COMPARE_MDCT_REFERENCE 0

/** @brief Run a IMDCT on RSP */
void rsp_clt_mdct_backward(const mdct_lookup *l, kiss_fft_scalar *in, kiss_fft_scalar * OPUS_RESTRICT out,
      const opus_val16 * OPUS_RESTRICT window, int overlap, int shift, int stride, int B, int NB, int arch)
{
    int N = l->n >> shift;

    #if COMPARE_MDCT_REFERENCE
    static kiss_fft_scalar *ref_out = NULL;
    if (!ref_out) ref_out = malloc_uncached((960+overlap)*sizeof(kiss_fft_scalar));
    memcpy(ref_out, out, (960+overlap)*sizeof(kiss_fft_scalar));
    debugf("ref impl\n");
    debug_hexdump(ref_out, 64);
    for (int b=0;b<B;b++) {
        clt_mdct_backward_c(l, &in[b], ref_out+NB*b, window, overlap, shift, stride, arch);
    }
    debugf("ref impl finish\n");
    debug_hexdump(ref_out, 64);
    // clt_mdct_backward_c(l, in, out, window, overlap, shift, stride, arch);
    // data_cache_hit_writeback_invalidate(out, l->n*2*sizeof(kiss_fft_scalar));
    #endif

    // debugf("rsp_clt_mdct_backward: in=%p-%p out=%p N=%d(nfft=%d) shift=%d stride=%d B=%d NB=%d\n", 
    //     in, in+N/2*stride-1, out, N, l->kfft[shift]->nfft, shift, stride, B, NB);

    // Workram layout:
    // 0-3840:      temporary buffer holding up to 1920 FFT values (after deinterleaving)
    // 3840-7936:   DMEM backup
    static uint8_t *rsp_workram = NULL;
    if (!rsp_workram) rsp_workram = malloc_uncached(3840+4096);

    data_cache_hit_writeback_invalidate(out, N*2*stride+overlap*2); // FIXME: maybe *stride is wrong? 
    data_cache_hit_writeback_invalidate(in, N*2*stride); // FIXME: maybe *stride is wrong?
    assertf(PhysicalAddr(in) % 8 == 0, "in=%p", in);
    assert(PhysicalAddr(l->kfft[shift]->bitrev) % 8 == 0);
    rspq_write(0x9<<28, 0x0,
        PhysicalAddr(in),
        (l->n-1) | ((stride-1)<<12) | (shift<<16),
        PhysicalAddr(rsp_workram),
        PhysicalAddr(l->kfft[shift]->bitrev),
        PhysicalAddr(l->kfft[shift]->nfft == 480 ? fft_480 : fft_60),
        PhysicalAddr(out+(overlap>>1))
    );

    static int16_t *rsp_window = NULL;
    if (!rsp_window) {
        // RSP window function requires values to be swizzled according to
        // a specific pattern for optimization reasons
        rsp_window = malloc_uncached(overlap * 2 * sizeof(int16_t));
        assert((overlap % 8) == 0);
        for (int i=0;i<overlap;i+=8) {
            rsp_window[i+0] = window[i+0];
            rsp_window[i+1] = window[i+4];
            rsp_window[i+2] = window[i+1];
            rsp_window[i+3] = window[i+5];
            rsp_window[i+4] = window[i+2];
            rsp_window[i+5] = window[i+6];
            rsp_window[i+6] = window[i+3];
            rsp_window[i+7] = window[i+7];
        }
        for (int i=0;i<overlap;i+=8) {
            rsp_window[overlap+i+7] = window[i+0];
            rsp_window[overlap+i+6] = window[i+4];
            rsp_window[overlap+i+5] = window[i+1];
            rsp_window[overlap+i+4] = window[i+5];
            rsp_window[overlap+i+3] = window[i+2];
            rsp_window[overlap+i+2] = window[i+6];
            rsp_window[overlap+i+1] = window[i+3];
            rsp_window[overlap+i+0] = window[i+7];
        }
        // debugf("Window: %p\n", rsp_window);
        // debug_hexdump(rsp_window, 64);
    }
    
    // rspq_wait();
    // debugf("Overlap area: %p\n", out);
    // debug_hexdump(out, overlap*4);

    assert(overlap < 256);
    for (int b=0; b<B; b++) {
        rspq_write(0x9<<28, 0x1,
            PhysicalAddr(out+NB*b),
            (overlap << 24) | PhysicalAddr(rsp_window)
        );
    }
    rspq_flush();

    #if COMPARE_MDCT_REFERENCE
    rspq_highpri_end();
    rspq_wait();

    int check_offset =  (overlap>>1);
    int N4 = l->n >> 2;
    uint16_t *debug = malloc_uncached(N4*2*4);
    uint16_t *workram = (uint16_t*)(out+check_offset);
    if (false) {
        for (int i=0;i<N4;i++) {
            debug[i*4+0] = workram[i*4+0];
            debug[i*4+1] = workram[i*4+2];
            debug[i*4+2] = workram[i*4+1];
            debug[i*4+3] = workram[i*4+3];
        }
    } else {
        memcpy(debug, workram, N4*2*4);
    }

    debugf("RSP: %p\n", in);
    debug_hexdump(debug, N4*2*4);
    debugf("REF: %p\n", in);
    debug_hexdump(ref_out+check_offset, N4*2*4);

    float error = 0;
    int32_t *workram32 = (int32_t*)debug;
    for (int i=0;i<N4*2;i++) {
        int32_t rsp = workram32[i];
        int32_t ref = (ref_out+check_offset)[i];
        float diff = fabsf(rsp - ref);
        if (diff > 16<<12) {
            debugf("ERROR: 0x%x: %f (%lx - %lx)\n", i*4, diff / (1<<12), rsp, ref);
        }
        error += diff*diff;
    }
    debugf("RMSD: %f\n", sqrtf(error / N4*2) / (1<<12));
    rspq_highpri_begin();
    #endif
}

/*******************************************************************************
 * Comb filter
 * RSP version of comb_filter() in celt.c
 *******************************************************************************/
/*
 * A comb filter is on paper a very simple filter: it just adds a delayed
 * version of the input signal to the output signal. Basically, a dual
 * comb filter is:
 *     
 *     buf[i] = buf[i] + buf[i-T0] * K0 + buf[i-T1] * K1
 * 
 * where T0,T1 are the delay lengths, and K0,K1 are the attenuation constants.
 * 
 * The complexity in the RSP implementation comes from the fact that we must
 * be efficient with memory usage and support arbitrary T0 and T1 values.
 * 
 * Given a fixed buffer available DMEM (the biggest we can support, defined by
 * RSP_MAX_SAMPLES), there are several possible "layouts" for the data needed
 * for the filter.
 * 
 * For instance, if T0 and T1 are quite big, the sample buffer must be split
 * into three different sub-buffers: one holding samples from buf[i] onward,
 * a second holding samples from buf[i-T0] onward, and a third holding samples
 * from buf[i-T1] onward.
 * 
 * If T0 and T1 are small, instead, we must instead use a single buffer of
 * samples that spans all the required data. Not only this is more efficient,
 * but it is also necessary, because the delayed samples (eg: buf[i-T0])
 * will be the result of previous iterations of the filter.
 * 
 * Calculating the correct usage of DMEM given T0, T1 and the available space
 * in DMEM is a non trivial problem, solved by the algorithm implemented
 * by rsp_comb_calc_layout. The function creates a rsp_layout_t structure
 * that describes how/where the buffer in DMEM must be split.
 */

/** @brief Compare RSP and C implementation of comb filter */
#define COMPARE_REFERENCE 0

/** @brief Compare RSP and C implementation of dual comb filter */
#define COMPARE_REFERENCE_DUAL 0

/** @brief Reference implementation of comb filter */
extern void comb_filter_const_c(opus_val32 *y, opus_val32 *x, int T, int N,
      opus_val16 g10, opus_val16 g11, opus_val16 g12);

static const int RSP_MAX_SAMPLES = 688;

/** @brief Layout of samples within RSP during combfilter */
typedef struct {
    int nT0;        // Delay offset T0 (possibly adjusted for alignment)
    int nT1;        // Delay offset T1 (possibly adjusted for alignment)
    int t1_idx;     // Index in DMEM where to load samples from x[i-T1] onward
    bool t1_merged; // If true, samples from x[i-T0] and x[i-T1] are merged together
    int i_idx;      // Index in DMEM where to load samples from x[i] onward
    bool i_merged;  // If true, samples from x[i] and x[i-T1] are merged together
} rsp_layout_t;

// Calculate a hopefully best layout for samples in DMEM, to maximize the
// number of processed samples per RSP call.
//   I: index of the first sample to process (normally, 0)
//   N: total number of samples to process
//   T0: delay offset T0 from I (delay group 0 starts from buf[I-T0])
//   T1: delay offset T1 from I (delay group 1 starts from buf[I-T1])
static rsp_layout_t rsp_comb_calc_layout(int I, int N, int T0, int T1) {
    int one_third = (RSP_MAX_SAMPLES-10)/3;
    int init_t1_idx = ROUND_UP(one_third+5, 4);
    rsp_layout_t L = (rsp_layout_t){
        .nT0 = ROUND_DOWN(T0, 4), .nT1 = ROUND_DOWN(T1, 4),
        .t1_idx = init_t1_idx,
        .i_idx = ROUND_UP(init_t1_idx+one_third+5, 8),
    };

    bool full_merge = false;
    int nproc = MIN(N, (RSP_MAX_SAMPLES - L.i_idx));
    if (T1+5+nproc > I) {
        int t1dist = ROUND_UP(I - L.nT1, 4);
        int half = (RSP_MAX_SAMPLES - t1dist - 5) / 2;
        L.i_idx = ROUND_UP(half + t1dist + 5, 8);
        L.t1_idx = L.i_idx - t1dist;
        L.nT1 = I - (L.i_idx - L.t1_idx);
        L.i_merged = true;
        nproc = MIN(N, (RSP_MAX_SAMPLES - L.i_idx));
        full_merge = T0+5+nproc > I;
    } else if (T0+5+nproc > T1) {
        int t0dist = ROUND_UP(L.nT1 - L.nT0, 4);
        int half = (RSP_MAX_SAMPLES - t0dist - 5) / 2;
        L.i_idx = ROUND_UP(half + t0dist + 5, 8);
        L.t1_idx = t0dist;
        L.nT1 = L.nT0 + t0dist;
        L.t1_merged = true;
        nproc = MIN(N, (RSP_MAX_SAMPLES - L.i_idx));
        full_merge = T1+5+nproc > I;
    }

    if (full_merge) {
        int t0dist = ROUND_UP(L.nT1 - L.nT0, 4);
        int t1dist = ROUND_UP(I - L.nT1, 4);
        L.i_idx = ROUND_UP(t0dist + t1dist, 8);
        L.t1_idx = L.i_idx - t1dist;
        L.nT1 = I - (L.i_idx - L.t1_idx);
        L.nT0 = L.nT1 - L.t1_idx;
        L.t1_merged = true;
        L.i_merged = true;
    }

    return L;
}

__attribute__((used))
static void rsp_comb_dump_layout(rsp_layout_t *L)
{
   debugf("nT0: %d      nT1: %d\n", L->nT0, L->nT1);
   debugf("t1_idx: %d    t1_merged: %d\n", L->t1_idx, L->t1_merged);
   debugf("i_idx: %d     i_merged: %d\n", L->i_idx, L->i_merged);
}

// Load all the samples needed for the comb filter into RSP DMEM, given
// the calculated DMEM layout.
__attribute__((noinline))
static int rsp_comb_fetch_all(opus_val32 *x, rsp_layout_t *L, int N)
{
    int nproc = MIN(N, RSP_MAX_SAMPLES - L->i_idx);
    int i_end = L->i_idx + nproc;
    int t1_end = L->i_merged ? i_end : L->i_idx;
    int t0_end = L->t1_merged ? t1_end : L->t1_idx;
    int t1_samples = t1_end - L->t1_idx;
    int t0_samples = t0_end - 0;

    rsp_cmd_comb_fetch(x + L->nT0, 0, t0_samples);
    if (!L->t1_merged)
        rsp_cmd_comb_fetch(x + L->nT1, L->t1_idx, t1_samples);
    if (!L->i_merged)
        rsp_cmd_comb_fetch(x, L->i_idx, nproc);
    return nproc;
}

/** @brief Run a comb filter on the RSP */
void rsp_opus_comb_filter_const(opus_val32 *y, opus_val32 *x, int T, int N,
      opus_val16 g10, opus_val16 g11, opus_val16 g12, int arch)
{
    assert(x == y);

#if COMPARE_REFERENCE
    rspq_wait();
    // debugf("comb_filter_const_c: x:%p y:%p T1:%d N:%d\n", x, y, T, N);
    assert(x == y);
    opus_val32 *yref = malloc((N+T+2)*sizeof(opus_val32));
    data_cache_hit_writeback_invalidate(x-T-2, (N+T+5)*sizeof(opus_val32));
    OPUS_MOVE(yref, x-T-2, N+T+2);
    comb_filter_const_c(yref+T+2, yref+T+2, T, N, g10, g11, g12);
#endif

    data_cache_hit_writeback_invalidate(x-T-2, (N+T+5)*sizeof(opus_val32));

    // Calculate the best DMEM layout for this filter
    int T0 = -T-2;
    rsp_layout_t L = rsp_comb_calc_layout(0, N, T0, T0);
    // rsp_comb_dump_layout(&L);
    int t0_idx = T0 - L.nT0 + 0;

    // debugf("g10:%04x g11:%04x g12:%04x t0_idx:%d\n", g10, g11, g12, t0_idx);
    int NN = N;
    opus_val32 *yy = y;
    while (NN > 0) {
        int nn = rsp_comb_fetch_all(x, &L, NN);

        // debugf("x:%p yy:%p(%d) NN:%d nn:%d\n", x, yy, yy-y, NN, nn);
        assert(PhysicalAddr(yy) % 8 == 0);
        assert(NN%8 == 0);
        assert(T >= 0 && T < 65536);
        rsp_cmd_comb_single(nn, L.i_idx, t0_idx, g10, g11, g12);
        rsp_cmd_comb_result(yy, L.i_idx, nn);

        x += nn;
        yy += nn;
        NN -= nn;
    }

#if COMPARE_REFERENCE
   rspq_wait();
   for (int i=0; i<N; i++) {
      int diff = y[i] - yref[i+T+2]; if (diff < 0) diff = -diff;
      if (diff > 3) {
         debugf("REF:\n");
         debug_hexdump(yref+T+2, 80*sizeof(opus_val32));
         debugf("RSP:\n");
         debug_hexdump(y, 80*sizeof(opus_val32));
         debugf("i:%d y:%08lx yref:%08lx diff:%d\n", i, y[i], yref[i+T+2], diff);
         while(1){}
      }
   }
   free(yref);
#endif
}

/** @brief Run a dual comb filter on the RSP */
void rsp_opus_comb_filter_dual(opus_val32 *y, opus_val32 *x, int T0, int T1, int N,
      opus_val16 g00, opus_val16 g01, opus_val16 g02,
      opus_val16 g10, opus_val16 g11, opus_val16 g12,
      const opus_val16 *window)
{
    #if COMPARE_REFERENCE_DUAL
        rspq_wait();
        int TT = T0 > T1 ? T0 : T1;
        opus_val32 *yref = malloc((N+TT+3)*sizeof(opus_val32));
        data_cache_hit_writeback_invalidate(x-TT-2, (N+TT+3)*sizeof(opus_val32)); 
        OPUS_MOVE(yref, x-TT-2, N+TT+3);
        {
            // opus_val32 *orig = x;
            opus_val32 *x = yref+TT+2;
            opus_val32 *y = yref+TT+2;
            opus_val32 x0, x1, x2, x3, x4;
            x1 = x[-T1+1];
            x2 = x[-T1  ];
            x3 = x[-T1-1];
            x4 = x[-T1-2];
            for (int i=0;i<N;i++)
            {
            opus_val16 f;
            x0=x[i-T1+2];
            f = MULT16_16_Q15(window[i],window[i]);
            // if (i==118) {
            //    debugf("x[%d]:%08lx\n", i, x[i]);
            //    debugf("x[T0][%p]:%08lx,%08lx,%08lx,%08lx,%08lx\n", &orig[i-T0+1], x[i-T0+2], x[i-T0+1], x[i-T0], x[i-T0-1], x[i-T0-2]);
            //    debugf("x[T1][%p]:%08lx,%08lx,%08lx,%08lx,%08lx\n", &orig[i-T1+1], x0, x1, x2, x3, x4);
            //    debugf("XXX: %08lx\n", orig[i-T0+1]);
            //    debugf("XXX: %08lx\n", x[i-T0+1]);
            //    debugf("window: %04x (f: %04x)\n", window[i], f);
            //    debugf("gT0:%lx,%lx,%lx\n", MULT16_16_Q15((Q15ONE-f),g00), MULT16_16_Q15((Q15ONE-f),g01), MULT16_16_Q15((Q15ONE-f),g02));
            //    debugf("gT1:%lx,%lx,%lx\n", MULT16_16_Q15(f,g10), MULT16_16_Q15(f,g11), MULT16_16_Q15(f,g12));
            // }
            y[i] = x[i]
                        + MULT16_32_Q15(MULT16_16_Q15((Q15ONE-f),g00),x[i-T0])
                        + MULT16_32_Q15(MULT16_16_Q15((Q15ONE-f),g01),ADD32(x[i-T0+1],x[i-T0-1]))
                        + MULT16_32_Q15(MULT16_16_Q15((Q15ONE-f),g02),ADD32(x[i-T0+2],x[i-T0-2]))
                        + MULT16_32_Q15(MULT16_16_Q15(f,g10),x2)
                        + MULT16_32_Q15(MULT16_16_Q15(f,g11),ADD32(x1,x3))
                        + MULT16_32_Q15(MULT16_16_Q15(f,g12),ADD32(x0,x4));
            y[i] = SATURATE(y[i], SIG_SAT);
            // if (i==118) {
            //    debugf("y[%d]:%08lx\n", i, y[i]);
            // }
            x4=x3;
            x3=x2;
            x2=x1;
            x1=x0;
            }
        }
    #endif

    #if !RSP_IMDCT
    data_cache_hit_writeback_invalidate(x-T0-2, (N+5)*sizeof(opus_val32));
    data_cache_hit_writeback_invalidate(x-T1-2, (N+5)*sizeof(opus_val32));
    data_cache_hit_writeback_invalidate(x, N*sizeof(opus_val32)); 
    #endif
    // debugf("OVERLAP INPUT: T0:%d T1:%d\n", T0, T1);

    // Calculate the best DMEM layout for this filter
    int oT0 = -T0-2;
    int oT1 = -T1-2;
    rsp_layout_t L; int t0_idx, t1_idx;

    if (oT0 <= oT1) {
        L = rsp_comb_calc_layout(0, N, oT0, oT1);
        t0_idx = oT0 - L.nT0 + 0;
        t1_idx = oT1 - L.nT1 + L.t1_idx;
    } else {
        L = rsp_comb_calc_layout(0, N, oT1, oT0);
        t1_idx = oT1 - L.nT0 + 0;
        t0_idx = oT0 - L.nT1 + L.t1_idx;
    }
    // rsp_comb_dump_layout(&L);

    // debugf("OVERLAP: g00:%04x g01:%04x g02:%04x t0_idx:%d (swapped:%d)\n", g00, g01, g02, t0_idx, oT0 > oT1);
    // debugf("OVERLAP: g10:%04x g11:%04x g12:%04x t1_idx:%d (swapped:%d)\n", g10, g11, g12, t1_idx, oT0 > oT1);

    // Fetch samples into RSP DMEM. Notice that in the case of the dual comb filter,
    // we support only doing the whole overlap in one go, as we don't currently
    // keep track of partially-updated window index.
    int nn = rsp_comb_fetch_all(x, &L, N);
    assert(nn == N);
    // debugf("OVERLAP: x:%p yy:%p(%d) NN:%d nn:%d\n", x, y, y-y, N, nn);
    assert(PhysicalAddr(y) % 8 == 0);
    assert(nn%8 == 0);
    rsp_cmd_comb_dual(nn, L.i_idx, t0_idx, t1_idx, g00, g01, g02, g10, g11, g12);
    rsp_cmd_comb_result(y, L.i_idx, nn);

    #if COMPARE_REFERENCE_DUAL
    rspq_wait();

    for (int i=0; i<N; i++) {
        int diff = y[i] - yref[i+TT+2]; if (diff < 0) diff = -diff;
        if (diff > 6) {
            debugf("REF:\n");
            debug_hexdump(yref+TT+2, 32*sizeof(opus_val32));
            debugf("RSP:\n");
            debug_hexdump(y, 32*sizeof(opus_val32));
            debugf("DUAL: i:%d y:%08lx yref:%08lx diff:%d\n", i, y[i], yref[i+TT+2], diff);
            while(1){}
        }
    }
    free(yref);
    #endif
}

#undef COMPARE_REFERENCE
#undef COMPARE_REFERENCE_DUAL


/*******************************************************************************
 * Emphasis filter
 * RSP version of deemphasis() in celt_decoder.c
 *******************************************************************************/

/** @brief Compare RSP and C emphasis filter */
#define RSP_DEEMPHASIS_COMPARE 0

/** @brief Reference implementation of the emphasis filter */
extern void deemphasis(celt_sig *in[], opus_val16 *pcm, int N, int C, int downsample, const opus_val16 *coef,
      celt_sig *mem, int accum);

/** @brief Run the emphasis filter on teh RSP */
void rsp_opus_deemphasis(celt_sig *in[], opus_val16 *pcm, int N, int C, int downsample, const opus_val16 *coef,
      celt_sig *mem, int accum)
{

   #if RSP_DEEMPHASIS_COMPARE > 0
   static opus_val16 pcm_ref[960*2];
   static int compare_count = 0; compare_count++;
   if (compare_count == RSP_DEEMPHASIS_COMPARE || true) {
      // debugf("rsp_deemphasis: N:%d C:%d downsample:%d accum:%d\n", N, C, downsample, accum);
      // debugf("   mem:%lx,%lx\n", mem[0], mem[1]);

      celt_sig mem2[2] = { mem[0]*2*0.85, mem[1]*2*0.85 };
      deemphasis(in, pcm_ref, N, C, downsample, coef, mem2, accum);

      // debugf("Reference:\n");
      // debug_hexdump(pcm_ref, 64*C*sizeof(int16_t));
      // debugf("MEM:%lx,%lx\n", (celt_sig)(mem2[0]/2/0.85), (celt_sig)(mem2[1]/2/0.85));
   }
   #endif

   assert(accum == 0);
   assert(PhysicalAddr(in[0]) % 8 == 0);
   if (C>1) assert(PhysicalAddr(in[1]) % 8 == 0);
   assert(PhysicalAddr(mem) % 8 == 0);

   #if !RSP_COMB_FILTER
   data_cache_hit_writeback(in[0], N*sizeof(celt_sig));
   if (C>1) data_cache_hit_writeback(in[1], N*sizeof(celt_sig));
   data_cache_hit_writeback_invalidate(mem, 2*sizeof(celt_sig));
   #endif

   const int MAX_SAMPLES = 240; // 10*24
   _Static_assert(MAX_SAMPLES % 24 == 0,
      "MAX_SAMPLES must be a multiple of 24 to be able to be used with all supported downsampling factors, and vectorized by 8");

   opus_val16 *pcmcur = pcm;
   celt_sig *incur[2] = { in[0], in[1] };
   int NN = N;
   while (NN > 0) {
      int nn = NN > MAX_SAMPLES ? MAX_SAMPLES : NN;
      assertf(nn % 8 == 0, "nn:%d", nn);
      int nnO = nn * C / downsample;

      rsp_cmd_deemphasis(incur[0], (C>1 ? incur[1] : 0), pcmcur, mem, nn, downsample);

      incur[0] += nn;
      if (C>1) incur[1] += nn;
      pcmcur += nnO;
      NN -= nn;
   }
   rspq_flush();

   #if RSP_DEEMPHASIS_COMPARE > 0
   if (compare_count == RSP_DEEMPHASIS_COMPARE || 1) {
      rspq_wait();
      // debugf("RSP:\n");
      // debug_hexdump(pcm, 64*C*sizeof(int16_t));
      // debugf("MEM:%lx,%lx\n", mem[0], mem[1]);
      // while(1) {}
      for (int i=0;i<N*2;i++) {
         int diff1 = pcm_ref[i] - pcm[i];
         if (diff1 < 0) diff1 = -diff1;
         if (diff1 > 8) {
            debugf("REF:\n");
            debug_hexdump(pcm_ref, 0x500);
            debugf("RSP:\n");
            debug_hexdump(pcm, 0x500);
            debugf("Difference at %x: %04x != %04x\n", i*2, pcm_ref[i], pcm[i]);
            while(1) {}
         }
         
      }
   }
   #endif
}

#undef RSP_DEEMPHASIS_COMPARE