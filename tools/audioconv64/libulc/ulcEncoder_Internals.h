/**************************************/
//! ulc-codec: Ultra-Low-Complexity Audio Codec
//! Copyright (C) 2024, Ruben Nunez (Aikku; aik AT aol DOT com DOT au)
//! Refer to the project README file for license terms.
/**************************************/
#pragma once
/**************************************/
#include "ulcEncoder.h"
/**************************************/

//! Get optimal log base-2 overlap and window scalings for transients
//! Bit codes for transient region coding, and their window sizes:
//!  First nybble:
//!   0xxx: No decimation. xxx = Overlap scaling
//!   1xxx: Decimate. xxx = Overlap scaling for the transient subblock
//!  Second nybble (when first nybble is 1xxx; otherwise, this is implicitly 0001):
//!   1xxx: Decimation by 1/8: Position = 0~7
//!    1000: N/8*,N/8,N/4,N/2
//!    1001: N/8,N/8*,N/4,N/2
//!    1010: N/4,N/8*,N/8,N/2
//!    1011: N/4,N/8,N/8*,N/2
//!    1100: N/2,N/8*,N/8,N/4
//!    1101: N/2,N/8,N/8*,N/4
//!    1110: N/2,N/4,N/8*,N/8
//!    1111: N/2,N/4,N/8,N/8*
//!   01xx: Decimation by 1/4: Position = 0~3
//!    0100: N/4*,N/4,N/2
//!    0101: N/4,N/4*,N/2
//!    0110: N/2,N/4*,N/4
//!    0111: N/2,N/4,N/4*
//!   001x: Decimation by 1/2: Position = 0~1
//!    0010: N/2*,N/2
//!    0011: N/2,N/2*
//!   0001: No decimation (not coded in the bitstream)
//!    0001: N/1*
//!  The starred subblocks set overlap scaling with regards to the last [sub]block.
int ULCi_GetWindowCtrl(
	const float *BlockData,
	struct ULC_TransientData_t *TransientBuffer,
	      float *TransientFilter,
	      float *TmpBuffer,
	      int    BlockSize,
	      int    nChan,
	      int    RateHz
);

//! Transform a block and prepare its coefficients
int ULCi_TransformBlock(struct ULC_EncoderState_t *State, const float *Data);

//! Encode block to target buffer
//! Returns the block size (in bits) and the number of coded (non-zero) coefficients
int ULCi_EncodePass(const struct ULC_EncoderState_t *State, void *_DstBuffer, int nOutCoef);

/**************************************/
#if ULC_USE_PSYCHOACOUSTICS
/**************************************/

//! Calculate masking levels for each frequency line
void ULCi_CalculatePsychoacoustics(
	float *MaskingNp,
	float *BufferAmp2,
	void  *BufferTemp,
	int    BlockSize,
	int    RateHz,
	uint32_t WindowCtrl
);

/**************************************/
#endif
/**************************************/
#if ULC_USE_NOISE_CODING
/**************************************/

//! Compute noise spectrum (logarithmic output)
//! NOTE: The output data is set to pairs of {Weight,Weight*Log[Level]}.
void ULCi_CalculateNoiseLogSpectrum(float *Data, void *Temp, int N, int RateHz);

//! Get the quantized noise amplitude for encoding
int ULCi_GetNoiseQ(const float *Data, int Band, int N, float q);

//! Compute quantized HF extension parameters for encoding
void ULCi_GetHFExtParams(const float *Data, int Band, int N, float q, int *_NoiseQ, int *_NoiseDecay);

/**************************************/
#endif
/**************************************/
//! EOF
/**************************************/
