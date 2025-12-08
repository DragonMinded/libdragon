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
          h264bsdDecodeMacroblockLayer
          h264bsdMbPartPredMode
          h264bsdNumMbPart
          h264bsdNumSubMbPart
          DecodeMbPred
          DecodeSubMbPred
          DecodeResidual
          DetermineNc
          CbpIntra16x16
          h264bsdPredModeIntra16x16
          h264bsdDecodeMacroblock
          ProcessResidual
          h264bsdSubMbPartMode

------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "h264bsd_macroblock_layer.h"
#include "h264bsd_slice_header.h"
#include "h264bsd_util.h"
#include "h264bsd_vlc.h"
#include "h264bsd_cavlc.h"
#include "h264bsd_nal_unit.h"
#include "h264bsd_neighbour.h"
#include "h264bsd_transform.h"
#include "h264bsd_intra_prediction.h"
#include "h264bsd_inter_prediction.h"

#include "omxdl/omxtypes.h"
#include "omxdl/omxVC.h"
#include "omxdl/armVC.h"

#ifdef H264BSD_N64
#include "../rsph264.h"
#include "../cache.h"
#endif
/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/
const u32 lumaIndex[16] = {   0,   4,  64,  68,
                                     8,  12,  72,  76,
                                   128, 132, 192, 196,
                                   136, 140, 200, 204 };

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

static u32 DecodeMbPred(strmData_t *pStrmData, mbPred_t *pMbPred,
    mbType_e mbType, u32 numRefIdxActive);
static u32 DecodeSubMbPred(strmData_t *pStrmData, subMbPred_t *pSubMbPred,
    mbType_e mbType, u32 numRefIdxActive);
static u32 DecodeResidual(strmData_t *pStrmData, residual_t *pResidual,
    mbStorage_t *pMb, mbType_e mbType, u32 codedBlockPattern);

u32 DetermineNc(mbStorage_t *pMb, u32 blockIndex, u8 *pTotalCoeff);

static u32 CbpIntra16x16(mbType_e mbType);
static u32 ProcessIntra4x4Residual(mbStorage_t *pMb, u32 constrainedIntraPred,
                    macroblockLayer_t *mbLayer, const u8 **pSrc, image_t *image);
static u32 ProcessChromaResidual(mbStorage_t *pMb, const u8 **pSrc, image_t *image);
static u32 ProcessIntra16x16Residual(mbStorage_t *pMb, u32 constrainedIntraPred,
                    u32 intraChromaPredMode, const u8 **pSrc, image_t *image);


/*------------------------------------------------------------------------------

    Function name: h264bsdDecodeMacroblockLayer

        Functional description:
          Parse macroblock specific information from bit stream.

        Inputs:
          pStrmData         pointer to stream data structure
          pMb               pointer to macroblock storage structure
          sliceType         type of the current slice
          numRefIdxActive   maximum reference index

        Outputs:
          pMbLayer          stores the macroblock data parsed from stream

        Returns:
          HANTRO_OK         success
          HANTRO_NOK        end of stream or error in stream

------------------------------------------------------------------------------*/

u32 h264bsdDecodeMacroblockLayer(strmData_t *pStrmData,
    macroblockLayer_t *pMbLayer, mbStorage_t *pMb, u32 sliceType,
    u32 numRefIdxActive)
{

/* Variables */

    u32 tmp, i, value;
    i32 itmp;
    mbPartPredMode_e partMode;

/* Code */

    ASSERT(pStrmData);
    ASSERT(pMbLayer);

    PROFILE_START(PS_H264_LAYER_CLEAR, 0);
#ifdef H264DEC_NEON
    h264bsdClearMbLayer(pMbLayer, ((sizeof(macroblockLayer_t) + 63) & ~0x3F));
#elif defined(H264BSD_N64)
    // On N64, we totally skip clearing pMBLayer. It's a waste of time and shows
    // up in the profile, probably because of data cache trashing. Instead,
    // we have adapted a little bit the code around here to not rely on this
    // initial clearing (eg: explicit clearing the specific fields in case
    // the codepath does not set them but later code relies on that).
#else
    H264SwDecMemset(pMbLayer, 0, sizeof(macroblockLayer_t));
#endif

    #if H264BSD_N64_CAVLC
    rsph264_queue_reset_total_coeff();
    #endif

    PROFILE_STOP(PS_H264_LAYER_CLEAR, 0);

    tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);

    if (IS_I_SLICE(sliceType))
    {
        if ((value + 6) > 31 || tmp != HANTRO_OK)
            return(HANTRO_NOK);
        pMbLayer->mbType = (mbType_e)(value + 6);
    }
    else
    {
        if ((value + 1) > 31 || tmp != HANTRO_OK)
            return(HANTRO_NOK);
        pMbLayer->mbType = (mbType_e)(value + 1);
    }

    if (pMbLayer->mbType == I_PCM)
    {
        while( !h264bsdIsByteAligned(pStrmData) )
        {
            /* pcm_alignment_zero_bit */
            tmp = h264bsdGetBits(pStrmData, 1);
            if (tmp)
                return(HANTRO_NOK);
        }

        u8 *level = pMbLayer->residual.posCoefBuf;

        for (i = 0; i < 384; i++)
        {
            value = h264bsdGetBits(pStrmData, 8);
            if (value == END_OF_STREAM)
                return(HANTRO_NOK);
            *level++ = value;
        }
    }
    else
    {
        PROFILE_START(PS_H264_LAYER_PRED, 0);
        partMode = h264bsdMbPartPredMode(pMbLayer->mbType);
        if ( (partMode == PRED_MODE_INTER) &&
             (h264bsdNumMbPart(pMbLayer->mbType) == 4) )
        {
            tmp = DecodeSubMbPred(pStrmData, &pMbLayer->subMbPred,
                pMbLayer->mbType, numRefIdxActive);
        }
        else
        {
            tmp = DecodeMbPred(pStrmData, &pMbLayer->mbPred,
                pMbLayer->mbType, numRefIdxActive);
        }
        if (tmp != HANTRO_OK) {
            PROFILE_STOP(PS_H264_LAYER_PRED, 0);
            return(tmp);
        }

        if (partMode != PRED_MODE_INTRA16x16)
        {
            tmp = h264bsdDecodeExpGolombMapped(pStrmData, &value,
                (u32)(partMode == PRED_MODE_INTRA4x4));
            if (tmp != HANTRO_OK) {
                PROFILE_STOP(PS_H264_LAYER_PRED, 0);
                return(tmp);
            }
            pMbLayer->codedBlockPattern = value;
        }
        else
        {
            pMbLayer->codedBlockPattern = CbpIntra16x16(pMbLayer->mbType);
        }
        PROFILE_STOP(PS_H264_LAYER_PRED, 0);

        if ( pMbLayer->codedBlockPattern ||
             (partMode == PRED_MODE_INTRA16x16) )
        {
            tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
            if (tmp != HANTRO_OK || (itmp < -26) || (itmp > 25) )
                return(HANTRO_NOK);
            pMbLayer->mbQpDelta = itmp;

            PROFILE_START(PS_H264_LAYER_RES, 0);
            tmp = DecodeResidual(pStrmData, &pMbLayer->residual, pMb,
                pMbLayer->mbType, pMbLayer->codedBlockPattern);
            PROFILE_STOP(PS_H264_LAYER_RES, 0);

            if (tmp != HANTRO_OK)
                return(tmp);
        } else {
            PROFILE_START(PS_H264_LAYER_CLEAR, 1);
            pMbLayer->mbQpDelta = 0;
            for (i = 0; i < 27; i++)
                pMbLayer->residual.totalCoeff[i] = 0;
            PROFILE_STOP(PS_H264_LAYER_CLEAR, 1);
        }
    }

    return(HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: h264bsdMbPartPredMode

        Functional description:
          Returns the prediction mode of a macroblock type

------------------------------------------------------------------------------*/

mbPartPredMode_e h264bsdMbPartPredMode(mbType_e mbType)
{

/* Variables */


/* Code */

    ASSERT(mbType <= 31);

    if ((mbType <= P_8x8ref0))
        return(PRED_MODE_INTER);
    else if (mbType == I_4x4)
        return(PRED_MODE_INTRA4x4);
    else
        return(PRED_MODE_INTRA16x16);

}

/*------------------------------------------------------------------------------

    Function: h264bsdNumMbPart

        Functional description:
          Returns the amount of macroblock partitions in a macroblock type

------------------------------------------------------------------------------*/

u32 h264bsdNumMbPart(mbType_e mbType)
{

/* Variables */


/* Code */

    ASSERT(h264bsdMbPartPredMode(mbType) == PRED_MODE_INTER);

    switch (mbType)
    {
        case P_L0_16x16:
        case P_Skip:
            return(1);

        case P_L0_L0_16x8:
        case P_L0_L0_8x16:
            return(2);

        /* P_8x8 or P_8x8ref0 */
        default:
            return(4);
    }

}

/*------------------------------------------------------------------------------

    Function: h264bsdNumSubMbPart

        Functional description:
          Returns the amount of sub-partitions in a sub-macroblock type

------------------------------------------------------------------------------*/

u32 h264bsdNumSubMbPart(subMbType_e subMbType)
{

/* Variables */


/* Code */

    ASSERT(subMbType <= P_L0_4x4);

    switch (subMbType)
    {
        case P_L0_8x8:
            return(1);

        case P_L0_8x4:
        case P_L0_4x8:
            return(2);

        /* P_L0_4x4 */
        default:
            return(4);
    }

}

/*------------------------------------------------------------------------------

    Function: DecodeMbPred

        Functional description:
          Parse macroblock prediction information from bit stream and store
          in 'pMbPred'.

------------------------------------------------------------------------------*/

u32 DecodeMbPred(strmData_t *pStrmData, mbPred_t *pMbPred, mbType_e mbType,
    u32 numRefIdxActive)
{

/* Variables */

    u32 tmp, i, j, value;
    i32 itmp;

/* Code */

    ASSERT(pStrmData);
    ASSERT(pMbPred);

    switch (h264bsdMbPartPredMode(mbType))
    {
        case PRED_MODE_INTER: /* PRED_MODE_INTER */
            if (numRefIdxActive > 1)
            {
                for (i = h264bsdNumMbPart(mbType), j = 0; i--;  j++)
                {
                    tmp = h264bsdDecodeExpGolombTruncated(pStrmData, &value,
                        (u32)(numRefIdxActive > 2));
                    if (tmp != HANTRO_OK || value >= numRefIdxActive)
                        return(HANTRO_NOK);

                    pMbPred->refIdxL0[j] = value;
                }
            } else {
                PROFILE_START(PS_H264_LAYER_CLEAR, 0);
                for (i=0; i<4; i++)
                    pMbPred->refIdxL0[i] = 0;
                PROFILE_STOP(PS_H264_LAYER_CLEAR, 0);
            }

            for (i = h264bsdNumMbPart(mbType), j = 0; i--;  j++)
            {
                tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
                if (tmp != HANTRO_OK)
                    return(tmp);
                pMbPred->mvdL0[j].hor = (i16)itmp;

                tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
                if (tmp != HANTRO_OK)
                    return(tmp);
                pMbPred->mvdL0[j].ver = (i16)itmp;
            }
            break;

        case PRED_MODE_INTRA4x4:
            for (itmp = 0, i = 0; itmp < 2; itmp++)
            {
                value = h264bsdShowBits32(pStrmData);
                tmp = 0;
                for (j = 8; j--; i++)
                {
                    pMbPred->prevIntra4x4PredModeFlag[i] =
                        value & 0x80000000 ? HANTRO_TRUE : HANTRO_FALSE;
                    value <<= 1;
                    if (!pMbPred->prevIntra4x4PredModeFlag[i])
                    {
                        pMbPred->remIntra4x4PredMode[i] = value>>29;
                        value <<= 3;
                        tmp++;
                    }
                }
                if (h264bsdFlushBits(pStrmData, 8 + 3*tmp) == END_OF_STREAM)
                    return(HANTRO_NOK);
            }
            /* fall-through */

        case PRED_MODE_INTRA16x16:
            tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
            if (tmp != HANTRO_OK || value > 3)
                return(HANTRO_NOK);
            pMbPred->intraChromaPredMode = value;
            break;
    }

    return(HANTRO_OK);

}

/*------------------------------------------------------------------------------

    Function: DecodeSubMbPred

        Functional description:
          Parse sub-macroblock prediction information from bit stream and
          store in 'pMbPred'.

------------------------------------------------------------------------------*/

u32 DecodeSubMbPred(strmData_t *pStrmData, subMbPred_t *pSubMbPred,
    mbType_e mbType, u32 numRefIdxActive)
{

/* Variables */

    u32 tmp, i, j, value;
    i32 itmp;

/* Code */

    ASSERT(pStrmData);
    ASSERT(pSubMbPred);
    ASSERT(h264bsdMbPartPredMode(mbType) == PRED_MODE_INTER);

    for (i = 0; i < 4; i++)
    {
        tmp = h264bsdDecodeExpGolombUnsigned(pStrmData, &value);
        if (tmp != HANTRO_OK || value > 3)
            return(HANTRO_NOK);
        pSubMbPred->subMbType[i] = (subMbType_e)value;
    }

    if ( (numRefIdxActive > 1) && (mbType != P_8x8ref0) )
    {
        for (i = 0; i < 4; i++)
        {
            tmp = h264bsdDecodeExpGolombTruncated(pStrmData, &value,
                (u32)(numRefIdxActive > 2));
            if (tmp != HANTRO_OK || value >= numRefIdxActive)
                return(HANTRO_NOK);
            pSubMbPred->refIdxL0[i] = value;
        }
    } else {
        PROFILE_START(PS_H264_LAYER_CLEAR, 0);
        for (i=0; i<4; i++)
            pSubMbPred->refIdxL0[i] = 0;
        PROFILE_STOP(PS_H264_LAYER_CLEAR, 0);
    }

    for (i = 0; i < 4; i++)
    {
        j = 0;
        for (value = h264bsdNumSubMbPart(pSubMbPred->subMbType[i]);
             value--; j++)
        {
            tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
            if (tmp != HANTRO_OK)
                return(tmp);
            pSubMbPred->mvdL0[i][j].hor = (i16)itmp;

            tmp = h264bsdDecodeExpGolombSigned(pStrmData, &itmp);
            if (tmp != HANTRO_OK)
                return(tmp);
            pSubMbPred->mvdL0[i][j].ver = (i16)itmp;
        }
    }

    return(HANTRO_OK);

}

#if H264BSD_N64_CAVLC

/*------------------------------------------------------------------------------

    Function: DecodeResidual

        Functional description:
          Parse residual information from bit stream and store in 'pResidual'.

------------------------------------------------------------------------------*/

u32 DecodeResidual(strmData_t *pStrmData, residual_t *pResidual,
    mbStorage_t *pMb, mbType_e mbType, u32 codedBlockPattern)
{
    OMXResult omxRes;
    OMX_U8 *pPosCoefBuf = pResidual->posCoefBuf;
    OMX_U8 *totalCoeffLeft = NULL, *totalCoeffUp = NULL;

    if (h264bsdIsNeighbourAvailable(pMb, pMb->mbA))
        totalCoeffLeft = pMb->mbA->totalCoeff;
    if (h264bsdIsNeighbourAvailable(pMb, pMb->mbB))
        totalCoeffUp = pMb->mbB->totalCoeff;

    OMX_U8 *strmCurr = STRM_CURR_PTR(pStrmData);
    OMX_S32 strmBitOff = STRM_CURR_BITOFF(pStrmData);
    omxRes = HIGHFUNC_DecodeResidual(
        (const OMX_U8 **) (&strmCurr),
        (OMX_S32*) (&strmBitOff),
        &pPosCoefBuf,
        pResidual->totalCoeff,
        totalCoeffLeft,
        totalCoeffUp,
        codedBlockPattern,
        h264bsdMbPartPredMode(mbType) == PRED_MODE_INTRA16x16
    );
    pStrmData->pCurr = ((u64)(u32)(strmCurr)<<3)+strmBitOff;

    if (omxRes != OMX_Sts_NoErr) {
        return (HANTRO_NOK);
    }
    return (HANTRO_OK);
}

#else
/*------------------------------------------------------------------------------

    Function: DecodeResidual

        Functional description:
          Parse residual information from bit stream and store in 'pResidual'.

------------------------------------------------------------------------------*/


/*
 *          Y             Cb       Cr
 *
 *     24 25 26 27      32 33     36 37
 *     -----------      -----     -- --
 *  28| 0  1  4  5   34|16 17  38|20 21
 *  29| 2  3  6  7   35|18 19  39|22 23
 *  30| 8  9 12 13
 *  31|10 11 14 15
 *
 */

// To determine NC, we need to average the totalCoeffs of
// the left and upper block. This table stores the direct indices
// for the left/upper block within the linear totalCoeffTable.
static const u8 ncLookupTable[24][2] = {
    { 28, 24 }, // 0
    {  0, 25 }, // 1
    { 29,  0 }, // 2
    {  2,  1 }, // 3
    {  1, 26 }, // 4
    {  4, 27 }, // 5
    {  3,  4 }, // 6
    {  6,  5 }, // 7
    { 30,  2 }, // 8
    {  8,  3 }, // 9
    { 31,  8 }, // 10
    { 10,  9 }, // 11
    {  9,  6 }, // 12
    { 12,  7 }, // 13
    { 11, 12 }, // 14
    { 14, 13 }, // 15
    { 34, 32 }, // 16
    { 16, 33 }, // 17
    { 35, 16 }, // 18
    { 18, 17 }, // 19
    { 38, 36 }, // 20
    { 20, 37 }, // 21
    { 39, 20 }, // 22
    { 22, 21 }, // 23
};

static u8 CalcNC(int block, const u8 *totalCoeffTable) {
    const u8 *k = ncLookupTable[block];

    u8 ncLeft = totalCoeffTable[k[0]];
    u8 ncUp = totalCoeffTable[k[1]];
    
    int hasLeft = 0;
    u8 n = 0;
    if ((i8)ncLeft >= 0) {
        n += ncLeft;
        hasLeft = 1;
    }
    if ((i8)ncUp >= 0) {
        n += ncUp;
        if (hasLeft) {
            n = (n+1)>>1;
        }
    }
    return n;
}

u32 DecodeResidual(strmData_t *pStrmData, residual_t *pResidual,
    mbStorage_t *pMb, mbType_e mbType, u32 codedBlockPattern)
{
    u8 totalCoeffTable[40];
    u8 *pPosCoefBuf = pResidual->posCoefBuf;
    u8 *totalCoeffLeft = NULL, *totalCoeffUp = NULL;
    u32 blockCoded;
    u8 lumaDC = 0, chromaDCR = 0, chromaDCB = 0;
    int is16x16 = (h264bsdMbPartPredMode(mbType) == PRED_MODE_INTRA16x16);
    int tmp;

    if (h264bsdIsNeighbourAvailable(pMb, pMb->mbA))
        totalCoeffLeft = pMb->mbA->totalCoeff;
    if (h264bsdIsNeighbourAvailable(pMb, pMb->mbB))
        totalCoeffUp = pMb->mbB->totalCoeff;

   for (int i=0;i<24;i++)
        totalCoeffTable[i] = 0;
    for (int i=24;i<40;i++)
        totalCoeffTable[i] = 0xFF;
    if (totalCoeffUp) {
        totalCoeffTable[24] = totalCoeffUp[10];
        totalCoeffTable[25] = totalCoeffUp[11];
        totalCoeffTable[26] = totalCoeffUp[14];
        totalCoeffTable[27] = totalCoeffUp[15];
        totalCoeffTable[32] = totalCoeffUp[18];
        totalCoeffTable[33] = totalCoeffUp[19];
        totalCoeffTable[36] = totalCoeffUp[22];
        totalCoeffTable[37] = totalCoeffUp[23];
    }
    if (totalCoeffLeft) {
        totalCoeffTable[28] = totalCoeffLeft[5];
        totalCoeffTable[29] = totalCoeffLeft[7];
        totalCoeffTable[30] = totalCoeffLeft[13];
        totalCoeffTable[31] = totalCoeffLeft[15];
        totalCoeffTable[34] = totalCoeffLeft[17];
        totalCoeffTable[35] = totalCoeffLeft[19];
        totalCoeffTable[38] = totalCoeffLeft[21];
        totalCoeffTable[39] = totalCoeffLeft[23];
    }

    // Decode residual info for luma DC (only present in 16x16 blocks)
    if (is16x16) {
        tmp = h264bsdDecodeResidualBlockCavlc(
                pStrmData, &pPosCoefBuf,
                CalcNC(0, totalCoeffTable), 16);
        if ((tmp & 0xF) != HANTRO_OK)
            return(tmp);
        lumaDC = (tmp >> 4) & 0xFF;
    }

    // Decode residual info for luma blocks. Notice that each block
    // contains up to 15 or 16 coefficients (depending if lumaDC is
    // present or not).
    int block = 0;
    for (int i=0;i<4;i++) {
        blockCoded = codedBlockPattern & 0x1;
        codedBlockPattern >>= 1;
        if (blockCoded) {
            for (int j=0;j<4;j++) {
                tmp = h264bsdDecodeResidualBlockCavlc(
                        pStrmData, &pPosCoefBuf,
                        CalcNC(block, totalCoeffTable), is16x16 ? 15 : 16);
                if ((tmp & 0xF) != HANTRO_OK)
                    return(tmp);
                totalCoeffTable[block] = (tmp >> 4) & 0xFF;
                block++;
            }
        } else {
            block += 4;
        }
    }

    // Decode residual info for chroma DC
    blockCoded = codedBlockPattern & 0x3;
    if (blockCoded)
    {
        tmp = h264bsdDecodeResidualBlockCavlc(
                pStrmData, &pPosCoefBuf,
                -1, 4);
        if ((tmp & 0xF) != HANTRO_OK)
            return(tmp);
        chromaDCR = (tmp >> 4) & 0xFF;
        tmp = h264bsdDecodeResidualBlockCavlc(
                pStrmData,
                &pPosCoefBuf,
                -1, 4);
        if ((tmp & 0xF) != HANTRO_OK)
            return(tmp);
        chromaDCB = (tmp >> 4) & 0xFF;
    }

    // Decode residual info for chroma
    blockCoded = codedBlockPattern & 0x2;
    if (blockCoded)
    {
        for (int i=0;i<8;i++) {
            tmp = h264bsdDecodeResidualBlockCavlc(
                    pStrmData, &pPosCoefBuf,
                    CalcNC(block, totalCoeffTable), 15);
            if ((tmp & 0xF) != HANTRO_OK)
                return(tmp);
            totalCoeffTable[block] = (tmp >> 4) & 0xFF;
            block++;
        }
    }

    for (int i=0;i<24;i++)
        pResidual->totalCoeff[i] = totalCoeffTable[i];
    pResidual->totalCoeff[24] = lumaDC;
    pResidual->totalCoeff[25] = chromaDCR;
    pResidual->totalCoeff[26] = chromaDCB;
    return (HANTRO_OK);
}

#endif

/*------------------------------------------------------------------------------

    Function: DetermineNc

        Functional description:
          Returns the nC of a block.

------------------------------------------------------------------------------*/
u32 DetermineNc(mbStorage_t *pMb, u32 blockIndex, u8 *pTotalCoeff)
{
/*lint -e702 */
/* Variables */

    u32 tmp;
    i32 n;
    const neighbour_t *neighbourA, *neighbourB;
    u8 neighbourAindex, neighbourBindex;

/* Code */

    ASSERT(blockIndex < 24);

    /* if neighbour block belongs to current macroblock totalCoeff array
     * mbStorage has not been set/updated yet -> use pTotalCoeff */
    neighbourA = h264bsdNeighbour4x4BlockA(blockIndex);
    neighbourB = h264bsdNeighbour4x4BlockB(blockIndex);
    neighbourAindex = neighbourA->index;
    neighbourBindex = neighbourB->index;
    if (neighbourA->mb == MB_CURR && neighbourB->mb == MB_CURR)
    {
        n = (pTotalCoeff[neighbourAindex] +
             pTotalCoeff[neighbourBindex] + 1)>>1;
    }
    else if (neighbourA->mb == MB_CURR)
    {
        n = pTotalCoeff[neighbourAindex];
        if (h264bsdIsNeighbourAvailable(pMb, pMb->mbB))
        {
            n = (n + pMb->mbB->totalCoeff[neighbourBindex] + 1) >> 1;
        }
    }
    else if (neighbourB->mb == MB_CURR)
    {
        n = pTotalCoeff[neighbourBindex];
        if (h264bsdIsNeighbourAvailable(pMb, pMb->mbA))
        {
            n = (n + pMb->mbA->totalCoeff[neighbourAindex] + 1) >> 1;
        }
    }
    else
    {
        n = tmp = 0;
        if (h264bsdIsNeighbourAvailable(pMb, pMb->mbA))
        {
            n = pMb->mbA->totalCoeff[neighbourAindex];
            tmp = 1;
        }
        if (h264bsdIsNeighbourAvailable(pMb, pMb->mbB))
        {
            if (tmp)
                n = (n + pMb->mbB->totalCoeff[neighbourBindex] + 1) >> 1;
            else
                n = pMb->mbB->totalCoeff[neighbourBindex];
        }
    }
    return((u32)n);
/*lint +e702 */
}

/*------------------------------------------------------------------------------

    Function: CbpIntra16x16

        Functional description:
          Returns the coded block pattern for intra 16x16 macroblock.

------------------------------------------------------------------------------*/

u32 CbpIntra16x16(mbType_e mbType)
{

/* Variables */

    u32 cbp;
    u32 tmp;

/* Code */

    ASSERT(mbType >= I_16x16_0_0_0 && mbType <= I_16x16_3_2_1);

    if (mbType >= I_16x16_0_0_1)
        cbp = 15;
    else
        cbp = 0;

    /* tmp is 0 for I_16x16_0_0_0 mb type */
    /* ignore lint warning on arithmetic on enum's */
    tmp = /*lint -e(656)*/(mbType - I_16x16_0_0_0) >> 2;
    if (tmp > 2)
        tmp -= 3;

    cbp += tmp << 4;

    return(cbp);

}

/*------------------------------------------------------------------------------

    Function: h264bsdPredModeIntra16x16

        Functional description:
          Returns the prediction mode for intra 16x16 macroblock.

------------------------------------------------------------------------------*/

u32 h264bsdPredModeIntra16x16(mbType_e mbType)
{

/* Variables */

    u32 tmp;

/* Code */

    ASSERT(mbType >= I_16x16_0_0_0 && mbType <= I_16x16_3_2_1);

    /* tmp is 0 for I_16x16_0_0_0 mb type */
    /* ignore lint warning on arithmetic on enum's */
    tmp = /*lint -e(656)*/(mbType - I_16x16_0_0_0);

    return(tmp & 0x3);

}

/*------------------------------------------------------------------------------

    Function: h264bsdDecodeMacroblock

        Functional description:
          Decode one macroblock and write into output image.

        Inputs:
          pMb           pointer to macroblock specific information
          mbLayer       pointer to current macroblock data from stream
          currImage     pointer to output image
          dpb           pointer to decoded picture buffer
          qpY           pointer to slice QP
          mbNum         current macroblock number
          constrainedIntraPred  flag specifying if neighbouring inter
                                macroblocks are used in intra prediction

        Outputs:
          pMb           structure is updated with current macroblock
          currImage     decoded macroblock is written into output image

        Returns:
          HANTRO_OK     success
          HANTRO_NOK    error in macroblock decoding

------------------------------------------------------------------------------*/

u32 h264bsdDecodeMacroblock(mbStorage_t *pMb, macroblockLayer_t *pMbLayer,
    image_t *currImage, dpbStorage_t *dpb, i32 *qpY, u32 mbNum,
    u32 constrainedIntraPredFlag)
{

/* Variables */

    u32 tmp;
    mbType_e mbType;
    const u8 *pSrc;
/* Code */

    ASSERT(pMb);
    ASSERT(pMbLayer);
    ASSERT(currImage);
    ASSERT(qpY && *qpY < 52);
    ASSERT(mbNum < currImage->width*currImage->height);

    mbType = pMbLayer->mbType;
    pMb->mbType = mbType;

#ifndef OPTIMIZE_NO_DECODED_FLAG
    pMb->decoded++;
#endif

    h264bsdSetCurrImageMbPointers(currImage, mbNum);

    if (mbType == I_PCM)
    {
        ASSERT(0);  // Check if PCM is really used
#if 0
        // TODO: this is rarely(never?) used so for now it's not implemented.
        // The below code should be reworked to write directly into 
        // currImage buffers.
        u8 *pData = (u8*)data;
        u8 *tot = pMb->totalCoeff;
        u8 *lev = pMbLayer->residual.posCoefBuf;

        pMb->qpY = 0;

#ifndef OPTIMIZE_NO_DECODED_FLAG
        /* if decoded flag > 1 -> mb has already been successfully decoded and
         * written to output -> do not write again */
        if (pMb->decoded > 1)
        {
            for (i = 24; i--;)
                *tot++ = 16;
            return HANTRO_OK;
        }
#endif
      
        for (i = 24; i--;)
        {
            *tot++ = 16;
            for (tmp = 16; tmp--;)
                *pData++ = (u8)(*lev++);
        }
        h264bsdWriteMacroblock(currImage, (u8*)data, 1);
#endif
        return(HANTRO_OK);
    }
    else
    {
        if (h264bsdMbPartPredMode(mbType) == PRED_MODE_INTER)
        {
            tmp = h264bsdInterPrediction(pMb, pMbLayer, dpb, mbNum, currImage);
            ASSERT(tmp == HANTRO_OK);
            if (tmp != HANTRO_OK) return (tmp);
        }

        if (mbType != P_Skip)
        {
            #if !H264BSD_N64_CAVLC
            H264SwDecMemcpy(pMb->totalCoeff,
                            pMbLayer->residual.totalCoeff,
                            27*sizeof(*pMb->totalCoeff));
            #ifdef H264BSD_N64
            fast_data_cache_hit_writeback(pMb->totalCoeff, 27);
            #endif
            #endif

            /* update qpY */
            if (pMbLayer->mbQpDelta)
            {
                *qpY = *qpY + pMbLayer->mbQpDelta;
                if (*qpY < 0) *qpY += 52;
                else if (*qpY >= 52) *qpY -= 52;
            }
            pMb->qpY = (u32)*qpY;

            pSrc = pMbLayer->residual.posCoefBuf;

            if (h264bsdMbPartPredMode(mbType) == PRED_MODE_INTER)
            {
                PROFILE_START(PS_H264_RESIDUAL_LUMA, 0);
                tmp = HIGHFUNC_ProcessLumaInterResidual(
                    &pSrc, currImage->luma, currImage->luma,
                    currImage->width*16, currImage->width*16, *qpY, pMb->totalCoeff);
                if (tmp != HANTRO_OK) {
                    #ifdef H264BSD_N64
                    PROFILE_STOP(PS_H264_RESIDUAL_LUMA, 0);
                    #endif
                    return (tmp);
                }
                PROFILE_STOP(PS_H264_RESIDUAL_LUMA, 0);
            }
            else if (h264bsdMbPartPredMode(mbType) == PRED_MODE_INTRA4x4)
            {
                tmp = ProcessIntra4x4Residual(pMb,
                                              constrainedIntraPredFlag,
                                              pMbLayer,
                                              &pSrc,
                                              currImage);
                if (tmp != HANTRO_OK) {
                    return (tmp);
                }
            }
            else if (h264bsdMbPartPredMode(mbType) == PRED_MODE_INTRA16x16)
            {
                tmp = ProcessIntra16x16Residual(pMb,
                                        constrainedIntraPredFlag,
                                        pMbLayer->mbPred.intraChromaPredMode,
                                        &pSrc,
                                        currImage);
                if (tmp != HANTRO_OK) {
                    return (tmp);
                }
            }

            PROFILE_START(PS_H264_RESIDUAL_CHROMA, 0);
            tmp = ProcessChromaResidual(pMb, &pSrc, currImage);
            PROFILE_STOP(PS_H264_RESIDUAL_CHROMA, 0);

            if (tmp != HANTRO_OK)
                return (tmp);
        }
        else
        {
            H264SwDecMemset(pMb->totalCoeff, 0, 27*sizeof(*pMb->totalCoeff));
            #ifdef H264BSD_N64
            fast_data_cache_hit_writeback(pMb->totalCoeff, 27);
            #endif
            pMb->qpY = (u32)*qpY;
        }

        /* if decoded flag > 1 -> mb has already been successfully decoded and
         * written to output -> do not write again */
        // TODO(rasky): this used to guard a call to WriteMacroblock, but
        // we now directly decode to the output buffer so there's no more
        // decoded to skip. I'm not sure what this flag means, but it's useless now.
        if (pMb->decoded > 1)
            return HANTRO_OK;

        #ifdef H264BSD_N64
        rsph264_queue_set_packed_delta_buffer_if_changed(0, NULL);
        #endif
    }

    return HANTRO_OK;
}


/*------------------------------------------------------------------------------

    Function: ProcessChromaResidual

        Functional description:
          Process the residual data of chroma with
          inverse quantization and inverse transform.

------------------------------------------------------------------------------*/
u32 ProcessChromaResidual(mbStorage_t *pMb, const u8 **pSrc, image_t *image)
{
    u32 chromaQp;
    // OMXResult result;

    /* chroma DC processing. First chroma dc block is block with index 25 */
    chromaQp =
        h264bsdQpC[CLIP3(0, 51, (i32)pMb->qpY + pMb->chromaQpIndexOffset)];

    OMXResult result;
    result = HIGHFUNC_ProcessChromaResidual(pSrc,
        image->cb, image->cr, image->width*8, image->width*8,
        chromaQp, pMb->totalCoeff);
    if (result != OMX_Sts_NoErr)
        return (HANTRO_NOK);
    return(HANTRO_OK);
}

/*------------------------------------------------------------------------------

    Function: ProcessIntra16x16Residual

        Functional description:
          Process the residual data of luma with
          inverse quantization and inverse transform.

------------------------------------------------------------------------------*/
u32 ProcessIntra16x16Residual(mbStorage_t *pMb,
                              u32 constrainedIntraPred,
                              u32 intraChromaPredMode,
                              const u8** pCoeff,
                              image_t *image)
{
    u32 availableA, availableB, availableD;
    OMXResult result;

    ASSERT(pMb);
    ASSERT(pCoeff);
    ASSERT(image);
    ASSERT(h264bsdPredModeIntra16x16(pMb->mbType) < 4);

    availableA = h264bsdIsNeighbourAvailable(pMb, pMb->mbA);
    if (availableA && constrainedIntraPred &&
       (h264bsdMbPartPredMode(pMb->mbA->mbType) == PRED_MODE_INTER))
        availableA = HANTRO_FALSE;
    availableB = h264bsdIsNeighbourAvailable(pMb, pMb->mbB);
    if (availableB && constrainedIntraPred &&
       (h264bsdMbPartPredMode(pMb->mbB->mbType) == PRED_MODE_INTER))
        availableB = HANTRO_FALSE;
    availableD = h264bsdIsNeighbourAvailable(pMb, pMb->mbD);
    if (availableD && constrainedIntraPred &&
       (h264bsdMbPartPredMode(pMb->mbD->mbType) == PRED_MODE_INTER))
        availableD = HANTRO_FALSE;

    PROFILE_START(PS_H264_INTRAPRED_16X16, 0);
    result = HIGHFUNC_ProcessLumaIntra16x16Residual(
        image->luma, image->luma, image->width*16, image->width*16,
        pCoeff, pMb->totalCoeff,
        (OMXVCM4P10Intra16x16PredMode)h264bsdPredModeIntra16x16(pMb->mbType),
        (i32)(availableB + (availableA<<1) + (availableD<<5)),
        pMb->qpY);
    if (result != OMX_Sts_NoErr)
        return (HANTRO_NOK);

    if (h264bsdIntraChromaPrediction(pMb,
                image,
                intraChromaPredMode,
                constrainedIntraPred) != HANTRO_OK)
        return(HANTRO_NOK);
    PROFILE_STOP(PS_H264_INTRAPRED_16X16, 0);

    return HANTRO_OK;
}

/*------------------------------------------------------------------------------

    Function: ProcessIntra4x4Residual

        Functional description:
          Process the residual data of luma with
          inverse quantization and inverse transform.

------------------------------------------------------------------------------*/
u32 ProcessIntra4x4Residual(mbStorage_t *pMb,
                            u32 constrainedIntraPred,
                            macroblockLayer_t *mbLayer,
                            const u8 **pSrc,
                            image_t *image)
{
    PROFILE_START(PS_H264_INTRAPRED_4X4, 0);
    if (h264bsdIntra4x4PredictionAndTransformAll(pMb, mbLayer,
                constrainedIntraPred, image, pSrc) != HANTRO_OK)
        return (HANTRO_NOK);

    if (h264bsdIntraChromaPrediction(pMb,
                image,
                mbLayer->mbPred.intraChromaPredMode,
                constrainedIntraPred) != HANTRO_OK)
        return(HANTRO_NOK);
    PROFILE_STOP(PS_H264_INTRAPRED_4X4, 0);

    return HANTRO_OK;
}


/*------------------------------------------------------------------------------

    Function: h264bsdSubMbPartMode

        Functional description:
          Returns the macroblock's sub-partition mode.

------------------------------------------------------------------------------*/

subMbPartMode_e h264bsdSubMbPartMode(subMbType_e subMbType)
{

/* Variables */


/* Code */

    ASSERT(subMbType < 4);

    return((subMbPartMode_e)subMbType);

}


