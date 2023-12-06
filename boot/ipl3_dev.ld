OUTPUT_FORMAT ("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH (mips)
EXTERN (_start)
ENTRY (_start)

MEMORY
{
    rom  : ORIGIN = 0xB0000000, LENGTH = 65536
    dmem : ORIGIN = 0xA4000000, LENGTH = 4096
}

SECTIONS {
    .text.prologue 0xB0000000 : {
        KEEP(*(.header))
        KEEP(*(.text.boot_trampoline))
        KEEP(*(.text.ique_trampoline))
    } > rom

    .text.dmem 0xA4000040 : AT ( 0xB0001040 ) {
        LONG(0x3044d236);  /* special dummy opcode to simplify hash cracking; this is not needed in dev mode, but we keep it in case it matters */
        KEEP(*(.stage1.pre))
        KEEP(*(.stage1))
        . = ALIGN(16);
        KEEP(*(.banner))
        build/ipl3.o       (.text .text.* .data)
        build/rdram.o      (.text .text.* .data)
        build/minidragon.o (.text .text.* .data)
        build/entropy.o    (.text .text.* .data)
        *(.bss)
        . = ALIGN(8);
    } > dmem

    __stage2_size = __stage2_end - __stage2_start;

    .text.rom 0xB0001040 + SIZEOF ( .text.dmem ) : {
        __stage2_start = .;
        KEEP(build/loader.o(.text.loader))
        KEEP(build/loader.o(.text .text.*))
        KEEP(build/loader.o(.data .data.*))
        . = ALIGN(8);
        __stage2_end = .;
        build/debug.o      (.text .text.* .data)
        *(.rodata)
        *(.rodata.*)
    } > rom

    /DISCARD/ : { *(.MIPS.abiflags) }
}
