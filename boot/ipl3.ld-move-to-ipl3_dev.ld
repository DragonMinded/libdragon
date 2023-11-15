OUTPUT_FORMAT ("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH (mips)
EXTERN (_start)
ENTRY (_start)

MEMORY
{
    dmem : ORIGIN = 0xA4000000, LENGTH = 4096
    imem : ORIGIN = 0xA4001000, LENGTH = 2048
    rom  : ORIGIN = 0xB0002000, LENGTH = 16384
}

SECTIONS {
    .text.dmem 0xA4000000 : {
        KEEP(*(.header))
        KEEP(*(.boot))
        . = ALIGN(16);
        KEEP(*(.banner))
        build/ipl3.o       (.text .text.* .data)
        build/rdram.o      (.text .text.* .data)
        build/minidragon.o (.text .text.* .data)
        . = ALIGN(8);
        __stage2_start = .;
        KEEP(build/loader.o(.text.loader))
        KEEP(build/loader.o(.text .text.*))
        KEEP(build/loader.o(.data .data.*))
        . = ALIGN(8);
        __stage2_end = .;
    } > dmem

    __stage2_size = __stage2_end - __stage2_start;

    .text.rom 0xB0001000 + SIZEOF ( .text.dmem ) : AT ( ADDR( .text.dmem ) + SIZEOF( .text.dmem ) ) {
        build/debug.o      (.text .text.* .data)
        *(.rodata)
        *(.rodata.*)
    }

    . = ALIGN(4);
    .bss : {
        *(.bss)
    } > imem

    /DISCARD/ : { *(.MIPS.abiflags) }
}
