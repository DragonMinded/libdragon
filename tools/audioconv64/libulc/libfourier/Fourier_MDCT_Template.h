/************************************************/
#include "Fourier.h"
#include "Fourier_Helper.h"
/************************************************/

// Implementation notes for MDCT:
//  MDCT is implemented via DCT-IV, which can be thought of
//  as splitting the MDCT inputs into four regions:
//   {A,B,C,D}
//  and then taking the DCT-IV of:
//   {C_r + D, B_r - A}
//  During non-overlap regions, A,B scales by 0.0 and C,D by 1.0.
//  As an optimization, the `C_r + D` region is written to
//  in a backwards direction, leading to `D_r + C`; this
//  leads to reuse of the trigonometric constants, avoiding
//  reading the array of sin/cos twice per call.
// Pseudocode:
//   for(n=0;n<(N-Overlap)/2;n++) {
//     MDCT[N/2-1-n] =  New[n];
//     MDST[N/2-1-n] = -New[n];
//     MDCT[N/2+n]   =  Old[N-1-n];
//     MDST[N/2+n]   = -Old[N-1-n];
//   }
//   for(;n<N/2;n++) {
//     MDCT[N/2-1-n] =  c*New[n] + s*New[N-1-n];
//     MDST[N/2-1-n] = -c*New[n] + s*New[N-1-n];
//     MDCT[N/2+n]   = -s*Old[n] + c*Old[N-1-n];
//     MDST[N/2+n]   = -s*Old[n] - c*Old[N-1-n];
//   }
//   DCT4(MDCT);
//   DCT4(MDST);
//   for(n=0;n<N/2;n++) Swap(MDST[n], MDST[N-1-n])
// If we don't care about MDST, this can be refactored:
//   for(n=0;n<(N-Overlap)/2;n++) {
//     MDCT[N/2-1-n] = New[n];
//     MDCT[N/2+n]   = Old[n];
//     Old[n]        = New[N-1-n];
//   }
//   for(;n<N/2;n++) {
//     MDCT[N/2-1-n] =  c*New[n] + s*New[N-1-n];
//     MDCT[N/2+n]   = Old[n];
//     Old[n]        = -s*New[n] + c*New[N-1-n];
//   }
//   DCT4(MDCT);
FOURIER_FORCE_INLINE
void Fourier_MDCT_MDST_Template(float *MDCT, float *MDST, const float *New, float *Lap, float *BufTmp, int N, int Overlap) {
	int n;
	FOURIER_ASSUME_ALIGNED(MDCT,   FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(MDST,   FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(New,    FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(Lap,    FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(BufTmp, FOURIER_ALIGNMENT);
	FOURIER_ASSUME(N >= 16);
	FOURIER_ASSUME(Overlap >= 0 && Overlap <= N);

	      float *LapLo = Lap;
	      float *LapHi = Lap + N;
	const float *NewLo = New;
	const float *NewHi = New + N;
	      float *MDCTMid = MDCT + N/2;
	      float *MDSTMid = MDST + N/2;

	// Perform windowed lapping
	// NOTE: We compute MDST using DCT-IV, so sign-flip every other
	// value here, and then reverse the whole array after DCT-IV.
	{
#if FOURIER_VSTRIDE > 1
		Fourier_Vec_t A, Br, C, Dr;
		Fourier_Vec_t t0 = FOURIER_VSET1(0.0f);
		for(n=0;n<(N-Overlap)/2;n+=FOURIER_VSTRIDE) {
			A  = FOURIER_VLOAD(LapLo                 + n);
			Br = FOURIER_VLOAD(LapHi-FOURIER_VSTRIDE - n);
			C  = FOURIER_VLOAD(NewLo                 + n);
			Dr = FOURIER_VLOAD(NewHi-FOURIER_VSTRIDE - n);
			Br = FOURIER_VREVERSE(Br);
			C  = FOURIER_VREVERSE(C);
			FOURIER_VSTORE(LapLo                   + n, t0);
			FOURIER_VSTORE(LapHi  -FOURIER_VSTRIDE - n, Dr);
			FOURIER_VSTORE(MDCTMid-FOURIER_VSTRIDE - n, C);
			FOURIER_VSTORE(MDSTMid-FOURIER_VSTRIDE - n, FOURIER_VNEGATE_ODD(C));
			C  = FOURIER_VSUB(Br, A);
			Dr = FOURIER_VADD(Br, A);
			FOURIER_VSTORE(MDCTMid   + n, C);
			FOURIER_VSTORE(MDSTMid   + n, FOURIER_VNEGATE_ODD(Dr));
		}
		Fourier_Vec_t t1 = FOURIER_VMUL(FOURIER_VSET1(1.0f/Overlap), FOURIER_VADD(FOURIER_VSET_LINEAR_RAMP(), FOURIER_VSET1(0.5f)));
		Fourier_Vec_t c  = Fourier_Cos(t1);
		Fourier_Vec_t s  = Fourier_Sin(t1);
		Fourier_Vec_t wc = Fourier_Cos(FOURIER_VSET1((float)FOURIER_VSTRIDE / Overlap));
		Fourier_Vec_t ws = Fourier_Sin(FOURIER_VSET1((float)FOURIER_VSTRIDE / Overlap));
		for(;n<N/2;n+=FOURIER_VSTRIDE) {
			A  = FOURIER_VLOAD(LapLo                 + n);
			Br = FOURIER_VLOAD(LapHi-FOURIER_VSTRIDE - n);
			C  = FOURIER_VLOAD(NewLo                 + n);
			Dr = FOURIER_VLOAD(NewHi-FOURIER_VSTRIDE - n);
			FOURIER_VSTORE(LapLo                 + n, FOURIER_VMUL(s, C));
			FOURIER_VSTORE(LapHi-FOURIER_VSTRIDE - n, FOURIER_VMUL(FOURIER_VREVERSE(c), Dr));
			Br = FOURIER_VREVERSE(Br);
			Dr = FOURIER_VREVERSE(Dr);
			C  = FOURIER_VMUL(C,  c);
			Dr = FOURIER_VMUL(Dr, s);
			t0 = FOURIER_VADD(C,  Dr);
			t1 = FOURIER_VSUB(C,  Dr);
			t0 = FOURIER_VREVERSE(t0);
			t1 = FOURIER_VREVERSE(t1);
			FOURIER_VSTORE(MDCTMid-FOURIER_VSTRIDE - n, t0);
			FOURIER_VSTORE(MDSTMid-FOURIER_VSTRIDE - n, FOURIER_VNEGATE_ODD(t1));
			t0 = FOURIER_VSUB(Br, A);
			t1 = FOURIER_VADD(Br, A);
			FOURIER_VSTORE(MDCTMid + n, t0);
			FOURIER_VSTORE(MDSTMid + n, FOURIER_VNEGATE_ODD(t1));
			t0 = c;
			t1 = s;
			c = FOURIER_VNFMA(t1, ws, FOURIER_VMUL(t0, wc));
			s = FOURIER_VFMA (t1, wc, FOURIER_VMUL(t0, ws));
		}
#else
		float A, Br, C, Dr;
		for(n=0;n<(N-Overlap)/2;n++) {
			A  = LapLo[   n];
			Br = LapHi[-1-n];
			C  = NewLo[   n];
			Dr = NewHi[-1-n];
			LapLo  [   n] =  0.0f;
			LapHi  [-1-n] =  Dr;
			MDCTMid[-1-n] =  C;
			MDSTMid[-1-n] =  C;
			MDCTMid[   n] = -A + Br;
			MDSTMid[   n] =  A + Br;
		}
		float c  = Fourier_Cos(0.5f / Overlap);
		float s  = Fourier_Sin(0.5f / Overlap);
		float wc = Fourier_Cos(1.0f / Overlap);
		float ws = Fourier_Sin(1.0f / Overlap);
		for(;n<N/2;n++) {
			A  = LapLo[   n];
			Br = LapHi[-1-n];
			C  = NewLo[   n];
			Dr = NewHi[-1-n];
			LapLo  [   n] = s*C;
			LapHi  [-1-n] = c*Dr;
			C *= c, Dr *= s;
			MDCTMid[-1-n] =  C + Dr;
			MDSTMid[-1-n] =  C - Dr;
			MDCTMid[   n] = -A + Br;
			MDSTMid[   n] =  A + Br;
			A  = c;
			Br = s;
			c  = wc*A - ws*Br;
			s  = ws*A + wc*Br;

		}
		for(n=1;n<N;n+=2) MDST[n] = -MDST[n];
#endif
	}

	// Do actual transforms
	Fourier_DCT4(MDCT, BufTmp, N);
	Fourier_DCT4(MDST, BufTmp, N);

	// Reverse array for MDST
	{
		float *BufLo = MDST;
		float *BufHi = MDST + N;
#if FOURIER_VSTRIDE > 1
		Fourier_Vec_t v0, v1;
		for(n=0;n<N/2;n+=FOURIER_VSTRIDE) {
			BufHi -= FOURIER_VSTRIDE;
			v0 = FOURIER_VLOAD(BufLo);
			v1 = FOURIER_VLOAD(BufHi);
			v0 = FOURIER_VREVERSE(v0);
			v1 = FOURIER_VREVERSE(v1);
			FOURIER_VSTORE(BufHi, v0);
			FOURIER_VSTORE(BufLo, v1);
			BufLo += FOURIER_VSTRIDE;
		}
#else
		for(n=0;n<N/2;n++) {
			BufHi--;
			float t = *BufLo;
			*BufLo = *BufHi;
			*BufHi = t;
			BufLo++;
		}
#endif
	}
}

/************************************************/
// EOF
/************************************************/
