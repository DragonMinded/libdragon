/* ----------------------------------------------------------------
 *
 * 
 * File Name:  omxVCM4P10_DequantTransformResidualFromPairAndAdd.c
 * OpenMAX DL: v1.0.2
 * Revision:   9641
 * Date:       Thursday, February 7, 2008
 * 
 * (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
 * 
 * 
 *
 * H.264 inverse quantize and transform module
 * 
 */
 
#include "omxtypes.h"
#include "armOMX.h"
#include "omxVC.h"

#include "armCOMM.h"
#include "armVC.h"

#ifdef H264BSD_N64
#include <libdragon.h>
#include "../../cache.h"
#include "../../rsph264.h"
#endif

/*
 * Description:
 * Dequantize Luma AC block
 */
static void DequantLumaAC4x4(
     OMX_S16* pSrcDst,
     OMX_INT QP        
)
{
    const OMX_U8 *pVRow = &armVCM4P10_VMatrix[QP%6][0];
    int Shift = QP / 6;
    int i;
    OMX_S32 Value;

    for (i=0; i<16; i++)
    {

        Value = (pSrcDst[i] * pVRow[armVCM4P10_PosToVCol4x4[i]]) << Shift;
        pSrcDst[i] = (OMX_S16)Value;
    }
}

/**
 * Function:  omxVCM4P10_DequantTransformResidualFromPairAndAdd   (6.3.4.2.3)
 *
 * Description:
 * Reconstruct the 4x4 residual block from coefficient-position pair buffer, 
 * perform dequantization and integer inverse transformation for 4x4 block of 
 * residuals with previous intra prediction or motion compensation data, and 
 * update the pair buffer pointer to next non-empty block. If pDC == NULL, 
 * there re 16 non-zero AC coefficients at most in the packed buffer starting 
 * from 4x4 block position 0; If pDC != NULL, there re 15 non-zero AC 
 * coefficients at most in the packet buffer starting from 4x4 block position 
 * 1. 
 *
 * Input Arguments:
 *   
 *   ppSrc - Double pointer to residual coefficient-position pair buffer 
 *            output by CALVC decoding 
 *   pPred - Pointer to the predicted 4x4 block; must be aligned on a 4-byte 
 *            boundary 
 *   predStep - Predicted frame step size in bytes; must be a multiple of 4 
 *   dstStep - Destination frame step in bytes; must be a multiple of 4 
 *   pDC - Pointer to the DC coefficient of this block, NULL if it doesn't 
 *            exist 
 *   QP - QP Quantization parameter.  It should be QpC in chroma 4x4 block 
 *            decoding, otherwise it should be QpY. 
 *   AC - Flag indicating if at least one non-zero AC coefficient exists 
 *
 * Output Arguments:
 *   
 *   pDst - pointer to the reconstructed 4x4 block data; must be aligned on a 
 *            4-byte boundary 
 *
 * Return Value:
 *    OMX_Sts_NoErr, if the function runs without error.
 *    OMX_Sts_BadArgErr - bad arguments: if one of the following cases occurs: 
 *    -    pPred or pDst is NULL. 
 *    -    pPred or pDst is not 4-byte aligned. 
 *    -    predStep or dstStep is not a multiple of 4. 
 *    -    AC !=0 and Qp is not in the range of [0-51] or ppSrc == NULL. 
 *    -    AC ==0 && pDC ==NULL. 
 *
 */

OMXResult omxVCM4P10_DequantTransformResidualFromPairAndAdd(
     const OMX_U8 **ppSrc,
     const OMX_U8 *pPred,
     const OMX_S16 *pDC,
     OMX_U8 *pDst,
     OMX_INT predStep,
     OMX_INT dstStep,
     OMX_INT QP,
     OMX_INT AC
)
{
#ifdef H264BSD_N64
    (void)DequantLumaAC4x4;
    assert(pPred == pDst);
    assert(predStep == dstStep);

    #if 1  // Alternate implementation to compare to reference implementation
    rsph264_queue_set_packed_delta_buffer_if_changed(0, *ppSrc);
    rsph264_queue_dequant_transform_residual(RSPH264_CACHE_SKIP_ALL,
        pDst, dstStep, pDC, QP, AC);
    // rsph264_sync();

    #else 

    static uint8_t tmpSrc[8*8];
    static uint8_t tmpDest[8*8];

    for (int y=0; y<4; y++) {
        for (int x=0; x<4; x++) {
            tmpSrc[y*8+x] = tmpDest[y*8+x] = pPred[y*predStep+x];
        }
    }
    fast_data_cache_hit_writeback_invalidate(tmpDest, sizeof(tmpDest));

    rsph264_queue_set_packed_delta_buffer_if_changed(0, *ppSrc);
    rsph264_queue_dequant_transform_residual(0,
        tmpDest, 8, pDC, QP, AC);
    rsph264_sync();

    const uint8_t *rspPPSrc = rsph264_cur_delta_buffer(*ppSrc);
    const uint8_t *origSrc = *ppSrc;
    printf("Dequant: deltaptr: %lx\n", (uint32_t)rspPPSrc);


    // REFERENCE IMPLEMENTATION
    // ************************

    OMX_S16 pBuffer[16+4];
    OMX_S16 *pDelta;
    int i,x,y;
    
    armRetArgErrIf(pPred == NULL,            OMX_Sts_BadArgErr);
    armRetArgErrIf(armNot4ByteAligned(pPred),OMX_Sts_BadArgErr);
    armRetArgErrIf(pDst   == NULL,           OMX_Sts_BadArgErr);
    armRetArgErrIf(armNot4ByteAligned(pDst), OMX_Sts_BadArgErr);
    armRetArgErrIf(predStep & 3,             OMX_Sts_BadArgErr);
    armRetArgErrIf(dstStep & 3,              OMX_Sts_BadArgErr);
    armRetArgErrIf(AC!=0 && (QP<0),          OMX_Sts_BadArgErr);
    armRetArgErrIf(AC!=0 && (QP>51),         OMX_Sts_BadArgErr);
    armRetArgErrIf(AC!=0 && ppSrc==NULL,     OMX_Sts_BadArgErr);
    armRetArgErrIf(AC!=0 && *ppSrc==NULL,    OMX_Sts_BadArgErr);
    armRetArgErrIf(AC==0 && pDC==NULL,       OMX_Sts_BadArgErr);
    
    pDelta = armAlignTo8Bytes(pBuffer);    

    for (i=0; i<16; i++)
    {
        pDelta[i] = 0;
    }
    if (AC)
    {
        armVCM4P10_UnpackBlock4x4(ppSrc, pDelta);
        DequantLumaAC4x4(pDelta, QP);
    }
    if (pDC)
    {
        pDelta[0] = pDC[0];
    }
    armVCM4P10_TransformResidual4x4(pDelta,pDelta);

    for (y=0; y<4; y++)
    {
        for (x=0; x<4; x++)
        {
            pDst[y*dstStep+x] = (OMX_U8)armClip(0,255,pPred[y*predStep+x] + pDelta[4*y+x]);
        }
    }

    // Find differences
    for (y=0; y<4; y++)
    {
        for (x=0; x<4; x++)
        {
            if (tmpDest[y*8+x] != pDst[y*dstStep+x]) {
                printf("DEQUANT COMPARISON FAILED:\n");
                printf("QP=%d/%d AC=%d\n", QP/6, QP%6, AC);
                if (pDC != NULL)
                    printf("DC=[%x,%x,%x,%x],[%x,%x,%x,%x]", pDC[0],pDC[1],pDC[2],pDC[3],pDC[4],pDC[5],pDC[6],pDC[7]);
                printf("(%d,%d): RSP=%02x REF=%02x\n", x, y, tmpDest[y*8+x], pPred[y*dstStep+x]);
                printf("SRC:\n");
                for (int y=0;y<4;y++) {
                    uint8_t *p = &tmpSrc[y*8];
                    printf("%02x %02x %02x %02x\n", p[0], p[1], p[2], p[3]);
                }
                printf("RSP:\n");
                for (int y=0;y<4;y++) {
                    uint8_t *p = &tmpDest[y*8];
                    printf("%02x %02x %02x %02x\n", p[0], p[1], p[2], p[3]);
                }
                printf("REF:\n");
                for (int y=0;y<4;y++) {
                    const uint8_t *p = &pPred[y*predStep];
                    printf("%02x %02x %02x %02x\n", p[0], p[1], p[2], p[3]);
                }
                printf("SRC: ");
                for (const uint8_t *p = origSrc; p != *ppSrc; p++) 
                    printf("%02x ", *p);
                printf("\n");
                while(1) {}
            }
        }
    }
    (void)rspPPSrc;
    if (rspPPSrc != *ppSrc) {
        printf("SRC: orig:%lx ref:%lx rsp:%lx AC:%d\n", (uint32_t)origSrc, (uint32_t)*ppSrc, (uint32_t)rspPPSrc, AC);
        while(1) {}
    }

    #endif
#else // H264BSD_N64   
    OMX_S16 pBuffer[16+4];
    OMX_S16 *pDelta;
    int i,x,y;
    
    #ifdef H264BSD_N64
    *ppSrc = rsph264_cur_delta_buffer(*ppSrc);
    #endif

    armRetArgErrIf(pPred == NULL,            OMX_Sts_BadArgErr);
    armRetArgErrIf(armNot4ByteAligned(pPred),OMX_Sts_BadArgErr);
    armRetArgErrIf(pDst   == NULL,           OMX_Sts_BadArgErr);
    armRetArgErrIf(armNot4ByteAligned(pDst), OMX_Sts_BadArgErr);
    armRetArgErrIf(predStep & 3,             OMX_Sts_BadArgErr);
    armRetArgErrIf(dstStep & 3,              OMX_Sts_BadArgErr);
    armRetArgErrIf(AC!=0 && (QP<0),          OMX_Sts_BadArgErr);
    armRetArgErrIf(AC!=0 && (QP>51),         OMX_Sts_BadArgErr);
    armRetArgErrIf(AC!=0 && ppSrc==NULL,     OMX_Sts_BadArgErr);
    armRetArgErrIf(AC!=0 && *ppSrc==NULL,    OMX_Sts_BadArgErr);
    armRetArgErrIf(AC==0 && pDC==NULL,       OMX_Sts_BadArgErr);
    
    pDelta = armAlignTo8Bytes(pBuffer);    

    for (i=0; i<16; i++)
    {
        pDelta[i] = 0;
    }
    if (AC)
    {
        armVCM4P10_UnpackBlock4x4(ppSrc, pDelta);
        DequantLumaAC4x4(pDelta, QP);
        // if (AC == 2) {        
        //     printf("After dequant:\n");
        //     for (i=0;i<4;i++) {
        //         printf("%04x %04x %04x %04x\n", (uint16_t)pDelta[i*4+0], (uint16_t)pDelta[i*4+1], (uint16_t)pDelta[i*4+2], (uint16_t)pDelta[i*4+3]);
        //     }
        // }
    }
    if (pDC)
    {
        pDelta[0] = pDC[0];
    }
    armVCM4P10_TransformResidual4x4(pDelta,pDelta);

    // if (AC == 2) {        
    //     printf("After IDCT:\n");
    //     for (i=0;i<4;i++) {
    //         printf("%04x %04x %04x %04x\n", (uint16_t)pDelta[i*4+0], (uint16_t)pDelta[i*4+1], (uint16_t)pDelta[i*4+2], (uint16_t)pDelta[i*4+3]);
    //     }
    //     printf("Pixels:\n");
    //     for (i=0;i<4;i++) {
    //         printf("%04x %04x %04x %04x\n", (uint16_t)pPred[i*predStep+0], (uint16_t)pPred[i*predStep+1], (uint16_t)pPred[i*predStep+2], (uint16_t)pPred[i*predStep+3]);
    //     }
    // }

    for (y=0; y<4; y++)
    {
        for (x=0; x<4; x++)
        {
            pDst[y*dstStep+x] = (OMX_U8)armClip(0,255,pPred[y*predStep+x] + pDelta[4*y+x]);
        }
    }
#endif

    return OMX_Sts_NoErr;
}


OMXResult HIGHFUNC_ProcessLumaInterResidual (
    const OMX_U8 **ppSrc,
    const OMX_U8 *pPred,
    OMX_U8 *pDst,
    OMX_INT predStep,
    OMX_INT dstStep,
    OMX_INT QP,
    OMX_U8 *AC
) {
#ifdef H264BSD_N64
    assert(pPred == pDst);
    assert(predStep == dstStep);
    #if H264BSD_N64_CAVLC
    AC = NULL;
    #endif

    rsph264_queue_set_packed_delta_buffer_if_changed(0, *ppSrc);
    rsph264_queue_process_luma_inter_residual(RSPH264_CACHE_SKIP_ALL,
        pDst, dstStep, 0, QP, AC);
#else
    static const OMX_U32 offset[16][2] = {
        {0,0},  {4,0},  {0,4},  {4,4},
        {8,0},  {12,0}, {8,4},  {12,4},
        {0,8},  {4,8},  {0,12}, {4,12},
        {8,8},  {12,8}, {8,12}, {12,12}
    };

    for (int i = 0; i < 16; i++, AC++)
    {
        int x = offset[i][0];
        int y = offset[i][1];
        OMX_U8 *p = pDst + y*dstStep + x;
        if (*AC)
        {
            OMX_U32 res = omxVCM4P10_DequantTransformResidualFromPairAndAdd(
                    ppSrc, p, 0, p, predStep, dstStep, QP, *AC);
            if (res != OMX_Sts_NoErr) {
                return res;
            }
        }
    }
#endif

    return OMX_Sts_NoErr;
}

OMXResult HIGHFUNC_ProcessLumaIntra16x16Residual (
    const OMX_U8 *pSrc,
    OMX_U8 *pDst,
    OMX_INT srcStep,
    OMX_INT dstStep,
    const OMX_U8 **pCoeff,
    const OMX_U8 *totalCoeff,
    OMX_S32 predMode,
    OMX_S32 availability,
    OMX_INT QP
) {
#if H264BSD_N64_INTRA
    #if H264BSD_N64_CAVLC
    totalCoeff = NULL;
    #endif
    rsph264_queue_set_packed_delta_buffer_if_changed(0, *pCoeff);
    rsph264_queue_process_luma_intra16_residual(RSPH264_CACHE_SKIP_ALL,
        pSrc, pDst, srcStep, dstStep,
        predMode, availability,
        QP, totalCoeff);
#else
    static const OMX_U32 dcCoeffIndex[16] =
        {0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15};
    static const OMX_U32 offset[16][2] = {
        {0,0},  {4,0},  {0,4},  {4,4},
        {8,0},  {12,0}, {8,4},  {12,4},
        {0,8},  {4,8},  {0,12}, {4,12},
        {8,8},  {12,8}, {8,12}, {12,12}
    };

    OMXResult result;
    OMX_S16 dc[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    int i;

    if (totalCoeff[24]) {
        result = omxVCM4P10_TransformDequantLumaDCFromPair(pCoeff, dc, QP);
        if (result != OMX_Sts_NoErr)
            return result;
    }

    result = omxVCM4P10_PredictIntra_16x16(
            (pSrc-1),
            (pSrc - srcStep),
            (pSrc - srcStep-1),
            pDst,
            srcStep,
            dstStep,
            (OMXVCM4P10Intra16x16PredMode)predMode,
            availability);
    if (result != OMX_Sts_NoErr)
        return result;

    for (i = 0; i < 16; i++, totalCoeff++)
    {
        int x = offset[i][0];
        int y = offset[i][1];
        OMX_U8 *p = pDst + y*dstStep + x;
        OMX_S16 *pDc = &dc[dcCoeffIndex[i]];
        if (*totalCoeff || *pDc)
        {
            result = omxVCM4P10_DequantTransformResidualFromPairAndAdd(
                    pCoeff,
                    p,
                    pDc,
                    p,
                    dstStep,
                    dstStep,
                    QP,
                    *totalCoeff);
            if (result != OMX_Sts_NoErr)
                return result;
        }
    }
#endif
    return OMX_Sts_NoErr;
}


OMXResult HIGHFUNC_ProcessChromaResidual (
    const OMX_U8 **pSrc,
    OMX_U8 *pDst1,
    OMX_U8 *pDst2,
    OMX_INT dstStep1,
    OMX_INT dstStep2,
    OMX_INT chromaQp,
    const OMX_U8 *totalCoeff
) {
#ifdef H264BSD_N64
    assert(dstStep1 == dstStep2);
    #if H264BSD_N64_CAVLC
    totalCoeff = NULL;
    #endif

    rsph264_queue_set_packed_delta_buffer_if_changed(0, *pSrc);
    rsph264_queue_process_chroma_residual(RSPH264_CACHE_SKIP_ALL,
        pDst1, pDst2, dstStep1, chromaQp, totalCoeff);
#else
    #define COMPARE_RSP 0

    #if COMPARE_RSP
    const OMX_U8 *pOrigSrc = *pSrc;
    OMX_U8 fakeDst[128], fakeDstPre[128];
    assert(pDst1+64 == pDst2);
    assert(dstStep1 == dstStep2);
    assert(dstStep1 == 8);
    memcpy(fakeDst, pDst1, 128);
    memcpy(fakeDstPre, pDst1, 128);
    uint32_t acflag = 0;
    for (int i=0;i<8;i++) 
        if (totalCoeff[16+i])
            acflag |= 1<<i;
    if (totalCoeff[25]) acflag |= 1<<9;
    if (totalCoeff[26]) acflag |= 1<<10;
    #endif

    OMX_U32 i, b;
    OMX_S16 *pDc;
    OMX_S16 dc[4 + 4] = {0,0,0,0,0,0,0,0};
    OMXResult result;
    OMX_U8 *p;
    static const OMX_U32 chromaIndex[4][2] = { 
        {0, 0}, {4, 0}, 
        {0, 4}, {4, 4},
    };

    if (totalCoeff[25])
    {
        pDc = dc;
        result = omxVCM4P10_TransformDequantChromaDCFromPair(
                pSrc,
                pDc,
                (OMX_S32)chromaQp);
        if (result != OMX_Sts_NoErr)
            return result;
    }
    if (totalCoeff[26])
    {
        pDc = dc+4;
        result = omxVCM4P10_TransformDequantChromaDCFromPair(
                pSrc,
                pDc,
                (OMX_S32)chromaQp);
        if (result != OMX_Sts_NoErr)
            return result;
    }

    pDc = dc;
    totalCoeff = totalCoeff + 16;
    for (b = 0; b < 2; b++) {    
        for (i = 0; i < 4; i++, pDc++, totalCoeff++)
        {
            /* chroma prediction */
            if (*totalCoeff || *pDc)
            {
                if (b == 0) {
                    p = pDst1 + chromaIndex[i][1]*dstStep1 + chromaIndex[i][0];
                } else {
                    p = pDst2 + chromaIndex[i][1]*dstStep2 + chromaIndex[i][0];                
                }

                result = omxVCM4P10_DequantTransformResidualFromPairAndAdd(
                        pSrc,
                        p,
                        pDc,
                        p,
                        (b == 0 ? dstStep1 : dstStep2),
                        (b == 0 ? dstStep1 : dstStep2),
                        (OMX_S32)chromaQp,
                        *totalCoeff);
                if (result != OMX_Sts_NoErr)
                    return result;
            }
        }
    }


    #if COMPARE_RSP
    rsph264_queue_set_packed_delta_buffer_if_changed(0, 0);
    rsph264_queue_set_packed_delta_buffer_if_changed(0, pOrigSrc);
    rsph264_queue_process_chroma_residual(0,
        fakeDst, dstStep1, chromaQp, acflag);
    rsph264_sync();

    static int counter = 0;
    for (int i=0;i<128;i++) {
        if (fakeDst[i] != pDst1[i]) {
            int b = i / 64;
            int y = (i%64) / 8;
            int x = (i%64) % 8;
            printf("CHROMARES: %d(%d,%d) @ %d\n", b,x,y,counter);
            printf("RSP=%02x REF=%02x PRE=%02x\n", fakeDst[i], pDst1[i], fakeDstPre[i]);
            printf("SRC:\n%02x%02x%02x%02x %02x%02x%02x%02x\n",
                pOrigSrc[0],pOrigSrc[1],pOrigSrc[2],pOrigSrc[3],
                pOrigSrc[4],pOrigSrc[5],pOrigSrc[6],pOrigSrc[7]);
            while(1){}
        }
    }
    ++counter;

    #endif


#endif

    return OMX_Sts_NoErr;
}

/* End of file */
