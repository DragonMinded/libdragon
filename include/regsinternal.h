#ifndef __LIBDRAGON_REGSINTERNAL_H
#define __LIBDRAGON_REGSINTERNAL_H

typedef struct AI_regs_s {
    short * address;
    unsigned long length;
    unsigned long control;
    unsigned long status;
    unsigned long dacrate;
    unsigned long samplesize;
} _AI_regs_s;

typedef struct MI_regs_s {
    unsigned long mode;
    unsigned long version;
    unsigned long intr;
    unsigned long mask;
} _MI_regs_s;

typedef struct VI_regs_s {
    unsigned long control;
    unsigned short int * framebuffer;
    unsigned long width;
    unsigned long v_int;
    unsigned long cur_line;
    unsigned long timing;
    unsigned long v_sync;
    unsigned long h_sync;
    unsigned long h_sync2;
    unsigned long h_limits;
    unsigned long v_limits;
    unsigned long color_burst;
    unsigned long h_scale;
    unsigned long v_scale;
} _VI_regs_s;

typedef struct PI_regs_s {
    void * ram_address;
    unsigned long pi_address;
    /* the length registers are named from the PI's
       perspective, thus read_length is the length
       to read from RAM, write_length is the length
       to write to RAM */
    unsigned long read_length;
    unsigned long write_length;
    unsigned long status;
} _PI_regs_s;

typedef struct SI_regs_s {
    volatile void * DRAM_addr;
    volatile void * PIF_addr_read; // for a read from PIF RAM
    unsigned long reserved1,reserved2;
    volatile void * PIF_addr_write; // for a write to PIF RAM
    unsigned long reserved3;
    unsigned long status;
} _SI_regs_s;

#endif
