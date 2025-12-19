/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*------------------------------------------------------------------------------

    Table of contents

     1. Include headers
     2. External compiler flags
     3. Module defines
     4. Local function prototypes
     5. Functions
          h264bsdDecodeSliceData
          SetMbParams
          h264bsdMarkSliceCorrupted

------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "h264bsd_slice_data.h"
#include "h264bsd_util.h"
#include "h264bsd_vlc.h"

#include "../profile.h"

#ifdef H264BSD_N64
#include "../rsph264_internal.h"
#include "../fastcache.h"
#endif

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

static void SetMbParams(mbStorage_t *pMb, sliceHeader_t *pSlice, u32 sliceId,
    i32 chromaQpIndexOffset);

/*------------------------------------------------------------------------------

   5.1  Function name: h264bsdDecodeSliceData

        Functional description:
            Decode one slice. Function decodes stream data, i.e. macroblocks
            and possible skip_run fields. h264bsdDecodeMacroblock function is
            called to handle all other macroblock related processing.
            Macroblock to slice group mapping is considered when next
            macroblock to process is determined (h264bsdNextMbAddress function)
            map

        Inputs:
            pStrmData       pointer to stream data structure
            pStorage        pointer to storage structure
            currImage       pointer to current processed picture, needed for
                            intra prediction of the macroblocks
            pSliceHeader    pointer to slice header of the current slice

        Outputs:
            currImage       processed macroblocks are written to current image
            pStorage        mbStorage structure of each processed macroblock
                            is updated here

        Returns:
            HANTRO_OK       success
            HANTRO_NOK      invalid stream data

------------------------------------------------------------------------------*/

u32 h264bsdDecodeSliceData(strmData_t *pStrmData, storage_t *pStorage,
    image_t *currImage, sliceHeader_t *pSliceHeader)
{

/* Variables */

    u32 tmp;
    u32 skipRun;
    u32 prevSkipped;
    u32 currMbAddr;
    u32 moreMbs;
    u32 mbCount;
    i32 qpY;
    macroblockLayer_t *mbLayer;

/* Code */

    ASSERT(pStrmData);
    ASSERT(pSliceHeader);
    ASSERT(pStorage);
    ASSERT(pSliceHeader->firstMbInSlice < pStorage->picSizeInMbs);

    currMbAddr = pSliceHeader->firstMbInSlice;
    skipRun = 0;
    prevSkipped = HANTRO_FALSE;

    /* increment slice index, will be one for decoding of the first slice of
     * the picture */
    pStorage->slice->sliceId++;

    /* lastMbAddr stores address of the macroblock that was last successfully
     * decoded, needed for error handling */
    pStorage->slice->lastMbAddr = 0;

    mbCount = 0;
    /* initial quantization parameter for the slice is obtained as the sum of
     * initial QP for the picture and sliceQpDelta for the current slice */
    qpY = (i32)pStorage->activePps->picInitQp + pSliceHeader->sliceQpDelta;
    do
    {
        mbLayer = &pStorage->mbLayers[pStorage->mbLayerIdx];

#ifndef OPTIMIZE_NO_DECODED_FLAG
      /* primary picture and already decoded macroblock -> error */
        if (!pSliceHeader->redundantPicCnt && pStorage->mb[currMbAddr].decoded)
        {
            EPRINT("Primary and already decoded");
            return(HANTRO_NOK);
        }
#endif

        SetMbParams(pStorage->mb + currMbAddr, pSliceHeader,
            pStorage->slice->sliceId, pStorage->activePps->chromaQpIndexOffset);

        if (!IS_I_SLICE(pSliceHeader->sliceType))
        {
            if (!prevSkipped)
            {
                tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &skipRun);
                if (tmp != HANTRO_OK)
                    return(tmp);
                /* skip_run shall be less than or equal to number of
                 * macroblocks left */
                if (skipRun > (pStorage->picSizeInMbs - currMbAddr))
                {
                    EPRINT("skip_run");
                    return(HANTRO_NOK);
                }
                if (skipRun)
                {
                    prevSkipped = HANTRO_TRUE;
                    H264SwDecMemset(&mbLayer->mbPred, 0, sizeof(mbPred_t));
                    /* mark current macroblock skipped */
                    mbLayer->mbType = P_Skip;
                }
            }
        }

        PROFILE_START(PS_H264_LAYER, 0);
        if (skipRun)
        {
            //DEBUG(("Skipping macroblock %d\n", currMbAddr));
            skipRun--;
        }
        else
        {
            prevSkipped = HANTRO_FALSE;
            tmp = h264bsdDecodeMacroblockLayer(pStrmData, mbLayer,
                pStorage->mb + currMbAddr, pSliceHeader->sliceType,
                pSliceHeader->numRefIdxL0Active);
            if (tmp != HANTRO_OK)
            {
                EPRINT("macroblock_layer");
                return(tmp);
            }
        }
        PROFILE_STOP(PS_H264_LAYER, 0);

        PROFILE_START(PS_H264_MACROB, 0);
        tmp = h264bsdDecodeMacroblock(pStorage->mb + currMbAddr, mbLayer,
            currImage, pStorage->dpb, &qpY, currMbAddr,
            pStorage->activePps->constrainedIntraPredFlag);
        PROFILE_STOP(PS_H264_MACROB, 0);
        if (tmp != HANTRO_OK)
        {
            EPRINT("MACRO_BLOCK");
            return(tmp);
        }

        if (!prevSkipped) {
            #if H264BSD_N64_CAVLC
            // Read back the stream buffer pointer from RSP.
            // This does a smart sync waiting just for the CAVLC
            // function to finish (not a full sync).
            rsph264_cur_cavlc_buffer((const u8**)&pStrmData->pStrmCurrPos, (int*)&pStrmData->bitPosInWord);

            // Copy the totalCoeff into the macroblock. This is important
            // because CAVLC will use totalCoeff of adjacent macroblocks.
            // (to compute the "nc" value).
            mbStorage_t *pMb = pStorage->mb + currMbAddr;
            H264SwDecMemcpy(pMb->totalCoeff,
                            mbLayer->residual.totalCoeff,
                            27*sizeof(*pMb->totalCoeff));
            #endif

            // Update the stream buffer read bits counter.
            #ifndef H264BSD_N64
            pStrmData->strmBuffReadBits =
                (u32)(pStrmData->pStrmCurrPos - pStrmData->pStrmBuffStart) * 8 +
                pStrmData->bitPosInWord;
            #endif
        }

        /* increment macroblock count only for macroblocks that were decoded
         * for the first time (redundant slices) */
#ifndef OPTIMIZE_NO_DECODED_FLAG
        if (pStorage->mb[currMbAddr].decoded == 1)
#endif          
            mbCount++;

        /* keep on processing as long as there is stream data left or
         * processing of macroblocks to be skipped based on the last skipRun is
         * not finished */
        moreMbs = (h264bsdMoreRbspData(pStrmData) || skipRun) ?
                                        HANTRO_TRUE : HANTRO_FALSE;

        /* lastMbAddr is only updated for intra slices (all macroblocks of
         * inter slices will be lost in case of an error) */
        if (IS_I_SLICE(pSliceHeader->sliceType))
            pStorage->slice->lastMbAddr = currMbAddr;

        currMbAddr = h264bsdNextMbAddress(pStorage->sliceGroupMap,
            pStorage->picSizeInMbs, currMbAddr);
        if (!prevSkipped)
            pStorage->mbLayerIdx = (pStorage->mbLayerIdx+1)%NUM_PARALLEL_MACROBLOCKS;
        /* data left in the buffer but no more macroblocks for current slice
         * group -> error */
        if (moreMbs && !currMbAddr)
        {
            EPRINT("Next mb address");
            return(HANTRO_NOK);
        }

    } while (moreMbs);

    if ((pStorage->slice->numDecodedMbs + mbCount) > pStorage->picSizeInMbs)
    {
        EPRINT("Num decoded mbs");
        return(HANTRO_NOK);
    }

    pStorage->slice->numDecodedMbs += mbCount;

    return(HANTRO_OK);

}

/*------------------------------------------------------------------------------

   5.2  Function: SetMbParams

        Functional description:
            Set macroblock parameters that remain constant for this slice

        Inputs:
            pSlice      pointer to current slice header
            sliceId     id of the current slice
            chromaQpIndexOffset

        Outputs:
            pMb         pointer to macroblock structure which is updated

        Returns:
            none

------------------------------------------------------------------------------*/

void SetMbParams(mbStorage_t *pMb, sliceHeader_t *pSlice, u32 sliceId,
    i32 chromaQpIndexOffset)
{

/* Variables */
    u32 tmp1;
    i32 tmp2, tmp3;

/* Code */

    tmp1 = pSlice->disableDeblockingFilterIdc;
    tmp2 = pSlice->sliceAlphaC0Offset;
    tmp3 = pSlice->sliceBetaOffset;
    pMb->sliceId = sliceId;
    pMb->disableDeblockingFilterIdc = tmp1;
    pMb->filterOffsetA = tmp2;
    pMb->filterOffsetB = tmp3;
    pMb->chromaQpIndexOffset = chromaQpIndexOffset;

}

/*------------------------------------------------------------------------------

   5.3  Function name: h264bsdMarkSliceCorrupted

        Functional description:
            Mark macroblocks of the slice corrupted. If lastMbAddr in the slice
            storage is set -> picWidhtInMbs (or at least 10) macroblocks back
            from  the lastMbAddr are marked corrupted. However, if lastMbAddr
            is not set -> all macroblocks of the slice are marked.

        Inputs:
            pStorage        pointer to storage structure
            firstMbInSlice  address of the first macroblock in the slice, this
                            identifies the slice to be marked corrupted

        Outputs:
            pStorage        mbStorage for the corrupted macroblocks updated

        Returns:
            none

------------------------------------------------------------------------------*/

#ifndef OPTIMIZE_NO_DECODED_FLAG
void h264bsdMarkSliceCorrupted(storage_t *pStorage, u32 firstMbInSlice)
{

/* Variables */

    u32 tmp, i;
    u32 sliceId;
    u32 currMbAddr;

/* Code */

    ASSERT(pStorage);
    ASSERT(firstMbInSlice < pStorage->picSizeInMbs);

    currMbAddr = firstMbInSlice;

    sliceId = pStorage->slice->sliceId;

    /* DecodeSliceData sets lastMbAddr for I slices -> if it was set, go back
     * MAX(picWidthInMbs, 10) macroblocks and start marking from there */
    if (pStorage->slice->lastMbAddr)
    {
        ASSERT(pStorage->mb[pStorage->slice->lastMbAddr].sliceId == sliceId);
        i = pStorage->slice->lastMbAddr - 1;
        tmp = 0;
        while (i > currMbAddr)
        {
            if (pStorage->mb[i].sliceId == sliceId)
            {
                tmp++;
                if (tmp >= MAX(pStorage->activeSps->picWidthInMbs, 10))
                    break;
            }
            i--;
        }
        currMbAddr = i;
    }

    do
    {

        if ( (pStorage->mb[currMbAddr].sliceId == sliceId) &&
             (pStorage->mb[currMbAddr].decoded) )
        {
            pStorage->mb[currMbAddr].decoded--;
        }
        else
        {
            break;
        }

        currMbAddr = h264bsdNextMbAddress(pStorage->sliceGroupMap,
            pStorage->picSizeInMbs, currMbAddr);

    } while (currMbAddr);

}
#endif
