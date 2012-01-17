#include <malloc.h>
#include "libdragon.h"
#include "regsinternal.h"

#define MI_INTR_SP 0x01
#define MI_INTR_SI 0x02
#define MI_INTR_AI 0x04
#define MI_INTR_VI 0x08
#define MI_INTR_PI 0x10
#define MI_INTR_DP 0x20

#define MI_MASK_SP 0x01
#define MI_MASK_SI 0x02
#define MI_MASK_AI 0x04
#define MI_MASK_VI 0x08
#define MI_MASK_PI 0x10
#define MI_MASK_DP 0x20

#define MI_MASK_CLR_SP 0x0001
#define MI_MASK_SET_SP 0x0002
#define MI_MASK_CLR_SI 0x0004
#define MI_MASK_SET_SI 0x0008
#define MI_MASK_CLR_AI 0x0010
#define MI_MASK_SET_AI 0x0020
#define MI_MASK_CLR_VI 0x0040
#define MI_MASK_SET_VI 0x0080
#define MI_MASK_CLR_PI 0x0100
#define MI_MASK_SET_PI 0x0200
#define MI_MASK_CLR_DP 0x0400
#define MI_MASK_SET_DP 0x0800

#define PI_CLEAR_INTERRUPT ( 1 << 1 )

/** @todo Need to allow unregistering of callbacks */

typedef struct callback_link
{
    void (*callback)();
    struct callback_link * next;
} _callback_link;

/* Register structures */
static volatile struct AI_regs_s * const AI_regs = (struct AI_regs_s *)0xa4500000;
static volatile struct MI_regs_s * const MI_regs = (struct MI_regs_s *)0xa4300000;
static volatile struct VI_regs_s * const VI_regs = (struct VI_regs_s *)0xa4400000;
static volatile struct PI_regs_s * const PI_regs = (struct PI_regs_s *)0xa4600000;

/* Callback linked lists */
struct callback_link * AI_callback = 0;
struct callback_link * VI_callback = 0;
struct callback_link * PI_callback = 0;
struct callback_link * DP_callback = 0;
struct callback_link * TI_callback = 0;

static void __call_callback( struct callback_link * head )
{
    /* Call each registered callback */
    while( head )
    {
        if( head->callback )
        {
        	head->callback();
        }

        /* Go to next */
	    head=head->next;
    }
}

void __MI_handler(void)
{
    unsigned long status = MI_regs->intr & MI_regs->mask;

    if( status & MI_INTR_SP )
    {
        /* TODO: handle SP */
    }

    if( status & MI_INTR_SI )
    {
        /* TODO: handle SI */
    }

    if( status & MI_INTR_AI )
    {
        /* Clear interrupt */
    	AI_regs->status=0;

	    __call_callback(AI_callback);
    }

    if( status & MI_INTR_VI )
    {
        /* Clear interrupt */
    	VI_regs->cur_line=VI_regs->cur_line;

    	__call_callback(VI_callback);
    }

    if( status & MI_INTR_PI )
    {
        /* Clear interrupt */
        PI_regs->status=PI_CLEAR_INTERRUPT;

        __call_callback(PI_callback);
    }

    if( status & MI_INTR_DP )
    {
        /* Clear interrupt */
        MI_regs->mode=0x0800;

        __call_callback(DP_callback);
    }
}

void __TI_handler(void)
{
	/* timer int cleared in int handler */
    __call_callback(TI_callback);
}

static void __register_callback( struct callback_link ** head, void (*callback)() )
{
    if( head )
    {
        /* Add to beginning of linked list */
        struct callback_link * next=*head;
        (*head) = malloc(sizeof(struct callback_link));

        if( *head )
        {
            (*head)->next=next;
            (*head)->callback=callback;
        }
    }
}

void register_AI_handler( void (*callback)() )
{
    __register_callback(&AI_callback,callback);
}

void register_VI_handler( void (*callback)() )
{
    __register_callback(&VI_callback,callback);
}

void register_PI_handler( void (*callback)() )
{
    __register_callback(&PI_callback,callback);
}

void register_DP_handler( void (*callback)() )
{
    __register_callback(&DP_callback,callback);
}

void register_TI_handler( void (*callback)() )
{
    __register_callback(&TI_callback,callback);
}

void set_AI_interrupt(int active)
{
    if( active )
    {
        MI_regs->mask=MI_MASK_SET_AI;
    }
    else
    {
        MI_regs->mask=MI_MASK_CLR_AI;
    }
}

void set_VI_interrupt(int active, unsigned long line)
{
    if( active )
    {
    	MI_regs->mask=MI_MASK_SET_VI;
	    VI_regs->v_int=line;
    }
    else
    {
        MI_regs->mask=MI_MASK_CLR_VI;
    }
}

void set_PI_interrupt(int active)
{
    if ( active )
    {
        MI_regs->mask=MI_MASK_SET_PI;
    }
    else
    {
        MI_regs->mask=MI_MASK_CLR_PI;
    }
}

void set_DP_interrupt(int active)
{
    if( active )
    {
        MI_regs->mask=MI_MASK_SET_DP;
    }
    else
    {
        MI_regs->mask=MI_MASK_CLR_DP;
    }
}

void set_MI_interrupt(int active, int clear)
{
    if( clear )
    {
        MI_regs->mask=MI_MASK_CLR_SP|MI_MASK_CLR_SI|MI_MASK_CLR_AI|MI_MASK_CLR_VI|MI_MASK_CLR_PI|MI_MASK_CLR_PI|MI_MASK_CLR_DP;
    }

    if( active )
    {
        asm("\tmfc0 $8,$12\n\tori $8,0x401\n\tmtc0 $8,$12\n\tnop":::"$8");
    }
    else
    {
        asm("\tmfc0 $8,$12\n\tla $9,~0x400\n\tand $8,$9\n\tmtc0 $8,$12\n\tnop":::"$8","$9");
    }
}

