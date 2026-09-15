/************************************************/
#include "Fourier.h"
#include "Fourier_Helper.h"
/************************************************/

FOURIER_FORCE_INLINE
void Fourier_FFTReCenter_Template(float *Buf, float *Tmp, int N) {
	int n;
	float *SrcA, *SrcB, *Dst;
	FOURIER_ASSUME_ALIGNED(Buf, FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(Tmp, FOURIER_ALIGNMENT);
	FOURIER_ASSUME(N >= FOURIER_VSTRIDE*2);

	// Compute Re -> Tmp[], Im -> Tmp[]+N/2
	SrcA = Buf + N/2;
	SrcB = Buf + N/2;
	Dst  = Tmp;
#if FOURIER_VSTRIDE > 1
	for(n=0;n<N/2;n+=FOURIER_VSTRIDE) {
		Fourier_Vec_t a, b;
		a = FOURIER_VLOAD(SrcA); SrcA += FOURIER_VSTRIDE;
		SrcB -= FOURIER_VSTRIDE; b = FOURIER_VREVERSE(FOURIER_VLOAD(SrcB));
		Fourier_Vec_t l = FOURIER_VADD(a, b);
		Fourier_Vec_t h = FOURIER_VSUB(a, b);
		              h = FOURIER_VNEGATE_ODD(h);
		FOURIER_VSTORE(Dst,     l);
		FOURIER_VSTORE(Dst+N/2, h);
		Dst += FOURIER_VSTRIDE;
	}
#else
	for(n=0;n<N/2;n+=2) {
		float a, b;
		a = *SrcA++, b = *--SrcB;
		Dst[N/2] = a - b;
		*Dst++   = a + b;
		a = *SrcA++, b = *--SrcB;
		Dst[N/2] = b - a; // <- Sign-flip for DST
		*Dst++   = a + b;
	}
#endif
	Fourier_DCT4(Tmp,     Buf, N/2);
	Fourier_DCT4(Tmp+N/2, Buf, N/2);

	// Reverse Im for DST, interleave {Re,Im}
	SrcA = Tmp;
	SrcB = Tmp + N;
	Dst  = Buf;
#if FOURIER_VSTRIDE > 1
	for(n=0;n<N/2;n+=FOURIER_VSTRIDE) {
		Fourier_Vec_t l, h;
		l = FOURIER_VLOAD(SrcA); SrcA += FOURIER_VSTRIDE;
		SrcB -= FOURIER_VSTRIDE; h = FOURIER_VREVERSE(FOURIER_VLOAD(SrcB));
		FOURIER_VINTERLEAVE(l, h, &l, &h);
		FOURIER_VSTORE(Dst+0,               l);
		FOURIER_VSTORE(Dst+FOURIER_VSTRIDE, h);
		Dst += 2*FOURIER_VSTRIDE;
	}
#else
	for(n=0;n<N/2;n++) {
		*Dst++ = *SrcA++;
		*Dst++ = *--SrcB;
	}
#endif
}

/************************************************/

FOURIER_FORCE_INLINE
void Fourier_iFFTReCenter_Template(float *Buf, float *Tmp, int N) {
	int n;
	float *DstA, *DstB, *Src;
	FOURIER_ASSUME_ALIGNED(Buf, FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(Tmp, FOURIER_ALIGNMENT);
	FOURIER_ASSUME(N >= FOURIER_VSTRIDE*2);

	// Deinterleave {Re,Im} and transform to xe,xo
	DstA = Tmp;
	DstB = Tmp + N;
	Src  = Buf;
#if FOURIER_VSTRIDE > 1
	for(n=0;n<N/2;n+=FOURIER_VSTRIDE) {
		Fourier_Vec_t a = FOURIER_VLOAD(Src+0);
		Fourier_Vec_t b = FOURIER_VLOAD(Src+FOURIER_VSTRIDE); Src += FOURIER_VSTRIDE*2;
		FOURIER_VSPLIT_EVEN_ODDREV(a, b, &a, &b);
		FOURIER_VSTORE(DstA, a); DstA += FOURIER_VSTRIDE;
		DstB -= FOURIER_VSTRIDE; FOURIER_VSTORE(DstB, b);
	}
#else
	for(n=0;n<N/2;n++) {
		*DstA++ = *Src++;
		*--DstB = *Src++;
	}
#endif
	Fourier_DCT4(Tmp,     Buf, N/2);
	Fourier_DCT4(Tmp+N/2, Buf, N/2);

	// Combine xe,xo
	DstA = Buf + N/2;
	DstB = Buf + N/2;
	Src  = Tmp;
#if FOURIER_VSTRIDE > 1
	for(n=0;n<N/2;n+=FOURIER_VSTRIDE) {
		Fourier_Vec_t a = FOURIER_VLOAD(Src+0);
		Fourier_Vec_t b = FOURIER_VLOAD(Src+N/2); Src += FOURIER_VSTRIDE;
		              b = FOURIER_VNEGATE_ODD(b);
		Fourier_Vec_t l = FOURIER_VADD(a, b);
		Fourier_Vec_t h = FOURIER_VSUB(a, b);
		              h = FOURIER_VREVERSE(h);
		FOURIER_VSTORE(DstA, l); DstA += FOURIER_VSTRIDE;
		DstB -= FOURIER_VSTRIDE; FOURIER_VSTORE(DstB, h);
	}
#else
	for(n=0;n<N/2;n+=2) {
		float a, b;
		b = Src[N/2], a = *Src++;
		*DstA++ = a + b;
		*--DstB = a - b;
		b = Src[N/2], a = *Src++;
		*DstA++ = a - b; // <- Sign-flip for DST
		*--DstB = a + b; // <- Sign-flip for DST
	}
#endif
}

/************************************************/
// EOF
/************************************************/
