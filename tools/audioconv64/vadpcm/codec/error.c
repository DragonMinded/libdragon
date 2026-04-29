// Copyright 2022 Dietrich Epp.
// This file is part of VADPCM. VADPCM is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "codec/vadpcm.h"

const char *vadpcm_error_name(vadpcm_error err) {
    switch (err) {
    case kVADPCMErrNone:
        return "no error";
    case kVADPCMErrInvalidData:
        return "invalid data";
    case kVADPCMErrInvalidParams:
        return "invalid encoding parameters";
    case kVADPCMErrMemory:
        return "memory allocation failed";
    }
    return 0;
}
