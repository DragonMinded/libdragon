/**************************************/
//! ulc-codec: Ultra-Low-Complexity Audio Codec
//! Copyright (C) 2022, Ruben Nunez (Aikku; aik AT aol DOT com DOT au)
//! Refer to the project README file for license terms.
/**************************************/
#pragma once
/**************************************/

//! Decoder state structure
//! NOTE:
//!  -The global state data must be set before calling ULC_DecoderState_Init()
//!  -{nChan, BlockSize,} must not change after calling ULC_EncoderState_Init()
struct ULC_DecoderState_t {
	//! Global state (do not change after initialization)
	int nChan;     //! Channels in encoding scheme
	int BlockSize; //! Transform block size

	//! Decoding state
	//! Buffer memory layout:
	//!  Data:
	//!   char  _Padding[];
	//!   float TransformBuffer[BlockSize]
	//!   float TransformTemp  [nChan * BlockSize]
	//!   float TransformInvLap[nChan * BlockSize/2]
	//! BufferData contains the pointer returned by malloc()
	//! TransformTemp[] is large because we need to interleave the output.
	int    LastSubBlockSize; //! Size of last [sub]block processed
	void  *BufferData;
	float *TransformBuffer;
	float *TransformTemp;
	float *TransformInvLap;
};

/**************************************/

//! Initialize decoder state
//! On success, returns a non-negative value
//! On failure, returns a negative value
int ULC_DecoderState_Init(struct ULC_DecoderState_t *State);

//! Destroy decoder state
void ULC_DecoderState_Destroy(struct ULC_DecoderState_t *State);

/**************************************/

//! Decode block
//! NOTE:
//!  -Output data will have its channels arranged sequentially;
//!   For example:
//!   {
//!    0,1,2,3...BlockSize-1, //! Chan0
//!    0,1,2,3...BlockSize-1, //! Chan1
//!   }
//!  -SrcBuffer will only be accessed via bytes.
//! Returns the number of bits read.
int ULC_DecodeBlock(struct ULC_DecoderState_t *State, float *DstData, const void *SrcBuffer);

/**************************************/
//! EOF
/**************************************/
