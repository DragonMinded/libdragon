/************************************************/
#include "Fourier.h"
#include "Fourier_Helper.h"
/************************************************/

#define FUNC_ARGS_DECL (float *Dst, const float *Src, const float *Amplitude, float DryLevel, int N)
#define FUNC_ARGS      (Dst, Src, Amplitude, DryLevel, N)

/************************************************/

// Declare specialized routines
void Fourier_ApplyAmplitude_Generic FUNC_ARGS_DECL;
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
void Fourier_ApplyAmplitude_SSE     FUNC_ARGS_DECL;
#  ifdef FOURIER_ALLOW_FMA
void Fourier_ApplyAmplitude_SSE_FMA FUNC_ARGS_DECL;
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
void Fourier_ApplyAmplitude_AVX     FUNC_ARGS_DECL;
#  ifdef FOURIER_ALLOW_FMA
void Fourier_ApplyAmplitude_AVX_FMA FUNC_ARGS_DECL;
#  endif
# endif
#endif

/************************************************/

static const struct Fourier_FuncTbl_t DispatchTbl_ApplyAmplitude = {
	.Generic = Fourier_ApplyAmplitude_Generic,
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
	.SSE = Fourier_ApplyAmplitude_SSE,
#  ifdef FOURIER_ALLOW_FMA
	.SSE_FMA = Fourier_ApplyAmplitude_SSE_FMA,
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
	.AVX = Fourier_ApplyAmplitude_AVX,
#  ifdef FOURIER_ALLOW_FMA
	.AVX_FMA = Fourier_ApplyAmplitude_AVX_FMA,
#  endif
# endif
#endif
};

/************************************************/

typedef void (*Dispatcher_ApplyAmplitude_t) FUNC_ARGS_DECL;
static void InitDispatcher_ApplyAmplitude FUNC_ARGS_DECL;

static Dispatcher_ApplyAmplitude_t Dispatcher_ApplyAmplitude = InitDispatcher_ApplyAmplitude;

static void InitDispatcher_ApplyAmplitude FUNC_ARGS_DECL {
	Dispatcher_ApplyAmplitude = (Dispatcher_ApplyAmplitude_t)Fourier_GetDispatchFnc(&DispatchTbl_ApplyAmplitude);
	Dispatcher_ApplyAmplitude FUNC_ARGS;
}

/************************************************/

void Fourier_ApplyAmplitude FUNC_ARGS_DECL {
	Dispatcher_ApplyAmplitude FUNC_ARGS;
}

/************************************************/

#include "Fourier_ApplyAmplitude_Template.h"
void Fourier_ApplyAmplitude_Generic FUNC_ARGS_DECL {
	Fourier_ApplyAmplitude_Template FUNC_ARGS;
}

#undef FUNC_ARGS_DECL
#undef FUNC_ARGS

/************************************************/
// EOF
/************************************************/
