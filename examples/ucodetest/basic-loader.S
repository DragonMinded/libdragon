.section .data

.balign 8
.global __basic_ucode_data_start
__basic_ucode_data_start:

.incbin "./basic-data.section.bin"

.balign 8
.global __basic_ucode_start
__basic_ucode_start:

.incbin "./basic-text.section.bin"

.balign 8
.global __basic_ucode_end
__basic_ucode_end: