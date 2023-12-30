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

DEFINE_RSP_UCODE(rsp_opus_dsp);
DEFINE_RSP_UCODE(rsp_opus_imdct);

void rsp_opus_init(void)
{
    static bool init = false;
    if (!init) {
        rspq_init();
        rspq_overlay_register_static(&rsp_opus_dsp, 0x8<<28);
        rspq_overlay_register_static(&rsp_opus_imdct, 0x9<<28);
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

void comb_filter_const_c(opus_val32 *y, opus_val32 *x, int T, int N,
      opus_val16 g10, opus_val16 g11, opus_val16 g12);

static const int RSP_MAX_SAMPLES = 688;

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

void rsp_opus_comb_filter_const(opus_val32 *y, opus_val32 *x, int T, int N,
      opus_val16 g10, opus_val16 g11, opus_val16 g12, int arch)
{
    assert(x == y);

#define COMPARE_REFERENCE 0

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

void rsp_opus_comb_filter_dual(opus_val32 *y, opus_val32 *x, int T0, int T1, int N,
      opus_val16 g00, opus_val16 g01, opus_val16 g02,
      opus_val16 g10, opus_val16 g11, opus_val16 g12,
      const opus_val16 *window)
{
    #define COMPARE_REFERENCE_DUAL 0
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

    data_cache_hit_writeback_invalidate(x-T0-2, (N+5)*sizeof(opus_val32));
    data_cache_hit_writeback_invalidate(x-T1-2, (N+5)*sizeof(opus_val32));
    data_cache_hit_writeback_invalidate(x, N*sizeof(opus_val32)); 
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

#define RSP_DEEMPHASIS_COMPARE 0

void deemphasis(celt_sig *in[], opus_val16 *pcm, int N, int C, int downsample, const opus_val16 *coef,
      celt_sig *mem, int accum);

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
   assert(PhysicalAddr(pcm) % 8 == 0);
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