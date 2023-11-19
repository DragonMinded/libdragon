OUTPUT_FORMAT ("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH (mips)
EXTERN (_start)
ENTRY (_start)

MEMORY
{
    dmem : ORIGIN = 0xA4000000, LENGTH = 4096
    imem : ORIGIN = 0xA4001010, LENGTH = 2032   /* First 16 bytes: boot information */
    rom  : ORIGIN = 0xB0001000, LENGTH = 16384
}

SECTIONS {
    .text.dmem 0xA4000000 : {
        KEEP(*(.header))
        KEEP(*(.boot))
        . = ALIGN(16);
        KEEP(*(.banner))
        build/ipl3.o       (.text .text.* .data .rodata .rodata.*)
        build/rdram.o      (.text .text.* .data .rodata .rodata.*)
        build/minidragon.o (.text .text.* .data .rodata .rodata.*)
        build/entropy.o    (.text .text.* .data .rodata .rodata.*)
        *(.bss)
        . = ALIGN(8);
        __stage2_start = .;
        KEEP(build/loader.o(.text.loader))
        KEEP(build/loader.o(.text .text.* .rodata .rodata.*))
        KEEP(build/loader.o(.data .data.*))
        . = ALIGN(8);
        __stage2_end = .;
    } > dmem

    .text.signature 0xA4000FF8 : {
        QUAD(0x0000000000000000);
    } > dmem

    __stage2_size = __stage2_end - __stage2_start;

    .text.rom 0xB0001000  : AT ( 0xA4001000 ) {
        KEEP(*(.text.ique_trampoline))
    } > rom

    /DISCARD/ : { *(.MIPS.abiflags) }
}
