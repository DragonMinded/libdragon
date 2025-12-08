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
    2. Module defines
    3. Data types
    4. Function prototypes

------------------------------------------------------------------------------*/

#ifndef H264SWDEC_CFG_H
#define H264SWDEC_CFG_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"

/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/

#define MAX_NUM_REF_PICS 16
#define MAX_NUM_SLICE_GROUPS 8
#define MAX_NUM_SEQ_PARAM_SETS 32
#define MAX_NUM_PIC_PARAM_SETS 256

// Number of fully decoded pictures that can be cached in DPB.
// On N64, we keep up to 4 extra pictures in the buffer. Notice
// that the buffer already contains 16 pictures for correctly
// keeping reference frames of a baseline profile video.
// These extra pics allow the client to keep decode pictures in
// advance and keep them around until it's time to display them,
// without having to copy them away.
#if H264BSD_N64
#define MAX_NUM_BUFFERED_PICS   4
#else
#define MAX_NUM_BUFFERED_PICS   0
#endif

/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

#endif /* #ifdef H264SWDEC_CFG_H */

