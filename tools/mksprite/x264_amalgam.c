/*
    x264_amalgam: minimal x264 C amalgam for libdragon tools
    This intentionally disables all assembly, threading, and external deps.
*/

#include "x264_amalgam_config.h"


// Encoder API (must be included before common.h macros are defined).
#include "x264/encoder/api.c"

// Common (bitdepth-independent) core.
#include "x264/common/base.c"
#include "x264/common/bitstream.c"
#include "x264/common/cabac.c"
#include "x264/common/common.c"
#include "x264/common/cpu.c"
#include "x264/common/dct.c"
#include "x264/common/deblock.c"
#include "x264/common/frame.c"
#include "x264/common/macroblock.c"
#include "x264/common/mc.c"
#include "x264/common/mvpred.c"
#include "x264/common/osdep.c"
#include "x264/common/pixel.c"
#include "x264/common/predict.c"
#include "x264/common/quant.c"
#include "x264/common/rectangle.c"
#include "x264/common/set.c"
#include "x264/common/tables.c"
#include "x264/common/vlc.c"

// Encoder.
#include "x264/encoder/cabac.c"
#include "x264/encoder/cavlc.c"
#include "x264/encoder/encoder.c"
#include "x264/encoder/lookahead.c"
#include "x264/encoder/macroblock.c"
#include "x264/encoder/me.c"
#include "x264/encoder/ratecontrol.c"
#include "x264/encoder/set.c"
