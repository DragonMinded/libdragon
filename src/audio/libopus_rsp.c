/**
 * @file libopus_rsp.c
 * @author Giovanni Bajo (giovannibajo@gmail.com)
 * @brief C-level glue for RSP ucode for Opus decoding
 */

#include "libopus_internal.h"
#include "rspq.h"
#include "debug.h"
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

/*******************************************************************************
 * Emphasis filter
 * RSP version of deemphasis() in celt_decoder.c
 *******************************************************************************/

void deemphasis(celt_sig *in[], opus_val16 *pcm, int N, int C, int downsample, const opus_val16 *coef,
      celt_sig *mem, int accum);

void rsp_opus_deemphasis(celt_sig *in[], opus_val16 *pcm, int N, int C, int downsample, const opus_val16 *coef,
      celt_sig *mem, int accum)
{
   #define RSP_DEEMPHASIS_COMPARE 0

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