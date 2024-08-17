OUTPUT_FORMAT ("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH (mips)
EXTERN (_start)
ENTRY (_start)

MEMORY
{
    dmem : ORIGIN = 0xA4000000, LENGTH = 4096
    rom  : ORIGIN = 0xB0001000, LENGTH = 16384
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
        *(.bss)
    } > dmem

    .text.signature 0xA4000FF8 : {
        QUAD(0x000097C7804A059C);
    } > dmem

    __stage2_size = __stage2_end - __stage2_start;

    .text.ique 0xB0001000  : AT ( 0xA4001000 ) {
        KEEP(*(.text.ique_trampoline))
    }
    .text.stage23 0xB0001040  : AT ( 0xA4001040 ) {
        KEEP(build/loader.o(.text.stage3))
        . = ALIGN(8);
        __stage2_start = .;
        LONG(__stage2_end - __stage2_start);
        LONG(0);
        KEEP(build/loader.o(.text.stage2))
        KEEP(build/loader.o(.text .text.* .rodata .rodata.*))
        KEEP(build/loader.o(.data .data.*))
        . = ALIGN(8);
        __stage2_end = .;
    } > rom
}
