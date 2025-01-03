#include "../src/regs.S"

.set noreorder

.text

#Function with R_MIPS_HI16 and R_MIPS_LO16 relocation
.global dl_test_hilo_reloc
dl_test_hilo_reloc:
lui v0, %hi(dl_test_jump_reloc+8)
addiu v0, v0, %lo(dl_test_jump_reloc+8)

#Function with R_MIPS_26 relocation for both J and JAL
.global dl_test_jump_reloc
dl_test_jump_reloc:
jal dl_test_hilo_reloc+4
j dl_test_jump_reloc+8

.data

#Global variable containing R_MIPS_32 relocation
.global dl_test_word_reloc
dl_test_word_reloc:
.word dl_test_hilo_reloc+4