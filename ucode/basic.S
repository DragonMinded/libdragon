#include <ucode.h>

.set noat

loadVector v1, BV(0, 0), s1
vectorOp ABS(0, v1, v2, v3)
break
