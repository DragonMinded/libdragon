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
          h264bsdWriteMacroblock
          h264bsdWriteOutputBlocks

------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "h264bsd_image.h"
#include "h264bsd_util.h"
#include "h264bsd_neighbour.h"

#ifdef H264BSD_N64
#include "../rsph264.h"
#endif

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/* x- and y-coordinates for each block, defined in h264bsd_intra_prediction.c */
extern const u32 h264bsdBlockX[];
extern const u32 h264bsdBlockY[];

/* clipping table, defined in h264bsd_intra_prediction.c */
extern const u8 h264bsdClip[];

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------

    Function: h264bsdWriteMacroblock

        Functional description:
            Write one macroblock into the image. Both luma and chroma
            components will be written at the same time.

        Inputs:
            data    pointer to macroblock data to be written, 256 values for
                    luma followed by 64 values for both chroma components

        Outputs:
            image   pointer to the image where the macroblock will be written

        Returns:
            none

------------------------------------------------------------------------------*/
#ifndef H264DEC_NEON
void h264bsdWriteMacroblock(image_t *image, u8 *data, int flush)
{
#ifdef H264BSD_N64
    int flags = flush ? 0 : RSPH264_CACHE_SKIP_ALL;
    rsph264_queue_write_macroblock(flags,
        data, image->luma, image->cb, image->cr, image->width);
    // rsph264_sync();
    return;
#else // N64
/* Variables */

    u32 i;
    u32 width;
    u32 *lum, *cb, *cr;
    u32 *ptr;
    u32 tmp1, tmp2;

/* Code */

    ASSERT(image);
    ASSERT(data);
    ASSERT(!((u32)data&0x3));

    width = image->width;

    /*lint -save -e826 lum, cb and cr used to copy 4 bytes at the time, disable
     * "area too small" info message */
    lum = (u32*)image->luma;
    cb = (u32*)image->cb;
    cr = (u32*)image->cr;
    ASSERT(!((u32)lum&0x3));
    ASSERT(!((u32)cb&0x3));
    ASSERT(!((u32)cr&0x3));

    ptr = (u32*)data;

    width *= 4;
    for (i = 16; i ; i--)
    {
        tmp1 = *ptr++;
        tmp2 = *ptr++;
        *lum++ = tmp1;
        *lum++ = tmp2;
        tmp1 = *ptr++;
        tmp2 = *ptr++;
        *lum++ = tmp1;
        *lum++ = tmp2;
        lum += width-4;
    }

    width >>= 1;
    for (i = 8; i ; i--)
    {
        tmp1 = *ptr++;
        tmp2 = *ptr++;
        *cb++ = tmp1;
        *cb++ = tmp2;
        cb += width-2;
    }

    for (i = 8; i ; i--)
    {
        tmp1 = *ptr++;
        tmp2 = *ptr++;
        *cr++ = tmp1;
        *cr++ = tmp2;
        cr += width-2;
    }
#endif // N64
}
#endif


