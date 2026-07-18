/************************************************/
#include "Fourier.h"
#include "Fourier_Helper.h"
/************************************************/

#define FUNC_ARGS_DECL (float *Dst, const float *Src, const float *Window, int N)
#define FUNC_ARGS      (Dst, Src, Window, N)

/************************************************/

// Declare specialized routines
void Fourier_ApplyWindow_Generic            FUNC_ARGS_DECL;
void Fourier_OverlapWindow_Generic          FUNC_ARGS_DECL;
void Fourier_ApplySymmetricWindow_Generic   FUNC_ARGS_DECL;
void Fourier_OverlapSymmetricWindow_Generic FUNC_ARGS_DECL;
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
void Fourier_ApplyWindow_SSE                FUNC_ARGS_DECL;
void Fourier_OverlapWindow_SSE              FUNC_ARGS_DECL;
void Fourier_ApplySymmetricWindow_SSE       FUNC_ARGS_DECL;
void Fourier_OverlapSymmetricWindow_SSE     FUNC_ARGS_DECL;
#  ifdef FOURIER_ALLOW_FMA
void Fourier_ApplyWindow_SSE_FMA            FUNC_ARGS_DECL;
void Fourier_OverlapWindow_SSE_FMA          FUNC_ARGS_DECL;
void Fourier_ApplySymmetricWindow_SSE_FMA   FUNC_ARGS_DECL;
void Fourier_OverlapSymmetricWindow_SSE_FMA FUNC_ARGS_DECL;
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
void Fourier_ApplyWindow_AVX                FUNC_ARGS_DECL;
void Fourier_OverlapWindow_AVX              FUNC_ARGS_DECL;
void Fourier_ApplySymmetricWindow_AVX       FUNC_ARGS_DECL;
void Fourier_OverlapSymmetricWindow_AVX     FUNC_ARGS_DECL;
#  ifdef FOURIER_ALLOW_FMA
void Fourier_ApplyWindow_AVX_FMA            FUNC_ARGS_DECL;
void Fourier_OverlapWindow_AVX_FMA          FUNC_ARGS_DECL;
void Fourier_ApplySymmetricWindow_AVX_FMA   FUNC_ARGS_DECL;
void Fourier_OverlapSymmetricWindow_AVX_FMA FUNC_ARGS_DECL;
#  endif
# endif
#endif

/************************************************/

static const struct Fourier_FuncTbl_t ApplyWindow_DispatchTbl = {
	.Generic = Fourier_ApplyWindow_Generic,
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
	.SSE = Fourier_ApplyWindow_SSE,
#  ifdef FOURIER_ALLOW_FMA
	.SSE_FMA = Fourier_ApplyWindow_SSE_FMA,
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
	.AVX = Fourier_ApplyWindow_AVX,
#  ifdef FOURIER_ALLOW_FMA
	.AVX_FMA = Fourier_ApplyWindow_AVX_FMA,
#  endif
# endif
#endif
};

static const struct Fourier_FuncTbl_t OverlapWindow_DispatchTbl = {
	.Generic = Fourier_OverlapWindow_Generic,
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
	.SSE = Fourier_OverlapWindow_SSE,
#  ifdef FOURIER_ALLOW_FMA
	.SSE_FMA = Fourier_OverlapWindow_SSE_FMA,
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
	.AVX = Fourier_OverlapWindow_AVX,
#  ifdef FOURIER_ALLOW_FMA
	.AVX_FMA = Fourier_OverlapWindow_AVX_FMA,
#  endif
# endif
#endif
};

static const struct Fourier_FuncTbl_t ApplySymmetricWindow_DispatchTbl = {
	.Generic = Fourier_ApplySymmetricWindow_Generic,
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
	.SSE = Fourier_ApplySymmetricWindow_SSE,
#  ifdef FOURIER_ALLOW_FMA
	.SSE_FMA = Fourier_ApplySymmetricWindow_SSE_FMA,
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
	.AVX = Fourier_ApplySymmetricWindow_AVX,
#  ifdef FOURIER_ALLOW_FMA
	.AVX_FMA = Fourier_ApplySymmetricWindow_AVX_FMA,
#  endif
# endif
#endif
};

static const struct Fourier_FuncTbl_t OverlapSymmetricWindow_DispatchTbl = {
	.Generic = Fourier_OverlapSymmetricWindow_Generic,
#ifdef FOURIER_IS_X86
# ifdef FOURIER_ALLOW_SSE
	.SSE = Fourier_OverlapSymmetricWindow_SSE,
#  ifdef FOURIER_ALLOW_FMA
	.SSE_FMA = Fourier_OverlapSymmetricWindow_SSE_FMA,
#  endif
# endif
# ifdef FOURIER_ALLOW_AVX
	.AVX = Fourier_OverlapSymmetricWindow_AVX,
#  ifdef FOURIER_ALLOW_FMA
	.AVX_FMA = Fourier_OverlapSymmetricWindow_AVX_FMA,
#  endif
# endif
#endif
};

/************************************************/

typedef void (*Dispatcher_ApplyWindow_t) FUNC_ARGS_DECL;
static void InitDispatcher_ApplyWindow            FUNC_ARGS_DECL;
static void InitDispatcher_OverlapWindow          FUNC_ARGS_DECL;
static void InitDispatcher_ApplySymmetricWindow   FUNC_ARGS_DECL;
static void InitDispatcher_OverlapSymmetricWindow FUNC_ARGS_DECL;

static Dispatcher_ApplyWindow_t Dispatcher_ApplyWindow            = InitDispatcher_ApplyWindow;
static Dispatcher_ApplyWindow_t Dispatcher_OverlapWindow          = InitDispatcher_OverlapWindow;
static Dispatcher_ApplyWindow_t Dispatcher_ApplySymmetricWindow   = InitDispatcher_ApplySymmetricWindow;
static Dispatcher_ApplyWindow_t Dispatcher_OverlapSymmetricWindow = InitDispatcher_OverlapSymmetricWindow;

static void InitDispatcher_ApplyWindow FUNC_ARGS_DECL {
	Dispatcher_ApplyWindow = (Dispatcher_ApplyWindow_t)Fourier_GetDispatchFnc(&ApplyWindow_DispatchTbl);
	Dispatcher_ApplyWindow FUNC_ARGS;
}

static void InitDispatcher_OverlapWindow FUNC_ARGS_DECL {
	Dispatcher_OverlapWindow = (Dispatcher_ApplyWindow_t)Fourier_GetDispatchFnc(&OverlapWindow_DispatchTbl);
	Dispatcher_OverlapWindow FUNC_ARGS;
}

static void InitDispatcher_ApplySymmetricWindow FUNC_ARGS_DECL {
	Dispatcher_ApplySymmetricWindow = (Dispatcher_ApplyWindow_t)Fourier_GetDispatchFnc(&ApplySymmetricWindow_DispatchTbl);
	Dispatcher_ApplySymmetricWindow FUNC_ARGS;
}

static void InitDispatcher_OverlapSymmetricWindow FUNC_ARGS_DECL {
	Dispatcher_OverlapSymmetricWindow = (Dispatcher_ApplyWindow_t)Fourier_GetDispatchFnc(&OverlapSymmetricWindow_DispatchTbl);
	Dispatcher_OverlapSymmetricWindow FUNC_ARGS;
}

/************************************************/

void Fourier_ApplyWindow FUNC_ARGS_DECL {
	Dispatcher_ApplyWindow FUNC_ARGS;
}
void Fourier_OverlapWindow FUNC_ARGS_DECL {
	Dispatcher_OverlapWindow FUNC_ARGS;
}
void Fourier_ApplySymmetricWindow FUNC_ARGS_DECL {
	Dispatcher_ApplySymmetricWindow FUNC_ARGS;
}
void Fourier_OverlapSymmetricWindow FUNC_ARGS_DECL {
	Dispatcher_OverlapSymmetricWindow FUNC_ARGS;
}

/************************************************/

#include "Fourier_ApplyWindow_Template.h"
void Fourier_ApplyWindow_Generic FUNC_ARGS_DECL {
	Fourier_ApplyWindow_Template FUNC_ARGS;
}
void Fourier_OverlapWindow_Generic FUNC_ARGS_DECL {
	Fourier_OverlapWindow_Template FUNC_ARGS;
}
void Fourier_ApplySymmetricWindow_Generic FUNC_ARGS_DECL {
	Fourier_ApplySymmetricWindow_Template FUNC_ARGS;
}
void Fourier_OverlapSymmetricWindow_Generic FUNC_ARGS_DECL {
	Fourier_OverlapSymmetricWindow_Template FUNC_ARGS;
}

#undef FUNC_ARGS_DECL
#undef FUNC_ARGS

/************************************************/
// EOF
/************************************************/
