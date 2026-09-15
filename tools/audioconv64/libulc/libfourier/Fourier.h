/************************************************/
#pragma once
/************************************************/
#ifdef __cplusplus
extern "C" {
#endif
/************************************************/

// x86 compilation options
// #define FOURIER_ALLOW_SSE
// #define FOURIER_ALLOW_FMA
// #define FOURIER_ALLOW_AVX

/************************************************/

// DCT-II/DCT-IV (scaled)
// Arguments:
//  Buf[N]
//  Tmp[N]
// Implemented transforms (matrix form):
//  MtxDCT2 = Table[Cos[(n-1/2)(k-1  )Pi/N], {k,N}, {n,N}]
//  MtxDCT4 = Table[Cos[(n-1/2)(k-1/2)Pi/N], {k,N}, {n,N}]
// Implementations from:
//  "Signal Processing based on Stable radix-2 DCT I-IV Algorithms having Orthogonal Factors"
//  DOI: 10.13001/1081-3810.3207
// Notes:
//  -N must be a power of two, and >= 8.
void Fourier_DCT2(float *Buf, float *Tmp, int N);
void Fourier_DCT4(float *Buf, float *Tmp, int N);

// Centered FFT/iFFT (scaled)
// Arguments:
//  Buf[N]
//  Tmp[N]
// FFT outputs N/2 complex lines (packed as {Re,Im}).
// Centered DFT derivation:
//  "The Centered Discrete Fourier Transform and a Parallel Implementation of the FFT"
//  DOI: 10.1109/ICASSP.2011.5946834
// Notes:
//  -N must be a power of two, and >= 16.
void Fourier_FFTReCenter (float *Buf, float *Tmp, int N); // Buf[N], Tmp[N]
void Fourier_iFFTReCenter(float *Buf, float *Tmp, int N); // Buf[N], Tmp[N]

// MDCT+MDST (based on DCT-IV; scaled)
// Arguments:
//  MDCT[N]
//  MDST[N]
//  New[N]
//  Lap[N]
//  BufTmp[N]
// Implemented transforms (matrix form):
//  MtxMDCT = Table[Cos[(n-1/2 - N/2 + N*2)(k-1/2)Pi/N], {k, N}, {n,2N}]
//  MtxMDST = Table[Sin[(n-1/2 + N/2 + N*2)(k-1/2)Pi/N], {k, N}, {n,2N}]
// Notes:
//  -N must be a power of two, and >= 16.
//  -Overlap must be a power of two, and >= 16 or 0.
//  -Shifted basis (note the signs in the matrices).
//  -Sine window (modulated lapped transform) is
//   always used for lapping.
//  -New can be the same as BufTmp. However, this
//   implies trashing of the buffer contents.
void Fourier_MDCT_MDST(float *MDCT, float *MDST, const float *New, float *Lap, float *BufTmp, int N, int Overlap);

// IMDCT (based on DCT-IV; scaled)
// Arguments:
//  BufOut[N]
//  BufIn[N]
//  BufLap[N/2]
//  BufTmp[N]
// Implemented transforms (matrix form):
//  mtxIMDCT = Table[Cos[(n-1/2 - N/2 + N*2)(k-1/2)Pi/N], {k,2N}, {n, N}]
// Notes:
//  -N must be a power of two, and >= 16.
//  -Overlap must be a power of two, and >= 16 or 0.
//  -BufOut must not be the same as BufIn.
//  -Shifted basis (note the sign in the matrix).
//   This re-inverts (ie. removes) the phase shift
//   in the MDCT implementation from above.
//  -BufIn can be the same as BufTmp. However, this
//   implies trashing of the buffer contents.
void Fourier_IMDCT(float *BufOut, const float *BufIn, float *BufLap, float *BufTmp, int N, int Overlap);

/************************************************/

// Apply amplitude to target
// Arguments:
//  Dst[N*2] (Re/Im pairs)
//  Src[N*2] (Re/Im pairs)
//  Amplitude[N]
// Notes:
//  -N must be a multiple of 16.
void Fourier_ApplyAmplitude(float *Dst, const float *Src, const float *Amplitude, float DryLevel, int N);

// Apply window to target
// Arguments:
//  Dst[N]
//  Src[N]
//  Window[N] (or Window[N/2] for symmetric windows)
// Notes:
//  -N must be a multiple of 8 (or 16 for symmetric windows).
//  -When applying overlapping windows, Dst[] is used as
//   the overlap source AND target, as an accumulator.
void Fourier_ApplyWindow           (float *Dst, const float *Src, const float *Window, int N);
void Fourier_OverlapWindow         (float *Dst, const float *Src, const float *Window, int N);
void Fourier_ApplySymmetricWindow  (float *Dst, const float *Src, const float *Window, int N);
void Fourier_OverlapSymmetricWindow(float *Dst, const float *Src, const float *Window, int N);

/************************************************/
#ifdef __cplusplus
}
#endif
/************************************************/
// EOF
/************************************************/
