/* ========================================================================
 *
 * n64ld.x
 *
 * GNU Linker script for building an image that is set up for the N64
 * but still has the data factored into sections.  It is not directly
 * runnable, and it contains the debug info if available.  It will need
 * a 'loader' to perform the final stage of transformation to produce
 * a raw image.
 *
 * Copyright (c) 1999 Ground Zero Development, All rights reserved.
 * Developed by Frank Somers <frank@g0dev.com>
 * Modifications by hcs (halleyscometsoftware@hotmail.com)
 *
 * $Header: /afs/icequake.net/users/nemesis/n64/sf/asdf/n64dev/lib/alt-libn64/n64ld.x,v 1.2 2006-08-11 15:54:11 halleyscometsw Exp $
 *
 * ========================================================================
 */

OUTPUT_FORMAT ("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH (mips)
EXTERN (_start)
ENTRY (_start)

__kseg0_start = 0x80000000;

/* Start address of code is 1K up in cached, unmapped RAM (KSEG0). We have
* to be at least this far up in order to not interfere with the cart
* boot code which is copying it down from the cart
*/
__libdragon_text_start  = __kseg0_start + 0x400;

MEMORY
{
    mem : ORIGIN = __kseg0_start, LENGTH = 4M
}

SECTIONS {
    /* The text section carries the app code and its relocation addr is
    * the first byte of the cart domain in cached, unmapped memory
    */
    .text __libdragon_text_start  : {
        *(.boot)
        . = ALIGN(16);
        __text_start = .;
        *(.text)
        *(.text.*)
        *(.init)
        *(.fini)
        *(.gnu.linkonce.t.*)
        . = ALIGN(16);
        __text_end  = .;
    } > mem

   .eh_frame_hdr : { *(.eh_frame_hdr) } > mem
   .eh_frame : { 
		__EH_FRAME_BEGIN__ = .; /* Define symbol for accessing eh_frame section */
		KEEP (*(.eh_frame)) 
	} > mem
   .gcc_except_table : { *(.gcc_except_table*) } > mem
   .jcr : { KEEP (*(.jcr)) } > mem

    .rodata : {
        *(.rdata)
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
        . = ALIGN(8);
    } > mem

    /* This is important to keep __CTOR_END__ consistent */
    . = ALIGN(4);

    .ctors : {
        __CTOR_LIST__ = .;
        /* Actually we should place the count here like;

        LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 2)

        but g++ seem to already do this (though incorrectly placing an 0xFFFFFFFF)
        and that breaks things if we also add it. Ideally this should be parsed
        by the code but we already know the end as well.

        The new build system links everything via g++ by default and should use
        __wrap___do_global_ctors and enables it via the --wrap linker option.
        When linking with ld, we should use __do_global_ctors, which is the default
        if you don't provide the --wrap option. */
        KEEP(*(.ctors))
         /* Similarly we should have a;

        LONG(0)

        here, and g++ seem to insert it at __CTOR_END__ */
        __CTOR_END__ = .;
    } > mem

    . = ALIGN(8);

    /* Data section has relocation address at start of RAM in cached,
    * unmapped memory, but is loaded just at the end of the text segment,
    * and must be copied to the correct location at startup
    * Gather all initialised data together.  The memory layout
    * will place the global initialised data at the lowest addrs.
    * The lit8, lit4, sdata and sbss sections have to be placed
    * together in that order from low to high addrs with the _gp symbol
    * positioned (aligned) at the start of the sdata section.
    * We then finish off with the standard bss section
    */
    .data : {
        __data_start = .;
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
        . = ALIGN(8);
    } > mem

    /* Small data START */

    .sdata : {
        _gp = . + 0x8000;
        *(.sdata)
        *(.sdata.*)
        *(.gnu.linkonce.s.*)
        . = ALIGN(8);
    } > mem

    .lit8 : {
        *(.lit8)
        . = ALIGN(8);
    } > mem
    .lit4 : {
        *(.lit4)
        . = ALIGN(8);
    } > mem

    __data_end = .;

    .sbss (NOLOAD) : {
         __bss_start = .;
        *(.sbss)
        *(.sbss.*)
        *(.gnu.linkonce.sb.*)
        *(.scommon)
        *(.scommon.*)
    } > mem

    /* Small data END */

    . = ALIGN(8);
    .bss (NOLOAD) : {
        *(.bss)
        *(.bss*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(8);
         __bss_end = .;
    } > mem

    . = ALIGN(8);
    __rom_end = .;

    /* Deprecated */
    end = .;
}
