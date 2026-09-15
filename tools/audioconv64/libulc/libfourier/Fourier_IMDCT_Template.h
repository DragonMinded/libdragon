/************************************************/
#include "Fourier.h"
#include "Fourier_Helper.h"
/************************************************/

// Implementation notes for IMDCT:
//  IMDCT is implemented via DCT-IV, which can be thought of
//  as splitting the MDCT inputs into four regions:
//   {A,B,C,D}
//  and then taking the DCT-IV of:
//   {C_r + D, B_r - A}
//  On IMDCT, we get back these latter values following an
//  inverse DCT-IV (which is itself a DCT-IV due to its
//  involutive nature).
//  From a prior call, we keep C_r+D buffered, which becomes
//  A_r+B after accounting for movement to the next block.
//  We can then state:
//   Reverse(A_r + B) -        (B_r - A) = (A + B_r) - (B_r - A) = 2A
//          (A_r + B) + Reverse(B_r - A) = (A_r + B) + (B - A_r) = 2B
//           ^ Buffered         ^ New input data
//  Allowing us to reconstruct the inputs A,B.
FOURIER_FORCE_INLINE
void Fourier_IMDCT_Template(float *BufOut, const float *BufIn, float *BufLap, float *BufTmp, int N, int Overlap) {
	int i;
	FOURIER_ASSUME_ALIGNED(BufOut, FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(BufIn,  FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(BufLap, FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(BufTmp, FOURIER_ALIGNMENT);
	FOURIER_ASSUME(N >= 16);
	FOURIER_ASSUME(Overlap >= 0 && Overlap <= N);

	const float *Lap   = BufLap + N/2;
	const float *Tmp   = BufTmp + N/2;
	      float *OutLo = BufOut;
	      float *OutHi = BufOut + N;

	// Undo transform
	for(i=0;i<N;i++) BufTmp[i] = BufIn[i];
	Fourier_DCT4(BufTmp, BufOut, N);

	// Undo lapping
#if FOURIER_VSTRIDE > 1
	Fourier_Vec_t a, b;
	for(i=0;i<(N-Overlap)/2;i+=FOURIER_VSTRIDE) {
		Lap -= FOURIER_VSTRIDE; a = FOURIER_VLOAD(Lap);
		b = FOURIER_VLOAD(Tmp); Tmp += FOURIER_VSTRIDE;
		a = FOURIER_VREVERSE(a);
		b = FOURIER_VREVERSE(b);
		FOURIER_VSTORE(OutLo, a); OutLo += FOURIER_VSTRIDE;
		OutHi -= FOURIER_VSTRIDE; FOURIER_VSTORE(OutHi, b);
	}
	Fourier_Vec_t t0, t1 = FOURIER_VMUL(FOURIER_VSET1(1.0f/Overlap), FOURIER_VADD(FOURIER_VSET_LINEAR_RAMP(), FOURIER_VSET1(0.5f)));
	Fourier_Vec_t c  = Fourier_Cos(t1);
	Fourier_Vec_t s  = Fourier_Sin(t1);
	Fourier_Vec_t wc = Fourier_Cos(FOURIER_VSET1((float)FOURIER_VSTRIDE / Overlap));
	Fourier_Vec_t ws = Fourier_Sin(FOURIER_VSET1((float)FOURIER_VSTRIDE / Overlap));
	for(;i<N/2;i+=FOURIER_VSTRIDE) {
		Lap -= FOURIER_VSTRIDE; a = FOURIER_VREVERSE(FOURIER_VLOAD(Lap));
		b  = FOURIER_VLOAD(Tmp); Tmp += FOURIER_VSTRIDE;
		t0 = FOURIER_VFMS(c, a, FOURIER_VMUL(s, b));
		t1 = FOURIER_VFMA(s, a, FOURIER_VMUL(c, b));
		t1 = FOURIER_VREVERSE(t1);
		FOURIER_VSTORE(OutLo, t0); OutLo += FOURIER_VSTRIDE;
		OutHi -= FOURIER_VSTRIDE; FOURIER_VSTORE(OutHi, t1);
		t0 = c;
		t1 = s;
		c = FOURIER_VNFMA(t1, ws, FOURIER_VMUL(t0, wc));
		s = FOURIER_VFMA (t1, wc, FOURIER_VMUL(t0, ws));
	}
#else
	for(i=0;i<(N-Overlap)/2;i++) {
		float a = *--Lap;
		float b = *Tmp++;
		*OutLo++ = a;
		*--OutHi = b;
	}
	float c  = Fourier_Cos(0.5f / Overlap);
	float s  = Fourier_Sin(0.5f / Overlap);
	float wc = Fourier_Cos(1.0f / Overlap);
	float ws = Fourier_Sin(1.0f / Overlap);
	for(;i<N/2;i++) {
		float a = *--Lap;
		float b = *Tmp++;
		*OutLo++ = c*a - s*b;
		*--OutHi = s*a + c*b;
		a = c;
		b = s;
		c = wc*a - ws*b;
		s = ws*a + wc*b;
	}
#endif
	// Copy state to old block
	for(i=0;i<N/2;i++) BufLap[i] = BufTmp[i];
}

/************************************************/
// EOF
/************************************************/
