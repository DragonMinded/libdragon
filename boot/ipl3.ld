OUTPUT_FORMAT ("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH (mips)
EXTERN (_start)
ENTRY (_start)

__ipl3_text_start   = 0xA4000040;
__ipl3_stage2_start = 0x807FF000;

MEMORY
{
    dmem : ORIGIN = 0xA4000040, LENGTH = 4088
    imem : ORIGIN = 0xA4001000, LENGTH = 2048
    rdram : ORIGIN = 0x807FF000, LENGTH = 4096
}

SECTIONS {
    /* The text section carries the app code and its relocation addr is
    * the first byte of the cart domain in cached, unmapped memory
    */
    .text __ipl3_text_start  : {
        *(.boot)
        . = ALIGN(16);
        __text_start = .;
        *(.text)
        *(.text.*)
        *(.data)
        *(.rodata)
        *(.rodata.*)
        __text_end  = .;
    } > dmem

    .text __ipl3_stage2_start : {
        *(.ipl3.stage2)
    } > rdram

    .ipl3section : {
        . = ALIGN(4);    
        ipl3section_start = .;
        *(.ipl3section)
        ipl3section_end = .;
        . = ALIGN(4);
    } > dmem

    . = ALIGN(4);
    .bss : {
        *(.bss)
    } > imem

    /DISCARD/ : { *(.MIPS.abiflags) }
}
