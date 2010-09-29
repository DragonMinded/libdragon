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
 * $Header: /cvsroot/n64dev/n64dev/lib/alt-libn64/n64ld.x,v 1.2 2006/08/11 15:54:11 halleyscometsw Exp $
 *
 * ========================================================================
 */

OUTPUT_FORMAT ("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH (mips)
EXTERN (_start)
ENTRY (_start)

SECTIONS {
   /* Start address of code is 1K up in uncached, unmapped RAM.  We have
    * to be at least this far up in order to not interfere with the cart
    * boot code which is copying it down from the cart
    */

   . = 0x80000400 ;

   /* The text section carries the app code and its relocation addr is
    * the first byte of the cart domain in cached, unmapped memory
    */

   .text : {
      FILL (0)

      *(.boot)
	  . = ALIGN(16);
      __text_start = . ;
      *(.text)
      *(.ctors)
      *(.dtors)
      *(.rodata)
	  *(.init)
      *(.fini)
      __text_end  = . ;
   }


   /* Data section has relocation address at start of RAM in cached,
    * unmapped memory, but is loaded just at the end of the text segment,
    * and must be copied to the correct location at startup
    */

   .data : {
      /* Gather all initialised data together.  The memory layout
       * will place the global initialised data at the lowest addrs.
       * The lit8, lit4, sdata and sbss sections have to be placed
       * together in that order from low to high addrs with the _gp symbol
       * positioned (aligned) at the start of the sdata section.
       * We then finish off with the standard bss section
       */

      FILL (0xaa)

	  . = ALIGN(16);
      __data_start = . ;
         *(.data)
         *(.lit8)
         *(.lit4) ;
     /* _gp = ALIGN(16) + 0x7ff0 ;*/
/*	 _gp = . + 0x7ff0; */
	 . = ALIGN(16);
	 _gp = . ;
         *(.sdata)
      __data_end = . ;
/*
      __bss_start = . ;
         *(.scommon)
         *(.sbss)
         *(COMMON)
         *(.bss)
      /* XXX Force 8-byte end alignment and update startup code */

      __bss_end = . ;
*/
   }

   .bss (NOLOAD) :  {
       	__bss_start = . ;
       	*(.scommon)
	*(.sbss)
	*(COMMON)
	*(.bss)
	__bss_end = . ;
	end = . ;
   }

}
