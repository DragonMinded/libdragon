/************************************************/
#include "Fourier.h"
#include "Fourier_Helper.h"
/************************************************/

FOURIER_FORCE_INLINE
void Fourier_DCT2_Template(float *Buf, float *Tmp, int N) {
	int i;
	FOURIER_ASSUME_ALIGNED(Buf, FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(Tmp, FOURIER_ALIGNMENT);
	FOURIER_ASSUME(N >= 8);

	// Stop condition
	if(N == 8) {
		Fourier_DCT2_8(Buf);
		return;
	}

	// Perform butterflies
	//  u = H_n.x
	{
		const float *SrcLo = Buf;
		const float *SrcHi = Buf + N;
		      float *DstLo = Tmp;
		      float *DstHi = Tmp + N/2;
#if FOURIER_VSTRIDE > 1
		Fourier_Vec_t a, b;
		Fourier_Vec_t s, d;
		for(i=0;i<N/2;i+=FOURIER_VSTRIDE) {
			SrcHi -= FOURIER_VSTRIDE; b = FOURIER_VREVERSE(FOURIER_VLOAD(SrcHi));
			a = FOURIER_VLOAD(SrcLo); SrcLo += FOURIER_VSTRIDE;
			s = FOURIER_VADD(a, b);
			d = FOURIER_VSUB(a, b);
			FOURIER_VSTORE(DstLo, s); DstLo += FOURIER_VSTRIDE;
			FOURIER_VSTORE(DstHi, d); DstHi += FOURIER_VSTRIDE;
		}
#else
		float a, b;
		for(i=0;i<N/2;i++) {
			a = *SrcLo++;
			b = *--SrcHi;
			*DstLo++ = a + b;
			*DstHi++ = a - b;
		}
#endif
	}

	// Perform recursion
	//  z1 = cos2([u_j][j=0..n/2-1],n/2)
	//  z2 = cos4([u_j][j=n/2..n-1],n/2)
	Fourier_DCT2(Tmp,       Buf,       N/2);
	Fourier_DCT4(Tmp + N/2, Buf + N/2, N/2);

	// Combine
	//  y = (P_n)^T.(z1^T, z2^T)^T
	{
		const float *SrcLo = Tmp;
		const float *SrcHi = Tmp + N/2;
		      float *Dst   = Buf;
#if FOURIER_VSTRIDE > 1
		Fourier_Vec_t a, b;
		for(i=0;i<N/2;i+=FOURIER_VSTRIDE) {
			a = FOURIER_VLOAD(SrcLo); SrcLo += FOURIER_VSTRIDE;
			b = FOURIER_VLOAD(SrcHi); SrcHi += FOURIER_VSTRIDE;
			FOURIER_VINTERLEAVE(a, b, &a, &b);
			FOURIER_VSTORE(Dst+0,               a);
			FOURIER_VSTORE(Dst+FOURIER_VSTRIDE, b); Dst += 2*FOURIER_VSTRIDE;
		}
#else
		float a, b;
		for(i=0;i<N/2;i++) {
			a = *SrcLo++;
			b = *SrcHi++;
			*Dst++ = a;
			*Dst++ = b;
		}
#endif
	}
}

/************************************************/
// EOF
/************************************************/
