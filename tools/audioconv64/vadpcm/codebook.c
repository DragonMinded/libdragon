// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "binary.h"
#include "vadpcm.h"

enum {
    // The only supported version number for VADPCM data. This is also the only
    // known version number.
    kVADPCMVersion = 1,

    // Header size for AIFC codebook, not counting the predictor data.
    kVADPCMAIFCHeaderSize = 6,
};

vadpcm_error vadpcm_read_codebook_aifc(
    struct vadpcm_codebook_spec *restrict spec, size_t *restrict data_offset,
    const void *VADPCM_RESTRICT data, size_t size) {
    const uint8_t *restrict p = data;

    // Read the header:
    // u16 version (equals 1)
    // u16 order
    // u16 predictor count
    if (size < 2) {
        return kVADPCMErrInvalidData;
    }
    int version = vadpcm_read16(p);
    if (version != kVADPCMVersion) {
        return kVADPCMErrUnknownVersion;
    }
    int order = vadpcm_read16(p + 2);
    int predictor_count = vadpcm_read16(p + 4);
    if (order == 0 || predictor_count == 0) {
        return kVADPCMErrInvalidData;
    }
    if (order > kVADPCMMaxOrder) {
        return kVADPCMErrLargeOrder;
    }
    if (predictor_count > kVADPCMMaxPredictorCount) {
        return kVADPCMErrLargePredictorCount;
    }

    // Check that there's enough space for the predictor data.
    size_t vsize = (size_t)16 * predictor_count * order;
    if (size < vsize + kVADPCMAIFCHeaderSize) {
        return kVADPCMErrInvalidData;
    }

    spec->predictor_count = predictor_count;
    spec->order = order;
    *data_offset = kVADPCMAIFCHeaderSize;
    return 0;
}

void vadpcm_read_vectors(int count, const void *VADPCM_RESTRICT data,
                         struct vadpcm_vector *VADPCM_RESTRICT vectors) {
    const uint8_t *ptr = data;
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < 8; j++) {
            vectors[i].v[j] = vadpcm_read16(ptr + 16 * i + 2 * j);
        }
    }
}
