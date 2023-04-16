#include "../src/regs.S"

.set noreorder

.data

#Define two variables as ASCII strings for verification of symbol search
.global dl_test_sym
dl_test_sym:
.asciz "dl_test_sym"
.global DLTestSym
DLTestSym:
.asciz "DLTestSym"

#4-byte aligned variable to verify that RTLD_DEFAULT works properly
.balign 4
.global dl_test_ptr
dl_test_ptr:
.word 0