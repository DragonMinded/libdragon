OUTPUT_FORMAT ("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH (mips)
EXTERN (_start)
ENTRY (_start)

MEMORY
{
    dmem : ORIGIN = 0x0000, LENGTH = 0x1000
    imem : ORIGIN = 0x1000, LENGTH = 0x1000
}

SECTIONS
{
    .text : { *(.text) } > imem
    .data : { *(.data) } > dmem
    .bss :  { *(.bss) }  > dmem
}
