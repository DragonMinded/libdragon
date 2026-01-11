#include "emux.h"


.macro emux_detect rd
    .word EMUX_XDETECT(\rd)
.endm

.macro emux_break
    .word EMUX_XBREAK()
.endm

.macro emux_ioctl_exit
    .word EMUX_XIOCTL(EMUX_IOCTL_EXIT)
.endm
