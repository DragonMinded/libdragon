#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lz4_dec_internal.h"

#define MIN_MATCH_SIZE  4
#define MIN_OFFSET 1
#define MAX_OFFSET 0xffff
#define HISTORY_SIZE 65536
#define LITERALS_RUN_LEN 15
#define MATCH_RUN_LEN 15

#define LZ4ULTRA_HEADER_SIZE        4
#define LZ4ULTRA_MAX_HEADER_SIZE    7
#define LZ4ULTRA_FRAME_SIZE         4

#define LZ4ULTRA_ENCODE_ERR         (-1)

#define LZ4ULTRA_DECODE_OK          0
#define LZ4ULTRA_DECODE_ERR_FORMAT  (-1)
#define LZ4ULTRA_DECODE_ERR_SUM     (-2)

/* Compression flags */
#define LZ4ULTRA_FLAG_FAVOR_RATIO    (1<<0)           /**< 1 to compress with the best ratio, 0 to trade some compression ratio for extra decompression speed */
#define LZ4ULTRA_FLAG_RAW_BLOCK      (1<<1)           /**< 1 to emit raw block */
#define LZ4ULTRA_FLAG_INDEP_BLOCKS   (1<<2)           /**< 1 if blocks are independent, 0 if using inter-block back references */
#define LZ4ULTRA_FLAG_LEGACY_FRAMES  (1<<3)           /**< 1 if using the legacy frames format, 0 if using the modern lz4 frame format */

#if defined(__GNUC__) || defined(__clang__)
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

#define LZ4ULTRA_DECOMPRESSOR_BUILD_LEN(__len) { \
   unsigned int byte; \
   do { \
      if (unlikely(pInBlock >= pInBlockEnd)) return -1; \
      byte = (unsigned int)*pInBlock++; \
      __len += byte; \
   } while (unlikely(byte == 255)); \
}

static void wait_dma(const void *pIn) {
   #ifdef N64
   static void *ptr; static bool finished = false;
   if (pIn == NULL) {
      finished = false;
      ptr = NULL;
      return;
   }
   if (finished) return;
   pIn += 4;
   while (ptr < pIn) {
      if (!(*(volatile uint32_t*)(0xa4600010) & 1)) {
         finished = true;
         return;
      }
      uint32_t addr = *(volatile uint32_t*)(0xa4600000); 
      ptr = (void*)(addr | 0x80000000);
   }
   #endif
}

/**
 * Decompress one data block
 *
 * @param pInBlock pointer to compressed data
 * @param nBlockSize size of compressed data, in bytes
 * @param pOutData pointer to output decompression buffer (previously decompressed bytes + room for decompressing this block)
 * @param nOutDataOffset starting index of where to store decompressed bytes in output buffer (and size of previously decompressed bytes)
 * @param nBlockMaxSize total size of output decompression buffer, in bytes
 *
 * @return size of decompressed data in bytes, or -1 for error
 */
int lz4ultra_decompressor_expand_block(const unsigned char *pInBlock, int nBlockSize, unsigned char *pOutData, int nOutDataOffset, int nBlockMaxSize, bool dma_race) {
   const unsigned char *pInBlockEnd = pInBlock + nBlockSize;
   unsigned char *pCurOutData = pOutData + nOutDataOffset;
   const unsigned char *pOutDataEnd = pCurOutData + nBlockMaxSize;
   const unsigned char *pOutDataFastEnd = pOutDataEnd - 18;

   if (dma_race) wait_dma(NULL);
   while (likely(pInBlock < pInBlockEnd)) {
      if (dma_race) wait_dma(pInBlock+1);
      const unsigned int token = (unsigned int)*pInBlock++;
      unsigned int nLiterals = ((token & 0xf0) >> 4);

      if (nLiterals != LITERALS_RUN_LEN && pCurOutData <= pOutDataFastEnd && (pInBlock + 16) <= pInBlockEnd) {
         if (dma_race) wait_dma(pInBlock+16);
         memcpy(pCurOutData, pInBlock, 16);
      }
      else {
         if (likely(nLiterals == LITERALS_RUN_LEN))
            LZ4ULTRA_DECOMPRESSOR_BUILD_LEN(nLiterals);

         if (unlikely((pInBlock + nLiterals) > pInBlockEnd)) return -1;
         if (unlikely((pCurOutData + nLiterals) > pOutDataEnd)) return -1;

         if (dma_race) wait_dma(pInBlock+nLiterals);
         memcpy(pCurOutData, pInBlock, nLiterals);
      }

      pInBlock += nLiterals;
      pCurOutData += nLiterals;

      if (likely((pInBlock + 2) <= pInBlockEnd)) {
         unsigned int nMatchOffset;

         if (dma_race) wait_dma(pInBlock+2);
         nMatchOffset = (unsigned int)*pInBlock++;
         nMatchOffset |= ((unsigned int)*pInBlock++) << 8;

         unsigned int nMatchLen = (token & 0x0f);

         nMatchLen += MIN_MATCH_SIZE;
         if (nMatchLen != (MATCH_RUN_LEN + MIN_MATCH_SIZE) && nMatchOffset >= 8 && pCurOutData <= pOutDataFastEnd) {
            const unsigned char *pSrc = pCurOutData - nMatchOffset;

            if (unlikely(pSrc < pOutData)) return -1;

            memcpy(pCurOutData, pSrc, 8);
            memcpy(pCurOutData + 8, pSrc + 8, 8);
            memcpy(pCurOutData + 16, pSrc + 16, 2);

            pCurOutData += nMatchLen;
         }
         else {
            if (likely(nMatchLen == (MATCH_RUN_LEN + MIN_MATCH_SIZE)))
               LZ4ULTRA_DECOMPRESSOR_BUILD_LEN(nMatchLen);

            if (unlikely((pCurOutData + nMatchLen) > pOutDataEnd)) return -1;

            const unsigned char *pSrc = pCurOutData - nMatchOffset;
            if (unlikely(pSrc < pOutData)) return -1;

            if (nMatchOffset >= 16 && (pCurOutData + nMatchLen) <= pOutDataFastEnd) {
               const unsigned char *pCopySrc = pSrc;
               unsigned char *pCopyDst = pCurOutData;
               const unsigned char *pCopyEndDst = pCurOutData + nMatchLen;

               do {
                  memcpy(pCopyDst, pCopySrc, 16);
                  pCopySrc += 16;
                  pCopyDst += 16;
               } while (pCopyDst < pCopyEndDst);

               pCurOutData += nMatchLen;
            }
            else {
               while (nMatchLen--) {
                  *pCurOutData++ = *pSrc++;
               }
            }
         }
      }
   }

   return (int)(pCurOutData - (pOutData + nOutDataOffset));
}
