/************************************************/
#include "Fourier.h"
#include "Fourier_Helper.h"
/************************************************/

// Declare specialized routines
void Fourier_DCT4_Generic(float *Buf, float *Tmp, int N);
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
void Fourier_DCT4_SSE    (float *Buf, float *Tmp, int N);
#  ifdef FOURIER_ALLOW_FMA
void Fourier_DCT4_SSE_FMA(float *Buf, float *Tmp, int N);
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
void Fourier_DCT4_AVX    (float *Buf, float *Tmp, int N);
#  ifdef FOURIER_ALLOW_FMA
void Fourier_DCT4_AVX_FMA(float *Buf, float *Tmp, int N);
#  endif
# endif
#endif

/************************************************/

static const struct Fourier_FuncTbl_t DispatchTbl_DCT4 = {
	.Generic = Fourier_DCT4_Generic,
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
	.SSE = Fourier_DCT4_SSE,
#  ifdef FOURIER_ALLOW_FMA
	.SSE_FMA = Fourier_DCT4_SSE_FMA,
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
	.AVX = Fourier_DCT4_AVX,
#  ifdef FOURIER_ALLOW_FMA
	.AVX_FMA = Fourier_DCT4_AVX_FMA,
#  endif
# endif
#endif
};

/************************************************/

typedef void (*Dispatcher_DCT4_t)(float *Buf, float *Tmp, int N);
static void InitDispatcher_DCT4(float *Buf, float *Tmp, int N);

static Dispatcher_DCT4_t Dispatcher_DCT4 = InitDispatcher_DCT4;

static void InitDispatcher_DCT4(float *Buf, float *Tmp, int N) {
	Dispatcher_DCT4 = (Dispatcher_DCT4_t)Fourier_GetDispatchFnc(&DispatchTbl_DCT4);
	Dispatcher_DCT4(Buf, Tmp, N);
}

/************************************************/

void Fourier_DCT4(float *Buf, float *Tmp, int N) {
	Dispatcher_DCT4(Buf, Tmp, N);
}

/************************************************/

// DCT-IV (N=8)
void Fourier_DCT4_8(float *x) {
	const float sqrt1_2 = 0x1.6A09E6p-1f;
	const float c1_3 = 0x1.D906BCp-1f, s1_3 = 0x1.87DE2Ap-2f;
	const float c1_5 = 0x1.FD88DAp-1f, s1_5 = 0x1.917A6Cp-4f;
	const float c3_5 = 0x1.E9F416p-1f, s3_5 = 0x1.294062p-2f;
	const float c5_5 = 0x1.C38B30p-1f, s5_5 = 0x1.E2B5D4p-2f;
	const float c7_5 = 0x1.8BC806p-1f, s7_5 = 0x1.44CF32p-1f;

	// First stage (rotation butterflies; DCT4_8)
	float ax =  c1_5*x[0] + s1_5*x[7];
	float ay =  s1_5*x[0] - c1_5*x[7];
	float bx =  c3_5*x[1] + s3_5*x[6];
	float by = -s3_5*x[1] + c3_5*x[6];
	float cx =  c5_5*x[2] + s5_5*x[5];
	float cy =  s5_5*x[2] - c5_5*x[5];
	float dx =  c7_5*x[3] + s7_5*x[4];
	float dy = -s7_5*x[3] + c7_5*x[4];

	// Second stage (butterflies; DCT2_4)
	float saxdx = ax + dx;
	float daxdx = ax - dx;
	float sbxcx = bx + cx;
	float dbxcx = bx - cx;
	float sdyay = dy + ay;
	float ddyay = dy - ay;
	float scyby = cy + by;
	float dcyby = cy - by;

	// Third stage (rotation butterflies; DCT2_2, DCT4_2)
	float sx =      saxdx +      sbxcx;
	float sy =      saxdx -      sbxcx;
	float tx = c1_3*daxdx + s1_3*dbxcx;
	float ty = s1_3*daxdx - c1_3*dbxcx;
	float ux =      sdyay +      scyby;
	float uy =      sdyay -      scyby;
	float vx = c1_3*ddyay + s1_3*dcyby;
	float vy = s1_3*ddyay - c1_3*dcyby;

	// Permute and final DCT4 stage
	x[0] = sx;
	x[1] = (tx - vy);
	x[2] = (tx + vy);
	x[3] = (sy + uy) * sqrt1_2;
	x[4] = (sy - uy) * sqrt1_2;
	x[5] = (ty - vx);
	x[6] = (ty + vx);
	x[7] = ux;
}

/************************************************/

#include "Fourier_DCT4_Template.h"
void Fourier_DCT4_Generic(float *Buf, float *Tmp, int N) {
	Fourier_DCT4_Template(Buf, Tmp, N);
}

/************************************************/
// EOF
/************************************************/
