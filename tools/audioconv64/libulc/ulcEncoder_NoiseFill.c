/**************************************/
//! ulc-codec: Ultra-Low-Complexity Audio Codec
//! Copyright (C) 2025, Ruben Nunez (Aikku; aik AT aol DOT com DOT au)
//! Refer to the project README file for license terms.
/**************************************/
#include <math.h>
/**************************************/
#include "ulcEncoder.h"
#include "ulcHelper.h"
/**************************************/
#if ULC_USE_NOISE_CODING
/**************************************/

//! Get the quantized noise amplitude for encoding
int ULCi_GetNoiseQ(const float *Data, int Band, int N, float q) {
	//! Fixup for DCT+DST -> Pseudo-DFT
	Data += Band / 2 * 2;
	N = (N + (Band & 1) + 1) / 2;

	//! Analyze for the noise amplitude (geometric mean over N coefficients)
	float Amplitude; {
		int n;
		float Sum = 0.0f, SumW = 0.0f;
		for(n=0;n<N;n++) {
			float w  = Data[n*2+0];
			float wy = Data[n*2+1]; //! wy = w * y
			Sum += wy, SumW += w;
		}
		if(Sum == 0.0f) return 0;
		Amplitude = expf(Sum/SumW);
	}

	//! Quantize the noise amplitude into final code
	int NoiseQ = ULCi_CompandedQuantizeCoefficientUnsigned(Amplitude*q, 1 + 0x7);
	return NoiseQ;
}

/**************************************/

//! Compute quantized HF extension parameters for encoding
static int GetHFExtParams_LeastSquares(const float *Data, int N, float *Amplitude, float *Decay) {
	//! Assumes Data[] is {Weight,Value} pairs
	int n;
	float SumX  = 0.0f;
	float SumX2 = 0.0f;
	float SumXY = 0.0f;
	float SumY  = 0.0f;
	float SumW  = 0.0f;
	for(n=0;n<N;n++) {
		float x  = n * 2.0f;
		float w  = Data[n*2+0];
		float wy = Data[n*2+1]; //! wy = w * y
		SumX  += w*x;
		SumX2 += w*x*x;
		SumXY +=   x*wy;
		SumY  +=     wy;
		SumW  += w;
	}

	//! Solve for amplitude and decay
	float Det = SumW*SumX2 - SQR(SumX); if(Det == 0.0f) return 0;
	*Amplitude = (SumX2*SumY  - SumX*SumXY) / Det;
	*Decay     = (SumW *SumXY - SumX*SumY ) / Det;
	return 1;
}
void ULCi_GetHFExtParams(const float *Data, int Band, int N, float q, int *_NoiseQ, int *_NoiseDecay) {
	//! Fixup for DCT+DST -> Pseudo-DFT
	Data += Band / 2 * 2;
	N = (N + (Band & 1) + 1) / 2;

	//! Solve for least-squares (in the log domain, for exponential fitting)
	float Amplitude, Decay; {
		//! If couldn't solve, disable noise fill
		if(!GetHFExtParams_LeastSquares(Data, N, &Amplitude, &Decay)) {
			*_NoiseQ = *_NoiseDecay = 0;
			return;
		}

		//! Convert to linear units
		Amplitude = expf(Amplitude);
		Decay     = (Decay < 0.0f) ? expf(Decay) : 1.0f; //! <- Ensure E^LogDecay is <= 1.0
	}

	//! Quantize amplitude and decay
	//! Amplitude has already been scaled by 4.0 (plus normalization),
	//! but we need to scale to 16.0 here because HF extension uses
	//! a 4bit amplitude instead of 3bit like "normal" noise fill does
	int NoiseQ     = ULCi_CompandedQuantizeCoefficientUnsigned(Amplitude*q*4.0f, 1 + 0xF);
	int NoiseDecay = ULCi_CompandedQuantizeUnsigned((Decay-1.0f) * -0x1.0p19f); //! (1-Decay) * 2^19
	if(!NoiseDecay) return; //! <- On immediate falloff, disable fill
	if(NoiseDecay > 0xFF) NoiseDecay = 0xFF;
	*_NoiseQ     = NoiseQ;
	*_NoiseDecay = NoiseDecay;
}

/**************************************/
#endif
/**************************************/
//! EOF
/**************************************/
