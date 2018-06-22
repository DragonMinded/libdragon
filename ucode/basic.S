#include <ucode.S>

.set noat

vabs v1, v2, v3, 0
swv v1, 0, 0, s2

break
