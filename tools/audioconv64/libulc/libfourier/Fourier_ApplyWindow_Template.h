/************************************************/
#include "Fourier.h"
#include "Fourier_Helper.h"
/************************************************/

FOURIER_FORCE_INLINE
void Fourier_ApplyWindow_Template(float *Dst, const float *Src, const float *Window, int N) {
	int n;
	FOURIER_ASSUME_ALIGNED(Dst, FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(Src, FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(Window, FOURIER_ALIGNMENT);
	FOURIER_ASSUME(N >= FOURIER_VSTRIDE);
	for(n=0;n<N;n+=FOURIER_VSTRIDE) {
		Fourier_Vec_t x, w;
		x = FOURIER_VLOAD(Src);
		w = FOURIER_VLOAD(Window);
		x = FOURIER_VMUL(x, w);
		FOURIER_VSTORE(Dst, x);
		Src    += FOURIER_VSTRIDE;
		Dst    += FOURIER_VSTRIDE;
		Window += FOURIER_VSTRIDE;
	}
}

FOURIER_FORCE_INLINE
void Fourier_OverlapWindow_Template(float *Dst, const float *Src, const float *Window, int N) {
	int n;
	FOURIER_ASSUME_ALIGNED(Dst, FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(Src, FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(Window, FOURIER_ALIGNMENT);
	FOURIER_ASSUME(N >= FOURIER_VSTRIDE);
	for(n=0;n<N;n+=FOURIER_VSTRIDE) {
		Fourier_Vec_t x, a, w;
		x = FOURIER_VLOAD(Src);
		a = FOURIER_VLOAD(Dst);
		w = FOURIER_VLOAD(Window);
		a = FOURIER_VFMA(x, w, a);
		FOURIER_VSTORE(Dst, a);
		Src    += FOURIER_VSTRIDE;
		Dst    += FOURIER_VSTRIDE;
		Window += FOURIER_VSTRIDE;
	}
}

/************************************************/

FOURIER_FORCE_INLINE
void Fourier_ApplySymmetricWindow_Template(float *Dst, const float *Src, const float *Window, int N) {
	int n;
	      float *DstA, *DstB;
	const float *SrcA, *SrcB;
	FOURIER_ASSUME_ALIGNED(Dst, FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(Src, FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(Window, FOURIER_ALIGNMENT);
	FOURIER_ASSUME(N >= FOURIER_VSTRIDE*2);
	SrcA = Src;
	SrcB = Src + N;
	DstA = Dst;
	DstB = Dst + N;
	for(n=0;n<N/2;n+=FOURIER_VSTRIDE) {
		Fourier_Vec_t x0, x1, w;
		SrcB -= FOURIER_VSTRIDE;
		DstB -= FOURIER_VSTRIDE;
		x0 = FOURIER_VLOAD(SrcA);
		x1 = FOURIER_VLOAD(SrcB);
		w  = FOURIER_VLOAD(Window);
		x0 = FOURIER_VMUL(x0, w);
		w  = FOURIER_VREVERSE(w);
		x1 = FOURIER_VMUL(x1, w);
		FOURIER_VSTORE(DstA, x0);
		FOURIER_VSTORE(DstB, x1);
		SrcA   += FOURIER_VSTRIDE;
		DstA   += FOURIER_VSTRIDE;
		Window += FOURIER_VSTRIDE;
	}
}

FOURIER_FORCE_INLINE
void Fourier_OverlapSymmetricWindow_Template(float *Dst, const float *Src, const float *Window, int N) {
	int n;
	      float *DstA, *DstB;
	const float *SrcA, *SrcB;
	FOURIER_ASSUME_ALIGNED(Dst, FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(Src, FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(Window, FOURIER_ALIGNMENT);
	FOURIER_ASSUME(N >= FOURIER_VSTRIDE*2);
	SrcA = Src;
	SrcB = Src + N;
	DstA = Dst;
	DstB = Dst + N;
	for(n=0;n<N/2;n+=FOURIER_VSTRIDE) {
		Fourier_Vec_t x0, x1, a0, a1, w;
		SrcB -= FOURIER_VSTRIDE;
		DstB -= FOURIER_VSTRIDE;
		x0 = FOURIER_VLOAD(SrcA);
		x1 = FOURIER_VLOAD(SrcB);
		a0 = FOURIER_VLOAD(DstA);
		a1 = FOURIER_VLOAD(DstB);
		w  = FOURIER_VLOAD(Window);
		a0 = FOURIER_VFMA(x0, w, a0);
		w  = FOURIER_VREVERSE(w);
		a1 = FOURIER_VFMA(x1, w, a1);
		FOURIER_VSTORE(DstA, a0);
		FOURIER_VSTORE(DstB, a1);
		SrcA   += FOURIER_VSTRIDE;
		DstA   += FOURIER_VSTRIDE;
		Window += FOURIER_VSTRIDE;
	}
}

/************************************************/
// EOF
/************************************************/
