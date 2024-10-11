OUTPUT_FORMAT ("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH (mips)
EXTERN (_start)
ENTRY (_start)

MEMORY
{
    dmem : ORIGIN = 0xA4000000, LENGTH = 4096
    rom  : ORIGIN = 0xB0000000, LENGTH = 16384
}

SECTIONS {
    /DISCARD/ : { *(.MIPS.abiflags) }

    .text.dmem 0xA4000000 : {
        KEEP(*(.header))
        LONG(0x3044d236);  /* special dummy opcode to simplify hash cracking */
        KEEP(*(.stage1.pre))
        KEEP(*(.stage1))
        . = ALIGN(16);
        KEEP(*(.banner))
        build/ipl3.o       (.text .text.* .data .rodata .rodata.*)
        build/rdram.o      (.text .text.* .data .rodata .rodata.*)
        build/minidragon.o (.text .text.* .data .rodata .rodata.*)
        build/entropy.o    (.text .text.* .data .rodata .rodata.*)
    } > dmem

    .text.stage23 0xB0000000 + SIZEOF(.text.dmem) : AT ( 0xA4000000 + SIZEOF(.text.dmem) ) {
        KEEP(build/loader_compat.o(.text.stage3))
        . = ALIGN(8);
        __stage2_start = .;
        LONG(__stage2_end - __stage2_start);
        LONG(0);
        KEEP(build/loader_compat.o(.text.stage2))
        KEEP(build/loader_compat.o(.text .text.* .rodata .rodata.*))
        KEEP(build/loader_compat.o(.data .data.*))
        . = ALIGN(8);
        __stage2_end = .;
        *(.bss)
    } > rom

    .text.signature 0xA4000FF8 : {
        QUAD(0x00004AE1656086B9);
    } > dmem

    __stage2_size = __stage2_end - __stage2_start;

}
