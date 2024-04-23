#include <stdio.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lz4_dec_internal.h"
#include "ringbuf_internal.h"
#include "../utils.h"
#include "../asset_internal.h"

#ifdef N64
#include <malloc.h>
#include "debug.h"
#include "dragonfs.h"
#include "n64sys.h"
#endif

#define MIN_MATCH_SIZE  4
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
      if (dma_race) wait_dma(pInBlock+1); \
      byte = (unsigned int)*pInBlock++; \
      __len += byte; \
   } while (unlikely(byte == 255)); \
}

#ifdef N64
#include "dma.h"
#endif

__attribute__((used))
static void wait_dma(const void *pIn) {
   #ifdef N64
   static void *ptr; static bool finished = false;
   if (pIn == NULL) {
      finished = false;
      ptr = NULL;
      return;
   }
   if (finished) return;
   while (ptr < pIn) {
      // Check if DMA is finished
      if (!(*PI_STATUS & 1)) {
         finished = true;
         return;
      }
      // Read current DMA position. Ignore partial cachelines as they
      // would create coherency problems if accessed by the CPU.
      ptr = (void*)((*PI_DRAM_ADDR & ~0xF) | 0x80000000);
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
int decompress_lz4_full_inplace(const unsigned char *pInBlock, size_t nBlockSize, unsigned char *pOutData, size_t nBlockMaxSize) {
#ifdef N64
   extern int decompress_lz4_full_fast(const void *inbuf, int insize, void *outbuf);
   return decompress_lz4_full_fast(pInBlock, nBlockSize, pOutData);
#else
   const unsigned char *pInBlockEnd = pInBlock + nBlockSize;
   unsigned char *pCurOutData = pOutData;
   const unsigned char *pOutDataEnd = pCurOutData + nBlockMaxSize;
   const unsigned char *pOutDataFastEnd = pOutDataEnd - 18;
   const bool dma_race = true;

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
         if (unlikely((pCurOutData + nLiterals) > pOutDataEnd)) return -2;

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

            if (unlikely(pSrc < pOutData)) return -3;

            memcpy(pCurOutData, pSrc, 8);
            memcpy(pCurOutData + 8, pSrc + 8, 8);
            memcpy(pCurOutData + 16, pSrc + 16, 2);

            pCurOutData += nMatchLen;
         }
         else {
            if (likely(nMatchLen == (MATCH_RUN_LEN + MIN_MATCH_SIZE)))
               LZ4ULTRA_DECOMPRESSOR_BUILD_LEN(nMatchLen);

            if (unlikely((pCurOutData + nMatchLen) > pOutDataEnd)) return -4;

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

   return (int)(pCurOutData - pOutData);
#endif
}

/**
 * @brief Fast-access state of the LZ4 algorithm (streaming version).
 * 
 * See the LZ4 block format for a better understanding of the fields.
 */
typedef struct lz4dec_faststate_s {
   uint8_t token;       ///< Current token
   int lit_len;         ///< Number of literals to copy
   int match_len;       ///< Number of bytes to copy from the ring buffer
   int match_off;       ///< Offset in the ring buffer to copy from
   int fsm_state;       ///< Current state of the streaming state machine
} lz4dec_faststate_t;

/**
 * @brief State of the LZ4 algorithm (streaming version).
 */
typedef struct lz4dec_state_s {
   uint8_t buf[128] __attribute__((aligned(8)));     ///< File buffer
   FILE *fp;                        ///< File pointer to read from
	int buf_idx;                     ///< Current index in the file buffer
	int buf_size;                    ///< Size of the file buffer
   bool eof;                        ///< True if we reached the end of the file
   lz4dec_faststate_t st;           ///< Fast-access state
   decompress_ringbuf_t ringbuf;    ///< Ring buffer
} lz4dec_state_t;

#ifdef N64
_Static_assert(sizeof(lz4dec_state_t) == DECOMPRESS_LZ4_STATE_SIZE, "decompress_lz4_state_t size mismatch");
#endif

static void lz4_refill(lz4dec_state_t *lz4)
{
   lz4->buf_size = fread(lz4->buf, 1, sizeof(lz4->buf), lz4->fp);
   lz4->buf_idx = 0;
   lz4->eof = (lz4->buf_size == 0);
}

static uint8_t lz4_readbyte(lz4dec_state_t *lz4)
{
   if (lz4->buf_idx >= lz4->buf_size)
      lz4_refill(lz4);
   return lz4->buf[lz4->buf_idx++];
}

static void lz4_read(lz4dec_state_t *lz4, void *buf, size_t len)
{
   while (len > 0) {
      int n = MIN(len, lz4->buf_size - lz4->buf_idx);
      memcpy(buf, lz4->buf + lz4->buf_idx, n);
      buf += n;
      len -= n;
      lz4->buf_idx += n;
      if (lz4->buf_idx >= lz4->buf_size)
         lz4_refill(lz4);
   }
}

void decompress_lz4_init(void *state, FILE *fp, int winsize)
{
   lz4dec_state_t *lz4 = (lz4dec_state_t*)state;
   lz4->fp = fp;
   __ringbuf_init(&lz4->ringbuf, state+sizeof(lz4dec_state_t), winsize);
   decompress_lz4_reset(state);
}

void decompress_lz4_reset(void *state)
{
   lz4dec_state_t *lz4 = (lz4dec_state_t*)state;
   lz4->eof = false;
   lz4->buf_idx = 0;
   lz4->buf_size = 0;
   memset(&lz4->st, 0, sizeof(lz4->st));
   lz4->ringbuf.ringbuf_pos = 0;
}

ssize_t decompress_lz4_read(void *state, void *buf, size_t len)
{
   lz4dec_state_t *lz4 = (lz4dec_state_t*)state;
   lz4dec_faststate_t st = lz4->st;
   void *buf_orig = buf;
   int n;

   while (!lz4->eof && len > 0) {
      switch (st.fsm_state) {
      case 0: // read token
         st.token = lz4_readbyte(lz4);
         st.lit_len = ((st.token & 0xf0) >> 4);
         if (unlikely(st.lit_len == LITERALS_RUN_LEN)) {
            uint8_t byte;
            do {
               byte = lz4_readbyte(lz4);
               st.lit_len += byte;
            } while (unlikely(byte == 255));
         }
         st.fsm_state = 1;
      case 1: // literals
         n = MIN(st.lit_len, len);
         lz4_read(lz4, buf, n);
         __ringbuf_write(&lz4->ringbuf, buf, n);
         buf += n;
         len -= n;
         st.lit_len -= n;
         if (st.lit_len || lz4->eof)
            break;
         st.match_off = lz4_readbyte(lz4);
         st.match_off |= ((uint16_t)lz4_readbyte(lz4)) << 8;
         st.match_len = (st.token & 0x0f);
         if (unlikely(st.match_len == MATCH_RUN_LEN)) {
            uint8_t byte;
            do {
               byte = lz4_readbyte(lz4);
               st.match_len += byte;
            } while (unlikely(byte == 255));
         }
         st.match_len += MIN_MATCH_SIZE;
         st.fsm_state = 2;
      case 2: // match
         n = MIN(st.match_len, len);
         __ringbuf_copy(&lz4->ringbuf, st.match_off, buf, n);
         buf += n;
         len -= n;
         st.match_len -= n;
         if (st.match_len)
            break;
         st.fsm_state = 0;
      }
   }

   lz4->st = st;
   return buf - buf_orig;
}
