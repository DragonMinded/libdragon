#include <ucode.S>
.text

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