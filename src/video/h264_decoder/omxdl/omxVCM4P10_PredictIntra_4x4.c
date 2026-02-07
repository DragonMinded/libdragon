/* ----------------------------------------------------------------
 *
 * 
 * File Name:  omxVCM4P10_PredictIntra_4x4.c
 * OpenMAX DL: v1.0.2
 * Revision:   9641
 * Date:       Thursday, February 7, 2008
 * 
 * (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
 * 
 * 
 *
 * H.264 4x4 intra prediction module
 * 
 */
 
#include "omxtypes.h"
#include "armOMX.h"
#include "omxVC.h"

#include "armCOMM.h"
#include "armVC.h"

#ifdef H264BSD_N64
#include "../../rsph264_internal.h"
#include "../h264bsd_macroblock_layer.h"
#endif

/**
 * Function:  omxVCM4P10_PredictIntra_4x4   (6.3.3.1.1)
 *
 * Description:
 * Perform Intra_4x4 prediction for luma samples. If the upper-right block is 
 * not available, then duplication work should be handled inside the function. 
 * Users need not define them outside. 
 *
 * Input Arguments:
 *   
 *   pSrcLeft -  Pointer to the buffer of 4 left pixels: 
 *                  p[x, y] (x = -1, y = 0..3) 
 *   pSrcAbove - Pointer to the buffer of 8 above pixels: 
 *                  p[x,y] (x = 0..7, y =-1); 
 *               must be aligned on a 4-byte boundary. 
 *   pSrcAboveLeft - Pointer to the above left pixels: p[x,y] (x = -1, y = -1) 
 *   leftStep - Step of left pixel buffer; must be a multiple of 4. 
 *   dstStep - Step of the destination buffer; must be a multiple of 4. 
 *   predMode - Intra_4x4 prediction mode. 
 *   availability - Neighboring 4x4 block availability flag, refer to 
 *             "Neighboring Macroblock Availability" . 
 *
 * Output Arguments:
 *   
 *   pDst - Pointer to the destination buffer; must be aligned on a 4-byte 
 *            boundary. 
 *
 * Return Value:
 *    If the function runs without error, it returns OMX_Sts_NoErr. 
 *    If one of the following cases occurs, the function returns 
 *              OMX_Sts_BadArgErr: 
 *    pDst is NULL. 
 *    dstStep < 4, or dstStep is not a multiple of 4. 
 *    leftStep is not a multiple of 4. 
 *    predMode is not in the valid range of enumeration 
 *              OMXVCM4P10Intra4x4PredMode. 
 *    predMode is OMX_VC_4x4_VERT, but availability doesn't set OMX_VC_UPPER 
 *              indicating p[x,-1] (x = 0..3) is not available. 
 *    predMode is OMX_VC_4x4_HOR, but availability doesn't set OMX_VC_LEFT 
 *              indicating p[-1,y] (y = 0..3) is not available. 
 *    predMode is OMX_VC_4x4_DIAG_DL, but availability doesn't set 
 *              OMX_VC_UPPER indicating p[x, 1] (x = 0..3) is not available. 
 *    predMode is OMX_VC_4x4_DIAG_DR, but availability doesn't set 
 *              OMX_VC_UPPER_LEFT or OMX_VC_UPPER or OMX_VC_LEFT indicating 
 *              p[x,-1] (x = 0..3), or p[-1,y] (y = 0..3) or p[-1,-1] is not 
 *              available. 
 *    predMode is OMX_VC_4x4_VR, but availability doesn't set 
 *              OMX_VC_UPPER_LEFT or OMX_VC_UPPER or OMX_VC_LEFT indicating 
 *              p[x,-1] (x = 0..3), or p[-1,y] (y = 0..3) or p[-1,-1] is not 
 *              available. 
 *    predMode is OMX_VC_4x4_HD, but availability doesn't set 
 *              OMX_VC_UPPER_LEFT or OMX_VC_UPPER or OMX_VC_LEFT indicating 
 *              p[x,-1] (x = 0..3), or p[-1,y] (y = 0..3) or p[-1,-1] is not 
 *              available. 
 *    predMode is OMX_VC_4x4_VL, but availability doesn't set OMX_VC_UPPER 
 *              indicating p[x,-1] (x = 0..3) is not available. 
 *    predMode is OMX_VC_4x4_HU, but availability doesn't set OMX_VC_LEFT 
 *              indicating p[-1,y] (y = 0..3) is not available. 
 *    availability sets OMX_VC_UPPER, but pSrcAbove is NULL. 
 *    availability sets OMX_VC_LEFT, but pSrcLeft is NULL. 
 *    availability sets OMX_VC_UPPER_LEFT, but pSrcAboveLeft is NULL. 
 *    either pSrcAbove or pDst is not aligned on a 4-byte boundary.  
 *
 * Note: 
 *     pSrcAbove, pSrcAbove, pSrcAboveLeft may be invalid pointers if 
 *     they are not used by intra prediction as implied in predMode. 
 *
 */

#define COMPARE_RSP 0
#if COMPARE_RSP
#include <stdio.h>
#include <memory.h>
#endif

OMXResult omxVCM4P10_PredictIntra_4x4(
     const OMX_U8* pSrcLeft,
     const OMX_U8 *pSrcAbove,
     const OMX_U8 *pSrcAboveLeft,
     OMX_U8* pDst,
     OMX_INT leftStep,
     OMX_INT dstStep,
     OMXVCM4P10Intra4x4PredMode predMode,
     OMX_S32 availability        
 )
{
#if H264BSD_N64_INTRA
    rsph264_queue_intrapred_luma_4x4(0, //RSPH264_CACHE_SKIP_ALL,
        pSrcLeft, pSrcAbove, pSrcAboveLeft,
        pDst, leftStep, dstStep,
        predMode, availability);
    // rsph264_sync();
    return OMX_Sts_NoErr;
#else
    int x, y;
    OMX_U8 pTmp[10];

    armRetArgErrIf(pDst == NULL, OMX_Sts_BadArgErr);
    armRetArgErrIf((leftStep % 4) != 0,  OMX_Sts_BadArgErr);
    armRetArgErrIf((dstStep % 4) != 0,  OMX_Sts_BadArgErr);
    armRetArgErrIf((dstStep < 4),  OMX_Sts_BadArgErr);
    armRetArgErrIf(armNot4ByteAligned(pSrcAbove), OMX_Sts_BadArgErr);
    armRetArgErrIf(armNot4ByteAligned(pDst), OMX_Sts_BadArgErr);    
    armRetArgErrIf((availability & OMX_VC_UPPER)      && pSrcAbove     == NULL, OMX_Sts_BadArgErr);
    armRetArgErrIf((availability & OMX_VC_LEFT )      && pSrcLeft      == NULL, OMX_Sts_BadArgErr);
    armRetArgErrIf((availability & OMX_VC_UPPER_LEFT) && pSrcAboveLeft == NULL, OMX_Sts_BadArgErr);
    armRetArgErrIf(predMode==OMX_VC_4X4_VERT    && !(availability & OMX_VC_UPPER),      OMX_Sts_BadArgErr);
    armRetArgErrIf(predMode==OMX_VC_4X4_HOR     && !(availability & OMX_VC_LEFT),       OMX_Sts_BadArgErr);
    armRetArgErrIf(predMode==OMX_VC_4X4_DIAG_DL && !(availability & OMX_VC_UPPER),      OMX_Sts_BadArgErr);
    armRetArgErrIf(predMode==OMX_VC_4X4_DIAG_DR && !(availability & OMX_VC_UPPER),      OMX_Sts_BadArgErr);
    armRetArgErrIf(predMode==OMX_VC_4X4_DIAG_DR && !(availability & OMX_VC_UPPER_LEFT), OMX_Sts_BadArgErr);
    armRetArgErrIf(predMode==OMX_VC_4X4_DIAG_DR && !(availability & OMX_VC_LEFT),       OMX_Sts_BadArgErr);
    armRetArgErrIf(predMode==OMX_VC_4X4_VR      && !(availability & OMX_VC_UPPER),      OMX_Sts_BadArgErr);
    armRetArgErrIf(predMode==OMX_VC_4X4_VR      && !(availability & OMX_VC_UPPER_LEFT), OMX_Sts_BadArgErr);
    armRetArgErrIf(predMode==OMX_VC_4X4_VR      && !(availability & OMX_VC_LEFT),       OMX_Sts_BadArgErr);
    armRetArgErrIf(predMode==OMX_VC_4X4_HD      && !(availability & OMX_VC_UPPER),      OMX_Sts_BadArgErr);
    armRetArgErrIf(predMode==OMX_VC_4X4_HD      && !(availability & OMX_VC_UPPER_LEFT), OMX_Sts_BadArgErr);
    armRetArgErrIf(predMode==OMX_VC_4X4_HD      && !(availability & OMX_VC_LEFT),       OMX_Sts_BadArgErr);
    armRetArgErrIf(predMode==OMX_VC_4X4_VL      && !(availability & OMX_VC_UPPER),      OMX_Sts_BadArgErr);
    armRetArgErrIf(predMode==OMX_VC_4X4_HU      && !(availability & OMX_VC_LEFT),       OMX_Sts_BadArgErr);
    armRetArgErrIf((unsigned)predMode > OMX_VC_4X4_HU,   OMX_Sts_BadArgErr);    

#if COMPARE_RSP
    #define FAKE_PITCH 16
    static uint8_t fakeBuf[1024];
    memset(fakeBuf, 0xAB, 1024);
    uint8_t *fakeDst = fakeBuf+100;
    rsph264_queue_intrapred_luma_4x4(0, //RSPH264_CACHE_SKIP_ALL,
        pSrcLeft, pSrcAbove, pSrcAboveLeft,
        fakeDst, leftStep, FAKE_PITCH,
        predMode, availability);
    rsph264_sync();
#endif

    /* Note: This code must not read the pSrc arrays unless the corresponding
     * block is marked as available. If the block is not avaibable then pSrc
     * may not be a valid pointer.
     *
     * Note: To make the code more readable we refer to the neighbouring pixels
     * in variables named as below:
     *
     *    UL U0 U1 U2 U3 U4 U5 U6 U7
     *    L0 xx xx xx xx
     *    L1 xx xx xx xx
     *    L2 xx xx xx xx
     *    L3 xx xx xx xx
     */
     
#define UL pSrcAboveLeft[0]
#define U0 pSrcAbove[0]
#define U1 pSrcAbove[1]
#define U2 pSrcAbove[2]
#define U3 pSrcAbove[3]
#define U4 pSrcAbove[4]
#define U5 pSrcAbove[5]
#define U6 pSrcAbove[6]
#define U7 pSrcAbove[7]
#define L0 pSrcLeft[0*leftStep]
#define L1 pSrcLeft[1*leftStep]
#define L2 pSrcLeft[2*leftStep]
#define L3 pSrcLeft[3*leftStep]

    switch (predMode)
    {
    case OMX_VC_4X4_VERT:
        for (y=0; y<4; y++)
        {
            pDst[y*dstStep+0] = U0;
            pDst[y*dstStep+1] = U1;
            pDst[y*dstStep+2] = U2;
            pDst[y*dstStep+3] = U3;
        }
        break;

    case OMX_VC_4X4_HOR:
        for (x=0; x<4; x++)
        {
            pDst[0*dstStep+x] = L0;
            pDst[1*dstStep+x] = L1;
            pDst[2*dstStep+x] = L2;
            pDst[3*dstStep+x] = L3;
        }
        break;
    
    case OMX_VC_4X4_DC:
        /* This can always be used even if no blocks available */
        armVCM4P10_PredictIntraDC4x4(pSrcLeft, pSrcAbove, pDst, leftStep, dstStep, availability);
        break;
        
    case OMX_VC_4X4_DIAG_DL:
        pTmp[0] = (OMX_U8)((U0 + 2*U1 + U2 + 2)>>2);
        pTmp[1] = (OMX_U8)((U1 + 2*U2 + U3 + 2)>>2);
        if (availability & OMX_VC_UPPER_RIGHT)
        {
            pTmp[2] = (OMX_U8)((U2 + 2*U3 + U4 + 2)>>2);
            pTmp[3] = (OMX_U8)((U3 + 2*U4 + U5 + 2)>>2);
            pTmp[4] = (OMX_U8)((U4 + 2*U5 + U6 + 2)>>2);
            pTmp[5] = (OMX_U8)((U5 + 2*U6 + U7 + 2)>>2);
            pTmp[6] = (OMX_U8)((U6 + 3*U7      + 2)>>2);
        }
        else
        {
            pTmp[2] = (OMX_U8)((U2 + 3*U3      + 2)>>2);
            pTmp[3] = U3;
            pTmp[4] = U3;
            pTmp[5] = U3;
            pTmp[6] = U3;
        }
        for (y=0; y<4; y++)
        {
            for (x=0; x<4; x++)
            {
                pDst[y*dstStep+x] = pTmp[x+y];
            }
        }
        break;

    case OMX_VC_4X4_DIAG_DR:        
        /* x-y = -3, -2, -1, 0, 1, 2, 3 */
        pTmp[0] = (OMX_U8)((L1 + 2*L2 + L3 + 2)>>2);
        pTmp[1] = (OMX_U8)((L0 + 2*L1 + L2 + 2)>>2);
        pTmp[2] = (OMX_U8)((UL + 2*L0 + L1 + 2)>>2);
        pTmp[3] = (OMX_U8)((U0 + 2*UL + L0 + 2)>>2);
        pTmp[4] = (OMX_U8)((U1 + 2*U0 + UL + 2)>>2);
        pTmp[5] = (OMX_U8)((U2 + 2*U1 + U0 + 2)>>2);
        pTmp[6] = (OMX_U8)((U3 + 2*U2 + U1 + 2)>>2);
        for (y=0; y<4; y++)
        {
            for (x=0; x<4; x++)
            {
                pDst[y*dstStep+x] = pTmp[3+x-y];
            }
        }
        break;

    case OMX_VC_4X4_VR:
        /* zVR=2x-y = -3, -2, -1, 0, 1, 2, 3, 4, 5, 6
         * x-(y>>1) = -1, -1,  0, 0, 1, 1, 2, 2, 3, 3
         * y        =  3,  2,  ?, ?, ?, ?, ?, ?, 1, 0
         */
        pTmp[0] = (OMX_U8)((L2 + 2*L1 + L0 + 2)>>2);
        pTmp[1] = (OMX_U8)((L1 + 2*L0 + UL + 2)>>2);
        pTmp[2] = (OMX_U8)((L0 + 2*UL + U0 + 2)>>2);
        pTmp[3] = (OMX_U8)((UL + U0 + 1)>>1);
        pTmp[4] = (OMX_U8)((UL + 2*U0 + U1 + 2)>>2);
        pTmp[5] = (OMX_U8)((U0 + U1 + 1)>>1);
        pTmp[6] = (OMX_U8)((U0 + 2*U1 + U2 + 2)>>2);
        pTmp[7] = (OMX_U8)((U1 + U2 + 1)>>1);
        pTmp[8] = (OMX_U8)((U1 + 2*U2 + U3 + 2)>>2);
        pTmp[9] = (OMX_U8)((U2 + U3 + 1)>>1);
        for (y=0; y<4; y++)
        {
            for (x=0; x<4; x++)
            {
                pDst[y*dstStep+x] = pTmp[3+2*x-y];
            }
        }
        break;

    case OMX_VC_4X4_HD:
        /* zHD=2y-x = -3 -2 -1  0  1  2  3  4  5  6
         * y-(x>>1) = -1 -1  0  0  1  1  2  2  3  3
         * x        =  3  2                    1  0
         */
        pTmp[0] = (OMX_U8)((U2 + 2*U1 + U0 + 2)>>2);
        pTmp[1] = (OMX_U8)((U1 + 2*U0 + UL + 2)>>2);
        pTmp[2] = (OMX_U8)((U0 + 2*UL + L0 + 2)>>2);
        pTmp[3] = (OMX_U8)((UL + L0 + 1)>>1);
        pTmp[4] = (OMX_U8)((UL + 2*L0 + L1 + 2)>>2);
        pTmp[5] = (OMX_U8)((L0 + L1 + 1)>>1);
        pTmp[6] = (OMX_U8)((L0 + 2*L1 + L2 + 2)>>2);
        pTmp[7] = (OMX_U8)((L1 + L2 + 1)>>1);
        pTmp[8] = (OMX_U8)((L1 + 2*L2 + L3 + 2)>>2);
        pTmp[9] = (OMX_U8)((L2 + L3 + 1)>>1);
        for (y=0; y<4; y++)
        {
            for (x=0; x<4; x++)
            {
                pDst[y*dstStep+x] = pTmp[3+2*y-x];
            }
        }
        break;

    case OMX_VC_4X4_VL:
        /* Note: x+(y>>1) = (2*x+y)>>1
         * 2x+y = 0 1 2 3 4 5 6 7 8 9
         */
        pTmp[0] = (OMX_U8)((U0 + U1 + 1)>>1);
        pTmp[1] = (OMX_U8)((U0 + 2*U1 + U2 + 2)>>2);
        pTmp[2] = (OMX_U8)((U1 + U2 + 1)>>1);
        pTmp[3] = (OMX_U8)((U1 + 2*U2 + U3 + 2)>>2);
        pTmp[4] = (OMX_U8)((U2 + U3 + 1)>>1);
        if (availability & OMX_VC_UPPER_RIGHT)
        {
            pTmp[5] = (OMX_U8)((U2 + 2*U3 + U4 + 2)>>2);
            pTmp[6] = (OMX_U8)((U3 + U4 + 1)>>1);
            pTmp[7] = (OMX_U8)((U3 + 2*U4 + U5 + 2)>>2);
            pTmp[8] = (OMX_U8)((U4 + U5 + 1)>>1);
            pTmp[9] = (OMX_U8)((U4 + 2*U5 + U6 + 2)>>2);
        }
        else
        {
            pTmp[5] = (OMX_U8)((U2 + 3*U3 + 2)>>2);
            pTmp[6] = U3;
            pTmp[7] = U3;
            pTmp[8] = U3;
            pTmp[9] = U3;
        }
        for (y=0; y<4; y++)
        {
            for (x=0; x<4; x++)
            {
                pDst[y*dstStep+x] = pTmp[2*x+y];
            }
        }
        break;

    case OMX_VC_4X4_HU:
        /* zHU = x+2*y */
        pTmp[0] = (OMX_U8)((L0 + L1 + 1)>>1);
        pTmp[1] = (OMX_U8)((L0 + 2*L1 + L2 + 2)>>2);
        pTmp[2] = (OMX_U8)((L1 + L2 + 1)>>1);
        pTmp[3] = (OMX_U8)((L1 + 2*L2 + L3 + 2)>>2);
        pTmp[4] = (OMX_U8)((L2 + L3 + 1)>>1);
        pTmp[5] = (OMX_U8)((L2 + 3*L3 + 2)>>2);
        pTmp[6] = L3;
        pTmp[7] = L3;
        pTmp[8] = L3;
        pTmp[9] = L3;
        for (y=0; y<4; y++)
        {
            for (x=0; x<4; x++)
            {
                pDst[y*dstStep+x] = pTmp[x+2*y];
            }
        }
        break;
    }

#if COMPARE_RSP
    for (int y=0;y<4;y++) {
        for (int x=0;x<4;x++) {
            if (pDst[y*dstStep+x] != fakeDst[y*FAKE_PITCH+x]) {
                printf("FAILED COMPARE INTRA 4x4:\n");
                printf("predMode:%d, avail:0x%x\n", predMode, availability);
                printf("REF:\n");
                for (int yy=0;yy<4;yy++)
                    printf("%02x %02x %02x %02x\n", pDst[yy*dstStep+0],pDst[yy*dstStep+1], pDst[yy*dstStep+2], pDst[yy*dstStep+3]);
                printf("RSP:\n");
                for (int yy=0;yy<4;yy++)
                    printf("%02x %02x %02x %02x\n", fakeDst[yy*FAKE_PITCH+0],fakeDst[yy*FAKE_PITCH+1], fakeDst[yy*FAKE_PITCH+2], fakeDst[yy*FAKE_PITCH+3]);
                while(1){}
            }
        }
        // for (int x=-4;x<0;x++) {
        //     if (fakeDst[y*FAKE_PITCH+x] != 0xAB) {
        //         printf("FAILED INTRA OVERFLOW\n");
        //         printf("RSP LINE: %d:\n", y);
        //             printf("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", 
        //                 fakeDst[y*FAKE_PITCH-4],fakeDst[y*FAKE_PITCH-3], fakeDst[y*FAKE_PITCH-2], fakeDst[y*FAKE_PITCH-1],
        //                 fakeDst[y*FAKE_PITCH+0],fakeDst[y*FAKE_PITCH+1], fakeDst[y*FAKE_PITCH+2], fakeDst[y*FAKE_PITCH+3],
        //                 fakeDst[y*FAKE_PITCH+4],fakeDst[y*FAKE_PITCH+5], fakeDst[y*FAKE_PITCH+6], fakeDst[y*FAKE_PITCH+7]
        //                 );

        //         while(1){}
        //     }
        // }
        // for (int x=4;x<8;x++) {
        //     if (fakeDst[y*FAKE_PITCH+x] != 0xAB) {
        //         printf("FAILED INTRA OVERFLOW\n");
        //         while(1){}
        //     }
        // }
    }
#endif


    return OMX_Sts_NoErr;
#endif
}

OMXResult HIGHFUNC_PredictIntraTransform_4x4(
     const OMX_U8* pFrame,
     OMX_U8* pDst,
     OMX_INT frameStep,
     OMX_INT dstStep,
     const OMX_U8 **pCoeff,
     const OMX_U8 *totalCoeff,
     const OMX_U8* modeAvail,
     OMX_U32 qpY
) {
#if H264BSD_N64_INTRA
    uint32_t totalCoeffMask = totalCoeff ? h264bsdTotalCoeffMask(totalCoeff) : 0;

    rsph264_queue_set_packed_delta_buffer_if_changed(0, *pCoeff);
    rsph264_queue_process_luma_intra4_residual(0,
        pFrame, pDst, frameStep, dstStep,
        modeAvail, qpY, totalCoeffMask);
    // rsph264_sync();
    return OMX_Sts_NoErr;
#else
    OMX_S32 availability;
    OMXVCM4P10Intra4x4PredMode mode;
    OMXResult omxRes;
    static const OMX_U32 offsetBlockX[16] =
        { 0, 4, 0, 4, 8, 12, 8, 12, 0, 4, 0, 4, 8, 12, 8, 12 };
    static const OMX_U32 offsetBlockY[16] =
        { 0, 0, 4, 4, 0, 0, 4, 4, 8, 8, 12, 12, 8, 8, 12, 12 };

    for (int block = 0; block < 16; block++, totalCoeff++) {
        int x = offsetBlockX[block];
        int y = offsetBlockY[block];
        int width = frameStep;
        OMX_U8 *p = pDst + y*dstStep + x;
        const OMX_U8 *l, *a, *al;

        if (y == 0)
            a = pFrame - width + x;
        else
            a = p-dstStep;

        if (x == 0)
            l = pFrame + y * width -1;
        else
        {
            l = p-1;
            width = dstStep;
        }

        if (x == 0)
            al = l-width;
        else
            al = a-1;

        availability = modeAvail[block] & 0xF;
        if (availability & (1<<2)) availability |= 1<<5;
        if (availability & (1<<3)) availability |= 1<<6;
        availability &= ~((1<<2)|(1<<3));

        mode = modeAvail[block] >> 4;

        omxRes = omxVCM4P10_PredictIntra_4x4( l,
                                              a,
                                              al,
                                              p,
                                              width,
                                              dstStep,
                                              mode,
                                              availability);
        if (omxRes != OMX_Sts_NoErr)
            return omxRes;


        if (*totalCoeff)
        {
            omxRes = omxVCM4P10_DequantTransformResidualFromPairAndAdd(
                    pCoeff,
                    p,
                    NULL,
                    p,
                    dstStep,
                    dstStep,
                    qpY,
                    *totalCoeff);
            if (omxRes != OMX_Sts_NoErr)
                return omxRes;
        }
    }
    return OMX_Sts_NoErr;
#endif
}
