/************************************************/
#include "Fourier.h"
#include "Fourier_Helper.h"
/************************************************/

#define FUNC_ARGS_LIST float *MDCT, float *MDST, const float *New, float *Lap, float *BufTmp, int N, int Overlap

/************************************************/

// Declare specialized routines
void Fourier_MDCT_MDST_Generic(FUNC_ARGS_LIST);
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
void Fourier_MDCT_MDST_SSE    (FUNC_ARGS_LIST);
#  ifdef FOURIER_ALLOW_FMA
void Fourier_MDCT_MDST_SSE_FMA(FUNC_ARGS_LIST);
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
void Fourier_MDCT_MDST_AVX    (FUNC_ARGS_LIST);
#  ifdef FOURIER_ALLOW_FMA
void Fourier_MDCT_MDST_AVX_FMA(FUNC_ARGS_LIST);
#  endif
# endif
#endif

/************************************************/

static const struct Fourier_FuncTbl_t DispatchTbl_MDCT = {
	.Generic = Fourier_MDCT_MDST_Generic,
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
	.SSE = Fourier_MDCT_MDST_SSE,
#  ifdef FOURIER_ALLOW_FMA
	.SSE_FMA = Fourier_MDCT_MDST_SSE_FMA,
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
	.AVX = Fourier_MDCT_MDST_AVX,
#  ifdef FOURIER_ALLOW_FMA
	.AVX_FMA = Fourier_MDCT_MDST_AVX_FMA,
#  endif
# endif
#endif
};

/************************************************/

typedef void (*Dispatcher_MDCT_t)(FUNC_ARGS_LIST);
static void InitDispatcher_MDCT(FUNC_ARGS_LIST);

static Dispatcher_MDCT_t Dispatcher_MDCT = InitDispatcher_MDCT;

static void InitDispatcher_MDCT(FUNC_ARGS_LIST) {
	Dispatcher_MDCT = (Dispatcher_MDCT_t)Fourier_GetDispatchFnc(&DispatchTbl_MDCT);
	Dispatcher_MDCT(MDCT, MDST, New, Lap, BufTmp, N, Overlap);
}

/************************************************/

void Fourier_MDCT_MDST(FUNC_ARGS_LIST) {
	Dispatcher_MDCT(MDCT, MDST, New, Lap, BufTmp, N, Overlap);
}

/************************************************/

#include "Fourier_MDCT_Template.h"
void Fourier_MDCT_MDST_Generic(FUNC_ARGS_LIST) {
	Fourier_MDCT_MDST_Template(MDCT, MDST, New, Lap, BufTmp, N, Overlap);
}

#undef FUNC_ARGS_LIST

/************************************************/
// EOF
/************************************************/
