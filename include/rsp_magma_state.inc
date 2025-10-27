#include "magma_constants.h"

.macro _MgState initialize
    MG_VIEWPORT:
        MG_VIEWPORT_SCALE:      .half   0,0,0,0
        MG_VIEWPORT_OFFSET:     .half   0,0,0,0
    .ifeq \initialize
        MG_CLIP_FACTORS:            .half   0,0,0,0
    .else
        MG_CLIP_FACTORS:            .half   1,1,MG_DEFAULT_GUARD_BAND,MG_DEFAULT_GUARD_BAND
    .endif
    MG_VERTEX_SIZE:             .half   0,0,0,0
    MG_SHADER_TEXT:             .word   0
    MG_SHADER_TEXT_SIZE:        .word   0
    MG_VERTEX_BUFFER:           .word   0
    .ifeq \initialize
        MG_TRI_CMD:                 .half   0
    .else
        MG_TRI_CMD:                 .half   0x800
    .endif
    MG_CULL_MODE:               .byte   0
    MG_OUTPUT_OFFSET:           .byte   0
.endm