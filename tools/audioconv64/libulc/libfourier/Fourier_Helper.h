/************************************************/
#pragma once
/************************************************/
#include <math.h>
#include <stdint.h>
/************************************************/
#if (defined(_M_IX86) || defined(_M_X64) || defined(__i386__))
# if defined(_MSC_VER)
#  define FOURIER_IS_X86
#  include <intrin.h>
# elif defined(__GNUC__)
#  define FOURIER_IS_X86
#  include <cpuid.h>
# else
#  warning "Unknown compiler. Disabling x86-specific code."
# endif
# if defined(FOURIER_IS_X86)
#  if (defined(__AVX__) && defined(FOURIER_ALLOW_AVX)) || (defined(__FMA__) && defined(FOURIER_ALLOW_FMA))
#   include <immintrin.h>
#  endif
#  if (defined(__SSE__) && defined(FOURIER_ALLOW_SSE))
#   include <xmmintrin.h>
#  endif
# endif
#endif
/************************************************/
#if defined(_MSC_VER)
# define FOURIER_FORCE_INLINE static inline __forceinline
# define FOURIER_ASSUME(Cond) __assume(Cond)
# define FOURIER_ASSUME_ALIGNED(x,Align) __assume(((uintptr_t)(x) & (Align)) == 0)
#elif defined(__GNUC__)
# define FOURIER_FORCE_INLINE static inline __attribute__ ((always_inline))
# define FOURIER_ASSUME(Cond) (Cond) ? ((void)0) : __builtin_unreachable()
# define FOURIER_ASSUME_ALIGNED(x,Align) x = __builtin_assume_aligned(x,Align)
#else
# define FOURIER_FORCE_INLINE static inline
# define FOURIER_ASSUME(Cond)
# define FOURIER_ASSUME_ALIGNED(x,Align)
#endif
/************************************************/

#if (defined(FOURIER_IS_X86) && defined(__AVX__) && defined(FOURIER_ALLOW_AVX))
  typedef __m256 Fourier_Vec_t;
# define FOURIER_VSTRIDE            8
# define FOURIER_ALIGNMENT          32
# define FOURIER_VLOAD(Src)         _mm256_load_ps(Src)
# define FOURIER_VLOADU(Src)        _mm256_loadu_ps(Src)
# define FOURIER_VSTORE(Dst, x)     _mm256_store_ps(Dst, x)
# define FOURIER_VSTOREU(Dst, x)    _mm256_storeu_ps(Dst, x)
# define FOURIER_VSET1(x)           _mm256_set1_ps(x)
# define FOURIER_VSET_LINEAR_RAMP() _mm256_setr_ps(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f)
# define FOURIER_VADD(x, y)         _mm256_add_ps(x, y)
# define FOURIER_VSUB(x, y)         _mm256_sub_ps(x, y)
# define FOURIER_VMUL(x, y)         _mm256_mul_ps(x, y)
# define FOURIER_VDIV(x, y)         _mm256_div_ps(x, y)
# define FOURIER_VREVERSE_LANE(x)   _mm256_shuffle_ps(x, x, 0x1B)
# define FOURIER_VREVERSE(x)        _mm256_permute2f128_ps(FOURIER_VREVERSE_LANE(x), FOURIER_VREVERSE_LANE(x), 0x01)
# define FOURIER_VNEGATE_ODD(x)     _mm256_xor_ps(x, _mm256_setr_ps(0.0f, -0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 0.0f, -0.0f))
# if (defined(__FMA__) && defined(FOURIER_ALLOW_FMA))
#  define FOURIER_VFMA(x, y, a)     _mm256_fmadd_ps(x, y, a)
#  define FOURIER_VFMS(x, y, a)     _mm256_fmsub_ps(x, y, a)
#  define FOURIER_VNFMA(x, y, a)    _mm256_fnmadd_ps(x, y, a)
# else
#  define FOURIER_VFMA(x, y, a)     _mm256_add_ps(_mm256_mul_ps(x, y), a)
#  define FOURIER_VFMS(x, y, a)     _mm256_sub_ps(_mm256_mul_ps(x, y), a)
#  define FOURIER_VNFMA(x, y, a)    _mm256_sub_ps(a, _mm256_mul_ps(x, y))
# endif
# define FOURIER_VSQRT(x)           _mm256_sqrt_ps(x)
# define FOURIER_VAND(x, y)         _mm256_and_ps(x, y)
# define FOURIER_VANDNOT(x, y)      _mm256_andnot_ps(y, x)
# define FOURIER_VOR(x, y)          _mm256_or_ps(x, y)
# define FOURIER_VCMPEQ(x, y)       _mm256_cmp_ps(x, y, 0)
# define FOURIER_VCMPNE(x, y)       _mm256_cmp_ps(x, y, 4)
# define FOURIER_VCMPLT(x, y)       _mm256_cmp_ps(x, y, 1)
  FOURIER_FORCE_INLINE void FOURIER_VINTERLEAVE(Fourier_Vec_t a, Fourier_Vec_t b, Fourier_Vec_t *Lo, Fourier_Vec_t *Hi) {
	__m256 t0 = _mm256_unpacklo_ps(a, b);
	__m256 t1 = _mm256_unpackhi_ps(a, b);
	*Lo = _mm256_permute2f128_ps(t0, t1, 0x20);
	*Hi = _mm256_permute2f128_ps(t0, t1, 0x31);
  }
  FOURIER_FORCE_INLINE void FOURIER_VSPLIT_EVEN_ODD(Fourier_Vec_t l, Fourier_Vec_t h, Fourier_Vec_t *Even, Fourier_Vec_t *Odd) {
	__m256 a = _mm256_permute2f128_ps(l, h, 0x20);
	__m256 b = _mm256_permute2f128_ps(l, h, 0x31);
	*Even = _mm256_shuffle_ps(a, b, 0x88);
	*Odd  = _mm256_shuffle_ps(a, b, 0xDD);
  }
  FOURIER_FORCE_INLINE void FOURIER_VSPLIT_EVEN_ODDREV(Fourier_Vec_t l, Fourier_Vec_t h, Fourier_Vec_t *Even, Fourier_Vec_t *Odd) {
	__m256 a = _mm256_permute2f128_ps(l, h, 0x20);
	__m256 b = _mm256_permute2f128_ps(l, h, 0x31);
	*Even = _mm256_shuffle_ps(a, b, 0x88);
	b     = _mm256_shuffle_ps(b, a, 0x77);
	*Odd  = _mm256_permute2f128_ps(b, b, 0x01);
  }
#elif (defined(FOURIER_IS_X86) && defined(__SSE__) && defined(FOURIER_ALLOW_SSE))
  typedef __m128 Fourier_Vec_t;
# define FOURIER_VSTRIDE            4
# define FOURIER_ALIGNMENT          16
# define FOURIER_VLOAD(Src)         _mm_load_ps(Src)
# define FOURIER_VLOADU(Src)        _mm_loadu_ps(Src)
# define FOURIER_VSTORE(Dst, x)     _mm_store_ps(Dst, x)
# define FOURIER_VSTOREU(Dst, x)    _mm_storeu_ps(Dst, x)
# define FOURIER_VSET1(x)           _mm_set1_ps(x)
# define FOURIER_VSET_LINEAR_RAMP() _mm_setr_ps(0.0f, 1.0f, 2.0f, 3.0f)
# define FOURIER_VADD(x, y)         _mm_add_ps(x, y)
# define FOURIER_VSUB(x, y)         _mm_sub_ps(x, y)
# define FOURIER_VMUL(x, y)         _mm_mul_ps(x, y)
# define FOURIER_VDIV(x, y)         _mm_div_ps(x, y)
# define FOURIER_VREVERSE(x)        _mm_shuffle_ps(x, x, 0x1B)
# define FOURIER_VNEGATE_ODD(x)     _mm_xor_ps(x, _mm_setr_ps(0.0f, -0.0f, 0.0f, -0.0f))
# if (defined(__FMA__) && defined(FOURIER_ALLOW_FMA))
#  define FOURIER_VFMA(x, y, a)     _mm_fmadd_ps(x, y, a)
#  define FOURIER_VFMS(x, y, a)     _mm_fmsub_ps(x, y, a)
#  define FOURIER_VNFMA(x, y, a)    _mm_fnmadd_ps(x, y, a)
# else
#  define FOURIER_VFMA(x, y, a)     _mm_add_ps(_mm_mul_ps(x, y), a)
#  define FOURIER_VFMS(x, y, a)     _mm_sub_ps(_mm_mul_ps(x, y), a)
#  define FOURIER_VNFMA(x, y, a)    _mm_sub_ps(a, _mm_mul_ps(x, y))
# endif
# define FOURIER_VSQRT(x)           _mm_sqrt_ps(x)
# define FOURIER_VAND(x, y)         _mm_and_ps(x, y)
# define FOURIER_VANDNOT(x, y)      _mm_andnot_ps(y, x)
# define FOURIER_VOR(x, y)          _mm_or_ps(x, y)
# define FOURIER_VCMPEQ(x, y)       _mm_cmpeq_ps(x, y)
# define FOURIER_VCMPNE(x, y)       _mm_cmpneq_ps(x, y)
# define FOURIER_VCMPLT(x, y)       _mm_cmplt_ps(x, y)
  FOURIER_FORCE_INLINE void FOURIER_VINTERLEAVE(Fourier_Vec_t a, Fourier_Vec_t b, Fourier_Vec_t *Lo, Fourier_Vec_t *Hi) {
	*Lo = _mm_unpacklo_ps(a, b);
	*Hi = _mm_unpackhi_ps(a, b);
  }
  FOURIER_FORCE_INLINE void FOURIER_VSPLIT_EVEN_ODD(Fourier_Vec_t l, Fourier_Vec_t h, Fourier_Vec_t *Even, Fourier_Vec_t *Odd) {
	*Even = _mm_shuffle_ps(l, h, 0x88);
	*Odd  = _mm_shuffle_ps(l, h, 0xDD);
  }
  FOURIER_FORCE_INLINE void FOURIER_VSPLIT_EVEN_ODDREV(Fourier_Vec_t l, Fourier_Vec_t h, Fourier_Vec_t *Even, Fourier_Vec_t *Odd) {
	*Even = _mm_shuffle_ps(l, h, 0x88);
	*Odd  = _mm_shuffle_ps(h, l, 0x77);
  }
#else
  typedef float Fourier_Vec_t;
# define FOURIER_VSTRIDE            1
# define FOURIER_ALIGNMENT          4
# define FOURIER_VLOAD(Src)         (*Src)
# define FOURIER_VLOADU(Src)        (*Src)
# define FOURIER_VSTORE(Dst, x)     (*Dst = x)
# define FOURIER_VSTOREU(Dst, x)    (*Dst = x)
# define FOURIER_VSET1(x)           (x)
# define FOURIER_VSET_LINEAR_RAMP() (0.0f)
# define FOURIER_VADD(x, y)         ((x) + (y))
# define FOURIER_VSUB(x, y)         ((x) - (y))
# define FOURIER_VMUL(x, y)         ((x) * (y))
# define FOURIER_VDIV(x, y)         ((x) / (y))
# define FOURIER_VREVERSE(x)        (x)
# define FOURIER_VNEGATE_ODD(x)     (x)
# define FOURIER_VFMA(x, y, a)      ((x)*(y) + (a))
# define FOURIER_VFMS(x, y, a)      ((x)*(y) - (a))
# define FOURIER_VNFMA(x, y, a)     ((a) - (x)*(y))
# define FOURIER_VSQRT(x)           sqrtf(x)
#endif

/************************************************/

// Function tables for different CPU specs
struct Fourier_FuncTbl_t {
	void *Generic;
#ifdef FOURIER_IS_X86
# if defined(FOURIER_ALLOW_SSE) || defined(FOURIER_ALLOW_AVX)
	void *SSE;
#  if defined(FOURIER_ALLOW_FMA)
	void *SSE_FMA;
#  endif
# endif
# if defined(FOURIER_ALLOW_AVX)
	void *AVX;
#  if defined(FOURIER_ALLOW_FMA)
	void *AVX_FMA;
#  endif
# endif
#endif
};

/************************************************/

// Get best function based on CPU specs
FOURIER_FORCE_INLINE
void *Fourier_GetDispatchFnc(const struct Fourier_FuncTbl_t *FuncTbl) {
	void *Fnc = FuncTbl->Generic;
#ifdef FOURIER_IS_X86
	uint32_t cpuInfo[4] = {0};
# if defined(_MSC_VER)
	__cpuid(cpuInfo, 1);
# else
	__get_cpuid(1, &cpuInfo[0], &cpuInfo[1], &cpuInfo[2], &cpuInfo[3]);
# endif
# if defined(FOURIER_ALLOW_SSE)
	if((cpuInfo[3] & (1<<25)) != 0)
		Fnc = FuncTbl->SSE;
#  if defined(FOURIER_ALLOW_FMA)
	if((cpuInfo[3] & (1<<25)) != 0 && (cpuInfo[2] & (1<<12)) != 0)
		Fnc = FuncTbl->SSE_FMA;
#  endif
# endif
# if defined(FOURIER_ALLOW_AVX)
	if((cpuInfo[2] & (1<<28)) != 0)
		Fnc = FuncTbl->AVX;
#  if defined(FOURIER_ALLOW_FMA)
	if((cpuInfo[2] & (1<<28)) != 0 && (cpuInfo[2] & (1<<12)) != 0)
		Fnc = FuncTbl->AVX_FMA;
#  endif
# endif
#endif
	return Fnc;
}

/************************************************/

// Sin[z] approximation (where z = x*Pi/2, x < Pi/2)
// Coefficients derived by RMSE minimization
FOURIER_FORCE_INLINE
Fourier_Vec_t Fourier_Sin(Fourier_Vec_t x) {
	Fourier_Vec_t x2  = FOURIER_VMUL(x, x);
	Fourier_Vec_t Res = FOURIER_VFMA(x2, FOURIER_VSET1(+0x1.3C8B08p-13f), FOURIER_VSET1(-0x1.3237ECp-8f));
	              Res = FOURIER_VFMA(x2, Res, FOURIER_VSET1(+0x1.4667ACp-4f));
	              Res = FOURIER_VFMA(x2, Res, FOURIER_VSET1(-0x1.4ABBB8p-1f));
	              Res = FOURIER_VFMA(x2, Res, FOURIER_VSET1(+0x1.921FB4p0f));
	return FOURIER_VMUL(x, Res);
}

/************************************************/

// Cos[z] approximation (where z = x*Pi/2, x < Pi/2)
// Coefficients derived by RMSE minimization
FOURIER_FORCE_INLINE
Fourier_Vec_t Fourier_Cos(Fourier_Vec_t x) {
	Fourier_Vec_t x2  = FOURIER_VMUL(x, x);
	Fourier_Vec_t Res = FOURIER_VFMA(x2, FOURIER_VSET1(+0x1.C2EA62p-11f), FOURIER_VSET1(-0x1.550810p-6f));
	              Res = FOURIER_VFMA(x2, Res, FOURIER_VSET1(+0x1.03BDD6p-2f));
	              Res = FOURIER_VFMA(x2, Res, FOURIER_VSET1(-0x1.3BD3B2p0f));
	              Res = FOURIER_VFMA(x2, Res, FOURIER_VSET1(1.0f));
	return Res;
}

/************************************************/

// End-condition routines
void Fourier_DCT2_8(float *x); // 8-point DCT-II
void Fourier_DCT4_8(float *x); // 8-point DCT-IV

/************************************************/
// EOF
/************************************************/
