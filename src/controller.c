#include <string.h>
#include "libdragon.h"
#include "regsinternal.h"

#define SI_STATUS_DMA_BUSY  ( 1 << 0 )
#define SI_STATUS_IO_BUSY   ( 1 << 1 )

static volatile struct SI_regs_s * const SI_regs = (struct SI_regs_s *)0xa4800000;
static void * const PIF_RAM = (void *)0x1fc007c0;

static unsigned long long SI_read_con_block[8] = 
{
    0xff010401ffffffff,
    0xff010401ffffffff,
    0xff010401ffffffff,
    0xff010401ffffffff,
    0xfe00000000000000,
    0,
    0,
    1
};

static struct controller_data current;
static struct controller_data last;

void controller_init()
{
    memset(&current, 0, sizeof(current));
    memset(&last, 0, sizeof(last));
}

static void __SI_DMA_wait(void) 
{
    while (SI_regs->status & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY)) ;
}

static void __controller_exec_PIF( unsigned long long const inblock[8], unsigned long long outblock[8] ) 
{
    volatile unsigned long long inblock_temp[8];
    volatile unsigned long long outblock_temp[8];

    data_cache_writeback_invalidate(inblock_temp,64);
    memcpy(UncachedAddr(inblock_temp),inblock,64);

    __SI_DMA_wait();

    SI_regs->DRAM_addr = inblock_temp; // only cares about 23:0
    SI_regs->PIF_addr_write = PIF_RAM; // is it really ever anything else?

    __SI_DMA_wait();

    data_cache_writeback_invalidate(outblock_temp,64);

    SI_regs->DRAM_addr = outblock_temp;
    SI_regs->PIF_addr_read = PIF_RAM;

    __SI_DMA_wait();

    memcpy(outblock,UncachedAddr(outblock_temp),64);
}


void controller_read(struct controller_data * output) 
{
    __controller_exec_PIF(SI_read_con_block,(unsigned long long *)output);
}

void controller_scan()
{
    /* Remember last */
    memcpy(&last, &current, sizeof(current));

    /* Grab current */
    memset(&current, 0, sizeof(current));
    controller_read(&current);
}

struct controller_data get_keys_down()
{
    struct controller_data ret;

    /* Start with baseline */
    memcpy(&ret, &current, sizeof(current));

    /* Figure out which wasn't pressed last time and is now */
    for(int i = 0; i < 4; i++)
    {
        ret.c[i].buttons = (current.c[i].buttons) & ~(last.c[i].buttons);
    }

    return ret;
}

struct controller_data get_keys_up()
{
    struct controller_data ret;

    /* Start with baseline */
    memcpy(&ret, &current, sizeof(current));

    /* Figure out which wasn't pressed last time and is now */
    for(int i = 0; i < 4; i++)
    {
        ret.c[i].buttons = ~(current.c[i].buttons) & (last.c[i].buttons);
    }

    return ret;
}

struct controller_data get_keys_held()
{
    struct controller_data ret;

    /* Start with baseline */
    memcpy(&ret, &current, sizeof(current));

    /* Figure out which wasn't pressed last time and is now */
    for(int i = 0; i < 4; i++)
    {
        ret.c[i].buttons = (current.c[i].buttons) & (last.c[i].buttons);
    }

    return ret;
}

struct controller_data get_keys_pressed()
{
    return current;
}

int get_dpad_direction( int controller )
{
    /* Diagonals first because it could only be right angles otherwise */
    if( current.c[controller & 0x3].up && current.c[controller & 0x3].left ) 
    {
        return 3;
    }
    
    if( current.c[controller & 0x3].up && current.c[controller & 0x3].right ) 
    {
        return 1;
    }

    if( current.c[controller & 0x3].down && current.c[controller & 0x3].left ) 
    {
        return 5;
    }

    if( current.c[controller & 0x3].down && current.c[controller & 0x3].right ) 
    {
        return 7;
    }

    if( current.c[controller & 0x3].right )
    {
        return 0;
    }

    if( current.c[controller & 0x3].up )
    {
        return 2;
    }

    if( current.c[controller & 0x3].left )
    {
        return 4;
    }

    if( current.c[controller & 0x3].down )
    {
        return 6;
    }

    return -1;
}
