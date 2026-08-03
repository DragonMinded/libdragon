/************************************************/
#include "Fourier.h"
#include "Fourier_Helper.h"
/************************************************/

#define AMPLITUDE_MAX_RATIO 1.0e10f

/************************************************/

FOURIER_FORCE_INLINE
void Fourier_ApplyAmplitude_Template(float *Dst, const float *Src, const float *Amplitude, float DryLevel, int N) {
	int n;
	FOURIER_ASSUME_ALIGNED(Dst, FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(Src, FOURIER_ALIGNMENT);
	FOURIER_ASSUME_ALIGNED(Amplitude, FOURIER_ALIGNMENT);
	FOURIER_ASSUME(N >= FOURIER_VSTRIDE);

	//! Because we will end up using all eight SSE
	//! registers for this, the code is written in
	//! a way that the compiler should never spill
	//! anything to the stack.
#if FOURIER_VSTRIDE > 1
	Fourier_Vec_t x0, x1, x2, x3, x4, x5, x6, x7 = FOURIER_VSET1(DryLevel);
	for(n=0;n<N;n+=FOURIER_VSTRIDE) {
		x0 = FOURIER_VLOAD(Src);
		x1 = FOURIER_VLOAD(Src+FOURIER_VSTRIDE);
		FOURIER_VSPLIT_EVEN_ODD(x0, x1, &x2, &x3); // x2 = Re, x3 = Im
		x2 = FOURIER_VMUL(x2, x2);
		x2 = FOURIER_VFMA(x3, x3, x2);
		x2 = FOURIER_VSQRT(x2); // Amp = sqrt(Re^2 + Im^2)
		x3 = FOURIER_VFMA(x2, x7, FOURIER_VLOAD(Amplitude)); // NewAmp = Amp*DryLevel + Amplitude
		x4 = FOURIER_VDIV(x3, x2); // Ratio = NewAmp / Amp
		x5 = FOURIER_VMUL(x2, FOURIER_VSET1(AMPLITUDE_MAX_RATIO)); // NewAmp < Amp*MAXRATIO
		x5 = FOURIER_VCMPLT(x3, x5);
		x6 = FOURIER_VMUL(x3, FOURIER_VSET1(AMPLITUDE_MAX_RATIO)); // Amp < NewAmp*MAXRATIO
		x6 = FOURIER_VCMPLT(x2, x6);
		x4 = FOURIER_VAND(x4, x5);
		x4 = FOURIER_VAND(x4, x6);
		FOURIER_VINTERLEAVE(x4, x4, &x4, &x5);
		x0 = FOURIER_VMUL(x0, x4); // Input *= Ratio
		x1 = FOURIER_VMUL(x1, x5);
		FOURIER_VSTORE(Dst,                 x0);
		FOURIER_VSTORE(Dst+FOURIER_VSTRIDE, x1);
		Dst       += FOURIER_VSTRIDE*2;
		Src       += FOURIER_VSTRIDE*2;
		Amplitude += FOURIER_VSTRIDE;
	}
#else
	for(n=0;n<N;n++) {
		float Re     = Src[n*2+0];
		float Im     = Src[n*2+1];
		float Amp    = sqrtf(Re*Re + Im*Im);
		float NewAmp = Amp*DryLevel + Amplitude[n];
		float Ratio;
		if(NewAmp < AMPLITUDE_MAX_RATIO*Amp && Amp < NewAmp*AMPLITUDE_MAX_RATIO) {
			Ratio = NewAmp / Amp;
		} else Ratio = 0.0f;
		Dst[n*2+0] = Re * Ratio;
		Dst[n*2+1] = Im * Ratio;
	}
#endif
}

/************************************************/
// EOF
/************************************************/
