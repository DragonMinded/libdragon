############################################################
# emux.inc
#
# Authors: Giovanni Bajo <giovannibajo@gmail.com>
############################################################
#include "emux.h"


.macro emux_detect rd, code
    .word EMUX_XDETECT(\rd, \code)
.endm

.macro emux_break
    .word EMUX_XBREAK()
.endm

.macro emux_ioctl_exit
    .word EMUX_XIOCTL(EMUX_IOCTL_EXIT)
.endm
