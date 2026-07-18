/**************************************/
//! ulc-codec: Ultra-Low-Complexity Audio Codec
//! Copyright (C) 2024, Ruben Nunez (Aikku; aik AT aol DOT com DOT au)
//! Refer to the project README file for license terms.
/**************************************/
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
/**************************************/
#include "libfourier/Fourier.h"
#include "ulcEncoder.h"
#include "ulcHelper.h"
#include "ulcEncoder_Internals.h"
/**************************************/
#define BUFFER_ALIGNMENT 64u //! Always align memory to 64-byte boundaries (preparation for AVX-512)
/**************************************/

#define MIN_CHANS       1
#define MAX_CHANS     255
#define MIN_BANDS     256 //! Limited by the transient detector's decimation
#define MAX_BANDS   32768
#define MIN_OVERLAP    16 //! Depends on SIMD routines; setting as 16 arbitrarily

//! Initialize encoder state
int ULC_EncoderState_Init(struct ULC_EncoderState_t *State) {
	//! Clear anything that is needed for EncoderState_Destroy()
	State->BufferData = NULL;

	//! Verify parameters
	int nChan      = State->nChan;
	int BlockSize  = State->BlockSize;
	if(nChan     < MIN_CHANS || nChan     > MAX_CHANS) return -1;
	if(BlockSize < MIN_BANDS || BlockSize > MAX_BANDS) return -1;
	if((BlockSize & (-BlockSize)) != BlockSize)        return -1;

	//! Get buffer offsets and allocation size
	//! NOTE: TransformTemp must be able to contain at least two
	//! blocks' worth of data (MDCT+MDST coefficients for analysis).
	int AllocSize = 0;
#define CREATE_BUFFER(Name, Sz) int Name##_Offs = AllocSize; AllocSize += Sz
	CREATE_BUFFER(SampleBuffer,    sizeof(float) * (nChan*BlockSize) * 2);
	CREATE_BUFFER(TransformBuffer, sizeof(float) * (nChan*BlockSize));
#if ULC_USE_NOISE_CODING
	CREATE_BUFFER(TransformNoise,  sizeof(float) * (nChan*BlockSize));
#endif
	CREATE_BUFFER(TransformFwdLap, sizeof(float) * (nChan*BlockSize));
	CREATE_BUFFER(TransformTemp,   sizeof(float) * ((nChan + (nChan < 2)) * BlockSize));
	CREATE_BUFFER(TransformIndex,  sizeof(int)   * (nChan*BlockSize));
	CREATE_BUFFER(TransientBuffer, sizeof(struct ULC_TransientData_t) * ULC_MAX_BLOCK_DECIMATION_FACTOR*2);
#undef CREATE_BUFFER

	//! Allocate buffer space
	char *Buf = State->BufferData = malloc(BUFFER_ALIGNMENT-1 + AllocSize);
	if(!Buf) return -1;

	//! Initialize pointers
	Buf += (-(uintptr_t)Buf) & (BUFFER_ALIGNMENT-1);
	State->SampleBuffer    = (float*)(Buf + SampleBuffer_Offs);
	State->TransformBuffer = (float*)(Buf + TransformBuffer_Offs);
#if ULC_USE_NOISE_CODING
	State->TransformNoise  = (float*)(Buf + TransformNoise_Offs);
#endif
	State->TransformFwdLap = (float*)(Buf + TransformFwdLap_Offs);
	State->TransformTemp   = (float*)(Buf + TransformTemp_Offs);
	State->TransformIndex  = (int  *)(Buf + TransformIndex_Offs);
	State->TransientBuffer = (struct ULC_TransientData_t*)(Buf + TransientBuffer_Offs);

	//! Set initial state
	int i;
	State->NextWindowCtrl = 0x10; //! No decimation, full overlap. Doesn't really matter, though.
	for(i=0;i<3;                i++) State->TransientFilter[i] = 0.0f;
	for(i=0;i<nChan*BlockSize*2;i++) State->SampleBuffer   [i] = 0.0f;
	for(i=0;i<nChan*BlockSize;  i++) State->TransformFwdLap[i] = 0.0f;
	for(i=0;i<ULC_MAX_BLOCK_DECIMATION_FACTOR*2;i++) {
		State->TransientBuffer[i] = (struct ULC_TransientData_t){.Sum = 0.0f, .SumW = 0.0f};
	}

	//! Success
	return 1;
}

/**************************************/

//! Destroy encoder state
void ULC_EncoderState_Destroy(struct ULC_EncoderState_t *State) {
	//! Free buffer space
	free(State->BufferData);
}

/**************************************/

//! Encode block (CBR mode)
int ULC_EncodeBlock_CBR_Core(struct ULC_EncoderState_t *State, void *DstBuffer, float RateKbps, int MaxCoef) {
	int Size;
	int nOutCoef  = -1;
	int BitBudget = (int)((State->BlockSize * RateKbps) * 1000.0f/State->RateHz); //! NOTE: Truncate

	//! Perform a binary search for the optimal nOutCoef
	int Lo = 0, Hi = MaxCoef;
	if(Lo < Hi) do {
		nOutCoef = (Lo + Hi) / 2u;
		Size = ULCi_EncodePass(State, DstBuffer, nOutCoef);
		     if(Size < BitBudget) Lo = nOutCoef;
		else if(Size > BitBudget) Hi = nOutCoef-1;
		else {
			//! Should very, VERY rarely happen, but just in case
			Lo = nOutCoef;
			break;
		}
	} while(Lo < Hi-1);

	//! Avoid going over budget
	int nOutCoefFinal = Lo;
	if(nOutCoefFinal != nOutCoef) Size = ULCi_EncodePass(State, DstBuffer, nOutCoef = nOutCoefFinal);
	return Size;
}
const void *ULC_EncodeBlock_CBR(struct ULC_EncoderState_t *State, const float *SrcData, int *Size, float RateKbps) {
	void *Buf = (void*)State->TransformTemp;
	int MaxCoef = ULCi_TransformBlock(State, SrcData);
	int Sz = ULC_EncodeBlock_CBR_Core(State, Buf, RateKbps, MaxCoef);
	if(Size) *Size = Sz;
	return Buf;
}

/**************************************/

//! Encode block (ABR mode)
const void *ULC_EncodeBlock_ABR(struct ULC_EncoderState_t *State, const float *SrcData, int *Size, float RateKbps, float AvgComplexity) {
	void *Buf = (void*)State->TransformTemp;
	int MaxCoef = ULCi_TransformBlock(State, SrcData);
	float TargetKbps = RateKbps * State->BlockComplexity / AvgComplexity;
	int Sz = ULC_EncodeBlock_CBR_Core(State, Buf, TargetKbps, MaxCoef);
	if(Size) *Size = Sz;
	return Buf;
}

/**************************************/

//! Encode block (VBR mode)
const void *ULC_EncodeBlock_VBR(struct ULC_EncoderState_t *State, const float *SrcData, int *Size, float Quality) {
	//! NOTE: The constant in front of the logarithm was experimentally
	//! dervied; I have no idea what relation it bears to actual encoding.
	void *Buf = (void*)State->TransformTemp;
	float TargetComplexity = 0x1.E4EFB7p3f*logf(100.0f / Quality); //! 0x1.E4EFB7p3 = E^E. This seems to closely match ABR mode's peak rates
	int MaxCoef  = ULCi_TransformBlock(State, SrcData);
	int nTargetCoef = MaxCoef; {
		//! TargetComplexity == 0 which would result in a
		//! divide-by-zero error. So instead we just leave
		//! nTargetCoef alone at its maximum value.
		if(TargetComplexity > 0.0f) {
			float fTarget = (State->nChan*State->BlockSize) * State->BlockComplexity / TargetComplexity;
			if(fTarget < MaxCoef) nTargetCoef = (int)fTarget;
		}
	}
	int Sz = ULCi_EncodePass(State, Buf, nTargetCoef);
	if(Size) *Size = Sz;
	return Buf;
}

/**************************************/
//! EOF
/**************************************/
