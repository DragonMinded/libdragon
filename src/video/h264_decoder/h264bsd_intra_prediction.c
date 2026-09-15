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
          h264bsdIntraPrediction
          h264bsdGetNeighbourPels
          h264bsdIntra16x16Prediction
          h264bsdIntra4x4Prediction
          h264bsdIntraChromaPrediction
          h264bsdAddResidual
          Intra16x16VerticalPrediction
          Intra16x16HorizontalPrediction
          Intra16x16DcPrediction
          Intra16x16PlanePrediction
          IntraChromaDcPrediction
          IntraChromaHorizontalPrediction
          IntraChromaVerticalPrediction
          IntraChromaPlanePrediction
          Get4x4NeighbourPels
          Write4x4To16x16
          Intra4x4VerticalPrediction
          Intra4x4HorizontalPrediction
          Intra4x4DcPrediction
          Intra4x4DiagonalDownLeftPrediction
          Intra4x4DiagonalDownRightPrediction
          Intra4x4VerticalRightPrediction
          Intra4x4HorizontalDownPrediction
          Intra4x4VerticalLeftPrediction
          Intra4x4HorizontalUpPrediction
          DetermineIntra4x4PredMode

------------------------------------------------------------------------------*/

#include "opttarget.h"

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "h264bsd_intra_prediction.h"
#include "h264bsd_util.h"
#include "h264bsd_macroblock_layer.h"
#include "h264bsd_neighbour.h"
#include "h264bsd_image.h"

#include "omxdl/omxtypes.h"
#include "omxdl/omxVC.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/* Switch off the following Lint messages for this file:
 * Info 702: Shift right of signed quantity (int)
 */
/*lint -e702 */


/* x- and y-coordinates for each block */
const u32 h264bsdBlockX[16] =
    { 0, 4, 0, 4, 8, 12, 8, 12, 0, 4, 0, 4, 8, 12, 8, 12 };
const u32 h264bsdBlockY[16] =
    { 0, 0, 4, 4, 0, 0, 4, 4, 8, 8, 12, 12, 8, 8, 12, 12 };

const u8 h264bsdClip[1280] =
{
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
    48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
    64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
    80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
    96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
    112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
    128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
    160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
    176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
    192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
    208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
    224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
    240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};

static u32 DetermineIntra4x4PredMode(macroblockLayer_t *pMbLayer,
    u32 available, neighbour_t *nA, neighbour_t *nB, u32 index,
    mbStorage_t *nMbA, mbStorage_t *nMbB);


/*------------------------------------------------------------------------------

    Function: h264bsdIntra16x16Prediction

        Functional description:
          Perform intra 16x16 prediction mode for luma pixels and add
          residual into prediction. The resulting luma pixels are
          stored in macroblock array 'data'.

------------------------------------------------------------------------------*/
u32 h264bsdIntra16x16Prediction(mbStorage_t *pMb, u8 *data, u8 *ptr,
                                u32 width, u32 constrainedIntraPred)
{

/* Variables */

    u32 availableA, availableB, availableD;
    OMXResult omxRes;

/* Code */
    ASSERT(pMb);
    ASSERT(data);
    ASSERT(ptr);
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

    omxRes = omxVCM4P10_PredictIntra_16x16( (ptr-1),
                                    (ptr - width),
                                    (ptr - width-1),
                                    data,
                                    (i32)width,
                                    16,
                                    (OMXVCM4P10Intra16x16PredMode)
                                    h264bsdPredModeIntra16x16(pMb->mbType),
                                    (i32)(availableB + (availableA<<1) +
                                     (availableD<<5)) );
    if (omxRes != OMX_Sts_NoErr)
        return HANTRO_NOK;
    else
        return(HANTRO_OK);
}

/*------------------------------------------------------------------------------

    Function: h264bsdIntra4x4Prediction

        Functional description:
          Perform intra 4x4 prediction for luma pixels and add residual
          into prediction. The resulting luma pixels are stored in
          macroblock array 'data'. The intra 4x4 prediction mode for each
          block is stored in 'pMb' structure.

------------------------------------------------------------------------------*/
u32 h264bsdIntra4x4Prediction(mbStorage_t *pMb, u8 *data,
                              macroblockLayer_t *mbLayer,
                              u8 *ptr, u32 width,
                              u32 constrainedIntraPred, u32 block)
{

/* Variables */
    u32 mode;
    neighbour_t neighbour, neighbourB;
    mbStorage_t *nMb, *nMb2;
    u32 availableA, availableB, availableC, availableD;

    OMXResult omxRes;
    u32 x, y;
    u8 *l, *a, *al;
/* Code */
    ASSERT(pMb);
    ASSERT(data);
    ASSERT(mbLayer);
    ASSERT(ptr);
    ASSERT(pMb->intra4x4PredMode[block] < 9);

    neighbour = *h264bsdNeighbour4x4BlockA(block);
    nMb = h264bsdGetNeighbourMb(pMb, neighbour.mb);
    availableA = h264bsdIsNeighbourAvailable(pMb, nMb);
    if (availableA && constrainedIntraPred &&
       ( h264bsdMbPartPredMode(nMb->mbType) == PRED_MODE_INTER) )
    {
        availableA = HANTRO_FALSE;
    }

    neighbourB = *h264bsdNeighbour4x4BlockB(block);
    nMb2 = h264bsdGetNeighbourMb(pMb, neighbourB.mb);
    availableB = h264bsdIsNeighbourAvailable(pMb, nMb2);
    if (availableB && constrainedIntraPred &&
       ( h264bsdMbPartPredMode(nMb2->mbType) == PRED_MODE_INTER) )
    {
        availableB = HANTRO_FALSE;
    }

    mode = DetermineIntra4x4PredMode(mbLayer,
        (u32)(availableA && availableB),
        &neighbour, &neighbourB, block, nMb, nMb2);
    pMb->intra4x4PredMode[block] = (u8)mode;

    neighbour = *h264bsdNeighbour4x4BlockC(block);
    nMb = h264bsdGetNeighbourMb(pMb, neighbour.mb);
    availableC = h264bsdIsNeighbourAvailable(pMb, nMb);
    if (availableC && constrainedIntraPred &&
       ( h264bsdMbPartPredMode(nMb->mbType) == PRED_MODE_INTER) )
    {
        availableC = HANTRO_FALSE;
    }

    neighbour = *h264bsdNeighbour4x4BlockD(block);
    nMb = h264bsdGetNeighbourMb(pMb, neighbour.mb);
    availableD = h264bsdIsNeighbourAvailable(pMb, nMb);
    if (availableD && constrainedIntraPred &&
       ( h264bsdMbPartPredMode(nMb->mbType) == PRED_MODE_INTER) )
    {
        availableD = HANTRO_FALSE;
    }

    x = h264bsdBlockX[block];
    y = h264bsdBlockY[block];

    if (y == 0)
        a = ptr - width + x;
    else
        a = data-16;

    if (x == 0)
        l = ptr + y * width -1;
    else
    {
        l = data-1;
        width = 16;
    }

    if (x == 0)
        al = l-width;
    else
        al = a-1;

    omxRes = omxVCM4P10_PredictIntra_4x4( l,
                                          a,
                                          al,
                                          data,
                                          (i32)width,
                                          16,
                                          (OMXVCM4P10Intra4x4PredMode)mode,
                                          (i32)(availableB +
                                          (availableA<<1) +
                                          (availableD<<5) +
                                          (availableC<<6)) );
    if (omxRes != OMX_Sts_NoErr)
        return HANTRO_NOK;

    return(HANTRO_OK);

}


u32 h264bsdIntra4x4PredictionAndTransformAll(mbStorage_t *pMb,
                                             macroblockLayer_t *mbLayer,
                                             u32 constrainedIntraPred,
                                             image_t *image,
                                             const u8 **pCoeff
) 
{
    u8 modeAvail[16];
    int i;

    ASSERT(pMb);
    ASSERT(mbLayer);

    for (i = 0; i < 16; i++)
    {
        u32 mode;
        neighbour_t neighbour, neighbourB;
        mbStorage_t *nMb, *nMb2;
        u32 availableA, availableB, availableC, availableD;
        int block = i;

        ASSERT(pMb->intra4x4PredMode[block] < 9);

        /* Inline h264bsdIntra4x4Prediction */

        neighbour = *h264bsdNeighbour4x4BlockA(block);
        nMb = h264bsdGetNeighbourMb(pMb, neighbour.mb);
        availableA = h264bsdIsNeighbourAvailable(pMb, nMb);
        if (availableA && constrainedIntraPred &&
           ( h264bsdMbPartPredMode(nMb->mbType) == PRED_MODE_INTER) )
        {
            availableA = HANTRO_FALSE;
        }

        neighbourB = *h264bsdNeighbour4x4BlockB(block);
        nMb2 = h264bsdGetNeighbourMb(pMb, neighbourB.mb);
        availableB = h264bsdIsNeighbourAvailable(pMb, nMb2);
        if (availableB && constrainedIntraPred &&
           ( h264bsdMbPartPredMode(nMb2->mbType) == PRED_MODE_INTER) )
        {
            availableB = HANTRO_FALSE;
        }

        mode = DetermineIntra4x4PredMode(mbLayer,
            (u32)(availableA && availableB),
            &neighbour, &neighbourB, block, nMb, nMb2);
        pMb->intra4x4PredMode[block] = (u8)mode;

        neighbour = *h264bsdNeighbour4x4BlockC(block);
        nMb = h264bsdGetNeighbourMb(pMb, neighbour.mb);
        availableC = h264bsdIsNeighbourAvailable(pMb, nMb);
        if (availableC && constrainedIntraPred &&
           ( h264bsdMbPartPredMode(nMb->mbType) == PRED_MODE_INTER) )
        {
            availableC = HANTRO_FALSE;
        }

        neighbour = *h264bsdNeighbour4x4BlockD(block);
        nMb = h264bsdGetNeighbourMb(pMb, neighbour.mb);
        availableD = h264bsdIsNeighbourAvailable(pMb, nMb);
        if (availableD && constrainedIntraPred &&
           ( h264bsdMbPartPredMode(nMb->mbType) == PRED_MODE_INTER) )
        {
            availableD = HANTRO_FALSE;
        }


        // For each 4x4 partition, we store:
        //   * 4 bits of availability (upper, left, upper-left and upper-right).
        //     These are the only availability bits used by Intra4x4.
        //   * 4 bits of intra mode (value 0-9)
        modeAvail[block] = 
          (availableB << 0) |   // upper
          (availableA << 1) |   // left
          (availableD << 2) |   // upper-left
          (availableC << 3) |   // upper-right
          (mode << 4);
    }


    OMXResult omxRes = HIGHFUNC_PredictIntraTransform_4x4(
                image->luma,
                image->luma,
                image->width*16,
                image->width*16,
                pCoeff,
                pMb->totalCoeff,
                modeAvail,
                pMb->qpY);
    if (omxRes != OMX_Sts_NoErr)
        return HANTRO_NOK;

    return(HANTRO_OK);
}



/*------------------------------------------------------------------------------

    Function: h264bsdIntraChromaPrediction

        Functional description:
          Perform intra prediction for chroma pixels and add residual
          into prediction. The resulting chroma pixels are stored in 'data'.

------------------------------------------------------------------------------*/
u32 h264bsdIntraChromaPrediction(mbStorage_t *pMb, image_t *image,
                                        u32 predMode, u32 constrainedIntraPred)
{

/* Variables */

    u32 availableA, availableB, availableD;
    OMXResult omxRes;
    u8 *ptr;
    u32 width;

/* Code */
    ASSERT(pMb);
    ASSERT(image);
    ASSERT(predMode < 4);

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

    ptr = image->cb;
    width = image->width*8;

    omxRes = omxVCM4P10_PredictIntraChroma_8x8( (ptr-1),
                                                (ptr - width),
                                                (ptr - width -1),
                                                image->cb,
                                                (i32)width,
                                                image->width*8,
                                                (OMXVCM4P10IntraChromaPredMode)
                                                predMode,
                                                (i32)(availableB +
                                                 (availableA<<1) +
                                                 (availableD<<5)) );
    if (omxRes != OMX_Sts_NoErr)
        return HANTRO_NOK;

    /* advance pointers */
    ptr = image->cr;

    omxRes = omxVCM4P10_PredictIntraChroma_8x8( (ptr-1),
                                                (ptr - width),
                                                (ptr - width -1),
                                                image->cr,
                                                (i32)width,
                                                image->width*8,
                                                (OMXVCM4P10IntraChromaPredMode)
                                                predMode,
                                                (i32)(availableB +
                                                 (availableA<<1) +
                                                 (availableD<<5)) );
    if (omxRes != OMX_Sts_NoErr)
        return HANTRO_NOK;

    return(HANTRO_OK);

}



/*------------------------------------------------------------------------------

    Function: Write4x4To16x16

        Functional description:
          Write a 4x4 block (data4x4) into correct position
          in 16x16 macroblock (data).

------------------------------------------------------------------------------*/

void Write4x4To16x16(u8 *data, u8 *data4x4, u32 blockNum)
{

/* Variables */

    u32 x, y;
    u32 *in32, *out32;

/* Code */

    ASSERT(data);
    ASSERT(data4x4);
    ASSERT(blockNum < 16);

    x = h264bsdBlockX[blockNum];
    y = h264bsdBlockY[blockNum];

    data += y*16+x;

    ASSERT(((u32)data&0x3) == 0);

    /*lint --e(826) */
    out32 = (u32 *)data;
    /*lint --e(826) */
    in32 = (u32 *)data4x4;

    out32[0] = *in32++;
    out32[4] = *in32++;
    out32[8] = *in32++;
    out32[12] = *in32++;
}

/*------------------------------------------------------------------------------

    Function: DetermineIntra4x4PredMode

        Functional description:
          Returns the intra 4x4 prediction mode of a block based on the
          neighbouring macroblocks and information parsed from stream.

------------------------------------------------------------------------------*/

u32 DetermineIntra4x4PredMode(macroblockLayer_t *pMbLayer,
    u32 available, neighbour_t *nA, neighbour_t *nB, u32 index,
    mbStorage_t *nMbA, mbStorage_t *nMbB)
{

/* Variables */

    u32 mode1, mode2;
    mbStorage_t *pMb;

/* Code */

    ASSERT(pMbLayer);

    /* dc only prediction? */
    if (!available)
        mode1 = 2;
    else
    {
        pMb = nMbA;
        if (h264bsdMbPartPredMode(pMb->mbType) == PRED_MODE_INTRA4x4)
        {
            mode1 = pMb->intra4x4PredMode[nA->index];
        }
        else
            mode1 = 2;

        pMb = nMbB;
        if (h264bsdMbPartPredMode(pMb->mbType) == PRED_MODE_INTRA4x4)
        {
            mode2 = pMb->intra4x4PredMode[nB->index];
        }
        else
            mode2 = 2;

        mode1 = MIN(mode1, mode2);
    }

    if (!pMbLayer->mbPred.prevIntra4x4PredModeFlag[index])
    {
        if (pMbLayer->mbPred.remIntra4x4PredMode[index] < mode1)
        {
            mode1 = pMbLayer->mbPred.remIntra4x4PredMode[index];
        }
        else
        {
            mode1 = pMbLayer->mbPred.remIntra4x4PredMode[index] + 1;
        }
    }

    return(mode1);
}


/*lint +e702 */


