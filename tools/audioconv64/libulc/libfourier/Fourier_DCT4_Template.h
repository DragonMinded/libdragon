/************************************************/
#include "Fourier.h"
#include "Fourier_Helper.h"
/************************************************/

FOURIER_FORCE_INLINE
void Fourier_DCT4_Template(float *Buf, float *Tmp, int N) {
	int i;
	FOURIER_ASSUME_ALIGNED(Buf, FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(Tmp, FOURIER_ALIGNMENT);
	FOURIER_ASSUME(N >= 8);

	// Stop condition
	if(N == 8) {
		Fourier_DCT4_8(Buf);
		return;
	}

	// Perform rotation butterflies
	//  u = R_n.x
	{
		const float *SrcLo = Buf;
		const float *SrcHi = Buf + N;
		      float *DstLo = Tmp;
		      float *DstHi = Tmp + N/2;
#if FOURIER_VSTRIDE > 1
		Fourier_Vec_t a, b;
		Fourier_Vec_t t0, t1 = FOURIER_VMUL(FOURIER_VSET1(1.0f/N), FOURIER_VADD(FOURIER_VSET_LINEAR_RAMP(), FOURIER_VSET1(0.5f)));
		Fourier_Vec_t c  = Fourier_Cos(t1);
		Fourier_Vec_t s  = Fourier_Sin(t1);
		Fourier_Vec_t wc = Fourier_Cos(FOURIER_VSET1((float)FOURIER_VSTRIDE / N));
		Fourier_Vec_t ws = Fourier_Sin(FOURIER_VSET1((float)FOURIER_VSTRIDE / N));
		for(i=0;i<N/2;i+=FOURIER_VSTRIDE) {
			SrcHi -= FOURIER_VSTRIDE; b = FOURIER_VREVERSE(FOURIER_VLOAD(SrcHi));
			a = FOURIER_VLOAD(SrcLo); SrcLo += FOURIER_VSTRIDE;
			t1 = FOURIER_VMUL(s, a);
			t0 = FOURIER_VMUL(c, a);
			t1 = FOURIER_VNFMA(c, b, t1);
			t0 = FOURIER_VFMA (s, b, t0);
			t1 = FOURIER_VNEGATE_ODD(t1);
			FOURIER_VSTORE(DstLo, t0); DstLo += FOURIER_VSTRIDE;
			FOURIER_VSTORE(DstHi, t1); DstHi += FOURIER_VSTRIDE;
			t0 = c;
			t1 = s;
			c = FOURIER_VNFMA(t1, ws, FOURIER_VMUL(t0, wc));
			s = FOURIER_VFMA (t1, wc, FOURIER_VMUL(t0, ws));
		}
#else
		float a, b;
		float c  = Fourier_Cos(0.5f / N);
		float s  = Fourier_Sin(0.5f / N);
		float wc = Fourier_Cos(1.0f / N);
		float ws = Fourier_Sin(1.0f / N);
		for(i=0;i<N/2;i+=2) {
			a = *SrcLo++;
			b = *--SrcHi;
			*DstLo++ =  c*a + s*b;
			*DstHi++ =  s*a - c*b;
			a = c;
			b = s;
			c = wc*a - ws*b;
			s = ws*a + wc*b;

			a = *SrcLo++;
			b = *--SrcHi;
			*DstLo++ =  c*a + s*b;
			*DstHi++ = -s*a + c*b; // <- Sign-flip for DST
			a = c;
			b = s;
			c = wc*a - ws*b;
			s = ws*a + wc*b;
		}
#endif
	}

	// Perform recursion
	//  z1 = cos2([u_j][j=0..n/2-1],n/2)
	//  z2 = cos2([u_j][j=n/2..n-1],n/2)
	Fourier_DCT2(Tmp,       Buf,       N/2);
	Fourier_DCT2(Tmp + N/2, Buf + N/2, N/2);

	// Combine
	//  w = U_n.(z1^T, z2^T)^T
	//  y = (P_n)^T.w
	{
		const float *TmpLo = Tmp;
		const float *TmpHi = Tmp + N;
		      float *Dst   = Buf;
		*Dst++ = *TmpLo++;
#if FOURIER_VSTRIDE > 1
		{
			Fourier_Vec_t a, b;
			Fourier_Vec_t t0, t1;
			for(i=0;i<N/2-FOURIER_VSTRIDE;i+=FOURIER_VSTRIDE) {
				TmpHi -= FOURIER_VSTRIDE; b = FOURIER_VREVERSE(FOURIER_VLOAD(TmpHi));
				a = FOURIER_VLOADU(TmpLo); TmpLo += FOURIER_VSTRIDE;
				t0 = FOURIER_VADD(a, b);
				t1 = FOURIER_VSUB(a, b);
				FOURIER_VINTERLEAVE(t0, t1, &a, &b);
				FOURIER_VSTOREU(Dst, a); Dst += FOURIER_VSTRIDE;
				FOURIER_VSTOREU(Dst, b); Dst += FOURIER_VSTRIDE;
			}
		}
#else
		i = 0;
#endif
		float a, b;
		for(;i<N/2-1;i++) {
			a = *TmpLo++;
			b = *--TmpHi;
			*Dst++ = a + b;
			*Dst++ = a - b;
		}
		*Dst++ = *--TmpHi;
	}
}

/************************************************/
// EOF
/************************************************/
