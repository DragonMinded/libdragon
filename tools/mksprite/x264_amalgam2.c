/*
    x264_amalgam2: second compilation unit for x264 (analysis/rdo)

    We need two compilation units to avoid linking errors, as x264
    include some files twice (eg: rdo.c), with different
    preprocessor definitions.
*/

#include "x264_amalgam_config.h"

// Encoder analysis (pulls in rdo.c and slicetype.c).
#include "x264/encoder/analyse.c"

