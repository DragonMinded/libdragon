/* ----------------------------------------------------------------
 *
 * 
 * File Name:  armVCM4P10_DecodeCoeffsToPair.c
 * OpenMAX DL: v1.0.2
 * Revision:   9641
 * Date:       Thursday, February 7, 2008
 * 
 * (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
 * 
 * 
 * 
 * H.264 decode coefficients module
 * 
 */
 
#ifdef DEBUG_ARMVCM4P10_DECODECOEFFSTOPAIR
#undef DEBUG_ON
#define DEBUG_ON
#endif
 
#include "omxtypes.h"
#include "armOMX.h"
#include "omxVC.h"

#include "armCOMM.h"
#include "armCOMM_Bitstream.h"
#include "armVCM4P10_CAVLCTables.h"

#ifdef H264BSD_N64
#include "../../rsph264_internal.h"
#endif

OMX_U8 DEBUG_armVCM4P10_DecodeCoeffsToPair_LastTotalZeros;

/* 4x4 DeZigZag table */

static const OMX_U8 armVCM4P10_ZigZag[16] =
{
    0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15
};

/*
 * Description:
 * This function perform the work required by the OpenMAX
 * DecodeCoeffsToPair function and DecodeChromaDCCoeffsToPair.
 * Since most of the code is common we share it here.
 *
 * Parameters:
 * [in]	ppBitStream		Double pointer to current byte in bit stream buffer
 * [in]	pOffset			Pointer to current bit position in the byte pointed
 *								to by *ppBitStream
 * [in]	sMaxNumCoeff	Maximum number of non-zero coefficients in current
 *								block (4,15 or 16)
 * [in]	nTable          Table number (0 to 4) according to the five columns
 *                      of Table 9-5 in the H.264 spec
 * [out]	ppBitStream		*ppBitStream is updated after each block is decoded
 * [out]	pOffset			*pOffset is updated after each block is decoded
 * [out]	pNumCoeff		Pointer to the number of nonzero coefficients in
 *								this block
 * [out]	ppPosCoefbuf	Double pointer to destination residual
 *								coefficient-position pair buffer
 * Return Value:
 * Standard omxError result. See enumeration for possible result codes.

 */

OMXResult armVCM4P10_DecodeCoeffsToPair(
     const OMX_U8** ppBitStream,
     OMX_S32* pOffset,
     OMX_U8* pNumCoeff,
     OMX_U8  **ppPosCoefbuf,
     OMX_INT nTable,
     OMX_INT sMaxNumCoeff        
 )
{
    int CoeffToken, TotalCoeff, TrailingOnes;
    int Level, LevelCode, LevelPrefix, LevelSuffix, LevelSuffixSize;
    int SuffixLength, Run, ZerosLeft,CoeffNum;
    int i, Flags;
    OMX_U8 *pPosCoefbuf = *ppPosCoefbuf;
    OMX_S16 pLevel[16];
    OMX_U8  pRun[16];

    CoeffToken = armUnPackVLC32(ppBitStream, pOffset, armVCM4P10_CAVLCCoeffTokenTables[nTable]);
    armRetDataErrIf(CoeffToken == ARM_NO_CODEBOOK_INDEX, OMX_Sts_Err);

    TrailingOnes = armVCM4P10_CAVLCTrailingOnes[CoeffToken];
    TotalCoeff   = armVCM4P10_CAVLCTotalCoeff[CoeffToken];
    *pNumCoeff   = (OMX_U8)TotalCoeff;

    DEBUG_PRINTF_2("TotalCoeff = %d, TrailingOnes = %d\n", TotalCoeff, TrailingOnes);

    if (TotalCoeff == 0)
    {
        /* Nothing to do */
        return OMX_Sts_NoErr;
    }

    /* Decode trailing ones */
    for (i=TotalCoeff-1; i>=TotalCoeff-TrailingOnes; i--)
    {
        if (armGetBits(ppBitStream, pOffset, 1))
        {
            Level = -1;
        }
        else
        {
            Level = +1;
        }
        pLevel[i] = (OMX_S16)Level;

        DEBUG_PRINTF_2("Level[%d] = %d\n", i, pLevel[i]);
    }

    /* Decode (non zero) level values */
    SuffixLength = 0;
    if (TotalCoeff>10 && TrailingOnes<3)
    {
        SuffixLength=1;
    }
    for ( ; i>=0; i--)
    {
        LevelPrefix = armUnPackVLC32(ppBitStream, pOffset, armVCM4P10_CAVLCLevelPrefix);
        armRetDataErrIf(LevelPrefix == ARM_NO_CODEBOOK_INDEX, OMX_Sts_Err);

        LevelSuffixSize = SuffixLength;
        if (LevelPrefix==14 && SuffixLength==0)
        {
            LevelSuffixSize = 4;
        }
        if (LevelPrefix==15)
        {
            LevelSuffixSize = 12;
        }
        
        LevelSuffix = 0;
        if (LevelSuffixSize > 0)
        {
            LevelSuffix = armGetBits(ppBitStream, pOffset, LevelSuffixSize);
        }

        LevelCode = (LevelPrefix << SuffixLength) + LevelSuffix;


        if (LevelPrefix==15 && SuffixLength==0)
        {
            LevelCode += 15;
        }

        /* LevelCode = 2*(magnitude-1) + sign */

        if (i==TotalCoeff-1-TrailingOnes && TrailingOnes<3)
        {
            /* Level magnitude can't be 1 */
            LevelCode += 2;
        }
        if (LevelCode & 1)
        {
            /* 2a+1 maps to -a-1 */
            Level = (-LevelCode-1)>>1;
        }
        else
        {
            /* 2a+0 maps to +a+1 */
            Level = (LevelCode+2)>>1;
        }
        pLevel[i] = (OMX_S16)Level;

        DEBUG_PRINTF_2("Level[%d] = %d\n", i, pLevel[i]);

        if (SuffixLength==0)
        {
            SuffixLength=1;
        }
        if ( ((LevelCode>>1)+1)>(3<<(SuffixLength-1)) && SuffixLength<6 )
        {
            SuffixLength++;
        }
    }

    /* Decode run values */
    ZerosLeft = 0;
    if (TotalCoeff < sMaxNumCoeff)
    {
        /* Decode TotalZeros VLC */
        if (sMaxNumCoeff==4)
        {
            ZerosLeft = armUnPackVLC32(ppBitStream, pOffset, armVCM4P10_CAVLCTotalZeros2x2Tables[TotalCoeff-1]);
            armRetDataErrIf(ZerosLeft ==ARM_NO_CODEBOOK_INDEX , OMX_Sts_Err);
        }
        else
        {
            ZerosLeft = armUnPackVLC32(ppBitStream, pOffset, armVCM4P10_CAVLCTotalZeroTables[TotalCoeff-1]);
             armRetDataErrIf(ZerosLeft ==ARM_NO_CODEBOOK_INDEX , OMX_Sts_Err);
	    }
    }

    DEBUG_PRINTF_1("TotalZeros = %d\n", ZerosLeft);
    DEBUG_armVCM4P10_DecodeCoeffsToPair_LastTotalZeros = ZerosLeft;

	CoeffNum=ZerosLeft+TotalCoeff-1;

    for (i=TotalCoeff-1; i>0; i--)
    {
        Run = 0;
        if (ZerosLeft > 0)
        {
            int Table = ZerosLeft;
            if (Table > 6)
            {
                Table = 7;
            }
            Run = armUnPackVLC32(ppBitStream, pOffset, armVCM4P10_CAVLCRunBeforeTables[Table-1]);
            armRetDataErrIf(Run == ARM_NO_CODEBOOK_INDEX, OMX_Sts_Err);
        }
        pRun[i] = (OMX_U8)Run;

        DEBUG_PRINTF_2("Run[%d] = %d\n", i, pRun[i]);

        ZerosLeft -= Run;
        // ****************************************************
        // rasky: added to detect error condition that goes unnoticed
        armRetDataErrIf(ZerosLeft < 0, OMX_Sts_Err);
        // ****************************************************
    }
    pRun[0] = (OMX_U8)ZerosLeft;

    DEBUG_PRINTF_1("Run[0] = %d\n", pRun[i]);
    // for (int i=TotalCoeff;i<16;i++) {
    //     pRun[i] = 255;
    // }
    // printf("REF: Run1: %d,%d,%d,%d,%d,%d,%d,%d\n", pRun[0], pRun[1], pRun[2], pRun[3], pRun[4], pRun[5], pRun[6], pRun[7]);
    // printf("REF: Run2: %d,%d,%d,%d,%d,%d,%d,%d\n", pRun[8], pRun[9], pRun[10], pRun[11], pRun[12], pRun[13], pRun[14], pRun[15]);
    // printf("ZEROLEFT: %d\n", ZerosLeft);

    /* Fill in coefficients */
	    
    if (sMaxNumCoeff==15)
    {
        CoeffNum++; /* Skip the DC position */
    }
	
	/*for (i=0;i<TotalCoeff;i++)
		CoeffNum += pRun[i]+1;*/
    
	for (i=(TotalCoeff-1); i>=0; i--)
    {
        /*CoeffNum += pRun[i]+1;*/
        Level     = pLevel[i];

        DEBUG_PRINTF_2("Coef[%d] = %d\n", CoeffNum, Level);

        Flags = CoeffNum;

        // ****************************************************
        // rasky: added to avoid buffer overflow
        armRetDataErrIf(Flags < 0 || Flags > 15, OMX_Sts_Err);
        // ****************************************************

		CoeffNum -= (pRun[i]+1);
        if (sMaxNumCoeff>4)
        {
            /* Perform 4x4 DeZigZag */
            Flags = armVCM4P10_ZigZag[Flags];
        }
        if (i==0)
        {   
            /* End of block flag */
            Flags += 0x20;
        }
        if (Level<-128 || Level>127)
        {
            /* Overflow flag */
            Flags += 0x10;
        }
        
        *pPosCoefbuf++ = (OMX_U8)(Flags);
        *pPosCoefbuf++ = (OMX_U8)(Level & 0xFF);
        if (Flags & 0x10)
        {
            *pPosCoefbuf++ = (OMX_U8)(Level>>8);
        }
    }

    *ppPosCoefbuf = pPosCoefbuf;

    return OMX_Sts_NoErr;
}

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
static const OMX_U8 ncLookupTable2[24][2] = {
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

static OMX_U8 totalCoeffTable[40];

OMX_U8 CalcNC2(int block) {
    const OMX_U8 *k = ncLookupTable2[block];

    OMX_U8 ncLeft = totalCoeffTable[k[0]];
    OMX_U8 ncUp = totalCoeffTable[k[1]];
    
    OMX_INT hasLeft = 0;
    OMX_U8 n = 0;
    if (ncLeft != 0xFF) {
        n += ncLeft;
        hasLeft = 1;
    }
    if (ncUp != 0xFF) {
        n += ncUp;
        if (hasLeft) {
            n = (n+1)>>1;
        }
    }
    return n;
}
OMXResult HIGHFUNC_DecodeResidual(
    const OMX_U8 **ppSrc, OMX_S32 *pSrcBit, 
    OMX_U8 **ppPosCoefBuf,
    OMX_U8 *totalCoeff,
    const OMX_U8 *totalCoeffLeft,
    const OMX_U8 *totalCoeffUp,
    OMX_U32 codedBlockPattern,
    OMX_INT is16x16)
{
#if H264BSD_N64_CAVLC
    // printf("RSP\n");
    rsph264_queue_set_cavlc_buffer(0, *ppSrc, *pSrcBit);
    rsph264_queue_reset_packed_delta_buffer();    
    rsph264_queue_decode_residual(0, *ppPosCoefBuf, totalCoeff,
        totalCoeffLeft, totalCoeffUp, codedBlockPattern, is16x16);
    // rsph264_sync();
    // rsph264_cur_cavlc_buffer(ppSrc, pSrcBit);
    return OMX_Sts_NoErr;
#else
    // printf("REF\n");
    OMX_U32 blockCoded;
    OMX_U8 lumaDC = 0, chromaDCR = 0, chromaDCB = 0;
    OMXResult omxRes;

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
        omxRes = omxVCM4P10_DecodeCoeffsToPairCAVLC(
                ppSrc, pSrcBit,
                &lumaDC,
                ppPosCoefBuf,
                CalcNC2(0), 16);
        if (omxRes != OMX_Sts_NoErr)
            return omxRes;
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
                omxRes = omxVCM4P10_DecodeCoeffsToPairCAVLC(
                        ppSrc, pSrcBit,
                        &totalCoeffTable[block],
                        ppPosCoefBuf,
                        CalcNC2(block), is16x16 ? 15 : 16);
                if (omxRes != OMX_Sts_NoErr)
                    return omxRes;
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
        omxRes = omxVCM4P10_DecodeChromaDcCoeffsToPairCAVLC(
                ppSrc, pSrcBit,
                &chromaDCR,
                ppPosCoefBuf);
        if (omxRes != OMX_Sts_NoErr)
            return omxRes;
        omxRes = omxVCM4P10_DecodeChromaDcCoeffsToPairCAVLC(
                ppSrc, pSrcBit,
                &chromaDCB,
                ppPosCoefBuf);
        if (omxRes != OMX_Sts_NoErr)
            return omxRes;
    }

    // Decode residual info for chroma
    blockCoded = codedBlockPattern & 0x2;
    if (blockCoded)
    {
        for (int i=0;i<8;i++) {
            omxRes = omxVCM4P10_DecodeCoeffsToPairCAVLC(
                    ppSrc, pSrcBit,
                    &totalCoeffTable[block],
                    ppPosCoefBuf,
                    CalcNC2(block), 15);
            if (omxRes != OMX_Sts_NoErr)
                return omxRes;
            block++;
        }
    }

    for (int i=0;i<24;i++)
        totalCoeff[i] = totalCoeffTable[i];
    totalCoeff[24] = lumaDC;
    totalCoeff[25] = chromaDCR;
    totalCoeff[26] = chromaDCB;

    return OMX_Sts_NoErr;
#endif
}


