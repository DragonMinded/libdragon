#include <rsp.inc>
.text

li t1, SP_WSTATUS_SET_INTR_ON_BREAK
mtc0 t1, COP0_SP_STATUS

lqv $v01, 0, 0, $2

vabs $v01, $v02, $v03, 1
lhv $v01, 15, 0, $2
lw $v00, var1
sw $9, var2
break

deadloop:
	j deadloop
	nop

.data
var1: .word 0xDEADBEEF
var2: .word 0xBEEFF00D
