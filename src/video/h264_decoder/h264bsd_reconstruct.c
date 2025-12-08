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

------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"
#include "h264bsd_reconstruct.h"
#include "h264bsd_macroblock_layer.h"
#include "h264bsd_image.h"
#include "h264bsd_util.h"

#include "omxdl/omxtypes.h"
#include "omxdl/omxVC.h"
#include "omxdl/armVC.h"

#define UNUSED(x) (void)(x)

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

/* Switch off the following Lint messages for this file:
 * Info 701: Shift left of signed quantity (int)
 * Info 702: Shift right of signed quantity (int)
 */
/*lint -e701 -e702 */

/* clipping table, defined in h264bsd_intra_prediction.c */
extern const u8 h264bsdClip[];

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------

    Function: h264bsdPredictSamples

        Functional description:
          This function reconstructs a prediction for a macroblock partition.
          The prediction is either copied or interpolated using the reference
          frame and the motion vector. Both luminance and chrominance parts are
          predicted. The prediction is stored in given macroblock array (data).
        Inputs:
          data          pointer to macroblock array (384 bytes) for output
          mv            pointer to motion vector used for prediction
          refPic        pointer to reference picture structure
          xA            x-coordinate for current macroblock
          yA            y-coordinate for current macroblock
          partX         x-offset for partition in macroblock
          partY         y-offset for partition in macroblock
          partWidth     width of partition
          partHeight    height of partition
        Outputs:
          data          macroblock array (16x16+8x8+8x8) where predicted
                        partition is stored at correct position

------------------------------------------------------------------------------*/

/*lint -e{550} Symbol 'res' not accessed */
void h264bsdPredictSamples(
  image_t *pic,
  mv_t *mv,
  image_t *refPic,
  u32 colAndRow,
  u32 part,
  u8 *pFill)

{

/* Variables */

    u32 xFrac, yFrac;
    u32 width, height;
    i32 xInt, yInt, x0, y0;
    u8 *partData, *ref;
    u32 partPitch;
    OMXSize roi;
    u32 fillWidth;
    u32 fillHeight;
    OMXResult res;
    u32 xA, yA;
    u32 partX, partY;
    u32 partWidth, partHeight;

/* Code */

    ASSERT(pic->luma);
    ASSERT(pic->cb);
    ASSERT(pic->cr);
    ASSERT(mv);
    ASSERT(refPic);
    ASSERT(refPic->data);
    ASSERT(refPic->width);
    ASSERT(refPic->height);

    xA = (colAndRow & 0xFFFF0000) >> 16;
    yA = (colAndRow & 0x0000FFFF);

    partX = (part & 0xFF000000) >> 24;
    partY = (part & 0x00FF0000) >> 16;
    partWidth = (part & 0x0000FF00) >> 8;
    partHeight = (part & 0x000000FF);

    ASSERT(partWidth);
    ASSERT(partHeight);

    /* luma */
    PROFILE_START(PS_H264_INTERPRED_LUMA, 0);
    partPitch = pic->width*16;
    partData = pic->luma + partPitch*partY + partX;

    xFrac = mv->hor & 0x3;
    yFrac = mv->ver & 0x3;

    width = 16 * refPic->width;
    height = 16 * refPic->height;

    xInt = (i32)xA + (i32)partX + (mv->hor >> 2);
    yInt = (i32)yA + (i32)partY + (mv->ver >> 2);

    x0 = (xFrac) ? xInt-2 : xInt;
    y0 = (yFrac) ? yInt-2 : yInt;

    if (xFrac)
    {
        if (partWidth == 16)
            fillWidth = 32;
        else
            fillWidth = 16;
    }
    else
        fillWidth = (partWidth*2);
    if (yFrac)
        fillHeight = partHeight+5;
    else
        fillHeight = partHeight;


    if ((x0 < 0) || ((u32)x0+partWidth+(xFrac?5:0)> width) ||
        (y0 < 0) || ((u32)y0+partHeight+(yFrac?5:0) > height))
    {
        h264bsdFillBlock(refPic->data, (u8*)pFill, x0, y0, width, height,
                fillWidth, fillHeight, fillWidth);

        x0 = 0;
        y0 = 0;
        ref = pFill;
        width = fillWidth;
        if (yFrac)
            ref += 2*width;
        if (xFrac)
            ref += 2;

        #ifdef H264BSD_N64
        fast_data_cache_hit_writeback(pFill, fillWidth*fillHeight);
        pFill += fillWidth*fillHeight;
        #endif
    }
    else
    {
        /*lint --e(737) Loss of sign */
        ref = refPic->data + yInt*width + xInt;
    }
    /* Luma interpolation */
    roi.width = (i32)partWidth;
    roi.height = (i32)partHeight;

#ifdef H264BSD_N64
    rsph264_queue_interpolate_luma(RSPH264_CACHE_SKIP_ALL,
        ref, (i32)width,
        partData, partPitch,
        roi.width, roi.height,
        (i32)xFrac, (i32)yFrac);
#else
    res = omxVCM4P10_InterpolateLuma(ref, (i32)width, partData, partPitch,
                                        (i32)xFrac, (i32)yFrac, roi);
    ASSERT(res == 0);
#endif
    PROFILE_STOP(PS_H264_INTERPRED_LUMA, 0);

    /* Chroma */
    PROFILE_START(PS_H264_INTERPRED_CHROMA, 0);
    width  = 8 * refPic->width;
    height = 8 * refPic->height;

    x0 = ((xA + partX) >> 1) + (mv->hor >> 3);
    y0 = ((yA + partY) >> 1) + (mv->ver >> 3);
    xFrac = mv->hor & 0x7;
    yFrac = mv->ver & 0x7;

    ref = refPic->data + 256 * refPic->width * refPic->height;

    roi.width = (i32)(partWidth >> 1);
    fillWidth = ((partWidth >> 1) + 8) & ~0x7;
    roi.height = (i32)(partHeight >> 1);
    fillHeight = (partHeight >> 1) + 1;

    if ((x0 < 0) || ((u32)x0+roi.width+(xFrac?1:0) > width) ||
        (y0 < 0) || ((u32)y0+roi.height+(yFrac?1:0) > height))
    {
        h264bsdFillBlock(ref, pFill, x0, y0, width, height,
            fillWidth, fillHeight, fillWidth);
        ref += width * height;
        h264bsdFillBlock(ref, pFill + fillWidth*fillHeight,
            x0, y0, width, height, fillWidth,
            fillHeight, fillWidth);

        ref = pFill;
        x0 = 0;
        y0 = 0;
        width = fillWidth;
        height = fillHeight;

        #ifdef H264BSD_N64
        fast_data_cache_hit_writeback(pFill, fillWidth*fillHeight*2);
        pFill += fillWidth*fillHeight*2;
        #endif
    }

    partPitch = pic->width*8;
    partData = pic->cb + (partY>>1)*partPitch + (partX>>1);

    /* Chroma interpolation */
    /*lint --e(737) Loss of sign */
    ref += y0 * width + x0;
#ifdef H264BSD_N64
    rsph264_queue_interpolate_chroma(RSPH264_CACHE_SKIP_ALL,
        ref, width,
        partData, partPitch,
        roi.width, roi.height,
        xFrac, yFrac);
#else
    res = armVCM4P10_Interpolate_Chroma(ref, width, partData, pic->width*8,
                            (u32)roi.width, (u32)roi.height, xFrac, yFrac);
    ASSERT(res == 0);
#endif
    partData = pic->cr + (partY>>1)*partPitch + (partX>>1);
    ref += height * width;
#ifdef H264BSD_N64
    rsph264_queue_interpolate_chroma(RSPH264_CACHE_SKIP_ALL,
        ref, width,
        partData, partPitch,
        roi.width, roi.height,
        xFrac, yFrac);
#else
    res = armVCM4P10_Interpolate_Chroma(ref, width, partData, pic->width*8,
                            (u32)roi.width, (u32)roi.height, xFrac, yFrac);
    ASSERT(res == 0);
#endif

    PROFILE_STOP(PS_H264_INTERPRED_CHROMA, 0);
    (void)res;
}


/*------------------------------------------------------------------------------

    Function: FillRow1

        Functional description:
          This function gets a row of reference pels in a 'normal' case when no
          overfilling is necessary.

------------------------------------------------------------------------------*/

static void FillRow1(
  u8 *ref,
  u8 *fill,
  i32 left,
  i32 center,
  i32 right)
{
    UNUSED(left);
    UNUSED(right);
    ASSERT(ref);
    ASSERT(fill);

    H264SwDecMemcpy(fill, ref, (u32)center);

    /*lint -e(715) */
}


/*------------------------------------------------------------------------------

    Function: h264bsdFillRow7

        Functional description:
          This function gets a row of reference pels when horizontal coordinate
          is partly negative or partly greater than reference picture width
          (overfilling some pels on left and/or right edge).
        Inputs:
          ref       pointer to reference samples
          left      amount of pixels to overfill on left-edge
          center    amount of pixels to copy
          right     amount of pixels to overfill on right-edge
        Outputs:
          fill      pointer where samples are stored

------------------------------------------------------------------------------*/
#ifndef H264DEC_NEON
void h264bsdFillRow7(
  u8 *ref,
  u8 *fill,
  i32 left,
  i32 center,
  i32 right)
{
    u8 tmp;

    ASSERT(ref);
    ASSERT(fill);

    if (left)
        tmp = *ref;

    for ( ; left; left--)
        /*lint -esym(644,tmp)  tmp is initialized if used */
        *fill++ = tmp;

    for ( ; center; center--)
        *fill++ = *ref++;

    if (right)
        tmp = ref[-1];

    for ( ; right; right--)
        /*lint -esym(644,tmp)  tmp is initialized if used */
        *fill++ = tmp;
}
#endif
/*------------------------------------------------------------------------------

    Function: h264bsdFillBlock

        Functional description:
          This function gets a block of reference pels. It determines whether
          overfilling is needed or not and repeatedly calls an appropriate
          function (by using a function pointer) that fills one row the block.
        Inputs:
          ref               pointer to reference frame
          x0                x-coordinate for block
          y0                y-coordinate for block
          width             width of reference frame
          height            height of reference frame
          blockWidth        width of block
          blockHeight       height of block
          fillScanLength    length of a line in output array (pixels)
        Outputs:
          fill              pointer to array where output block is written

------------------------------------------------------------------------------*/

void h264bsdFillBlock(
  u8 *ref,
  u8 *fill,
  i32 x0,
  i32 y0,
  u32 width,
  u32 height,
  u32 blockWidth,
  u32 blockHeight,
  u32 fillScanLength)

{

/* Variables */

    i32 xstop, ystop;
    void (*fp)(u8*, u8*, i32, i32, i32);
    i32 left, x, right;
    i32 top, y, bottom;

/* Code */

    ASSERT(ref);
    ASSERT(fill);
    ASSERT(width);
    ASSERT(height);
    ASSERT(fill);
    ASSERT(blockWidth);
    ASSERT(blockHeight);

    xstop = x0 + (i32)blockWidth;
    ystop = y0 + (i32)blockHeight;

    /* Choose correct function whether overfilling on left-edge or right-edge
     * is needed or not */
    if (x0 >= 0 && xstop <= (i32)width)
        fp = FillRow1;
    else
        fp = h264bsdFillRow7;

    if (ystop < 0)
        y0 = -(i32)blockHeight;

    if (xstop < 0)
        x0 = -(i32)blockWidth;

    if (y0 > (i32)height)
        y0 = (i32)height;

    if (x0 > (i32)width)
        x0 = (i32)width;

    xstop = x0 + (i32)blockWidth;
    ystop = y0 + (i32)blockHeight;

    if (x0 > 0)
        ref += x0;

    if (y0 > 0)
        ref += y0 * (i32)width;

    left = x0 < 0 ? -x0 : 0;
    right = xstop > (i32)width ? xstop - (i32)width : 0;
    x = (i32)blockWidth - left - right;

    top = y0 < 0 ? -y0 : 0;
    bottom = ystop > (i32)height ? ystop - (i32)height : 0;
    y = (i32)blockHeight - top - bottom;

    /* Top-overfilling */
    for ( ; top; top-- )
    {
        (*fp)(ref, fill, left, x, right);
        fill += fillScanLength;
    }

    /* Lines inside reference image */
    for ( ; y; y-- )
    {
        (*fp)(ref, fill, left, x, right);
        ref += width;
        fill += fillScanLength;
    }

    ref -= width;

    /* Bottom-overfilling */
    for ( ; bottom; bottom-- )
    {
        (*fp)(ref, fill, left, x, right);
        fill += fillScanLength;
    }
}

/*lint +e701 +e702 */


