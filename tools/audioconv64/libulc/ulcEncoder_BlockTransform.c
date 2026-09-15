/**************************************/
//! ulc-codec: Ultra-Low-Complexity Audio Codec
//! Copyright (C) 2025, Ruben Nunez (Aikku; aik AT aol DOT com DOT au)
//! Refer to the project README file for license terms.
/**************************************/
#include <math.h>
#include <stddef.h>
#include <stdint.h>
/**************************************/
#include "libfourier/Fourier.h"
#include "ulcHelper.h"
#include "ulcEncoder_Internals.h"
/**************************************/

//! Implementation for heapsort
//! This is used because qsort() tends to use quicksort, which
//! has some nasty pathological cases, so I'd rather have a
//! sort method that has more consistent timing, even if it's
//! very slightly slower for the average case.
static void Heapsort_SiftDown(const float *SortValues, int *Order, int Root, int N) {
	//! NOTE: Most of the sorting time is spent in this function,
	//! so I've tried to optimize it as best as I could. The
	//! code below is what performed best on my Intel i7 CPU,
	//! so mileage may vary. There may exist other variations
	//! that have even better performance, but this is good
	//! enough that it probably doesn't need it anyway.
	int Child = 2*Root+1;
	if(Child < N) for(;;) {
		//! Get the values of the root and child
		//! NOTE: If a sibling exists, check it also.
		//! NOTE: Using pre-increment (combined with post-decrement
		//! for the FALSE case) appears to result in better performance?
		int RootIdx  = Order[Root];
		int ChildIdx = Order[Child];
		if(++Child < N && SortValues[Order[Child]] < SortValues[ChildIdx])
			ChildIdx = Order[Child];
		else Child--;

		//! Already in order? Exit
		if(SortValues[ChildIdx] > SortValues[RootIdx]) return;

		//! Swap to restore heap order, and travel further down the heap
		//! NOTE: Stashing Order[Root],Order[Child] in temps appears to
		//! give slightly worse performance. This may be an x86 quirk
		//! due to its low register count, though.
		Order[Root]  = ChildIdx;
		Order[Child] = RootIdx;
		Root = Child, Child = 2*Root+1;
		if(Child >= N) return;
	}
}
static void SortIndices(int *SortedIndices, const float *SortValues, int *Temp, int N) {
	int n;

	//! Start with mapping the indices directly
	int *Order = Temp;
	for(n=0;n<N;n++) Order[n] = n;

	//! Begin sorting
	//! NOTE: This was heavily borrowed from Rosetta Code (Heapsort).
	//! qsort() has some edge cases that degrade performance enough
	//! to roll our own variant here.
	{
		//! Heapify the array
		n = N/2u - 1;
		do Heapsort_SiftDown(SortValues, Order, n, N); while(--n >= 0);

		//! Pop all elements off the heap one at a time
		n = N-1;
		do {
			SortedIndices[Order[0]] = n;
			Order[0] = Order[n];
			Heapsort_SiftDown(SortValues, Order, 0, n);
		} while(--n);
		SortedIndices[Order[0]] = n;
	}
}

/**************************************/

//! Transform a block and prepare its coefficients
int ULCi_TransformBlock(struct ULC_EncoderState_t *State, const float *Data) {
	int nChan     = State->nChan;
	int BlockSize = State->BlockSize;

	//! Append new data samples
	{
		int n, Chan;
		float *Old = State->SampleBuffer;
		float *New = State->SampleBuffer + BlockSize*nChan;

		//! Shift last block
		for(n=0;n<BlockSize*nChan;n++) Old[n] = New[n];

		//! Deinterleave samples to new buffer
		for(Chan=0;Chan<nChan;Chan++) for(n=0;n<BlockSize;n++) {
			New[Chan*BlockSize+n] = Data[n*nChan+Chan];
		}

		//! Apply M/S transform to data
		//! NOTE: Fully normalized; not orthogonal.
		for(Chan=1;Chan<nChan;Chan+=2) {
			float *Buf = New + Chan*BlockSize;
			for(n=0;n<BlockSize;n++) {
				float a = Buf[n - BlockSize];
				float b = Buf[n];
				Buf[n - BlockSize] = (a+b) * 0.5f;
				Buf[n]             = (a-b) * 0.5f;
			}
		}
	}

	//! Get the window control parameters for this block and the next
	int WindowCtrl     = State->WindowCtrl     = State->NextWindowCtrl;
	int NextWindowCtrl = State->NextWindowCtrl = ULCi_GetWindowCtrl(
		State->SampleBuffer,
		State->TransientBuffer,
		State->TransientFilter,
		State->TransformTemp,
		BlockSize,
		nChan,
		State->RateHz
	);
	int NextBlockOverlap; {
		int Pattern = ULCi_SubBlockDecimationPattern(NextWindowCtrl);
		NextBlockOverlap = BlockSize >> (Pattern&0x7);
		if(Pattern&0x8) NextBlockOverlap >>= (NextWindowCtrl&0x7);
	}

	//! Transform channels and insert keys for each codeable coefficient
	//! It's not /strictly/ required to calculate nNzCoef, but it can
	//! speed things up in the rate-control step
	int nNzCoef = 0; {
		int n, Chan;
		float *BufferSamples = State->SampleBuffer;
		float *BufferMDCT    = State->TransformBuffer;
		float *BufferIndex   = (float*)State->TransformIndex;
		float *BufferFwdLap  = State->TransformFwdLap;
#if ULC_USE_NOISE_CODING
		float *BufferNoise   = State->TransformNoise;

		//! Clear the noise buffer; we'll be adding two MDCT/MDST lines into one pseudo-DFT line here
		//! and then split it out into {Weight,Weight*LogNoiseLevel} pairs
		for(n=0;n<nChan*BlockSize;n++) BufferNoise[n] = 0.0f;
#endif
		float *BufferTemp    = State->TransformTemp;
#if ULC_USE_PSYCHOACOUSTICS
		float *MaskingNp     = BufferSamples;          //! NOTE: Aliasing of BufferSamples
		float *BufferAmp2    = BufferTemp + BlockSize; //! NOTE: Using upper half of BufferTemp

		//! Clear the amplitude buffer; we'll be accumulating all channels here
		for(n=0;n<BlockSize/2;n++) BufferAmp2[n] = 0.0f;
#endif
		//! Transform the input data and get complexity measure (ABR, VBR modes)
		float Complexity = 0.0f, ComplexityW = 0.0f;
		for(Chan=0;Chan<nChan;Chan++) {
			ULC_SubBlockDecimationPattern_t DecimationPattern = ULCi_SubBlockDecimationPattern(WindowCtrl);
			do {
				//! Get the size of this subblock and the overlap at the next
				int SubBlockSize = BlockSize >> (DecimationPattern&0x7);
				int OverlapSize; {
					//! If we're still in the same block, poll the next subblock's
					//! size. Otherwise, use the next block's first [sub]block
					DecimationPattern >>= 4;
					if(DecimationPattern) {
						OverlapSize = BlockSize >> (DecimationPattern&0x7);
						if(DecimationPattern&0x8) OverlapSize >>= (WindowCtrl&0x7);
					} else OverlapSize = NextBlockOverlap;

					//! Limit overlap to the maximum allowed by this subblock
					if(OverlapSize > SubBlockSize) OverlapSize = SubBlockSize;
				}

				//! Cycle data through the lapping buffer
				float *SmpBuf = BufferTemp; {
					/*!   |            . | .____________|
					      |            . |/.            |
					      |            . | .            |
					      |            . | .            |
					      |____________./| .            |
					      |            . | .            |
					      <-    L    -><-M-><-    R    ->
					      <-BlockSize/2->|<-BlockSize/2->
					      <-         BlockSize         ->

					    L_Size = (BlockSize - SubBlockSize)/2
					    M_Size = SubBlockSize
					    R_Size = (BlockSize - SubBlockSize)/2
					    L_Offs = 0
					    M_Offs = (BlockSize - SubBlockSize)/2
					    R_Offs = (BlockSize + SubBlockSize)/2

					    The L segment contains all 0s.
					    The M segment contains our lapping data.
					    The R segment contains data that we must transform.

					    Therefore:
					    When using Fourier_MDCT_MDST(), we must align BufferFwdLap
					    with the M segment, and cycle data through the R segment.
					!*/

					//! Do we have the full [sub]block in the R side of the lapping buffer?
					int nAvailable = (BlockSize-SubBlockSize)/2;
					      float *SmpDst = SmpBuf;
					const float *SmpSrc = BufferSamples;
					      float *LapDst = BufferFwdLap + (BlockSize+SubBlockSize)/2;
					const float *LapSrc = LapDst;
					if(nAvailable < SubBlockSize) {
						//! Don't have enough samples in lapping buffer
						//! for the full block - stream new data in and
						//! refill the lapping buffer
						for(n=0;n<nAvailable;  n++) *SmpDst++ = *LapSrc++;
						for(   ;n<SubBlockSize;n++) *SmpDst++ = *SmpSrc++;
						for(n=0;n<nAvailable;  n++) *LapDst++ = *SmpSrc++;
					} else {
						//! We got a full [sub]block, and we might have data
						//! remaining in the lapping buffer. This data must
						//! now be shifted down and then the lapping buffer
						//! refilled with new data
						for(n=0;n<SubBlockSize;n++) *SmpDst++ = *LapSrc++;
						for(   ;n<nAvailable;  n++) *LapDst++ = *LapSrc++;
						for(n=0;n<SubBlockSize;n++) *LapDst++ = *SmpSrc++;
					}
				}

				//! Perform the actual MDCT+MDST
				//! NOTE: BufferSamples[] used for MDST output
				float *BufferMDST = BufferSamples;
				Fourier_MDCT_MDST(
					BufferMDCT,
					BufferMDST,
					SmpBuf,
					BufferFwdLap + (BlockSize-SubBlockSize)/2,
					BufferTemp,
					SubBlockSize,
					OverlapSize
				);

				//! Normalize the spectra, and then accumulate the
				//! coefficients for psychoacoustic analysis.
				//! MDCT is treated as the Real part of a DFT,
				//! while MDST is treated as the Imaginary part.
				float Norm = 2.0f / SubBlockSize;
				for(n=0;n<SubBlockSize;n++) {
					//! NOTE: MDST is only used here, so don't bother
					//! storing back the normalized data once we're done.
					//! NOTE: When the MDCT coefficient is out of bounds
					//! for encoding, set an extremely large negative
					//! value so that it gets sent to the back of the
					//! priority list for rate/quality control.
					//! NOTE: When storing to BufferIndex[], we form the
					//! first part of the psychoacoustics equation, and
					//! will correct it after we've got that data.
					float Re = (BufferMDCT[n] *= Norm), Re2 = SQR(Re);
					float Im = (BufferMDST[n] *  Norm), Im2 = SQR(Im);
					(void)Im2; //! <- Needed to avoid warning with ULC_USE_PSYCHOACOUSTICS==0
					float AbsRe = ABS(Re);
#if ULC_USE_NOISE_CODING || ULC_USE_PSYCHOACOUSTICS
					float Abs2 = Re2 + Im2;
#endif
					if(AbsRe < 0.5f*ULC_COEF_EPS) {
						BufferIndex[n] = -INFINITY;
					} else {
						float Level = Re2;
						BufferIndex[n] = FastLog(Level);
						nNzCoef++;
					}
#if ULC_USE_NOISE_CODING
					BufferNoise[n/2] += Abs2; //! <- DCT/DFT weirdness; two MDCT+MDST coefficients = One frequency line
#endif
#if ULC_USE_PSYCHOACOUSTICS
					BufferAmp2[n/2] += Abs2;  //! <- DCT/DFT weirdness
#endif
					//! NOTE: Using MDCT coefficients for complexity analysis
					//! works out much better than combined MDCT+MDST as the
					//! behaviour is far less eratic (and also gives accurate
					//! statistics about the encoding performance, since we
					//! don't actually code MDST coefficients).
					Complexity  += Re2;
					ComplexityW += AbsRe;
				}
#if ULC_USE_NOISE_CODING
				//! Compute noise spectrum
				//! NOTE: This outputs 2*(SubBlockSize/2) values into BufferNoise,
				//! corresponding to {Weight,Weight*LogNoiseLevel} pairs.
				ULCi_CalculateNoiseLogSpectrum(BufferNoise, BufferTemp, SubBlockSize, State->RateHz);
#endif
				//! Move to the next subblock
				BufferSamples += SubBlockSize;
				BufferMDCT    += SubBlockSize;
				BufferIndex   += SubBlockSize;
#if ULC_USE_PSYCHOACOUSTICS
				BufferAmp2    += SubBlockSize/2;
#endif
#if ULC_USE_NOISE_CODING
				BufferNoise   += SubBlockSize;
#endif
			} while(DecimationPattern);

			//! Move to the next channel
			BufferFwdLap += BlockSize;
#if ULC_USE_PSYCHOACOUSTICS
			BufferAmp2   -= BlockSize/2; //! <- Accumulated across all channels - rewind
#endif
		}
		BufferMDCT  -= BlockSize*nChan; //! Rewind to start of buffer
		BufferIndex -= BlockSize*nChan;

		//! Finalize and store block complexity
		if(Complexity) {
			//! Based off the same principles of normalized entropy:
			//!  Entropy = (Log[Total[x]] - Total[x*Log[x]]/Total[x]) / Log[N]
			//! Instead of accumulating log values, we accumulate
			//! raw values, meaning we need to take the log:
			//!  Total[x*Log[x]]/Total[x] -> Log[Total[x*x]/Total[x]]
			//! Simplifying:
			//!   (Log[Total[x]] - Log[Total[x*x]/Total[x]]) / Log[N]
			//!  =(Log[Total[x] / (Total[x*x]/Total[x])) / Log[N]
			//!  =Log[Total[x]^2 / Total[x^2]] / Log[N]
			float ComplexityScale = 0x1.62E430p-1f*(31 - __builtin_clz(BlockSize)); //! 0x1.62E430p-1 = 1/Log2[E] for change-of-base
			Complexity = logf(SQR(ComplexityW) / Complexity) / ComplexityScale;
			if(Complexity < 0.0f) Complexity = 0.0f; //! In case of round-off error
			if(Complexity > 1.0f) Complexity = 1.0f;
		}
		State->BlockComplexity = Complexity;
#if ULC_USE_PSYCHOACOUSTICS
		//! Perform psychoacoustics analysis
		//! NOTE: Trashes BufferAmp2[]
		ULCi_CalculatePsychoacoustics(MaskingNp, BufferAmp2, BufferTemp, BlockSize, State->RateHz, WindowCtrl);

		//! Add the psychoacoustics adjustment to the importance levels
		//! NOTE: No need to split this section into subblock handling.
		//! All the coefficients and their levels are in order relative
		//! to that of the output of psychoacoustics.
		//! NOTE: Because we stored out-of-range values as -INFINITY,
		//! we can do simple arithmetic on them without affecting things.
		for(Chan=0;Chan<nChan;Chan++) {
			for(n=0;n<BlockSize;n++) {
				float ValNp = BufferIndex[n];
				//if(ValNp != -INFINITY) {
					BufferIndex[n] = 2*ValNp + MaskingNp[n/2] + -0x1.62E430p0f*(Chan&1); //! -0x1.62E430p0 = Log[0.5^2]
				//}
			}
			BufferIndex += BlockSize;
		}
#endif
	}

	//! Create the coefficient sorting indices
	{
		int *BufferTmp = (int*)State->TransformTemp;
		int *BufferIdx = State->TransformIndex;
		SortIndices(BufferIdx, (float*)BufferIdx, BufferTmp, nChan * BlockSize);
	}
	return nNzCoef;
}

/**************************************/
//! EOF
/**************************************/
