/* 
 * rsp.ld: Linker script for rsp ucode. 
 */

OUTPUT_FORMAT ("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH (mips)
ENTRY (_start)

MEMORY
{
    /* This is the layout in ROM. */
    rom_dmem : ORIGIN = 0x0000, LENGTH = 0x1000
    rom_imem : ORIGIN = 0x1000, LENGTH = 0x1000

    /* This is a workaround to make ld place text symbols at the correct addresses (0x0 - 0x1000).
       The RSP technically uses a harvard-architecture (https://en.wikipedia.org/wiki/Harvard_architecture)
       which means that it uses different address spaces for instructions and data accesses.
       Because ld is not designed for such architectures, we need to place the data section somewhere different,
       since it would otherwise overlap the text section. As a workaround, we place it at 0x04000000 (which is also
       the location of DMEM from the VR4300's point of view). Because the RSP only uses the lower 12 bits
       of any address, this works out fine (as long as we always wrap data addresses in "%lo()").
       
       Note that this is not actually required to run the ucode correctly (instruction addresses above 0x1000 are truncated anyway),
       but it makes debugging with gdb a lot easier (e.g. using this fork of cen64 https://github.com/lambertjamesd/cen64).
     */
    ram_data : ORIGIN = 0xA4000000, LENGTH = 0x1000
    ram_text : ORIGIN = 0x00000000, LENGTH = 0x1000
}

SECTIONS
{
    .text : { 
        KEEP(*(.text))
        *(.text.*)
    } > ram_text AT > rom_imem

    .data : { 
        KEEP(*(.data))
        *(.data.*)
    } > ram_data AT > rom_dmem

    . = ALIGN(8);

    .bss  : { 
        KEEP(*(.bss))
        *(.bss.*)
    } > ram_data AT > rom_dmem

    /DISCARD/ : { *(.MIPS.abiflags) }
}
