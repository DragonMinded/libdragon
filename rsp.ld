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

    /* Define runtime addresses for text and data segments. On the RSP,
       only the lowest 12 bits are used in general for addressing (and %lo()
       is often used to make instructions shorter), so labels in the data
       segment will resolve to 0x000-0xFFF and labels in the text segment
       will resolve to 0x1000-0x1FFF. This makes it easier to use those
       addresses as part of DMA transfers.

       The upper part of the addresses (ignored by RSP) was chosen to
       match the VR4300 addresses where DMEM/IMEM are mapped, which makes
       it easier to resolve symbols also for debuggers like the gdb stub in cen64.
    */
    ram_data : ORIGIN = 0xA4000000, LENGTH = 0x1000
    ram_text : ORIGIN = 0xA4001000, LENGTH = 0x1000
}

SECTIONS
{
    .text : { 
        _start = .;
        KEEP(*(.text))
        *(.text.*)
        _text_end = .;
    } > ram_text AT > rom_imem

    .data : { 
        KEEP(*(.data))
        *(.data.*)
        _data_end = .;
    } > ram_data AT > rom_dmem

    . = ALIGN(8);

    OVERLAY : {
        .bss { 
            KEEP(*(.bss))
            *(.bss.*)
        } 
        .bssovl1 { 
            KEEP(*(.bssovl1))
            *(.bssovl1.*)
        } 
        .bssovl2 { 
            KEEP(*(.bssovl2))
            *(.bssovl2.*)
        } 
        .bssovl3 { 
            KEEP(*(.bssovl3))
            *(.bssovl3.*)
        } 
    } > ram_data AT > rom_dmem

    /DISCARD/ : { *(.MIPS.abiflags) }
}
