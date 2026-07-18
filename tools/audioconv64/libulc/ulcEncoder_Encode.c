/**************************************/
//! ulc-codec: Ultra-Low-Complexity Audio Codec
//! Copyright (C) 2024, Ruben Nunez (Aikku; aik AT aol DOT com DOT au)
//! Refer to the project README file for license terms.
/**************************************/
#include <math.h>
#include <stdint.h>
/**************************************/
#include "libfourier/Fourier.h"
#include "ulcEncoder.h"
#include "ulcHelper.h"
#include "ulcEncoder_Internals.h"
/**************************************/

//! The target memory is always aligned to 64 bytes, so just
//! use whatever is most performant on the target architecture
typedef uint8_t BitStream_t; //! <- MUST BE UNSIGNED
#define BITSTREAM_NBITS (8u*sizeof(BitStream_t))

/**************************************/

//! Write nybble to output
static void WriteNybble(BitStream_t x, BitStream_t *Dst, int *Size) {
	//! Push nybble
	BitStream_t *p = &Dst[*Size / BITSTREAM_NBITS];
	*p >>= 4;
	*p  |= x << (BITSTREAM_NBITS - 4);
	*Size += 4;
}

//! Write quantizer to output
static void WriteQuantizer(int qi, BitStream_t *DstBuffer, int *Size, int Lead) {
	int s = qi - 5;
	if(Lead) {
		WriteNybble(0xF, DstBuffer, Size);
	}
	if(s < 0xE) {
		//! Fh,0h..Dh: Quantizer change
		WriteNybble(s, DstBuffer, Size);
	} else {
		//! Fh,Eh,0h..Ch: Quantizer change (extended precision)
		WriteNybble(  0xE, DstBuffer, Size);
		WriteNybble(s-0xE, DstBuffer, Size);
	}
}

/**************************************/

//! Build quantizer from weighted average
static int BuildQuantizer(float MaxVal) {
	//! NOTE: `MaxVal` approaches (4/Pi)^2 as BlockSize
	//! approaches infinity, due to the p=1 norm of
	//! the MDCT matrix. Therefore, we use a bias of 5
	//! in the syntax to allow for a full range (that is
	//! to say: 7^2 * 2^-5 = 1.53125, 1.53125 >= 4/Pi).
	//! As for rounding, we want to minimize the error
	//! of e = |2^-q - MaxVal|:
	//!  We start with qm = IntegerPart[-Log2[MaxVal]].
	//!  We then choose q = qm, or qm+1, whichever has
	//!  smaller error.
	//!  Some identities:
	//!   MaxVal <= 2^-qm
	//!   MaxVal >= 2^-(qm+1)
	//!  Now we can say:
	//!   q = qm,   |2^-qm - MaxVal| < |2^-(qm+1) - MaxVal|
	//!       qm+1, |2^-(qm+1) - MaxVal| < |2^-qm - MaxVal|
	//!  In other words, we add 1 to qm if the latter
	//!  condition is met. Now simplification:
	//!    |2^-(qm+1) - MaxVal| < |2^-qm - MaxVal|
	//!  We know that 2^-(qm+1) <= MaxVal, so we can remove
	//!  the first absolute after negation:
	//!    (MaxVal - 2^-(qm+1)) < |2^-qm - MaxVal|
	//!  And we know that 2^-qm <= MaxVal:
	//!    (MaxVal - 2^-(qm+1)) < (2^-qm - MaxVal)
	//!  Now some algebraic manipulation leads to:
	//!    2^-qm/MaxVal > (4/3)
	//!  Putting it all together, we can finally state:
	//!   q = IntegerPart[-Log2[MaxVal*2/3]]
	//!  And since we have a bias of 5:
	//!   q = IntegerPart[5 - Log2[MaxVal*2/3]]
	//!     = IntegerPart[5 - (Log2[MaxVal] + Log2[2/3])]
	//!     = IntegerPart[(5 - Log2[2/3]) - Log2[MaxVal]]
	int q = (int)(0x1.657006p2f + -0x1.715476p0f*logf(MaxVal)); //! 0x1.657006p2 = 5-Log2[2/3], 0x1.715476p0 == 1/Ln[2] for change of base
	if(q < 5) q = 5;
	if(q > 5 + 0xE + 0xC) q = 5 + 0xE + 0xC; //! 5+Eh+Ch = Maximum extended-precision quantizer value (including a bias of 5)
	return q;
}

/**************************************/

//! Encode a range of coefficients
static int WriteQuantizerZone(
	int           CurIdx,
	int           EndIdx,
	float         Quant,
	const float  *Coef,
#if ULC_USE_NOISE_CODING
	const float  *CoefNoise,
#endif
	const int    *CoefIdx,
	int           NextCodedIdx,
	int           nOutCoef,
	BitStream_t  *DstBuffer,
	int          *Size
) {
	for(;;) {
		//! Seek the next viable coefficient
		while(CurIdx < EndIdx && CoefIdx[CurIdx] >= nOutCoef) CurIdx++;
		if(CurIdx >= EndIdx) break;

		//! If the coefficient collapses to become uncodeable, skip it
		//! Note that the expression we test here is the exact expansion
		//! of ULCi_CompandedQuantizeCoefficient(Coef[CurIdx]*Quant) < 2.
		if(ABS(Coef[CurIdx]*Quant) < 2.5f) { CurIdx++; continue; }

		//! We now know the coefficient can be coded, so write out the zeros/noise run(s).
		int n, v;
		int zR = CurIdx - NextCodedIdx;
		while(zR) {
			//! If we plan to skip two or less coefficients, try to encode
			//! them instead, as this is cheaper+better than filling with zero
			if(zR <= 2) {
				int Qn1, Qn2;
				if(1)       Qn1 = ULCi_CompandedQuantizeCoefficient(Coef[NextCodedIdx+0]*Quant, 0x7);
				if(zR >= 2) Qn2 = ULCi_CompandedQuantizeCoefficient(Coef[NextCodedIdx+1]*Quant, 0x7);
				if(ABS(Qn1) > 1 && (zR < 2 || ABS(Qn2) > 1)) {
					if(1)       WriteNybble(Qn1, DstBuffer, Size);
					if(zR >= 2) WriteNybble(Qn2, DstBuffer, Size);
					NextCodedIdx += zR;
					break;
				}
			}
#if ULC_USE_NOISE_CODING
			//! Determine the quantized coefficient for noise-fill mode
			//! NOTE: The range of noise samples is coded as a trade-off
			//! between noise-fill commands and coded coefficients, as
			//! noise-fill competes with coefficients we /want/ to code
			//! for the bit bandwidth available. Setting the start range
			//! too low favours noise-fill over psychoacoustically
			//! significant coefficients, but setting it too high can
			//! cause degradation in small block sizes as well as when
			//! we get many short runs.
			//! TODO: Is there a better min-number-of-coefficients?
			//! Too low and this fights too much with the coefficients
			//! we are actually trying to code (reducing the coding
			//! rate of them, and increasing the coding rate of noise).
			//! However, with small BlockSize (or SubBlockSize when
			//! decimating), smaller runs are useful.
			int NoiseQ = 0;
			if(zR >= 16) {
				v = zR - 16; if(v > 0x01FF) v = 0x01FF;
				n = v  + 16;
				NoiseQ = ULCi_GetNoiseQ(CoefNoise, NextCodedIdx, n, Quant);
			}
			if(NoiseQ) {
				//! 8h,Zh,Yh,Xh: 16 .. 527 noise fill (Xh != 0)
				WriteNybble(0x8,  DstBuffer, Size);
				WriteNybble(v>>5, DstBuffer, Size);
				WriteNybble(v>>1, DstBuffer, Size);
				WriteNybble((v&1) | ((NoiseQ-1)<<1), DstBuffer, Size);
			} else {
#endif
				//! Determine which run type to use and get the number of zeros coded
				//! NOTE: A short run takes 2 nybbles, and a long run takes 4 nybbles.
				//! So two short runs of maximum length code up to 32 zeros with the
				//! same efficiency as a long run, meaning that long runs start with
				//! 33 zeros.
				if(zR < 33) {
					//! 0h,0h..Fh: Zeros fill (1 .. 16 coefficients)
					v = zR - 1; if(v > 0xF) v = 0xF;
					n = v  + 1;
					WriteNybble(0x0, DstBuffer, Size);
					WriteNybble(v,   DstBuffer, Size);
				} else {
					//! 1h,Yh,Xh: 33 .. 542 zeros fill (Xh == 0)
					v = zR - 33; if(v > 0xFF) v = 0xFF;
					n = v  + 33;
					WriteNybble(0x1,  DstBuffer, Size);
					WriteNybble(v>>4, DstBuffer, Size);
					WriteNybble(v>>0, DstBuffer, Size);
				}
#if ULC_USE_NOISE_CODING
			}
#endif
			//! Skip the zeros
			NextCodedIdx += n;
			zR           -= n;
		}

		//! -7h..-2h, +2h..+7h: Normal coefficient
		int Qn = ULCi_CompandedQuantizeCoefficient(Coef[CurIdx]*Quant, 0x7);
		WriteNybble(Qn, DstBuffer, Size);
		NextCodedIdx++;
		CurIdx++;
	}
	return NextCodedIdx;
}

//! Encode a [sub]block
static void WriteSubBlock(
	int           Idx,
	int           SubBlockSize,
	const float  *Coef,
#if ULC_USE_NOISE_CODING
	const float  *CoefNoise,
#endif
	const int    *CoefIdx,
	int           nOutCoef,
	BitStream_t  *DstBuffer,
	int          *Size
) {
	//! Encode direct coefficients
	int EndIdx        = Idx+SubBlockSize;
	int NextCodedIdx  = Idx;
	int PrevQuant     = -1;
	int QuantStartIdx = -1;
	float QuantMin = 1000.0f, QuantMax = -1000.0f;
	do {
		//! Seek the next coefficient
		while(Idx < EndIdx && CoefIdx[Idx] >= nOutCoef) Idx++;

		//! Read coefficient and set the first quantizer's first coefficient index
		//! NOTE: Set NewMin=0.0 upon reaching the end. This causes the range
		//! check to fail in the next step, causing the final quantizer zone to dump
		//! if we had any data (if we don't, the check "passes" because NewMax==0).
		float NewMin = 0.0f;
		float NewMax = QuantMax;
		float CurLevel = 0.0f;
		if(Idx < EndIdx) {
			CurLevel = ABS(Coef[Idx]);
			NewMin = (CurLevel < QuantMin) ? CurLevel : QuantMin;
			NewMax = (CurLevel > QuantMax) ? CurLevel : QuantMax;
			if(QuantStartIdx == -1) QuantStartIdx = Idx;
		}

		//! Level out of range in this quantizer zone?
		const float MaxRange = 4.0f;
		if(NewMax > NewMin*MaxRange) {
			//! Write/update the quantizer
			int qi = BuildQuantizer(QuantMax);
			if(qi != PrevQuant) {
				WriteQuantizer(qi, DstBuffer, Size, PrevQuant != -1);
				PrevQuant = qi;
			}

			//! Write the quantizer zone we just searched through
			//! and start a new one from this coefficient
			NextCodedIdx = WriteQuantizerZone(
				QuantStartIdx,
				Idx,
				(float)(1u << qi),
				Coef,
#if ULC_USE_NOISE_CODING
				CoefNoise,
#endif
				CoefIdx,
				NextCodedIdx,
				nOutCoef,
				DstBuffer,
				Size
			);
			QuantStartIdx = Idx;
			QuantMin = QuantMax = CurLevel;
		} else {
			//! Update ranges
			QuantMin = NewMin;
			QuantMax = NewMax;
		}
	} while(++Idx <= EndIdx);

	//! Decide what to do about the tail coefficients
	//! If we're at the edge of the block, it might work better to just fill with 0h
	int n = EndIdx - NextCodedIdx;
	if(n > 4) {
		//! If we coded anything, then we must specify the lead sequence
		if(PrevQuant != -1) {
			WriteNybble(0xF, DstBuffer, Size);
		}

		//! Analyze the remaining data for noise-fill mode
#if ULC_USE_NOISE_CODING
		int NoiseQ = 0, NoiseDecay = 0;
		if(PrevQuant != -1 && n >= 16) { //! Don't use noise-fill for ultra-short tails
			ULCi_GetHFExtParams(
				CoefNoise,
				NextCodedIdx,
				n,
				(float)(1u << PrevQuant),
				&NoiseQ,
				&NoiseDecay
			);
		}
		if(NoiseQ) {
			//! Fh,Fh,Zh,Yh,Xh: Noise fill (to end; exp-decay)
			WriteNybble(0xF,           DstBuffer, Size);
			WriteNybble(NoiseQ-1,      DstBuffer, Size);
			WriteNybble(NoiseDecay>>4, DstBuffer, Size);
			WriteNybble(NoiseDecay,    DstBuffer, Size);
		} else {
#endif
			//! Fh,Eh,Fh: Stop
			WriteNybble(0xE, DstBuffer, Size);
			WriteNybble(0xF, DstBuffer, Size);
#if ULC_USE_NOISE_CODING
		}
#endif
	} else if(n > 0) {
		//! If we have less than 4 coefficients, it's cheaper to
		//! store a zero run than to do anything else.
		WriteNybble(0x0, DstBuffer, Size);
		WriteNybble(n-1, DstBuffer, Size);
	}
}

/**************************************/

//! Encode block to target buffer
//! Returns the block size (in bits) and the number of coded (non-zero) coefficients
int ULCi_EncodePass(const struct ULC_EncoderState_t *State, void *_DstBuffer, int nOutCoef) {
	int BlockSize   = State->BlockSize;
	int Chan, nChan = State->nChan;
	const float *Coef      = State->TransformBuffer;
#if ULC_USE_NOISE_CODING
	const float *CoefNoise = State->TransformNoise;
#endif
	const int   *CoefIdx   = State->TransformIndex;
	BitStream_t *DstBuffer = _DstBuffer;

	//! Begin coding
	int Idx  = 0;
	int Size = 0; //! Block size (in bits)
	int WindowCtrl = State->WindowCtrl; {
		WriteNybble(WindowCtrl, DstBuffer, &Size);
		if(WindowCtrl & 0x8) WriteNybble(WindowCtrl >> 4, DstBuffer, &Size);
	}
	for(Chan=0;Chan<nChan;Chan++) {
		ULC_SubBlockDecimationPattern_t DecimationPattern = ULCi_SubBlockDecimationPattern(WindowCtrl);
		do {
			int SubBlockSize = BlockSize >> (DecimationPattern&0x7);
			WriteSubBlock(
				Idx,
				SubBlockSize,
				Coef,
#if ULC_USE_NOISE_CODING
				CoefNoise,
#endif
				CoefIdx,
				nOutCoef,
				DstBuffer,
				&Size
			);
			Idx += SubBlockSize;
		} while(DecimationPattern >>= 4);
	}

	//! Align the output stream and pad size to bytes
	DstBuffer[Size/BITSTREAM_NBITS] >>= (-Size) % BITSTREAM_NBITS;
	Size = (Size+7) &~ 7;
	return Size;
}

/**************************************/
//! EOF
/**************************************/
