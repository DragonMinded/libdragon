/************************************************/
#include "Fourier.h"
#include "Fourier_Helper.h"
/************************************************/

// Declare specialized routines
void Fourier_DCT2_Generic(float *Buf, float *Tmp, int N);
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
void Fourier_DCT2_SSE    (float *Buf, float *Tmp, int N);
#  ifdef FOURIER_ALLOW_FMA
void Fourier_DCT2_SSE_FMA(float *Buf, float *Tmp, int N);
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
void Fourier_DCT2_AVX    (float *Buf, float *Tmp, int N);
#  ifdef FOURIER_ALLOW_FMA
void Fourier_DCT2_AVX_FMA(float *Buf, float *Tmp, int N);
#  endif
# endif
#endif

/************************************************/

static const struct Fourier_FuncTbl_t DispatchTbl_DCT2 = {
	.Generic = Fourier_DCT2_Generic,
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
	.SSE = Fourier_DCT2_SSE,
#  ifdef FOURIER_ALLOW_FMA
	.SSE_FMA = Fourier_DCT2_SSE_FMA,
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
	.AVX = Fourier_DCT2_AVX,
#  ifdef FOURIER_ALLOW_FMA
	.AVX_FMA = Fourier_DCT2_AVX_FMA,
#  endif
# endif
#endif
};

/************************************************/

typedef void (*Dispatcher_DCT2_t)(float *Buf, float *Tmp, int N);
static void InitDispatcher_DCT2(float *Buf, float *Tmp, int N);

static Dispatcher_DCT2_t Dispatcher_DCT2 = InitDispatcher_DCT2;

static void InitDispatcher_DCT2(float *Buf, float *Tmp, int N) {
	Dispatcher_DCT2 = (Dispatcher_DCT2_t)Fourier_GetDispatchFnc(&DispatchTbl_DCT2);
	Dispatcher_DCT2(Buf, Tmp, N);
}

/************************************************/

void Fourier_DCT2(float *Buf, float *Tmp, int N) {
	Dispatcher_DCT2(Buf, Tmp, N);
}

/************************************************/

// DCT-II (N=8)
void Fourier_DCT2_8(float *x) {
	const float sqrt1_2 = 0x1.6A09E6p-1f;
	const float c1_4 = 0x1.F6297Cp-1f, s1_4 = 0x1.8F8B84p-3f;
	const float c3_4 = 0x1.A9B662p-1f, s3_4 = 0x1.1C73B4p-1f;
	const float c6_4 = 0x1.87DE2Ap-2f, s6_4 = 0x1.D906BCp-1f;

	// First stage butterflies (DCT2_8)
	float s07 = x[0]+x[7];
	float d07 = x[0]-x[7];
	float s16 = x[1]+x[6];
	float d16 = x[1]-x[6];
	float s25 = x[2]+x[5];
	float d25 = x[2]-x[5];
	float s34 = x[3]+x[4];
	float d34 = x[3]-x[4];

	// Second stage (DCT2_4, DCT4_4)
	float ss07s34 = s07+s34;
	float ds07s34 = s07-s34;
	float ss16s25 = s16+s25;
	float ds16s25 = s16-s25;
	float d34d07x =  c3_4*d34 + s3_4*d07;
	float d34d07y = -s3_4*d34 + c3_4*d07;
	float d25d16x =  c1_4*d25 + s1_4*d16;
	float d25d16y = -s1_4*d25 + c1_4*d16;

	// Third stage (rotation butterflies; DCT2_2, DCT4_2, DCT2_2, DCT2_2)
	float a0 =       ss07s34 +      ss16s25;
	float b0 =       ss07s34 -      ss16s25;
	float c0 =  c6_4*ds16s25 + s6_4*ds07s34;
	float d0 = -s6_4*ds16s25 + c6_4*ds07s34;
	float a1 =       d34d07y +      d25d16x;
	float c1 =       d34d07y -      d25d16x;
	float d1 =       d34d07x +      d25d16y;
	float b1 =       d34d07x -      d25d16y;

	// Permute and final DCT4 stage
	x[0] = a0;
	x[4] = b0 * sqrt1_2;
	x[2] = c0;
	x[6] = d0;
	x[1] = (a1 + d1) * sqrt1_2;
	x[5] = b1;
	x[3] = c1;
	x[7] = (a1 - d1) * sqrt1_2;
}

/************************************************/

#include "Fourier_DCT2_Template.h"
void Fourier_DCT2_Generic(float *Buf, float *Tmp, int N) {
	Fourier_DCT2_Template(Buf, Tmp, N);
}

/************************************************/
// EOF
/************************************************/
