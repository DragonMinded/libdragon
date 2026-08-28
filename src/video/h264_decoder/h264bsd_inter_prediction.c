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
          h264bsdInterPrediction
          MvPrediction16x16
          MvPrediction16x8
          MvPrediction8x16
          MvPrediction8x8
          MvPrediction
          MedianFilter
          GetInterNeighbour
          GetPredictionMv

------------------------------------------------------------------------------*/

#include "opttarget.h"

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "h264bsd_inter_prediction.h"
#include "h264bsd_neighbour.h"
#include "h264bsd_util.h"
#include "h264bsd_reconstruct.h"
#include "h264bsd_dpb.h"

#ifdef H264BSD_N64
#include "../fastcache.h"

// We can substitute the whole h264bsdPredictSamples with our
// RSP-based implementation of inter-prediction with overfill.
#define h264bsdPredictSamples(currImage,mv,refPic,colAndRow,part,pFill) \
  n64PredictSamples(currImage,mv,refPic,colAndRow,part)

static inline void n64PredictSamples(image_t *img, mv_t *mv, image_t *refPic, u32 colAndRow, u32 part);
static inline void n64SetWeights(const sliceHeader_t *pSliceHeader, u32 refIdx);

#endif



/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

typedef struct
{
    u32 available;
    u32 refIndex;
    mv_t mv;
} interNeighbour_t;

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

static u32 MvPrediction16x16(mbStorage_t *pMb, mbPred_t *mbPred,
    dpbStorage_t *dpb);
static u32 MvPrediction16x8(mbStorage_t *pMb, mbPred_t *mbPred,
    dpbStorage_t *dpb);
static u32 MvPrediction8x16(mbStorage_t *pMb, mbPred_t *mbPred,
    dpbStorage_t *dpb);
static u32 MvPrediction8x8(mbStorage_t *pMb, subMbPred_t *subMbPred,
    dpbStorage_t *dpb);
static u32 MvPrediction(mbStorage_t *pMb, subMbPred_t *subMbPred,
    u32 mbPartIdx, u32 subMbPartIdx);
static i32 MedianFilter(i32 a, i32 b, i32 c);

static void GetInterNeighbour(u32 sliceId, mbStorage_t *nMb,
    interNeighbour_t *n, u32 index);
static void GetPredictionMv(mv_t *mv, interNeighbour_t *a, u32 refIndex);

#if !H264BSD_N64

static void ApplyWeightPart(image_t *currImage,
    const sliceHeader_t *pSliceHeader, u32 part, u32 refIdx)
{
    u32 x, y;
    u32 partX, partY, partWidth, partHeight;
    u32 lumaPitch, chromaPitch;
    u32 lumaDenom, chromaDenom;
    i32 lumaRound, chromaRound;
    i32 lumaWeight, lumaOffset;
    i32 chromaWeightCb, chromaWeightCr;
    i32 chromaOffsetCb, chromaOffsetCr;
    u8 *pLuma, *pCb, *pCr;

    if (pSliceHeader == NULL || !pSliceHeader->weightedPredFlag)
        return;
    if (refIdx >= MAX_NUM_REF_PICS)
        return;

    partX = (part & 0xFF000000) >> 24;
    partY = (part & 0x00FF0000) >> 16;
    partWidth = (part & 0x0000FF00) >> 8;
    partHeight = (part & 0x000000FF);

    lumaDenom = pSliceHeader->predWeightTable.lumaLog2WeightDenom;
    chromaDenom = pSliceHeader->predWeightTable.chromaLog2WeightDenom;
    lumaRound = lumaDenom ? (1 << (lumaDenom - 1)) : 0;
    chromaRound = chromaDenom ? (1 << (chromaDenom - 1)) : 0;

    lumaWeight = pSliceHeader->predWeightTable.lumaWeightL0[refIdx];
    lumaOffset = pSliceHeader->predWeightTable.lumaOffsetL0[refIdx];
    chromaWeightCb = pSliceHeader->predWeightTable.chromaWeightL0[refIdx][0];
    chromaWeightCr = pSliceHeader->predWeightTable.chromaWeightL0[refIdx][1];
    chromaOffsetCb = pSliceHeader->predWeightTable.chromaOffsetL0[refIdx][0];
    chromaOffsetCr = pSliceHeader->predWeightTable.chromaOffsetL0[refIdx][1];

    lumaPitch = currImage->width * 16;
    pLuma = currImage->luma + lumaPitch * partY + partX;
    if (lumaWeight != (1 << lumaDenom) || lumaOffset != 0)
    {
        for (y = 0; y < partHeight; y++)
        {
            u8 *row = pLuma + y * lumaPitch;
            for (x = 0; x < partWidth; x++)
            {
                i32 pred = row[x];
                i32 weighted = ((lumaWeight * pred + lumaRound) >> lumaDenom) +
                    lumaOffset;
                row[x] = CLIP1(weighted);
            }
        }
    }

    chromaPitch = currImage->width * 8;
    pCb = currImage->cb + (partY >> 1) * chromaPitch + (partX >> 1);
    pCr = currImage->cr + (partY >> 1) * chromaPitch + (partX >> 1);
    if (chromaWeightCb != (1 << chromaDenom) || chromaOffsetCb != 0 ||
        chromaWeightCr != (1 << chromaDenom) || chromaOffsetCr != 0)
    {
        for (y = 0; y < (partHeight >> 1); y++)
        {
            u8 *rowCb = pCb + y * chromaPitch;
            u8 *rowCr = pCr + y * chromaPitch;
            for (x = 0; x < (partWidth >> 1); x++)
            {
                i32 predCb = rowCb[x];
                i32 predCr = rowCr[x];
                i32 weightedCb = ((chromaWeightCb * predCb + chromaRound) >>
                    chromaDenom) + chromaOffsetCb;
                i32 weightedCr = ((chromaWeightCr * predCr + chromaRound) >>
                    chromaDenom) + chromaOffsetCr;
                rowCb[x] = CLIP1(weightedCb);
                rowCr[x] = CLIP1(weightedCr);
            }
        }
    }
}
#endif

static inline void PredictPart(image_t *currImage, mv_t *pMv, image_t *refImage,
    u32 colAndRow, u32 part, u8 *pFill, u32 refIdx,
    const sliceHeader_t *pSliceHeader)
{
#if H264BSD_N64
    /* the RSP applies the weights as part of the prediction task, so the
     * coefficients must be configured before queueing it */
    n64SetWeights(pSliceHeader, refIdx);
#endif
    h264bsdPredictSamples(currImage, pMv, refImage, colAndRow, part, pFill);
#if !H264BSD_N64
    ApplyWeightPart(currImage, pSliceHeader, part, refIdx);
#endif
}

static const neighbour_t N_A_SUB_PART[4][4][4] = {
    { { {MB_A,5}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_A,5}, {MB_A,7}, {MB_NA,0}, {MB_NA,0} },
      { {MB_A,5}, {MB_CURR,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_A,5}, {MB_CURR,0}, {MB_A,7}, {MB_CURR,2} } },

    { { {MB_CURR,1}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,1}, {MB_CURR,3}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,1}, {MB_CURR,4}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,1}, {MB_CURR,4}, {MB_CURR,3}, {MB_CURR,6} } },

    { { {MB_A,13}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_A,13}, {MB_A,15}, {MB_NA,0}, {MB_NA,0} },
      { {MB_A,13}, {MB_CURR,8}, {MB_NA,0}, {MB_NA,0} },
      { {MB_A,13}, {MB_CURR,8}, {MB_A,15}, {MB_CURR,10} } },

    { { {MB_CURR,9}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,9}, {MB_CURR,11}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,9}, {MB_CURR,12}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,9}, {MB_CURR,12}, {MB_CURR,11}, {MB_CURR,14} } } };

static const neighbour_t N_B_SUB_PART[4][4][4] = {
    { { {MB_B,10}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_B,10}, {MB_CURR,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_B,10}, {MB_B,11}, {MB_NA,0}, {MB_NA,0} },
      { {MB_B,10}, {MB_B,11}, {MB_CURR,0}, {MB_CURR,1} } },

    { { {MB_B,14}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_B,14}, {MB_CURR,4}, {MB_NA,0}, {MB_NA,0} },
      { {MB_B,14}, {MB_B,15}, {MB_NA,0}, {MB_NA,0} },
      { {MB_B,14}, {MB_B,15}, {MB_CURR,4}, {MB_CURR,5} } },

    { { {MB_CURR,2}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,2}, {MB_CURR,8}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,2}, {MB_CURR,3}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,2}, {MB_CURR,3}, {MB_CURR,8}, {MB_CURR,9} } },

    { { {MB_CURR,6}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,6}, {MB_CURR,12}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,6}, {MB_CURR,7}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,6}, {MB_CURR,7}, {MB_CURR,12}, {MB_CURR,13} } } };

static const neighbour_t N_C_SUB_PART[4][4][4] = {
    { { {MB_B,14}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_B,14}, {MB_NA,4}, {MB_NA,0}, {MB_NA,0} },
      { {MB_B,11}, {MB_B,14}, {MB_NA,0}, {MB_NA,0} },
      { {MB_B,11}, {MB_B,14}, {MB_CURR,1}, {MB_NA,4} } },

    { { {MB_C,10}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_C,10}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_B,15}, {MB_C,10}, {MB_NA,0}, {MB_NA,0} },
      { {MB_B,15}, {MB_C,10}, {MB_CURR,5}, {MB_NA,0} } },

    { { {MB_CURR,6}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,6}, {MB_NA,12}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,3}, {MB_CURR,6}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,3}, {MB_CURR,6}, {MB_CURR,9}, {MB_NA,12} } },

    { { {MB_NA,2}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_NA,2}, {MB_NA,8}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,7}, {MB_NA,2}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,7}, {MB_NA,2}, {MB_CURR,13}, {MB_NA,8} } } };

static const neighbour_t N_D_SUB_PART[4][4][4] = {
    { { {MB_D,15}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_D,15}, {MB_A,5}, {MB_NA,0}, {MB_NA,0} },
      { {MB_D,15}, {MB_B,10}, {MB_NA,0}, {MB_NA,0} },
      { {MB_D,15}, {MB_B,10}, {MB_A,5}, {MB_CURR,0} } },

    { { {MB_B,11}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_B,11}, {MB_CURR,1}, {MB_NA,0}, {MB_NA,0} },
      { {MB_B,11}, {MB_B,14}, {MB_NA,0}, {MB_NA,0} },
      { {MB_B,11}, {MB_B,14}, {MB_CURR,1}, {MB_CURR,4} } },

    { { {MB_A,7}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_A,7}, {MB_A,13}, {MB_NA,0}, {MB_NA,0} },
      { {MB_A,7}, {MB_CURR,2}, {MB_NA,0}, {MB_NA,0} },
      { {MB_A,7}, {MB_CURR,2}, {MB_A,13}, {MB_CURR,8} } },

    { { {MB_CURR,3}, {MB_NA,0}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,3}, {MB_CURR,9}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,3}, {MB_CURR,6}, {MB_NA,0}, {MB_NA,0} },
      { {MB_CURR,3}, {MB_CURR,6}, {MB_CURR,9}, {MB_CURR,12} } } };



#ifdef H264BSD_N64
#include "../rsph264_internal.h"

static inline void n64SetWeights(
  const sliceHeader_t *pSliceHeader,
  u32 refIdx) {

  const predWeightTable_t *pWeights;
  u32 lumaDenom, chromaDenom;

  if (pSliceHeader == NULL || !pSliceHeader->weightedPredFlag) {
    rsph264_queue_set_weights_if_changed(RSPH264_WEIGHT_IDENTITY,
        RSPH264_WEIGHT_IDENTITY, RSPH264_WEIGHT_IDENTITY);
    return;
  }

  assertf(refIdx < MAX_NUM_REF_PICS, "invalid reference index %u", (unsigned)refIdx);
  pWeights = &pSliceHeader->predWeightTable;
  lumaDenom = pWeights->lumaLog2WeightDenom;
  chromaDenom = pWeights->chromaLog2WeightDenom;

  rsph264_queue_set_weights_if_changed(
      rsph264_weight_pack(pWeights->lumaWeightL0[refIdx],
          pWeights->lumaOffsetL0[refIdx], lumaDenom),
      rsph264_weight_pack(pWeights->chromaWeightL0[refIdx][0],
          pWeights->chromaOffsetL0[refIdx][0], chromaDenom),
      rsph264_weight_pack(pWeights->chromaWeightL0[refIdx][1],
          pWeights->chromaOffsetL0[refIdx][1], chromaDenom));
}

static inline void n64PredictSamples(
  image_t *img,
  mv_t *mv,
  image_t *refPic,
  u32 colAndRow,
  u32 part) {

  uint32_t  partX = (part & 0xFF000000) >> 24;
  uint32_t  partY = (part & 0x00FF0000) >> 16;
  uint32_t  partWidth = (part & 0x0000FF00) >> 8;
  uint32_t  partHeight = (part & 0x000000FF);

  uint32_t  partPos = (partX<<16) | (partY);

  uint32_t  lumaPitch = img->width*16;
  uint32_t  chromaPitch = img->width*8;

  rsph264_queue_interpolate_all_overfill(RSPH264_CACHE_SKIP_ALL,
      refPic->data, refPic->width*16,
      img->luma + lumaPitch*partY + partX,
      img->cb + chromaPitch*partY/2 + partX/2,
      img->cr + chromaPitch*partY/2 + partX/2, 
      img->width*16,
      (refPic->width*16) << 16 | (refPic->height*16),
      (partWidth << 16) | partHeight,
      *(uint32_t*)mv, colAndRow + partPos
  );
}

#endif

/*------------------------------------------------------------------------------

    Function: h264bsdInterPrediction

        Functional description:
          Processes one inter macroblock. Performs motion vector prediction
          and reconstructs prediction macroblock. Writes the final macroblock
          (prediction + residual) into the output image (currImage)

        Inputs:
          pMb           pointer to macroblock specific information
          pMbLayer      pointer to current macroblock data from stream
          dpb           pointer to decoded picture buffer
          mbNum         current macroblock number
          currImage     pointer to output image
          data          pointer where predicted macroblock will be stored

        Outputs:
          pMb           structure is updated with current macroblock
          currImage     current macroblock is written into image
          data          prediction is stored here

        Returns:
          HANTRO_OK     success
          HANTRO_NOK    error in motion vector prediction

------------------------------------------------------------------------------*/
u32 h264bsdInterPrediction(mbStorage_t *pMb, macroblockLayer_t *pMbLayer,
    dpbStorage_t *dpb, u32 mbNum, image_t *currImage,
    const sliceHeader_t *pSliceHeader)
{
    PROFILE_START(PS_H264_INTERPRED);

/* Variables */

    u32 i;
    u32 x, y;
    u32 colAndRow;
    subMbPartMode_e subPartMode;
    image_t refImage;
    #ifdef H264BSD_N64
    #if 0
    static u8 fillBuff[2][16384];
    static int fillBufIdx = 0;
    #endif
    #else 
    u8 fillBuff[32*21 + 15 + 32];
    #endif
    u8 *pFill = NULL;
    u32 tmp;
/* Code */

    ASSERT(pMb);
    ASSERT(h264bsdMbPartPredMode(pMb->mbType) == PRED_MODE_INTER);
    ASSERT(pMbLayer);

    /* 16-byte alignment */
    #ifdef H264BSD_N64
    #if 0
    // Alternate two buffers, in case RSP is still processing 
    // the previous macroblock.
    pFill = ALIGN(fillBuff[fillBufIdx], 16);
    fillBufIdx = 1-fillBufIdx;
    #endif
    #else
    pFill = ALIGN(fillBuff, 16);
    #endif

    /* set row bits 15:0 */
    colAndRow = mbNum / currImage->width;
    /*set col to bits 31:16 */
    colAndRow += (mbNum - colAndRow * currImage->width) << 16;
    colAndRow <<= 4;

    refImage.width = currImage->width;
    refImage.height = currImage->height;

    switch (pMb->mbType)
    {
        case P_Skip:
        case P_L0_16x16:
            if (MvPrediction16x16(pMb, &pMbLayer->mbPred, dpb) != HANTRO_OK) {
                PROFILE_STOP(PS_H264_INTERPRED);
                return(HANTRO_NOK);
            }
            refImage.data = pMb->refAddr[0];
            tmp = (0<<24) + (0<<16) + (16<<8) + 16;
            PredictPart(currImage, pMb->mv, &refImage,
                colAndRow, tmp, pFill, pMb->refPic[0], pSliceHeader);
            #ifdef H264BSD_N64
            pFill += 800;
            #endif
            break;

        case P_L0_L0_16x8:
            if ( MvPrediction16x8(pMb, &pMbLayer->mbPred, dpb) != HANTRO_OK){
                PROFILE_STOP(PS_H264_INTERPRED);
                return(HANTRO_NOK);
            }
            refImage.data = pMb->refAddr[0];
            tmp = (0<<24) + (0<<16) + (16<<8) + 8;
            PredictPart(currImage, pMb->mv, &refImage,
                colAndRow, tmp, pFill, pMb->refPic[0], pSliceHeader);
            #ifdef H264BSD_N64
            pFill += 800;
            #endif

            refImage.data = pMb->refAddr[2];
            tmp = (0<<24) + (8<<16) + (16<<8) + 8;
            PredictPart(currImage, pMb->mv+8, &refImage,
                colAndRow, tmp, pFill, pMb->refPic[2], pSliceHeader);
            #ifdef H264BSD_N64
            pFill += 800;
            #endif
            break;

        case P_L0_L0_8x16:
            if ( MvPrediction8x16(pMb, &pMbLayer->mbPred, dpb) != HANTRO_OK) {
                PROFILE_STOP(PS_H264_INTERPRED);
                return(HANTRO_NOK);              
            }
            refImage.data = pMb->refAddr[0];
            tmp = (0<<24) + (0<<16) + (8<<8) + 16;
            PredictPart(currImage, pMb->mv, &refImage,
                colAndRow, tmp, pFill, pMb->refPic[0], pSliceHeader);
            #ifdef H264BSD_N64
            pFill += 800;
            #endif
            refImage.data = pMb->refAddr[1];
            tmp = (8<<24) + (0<<16) + (8<<8) + 16;
            PredictPart(currImage, pMb->mv+4, &refImage,
                colAndRow, tmp, pFill, pMb->refPic[1], pSliceHeader);
            #ifdef H264BSD_N64
            pFill += 800;
            #endif
            break;

        default: /* P_8x8 and P_8x8ref0 */
            if ( MvPrediction8x8(pMb, &pMbLayer->subMbPred, dpb) != HANTRO_OK) {
                PROFILE_STOP(PS_H264_INTERPRED);
                return(HANTRO_NOK);
            }
            for (i = 0; i < 4; i++)
            {
                refImage.data = pMb->refAddr[i];
                subPartMode =
                    h264bsdSubMbPartMode(pMbLayer->subMbPred.subMbType[i]);
                x = i & 0x1 ? 8 : 0;
                y = i < 2 ? 0 : 8;
                switch (subPartMode)
                {
                    case MB_SP_8x8:
                        tmp = (x<<24) + (y<<16) + (8<<8) + 8;
                        PredictPart(currImage, pMb->mv+4*i, &refImage,
                            colAndRow, tmp, pFill, pMb->refPic[i],
                            pSliceHeader);
                        #ifdef H264BSD_N64
                        pFill += 800;
                        #endif
                        break;

                    case MB_SP_8x4:
                        tmp = (x<<24) + (y<<16) + (8<<8) + 4;
                        PredictPart(currImage, pMb->mv+4*i, &refImage,
                            colAndRow, tmp, pFill, pMb->refPic[i],
                            pSliceHeader);
                        #ifdef H264BSD_N64
                        pFill += 800;
                        #endif
                        tmp = (x<<24) + ((y+4)<<16) + (8<<8) + 4;
                        PredictPart(currImage, pMb->mv+4*i+2, &refImage,
                            colAndRow, tmp, pFill, pMb->refPic[i],
                            pSliceHeader);
                        #ifdef H264BSD_N64
                        pFill += 800;
                        #endif
                        break;

                    case MB_SP_4x8:
                        tmp = (x<<24) + (y<<16) + (4<<8) + 8;
                        PredictPart(currImage, pMb->mv+4*i, &refImage,
                            colAndRow, tmp, pFill, pMb->refPic[i],
                            pSliceHeader);
                        #ifdef H264BSD_N64
                        pFill += 800;
                        #endif
                        tmp = ((x+4)<<24) + (y<<16) + (4<<8) + 8;
                        PredictPart(currImage, pMb->mv+4*i+1, &refImage,
                            colAndRow, tmp, pFill, pMb->refPic[i],
                            pSliceHeader);
                        #ifdef H264BSD_N64
                        pFill += 800;
                        #endif
                        break;

                    default:
                        tmp = (x<<24) + (y<<16) + (4<<8) + 4;
                        PredictPart(currImage, pMb->mv+4*i, &refImage,
                            colAndRow, tmp, pFill, pMb->refPic[i],
                            pSliceHeader);
                        #ifdef H264BSD_N64
                        pFill += 800;
                        #endif
                        tmp = ((x+4)<<24) + (y<<16) + (4<<8) + 4;
                        PredictPart(currImage, pMb->mv+4*i+1, &refImage,
                            colAndRow, tmp, pFill, pMb->refPic[i],
                            pSliceHeader);
                        #ifdef H264BSD_N64
                        pFill += 800;
                        #endif
                        tmp = (x<<24) + ((y+4)<<16) + (4<<8) + 4;
                        PredictPart(currImage, pMb->mv+4*i+2, &refImage,
                            colAndRow, tmp, pFill, pMb->refPic[i],
                            pSliceHeader);
                        #ifdef H264BSD_N64
                        pFill += 800;
                        #endif
                        tmp = ((x+4)<<24) + ((y+4)<<16) + (4<<8) + 4;
                        PredictPart(currImage, pMb->mv+4*i+3, &refImage,
                            colAndRow, tmp, pFill, pMb->refPic[i],
                            pSliceHeader);
                        #ifdef H264BSD_N64
                        pFill += 800;
                        #endif
                        break;
                }
            }
            break;
    }
    PROFILE_STOP(PS_H264_INTERPRED);

    /* if decoded flag > 1 -> mb has already been successfully decoded and
     * written to output -> do not write again */
#ifndef OPTIMIZE_NO_DECODED_FLAG
    if (pMb->decoded > 1)
        return HANTRO_OK;
#endif

    return(HANTRO_OK);
}


/*------------------------------------------------------------------------------

    Function: MvPrediction16x16

        Functional description:
            Motion vector prediction for 16x16 partition mode

------------------------------------------------------------------------------*/

u32 MvPrediction16x16(mbStorage_t *pMb, mbPred_t *mbPred, dpbStorage_t *dpb)
{

/* Variables */

    mv_t mv;
    mv_t mvPred;
    interNeighbour_t a[3]; /* A, B, C */
    u32 refIndex;
    u8 *tmp;

/* Code */

    refIndex = mbPred->refIdxL0[0];

    GetInterNeighbour(pMb->sliceId, pMb->mbA, a, 5);
    GetInterNeighbour(pMb->sliceId, pMb->mbB, a+1, 10);
    if (pMb->mbType == P_Skip &&
        (!a[0].available || !a[1].available ||
         ( a[0].refIndex == 0 && a[0].mv.hor == 0 && a[0].mv.ver == 0 ) ||
         ( a[1].refIndex == 0 && a[1].mv.hor == 0 && a[1].mv.ver == 0 )))
    {
            mv.hor = mv.ver = 0;
    }
    else
    {
        mv = mbPred->mvdL0[0];
        GetInterNeighbour(pMb->sliceId, pMb->mbC, a+2, 10);
        if (!a[2].available)
        {
            GetInterNeighbour(pMb->sliceId, pMb->mbD, a+2, 15);
        }

        GetPredictionMv(&mvPred, a, refIndex);

        mv.hor += mvPred.hor;
        mv.ver += mvPred.ver;

        /* horizontal motion vector range [-2048, 2047.75] */
        if ((u32)(i32)(mv.hor+8192) >= (16384))
            return(HANTRO_NOK);

        /* vertical motion vector range [-512, 511.75]
         * (smaller for low levels) */
        if ((u32)(i32)(mv.ver+2048) >= (4096))
            return(HANTRO_NOK);
    }

    tmp = h264bsdGetRefPicData(dpb, refIndex);
    if (tmp == NULL)
        return(HANTRO_NOK);

    pMb->mv[0] = pMb->mv[1] = pMb->mv[2] = pMb->mv[3] =
    pMb->mv[4] = pMb->mv[5] = pMb->mv[6] = pMb->mv[7] =
    pMb->mv[8] = pMb->mv[9] = pMb->mv[10] = pMb->mv[11] =
    pMb->mv[12] = pMb->mv[13] = pMb->mv[14] = pMb->mv[15] = mv;

    pMb->refPic[0] = refIndex;
    pMb->refPic[1] = refIndex;
    pMb->refPic[2] = refIndex;
    pMb->refPic[3] = refIndex;
    pMb->refAddr[0] = tmp;
    pMb->refAddr[1] = tmp;
    pMb->refAddr[2] = tmp;
    pMb->refAddr[3] = tmp;

    return(HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: MvPrediction16x8

        Functional description:
            Motion vector prediction for 16x8 partition mode

------------------------------------------------------------------------------*/

u32 MvPrediction16x8(mbStorage_t *pMb, mbPred_t *mbPred, dpbStorage_t *dpb)
{

/* Variables */

    mv_t mv;
    mv_t mvPred;
    interNeighbour_t a[3]; /* A, B, C */
    u32 refIndex;
    u8 *tmp;

/* Code */

    mv = mbPred->mvdL0[0];
    refIndex = mbPred->refIdxL0[0];

    GetInterNeighbour(pMb->sliceId, pMb->mbB, a+1, 10);

    if (a[1].refIndex == refIndex)
        mvPred = a[1].mv;
    else
    {
        GetInterNeighbour(pMb->sliceId, pMb->mbA, a, 5);
        GetInterNeighbour(pMb->sliceId, pMb->mbC, a+2, 10);
        if (!a[2].available)
        {
            GetInterNeighbour(pMb->sliceId, pMb->mbD, a+2, 15);
        }

        GetPredictionMv(&mvPred, a, refIndex);

    }
    mv.hor += mvPred.hor;
    mv.ver += mvPred.ver;

    /* horizontal motion vector range [-2048, 2047.75] */
    if ((u32)(i32)(mv.hor+8192) >= (16384))
        return(HANTRO_NOK);

    /* vertical motion vector range [-512, 511.75] (smaller for low levels) */
    if ((u32)(i32)(mv.ver+2048) >= (4096))
        return(HANTRO_NOK);

    tmp = h264bsdGetRefPicData(dpb, refIndex);
    if (tmp == NULL)
        return(HANTRO_NOK);

    pMb->mv[0] = pMb->mv[1] = pMb->mv[2] = pMb->mv[3] =
    pMb->mv[4] = pMb->mv[5] = pMb->mv[6] = pMb->mv[7] = mv;
    pMb->refPic[0] = refIndex;
    pMb->refPic[1] = refIndex;
    pMb->refAddr[0] = tmp;
    pMb->refAddr[1] = tmp;

    mv = mbPred->mvdL0[1];
    refIndex = mbPred->refIdxL0[1];

    GetInterNeighbour(pMb->sliceId, pMb->mbA, a, 13);
    if (a[0].refIndex == refIndex)
        mvPred = a[0].mv;
    else
    {
        a[1].available = HANTRO_TRUE;
        a[1].refIndex = pMb->refPic[0];
        a[1].mv = pMb->mv[0];

        /* c is not available */
        GetInterNeighbour(pMb->sliceId, pMb->mbA, a+2, 7);

        GetPredictionMv(&mvPred, a, refIndex);

    }
    mv.hor += mvPred.hor;
    mv.ver += mvPred.ver;

    /* horizontal motion vector range [-2048, 2047.75] */
    if ((u32)(i32)(mv.hor+8192) >= (16384))
        return(HANTRO_NOK);

    /* vertical motion vector range [-512, 511.75] (smaller for low levels) */
    if ((u32)(i32)(mv.ver+2048) >= (4096))
        return(HANTRO_NOK);

    tmp = h264bsdGetRefPicData(dpb, refIndex);
    if (tmp == NULL)
        return(HANTRO_NOK);

    pMb->mv[8] = pMb->mv[9] = pMb->mv[10] = pMb->mv[11] =
    pMb->mv[12] = pMb->mv[13] = pMb->mv[14] = pMb->mv[15] = mv;
    pMb->refPic[2] = refIndex;
    pMb->refPic[3] = refIndex;
    pMb->refAddr[2] = tmp;
    pMb->refAddr[3] = tmp;

    return(HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: MvPrediction8x16

        Functional description:
            Motion vector prediction for 8x16 partition mode

------------------------------------------------------------------------------*/

u32 MvPrediction8x16(mbStorage_t *pMb, mbPred_t *mbPred, dpbStorage_t *dpb)
{

/* Variables */

    mv_t mv;
    mv_t mvPred;
    interNeighbour_t a[3]; /* A, B, C */
    u32 refIndex;
    u8 *tmp;

/* Code */

    mv = mbPred->mvdL0[0];
    refIndex = mbPred->refIdxL0[0];

    GetInterNeighbour(pMb->sliceId, pMb->mbA, a, 5);

    if (a[0].refIndex == refIndex)
        mvPred = a[0].mv;
    else
    {
        GetInterNeighbour(pMb->sliceId, pMb->mbB, a+1, 10);
        GetInterNeighbour(pMb->sliceId, pMb->mbB, a+2, 14);
        if (!a[2].available)
        {
            GetInterNeighbour(pMb->sliceId, pMb->mbD, a+2, 15);
        }

        GetPredictionMv(&mvPred, a, refIndex);

    }
    mv.hor += mvPred.hor;
    mv.ver += mvPred.ver;

    /* horizontal motion vector range [-2048, 2047.75] */
    if ((u32)(i32)(mv.hor+8192) >= (16384))
        return(HANTRO_NOK);

    /* vertical motion vector range [-512, 511.75] (smaller for low levels) */
    if ((u32)(i32)(mv.ver+2048) >= (4096))
        return(HANTRO_NOK);

    tmp = h264bsdGetRefPicData(dpb, refIndex);
    if (tmp == NULL)
        return(HANTRO_NOK);

    pMb->mv[0] = pMb->mv[1] = pMb->mv[2] = pMb->mv[3] =
    pMb->mv[8] = pMb->mv[9] = pMb->mv[10] = pMb->mv[11] = mv;
    pMb->refPic[0] = refIndex;
    pMb->refPic[2] = refIndex;
    pMb->refAddr[0] = tmp;
    pMb->refAddr[2] = tmp;

    mv = mbPred->mvdL0[1];
    refIndex = mbPred->refIdxL0[1];

    GetInterNeighbour(pMb->sliceId, pMb->mbC, a+2, 10);
    if (!a[2].available)
    {
        GetInterNeighbour(pMb->sliceId, pMb->mbB, a+2, 11);
    }
    if (a[2].refIndex == refIndex)
        mvPred = a[2].mv;
    else
    {
        a[0].available = HANTRO_TRUE;
        a[0].refIndex = pMb->refPic[0];
        a[0].mv = pMb->mv[0];

        GetInterNeighbour(pMb->sliceId, pMb->mbB, a+1, 14);

        GetPredictionMv(&mvPred, a, refIndex);

    }
    mv.hor += mvPred.hor;
    mv.ver += mvPred.ver;

    /* horizontal motion vector range [-2048, 2047.75] */
    if ((u32)(i32)(mv.hor+8192) >= (16384))
        return(HANTRO_NOK);

    /* vertical motion vector range [-512, 511.75] (smaller for low levels) */
    if ((u32)(i32)(mv.ver+2048) >= (4096))
        return(HANTRO_NOK);

    tmp = h264bsdGetRefPicData(dpb, refIndex);
    if (tmp == NULL)
        return(HANTRO_NOK);

    pMb->mv[4] = pMb->mv[5] = pMb->mv[6] = pMb->mv[7] =
    pMb->mv[12] = pMb->mv[13] = pMb->mv[14] = pMb->mv[15] = mv;
    pMb->refPic[1] = refIndex;
    pMb->refPic[3] = refIndex;
    pMb->refAddr[1] = tmp;
    pMb->refAddr[3] = tmp;

    return(HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: MvPrediction8x8

        Functional description:
            Motion vector prediction for 8x8 partition mode

------------------------------------------------------------------------------*/

u32 MvPrediction8x8(mbStorage_t *pMb, subMbPred_t *subMbPred, dpbStorage_t *dpb)
{

/* Variables */

    u32 i, j;
    u32 numSubMbPart;

/* Code */

    for (i = 0; i < 4; i++)
    {
        numSubMbPart = h264bsdNumSubMbPart(subMbPred->subMbType[i]);
        pMb->refPic[i] = subMbPred->refIdxL0[i];
        pMb->refAddr[i] = h264bsdGetRefPicData(dpb, subMbPred->refIdxL0[i]);
        if (pMb->refAddr[i] == NULL)
            return(HANTRO_NOK);
        for (j = 0; j < numSubMbPart; j++)
        {
            if (MvPrediction(pMb, subMbPred, i, j) != HANTRO_OK)
                return(HANTRO_NOK);
        }
    }

    return(HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: MvPrediction

        Functional description:
            Perform motion vector prediction for sub-partition

------------------------------------------------------------------------------*/

u32 MvPrediction(mbStorage_t *pMb, subMbPred_t *subMbPred, u32 mbPartIdx,
    u32 subMbPartIdx)
{

/* Variables */

    mv_t mv, mvPred;
    u32 refIndex;
    subMbPartMode_e subMbPartMode;
    const neighbour_t *n;
    mbStorage_t *nMb;
    interNeighbour_t a[3]; /* A, B, C */

/* Code */

    mv = subMbPred->mvdL0[mbPartIdx][subMbPartIdx];
    subMbPartMode = h264bsdSubMbPartMode(subMbPred->subMbType[mbPartIdx]);
    refIndex = subMbPred->refIdxL0[mbPartIdx];

    n = N_A_SUB_PART[mbPartIdx][subMbPartMode]+subMbPartIdx;
    nMb = h264bsdGetNeighbourMb(pMb, n->mb);
    GetInterNeighbour(pMb->sliceId, nMb, a, n->index);

    n = N_B_SUB_PART[mbPartIdx][subMbPartMode]+subMbPartIdx;
    nMb = h264bsdGetNeighbourMb(pMb, n->mb);
    GetInterNeighbour(pMb->sliceId, nMb, a+1, n->index);

    n = N_C_SUB_PART[mbPartIdx][subMbPartMode]+subMbPartIdx;
    nMb = h264bsdGetNeighbourMb(pMb, n->mb);
    GetInterNeighbour(pMb->sliceId, nMb, a+2, n->index);

    if (!a[2].available)
    {
        n = N_D_SUB_PART[mbPartIdx][subMbPartMode]+subMbPartIdx;
        nMb = h264bsdGetNeighbourMb(pMb, n->mb);
        GetInterNeighbour(pMb->sliceId, nMb, a+2, n->index);
    }

    GetPredictionMv(&mvPred, a, refIndex);

    mv.hor += mvPred.hor;
    mv.ver += mvPred.ver;

    /* horizontal motion vector range [-2048, 2047.75] */
    if (((u32)(i32)(mv.hor+8192) >= (16384)))
        return(HANTRO_NOK);

    /* vertical motion vector range [-512, 511.75] (smaller for low levels) */
    if (((u32)(i32)(mv.ver+2048) >= (4096)))
        return(HANTRO_NOK);

    switch (subMbPartMode)
    {
        case MB_SP_8x8:
            pMb->mv[4*mbPartIdx] = mv;
            pMb->mv[4*mbPartIdx + 1] = mv;
            pMb->mv[4*mbPartIdx + 2] = mv;
            pMb->mv[4*mbPartIdx + 3] = mv;
            break;

        case MB_SP_8x4:
            pMb->mv[4*mbPartIdx + 2*subMbPartIdx] = mv;
            pMb->mv[4*mbPartIdx + 2*subMbPartIdx + 1] = mv;
            break;

        case MB_SP_4x8:
            pMb->mv[4*mbPartIdx + subMbPartIdx] = mv;
            pMb->mv[4*mbPartIdx + subMbPartIdx + 2] = mv;
            break;

        case MB_SP_4x4:
            pMb->mv[4*mbPartIdx + subMbPartIdx] = mv;
            break;
    }

    return(HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: MedianFilter

        Functional description:
            Median filtering for motion vector prediction

------------------------------------------------------------------------------*/

i32 MedianFilter(i32 a, i32 b, i32 c)
{

/* Variables */

    i32 max,min,med;

/* Code */

    max = min = med = a;
    if (b > max)
    {
        max = b;
    }
    else if (b < min)
    {
        min = b;
    }
    if (c > max)
    {
        med = max;
    }
    else if (c < min)
    {
        med = min;
    }
    else
    {
        med = c;
    }

    return(med);
}

/*------------------------------------------------------------------------------

    Function: GetInterNeighbour

        Functional description:
            Get availability, reference index and motion vector of a neighbour

------------------------------------------------------------------------------*/

void GetInterNeighbour(u32 sliceId, mbStorage_t *nMb,
    interNeighbour_t *n, u32 index)
{

    n->available = HANTRO_FALSE;
    n->refIndex = 0xFFFFFFFF;
    n->mv.hor = n->mv.ver = 0;

    if (nMb && (sliceId == nMb->sliceId))
    {
        u32 tmp;
        mv_t tmpMv;

        tmp = nMb->mbType;
        n->available = HANTRO_TRUE;
        /* MbPartPredMode "inlined" */
        if (tmp <= P_8x8ref0)
        {
            tmpMv = nMb->mv[index];
            tmp = nMb->refPic[index>>2];
            n->refIndex = tmp;
            n->mv = tmpMv;
        }
    }

}

/*------------------------------------------------------------------------------

    Function: GetPredictionMv

        Functional description:
            Compute motion vector predictor based on neighbours A, B and C

------------------------------------------------------------------------------*/

void GetPredictionMv(mv_t *mv, interNeighbour_t *a, u32 refIndex)
{

    if ( a[1].available || a[2].available || !a[0].available)
    {
        u32 isA, isB, isC;
        isA = (a[0].refIndex == refIndex) ? HANTRO_TRUE : HANTRO_FALSE;
        isB = (a[1].refIndex == refIndex) ? HANTRO_TRUE : HANTRO_FALSE;
        isC = (a[2].refIndex == refIndex) ? HANTRO_TRUE : HANTRO_FALSE;

        if (((u32)isA+(u32)isB+(u32)isC) != 1)
        {
            mv->hor = (i16)MedianFilter(a[0].mv.hor, a[1].mv.hor, a[2].mv.hor);
            mv->ver = (i16)MedianFilter(a[0].mv.ver, a[1].mv.ver, a[2].mv.ver);
        }
        else if (isA)
            *mv = a[0].mv;
        else if (isB)
            *mv = a[1].mv;
        else
            *mv = a[2].mv;
    }
    else
    {
        *mv = a[0].mv;
    }

}


