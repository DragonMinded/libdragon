/*
 * shrink.c - compressor implementation
 *
 * Copyright (C) 2019 Emmanuel Marty
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

/*
 * Uses the libdivsufsort library Copyright (c) 2003-2008 Yuta Mori
 *
 * Inspired by cap by Sven-�ke Dahl. https://github.com/svendahl/cap
 * Also inspired by Charles Bloom's compression blog. http://cbloomrants.blogspot.com/
 * With ideas from LZ4 by Yann Collet. https://github.com/lz4/lz4
 * With help and support from spke <zxintrospec@gmail.com>
 *
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "libapultra.h"
#include "matchfinder.h"
#include "shrink.h"
#include "format.h"

#define TOKEN_CODE_LARGE_MATCH   2 /* 10 */
#define TOKEN_SIZE_LARGE_MATCH   2

#define TOKEN_CODE_7BIT_MATCH    6 /* 110 */
#define TOKEN_SIZE_7BIT_MATCH    3

#define TOKEN_CODE_4BIT_MATCH    7 /* 111 */
#define TOKEN_SIZE_4BIT_MATCH    3

#define CountShift(N,bits)  if ((N)>>(bits)) { (N)>>=(bits); (n) += (bits); }

/** Gamma2 bit counts for common values, up to 2047 */
static const char _gamma2_size[2048] = {
   0, 0, 2, 2, 4, 4, 4, 4, 6, 6, 6, 6, 6, 6, 6, 6, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
};

/**
 * Write bitpacked value to output (compressed) buffer
 *
 * @param pOutData pointer to output buffer
 * @param nOutOffset current write index into output buffer
 * @param nMaxOutDataSize maximum size of output buffer, in bytes
 * @param nValue value to write
 * @param nBits number of least significant bits to write in value
 * @param nCurBitsOffset write index into output buffer, of current byte being filled with bits
 * @param nCurBitShift bit shift count
 *
 * @return updated write index into output buffer, or -1 in case of an error
 */
static int apultra_write_bits(unsigned char *pOutData, int nOutOffset, const int nMaxOutDataSize, const int nValue, const int nBits, int *nCurBitsOffset, int *nCurBitShift) {
   if (nOutOffset >= 0) {
      int i;

      for (i = nBits - 1; i >= 0; i--) {
         if ((*nCurBitShift) == -1) {
            /* Allocate a new byte in the stream to pack bits in */
            if (nOutOffset >= nMaxOutDataSize) return -1;
            (*nCurBitsOffset) = nOutOffset;
            (*nCurBitShift) = 7;
            pOutData[nOutOffset++] = 0;
         }

         pOutData[(*nCurBitsOffset)] |= ((nValue >> i) & 1) << (*nCurBitShift);

         (*nCurBitShift)--;
      }
   }

   return nOutOffset;
}

/**
 * Get size of gamma2 encoded value
 *
 * @param nValue value of evaluate (2..n)
 *
 * @return number of bits required
 */
static int apultra_get_gamma2_size(int nValue) {
   if (nValue >= 0 && nValue < 2048)
      return _gamma2_size[nValue];
   else {
      unsigned int n = 0;
      CountShift(nValue, 16);
      CountShift(nValue, 8);
      CountShift(nValue, 4);
      CountShift(nValue, 2);
      CountShift(nValue, 1);

      return n << 1;
   }
}

/**
 * Write gamma2 encoded value to output (compressed) buffer
 *
 * @param pOutData pointer to output buffer
 * @param nOutOffset current write index into output buffer
 * @param nMaxOutDataSize maximum size of output buffer, in bytes
 * @param nValue value of write (2..n)
 * @param nCurBitsOffset write index into output buffer, of current byte being filled with bits
 * @param nCurBitShift bit shift count
 *
 * @return updated write index into output buffer, or -1 in case of an error
 */
static int apultra_write_gamma2_value(unsigned char *pOutData, int nOutOffset, const int nMaxOutDataSize, const int nValue, int *nCurBitsOffset, int *nCurBitShift) {
   int msb = 30;
   while ((nValue >> msb--) == 0);

   while (msb > 0) {
      const int bit = (nValue >> --msb) & 2;
   
      nOutOffset = apultra_write_bits(pOutData, nOutOffset, nMaxOutDataSize, bit | 1, 2, nCurBitsOffset, nCurBitShift);
   }

   return apultra_write_bits(pOutData, nOutOffset, nMaxOutDataSize, (nValue & 1) << 1, 2, nCurBitsOffset, nCurBitShift);
}

/**
 * Get the number of extra bits required to represent a match offset
 *
 * @param nLength match length
 * @param nMatchOffset match offset
 * @param nFollowsLiteral non-zero if the match follows a literal, zero if it immediately follows another match
 *
 * @return number of extra bits required
 */
static inline int apultra_get_offset_varlen_size(const int nLength, const int nMatchOffset, const int nFollowsLiteral) {
   if (nLength <= 3 && nMatchOffset < 128)
      return 8 + TOKEN_SIZE_7BIT_MATCH;
   else {
      if (nFollowsLiteral)
         return 8 + TOKEN_SIZE_LARGE_MATCH + apultra_get_gamma2_size((nMatchOffset >> 8) + 3);
      else
         return 8 + TOKEN_SIZE_LARGE_MATCH + apultra_get_gamma2_size((nMatchOffset >> 8) + 2);
   }
}

/**
 * Get the number of extra bits required to represent a match length
 *
 * @param nLength match length
 * @param nMatchOffset match offset
 *
 * @return number of extra bits required
 */
static inline int apultra_get_match_varlen_size(const int nLength, const int nMatchOffset) {
   if (nLength <= 3 && nMatchOffset < 128)
      return 0;
   else {
      if (nMatchOffset < 128 || nMatchOffset >= MINMATCH4_OFFSET)
         return apultra_get_gamma2_size(nLength - 2);
      else if (nMatchOffset < MINMATCH3_OFFSET)
         return apultra_get_gamma2_size(nLength);
      else
         return apultra_get_gamma2_size(nLength - 1);
   }
}

/**
 * Insert forward rep candidate
 *
 * @param pCompressor compression context
 * @param pInWindow pointer to input data window (previously compressed bytes + bytes to compress)
 * @param i input data window position whose matches are being considered
 * @param nMatchOffset match offset to use as rep candidate
 * @param nStartOffset current offset in input window (typically the number of previously compressed bytes)
 * @param nEndOffset offset to end finding matches at (typically the size of the total input window in bytes
 * @param nArrivalsPerPosition maximum number of arrivals per input buffer position
 * @param nDepth current insertion depth
 */
static void apultra_insert_forward_match(apultra_compressor *pCompressor, const unsigned char *pInWindow, const int i, const int nMatchOffset, const int nStartOffset, const int nEndOffset, const int nArrivalsPerPosition, const int nDepth) {
   const apultra_arrival *arrival = pCompressor->arrival + ((i - nStartOffset) * nArrivalsPerPosition);
   const int *rle_len = (const int*)pCompressor->intervals /* reuse */;
   apultra_visited* visited = ((apultra_visited*)pCompressor->pos_data) - nStartOffset /* reuse */;
   int j;

   for (j = 0; j < nArrivalsPerPosition && arrival[j].from_slot; j++) {
      if (arrival[j].follows_literal) {
         const int nRepOffset = arrival[j].rep_offset;

         if (nMatchOffset != nRepOffset) {
            const int nRepPos = arrival[j].rep_pos;

            if (nRepPos >= nStartOffset &&
               (nRepPos + 1) < nEndOffset &&
               visited[nRepPos] != nMatchOffset) {

               visited[nRepPos] = nMatchOffset;

               apultra_match* fwd_match = pCompressor->match + ((nRepPos - nStartOffset) << MATCHES_PER_INDEX_SHIFT);

               if (fwd_match[NMATCHES_PER_INDEX - 1].length == 0) {
                  if (nRepPos >= nMatchOffset) {
                     const unsigned char* pInWindowStart = pInWindow + nRepPos;

                     if (!memcmp(pInWindowStart, pInWindowStart - nMatchOffset, 2)) {
                        if (nRepOffset) {
                           const int nLen0 = rle_len[nRepPos - nMatchOffset];
                           const int nLen1 = rle_len[nRepPos];
                           const int nMinLen = (nLen0 < nLen1) ? nLen0 : nLen1;

                           unsigned int nMaxRepLen = nEndOffset - nRepPos;
                           if (nMaxRepLen > LCP_MAX)
                              nMaxRepLen = LCP_MAX;

                           const unsigned char* pInWindowMax = pInWindowStart + nMaxRepLen;
                           const unsigned char* pInWindowAtRepOffset = pInWindowStart + nMinLen;

                           if (pInWindowAtRepOffset > pInWindowMax)
                              pInWindowAtRepOffset = pInWindowMax;

                           while ((pInWindowAtRepOffset + 8) < pInWindowMax && !memcmp(pInWindowAtRepOffset, pInWindowAtRepOffset - nMatchOffset, 8))
                              pInWindowAtRepOffset += 8;
                           while ((pInWindowAtRepOffset + 4) < pInWindowMax && !memcmp(pInWindowAtRepOffset, pInWindowAtRepOffset - nMatchOffset, 4))
                              pInWindowAtRepOffset += 4;
                           while (pInWindowAtRepOffset < pInWindowMax && pInWindowAtRepOffset[0] == pInWindowAtRepOffset[-nMatchOffset])
                              pInWindowAtRepOffset++;

                           const unsigned int nCurRepLen = (const unsigned int)(pInWindowAtRepOffset - pInWindowStart);

                           unsigned short* fwd_depth = pCompressor->match_depth + ((nRepPos - nStartOffset) << MATCHES_PER_INDEX_SHIFT);
                           int r;

                           for (r = 0; fwd_match[r].length; r++) {
                              if (fwd_match[r].offset == nMatchOffset && (fwd_depth[r] & 0x3fff) == 0) {
                                 if (fwd_match[r].length < nCurRepLen) {
                                    fwd_match[r].length = nCurRepLen;
                                    fwd_depth[r] = 0;
                                 }
                                 break;
                              }
                           }

                           if (fwd_match[r].length == 0) {
                              fwd_match[r].length = nCurRepLen;
                              fwd_match[r].offset = nMatchOffset;
                              fwd_depth[r] = 0;

                              if (nDepth < 9)
                                 apultra_insert_forward_match(pCompressor, pInWindow, nRepPos, nMatchOffset, nStartOffset, nEndOffset, nArrivalsPerPosition, nDepth + 1);
                           }
                        }
                     }
                  }
               }
            }
         }
      }
   }
}

/**
 * Attempt to pick optimal matches, so as to produce the smallest possible output that decompresses to the same input
 *
 * @param pCompressor compression context
 * @param pInWindow pointer to input data window (previously compressed bytes + bytes to compress)
 * @param nStartOffset current offset in input window (typically the number of previously compressed bytes)
 * @param nEndOffset offset to end finding matches at (typically the size of the total input window in bytes
 * @param nInsertForwardReps non-zero to insert forward repmatch candidates, zero to use the previously inserted candidates
 * @param nCurRepMatchOffset starting rep offset for this block
 * @param nBlockFlags bit 0: 1 for first block, 0 otherwise; bit 1: 1 for last block, 0 otherwise
 * @param nArrivalsPerPosition maximum number of arrivals per input buffer position
 */
static void apultra_optimize_forward(apultra_compressor *pCompressor, const unsigned char *pInWindow, const int nStartOffset, const int nEndOffset, const int nInsertForwardReps, const int *nCurRepMatchOffset, const int nBlockFlags, const int nArrivalsPerPosition) {
   apultra_arrival *arrival = pCompressor->arrival - (nStartOffset * nArrivalsPerPosition);
   const int* rle_len = (const int*)pCompressor->intervals /* reuse */;
   apultra_arrival* cur_arrival;
   int i;

   if ((nEndOffset - nStartOffset) > pCompressor->block_size) return;

   for (i = (nStartOffset * nArrivalsPerPosition); i != ((nEndOffset+1) * nArrivalsPerPosition); i += nArrivalsPerPosition) {
      int j;

      memset(arrival + i, 0, sizeof(apultra_arrival) * nArrivalsPerPosition);

      for (j = 0; j < nArrivalsPerPosition; j++)
         arrival[i + j].cost = 0x40000000;
   }

   arrival[nStartOffset * nArrivalsPerPosition].cost = 0;
   arrival[nStartOffset * nArrivalsPerPosition].from_slot = -1;
   arrival[nStartOffset * nArrivalsPerPosition].rep_offset = *nCurRepMatchOffset;

   if (nInsertForwardReps) {
      apultra_visited* visited = ((apultra_visited*)pCompressor->pos_data) - nStartOffset /* reuse */;

      memset(visited + nStartOffset, 0, (nEndOffset - nStartOffset) * sizeof(apultra_visited));
   }

   for (i = nStartOffset, cur_arrival = &arrival[nStartOffset * nArrivalsPerPosition]; i != nEndOffset; i++, cur_arrival += nArrivalsPerPosition) {
      int j, m;
      
      const unsigned char nMatch1Offs = pCompressor->match1[i - nStartOffset];
      int nShortOffset;
      int nShortLen;
      int nLiteralScore;
      int nLiteralCost;

      if ((pInWindow[i] != 0 && nMatch1Offs == 0) || (i == nStartOffset && (nBlockFlags & 1))) {
         nShortOffset = 0;
         nShortLen = 0;
         nLiteralScore = 1;
         nLiteralCost = 9 /* literal bit + literal byte */;
      }
      else {
         nShortOffset = (pInWindow[i] != 0) ? nMatch1Offs : 0;
         nShortLen = 1;
         nLiteralScore = nShortOffset ? 3 : 1;
         nLiteralCost = 4 + TOKEN_SIZE_4BIT_MATCH /* command and offset cost; no length cost */;
      }

      if (cur_arrival[nArrivalsPerPosition].from_slot) {
         apultra_arrival* pDestSlots = &cur_arrival[nArrivalsPerPosition];

         for (j = 0; j < nArrivalsPerPosition && cur_arrival[j].from_slot; j++) {
            const int nCodingChoiceCost = cur_arrival[j].cost + nLiteralCost;
            const int nScore = cur_arrival[j].score + nLiteralScore;
            const int nRepOffset = cur_arrival[j].rep_offset;

            if (nCodingChoiceCost < pDestSlots[nArrivalsPerPosition - 1].cost ||
               (nCodingChoiceCost == pDestSlots[nArrivalsPerPosition - 1].cost && nScore < pDestSlots[nArrivalsPerPosition - 1].score &&
                  nRepOffset != pDestSlots[nArrivalsPerPosition - 1].rep_offset)) {
               int exists = 0, n;

               for (n = 0;
                  pDestSlots[n].cost < nCodingChoiceCost;
                  n++) {
                  if (pDestSlots[n].rep_offset == nRepOffset) {
                     exists = 1;
                     break;
                  }
               }

               if (!exists) {
                  for (;
                     pDestSlots[n].cost == nCodingChoiceCost && nScore >= pDestSlots[n].score;
                     n++) {
                     if (pDestSlots[n].rep_offset == nRepOffset) {
                        exists = 1;
                        break;
                     }
                  }

                  if (!exists) {
                     int z;

                     for (z = n;
                        z < nArrivalsPerPosition - 1 && pDestSlots[z].cost == nCodingChoiceCost;
                        z++) {
                        if (pDestSlots[z].rep_offset == nRepOffset) {
                           exists = 1;
                           break;
                        }
                     }

                     if (!exists) {
                        for (; z < nArrivalsPerPosition - 1 && pDestSlots[z].from_slot; z++) {
                           if (pDestSlots[z].rep_offset == nRepOffset)
                              break;
                        }

                        memmove(&pDestSlots[n + 1],
                           &pDestSlots[n],
                           sizeof(apultra_arrival) * (z - n));

                        apultra_arrival* pDestArrival = &pDestSlots[n];
                        pDestArrival->cost = nCodingChoiceCost;
                        pDestArrival->from_pos = i;
                        pDestArrival->from_slot = j + 1;
                        pDestArrival->follows_literal = 1;
                        pDestArrival->rep_offset = nRepOffset;
                        pDestArrival->short_offset = nShortOffset;
                        pDestArrival->rep_pos = cur_arrival[j].rep_pos;
                        pDestArrival->match_len = nShortLen;
                        pDestArrival->score = nScore;
                     }
                  }
               }
            }
         }
      }
      else {
         apultra_arrival *pDestArrival = &cur_arrival[nArrivalsPerPosition];

         for (j = 0; j < nArrivalsPerPosition && cur_arrival[j].from_slot; j++, pDestArrival++) {
            pDestArrival->cost = cur_arrival[j].cost + nLiteralCost;
            pDestArrival->from_pos = i;
            pDestArrival->from_slot = j + 1;
            pDestArrival->follows_literal = 1;
            pDestArrival->rep_offset = cur_arrival[j].rep_offset;
            pDestArrival->short_offset = nShortOffset;
            pDestArrival->rep_pos = cur_arrival[j].rep_pos;
            pDestArrival->match_len = nShortLen;
            pDestArrival->score = cur_arrival[j].score + nLiteralScore;
         }
      }

      if (i == nStartOffset && (nBlockFlags & 1)) continue;

      const apultra_match *match = pCompressor->match + ((i - nStartOffset) << MATCHES_PER_INDEX_SHIFT);
      const unsigned short *match_depth = pCompressor->match_depth + ((i - nStartOffset) << MATCHES_PER_INDEX_SHIFT);
      const int nNumArrivalsForThisPos = j;
      int nOverallMinRepLen = 0, nOverallMaxRepLen = 0;

      int nRepMatchArrivalIdx[(2 * NARRIVALS_PER_POSITION_MAX) + 1];
      int nNumRepMatchArrivals = 0;

      if ((i + 2) <= nEndOffset) {
         int nMaxRepLenForPos = nEndOffset - i;
         if (nMaxRepLenForPos > LCP_MAX)
            nMaxRepLenForPos = LCP_MAX;
         const unsigned char* pInWindowStart = pInWindow + i;
         const unsigned char* pInWindowMax = pInWindowStart + nMaxRepLenForPos;

         for (j = 0; j < nNumArrivalsForThisPos; j++) {
            if (cur_arrival[j].follows_literal) {
               const int nRepOffset = cur_arrival[j].rep_offset;

               if (i >= nRepOffset) {
                  if (!memcmp(pInWindowStart, pInWindowStart - nRepOffset, 2)) {
                     if (nRepOffset) {
                        const int nLen0 = rle_len[i - nRepOffset];
                        const int nLen1 = rle_len[i];
                        const int nMinLen = (nLen0 < nLen1) ? nLen0 : nLen1;
                        const unsigned char* pInWindowAtRepOffset = pInWindowStart + nMinLen;

                        if (pInWindowAtRepOffset > pInWindowMax)
                           pInWindowAtRepOffset = pInWindowMax;

                        while ((pInWindowAtRepOffset + 8) < pInWindowMax && !memcmp(pInWindowAtRepOffset, pInWindowAtRepOffset - nRepOffset, 8))
                           pInWindowAtRepOffset += 8;
                        while ((pInWindowAtRepOffset + 4) < pInWindowMax && !memcmp(pInWindowAtRepOffset, pInWindowAtRepOffset - nRepOffset, 4))
                           pInWindowAtRepOffset += 4;
                        while (pInWindowAtRepOffset < pInWindowMax && pInWindowAtRepOffset[0] == pInWindowAtRepOffset[-nRepOffset])
                           pInWindowAtRepOffset++;

                        const int nCurMaxLen = (const int)(pInWindowAtRepOffset - pInWindowStart);

                        nRepMatchArrivalIdx[nNumRepMatchArrivals++] = j;
                        nRepMatchArrivalIdx[nNumRepMatchArrivals++] = nCurMaxLen;

                        if (nOverallMaxRepLen < nCurMaxLen)
                           nOverallMaxRepLen = nCurMaxLen;
                     }
                  }
               }
            }
         }
      }
      nRepMatchArrivalIdx[nNumRepMatchArrivals] = -1;

      for (m = 0; m < NMATCHES_PER_INDEX && match[m].length; m++) {
         int nOrigMatchLen = match[m].length;
         const int nOrigMatchOffset = match[m].offset;
         const unsigned int nOrigMatchDepth = match_depth[m] & 0x3fff;
         const int nScorePenalty = 3 + (match_depth[m] >> 15);
         unsigned int d;

         if ((i + nOrigMatchLen) > nEndOffset)
            nOrigMatchLen = nEndOffset - i;

         for (d = 0; d <= nOrigMatchDepth; d += (nOrigMatchDepth ? nOrigMatchDepth : 1)) {
            const int nMatchLen = nOrigMatchLen - d;
            const int nMatchOffset = nOrigMatchOffset - d;

            if (nInsertForwardReps) {
               apultra_insert_forward_match(pCompressor, pInWindow, i, nMatchOffset, nStartOffset, nEndOffset, nArrivalsPerPosition, 0);
            }

            if (nMatchLen >= 2) {
               int nStartingMatchLen, nJumpMatchLen, k;
               int nNoRepMatchOffsetCostForLit[2], nNoRepMatchOffsetCostDelta;
               int nMinMatchLenForOffset;
               const int nNoRepCostAdjusment = (nMatchLen >= LCP_MAX) ? 1 : 0;

               if (nMatchOffset < MINMATCH3_OFFSET)
                  nMinMatchLenForOffset = 2;
               else {
                  if (nMatchOffset < MINMATCH4_OFFSET)
                     nMinMatchLenForOffset = 3;
                  else
                     nMinMatchLenForOffset = 4;
               }

               if (nMatchLen >= LEAVE_ALONE_MATCH_SIZE && i >= nMatchLen)
                  nStartingMatchLen = nMatchLen;
               else
                  nStartingMatchLen = 2;

               if ((nBlockFlags & 3) == 3 && nMatchLen > 90 && i >= 90)
                  nJumpMatchLen = 90;
               else
                  nJumpMatchLen = nMatchLen + 1;

               if (nStartingMatchLen <= 3 && nMatchOffset < 128) {
                  nNoRepMatchOffsetCostForLit[1] = nNoRepMatchOffsetCostForLit[0] = 8 + TOKEN_SIZE_7BIT_MATCH;
               }
               else {
                  nNoRepMatchOffsetCostForLit[0] = 8 + TOKEN_SIZE_LARGE_MATCH + apultra_get_gamma2_size((nMatchOffset >> 8) + 2);
                  nNoRepMatchOffsetCostForLit[1] = 8 + TOKEN_SIZE_LARGE_MATCH + apultra_get_gamma2_size((nMatchOffset >> 8) + 3);
               }
               nNoRepMatchOffsetCostDelta = nNoRepMatchOffsetCostForLit[1] - nNoRepMatchOffsetCostForLit[0];

               for (k = nStartingMatchLen; k <= nMatchLen; k++) {
                  const int nRepMatchMatchLenCost = apultra_get_gamma2_size(k);
                  apultra_arrival *pDestSlots = &cur_arrival[k * nArrivalsPerPosition];
 
                  /* Insert non-repmatch candidate */

                  if (k >= nMinMatchLenForOffset) {
                     int nNoRepMatchMatchLenCost;

                     if (k <= 3 && nMatchOffset < 128)
                        nNoRepMatchMatchLenCost = 0;
                     else {
                        if (nMatchOffset < 128 || nMatchOffset >= MINMATCH4_OFFSET)
                           nNoRepMatchMatchLenCost = apultra_get_gamma2_size(k - 2);
                        else if (nMatchOffset < MINMATCH3_OFFSET)
                           nNoRepMatchMatchLenCost = nRepMatchMatchLenCost;
                        else
                           nNoRepMatchMatchLenCost = apultra_get_gamma2_size(k - 1);
                     }

                     for (j = 0; j < nNumArrivalsForThisPos; j++) {
                        const int nFollowsLiteral = cur_arrival[j].follows_literal;
                        if (nMatchOffset != cur_arrival[j].rep_offset || nFollowsLiteral == 0) {
                           const int nMatchCmdCost = nNoRepMatchMatchLenCost + nNoRepMatchOffsetCostForLit[nFollowsLiteral];
                           const int nCodingChoiceCost = cur_arrival[j].cost + nMatchCmdCost;

                           if (nCodingChoiceCost <= (pDestSlots[nArrivalsPerPosition - 1].cost + 1)) {
                              const int nScore = cur_arrival[j].score + nScorePenalty;

                              if (nCodingChoiceCost < pDestSlots[nArrivalsPerPosition - 2].cost ||
                                 (nCodingChoiceCost == pDestSlots[nArrivalsPerPosition - 2].cost && nScore < pDestSlots[nArrivalsPerPosition - 2].score &&
                                    (nCodingChoiceCost != pDestSlots[nArrivalsPerPosition - 1].cost || nMatchOffset != pDestSlots[nArrivalsPerPosition - 1].rep_offset))) {
                                 int exists = 0, n;

                                 for (n = 0;
                                    pDestSlots[n].cost < nCodingChoiceCost;
                                    n++) {
                                    if (pDestSlots[n].rep_offset == nMatchOffset) {
                                       exists = 1;
                                       break;
                                    }
                                 }

                                 if (!exists) {
                                    const int nRevisedCodingChoiceCost = nCodingChoiceCost - nNoRepCostAdjusment;

                                    for (;
                                       n < nArrivalsPerPosition - 1 && pDestSlots[n].cost == nRevisedCodingChoiceCost && nScore >= pDestSlots[n].score;
                                       n++) {
                                       if (pDestSlots[n].rep_offset == nMatchOffset) {
                                          exists = 1;
                                          break;
                                       }
                                    }

                                    if (!exists) {
                                       if (n < nArrivalsPerPosition - 1) {
                                          int z;

                                          for (z = n;
                                             z < nArrivalsPerPosition - 1 && pDestSlots[z].cost == nCodingChoiceCost;
                                             z++) {
                                             if (pDestSlots[z].rep_offset == nMatchOffset) {
                                                exists = 1;
                                                break;
                                             }
                                          }

                                          if (!exists) {
                                             for (; z < nArrivalsPerPosition - 1 && pDestSlots[z].from_slot; z++) {
                                                if (pDestSlots[z].rep_offset == nMatchOffset)
                                                   break;
                                             }

                                             memmove(&pDestSlots[n + 1],
                                                &pDestSlots[n],
                                                sizeof(apultra_arrival) * (z - n));

                                             apultra_arrival* pDestArrival = &pDestSlots[n];
                                             pDestArrival->cost = nRevisedCodingChoiceCost;
                                             pDestArrival->from_pos = i;
                                             pDestArrival->from_slot = j + 1;
                                             pDestArrival->follows_literal = 0;
                                             pDestArrival->rep_offset = nMatchOffset;
                                             pDestArrival->short_offset = 0;
                                             pDestArrival->rep_pos = i;
                                             pDestArrival->match_len = k;
                                             pDestArrival->score = nScore;
                                          }
                                       }
                                    }
                                 }
                                 else {
                                    if ((nCodingChoiceCost - pDestSlots[n].cost) >= nNoRepMatchOffsetCostDelta)
                                       break;
                                 }
                              }
                              if (cur_arrival[j].follows_literal == 0 || nNoRepMatchOffsetCostDelta == 0)
                                 break;
                           }
                           else {
                              break;
                           }
                        }
                     }
                  }

                  if (k == 3 && nMatchOffset < 128) {
                     nNoRepMatchOffsetCostForLit[1] = nNoRepMatchOffsetCostForLit[0] = 8 + TOKEN_SIZE_LARGE_MATCH + 2;
                  }

                  /* Insert repmatch candidate */

                  if (k > nOverallMinRepLen && k <= nOverallMaxRepLen) {
                     const int nRepMatchCmdCost = TOKEN_SIZE_LARGE_MATCH + 2 /* apultra_get_gamma2_size(2) */ + nRepMatchMatchLenCost;
                     int nCurRepMatchArrival;

                     if (k <= 90)
                        nOverallMinRepLen = k;
                     else if (nOverallMaxRepLen == k)
                        nOverallMaxRepLen--;
                     
                     for (nCurRepMatchArrival = 0; (j = nRepMatchArrivalIdx[nCurRepMatchArrival]) >= 0; nCurRepMatchArrival += 2) {
                        if (nRepMatchArrivalIdx[nCurRepMatchArrival + 1] >= k) {
                           const int nRepCodingChoiceCost = cur_arrival[j].cost + nRepMatchCmdCost;
                           const int nScore = cur_arrival[j].score + 2;
                           const int nRepOffset = cur_arrival[j].rep_offset;

                           if (nRepCodingChoiceCost < pDestSlots[nArrivalsPerPosition - 1].cost ||
                              (nRepCodingChoiceCost == pDestSlots[nArrivalsPerPosition - 1].cost && nScore < pDestSlots[nArrivalsPerPosition - 1].score &&
                                 nRepOffset != pDestSlots[nArrivalsPerPosition - 1].rep_offset)) {
                              int exists = 0, n;

                              for (n = 0;
                                 pDestSlots[n].cost < nRepCodingChoiceCost;
                                 n++) {
                                 if (pDestSlots[n].rep_offset == nRepOffset) {
                                    exists = 1;
                                    break;
                                 }
                              }

                              if (!exists) {
                                 for (;
                                    pDestSlots[n].cost == nRepCodingChoiceCost && nScore >= pDestSlots[n].score;
                                    n++) {
                                    if (pDestSlots[n].rep_offset == nRepOffset) {
                                       exists = 1;
                                       break;
                                    }
                                 }

                                 if (!exists) {
                                    int z;

                                    for (z = n;
                                       z < nArrivalsPerPosition - 1 && pDestSlots[z].cost == nRepCodingChoiceCost;
                                       z++) {
                                       if (pDestSlots[z].rep_offset == nRepOffset) {
                                          exists = 1;
                                          break;
                                       }
                                    }

                                    if (!exists) {
                                       for (; z < nArrivalsPerPosition - 1 && pDestSlots[z].from_slot; z++) {
                                          if (pDestSlots[z].rep_offset == nRepOffset)
                                             break;
                                       }

                                       memmove(&pDestSlots[n + 1],
                                          &pDestSlots[n],
                                          sizeof(apultra_arrival) * (z - n));

                                       apultra_arrival* pDestArrival = &pDestSlots[n];
                                       pDestArrival->cost = nRepCodingChoiceCost;
                                       pDestArrival->from_pos = i;
                                       pDestArrival->from_slot = j + 1;
                                       pDestArrival->follows_literal = 0;
                                       pDestArrival->rep_offset = nRepOffset;
                                       pDestArrival->short_offset = 0;
                                       pDestArrival->rep_pos = i;
                                       pDestArrival->match_len = k;
                                       pDestArrival->score = nScore;
                                    }
                                 }
                              }
                           }
                           else {
                              break;
                           }
                        }
                     }
                  }

                  if (k == nJumpMatchLen)
                     k = nMatchLen - 1;
               }
            }

            if (nOrigMatchLen >= 512)
               break;
         }
      }
   }
   
   if (!nInsertForwardReps) {
      const apultra_arrival* end_arrival = &arrival[i * nArrivalsPerPosition];
      apultra_final_match* pBestMatch = pCompressor->best_match - nStartOffset;

      while (end_arrival->from_slot > 0 && end_arrival->from_pos < (const unsigned int)nEndOffset) {
         pBestMatch[end_arrival->from_pos].length = end_arrival->match_len;
         pBestMatch[end_arrival->from_pos].offset = (end_arrival->match_len >= 2) ? end_arrival->rep_offset : end_arrival->short_offset;

         end_arrival = &arrival[(end_arrival->from_pos * nArrivalsPerPosition) + (end_arrival->from_slot - 1)];
      }
   }
}

/**
 * Attempt to replace matches by literals when it makes the final bitstream smaller, and merge large matches
 *
 * @param pCompressor compression context
 * @param pInWindow pointer to input data window (previously compressed bytes + bytes to compress)
 * @param pBestMatch optimal matches to evaluate and update
 * @param nStartOffset current offset in input window (typically the number of previously compressed bytes)
 * @param nEndOffset offset to end finding matches at (typically the size of the total input window in bytes
 * @param nCurRepMatchOffset starting rep offset for this block
 * @param nBlockFlags bit 0: 1 for first block, 0 otherwise; bit 1: 1 for last block, 0 otherwise
 *
 * @return non-zero if the number of tokens was reduced, 0 if it wasn't
 */
static int apultra_reduce_commands(apultra_compressor *pCompressor, const unsigned char *pInWindow, apultra_final_match *pBestMatch, const int nStartOffset, const int nEndOffset, const int *nCurRepMatchOffset, const int nBlockFlags) {
   int i;
   int nRepMatchOffset = *nCurRepMatchOffset;
   int nFollowsLiteral = 0;
   int nDidReduce = 0;
   int nLastMatchLen = 0;
   const unsigned char *match1 = pCompressor->match1 - nStartOffset;

   for (i = nStartOffset + (nBlockFlags & 1); i < nEndOffset; ) {
      apultra_final_match *pMatch = pBestMatch + i;

      if (pMatch->length <= 1 &&
         (i + 1) < nEndOffset &&
         pBestMatch[i + 1].length >= 2 &&
         pBestMatch[i + 1].length < MAX_VARLEN &&
         pBestMatch[i + 1].offset &&
         i >= pBestMatch[i + 1].offset &&
         (i + pBestMatch[i + 1].length + 1) <= nEndOffset &&
         !memcmp(pInWindow + i - (pBestMatch[i + 1].offset), pInWindow + i, pBestMatch[i + 1].length + 1)) {
         if (pBestMatch[i + 1].offset < MINMATCH4_OFFSET || (pBestMatch[i + 1].length + 1) >= 4 || (pBestMatch[i + 1].offset == nRepMatchOffset && nFollowsLiteral)) {

            int nCurPartialCommandSize = (pMatch->length == 1) ? (TOKEN_SIZE_4BIT_MATCH + 4) : (1 /* literal bit */ + 8 /* literal size */);
            if (pBestMatch[i + 1].offset == nRepMatchOffset /* always follows a literal, the one at the current position */) {
               nCurPartialCommandSize += TOKEN_SIZE_LARGE_MATCH + 2 /* apultra_get_gamma2_size(2) */ + apultra_get_gamma2_size(pBestMatch[i + 1].length);
            }
            else {
               nCurPartialCommandSize += apultra_get_offset_varlen_size(pBestMatch[i + 1].length, pBestMatch[i + 1].offset, 1) + apultra_get_match_varlen_size(pBestMatch[i + 1].length, pBestMatch[i + 1].offset);
            }

            int nReducedPartialCommandSize;
            if (pBestMatch[i + 1].offset == nRepMatchOffset && nFollowsLiteral) {
               nReducedPartialCommandSize = TOKEN_SIZE_LARGE_MATCH + 2 /* apultra_get_gamma2_size(2) */ + apultra_get_gamma2_size(pBestMatch[i + 1].length + 1);
            }
            else {
               nReducedPartialCommandSize = apultra_get_offset_varlen_size(pBestMatch[i + 1].length + 1, pBestMatch[i + 1].offset, nFollowsLiteral) + apultra_get_match_varlen_size(pBestMatch[i + 1].length + 1, pBestMatch[i + 1].offset);
            }

            if (nReducedPartialCommandSize < nCurPartialCommandSize || (nFollowsLiteral == 0 && nLastMatchLen >= LCP_MAX)) {
               /* Merge */
               pBestMatch[i].length = pBestMatch[i + 1].length + 1;
               pBestMatch[i].offset = pBestMatch[i + 1].offset;
               pBestMatch[i + 1].length = 0;
               pBestMatch[i + 1].offset = 0;
               nDidReduce = 1;
               continue;
            }
         }
      }

      if (pMatch->length >= 2) {
         if (pMatch->length < LCP_MAX /* Don't waste time considering large matches, they will always win over literals */) {
            int nNextIndex = i + pMatch->length;
            int nNextFollowsLiteral = 0;

            while (nNextIndex < nEndOffset && pBestMatch[nNextIndex].length < 2) {
               nNextIndex++;
               nNextFollowsLiteral = 1;
            }

            if (nNextIndex < nEndOffset && pBestMatch[nNextIndex].length >= 2) {
               int nCannotEncode = 0;

               if (nRepMatchOffset && nRepMatchOffset != pMatch->offset && pBestMatch[nNextIndex].offset && pMatch->offset != pBestMatch[nNextIndex].offset &&
                  nNextFollowsLiteral) {
                  /* Try to gain a match forward */
                  if (i >= pBestMatch[nNextIndex].offset && (i + pMatch->length) <= nEndOffset) {
                     if ((pBestMatch[nNextIndex].offset < MINMATCH3_OFFSET || pMatch->length >= 3) &&
                        (pBestMatch[nNextIndex].offset < MINMATCH4_OFFSET || pMatch->length >= 4)) {
                        int nMaxLen = 0;
                        const unsigned char* pInWindowAtPos = pInWindow + i;
                        while ((nMaxLen + 8) < pMatch->length && !memcmp(pInWindowAtPos + nMaxLen - pBestMatch[nNextIndex].offset, pInWindowAtPos + nMaxLen, 8))
                           nMaxLen += 8;
                        while ((nMaxLen + 4) < pMatch->length && !memcmp(pInWindowAtPos + nMaxLen - pBestMatch[nNextIndex].offset, pInWindowAtPos + nMaxLen, 4))
                           nMaxLen += 4;
                        while (nMaxLen < pMatch->length && pInWindowAtPos[nMaxLen - pBestMatch[nNextIndex].offset] == pInWindowAtPos[nMaxLen])
                           nMaxLen++;

                        if (nMaxLen >= pMatch->length) {
                           /* Replace */
                           pMatch->offset = pBestMatch[nNextIndex].offset;
                           nDidReduce = 1;
                        }
                        else if (nMaxLen >= 2) {
                           if ((nFollowsLiteral && nRepMatchOffset == pBestMatch[nNextIndex].offset) ||
                              ((pBestMatch[nNextIndex].offset < MINMATCH3_OFFSET || nMaxLen >= 3) &&
                               (pBestMatch[nNextIndex].offset < MINMATCH4_OFFSET || nMaxLen >= 4))) {

                              int nPartialSizeBefore, nPartialSizeAfter, j;

                              nPartialSizeBefore = apultra_get_offset_varlen_size(pMatch->length, pMatch->offset, nFollowsLiteral);
                              nPartialSizeBefore += apultra_get_match_varlen_size(pMatch->length, pMatch->offset);

                              nPartialSizeBefore += apultra_get_offset_varlen_size(pBestMatch[nNextIndex].length, pBestMatch[nNextIndex].offset, 1);
                              nPartialSizeBefore += apultra_get_match_varlen_size(pBestMatch[nNextIndex].length, pBestMatch[nNextIndex].offset);

                              nPartialSizeAfter = apultra_get_offset_varlen_size(nMaxLen, pBestMatch[nNextIndex].offset, nFollowsLiteral);
                              if (nFollowsLiteral && nRepMatchOffset == pBestMatch[nNextIndex].offset)
                                 nPartialSizeAfter += apultra_get_gamma2_size(nMaxLen);
                              else
                                 nPartialSizeAfter += apultra_get_match_varlen_size(nMaxLen, pBestMatch[nNextIndex].offset);

                              nPartialSizeAfter += TOKEN_SIZE_LARGE_MATCH + 2 /* apultra_get_gamma2_size(2) */;
                              nPartialSizeAfter += apultra_get_gamma2_size(pBestMatch[nNextIndex].length);

                              for (j = nMaxLen; j < pMatch->length; j++) {
                                 if (pInWindow[i + j] == 0 || match1[i + j])
                                    nPartialSizeAfter += TOKEN_SIZE_4BIT_MATCH + 4;
                                 else
                                    nPartialSizeAfter += 1 /* literal bit */ + 8 /* literal byte */;
                              }

                              if (nPartialSizeAfter < nPartialSizeBefore) {
                                 /* We gain a repmatch that is shorter than the original match as this is the best we can do, so it is followed by extra literals, but
                                  * we have calculated that this is shorter */

                                 const int nOrigLen = pMatch->length;

                                 pMatch->offset = pBestMatch[nNextIndex].offset;
                                 pMatch->length = nMaxLen;

                                 for (j = nMaxLen; j < nOrigLen; j++) {
                                    pBestMatch[i + j].offset = match1[i + j];
                                    pBestMatch[i + j].length = (pInWindow[i + j] && match1[i+j] == 0) ? 0 : 1;
                                 }

                                 nDidReduce = 1;
                                 continue;
                              }
                           }
                        }
                     }
                  }
               }

               /* Calculate this command's current cost */

               int nCurCommandSize;
               if (pMatch->offset == nRepMatchOffset && nFollowsLiteral) {
                  nCurCommandSize = TOKEN_SIZE_LARGE_MATCH + 2 /* apultra_get_gamma2_size(2) */ + apultra_get_gamma2_size(pMatch->length);
               }
               else {
                  nCurCommandSize = apultra_get_offset_varlen_size(pMatch->length, pMatch->offset, nFollowsLiteral) + apultra_get_match_varlen_size(pMatch->length, pMatch->offset);
               }

               /* Calculate the next command's current cost */
               int nNextCommandSize;
               if (pBestMatch[nNextIndex].offset == pMatch->offset && nNextFollowsLiteral && pBestMatch[nNextIndex].length >= 2) {
                  nNextCommandSize = TOKEN_SIZE_LARGE_MATCH + 2 /* apultra_get_gamma2_size(2) */ + apultra_get_gamma2_size(pBestMatch[nNextIndex].length);
               }
               else {
                  nNextCommandSize = apultra_get_offset_varlen_size(pBestMatch[nNextIndex].length, pBestMatch[nNextIndex].offset, nNextFollowsLiteral) + apultra_get_match_varlen_size(pBestMatch[nNextIndex].length, pBestMatch[nNextIndex].offset);
               }

               const int nOriginalCombinedCommandSize = nCurCommandSize + nNextCommandSize;

               /* Calculate the cost of replacing this match command by literals + the effect on the cost of the next command */
               int nReducedCommandSize = 0;
               int j;

               for (j = 0; j < pMatch->length; j++) {
                  if (pInWindow[i + j] == 0 || match1[i + j])
                     nReducedCommandSize += TOKEN_SIZE_4BIT_MATCH + 4;
                  else
                     nReducedCommandSize += 1 /* literal bit */ + 8;
               }

               if (pBestMatch[nNextIndex].offset == nRepMatchOffset /* the new command would always follow literals, the ones we create */ && pBestMatch[nNextIndex].length >= 2) {
                  nReducedCommandSize += TOKEN_SIZE_LARGE_MATCH + 2 /* apultra_get_gamma2_size(2) */ + apultra_get_gamma2_size(pBestMatch[nNextIndex].length);
               }
               else {
                  if ((pBestMatch[nNextIndex].length < 3 && pBestMatch[nNextIndex].offset >= MINMATCH3_OFFSET) ||
                     (pBestMatch[nNextIndex].length < 4 && pBestMatch[nNextIndex].offset >= MINMATCH4_OFFSET)) {
                     /* This match length can only be encoded with a rep-match */
                     nCannotEncode = 1;
                  }
                  else {
                     nReducedCommandSize += apultra_get_offset_varlen_size(pBestMatch[nNextIndex].length, pBestMatch[nNextIndex].offset, 1 /* follows literals */) + apultra_get_match_varlen_size(pBestMatch[nNextIndex].length, pBestMatch[nNextIndex].offset);
                  }
               }

               if (nOriginalCombinedCommandSize > nReducedCommandSize && !nCannotEncode) {
                  /* Reduce */
                  const int nMatchLen = pMatch->length;

                  for (j = 0; j < nMatchLen; j++) {
                     pBestMatch[i + j].offset = match1[i + j];
                     pBestMatch[i + j].length = (pInWindow[i + j] && match1[i + j] == 0) ? 0 : 1;
                  }

                  nDidReduce = 1;
                  continue;
               }
            }
         }

         if ((i + pMatch->length) < nEndOffset && pMatch->offset > 0 &&
            pBestMatch[i + pMatch->length].offset > 0 &&
            pBestMatch[i + pMatch->length].length >= 2 &&
            (pMatch->length + pBestMatch[i + pMatch->length].length) <= MAX_VARLEN &&
            (i + pMatch->length) >= pMatch->offset &&
            (i + pMatch->length) >= pBestMatch[i + pMatch->length].offset &&
            (i + pMatch->length + pBestMatch[i + pMatch->length].length) <= nEndOffset &&
            !memcmp(pInWindow + i + pMatch->length - pMatch->offset,
               pInWindow + i + pMatch->length - pBestMatch[i + pMatch->length].offset,
               pBestMatch[i + pMatch->length].length)) {
            /* Join large matches */

            const int nMatchLen = pMatch->length;
            int nNextIndex = i + nMatchLen + pBestMatch[i + nMatchLen].length;
            int nNextFollowsLiteral = 0;
            int nCannotEncode = 0;
            int nCurCommandSize, nReducedCommandSize;

            while (nNextIndex < nEndOffset && pBestMatch[nNextIndex].length < 2) {
               nNextIndex++;
               nNextFollowsLiteral = 1;
            }

            if (pMatch->offset == nRepMatchOffset && nFollowsLiteral) {
               nCurCommandSize = TOKEN_SIZE_LARGE_MATCH + 2 /* apultra_get_gamma2_size(2) */ + apultra_get_gamma2_size(nMatchLen);
            }
            else {
               nCurCommandSize = apultra_get_offset_varlen_size(nMatchLen, pMatch->offset, nFollowsLiteral) + apultra_get_match_varlen_size(nMatchLen, pMatch->offset);
            }

            nCurCommandSize += apultra_get_offset_varlen_size(pBestMatch[i + nMatchLen].length, pBestMatch[i + nMatchLen].offset, 0 /* follows literal */) + apultra_get_match_varlen_size(pBestMatch[i + nMatchLen].length, pBestMatch[i + nMatchLen].offset);

            if (nNextIndex < nEndOffset && pBestMatch[nNextIndex].length >= 2) {
               if (pBestMatch[nNextIndex].offset == pBestMatch[i + nMatchLen].offset && nNextFollowsLiteral) {
                  nCurCommandSize += TOKEN_SIZE_LARGE_MATCH + 2 /* apultra_get_gamma2_size(2) */ + apultra_get_gamma2_size(pBestMatch[nNextIndex].length);
               }
               else {
                  nCurCommandSize += apultra_get_offset_varlen_size(pBestMatch[nNextIndex].length, pBestMatch[nNextIndex].offset, nNextFollowsLiteral) + apultra_get_match_varlen_size(pBestMatch[nNextIndex].length, pBestMatch[nNextIndex].offset);
               }
            }

            if (pMatch->offset == nRepMatchOffset && nFollowsLiteral) {
               nReducedCommandSize = TOKEN_SIZE_LARGE_MATCH + 2 /* apultra_get_gamma2_size(2) */ + apultra_get_gamma2_size(nMatchLen + pBestMatch[i + nMatchLen].length);
            }
            else {
               nReducedCommandSize = apultra_get_offset_varlen_size(nMatchLen + pBestMatch[i + nMatchLen].length, pMatch->offset, nFollowsLiteral) + apultra_get_match_varlen_size(nMatchLen + pBestMatch[i + nMatchLen].length, pMatch->offset);
            }

            if (nNextIndex < nEndOffset && pBestMatch[nNextIndex].length >= 2) {
               if (pBestMatch[nNextIndex].offset == pMatch->offset && nNextFollowsLiteral) {
                  nReducedCommandSize += TOKEN_SIZE_LARGE_MATCH + 2 /* apultra_get_gamma2_size(2) */ + apultra_get_gamma2_size(pBestMatch[nNextIndex].length);
               }
               else {
                  nReducedCommandSize += apultra_get_offset_varlen_size(pBestMatch[nNextIndex].length, pBestMatch[nNextIndex].offset, nNextFollowsLiteral) + apultra_get_match_varlen_size(pBestMatch[nNextIndex].length, pBestMatch[nNextIndex].offset);

                  if ((pBestMatch[nNextIndex].offset >= MINMATCH3_OFFSET && pBestMatch[nNextIndex].length < 3) ||
                     (pBestMatch[nNextIndex].offset >= MINMATCH4_OFFSET && pBestMatch[nNextIndex].length < 4)) {
                     nCannotEncode = 1;
                  }
               }
            }

            if (nCurCommandSize >= nReducedCommandSize) {
               if (!nCannotEncode) {
                  pMatch->length += pBestMatch[i + nMatchLen].length;
                  pBestMatch[i + nMatchLen].length = 0;
                  pBestMatch[i + nMatchLen].offset = 0;
                  nDidReduce = 1;
                  continue;
               }
            }
         }

         nRepMatchOffset = pMatch->offset;
         nFollowsLiteral = 0;
         nLastMatchLen = pMatch->length;

         i += pMatch->length;
      }
      else {
         /* 4 bits offset (1 byte match) or literal */
         i++;
         nFollowsLiteral = 1;
         nLastMatchLen = 0;
      }
   }

   return nDidReduce;
}

/**
 * Emit a block of compressed data
 *
 * @param pCompressor compression context
 * @param pBestMatch optimal matches to emit
 * @param pInWindow pointer to input data window (previously compressed bytes + bytes to compress)
 * @param nStartOffset current offset in input window (typically the number of previously compressed bytes)
 * @param nEndOffset offset to end finding matches at (typically the size of the total input window in bytes
 * @param pOutData pointer to output buffer
 * @param nMaxOutDataSize maximum size of output buffer, in bytes
 * @param nCurBitsOffset write index into output buffer, of current byte being filled with bits
 * @param nCurBitShift bit shift count
 * @param nFollowsLiteral non-zero if the next command to be issued follows a literal, 0 if not
 * @param nCurRepMatchOffset starting rep offset for this block, updated after the block is compressed successfully
 * @param nBlockFlags bit 0: 1 for first block, 0 otherwise; bit 1: 1 for last block, 0 otherwise
 *
 * @return size of compressed data in output buffer, or -1 if the data is uncompressible
 */
static int apultra_write_block(apultra_compressor *pCompressor, const apultra_final_match *pBestMatch, const unsigned char *pInWindow, const int nStartOffset, const int nEndOffset, unsigned char *pOutData, const int nMaxOutDataSize, int *nCurBitsOffset, int *nCurBitShift, int *nFollowsLiteral, int *nCurRepMatchOffset, const int nBlockFlags) {
   int i;
   int nRepMatchOffset = *nCurRepMatchOffset;
   int nCurFollowsLiteral = *nFollowsLiteral;
   int nOutOffset = 0;
   const int nMaxOffset = pCompressor->max_offset;

   if (nBlockFlags & 1) {
      if (nMaxOutDataSize <= 0)
         return -1;
      pOutData[nOutOffset++] = pInWindow[nStartOffset];
      nCurFollowsLiteral = 1;
   }

   for (i = nStartOffset + (nBlockFlags & 1); i < nEndOffset; ) {
      const apultra_final_match *pMatch = pBestMatch + i;

      if (pMatch->length >= 2) {
         const int nMatchLen = pMatch->length;
         const int nMatchOffset = pMatch->offset;

         if (nMatchOffset < MIN_OFFSET || nMatchOffset > nMaxOffset)
            return -1;

         if (nMatchOffset == nRepMatchOffset && nCurFollowsLiteral) {
            /* Rep-match */
            nOutOffset = apultra_write_bits(pOutData, nOutOffset, nMaxOutDataSize, TOKEN_CODE_LARGE_MATCH, TOKEN_SIZE_LARGE_MATCH, nCurBitsOffset, nCurBitShift);
            nOutOffset = apultra_write_bits(pOutData, nOutOffset, nMaxOutDataSize, 0 /* length of 2 encoded as gamma 2 */, 2, nCurBitsOffset, nCurBitShift);

            /* The match length isn't encoded in the command, emit elias gamma value */
            nOutOffset = apultra_write_gamma2_value(pOutData, nOutOffset, nMaxOutDataSize, nMatchLen, nCurBitsOffset, nCurBitShift);
            if (nOutOffset < 0) return -1;

            nCurFollowsLiteral = 0;

            pCompressor->stats.num_rep_matches++;
         }
         else {
            if (nMatchLen <= 3 && nMatchOffset < 128) {
               /* 7 bits offset + 1 bit length */
               nOutOffset = apultra_write_bits(pOutData, nOutOffset, nMaxOutDataSize, TOKEN_CODE_7BIT_MATCH, TOKEN_SIZE_7BIT_MATCH, nCurBitsOffset, nCurBitShift);

               if (nOutOffset < 0 || nOutOffset >= nMaxOutDataSize)
                  return -1;
               pOutData[nOutOffset++] = ((nMatchOffset) & 0x7f) << 1 | (nMatchLen - 2);

               nCurFollowsLiteral = 0;
               nRepMatchOffset = nMatchOffset;

               pCompressor->stats.num_7bit_matches++;
            }
            else {
               /* 8+n bits offset */
               nOutOffset = apultra_write_bits(pOutData, nOutOffset, nMaxOutDataSize, TOKEN_CODE_LARGE_MATCH, TOKEN_SIZE_LARGE_MATCH, nCurBitsOffset, nCurBitShift);
               nOutOffset = apultra_write_gamma2_value(pOutData, nOutOffset, nMaxOutDataSize, (nMatchOffset >> 8) + 2 + (nCurFollowsLiteral & 1), nCurBitsOffset, nCurBitShift);

               const unsigned char nLowMatchBits = nMatchOffset & 0xff;
               if (nOutOffset < 0 || nOutOffset >= nMaxOutDataSize)
                  return -1;
               pOutData[nOutOffset++] = nLowMatchBits;

               /* The match length isn't encoded in the command, emit elias gamma value */

               if (nMatchOffset < 128 || nMatchOffset >= MINMATCH4_OFFSET)
                  nOutOffset = apultra_write_gamma2_value(pOutData, nOutOffset, nMaxOutDataSize, nMatchLen - 2, nCurBitsOffset, nCurBitShift);
               else if (nMatchOffset < MINMATCH3_OFFSET)
                  nOutOffset = apultra_write_gamma2_value(pOutData, nOutOffset, nMaxOutDataSize, nMatchLen, nCurBitsOffset, nCurBitShift);
               else
                  nOutOffset = apultra_write_gamma2_value(pOutData, nOutOffset, nMaxOutDataSize, nMatchLen - 1, nCurBitsOffset, nCurBitShift);
               if (nOutOffset < 0) return -1;

               nCurFollowsLiteral = 0;
               nRepMatchOffset = nMatchOffset;

               pCompressor->stats.num_variable_matches++;
            }
         }

         if (nMatchOffset < pCompressor->stats.min_offset || pCompressor->stats.min_offset == -1)
            pCompressor->stats.min_offset = nMatchOffset;
         if (nMatchOffset > pCompressor->stats.max_offset)
            pCompressor->stats.max_offset = nMatchOffset;
         pCompressor->stats.total_offsets += (long long)nMatchOffset;

         if (nMatchLen < pCompressor->stats.min_match_len || pCompressor->stats.min_match_len == -1)
            pCompressor->stats.min_match_len = nMatchLen;
         if (nMatchLen > pCompressor->stats.max_match_len)
            pCompressor->stats.max_match_len = nMatchLen;
         pCompressor->stats.total_match_lens += nMatchLen;
         pCompressor->stats.match_divisor++;

         if (nMatchOffset == 1) {
            if (nMatchLen < pCompressor->stats.min_rle1_len || pCompressor->stats.min_rle1_len == -1)
               pCompressor->stats.min_rle1_len = nMatchLen;
            if (nMatchLen > pCompressor->stats.max_rle1_len)
               pCompressor->stats.max_rle1_len = nMatchLen;
            pCompressor->stats.total_rle1_lens += nMatchLen;
            pCompressor->stats.rle1_divisor++;
         }
         else if (nMatchOffset == 2) {
            if (nMatchLen < pCompressor->stats.min_rle2_len || pCompressor->stats.min_rle2_len == -1)
               pCompressor->stats.min_rle2_len = nMatchLen;
            if (nMatchLen > pCompressor->stats.max_rle2_len)
               pCompressor->stats.max_rle2_len = nMatchLen;
            pCompressor->stats.total_rle2_lens += nMatchLen;
            pCompressor->stats.rle2_divisor++;
         }

         i += nMatchLen;

         pCompressor->stats.commands_divisor++;
      }
      else if (pMatch->length == 1) {
         const int nMatchOffset = pMatch->offset;

         /* 4 bits offset */

         if (nMatchOffset < 0 || nMatchOffset > 15)
            return -1;

         nOutOffset = apultra_write_bits(pOutData, nOutOffset, nMaxOutDataSize, TOKEN_CODE_4BIT_MATCH, TOKEN_SIZE_4BIT_MATCH, nCurBitsOffset, nCurBitShift);
         nOutOffset = apultra_write_bits(pOutData, nOutOffset, nMaxOutDataSize, nMatchOffset, 4, nCurBitsOffset, nCurBitShift);
         if (nOutOffset < 0) return -1;

         pCompressor->stats.num_4bit_matches++;
         pCompressor->stats.commands_divisor++;

         i++;
         nCurFollowsLiteral = 1;
      }
      else {
         /* Literal */

         nOutOffset = apultra_write_bits(pOutData, nOutOffset, nMaxOutDataSize, 0 /* literal */, 1, nCurBitsOffset, nCurBitShift);

         if (nOutOffset < 0 || nOutOffset >= nMaxOutDataSize)
            return -1;
         pOutData[nOutOffset++] = pInWindow[i];

         pCompressor->stats.num_literals++;
         pCompressor->stats.commands_divisor++;
         i++;
         nCurFollowsLiteral = 1;
      }

      const int nCurSafeDist = (i - nStartOffset) - nOutOffset;
      if (nCurSafeDist >= 0 && pCompressor->stats.safe_dist < nCurSafeDist)
         pCompressor->stats.safe_dist = nCurSafeDist;
   }

   if (nBlockFlags & 2) {
      /* 8 bits offset */

      nOutOffset = apultra_write_bits(pOutData, nOutOffset, nMaxOutDataSize, TOKEN_CODE_7BIT_MATCH, TOKEN_SIZE_7BIT_MATCH, nCurBitsOffset, nCurBitShift);

      if (nOutOffset < 0 || nOutOffset >= nMaxOutDataSize)
         return -1;
      pOutData[nOutOffset++] = 0x00;   /* Offset: EOD */
      pCompressor->stats.num_eod++;
      pCompressor->stats.commands_divisor++;

      const int nCurSafeDist = (i - nStartOffset) - nOutOffset;
      if (nCurSafeDist >= 0 && pCompressor->stats.safe_dist < nCurSafeDist)
         pCompressor->stats.safe_dist = nCurSafeDist;
   }

   *nCurRepMatchOffset = nRepMatchOffset;
   *nFollowsLiteral = nCurFollowsLiteral;

   return nOutOffset;
}

/**
 * Select the most optimal matches, reduce the token count if possible, and then emit a block of compressed data
 *
 * @param pCompressor compression context
 * @param pInWindow pointer to input data window (previously compressed bytes + bytes to compress)
 * @param nPreviousBlockSize number of previously compressed bytes (or 0 for none)
 * @param nInDataSize number of input bytes to compress
 * @param pOutData pointer to output buffer
 * @param nMaxOutDataSize maximum size of output buffer, in bytes
 * @param nCurBitsOffset write index into output buffer, of current byte being filled with bits
 * @param nCurBitShift bit shift count
 * @param nCurFollowsLiteral non-zero if the next command to be issued follows a literal, 0 if not
 * @param nCurRepMatchOffset starting rep offset for this block, updated after the block is compressed successfully
 * @param nBlockFlags bit 0: 1 for first block, 0 otherwise; bit 1: 1 for last block, 0 otherwise
 *
 * @return size of compressed data in output buffer, or -1 if the data is uncompressible
 */
static int apultra_optimize_and_write_block(apultra_compressor *pCompressor, const unsigned char *pInWindow, const int nPreviousBlockSize, const int nInDataSize, unsigned char *pOutData, const int nMaxOutDataSize, int *nCurBitsOffset, int *nCurBitShift, int *nCurFollowsLiteral, int *nCurRepMatchOffset, const int nBlockFlags) {
   const int nEndOffset = nPreviousBlockSize + nInDataSize;
   const int nArrivalsPerPosition = pCompressor->max_arrivals;
   int *rle_len = (int*)pCompressor->intervals /* reuse */;
   int i, nPosition;

   memset(pCompressor->best_match, 0, pCompressor->block_size * sizeof(apultra_final_match));

   i = 0;
   while (i < nEndOffset) {
      int nRangeStartIdx = i;
      const unsigned char c = pInWindow[nRangeStartIdx];

      do {
         i++;
      } while (i < nEndOffset && pInWindow[i] == c);

      while (nRangeStartIdx < i) {
         rle_len[nRangeStartIdx] = i - nRangeStartIdx;
         nRangeStartIdx++;
      }
   }

   if ((nBlockFlags & 3) == 3) {
      int *first_offset_for_byte = pCompressor->first_offset_for_byte;
      int *next_offset_for_pos = pCompressor->next_offset_for_pos;

      /* Supplement 2 and 3-byte matches */

      memset(first_offset_for_byte, 0xff, sizeof(int) * 65536);
      memset(next_offset_for_pos, 0xff, sizeof(int) * nInDataSize);

      for (nPosition = nPreviousBlockSize; nPosition < (nEndOffset - 1); nPosition++) {
         next_offset_for_pos[nPosition - nPreviousBlockSize] = first_offset_for_byte[((unsigned int)pInWindow[nPosition]) | (((unsigned int)pInWindow[nPosition + 1]) << 8)];
         first_offset_for_byte[((unsigned int)pInWindow[nPosition]) | (((unsigned int)pInWindow[nPosition + 1]) << 8)] = nPosition;
      }

      for (nPosition = nPreviousBlockSize + 1; nPosition < (nEndOffset - 1); nPosition++) {
         apultra_match *match = pCompressor->match + ((nPosition - nPreviousBlockSize) << MATCHES_PER_INDEX_SHIFT);
         unsigned short *match_depth = pCompressor->match_depth + ((nPosition - nPreviousBlockSize) << MATCHES_PER_INDEX_SHIFT);
         int m = 0, nInserted = 0;
         int nMatchPos;

         while (m < 15 && match[m].length)
            m++;

         for (nMatchPos = next_offset_for_pos[nPosition - nPreviousBlockSize]; m < 15 && nMatchPos >= 0; nMatchPos = next_offset_for_pos[nMatchPos - nPreviousBlockSize]) {
            const int nMatchOffset = nPosition - nMatchPos;

            if (nMatchOffset <= pCompressor->max_offset) {
               int nExistingMatchIdx;
               int nAlreadyExists = 0;

               for (nExistingMatchIdx = 0; nExistingMatchIdx < m; nExistingMatchIdx++) {
                  if (match[nExistingMatchIdx].offset == nMatchOffset ||
                     (match[nExistingMatchIdx].offset - (match_depth[nExistingMatchIdx] & 0x3fff)) == nMatchOffset) {
                     nAlreadyExists = 1;
                     break;
                  }
               }

               if (!nAlreadyExists) {
                  match[m].length = (nPosition < (nEndOffset - 2) && pInWindow[nMatchPos + 2] == pInWindow[nPosition + 2]) ? 3 : 2;
                  match[m].offset = nMatchOffset;
                  match_depth[m] = 0x4000;
                  m++;
                  nInserted++;
                  if (nInserted >= 6)
                     break;
               }
            }
            else {
               break;
            }
         }
      }
   }

   apultra_optimize_forward(pCompressor, pInWindow, nPreviousBlockSize, nEndOffset, 1 /* nInsertForwardReps */, nCurRepMatchOffset, nBlockFlags, nArrivalsPerPosition);

   if ((nBlockFlags & 3) == 3 && nArrivalsPerPosition == NARRIVALS_PER_POSITION_MAX) {
      const int* next_offset_for_pos = pCompressor->next_offset_for_pos;
      int* offset_cache = pCompressor->offset_cache;

      /* Supplement matches further */

      memset(offset_cache, 0xff, sizeof(int) * 2048);

      for (nPosition = nPreviousBlockSize + 1; nPosition < (nEndOffset - 1); nPosition++) {
         apultra_match* match = pCompressor->match + ((nPosition - nPreviousBlockSize) << MATCHES_PER_INDEX_SHIFT);

         if (match[0].length < 8) {
            unsigned short* match_depth = pCompressor->match_depth + ((nPosition - nPreviousBlockSize) << MATCHES_PER_INDEX_SHIFT);
            int m = 0, nInserted = 0;
            int nMatchPos;
            int nMaxForwardPos = nPosition + 2 + 1 + 5;

            if (nMaxForwardPos > (nEndOffset - 2))
               nMaxForwardPos = nEndOffset - 2;

            while (m < 46 && match[m].length) {
               offset_cache[match[m].offset & 2047] = nPosition;
               offset_cache[(match[m].offset - (match_depth[m] & 0x3fff)) & 2047] = nPosition;
               m++;
            }

            for (nMatchPos = next_offset_for_pos[nPosition - nPreviousBlockSize]; m < 46 && nMatchPos >= 0; nMatchPos = next_offset_for_pos[nMatchPos - nPreviousBlockSize]) {
               const int nMatchOffset = nPosition - nMatchPos;

               if (nMatchOffset <= pCompressor->max_offset) {
                  int nAlreadyExists = 0;

                  if (offset_cache[nMatchOffset & 2047] == nPosition) {
                     int nExistingMatchIdx;

                     for (nExistingMatchIdx = 0; nExistingMatchIdx < m; nExistingMatchIdx++) {
                        if (match[nExistingMatchIdx].offset == nMatchOffset ||
                           (match[nExistingMatchIdx].offset - (match_depth[nExistingMatchIdx] & 0x3fff)) == nMatchOffset) {
                           nAlreadyExists = 1;

                           if (match_depth[nExistingMatchIdx] == 0x4000) {
                              int nMatchLen = 2;
                              while ((nMatchLen + 8) < 16 && (nPosition + nMatchLen + 8) < nEndOffset && !memcmp(pInWindow + nMatchPos + nMatchLen, pInWindow + nPosition + nMatchLen, 8))
                                 nMatchLen += 8;
                              while ((nMatchLen + 4) < 16 && (nPosition + nMatchLen + 4) < nEndOffset && !memcmp(pInWindow + nMatchPos + nMatchLen, pInWindow + nPosition + nMatchLen, 4))
                                 nMatchLen += 4;
                              while (nMatchLen < 16 && (nPosition + nMatchLen) < nEndOffset && pInWindow[nMatchPos + nMatchLen] == pInWindow[nPosition + nMatchLen])
                                 nMatchLen++;
                              if (nMatchLen > (const int)match[nExistingMatchIdx].length)
                                 match[nExistingMatchIdx].length = nMatchLen;
                           }

                           break;
                        }
                     }
                  }

                  if (!nAlreadyExists) {
                     int nForwardPos = nPosition + 2 + 1;

                     if (nForwardPos >= nMatchOffset) {
                        int nGotMatch = 0;

                        while (nForwardPos < nMaxForwardPos) {
                           if (!memcmp(pInWindow + nForwardPos, pInWindow + nForwardPos - nMatchOffset, 2)) {
                              nGotMatch = 1;
                              break;
                           }
                           nForwardPos++;
                        }

                        if (nGotMatch) {
                           int nMatchLen = 2;
                           while ((nMatchLen + 8) < 16 && (nPosition + nMatchLen + 8) < nEndOffset && !memcmp(pInWindow + nMatchPos + nMatchLen, pInWindow + nPosition + nMatchLen, 8))
                              nMatchLen += 8;
                           while ((nMatchLen + 4) < 16 && (nPosition + nMatchLen + 4) < nEndOffset && !memcmp(pInWindow + nMatchPos + nMatchLen, pInWindow + nPosition + nMatchLen, 4))
                              nMatchLen += 4;
                           while (nMatchLen < 16 && (nPosition + nMatchLen) < nEndOffset && pInWindow[nMatchPos + nMatchLen] == pInWindow[nPosition + nMatchLen])
                              nMatchLen++;
                           match[m].length = nMatchLen;
                           match[m].offset = nMatchOffset;
                           match_depth[m] = 0;
                           m++;

                           apultra_insert_forward_match(pCompressor, pInWindow, nPosition, nMatchOffset, nPreviousBlockSize, nEndOffset, nArrivalsPerPosition, 8);

                           nInserted++;
                           if (nInserted >= 18 || (nInserted >= 15 && m >= 38))
                              break;
                        }
                     }
                  }
               }
               else {
                  break;
               }
            }
         }
      }
   }

   /* Pick optimal matches */
   apultra_optimize_forward(pCompressor, pInWindow, nPreviousBlockSize, nEndOffset, 0 /* nInsertForwardReps */, nCurRepMatchOffset, nBlockFlags, nArrivalsPerPosition);

   /* Apply reduction and merge pass */
   int nDidReduce;
   int nPasses = 0;
   do {
      nDidReduce = apultra_reduce_commands(pCompressor, pInWindow, pCompressor->best_match - nPreviousBlockSize, nPreviousBlockSize, nEndOffset, nCurRepMatchOffset, nBlockFlags);
      nPasses++;
   } while (nDidReduce && nPasses < 20);

   /* Write compressed block */

   return apultra_write_block(pCompressor, pCompressor->best_match - nPreviousBlockSize, pInWindow, nPreviousBlockSize, nEndOffset, pOutData, nMaxOutDataSize, nCurBitsOffset, nCurBitShift, nCurFollowsLiteral, nCurRepMatchOffset, nBlockFlags);
}

/* Forward declaration */
static void apultra_compressor_destroy(apultra_compressor *pCompressor);

/**
 * Initialize compression context
 *
 * @param pCompressor compression context to initialize
 * @param nBlockSize maximum size of input data (bytes to compress only)
 * @param nMaxWindowSize maximum size of input data window (previously compressed bytes + bytes to compress)
 * @param nMaxArrivals maximum number of arrivals per position
 * @param nFlags compression flags
 *
 * @return 0 for success, non-zero for failure
 */
static int apultra_compressor_init(apultra_compressor *pCompressor, const int nBlockSize, const int nMaxWindowSize, const int nMaxArrivals, const int nFlags) {
   int nResult;

   nResult = divsufsort_init(&pCompressor->divsufsort_context);
   pCompressor->intervals = NULL;
   pCompressor->pos_data = NULL;
   pCompressor->open_intervals = NULL;
   pCompressor->match = NULL;
   pCompressor->match_depth = NULL;
   pCompressor->match1 = NULL;
   pCompressor->best_match = NULL;
   pCompressor->arrival = NULL;
   pCompressor->first_offset_for_byte = NULL;
   pCompressor->next_offset_for_pos = NULL;
   pCompressor->offset_cache = NULL;
   pCompressor->flags = nFlags;
   pCompressor->block_size = nBlockSize;
   pCompressor->max_arrivals = nMaxArrivals;

   memset(&pCompressor->stats, 0, sizeof(pCompressor->stats));
   pCompressor->stats.min_match_len = -1;
   pCompressor->stats.min_offset = -1;
   pCompressor->stats.min_rle1_len = -1;
   pCompressor->stats.min_rle2_len = -1;

   if (!nResult) {
      pCompressor->intervals = (unsigned long long *)malloc(nMaxWindowSize * sizeof(unsigned long long));

      if (pCompressor->intervals) {
         pCompressor->pos_data = (unsigned long long *)malloc(nMaxWindowSize * sizeof(unsigned long long));

         if (pCompressor->pos_data) {
            pCompressor->open_intervals = (unsigned long long *)malloc((LCP_AND_TAG_MAX + 1) * sizeof(unsigned long long));

            if (pCompressor->open_intervals) {
               pCompressor->arrival = (apultra_arrival *)malloc((nBlockSize + 1) * nMaxArrivals * sizeof(apultra_arrival));

               if (pCompressor->arrival) {
                  pCompressor->best_match = (apultra_final_match *)malloc(nBlockSize * sizeof(apultra_final_match));

                  if (pCompressor->best_match) {
                     pCompressor->match = (apultra_match *)malloc(nBlockSize * NMATCHES_PER_INDEX * sizeof(apultra_match));
                     if (pCompressor->match) {
                        pCompressor->match_depth = (unsigned short *)malloc(nBlockSize * NMATCHES_PER_INDEX * sizeof(unsigned short));
                        if (pCompressor->match_depth) {
                           pCompressor->match1 = (unsigned char *)malloc(nBlockSize * sizeof(unsigned char));
                           if (pCompressor->match1) {
                              pCompressor->first_offset_for_byte = (int*)malloc(65536 * sizeof(int));
                              if (pCompressor->first_offset_for_byte) {
                                 pCompressor->next_offset_for_pos = (int*)malloc(nBlockSize * sizeof(int));
                                 if (pCompressor->next_offset_for_pos) {
                                    if (nMaxArrivals == NARRIVALS_PER_POSITION_MAX) {
                                       pCompressor->offset_cache = (int*)malloc(2048 * sizeof(int));
                                       if (pCompressor->offset_cache) {
                                          return 0;
                                       }
                                    }
                                    else {
                                       return 0;
                                    }
                                 }
                              }
                           }
                        }
                     }
                  }
               }
            }
         }
      }
   }

   apultra_compressor_destroy(pCompressor);
   return 100;
}

/**
 * Clean up compression context and free up any associated resources
 *
 * @param pCompressor compression context to clean up
 */
static void apultra_compressor_destroy(apultra_compressor *pCompressor) {
   divsufsort_destroy(&pCompressor->divsufsort_context);

   if (pCompressor->offset_cache) {
      free(pCompressor->offset_cache);
      pCompressor->offset_cache = NULL;
   }

   if (pCompressor->next_offset_for_pos) {
      free(pCompressor->next_offset_for_pos);
      pCompressor->next_offset_for_pos = NULL;
   }

   if (pCompressor->first_offset_for_byte) {
      free(pCompressor->first_offset_for_byte);
      pCompressor->first_offset_for_byte = NULL;
   }

   if (pCompressor->match1) {
      free(pCompressor->match1);
      pCompressor->match1 = NULL;
   }

   if (pCompressor->match_depth) {
      free(pCompressor->match_depth);
      pCompressor->match_depth = NULL;
   }

   if (pCompressor->match) {
      free(pCompressor->match);
      pCompressor->match = NULL;
   }

   if (pCompressor->arrival) {
      free(pCompressor->arrival);
      pCompressor->arrival = NULL;
   }

   if (pCompressor->best_match) {
      free(pCompressor->best_match);
      pCompressor->best_match = NULL;
   }

   if (pCompressor->open_intervals) {
      free(pCompressor->open_intervals);
      pCompressor->open_intervals = NULL;
   }

   if (pCompressor->pos_data) {
      free(pCompressor->pos_data);
      pCompressor->pos_data = NULL;
   }

   if (pCompressor->intervals) {
      free(pCompressor->intervals);
      pCompressor->intervals = NULL;
   }
}

/**
 * Compress one block of data
 *
 * @param pCompressor compression context
 * @param pInWindow pointer to input data window (previously compressed bytes + bytes to compress)
 * @param nPreviousBlockSize number of previously compressed bytes (or 0 for none)
 * @param nInDataSize number of input bytes to compress
 * @param pOutData pointer to output buffer
 * @param nMaxOutDataSize maximum size of output buffer, in bytes
 * @param nCurBitsOffset write index into output buffer, of current byte being filled with bits
 * @param nCurBitShift bit shift count
 * @param nCurFollowsLiteral non-zero if the next command to be issued follows a literal, 0 if not
 * @param nCurRepMatchOffset starting rep offset for this block, updated after the block is compressed successfully
 * @param nBlockFlags bit 0: 1 for first block, 0 otherwise; bit 1: 1 for last block, 0 otherwise
 *
 * @return size of compressed data in output buffer, or -1 if the data is uncompressible
 */
static int apultra_compressor_shrink_block(apultra_compressor *pCompressor, const unsigned char *pInWindow, const int nPreviousBlockSize, const int nInDataSize, unsigned char *pOutData, const int nMaxOutDataSize, int *nCurBitsOffset, int *nCurBitShift, int *nCurFollowsLiteral, int *nCurRepMatchOffset, const int nBlockFlags) {
   int nCompressedSize;

   if (apultra_build_suffix_array(pCompressor, pInWindow, nPreviousBlockSize + nInDataSize))
      nCompressedSize = -1;
   else {
      if (nPreviousBlockSize) {
         apultra_skip_matches(pCompressor, 0, nPreviousBlockSize);
      }
      apultra_find_all_matches(pCompressor, NMATCHES_PER_INDEX, nPreviousBlockSize, nPreviousBlockSize + nInDataSize, nBlockFlags);

      nCompressedSize = apultra_optimize_and_write_block(pCompressor, pInWindow, nPreviousBlockSize, nInDataSize, pOutData, nMaxOutDataSize, nCurBitsOffset, nCurBitShift, nCurFollowsLiteral, nCurRepMatchOffset, nBlockFlags);
   }

   return nCompressedSize;
}

/**
 * Get maximum compressed size of input(source) data
 *
 * @param nInputSize input(source) size in bytes
 *
 * @return maximum compressed size
 */
size_t apultra_get_max_compressed_size(const size_t nInputSize) {
   return ((nInputSize * 9 /* literals + literal bits */ + 1 /* match bit */ + 2 /* 7+1 command bits */ + 8 /* EOD offset bits */) + 7) >> 3;
}

/**
 * Compress memory
 *
 * @param pInputData pointer to input(source) data to compress
 * @param pOutBuffer buffer for compressed data
 * @param nInputSize input(source) size in bytes
 * @param nMaxOutBufferSize maximum capacity of compression buffer
 * @param nFlags compression flags (set to 0)
 * @param nMaxWindowSize maximum window size to use (0 for default)
 * @param nDictionarySize size of dictionary in front of input data (0 for none)
 * @param progress progress function, called after compressing each block, or NULL for none
 * @param pStats pointer to compression stats that are filled if this function is successful, or NULL
 *
 * @return actual compressed size, or -1 for error
 */
size_t apultra_compress(const unsigned char *pInputData, unsigned char *pOutBuffer, const size_t nInputSize, const size_t nMaxOutBufferSize,
      const unsigned int nFlags, const size_t nMaxWindowSize, const size_t nDictionarySize, void(*progress)(long long nOriginalSize, long long nCompressedSize), apultra_stats *pStats) {
   apultra_compressor compressor;
   size_t nOriginalSize = 0;
   size_t nCompressedSize = 0L;
   int nResult;
   int nMaxArrivals = NARRIVALS_PER_POSITION_SMALL;
   int nError = 0;
   const int nBlockSize = (nInputSize < BLOCK_SIZE) ? ((nInputSize < 1024) ? 1024 : (const int)nInputSize) : BLOCK_SIZE;
   const int nMaxOutBlockSize = (const int)apultra_get_max_compressed_size(nBlockSize);

   if (nDictionarySize < nInputSize) {
      int nInDataSize = (int)(nInputSize - nDictionarySize);
      if (nInDataSize > nBlockSize)
         nInDataSize = nBlockSize;

      if (nInDataSize > 0 && (nDictionarySize + nInDataSize) >= nInputSize) {
         if (nInputSize <= 262144)
            nMaxArrivals = NARRIVALS_PER_POSITION_MAX;
         else
            nMaxArrivals = NARRIVALS_PER_POSITION_NORMAL;
      }
   }

   nResult = apultra_compressor_init(&compressor, nBlockSize, nBlockSize * 2, nMaxArrivals, nFlags);
   if (nResult != 0) {
      return -1;
   }

   compressor.max_offset = nMaxWindowSize ? (int)nMaxWindowSize : MAX_OFFSET;

   int nPreviousBlockSize = 0;
   // int nNumBlocks = 0;
   int nCurBitsOffset = 0, nCurBitShift = -1, nCurFollowsLiteral = 0;
   int nBlockFlags = 1;
   int nCurRepMatchOffset = 0;

   if (nDictionarySize) {
      nOriginalSize = (int)nDictionarySize;
      nPreviousBlockSize = (int)nDictionarySize;
   }

   while (nOriginalSize < nInputSize && !nError) {
      int nInDataSize;

      nInDataSize = (int)(nInputSize - nOriginalSize);
      if (nInDataSize > nBlockSize)
         nInDataSize = nBlockSize;

      if (nInDataSize > 0) {
         int nOutDataSize;
         int nOutDataEnd = (int)(nMaxOutBufferSize - nCompressedSize);

         if (nOutDataEnd > nMaxOutBlockSize)
            nOutDataEnd = nMaxOutBlockSize;

         if ((nOriginalSize + nInDataSize) >= nInputSize)
            nBlockFlags |= 2;
         nOutDataSize = apultra_compressor_shrink_block(&compressor, pInputData + nOriginalSize - nPreviousBlockSize, nPreviousBlockSize, nInDataSize, pOutBuffer + nCompressedSize, nOutDataEnd,
            &nCurBitsOffset, &nCurBitShift, &nCurFollowsLiteral, &nCurRepMatchOffset, nBlockFlags);
         nBlockFlags &= (~1);

         if (nOutDataSize >= 0) {
            /* Write compressed block */

            nOriginalSize += nInDataSize;
            nCompressedSize += nOutDataSize;
            if (nCurBitShift != -1)
               nCurBitsOffset -= nOutDataSize;
         }
         else {
            nError = -1;
         }

         nPreviousBlockSize = nInDataSize;
         // nNumBlocks++;
      }

      if (!nError && nOriginalSize < nInputSize) {
         if (progress)
            progress(nOriginalSize, nCompressedSize);
      }
   }

   if (progress)
      progress(nOriginalSize, nCompressedSize);
   if (pStats)
      *pStats = compressor.stats;

   apultra_compressor_destroy(&compressor);

   if (nError) {
      return -1;
   }
   else {
      return nCompressedSize;
   }
}
