/**
 * @file interrupt.c
 * @brief Interrupt Controller
 * @ingroup interrupt
 */
#include <malloc.h>
#include "libdragon.h"
#include "regsinternal.h"

/**
 * @defgroup interrupt Interrupt Controller
 * @ingroup lowlevel
 *
 * @todo Need to allow unregistering of callbacks
 * @{
 */

/** @brief SP interrupt bit */
#define MI_INTR_SP 0x01
/** @brief SI interrupt bit */
#define MI_INTR_SI 0x02
/** @brief AI interrupt bit */
#define MI_INTR_AI 0x04
/** @brief VI interrupt bit */
#define MI_INTR_VI 0x08
/** @brief PI interrupt bit */
#define MI_INTR_PI 0x10
/** @brief DP interrupt bit */
#define MI_INTR_DP 0x20

/** @brief SP mask bit */
#define MI_MASK_SP 0x01
/** @brief SI mask bit */
#define MI_MASK_SI 0x02
/** @brief AI mask bit */
#define MI_MASK_AI 0x04
/** @brief VI mask bit */
#define MI_MASK_VI 0x08
/** @brief PI mask bit */
#define MI_MASK_PI 0x10
/** @brief DP mask bit */
#define MI_MASK_DP 0x20

/** @brief Clear SP mask */
#define MI_MASK_CLR_SP 0x0001
/** @brief Set SP mask */
#define MI_MASK_SET_SP 0x0002
/** @brief Clear SI mask */
#define MI_MASK_CLR_SI 0x0004
/** @brief Set SI mask */
#define MI_MASK_SET_SI 0x0008
/** @brief Clear AI mask */
#define MI_MASK_CLR_AI 0x0010
/** @brief Set AI mask */
#define MI_MASK_SET_AI 0x0020
/** @brief Clear VI mask */
#define MI_MASK_CLR_VI 0x0040
/** @brief Set VI mask */
#define MI_MASK_SET_VI 0x0080
/** @brief Clear PI mask */
#define MI_MASK_CLR_PI 0x0100
/** @brief Set PI mask */
#define MI_MASK_SET_PI 0x0200
/** @brief Clear DP mask */
#define MI_MASK_CLR_DP 0x0400
/** @brief Set DP mask */
#define MI_MASK_SET_DP 0x0800

/** @brief Bit to set to clear the PI interrupt */
#define PI_CLEAR_INTERRUPT ( 1 << 1 )

/**
 * @brief Structure of an interrupt callback
 */
typedef struct callback_link
{
    /** @brief Callback function */
    void (*callback)();
    /** @brief Pointer to next callback */
    struct callback_link * next;
} _callback_link;

/** @brief Static structure to address AI registers */
static volatile struct AI_regs_s * const AI_regs = (struct AI_regs_s *)0xa4500000;
/** @brief Static structure to address MI registers */
static volatile struct MI_regs_s * const MI_regs = (struct MI_regs_s *)0xa4300000;
/** @brief Static structure to address VI registers */
static volatile struct VI_regs_s * const VI_regs = (struct VI_regs_s *)0xa4400000;
/** @brief Static structure to address PI registers */
static volatile struct PI_regs_s * const PI_regs = (struct PI_regs_s *)0xa4600000;

/** @brief Linked list of AI callbacks */
struct callback_link * AI_callback = 0;
/** @brief Linked list of VI callbacks */
struct callback_link * VI_callback = 0;
/** @brief Linked list of PI callbacks */
struct callback_link * PI_callback = 0;
/** @brief Linked list of DP callbacks */
struct callback_link * DP_callback = 0;
/** @brief Linked list of TI callbacks */
struct callback_link * TI_callback = 0;

/** 
 * @brief Call each callback in a linked list of callbacks
 *
 * @param[in] head
 *            Pointer to the head of a callback linke list
 */
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

/**
 * @brief Handle an MI interrupt
 *
 * @note This function handles most of the interrupts on the system as
 *       they come through the MI.
 */
void __MI_handler(void)
{
    unsigned long status = MI_regs->intr & MI_regs->mask;

    if( status & MI_INTR_SP )
    {
        /** @todo handle SP */
    }

    if( status & MI_INTR_SI )
    {
        /** @todo handle SI */
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

/**
 * @brief Handle a timer interrupt
 */
void __TI_handler(void)
{
	/* timer int cleared in int handler */
    __call_callback(TI_callback);
}

/**
 * @brief Add a callback to a linked list of callbacks
 *
 * @param[in,out] head
 *                Pointer to the head of a linked list to add to
 * @param[in]     callback
 *                Function to call when executing callbacks in this linked list
 */
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

/**
 * @brief Register an AI callback
 *
 * @param[in] callback
 *            Function to call when an AI interrupt occurs
 */
void register_AI_handler( void (*callback)() )
{
    __register_callback(&AI_callback,callback);
}

/**
 * @brief Register a VI callback
 *
 * @param[in] callback
 *            Function to call when a VI interrupt occurs
 */
void register_VI_handler( void (*callback)() )
{
    __register_callback(&VI_callback,callback);
}

/**
 * @brief Register a PI callback
 *
 * @param[in] callback
 *            Function to call when a PI interrupt occurs
 */
void register_PI_handler( void (*callback)() )
{
    __register_callback(&PI_callback,callback);
}

/**
 * @brief Register a DP callback
 *
 * @param[in] callback
 *            Function to call when a DP interrupt occurs
 */
void register_DP_handler( void (*callback)() )
{
    __register_callback(&DP_callback,callback);
}

/**
 * @brief Register a TI callback
 *
 * @param[in] callback
 *            Function to call when a TI interrupt occurs
 */
void register_TI_handler( void (*callback)() )
{
    __register_callback(&TI_callback,callback);
}

/**
 * @brief Enable or disable the AI interrupt
 *
 * @param[in] active
 *            Flag to specify whether the AI interupt should be active
 */
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

/**
 * @brief Enable or disable the VI interrupt
 *
 * @param[in] active
 *            Flag to specify whether the VI interupt should be active
 * @param[in] line
 *            The vertical line that causes this interrupt to fire.  Ignored
 *            when setting the interrupt inactive
 */
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

/**
 * @brief Enable or disable the PI interrupt
 *
 * @param[in] active
 *            Flag to specify whether the PI interupt should be active
 */
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

/**
 * @brief Enable or disable the DP interrupt
 *
 * @param[in] active
 *            Flag to specify whether the DP interupt should be active
 */
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

/**
 * @brief Enable or disable the MI interrupt
 *
 * @param[in] active
 *            Flag to specify whether the VI interupt should be active
 * @param[in] clear
 *            Flag to specify if the interrupt mask should be cleared
 */
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

/** @} */
