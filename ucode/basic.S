#include <ucode.S>
.global __basic_ucode_start
.global __basic_ucode_end

.section .data
.p2align 4, 0
__basic_ucode_start:

vabs v1, v2, v3, 1
lhv v1, 15, 0, s2
break

# Fill with zeros
.org __basic_ucode_start + 2048
__basic_ucode_data_start:

# Fill with zeros
.org __basic_ucode_data_start + 2048
__basic_ucode_end: