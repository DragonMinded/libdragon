OUTPUT_FORMAT ("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH (mips)
EXTERN (_start)
ENTRY (_start)

__ipl3_text_start   = 0xA4000040;

MEMORY
{
    dmem : ORIGIN = 0xA4000040, LENGTH = 4088
    imem : ORIGIN = 0xA4001000, LENGTH = 2048
}

SECTIONS {
    /* The text section carries the app code and its relocation addr is
    * the first byte of the cart domain in cached, unmapped memory
    */
    .text __ipl3_text_start  : {
        . = ALIGN(16);
        *(.boot)
        build/ipl3.o       (.text .text.* .data .rodata .rodata.*)
        build/rdram.o      (.text .text.* .data .rodata .rodata.*)
        build/minidragon.o (.text .text.* .data .rodata .rodata.*)
        build/debug.o      (.text .text.* .data .rodata .rodata.*)
        . = ALIGN(8);
        __stage2_start = .;
        KEEP(build/loader.o(.text .text.*))
        KEEP(build/loader.o(.data .data.* .rodata .rodata.*))
        . = ALIGN(8);
        __stage2_end = .;
    } > dmem

    __stage2_size = __stage2_end - __stage2_start;

    . = ALIGN(4);
    .bss : {
        *(.bss)
    } > imem

    /DISCARD/ : { *(.MIPS.abiflags) }
}
