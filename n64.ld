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

MEMORY
{
    mem : ORIGIN = 0x80000400, LENGTH = 4M-0x0400
}

SECTIONS {
    . = 0x80000400;

    .text : {
        *(.boot)
        . = ALIGN(16);
        __text_start = .;
        *(.text)
        *(.text.*)
        *(.init)
        *(.fini)
        *(.gnu.linkonce.t.*)
        __text_end  = .;
    } > mem

   .eh_frame_hdr : { *(.eh_frame_hdr) } > mem
   .eh_frame : { KEEP (*(.eh_frame)) } > mem
   .gcc_except_table : { *(.gcc_except_table*) } > mem
   .jcr : { KEEP (*(.jcr)) } > mem

    .rodata : {
        *(.rdata)
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
    } > mem

    . = ALIGN(8);

    .ctors : {
        __CTOR_LIST_SIZE__ = .;
        LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 1)
        __CTOR_LIST__ = .;
        *(.ctors)
        LONG(0)
        __CTOR_END__ = .;
    } > mem

    . = ALIGN(8);

    .dtors : {
        __DTOR_LIST__ = .;
        LONG((__DTOR_END__ - __DTOR_LIST__) / 4 - 2)
        *(.dtors)
        LONG(0)
        __DTOR_END__ = .;
    } > mem

    . = ALIGN(8);

    .data : {
        __data_start = .;
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
    } > mem

    .lit8 : {
        *(.lit8)
    } > mem
    .lit4 : {
        *(.lit4)
    } > mem

    . = ALIGN(8);
    .sdata : {
        /* Ideally, this should be _gp = . + 0x8000 - GCC is complaining about small data not being smaller than 64k - Any ideas? */
        _gp = . + 0x7000;
        *(.sdata)
        *(.sdata.*)
        *(.gnu.linkonce.s.*)
    } > mem

    __data_end = .;

    . = ALIGN(8);
    .sbss (NOLOAD) : {
         __bss_start = .;
        *(.sbss)
        *(.sbss.*)
        *(.gnu.linkonce.sb.*)
        *(.scommon)
        *(.scommon.*)
    } > mem


    . = ALIGN(8);
    .bss (NOLOAD) : {
        *(.bss)
        *(.bss*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        __bss_end = .;
    } > mem

    . = ALIGN(8);
    end = .;
}
