/************************************************/
#include "Fourier.h"
#include "Fourier_Helper.h"
/************************************************/

#define FUNC_ARGS_LIST float *BufOut, const float *BufIn, float *BufLap, float *BufTmp, int N, int Overlap

/************************************************/

// Declare specialized routines
void Fourier_IMDCT_Generic(FUNC_ARGS_LIST);
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
void Fourier_IMDCT_SSE    (FUNC_ARGS_LIST);
#  ifdef FOURIER_ALLOW_FMA
void Fourier_IMDCT_SSE_FMA(FUNC_ARGS_LIST);
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
void Fourier_IMDCT_AVX    (FUNC_ARGS_LIST);
#  ifdef FOURIER_ALLOW_FMA
void Fourier_IMDCT_AVX_FMA(FUNC_ARGS_LIST);
#  endif
# endif
#endif

/************************************************/

static const struct Fourier_FuncTbl_t DispatchTbl_IMDCT = {
	.Generic = Fourier_IMDCT_Generic,
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
	.SSE = Fourier_IMDCT_SSE,
#  ifdef FOURIER_ALLOW_FMA
	.SSE_FMA = Fourier_IMDCT_SSE_FMA,
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
	.AVX = Fourier_IMDCT_AVX,
#  ifdef FOURIER_ALLOW_FMA
	.AVX_FMA = Fourier_IMDCT_AVX_FMA,
#  endif
# endif
#endif
};

/************************************************/

typedef void (*Dispatcher_IMDCT_t)(FUNC_ARGS_LIST);
static void InitDispatcher_IMDCT(FUNC_ARGS_LIST);

static Dispatcher_IMDCT_t Dispatcher_IMDCT = InitDispatcher_IMDCT;

static void InitDispatcher_IMDCT(FUNC_ARGS_LIST) {
	Dispatcher_IMDCT = (Dispatcher_IMDCT_t)Fourier_GetDispatchFnc(&DispatchTbl_IMDCT);
	Dispatcher_IMDCT(BufOut, BufIn, BufLap, BufTmp, N, Overlap);
}

/************************************************/

void Fourier_IMDCT(FUNC_ARGS_LIST) {
	Dispatcher_IMDCT(BufOut, BufIn, BufLap, BufTmp, N, Overlap);
}

/************************************************/

#include "Fourier_IMDCT_Template.h"
void Fourier_IMDCT_Generic(FUNC_ARGS_LIST) {
	Fourier_IMDCT_Template(BufOut, BufIn, BufLap, BufTmp, N, Overlap);
}

#undef FUNC_ARGS_LIST

/************************************************/
// EOF
/************************************************/
