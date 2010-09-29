#include "n64sys.h"

volatile unsigned long get_ticks(void)
{
    unsigned long count;
    // reg $9 on COP0 is count
    asm("\tmfc0 %0,$9\n\tnop":"=r"(count));

    return count;
}

volatile unsigned long get_ticks_ms(void)
{
    unsigned long count;
    // reg $9 on COP0 is count
    asm("\tmfc0 %0,$9\n\tnop":"=r"(count));

    return count / (COUNTS_PER_SECOND / 1000);
}

void wait_ticks( unsigned long wait )
{
    unsigned int stop = wait + get_ticks();

    while( stop > get_ticks() );
}

void wait_ms( unsigned long wait )
{
    unsigned int stop = wait + get_ticks_ms();

    while( stop > get_ticks_ms() );
}

#define cache_op(op) \
    addr=(void*)(((unsigned long)addr)&(~3));\
    for (;length>0;length-=4,addr+=4) \
	asm ("\tcache %0,(%1)\n"::"i" (op), "r" (addr))

void data_cache_writeback(volatile void * addr, unsigned long length)
{
    cache_op(0x19);
}
void data_cache_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(0x11);
}
void data_cache_writeback_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(0x15);
}
void inst_cache_writeback(volatile void * addr, unsigned long length)
{
    cache_op(0x18);
}
void inst_cache_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(0x10);
}
void inst_cache_writeback_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(0x14);
}

void disable_interrupts()
{
	asm("\tmfc0 $8,$12\n\tla $9,~1\n\tand $8,$9\n\tmtc0 $8,$12\n\tnop":::"$8","$9");
}

void enable_interrupts()
{
	asm("\tmfc0 $8,$12\n\tori $8,1\n\tmtc0 $8,$12\n\tnop":::"$8");
}
