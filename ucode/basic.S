#include <ucode.S>

vabs v1, v2, v3, 1
lhv v1, 15, 0, s2
break

# Fill with zeros
.org 2048, 0