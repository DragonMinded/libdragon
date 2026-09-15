/************************************************/
#include "Fourier.h"
#include "Fourier_Helper.h"
/************************************************/

// Declare specialized routines
void Fourier_FFTReCenter_Generic (float *Buf, float *Tmp, int N);
void Fourier_iFFTReCenter_Generic(float *Buf, float *Tmp, int N);
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
void Fourier_FFTReCenter_SSE     (float *Buf, float *Tmp, int N);
void Fourier_iFFTReCenter_SSE    (float *Buf, float *Tmp, int N);
#  ifdef FOURIER_ALLOW_FMA
void Fourier_FFTReCenter_SSE_FMA (float *Buf, float *Tmp, int N);
void Fourier_iFFTReCenter_SSE_FMA(float *Buf, float *Tmp, int N);
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
void Fourier_FFTReCenter_AVX     (float *Buf, float *Tmp, int N);
void Fourier_iFFTReCenter_AVX    (float *Buf, float *Tmp, int N);
#  ifdef FOURIER_ALLOW_FMA
void Fourier_FFTReCenter_AVX_FMA (float *Buf, float *Tmp, int N);
void Fourier_iFFTReCenter_AVX_FMA(float *Buf, float *Tmp, int N);
#  endif
# endif
#endif

/************************************************/

static const struct Fourier_FuncTbl_t FFT_DispatchTbl = {
	.Generic = Fourier_FFTReCenter_Generic,
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
	.SSE = Fourier_FFTReCenter_SSE,
#  ifdef FOURIER_ALLOW_FMA
	.SSE_FMA = Fourier_FFTReCenter_SSE_FMA,
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
	.AVX = Fourier_FFTReCenter_AVX,
#  ifdef FOURIER_ALLOW_FMA
	.AVX_FMA = Fourier_FFTReCenter_AVX_FMA,
#  endif
# endif
#endif
};

static const struct Fourier_FuncTbl_t iFFT_DispatchTbl = {
	.Generic = Fourier_iFFTReCenter_Generic,
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
	.SSE = Fourier_iFFTReCenter_SSE,
#  ifdef FOURIER_ALLOW_FMA
	.SSE_FMA = Fourier_iFFTReCenter_SSE_FMA,
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
	.AVX = Fourier_iFFTReCenter_AVX,
#  ifdef FOURIER_ALLOW_FMA
	.AVX_FMA = Fourier_iFFTReCenter_AVX_FMA,
#  endif
# endif
#endif
};

/************************************************/

typedef void (*Dispatcher_FFT_t)(float *Buf, float *Tmp, int N);
static void InitDispatcher_FFT (float *Buf, float *Tmp, int N);
static void InitDispatcher_iFFT(float *Buf, float *Tmp, int N);

static Dispatcher_FFT_t Dispatcher_FFT  = InitDispatcher_FFT;
static Dispatcher_FFT_t Dispatcher_iFFT = InitDispatcher_iFFT;

static void InitDispatcher_FFT(float *Buf, float *Tmp, int N) {
	Dispatcher_FFT = (Dispatcher_FFT_t)Fourier_GetDispatchFnc(&FFT_DispatchTbl);
	Dispatcher_FFT(Buf, Tmp, N);
}

static void InitDispatcher_iFFT(float *Buf, float *Tmp, int N) {
	Dispatcher_iFFT = (Dispatcher_FFT_t)Fourier_GetDispatchFnc(&iFFT_DispatchTbl);
	Dispatcher_iFFT(Buf, Tmp, N);
}

/************************************************/

void Fourier_FFTReCenter(float *Buf, float *Tmp, int N) {
	Dispatcher_FFT(Buf, Tmp, N);
}
void Fourier_iFFTReCenter(float *Buf, float *Tmp, int N) {
	Dispatcher_iFFT(Buf, Tmp, N);
}

/************************************************/

#include "Fourier_CenterFFT_Template.h"
void Fourier_FFTReCenter_Generic(float *Buf, float *Tmp, int N) {
	Fourier_FFTReCenter_Template(Buf, Tmp, N);
}
void Fourier_iFFTReCenter_Generic(float *Buf, float *Tmp, int N) {
	Fourier_iFFTReCenter_Template(Buf, Tmp, N);
}

/************************************************/
// EOF
/************************************************/
