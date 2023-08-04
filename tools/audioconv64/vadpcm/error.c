// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "vadpcm.h"

const char *vadpcm_error_name(vadpcm_error err) {
    switch (err) {
    case kVADPCMErrNone:
        return "no error";
    case kVADPCMErrInvalidData:
        return "invalid data";
    case kVADPCMErrLargeOrder:
        return "predictor order too large";
    case kVADPCMErrLargePredictorCount:
        return "predictor count too large";
    case kVADPCMErrUnknownVersion:
        return "unknown VADPCM version";
    case kVADPCMErrInvalidParams:
        return "invalid encoding parameters";
    }
    return 0;
}
