#include <ucode.S>
.text

vabs v1, v2, v3, 1
lhv v1, 15, 0, s2
lw $9, var1
sw $9, var2
break

deadloop:
	j deadloop
	nop

.data
var1: .word 0xDEADBEEF
var2: .word 0xBEEFF00D