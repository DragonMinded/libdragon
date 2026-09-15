// Copyright 2023 Dietrich Epp.
// This file is part of VADPCM. VADPCM is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "codec/autocorr.h"

#include "codec/vadpcm.h"

void vadpcm_autocorr(size_t frame_count, float (*restrict corr)[6],
                     const int16_t *restrict src) {
    float x0 = 0.0f, x1 = 0.0f, x2 = 0.0f, m[6];
    size_t frame;
    int i;

    for (frame = 0; frame < frame_count; frame++) {
        for (i = 0; i < 6; i++) {
            m[i] = 0.0f;
        }
        for (i = 0; i < kVADPCMFrameSampleCount; i++) {
            x2 = x1;
            x1 = x0;
            x0 = src[frame * kVADPCMFrameSampleCount + i] * (1.0f / 32768.0f);
            m[0] += x0 * x0;
            m[1] += x1 * x0;
            m[2] += x1 * x1;
            m[3] += x2 * x0;
            m[4] += x2 * x1;
            m[5] += x2 * x2;
        }
        for (int i = 0; i < 6; i++) {
            corr[frame][i] = m[i];
        }
    }
}
