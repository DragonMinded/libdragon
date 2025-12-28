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
          h264bsdFilterPicture
          FilterVerLumaEdge
          FilterHorLumaEdge
          FilterHorLuma
          FilterVerChromaEdge
          FilterHorChromaEdge
          FilterHorChroma
          InnerBoundaryStrength
          EdgeBoundaryStrength
          GetBoundaryStrengths
          IsSliceBoundaryOnLeft
          IsSliceBoundaryOnTop
          GetMbFilteringFlags
          GetLumaEdgeThresholds
          GetChromaEdgeThresholds
          FilterLuma
          FilterChroma

------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"
#include "h264bsd_util.h"
#include "h264bsd_macroblock_layer.h"
#include "h264bsd_deblocking.h"
#include "h264bsd_dpb.h"

#include "omxdl/omxtypes.h"
#include "omxdl/omxVC.h"
#include "omxdl/armVC.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/* Switch off the following Lint messages for this file:
 * Info 701: Shift left of signed quantity (int)
 * Info 702: Shift right of signed quantity (int)
 */
/*lint -e701 -e702 */

/* array of alpha values, from the standard */
static const u8 alphas[52] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,4,5,6,7,8,9,10,
    12,13,15,17,20,22,25,28,32,36,40,45,50,56,63,71,80,90,101,113,127,144,162,
    182,203,226,255,255};

/* array of beta values, from the standard */
static const u8 betas[52] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,3,3,3,3,4,4,
    4,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,16,16,17,17,18,18};



/* array of tc0 values, from the standard, each triplet corresponds to a
 * column in the table. Indexing goes as tc0[indexA][bS] */
static const u8 tc0[52][5] = {
    {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 1, 0},
    {0, 0, 0, 1, 0}, {0, 0, 1, 1, 0}, {0, 0, 1, 1, 0}, {0, 1, 1, 1, 0},
    {0, 1, 1, 1, 0}, {0, 1, 1, 1, 0}, {0, 1, 1, 1, 0}, {0, 1, 1, 2, 0},
    {0, 1, 1, 2, 0}, {0, 1, 1, 2, 0}, {0, 1, 1, 2, 0}, {0, 1, 2, 3, 0},
    {0, 1, 2, 3, 0}, {0, 2, 2, 3, 0}, {0, 2, 2, 4, 0}, {0, 2, 3, 4, 0},
    {0, 2, 3, 4, 0}, {0, 3, 3, 5, 0}, {0, 3, 4, 6, 0}, {0, 3, 4, 6, 0},
    {0, 4, 5, 7, 0}, {0, 4, 5, 8, 0}, {0, 4, 6, 9, 0}, {0, 5, 7, 10, 0},
    {0, 6, 8, 11, 0}, {0, 6, 8, 13, 0}, {0, 7, 10, 14, 0},
    {0, 8, 11, 16, 0}, {0, 9, 12, 18, 0}, {0, 10, 13, 20, 0},
    {0, 11, 15, 23, 0}, {0, 13, 17, 25, 0}
};



#define FILTER_LEFT_EDGE    0x04
#define FILTER_TOP_EDGE     0x02
#define FILTER_INNER_EDGE   0x01


/* clipping table defined in intra_prediction.c */
extern const u8 h264bsdClip[];

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

static u32 InnerBoundaryStrength(mbStorage_t *mb1, u32 i1, u32 i2);

static u32 InnerBoundaryStrength2(mbStorage_t *mb1, u32 i1, u32 i2);
static u32 EdgeBoundaryStrengthLeft(mbStorage_t *mb1, mbStorage_t *mb2);
static u32 EdgeBoundaryStrengthTop(mbStorage_t *mb1, mbStorage_t *mb2);

static u32 IsSliceBoundaryOnLeft(mbStorage_t *mb);

static u32 IsSliceBoundaryOnTop(mbStorage_t *mb);

static u32 GetMbFilteringFlags(mbStorage_t *mb);


static u32 GetBoundaryStrengths(mbStorage_t *mb, u8 (*bs)[16], u32 flags);

static void GetLumaEdgeThresholds(
    mbStorage_t *mb,
    u8 (*alpha)[2],
    u8 (*beta)[2],
    u8 (*threshold)[16],
    u8 (*bs)[16],
    u32 filteringFlags );

static void GetChromaEdgeThresholds(
    mbStorage_t *mb,
    u8 (*alpha)[2],
    u8 (*beta)[2],
    u8 (*threshold)[8],
    u8 (*bs)[16],
    u32 filteringFlags,
    i32 chromaQpIndexOffset);


/*------------------------------------------------------------------------------

    Function: IsSliceBoundaryOnLeft

        Functional description:
            Function to determine if there is a slice boundary on the left side
            of a macroblock.

------------------------------------------------------------------------------*/
u32 IsSliceBoundaryOnLeft(mbStorage_t *mb)
{

/* Variables */

/* Code */

    ASSERT(mb && mb->mbA);

    if (mb->sliceId != mb->mbA->sliceId)
        return(HANTRO_TRUE);
    else
        return(HANTRO_FALSE);

}

/*------------------------------------------------------------------------------

    Function: IsSliceBoundaryOnTop

        Functional description:
            Function to determine if there is a slice boundary above the
            current macroblock.

------------------------------------------------------------------------------*/
u32 IsSliceBoundaryOnTop(mbStorage_t *mb)
{

/* Variables */

/* Code */

    ASSERT(mb && mb->mbB);

    if (mb->sliceId != mb->mbB->sliceId)
        return(HANTRO_TRUE);
    else
        return(HANTRO_FALSE);

}

/*------------------------------------------------------------------------------

    Function: GetMbFilteringFlags

        Functional description:
          Function to determine which edges of a macroblock has to be
          filtered. Output is a bit-wise OR of FILTER_LEFT_EDGE,
          FILTER_TOP_EDGE and FILTER_INNER_EDGE, depending on which edges
          shall be filtered.

------------------------------------------------------------------------------*/
u32 GetMbFilteringFlags(mbStorage_t *mb)
{

/* Variables */

    u32 flags = 0;

/* Code */

    ASSERT(mb);

    /* nothing will be filtered if disableDeblockingFilterIdc == 1 */
    if (mb->disableDeblockingFilterIdc != 1)
    {
        flags |= FILTER_INNER_EDGE;

        /* filterLeftMbEdgeFlag, left mb is MB_A */
        if (mb->mbA &&
            ((mb->disableDeblockingFilterIdc != 2) ||
             !IsSliceBoundaryOnLeft(mb)))
            flags |= FILTER_LEFT_EDGE;

        /* filterTopMbEdgeFlag */
        if (mb->mbB &&
            ((mb->disableDeblockingFilterIdc != 2) ||
             !IsSliceBoundaryOnTop(mb)))
            flags |= FILTER_TOP_EDGE;
    }

    return(flags);

}

/*------------------------------------------------------------------------------

    Function: InnerBoundaryStrength

        Functional description:
            Function to calculate boundary strength value bs for an inner
            edge of a macroblock. Macroblock type is checked before this is
            called -> no intra mb condition here.

------------------------------------------------------------------------------*/
u32 InnerBoundaryStrength(mbStorage_t *mb1, u32 ind1, u32 ind2)
{
    i32 tmp1, tmp2;
    i32 mv1, mv2, mv3, mv4;

    tmp1 = mb1->totalCoeff[ind1];
    tmp2 = mb1->totalCoeff[ind2];
    mv1 = mb1->mv[ind1].hor;
    mv2 = mb1->mv[ind2].hor;
    mv3 = mb1->mv[ind1].ver;
    mv4 = mb1->mv[ind2].ver;

    if (tmp1 || tmp2)
    {
        return 2;
    }
    else if ( (ABS(mv1 - mv2) >= 4) || (ABS(mv3 - mv4) >= 4) ||
              (mb1->refAddr[ind1 >> 2] != mb1->refAddr[ind2 >> 2]) )
    {
        return 1;
    }
    else
        return 0;
}

/*------------------------------------------------------------------------------

    Function: InnerBoundaryStrength2

        Functional description:
            Function to calculate boundary strength value bs for an inner
            edge of a macroblock. The function is the same as
            InnerBoundaryStrength but without checking totalCoeff.

------------------------------------------------------------------------------*/
u32 InnerBoundaryStrength2(mbStorage_t *mb1, u32 ind1, u32 ind2)
{
    i32 tmp1, tmp2, tmp3, tmp4;

    tmp1 = mb1->mv[ind1].hor;
    tmp2 = mb1->mv[ind2].hor;
    tmp3 = mb1->mv[ind1].ver;
    tmp4 = mb1->mv[ind2].ver;

    if ( (ABS(tmp1 - tmp2) >= 4) || (ABS(tmp3 - tmp4) >= 4) ||
         (mb1->refAddr[ind1 >> 2] != mb1->refAddr[ind2 >> 2]))
    {
        return 1;
    }
    else
        return 0;
}


/*------------------------------------------------------------------------------

    Function: EdgeBoundaryStrengthTop

        Functional description:
            Function to calculate boundary strength value bs for
            top-most edge of a macroblock. Macroblock types are checked
            before this is called -> no intra mb conditions here.

------------------------------------------------------------------------------*/
u32 EdgeBoundaryStrengthTop(mbStorage_t *mb1, mbStorage_t *mb2)
{
    u32 topBs = 0;
    u32 tmp1, tmp2, tmp3, tmp4;

    tmp1 = mb1->totalCoeff[0];
    tmp2 = mb2->totalCoeff[10];
    tmp3 = mb1->totalCoeff[1];
    tmp4 = mb2->totalCoeff[11];
    if (tmp1 || tmp2)
    {
        topBs = 2<<0;
    }
    else if ((ABS(mb1->mv[0].hor - mb2->mv[10].hor) >= 4) ||
             (ABS(mb1->mv[0].ver - mb2->mv[10].ver) >= 4) ||
             (mb1->refAddr[0] != mb2->refAddr[10 >> 2]))
    {
        topBs = 1<<0;
    }
    tmp1 = mb1->totalCoeff[4];
    tmp2 = mb2->totalCoeff[14];
    if (tmp3 || tmp4)
    {
        topBs += 2<<8;
    }
    else if ((ABS(mb1->mv[1].hor - mb2->mv[11].hor) >= 4) ||
             (ABS(mb1->mv[1].ver - mb2->mv[11].ver) >= 4) ||
             (mb1->refAddr[0] != mb2->refAddr[11 >> 2]))
    {
        topBs += 1<<8;
    }
    tmp3 = mb1->totalCoeff[5];
    tmp4 = mb2->totalCoeff[15];
    if (tmp1 || tmp2)
    {
        topBs += 2<<16;
    }
    else if ((ABS(mb1->mv[4].hor - mb2->mv[14].hor) >= 4) ||
             (ABS(mb1->mv[4].ver - mb2->mv[14].ver) >= 4) ||
             (mb1->refAddr[4 >> 2] != mb2->refAddr[14 >> 2]))
    {
        topBs += 1<<16;
    }
    if (tmp3 || tmp4)
    {
        topBs += 2<<24;
    }
    else if ((ABS(mb1->mv[5].hor - mb2->mv[15].hor) >= 4) ||
             (ABS(mb1->mv[5].ver - mb2->mv[15].ver) >= 4) ||
             (mb1->refAddr[5 >> 2] != mb2->refAddr[15 >> 2]))
    {
        topBs += 1<<24;
    }

    return topBs;
}

/*------------------------------------------------------------------------------

    Function: EdgeBoundaryStrengthLeft

        Functional description:
            Function to calculate boundary strength value bs for left-
            edge of a macroblock. Macroblock types are checked
            before this is called -> no intra mb conditions here.

------------------------------------------------------------------------------*/
u32 EdgeBoundaryStrengthLeft(mbStorage_t *mb1, mbStorage_t *mb2)
{
    u32 leftBs = 0;
    u32 tmp1, tmp2, tmp3, tmp4;

    tmp1 = mb1->totalCoeff[0];
    tmp2 = mb2->totalCoeff[5];
    tmp3 = mb1->totalCoeff[2];
    tmp4 = mb2->totalCoeff[7];

    if (tmp1 || tmp2)
    {
        leftBs = 2<<0;
    }
    else if ((ABS(mb1->mv[0].hor - mb2->mv[5].hor) >= 4) ||
             (ABS(mb1->mv[0].ver - mb2->mv[5].ver) >= 4) ||
             (mb1->refAddr[0] != mb2->refAddr[5 >> 2]))
    {
        leftBs = 1<<0;
    }
    tmp1 = mb1->totalCoeff[8];
    tmp2 = mb2->totalCoeff[13];
    if (tmp3 || tmp4)
    {
        leftBs += 2<<8;
    }
    else if ((ABS(mb1->mv[2].hor - mb2->mv[7].hor) >= 4) ||
             (ABS(mb1->mv[2].ver - mb2->mv[7].ver) >= 4) ||
             (mb1->refAddr[0] != mb2->refAddr[7 >> 2]))
    {
        leftBs += 1<<8;
    }
    tmp3 = mb1->totalCoeff[10];
    tmp4 = mb2->totalCoeff[15];
    if (tmp1 || tmp2)
    {
        leftBs += 2<<16;
    }
    else if ((ABS(mb1->mv[8].hor - mb2->mv[13].hor) >= 4) ||
             (ABS(mb1->mv[8].ver - mb2->mv[13].ver) >= 4) ||
             (mb1->refAddr[8 >> 2] != mb2->refAddr[13 >> 2]))
    {
        leftBs += 1<<16;
    }
    if (tmp3 || tmp4)
    {
        leftBs += 2<<24;
    }
    else if ((ABS(mb1->mv[10].hor - mb2->mv[15].hor) >= 4) ||
             (ABS(mb1->mv[10].ver - mb2->mv[15].ver) >= 4) ||
             (mb1->refAddr[10 >> 2] != mb2->refAddr[15 >> 2]))
    {
        leftBs += 1<<24;
    }

    return leftBs;
}


/*------------------------------------------------------------------------------

    Function: h264bsdFilterPicture

        Functional description:
          Perform deblocking filtering for a picture. Filter does not copy
          the original picture anywhere but filtering is performed directly
          on the original image. Parameters controlling the filtering process
          are computed based on information in macroblock structures of the
          filtered macroblock, macroblock above and macroblock on the left of
          the filtered one.

        Inputs:
          image         pointer to image to be filtered
          mb            pointer to macroblock data structure of the top-left
                        macroblock of the picture

        Outputs:
          image         filtered image stored here

        Returns:
          none

------------------------------------------------------------------------------*/

/*lint --e{550} Symbol not accessed */
void h264bsdFilterPicture(
  image_t *image,
  mbStorage_t *mb)
{

/* Variables */

    u32 flags;
    u32 picSizeInMbs, mbRow, mbCol;
    u32 picWidthInMbs;
    u8 *data;
    mbStorage_t *pMb;
    u8 bS[2][16];
    u8 thresholdLuma[2][16];
    u8 thresholdChroma[2][8];
    u8 alpha[2][2];
    u8 beta[2][2];
    OMXResult res;

/* Code */

    ASSERT(image);
    ASSERT(mb);
    ASSERT(image->data);
    ASSERT(image->width);
    ASSERT(image->height);

    picWidthInMbs = image->width;
    data = image->data;
    picSizeInMbs = picWidthInMbs * image->height;

    pMb = mb;

    for (mbRow = 0, mbCol = 0; mbRow < image->height; pMb++)
    {
        flags = GetMbFilteringFlags(pMb);

        if (flags)
        {
            /* GetBoundaryStrengths function returns non-zero value if any of
             * the bS values for the macroblock being processed was non-zero */
            if (GetBoundaryStrengths(pMb, bS, flags))
            {

                /* Luma */
                GetLumaEdgeThresholds(pMb,alpha,beta,thresholdLuma,bS,flags);
                data = image->data + mbRow * picWidthInMbs * 256 + mbCol * 16;

                res = omxVCM4P10_FilterDeblockingLuma_VerEdge_I( data,
                                                (OMX_S32)(picWidthInMbs*16),
                                                (const OMX_U8*)alpha,
                                                (const OMX_U8*)beta,
                                                (const OMX_U8*)thresholdLuma,
                                                (const OMX_U8*)bS );

                res = omxVCM4P10_FilterDeblockingLuma_HorEdge_I( data,
                                                (OMX_S32)(picWidthInMbs*16),
                                                (const OMX_U8*)alpha+2,
                                                (const OMX_U8*)beta+2,
                                                (const OMX_U8*)thresholdLuma+16,
                                                (const OMX_U8*)bS+16 );
                /* Cb */
                GetChromaEdgeThresholds(pMb, alpha, beta, thresholdChroma,
                                        bS, flags, pMb->chromaQpIndexOffset);
                data = image->data + picSizeInMbs * 256 +
                    mbRow * picWidthInMbs * 64 + mbCol * 8;

                res = omxVCM4P10_FilterDeblockingChroma_VerEdge_I( data,
                                              (OMX_S32)(picWidthInMbs*8),
                                              (const OMX_U8*)alpha,
                                              (const OMX_U8*)beta,
                                              (const OMX_U8*)thresholdChroma,
                                              (const OMX_U8*)bS );
                res = omxVCM4P10_FilterDeblockingChroma_HorEdge_I( data,
                                              (OMX_S32)(picWidthInMbs*8),
                                              (const OMX_U8*)alpha+2,
                                              (const OMX_U8*)beta+2,
                                              (const OMX_U8*)thresholdChroma+8,
                                              (const OMX_U8*)bS+16 );
                /* Cr */
                data += (picSizeInMbs * 64);
                res = omxVCM4P10_FilterDeblockingChroma_VerEdge_I( data,
                                              (OMX_S32)(picWidthInMbs*8),
                                              (const OMX_U8*)alpha,
                                              (const OMX_U8*)beta,
                                              (const OMX_U8*)thresholdChroma,
                                              (const OMX_U8*)bS );
                res = omxVCM4P10_FilterDeblockingChroma_HorEdge_I( data,
                                              (OMX_S32)(picWidthInMbs*8),
                                              (const OMX_U8*)alpha+2,
                                              (const OMX_U8*)beta+2,
                                              (const OMX_U8*)thresholdChroma+8,
                                              (const OMX_U8*)bS+16 );
            }
        }

        mbCol++;
        if (mbCol == picWidthInMbs)
        {
            mbCol = 0;
            mbRow++;
        }
    }

    // RASKY: FIXME
    (void)res;
}

/*------------------------------------------------------------------------------

    Function: GetBoundaryStrengths

        Functional description:
            Function to calculate boundary strengths for all edges of a
            macroblock. Function returns HANTRO_TRUE if any of the bS values for
            the macroblock had non-zero value, HANTRO_FALSE otherwise.

------------------------------------------------------------------------------*/
u32 GetBoundaryStrengths(mbStorage_t *mb, u8 (*bS)[16], u32 flags)
{

/* Variables */

    /* this flag is set HANTRO_TRUE as soon as any boundary strength value is
     * non-zero */
    u32 nonZeroBs = HANTRO_FALSE;
    u32 *pTmp;
    u32 tmp1, tmp2, isIntraMb;

/* Code */

    ASSERT(mb);
    ASSERT(bS);
    ASSERT(flags);

    isIntraMb = IS_INTRA_MB(*mb);

    /* top edges */
    pTmp = (u32*)&bS[1][0];
    if (flags & FILTER_TOP_EDGE)
    {
        if (isIntraMb || IS_INTRA_MB(*mb->mbB))
        {
            *pTmp = 0x04040404;
            nonZeroBs = HANTRO_TRUE;
        }
        else
        {
            *pTmp = EdgeBoundaryStrengthTop(mb, mb->mbB);
            if (*pTmp)
                nonZeroBs = HANTRO_TRUE;
        }
    }
    else
    {
        *pTmp = 0;
    }

    /* left edges */
    pTmp = (u32*)&bS[0][0];
    if (flags & FILTER_LEFT_EDGE)
    {
        if (isIntraMb || IS_INTRA_MB(*mb->mbA))
        {
            /*bS[0][0] = bS[0][1] = bS[0][2] = bS[0][3] = 4;*/
            *pTmp = 0x04040404;
            nonZeroBs = HANTRO_TRUE;
        }
        else
        {
            *pTmp = EdgeBoundaryStrengthLeft(mb, mb->mbA);
            if (!nonZeroBs && *pTmp)
                nonZeroBs = HANTRO_TRUE;
        }
    }
    else
    {
        *pTmp = 0;
    }

    /* inner edges */
    if (isIntraMb)
    {
        pTmp++;
        *pTmp++ = 0x03030303;
        *pTmp++ = 0x03030303;
        *pTmp++ = 0x03030303;
        pTmp++;
        *pTmp++ = 0x03030303;
        *pTmp++ = 0x03030303;
        *pTmp = 0x03030303;

        nonZeroBs = HANTRO_TRUE;
    }
    else
    {
        pTmp = (u32*)mb->totalCoeff;

        /* 16x16 inter mb -> ref addresses or motion vectors cannot differ,
         * only check if either of the blocks contain coefficients */
        if (h264bsdNumMbPart(mb->mbType) == 1)
        {
            tmp1 = *pTmp++;
            tmp2 = *pTmp++;
            bS[1][4]  = (tmp1 & 0x00FF00FF) ? 2 : 0; /* [2]  || [0] */
            bS[1][5]  = (tmp1 & 0xFF00FF00) ? 2 : 0; /* [3]  || [1] */
            bS[0][4]  = (tmp1 & 0x0000FFFF) ? 2 : 0; /* [1]  || [0] */
            bS[0][5]  = (tmp1 & 0xFFFF0000) ? 2 : 0; /* [3]  || [2] */

            tmp1 = *pTmp++;
            bS[1][6]  = (tmp2 & 0x00FF00FF) ? 2 : 0; /* [6]  || [4] */
            bS[1][7]  = (tmp2 & 0xFF00FF00) ? 2 : 0; /* [7]  || [5] */
            bS[0][12] = (tmp2 & 0x0000FFFF) ? 2 : 0; /* [5]  || [4] */
            bS[0][13] = (tmp2 & 0xFFFF0000) ? 2 : 0; /* [7]  || [6] */
            tmp2 = *pTmp;
            bS[1][12] = (tmp1 & 0x00FF00FF) ? 2 : 0; /* [10] || [8] */
            bS[1][13] = (tmp1 & 0xFF00FF00) ? 2 : 0; /* [11] || [9] */
            bS[0][6]  = (tmp1 & 0x0000FFFF) ? 2 : 0; /* [9]  || [8] */
            bS[0][7]  = (tmp1 & 0xFFFF0000) ? 2 : 0; /* [11] || [10] */

            bS[1][14] = (tmp2 & 0x00FF00FF) ? 2 : 0; /* [14] || [12] */
            bS[1][15] = (tmp2 & 0xFF00FF00) ? 2 : 0; /* [15] || [13] */
            bS[0][14] = (tmp2 & 0x0000FFFF) ? 2 : 0; /* [13] || [12] */
            bS[0][15] = (tmp2 & 0xFFFF0000) ? 2 : 0; /* [15] || [14] */

            {
            u32 tmp3, tmp4;

            tmp1 = mb->totalCoeff[8];
            tmp2 = mb->totalCoeff[2];
            tmp3 = mb->totalCoeff[9];
            tmp4 = mb->totalCoeff[3];

            bS[1][8] = tmp1 || tmp2 ? 2 : 0;
            tmp1 = mb->totalCoeff[12];
            tmp2 = mb->totalCoeff[6];
            bS[1][9] = tmp3 || tmp4 ? 2 : 0;
            tmp3 = mb->totalCoeff[13];
            tmp4 = mb->totalCoeff[7];
            bS[1][10] = tmp1 || tmp2 ? 2 : 0;
            tmp1 = mb->totalCoeff[4];
            tmp2 = mb->totalCoeff[1];
            bS[1][11] = tmp3 || tmp4 ? 2 : 0;
            tmp3 = mb->totalCoeff[6];
            tmp4 = mb->totalCoeff[3];
            bS[0][8] = tmp1 || tmp2 ? 2 : 0;
            tmp1 = mb->totalCoeff[12];
            tmp2 = mb->totalCoeff[9];
            bS[0][9] = tmp3 || tmp4 ? 2 : 0;
            tmp3 = mb->totalCoeff[14];
            tmp4 = mb->totalCoeff[11];
            bS[0][10] = tmp1 || tmp2 ? 2 : 0;
            bS[0][11] = tmp3 || tmp4 ? 2 : 0;
            }
        }

        /* 16x8 inter mb -> ref addresses and motion vectors can be different
         * only for the middle horizontal edge, for the other top edges it is
         * enough to check whether the blocks contain coefficients or not. The
         * same applies to all internal left edges. */
        else if (mb->mbType == P_L0_L0_16x8)
        {
            tmp1 = *pTmp++;
            tmp2 = *pTmp++;
            bS[1][4]  = (tmp1 & 0x00FF00FF) ? 2 : 0; /* [2]  || [0] */
            bS[1][5]  = (tmp1 & 0xFF00FF00) ? 2 : 0; /* [3]  || [1] */
            bS[0][4]  = (tmp1 & 0x0000FFFF) ? 2 : 0; /* [1]  || [0] */
            bS[0][5]  = (tmp1 & 0xFFFF0000) ? 2 : 0; /* [3]  || [2] */
            tmp1 = *pTmp++;
            bS[1][6]  = (tmp2 & 0x00FF00FF) ? 2 : 0; /* [6]  || [4] */
            bS[1][7]  = (tmp2 & 0xFF00FF00) ? 2 : 0; /* [7]  || [5] */
            bS[0][12] = (tmp2 & 0x0000FFFF) ? 2 : 0; /* [5]  || [4] */
            bS[0][13] = (tmp2 & 0xFFFF0000) ? 2 : 0; /* [7]  || [6] */
            tmp2 = *pTmp;
            bS[1][12] = (tmp1 & 0x00FF00FF) ? 2 : 0; /* [10] || [8] */
            bS[1][13] = (tmp1 & 0xFF00FF00) ? 2 : 0; /* [11] || [9] */
            bS[0][6]  = (tmp1 & 0x0000FFFF) ? 2 : 0; /* [9]  || [8] */
            bS[0][7]  = (tmp1 & 0xFFFF0000) ? 2 : 0; /* [11] || [10] */

            bS[1][14] = (tmp2 & 0x00FF00FF) ? 2 : 0; /* [14] || [12] */
            bS[1][15] = (tmp2 & 0xFF00FF00) ? 2 : 0; /* [15] || [13] */
            bS[0][14] = (tmp2 & 0x0000FFFF) ? 2 : 0; /* [13] || [12] */
            bS[0][15] = (tmp2 & 0xFFFF0000) ? 2 : 0; /* [15] || [14] */

            bS[1][8] = (u8)InnerBoundaryStrength(mb, 8, 2);
            bS[1][9] = (u8)InnerBoundaryStrength(mb, 9, 3);
            bS[1][10] = (u8)InnerBoundaryStrength(mb, 12, 6);
            bS[1][11] = (u8)InnerBoundaryStrength(mb, 13, 7);

            {
            u32 tmp3, tmp4;

            tmp1 = mb->totalCoeff[4];
            tmp2 = mb->totalCoeff[1];
            tmp3 = mb->totalCoeff[6];
            tmp4 = mb->totalCoeff[3];
            bS[0][8] = tmp1 || tmp2 ? 2 : 0;
            tmp1 = mb->totalCoeff[12];
            tmp2 = mb->totalCoeff[9];
            bS[0][9] = tmp3 || tmp4 ? 2 : 0;
            tmp3 = mb->totalCoeff[14];
            tmp4 = mb->totalCoeff[11];
            bS[0][10] = tmp1 || tmp2 ? 2 : 0;
            bS[0][11] = tmp3 || tmp4 ? 2 : 0;
            }
        }
        /* 8x16 inter mb -> ref addresses and motion vectors can be different
         * only for the middle vertical edge, for the other left edges it is
         * enough to check whether the blocks contain coefficients or not. The
         * same applies to all internal top edges. */
        else if (mb->mbType == P_L0_L0_8x16)
        {
            tmp1 = *pTmp++;
            tmp2 = *pTmp++;
            bS[1][4]  = (tmp1 & 0x00FF00FF) ? 2 : 0; /* [2]  || [0] */
            bS[1][5]  = (tmp1 & 0xFF00FF00) ? 2 : 0; /* [3]  || [1] */
            bS[0][4]  = (tmp1 & 0x0000FFFF) ? 2 : 0; /* [1]  || [0] */
            bS[0][5]  = (tmp1 & 0xFFFF0000) ? 2 : 0; /* [3]  || [2] */
            tmp1 = *pTmp++;
            bS[1][6]  = (tmp2 & 0x00FF00FF) ? 2 : 0; /* [6]  || [4] */
            bS[1][7]  = (tmp2 & 0xFF00FF00) ? 2 : 0; /* [7]  || [5] */
            bS[0][12] = (tmp2 & 0x0000FFFF) ? 2 : 0; /* [5]  || [4] */
            bS[0][13] = (tmp2 & 0xFFFF0000) ? 2 : 0; /* [7]  || [6] */
            tmp2 = *pTmp;
            bS[1][12] = (tmp1 & 0x00FF00FF) ? 2 : 0; /* [10] || [8] */
            bS[1][13] = (tmp1 & 0xFF00FF00) ? 2 : 0; /* [11] || [9] */
            bS[0][6]  = (tmp1 & 0x0000FFFF) ? 2 : 0; /* [9]  || [8] */
            bS[0][7]  = (tmp1 & 0xFFFF0000) ? 2 : 0; /* [11] || [10] */

            bS[1][14] = (tmp2 & 0x00FF00FF) ? 2 : 0; /* [14] || [12] */
            bS[1][15] = (tmp2 & 0xFF00FF00) ? 2 : 0; /* [15] || [13] */
            bS[0][14] = (tmp2 & 0x0000FFFF) ? 2 : 0; /* [13] || [12] */
            bS[0][15] = (tmp2 & 0xFFFF0000) ? 2 : 0; /* [15] || [14] */

            bS[0][8] = (u8)InnerBoundaryStrength(mb, 4, 1);
            bS[0][9] = (u8)InnerBoundaryStrength(mb, 6, 3);
            bS[0][10] = (u8)InnerBoundaryStrength(mb, 12, 9);
            bS[0][11] = (u8)InnerBoundaryStrength(mb, 14, 11);

            {
            u32 tmp3, tmp4;

            tmp1 = mb->totalCoeff[8];
            tmp2 = mb->totalCoeff[2];
            tmp3 = mb->totalCoeff[9];
            tmp4 = mb->totalCoeff[3];
            bS[1][8] = tmp1 || tmp2 ? 2 : 0;
            tmp1 = mb->totalCoeff[12];
            tmp2 = mb->totalCoeff[6];
            bS[1][9] = tmp3 || tmp4 ? 2 : 0;
            tmp3 = mb->totalCoeff[13];
            tmp4 = mb->totalCoeff[7];
            bS[1][10] = tmp1 || tmp2 ? 2 : 0;
            bS[1][11] = tmp3 || tmp4 ? 2 : 0;
            }
        }
        else
        {
            tmp1 = *pTmp++;
            bS[1][4] = (tmp1 & 0x00FF00FF) ? 2 : (u8)InnerBoundaryStrength2(mb, 2, 0);
            bS[1][5] = (tmp1 & 0xFF00FF00) ? 2 : (u8)InnerBoundaryStrength2(mb, 3, 1);
            bS[0][4] = (tmp1 & 0x0000FFFF) ? 2 : (u8)InnerBoundaryStrength2(mb, 1, 0);
            bS[0][5] = (tmp1 & 0xFFFF0000) ? 2 : (u8)InnerBoundaryStrength2(mb, 3, 2);
            tmp1 = *pTmp++;
            bS[1][6]  = (tmp1 & 0x00FF00FF) ? 2 : (u8)InnerBoundaryStrength2(mb, 6, 4);
            bS[1][7]  = (tmp1 & 0xFF00FF00) ? 2 : (u8)InnerBoundaryStrength2(mb, 7, 5);
            bS[0][12] = (tmp1 & 0x0000FFFF) ? 2 : (u8)InnerBoundaryStrength2(mb, 5, 4);
            bS[0][13] = (tmp1 & 0xFFFF0000) ? 2 : (u8)InnerBoundaryStrength2(mb, 7, 6);
            tmp1 = *pTmp++;
            bS[1][12] = (tmp1 & 0x00FF00FF) ? 2 : (u8)InnerBoundaryStrength2(mb, 10, 8);
            bS[1][13] = (tmp1 & 0xFF00FF00) ? 2 : (u8)InnerBoundaryStrength2(mb, 11, 9);
            bS[0][6]  = (tmp1 & 0x0000FFFF) ? 2 : (u8)InnerBoundaryStrength2(mb, 9, 8);
            bS[0][7]  = (tmp1 & 0xFFFF0000) ? 2 : (u8)InnerBoundaryStrength2(mb, 11, 10);
            tmp1 = *pTmp;
            bS[1][14] = (tmp1 & 0x00FF00FF) ? 2 : (u8)InnerBoundaryStrength2(mb, 14, 12);
            bS[1][15] = (tmp1 & 0xFF00FF00) ? 2 : (u8)InnerBoundaryStrength2(mb, 15, 13);
            bS[0][14] = (tmp1 & 0x0000FFFF) ? 2 : (u8)InnerBoundaryStrength2(mb, 13, 12);
            bS[0][15] = (tmp1 & 0xFFFF0000) ? 2 : (u8)InnerBoundaryStrength2(mb, 15, 14);

            bS[1][8] = (u8)InnerBoundaryStrength(mb, 8, 2);
            bS[1][9] = (u8)InnerBoundaryStrength(mb, 9, 3);
            bS[1][10] = (u8)InnerBoundaryStrength(mb, 12, 6);
            bS[1][11] = (u8)InnerBoundaryStrength(mb, 13, 7);

            bS[0][8] = (u8)InnerBoundaryStrength(mb, 4, 1);
            bS[0][9] = (u8)InnerBoundaryStrength(mb, 6, 3);
            bS[0][10] = (u8)InnerBoundaryStrength(mb, 12, 9);
            bS[0][11] = (u8)InnerBoundaryStrength(mb, 14, 11);
        }
        pTmp = (u32*)&bS[0][0];
        if (!nonZeroBs && (pTmp[1] || pTmp[2] || pTmp[3] ||
                           pTmp[5] || pTmp[6] || pTmp[7]) )
        {
            nonZeroBs = HANTRO_TRUE;
        }
    }

    return(nonZeroBs);

}

/*------------------------------------------------------------------------------

    Function: GetLumaEdgeThresholds

        Functional description:
            Compute alpha, beta and tc0 thresholds for inner, left and top
            luma edges of a macroblock.

------------------------------------------------------------------------------*/
void GetLumaEdgeThresholds(
    mbStorage_t *mb,
    u8 (*alpha)[2],
    u8 (*beta)[2],
    u8 (*threshold)[16],
    u8 (*bs)[16],
    u32 filteringFlags )
{

/* Variables */

    u32 indexA, indexB;
    u32 qpAv, qp, qpTmp;
    u32 i;

/* Code */

    ASSERT(threshold);
    ASSERT(bs);
    ASSERT(beta);
    ASSERT(alpha);
    ASSERT(mb);

    qp = mb->qpY;

    indexA = (u32)CLIP3(0, 51, (i32)qp + mb->filterOffsetA);
    indexB = (u32)CLIP3(0, 51, (i32)qp + mb->filterOffsetB);

    /* Internal edge values */
    alpha[0][1] = alphas[indexA];
    alpha[1][1] = alphas[indexA];
    alpha[1][0] = alphas[indexA];
    alpha[0][0] = alphas[indexA];
    beta[0][1] = betas[indexB];
    beta[1][1] = betas[indexB];
    beta[1][0] = betas[indexB];
    beta[0][0] = betas[indexB];

    /* vertical scan order */
    for (i = 0; i < 2; i++)
    {
        u32 t1, t2;

        t1 = bs[i][0];
        t2 = bs[i][1];
        threshold[i][0]  = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][2];
        threshold[i][1]  = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][3];
        threshold[i][2]  = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][4];
        threshold[i][3]  = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][5];
        threshold[i][4]  = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][6];
        threshold[i][5]  = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][7];
        threshold[i][6]  = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][8];
        threshold[i][7]  = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][9];
        threshold[i][8]  = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][10];
        threshold[i][9]  = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][11];
        threshold[i][10] = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][12];
        threshold[i][11] = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][13];
        threshold[i][12] = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][14];
        threshold[i][13] = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][15];
        threshold[i][14] = (t1) ? tc0[indexA][t1] : 0;
        threshold[i][15] = (t2) ? tc0[indexA][t2] : 0;
    }

    if (filteringFlags & FILTER_TOP_EDGE)
    {
        qpTmp = mb->mbB->qpY;
        if (qpTmp != qp)
        {
            u32 t1, t2, t3, t4;
            qpAv = (qp + qpTmp + 1) >> 1;

            indexA = (u32)CLIP3(0, 51, (i32)qpAv + mb->filterOffsetA);
            indexB = (u32)CLIP3(0, 51, (i32)qpAv + mb->filterOffsetB);

            alpha[1][0] = alphas[indexA];
            beta[1][0] = betas[indexB];
            t1 = bs[1][0];
            t2 = bs[1][1];
            t3 = bs[1][2];
            t4 = bs[1][3];
            threshold[1][0] = (t1 && (t1 < 4)) ? tc0[indexA][t1] : 0;
            threshold[1][1] = (t2 && (t2 < 4)) ? tc0[indexA][t2] : 0;
            threshold[1][2] = (t3 && (t3 < 4)) ? tc0[indexA][t3] : 0;
            threshold[1][3] = (t4 && (t4 < 4)) ? tc0[indexA][t4] : 0;
        }
    }
    if (filteringFlags & FILTER_LEFT_EDGE)
    {
        qpTmp = mb->mbA->qpY;
        if (qpTmp != qp)
        {
            qpAv = (qp + qpTmp + 1) >> 1;

            indexA = (u32)CLIP3(0, 51, (i32)qpAv + mb->filterOffsetA);
            indexB = (u32)CLIP3(0, 51, (i32)qpAv + mb->filterOffsetB);

            alpha[0][0] = alphas[indexA];
            beta[0][0] = betas[indexB];
            threshold[0][0] = (bs[0][0] && (bs[0][0] < 4)) ? tc0[indexA][bs[0][0]] : 0;
            threshold[0][1] = (bs[0][1] && (bs[0][1] < 4)) ? tc0[indexA][bs[0][1]] : 0;
            threshold[0][2] = (bs[0][2] && (bs[0][2] < 4)) ? tc0[indexA][bs[0][2]] : 0;
            threshold[0][3] = (bs[0][3] && (bs[0][3] < 4)) ? tc0[indexA][bs[0][3]] : 0;
        }
    }

}

/*------------------------------------------------------------------------------

    Function: GetChromaEdgeThresholds

        Functional description:
            Compute alpha, beta and tc0 thresholds for inner, left and top
            chroma edges of a macroblock.

------------------------------------------------------------------------------*/
void GetChromaEdgeThresholds(
    mbStorage_t *mb,
    u8 (*alpha)[2],
    u8 (*beta)[2],
    u8 (*threshold)[8],
    u8 (*bs)[16],
    u32 filteringFlags,
    i32 chromaQpIndexOffset)
{

/* Variables */

    u32 indexA, indexB;
    u32 qpAv, qp, qpTmp;
    u32 i;

/* Code */

    ASSERT(threshold);
    ASSERT(bs);
    ASSERT(beta);
    ASSERT(alpha);
    ASSERT(mb);
    ASSERT(mb);

    qp = mb->qpY;
    qp = h264bsdQpC[CLIP3(0, 51, (i32)qp + chromaQpIndexOffset)];

    indexA = (u32)CLIP3(0, 51, (i32)qp + mb->filterOffsetA);
    indexB = (u32)CLIP3(0, 51, (i32)qp + mb->filterOffsetB);

    alpha[0][1] = alphas[indexA];
    alpha[1][1] = alphas[indexA];
    alpha[1][0] = alphas[indexA];
    alpha[0][0] = alphas[indexA];
    beta[0][1] = betas[indexB];
    beta[1][1] = betas[indexB];
    beta[1][0] = betas[indexB];
    beta[0][0] = betas[indexB];

    for (i = 0; i < 2; i++)
    {
        u32 t1, t2;

        t1 = bs[i][0];
        t2 = bs[i][1];
        threshold[i][0]  = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][2];
        threshold[i][1]  = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][3];
        threshold[i][2]  = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][8];
        threshold[i][3]  = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][9];
        threshold[i][4]  = (t1) ? tc0[indexA][t1] : 0;
        t1 = bs[i][10];
        threshold[i][5]  = (t2) ? tc0[indexA][t2] : 0;
        t2 = bs[i][11];
        threshold[i][6]  = (t1) ? tc0[indexA][t1] : 0;
        threshold[i][7]  = (t2) ? tc0[indexA][t2] : 0;
    }

    if (filteringFlags & FILTER_TOP_EDGE)
    {
        qpTmp = mb->mbB->qpY;
        if (qpTmp != mb->qpY)
        {
            u32 t1, t2, t3, t4;
            qpTmp = h264bsdQpC[CLIP3(0, 51, (i32)qpTmp + chromaQpIndexOffset)];
            qpAv = (qp + qpTmp + 1) >> 1;

            indexA = (u32)CLIP3(0, 51, (i32)qpAv + mb->filterOffsetA);
            indexB = (u32)CLIP3(0, 51, (i32)qpAv + mb->filterOffsetB);

            alpha[1][0] = alphas[indexA];
            beta[1][0] = betas[indexB];

            t1 = bs[1][0];
            t2 = bs[1][1];
            t3 = bs[1][2];
            t4 = bs[1][3];
            threshold[1][0] = (t1) ? tc0[indexA][t1] : 0;
            threshold[1][1] = (t2) ? tc0[indexA][t2] : 0;
            threshold[1][2] = (t3) ? tc0[indexA][t3] : 0;
            threshold[1][3] = (t4) ? tc0[indexA][t4] : 0;
        }
    }
    if (filteringFlags & FILTER_LEFT_EDGE)
    {
        qpTmp = mb->mbA->qpY;
        if (qpTmp != mb->qpY)
        {

            qpTmp = h264bsdQpC[CLIP3(0, 51, (i32)qpTmp + chromaQpIndexOffset)];
            qpAv = (qp + qpTmp + 1) >> 1;

            indexA = (u32)CLIP3(0, 51, (i32)qpAv + mb->filterOffsetA);
            indexB = (u32)CLIP3(0, 51, (i32)qpAv + mb->filterOffsetB);

            alpha[0][0] = alphas[indexA];
            beta[0][0] = betas[indexB];
            threshold[0][0] = (bs[0][0]) ? tc0[indexA][bs[0][0]] : 0;
            threshold[0][1] = (bs[0][1]) ? tc0[indexA][bs[0][1]] : 0;
            threshold[0][2] = (bs[0][2]) ? tc0[indexA][bs[0][2]] : 0;
            threshold[0][3] = (bs[0][3]) ? tc0[indexA][bs[0][3]] : 0;
        }
    }

}


/*lint +e701 +e702 */

