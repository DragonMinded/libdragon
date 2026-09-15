/**************************************/
//! ulc-codec: Ultra-Low-Complexity Audio Codec
//! Copyright (C) 2022, Ruben Nunez (Aikku; aik AT aol DOT com DOT au)
//! Refer to the project README file for license terms.
/**************************************/
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
/**************************************/
#include "libfourier/Fourier.h"
#include "ulcDecoder.h"
#include "ulcHelper.h"
/**************************************/
#define BUFFER_ALIGNMENT 64u //! Always align memory to 64-byte boundaries (preparation for AVX-512)
/**************************************/

//! Just for consistency
#define MIN_CHANS     1
#define MAX_CHANS   255
#define MIN_BANDS   256
#define MAX_BANDS 32768

/**************************************/

//! Initialize decoder state
int ULC_DecoderState_Init(struct ULC_DecoderState_t *State) {
	//! Clear anything that is needed for EncoderState_Destroy()
	State->BufferData  = NULL;

	//! Verify parameters
	int nChan     = State->nChan;
	int BlockSize = State->BlockSize;
	if(nChan     < MIN_CHANS || nChan     > MAX_CHANS) return -1;
	if(BlockSize < MIN_BANDS || BlockSize > MAX_BANDS) return -1;
	if((BlockSize & (-BlockSize)) != BlockSize)        return -1;

	//! Get buffer offsets and allocation size
	int AllocSize = 0;
#define CREATE_BUFFER(Name, Sz) int Name##_Offs = AllocSize; AllocSize += Sz
	CREATE_BUFFER(TransformBuffer, sizeof(float) * (       BlockSize   ));
	CREATE_BUFFER(TransformTemp,   sizeof(float) * (nChan* BlockSize   ) * 2);
	CREATE_BUFFER(TransformInvLap, sizeof(float) * (nChan*(BlockSize/2)));
#undef CREATE_BUFFER

	//! Allocate buffer space
	char *Buf = State->BufferData = malloc(BUFFER_ALIGNMENT-1 + AllocSize);
	if(!Buf) return -1;

	//! Initialize state
	int i;
	Buf += (-(uintptr_t)Buf) & (BUFFER_ALIGNMENT-1);
	State->LastSubBlockSize = 0;
	State->TransformBuffer = (float*)(Buf + TransformBuffer_Offs);
	State->TransformTemp   = (float*)(Buf + TransformTemp_Offs);
	State->TransformInvLap = (float*)(Buf + TransformInvLap_Offs);
	for(i=0;i<nChan*(BlockSize/2);i++) State->TransformInvLap[i] = 0.0f;

	//! Success
	return 1;
}

/**************************************/

//! Destroy decoder state
void ULC_DecoderState_Destroy(struct ULC_DecoderState_t *State) {
	//! Free buffer space
	free(State->BufferData);
}

/**************************************/

//! Decode block
#define ESCAPE_SEQUENCE_STOP           (-1)
#define ESCAPE_SEQUENCE_STOP_NOISEFILL (-2)
static inline uint32_t Block_Decode_UpdateRandomSeed(void) {
	static uint32_t Seed = 1234567;
	Seed ^= Seed << 13; //! Xorshift
	Seed ^= Seed >> 17;
	Seed ^= Seed <<  5;
	return Seed;
}
static inline uint8_t Block_Decode_ReadNybble(const uint8_t **Src, int *Size) {
	//! Fetch and shift nybble
	uint8_t x = *(*Src);
	*Size += 4;
	if((*Size)%8u == 0) x >>= 4, (*Src)++;
	return x&0xF;
}
static inline int Block_Decode_ReadQuantizer(const uint8_t **Src, int *Size) {
	int           qi  = Block_Decode_ReadNybble(Src, Size); //! Fh,0h..Dh:      Quantizer change
	if(qi == 0xF) return ESCAPE_SEQUENCE_STOP_NOISEFILL;    //! Fh,Fh,Zh,Yh,Xh: Noise fill (to end; exp-decay)
	if(qi == 0xE) qi += Block_Decode_ReadNybble(Src, Size); //! Fh,Eh,0h..Ch:   Quantizer change (extended precision)
	if(qi == 0xE + 0xF) return ESCAPE_SEQUENCE_STOP;        //! Fh,Eh,Fh:       Zeros fill (to end)
	return qi;
}
static inline float Block_Decode_ExpandQuantizer(int qi) {
	return 0x1.0p-31f * ((1u<<(31-5)) >> qi); //! 1 / (2^5 * 2^qi)
}
static inline int Block_Decode_DecodeSubBlockCoefs(float *CoefDst, int N, const uint8_t **Src, int *Size) {
	int32_t n, v;

	//! Check first quantizer for Stop code
	v = Block_Decode_ReadQuantizer(Src, Size);
	if(v == ESCAPE_SEQUENCE_STOP) {
		//! [Fh,]Eh,Fh: Stop
		do *CoefDst++ = 0.0f; while(--N);
		return 1;
	}

	//! Unpack the [sub]block's coefficients
	float Quant = Block_Decode_ExpandQuantizer(v);
	for(;;) {
		//! -7h..-2h, +2..+7h: Normal
		v = Block_Decode_ReadNybble(Src, Size);
		if(v != 0x0 && v != 0x1 && v != 0x8 && v != 0xF) { //! <- Exclude all control codes
			//! Store linearized, dequantized coefficient
			v = (v^0x8) - 0x8; //! Sign extension
			v = (v < 0) ? (-v*v) : (+v*v);
			*CoefDst++ = v * Quant;
			if(--N == 0) break;
			continue;
		}

		//! 0h,0h..Fh: Zeros fill (1 .. 16 coefficients)
		if(v == 0x0) {
			n = Block_Decode_ReadNybble(Src, Size) + 1;
			if(n > N) return 0;
			N -= n;
			do *CoefDst++ = 0.0f; while(--n);
			if(N == 0) break;
			continue;
		}

		//! 1h,Yh,Xh: 33 .. 288 zeros fill
		if(v == 0x1) {
			n  = Block_Decode_ReadNybble(Src, Size);
			n  = Block_Decode_ReadNybble(Src, Size) | (n<<4);
			n += 33;
			if(n > N) return 0;
			N -= n;
			do *CoefDst++ = 0.0f; while(--n);
			if(N == 0) break;
			continue;
		}

		//! 8h,Zh,Yh,Xh: 16 .. 527 noise fill
		if(v == 0x8) {
			n  = Block_Decode_ReadNybble(Src, Size);
			n  = Block_Decode_ReadNybble(Src, Size) | (n<<4);
			v  = Block_Decode_ReadNybble(Src, Size);
			n  = (v&1) | (n<<1);
			v  = (v>>1) + 1;
			n += 16;
			if(n > N) return 0;
			N -= n; {
				float p = (v*v) * Quant * (1.0f/4);
				do {
					if(Block_Decode_UpdateRandomSeed() & 0x80000000) p = -p;
					*CoefDst++ = p;
				} while(--n);
			}
			if(N == 0) break;
			continue;
		}

		//! Fh,0h..Dh:    Quantizer change
		//! Fh,Eh,0h..Ch: Quantizer change (extended precision)
		v = Block_Decode_ReadQuantizer(Src, Size);
		if(v >= 0) {
			Quant = Block_Decode_ExpandQuantizer(v);
			continue;
		}

		//! Fh,Fh,Zh,Yh,Xh: Noise fill (to end; exp-decay)
		if(v == ESCAPE_SEQUENCE_STOP_NOISEFILL) {
			v = Block_Decode_ReadNybble(Src, Size) + 1;
			n = Block_Decode_ReadNybble(Src, Size);
			n = Block_Decode_ReadNybble(Src, Size) | (n<<4);
			float p = (v*v) * Quant * (1.0f/16);
			float r = 1.0f + (n*n)*-0x1.0p-19f;
			do {
				if(Block_Decode_UpdateRandomSeed() & 0x80000000) p = -p;
				*CoefDst++ = p, p *= r;
			} while(--N);
			break;
		}

		//! Fh,Eh,Dh: Unused
		//! Fh,Eh,Eh: Unused
		//! Fh,Eh,Fh: Zeros fill (to end)
		if(v == ESCAPE_SEQUENCE_STOP) {
			do *CoefDst++ = 0.0f; while(--N);
			break;
		}
	}
	return 1;
}
int ULC_DecodeBlock(struct ULC_DecoderState_t *State, float *DstData, const void *_SrcBuffer) {
	//! Spill state to local variables to make things easier to read
	int    n;
	int    nChan           = State->nChan;
	int    BlockSize       = State->BlockSize;
	float *TransformBuffer = State->TransformBuffer;
	float *TransformTemp   = State->TransformTemp;
	float *TransformInvLap = State->TransformInvLap;
	const uint8_t *SrcBuffer = _SrcBuffer;

	//! Begin decoding
	int Chan, Size = 0;
	int LastSubBlockSize = 0; //! <- Shuts gcc up
	int WindowCtrl; {
		//! Read window control information
		WindowCtrl = Block_Decode_ReadNybble(&SrcBuffer, &Size);
		if(WindowCtrl & 0x8) WindowCtrl |= Block_Decode_ReadNybble(&SrcBuffer, &Size) << 4;
		else                 WindowCtrl |= 1 << 4;
	}
	for(Chan=0;Chan<nChan;Chan++) {
		//! Reset overlap scaling for this channel
		LastSubBlockSize = State->LastSubBlockSize;

		//! Process subblocks
		float *Dst = DstData + Chan*BlockSize;
		float *Src = TransformBuffer;
		float *Lap = TransformInvLap;
		ULC_SubBlockDecimationPattern_t DecimationPattern = ULCi_SubBlockDecimationPattern(WindowCtrl);
		do {
			int SubBlockSize = BlockSize >> (DecimationPattern&0x7);
			if(!Block_Decode_DecodeSubBlockCoefs(Src, SubBlockSize, &SrcBuffer, &Size)) {
				//! Corrupt block
				return 0;
			}

			//! Get+update overlap size and limit to that of the last subblock
			int OverlapSize = SubBlockSize;
			if(DecimationPattern&0x8)
				OverlapSize >>= (WindowCtrl & 0x7);
			if(OverlapSize > LastSubBlockSize)
				OverlapSize = LastSubBlockSize;
			LastSubBlockSize = SubBlockSize;

			//! A single long block can be read straight into the output buffer
			if(SubBlockSize == BlockSize) {
				Fourier_IMDCT(Dst, Src, Lap, TransformTemp, SubBlockSize, OverlapSize);
				break;
			}

			//! For small blocks, we store the decoded data to a scratch buffer
			float *DecBuf = TransformTemp + SubBlockSize;
			Fourier_IMDCT(DecBuf, Src, Lap, TransformTemp, SubBlockSize, OverlapSize);

			//! Output samples from the lapping buffer, and cycle
			//! the new samples through it for the next call
			int nAvailable = (BlockSize - SubBlockSize) / 2;
			      float *LapDst = Lap + BlockSize/2;
			const float *LapSrc = LapDst;
			if(SubBlockSize <= nAvailable) {
				//! We have enough data in the lapping buffer
				//! to output a full subblock directly from it,
				//! so we do that and then shift any remaining
				//! data before re-filling the buffer.
				for(n=0;n<SubBlockSize;n++) *Dst++    = *--LapSrc;
				for(   ;n<nAvailable  ;n++) *--LapDst = *--LapSrc;
				for(n=0;n<SubBlockSize;n++) *--LapDst = *DecBuf++;
			} else {
				//! We only have enough data for a partial output
				//! from the lapping buffer, so output what we can
				//! and output the rest from the decoded buffer
				//! before re-filling.
				for(n=0;n<nAvailable;  n++) *Dst++    = *--LapSrc;
				for(   ;n<SubBlockSize;n++) *Dst++    = *DecBuf++;
				for(n=0;n<nAvailable;  n++) *--LapDst = *DecBuf++;
			}
		} while(DecimationPattern >>= 4);

		//! Move to next channel
		TransformInvLap += BlockSize/2;
	}

	//! Undo M/S transform
	//! NOTE: Not orthogonal; must be fully normalized on the encoder side.
	for(Chan=1;Chan<nChan;Chan+=2) {
		float *Buf = DstData + Chan*BlockSize;
		for(n=0;n<BlockSize;n++) {
			float a = Buf[n - BlockSize];
			float b = Buf[n];
			Buf[n - BlockSize] = (a+b);
			Buf[n]             = (a-b);
		}
	}

	//! Interleave channels
	if(nChan != 1) {
		for(n=0;n<BlockSize*nChan;n++) TransformTemp[n] = DstData[n];
		for(Chan=0;Chan<nChan;Chan++) for(n=0;n<BlockSize;n++) {
			DstData[n*nChan+Chan] = TransformTemp[Chan*BlockSize+n];
		}
	}

	//! Store the last [sub]block size, and return the number of bits read
	State->LastSubBlockSize = LastSubBlockSize;
	return Size;
}

/**************************************/
//! EOF
/**************************************/
