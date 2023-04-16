SECTIONS {
    /* Write text section */
    .text  : {
        *(.text)
        *(.text.*)
        *(.init)
        *(.fini)
        *(.gnu.linkonce.t.*)
    }
   .eh_frame_hdr : { *(.eh_frame_hdr) }
   /* Write exception frames which must be 4-byte aligned to satisfy MIPS requirements */
   .eh_frame ALIGN(4) : {
        __EH_FRAME_BEGIN__ = .; /* Define symbol for accessing eh_frame section */
        KEEP (*(.eh_frame))
        /* Add terminator to section */
        LONG(0);
    }
   .gcc_except_table : { *(.gcc_except_table*) }
   
   /* Write read-only data */
    .rodata : {
        *(.rdata)
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
    }
    
    /* Write constructors and destructors which each must be 4-byte aligned */
    .ctors ALIGN(4) : {
        LONG(0); /* Add terminator to CTOR list */
        KEEP(*(SORT(.ctors.*)))
        KEEP(*(.ctors))
        __CTOR_LIST__ = .-4; /* Define symbol for CTOR list */
    }
    
    .dtors ALIGN(4) : {
        __DTOR_LIST__ = .; /* Define symbol for DTOR list */
        KEEP(*(SORT(.dtors.*)))
        KEEP(*(.dtors))
        LONG(0); /* Add terminator to DTOR list */
    }
    
    /* Write data sections */
    
    .data : {
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
    }
    
    .sdata : {
        *(.sdata)
        *(.sdata.*)
        *(.gnu.linkonce.s.*)
        /* Define 4 bytes of space for __dso_handle */
        . = ALIGN(4);
        __dso_handle = .;
        LONG(0);
    }

    .lit8 : {
        *(.lit8)
    }
    
    .lit4 : {
        *(.lit4)
    }
    
    /* Write bss sections */
    .sbss (NOLOAD) : {
        *(.sbss)
        *(.sbss.*)
        *(.gnu.linkonce.sb.*)
        *(.scommon)
        *(.scommon.*)
    }
    
    .bss (NOLOAD) : {
        *(.bss)
        *(.bss*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
    }
}
