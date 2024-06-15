/**
 * @file interrupt.c
 * @brief Interrupt Controller
 * @ingroup interrupt
 */
#include <malloc.h>
#include "libdragon.h"
#include "regsinternal.h"

/** @brief Bit to set to clear the PI interrupt */
#define PI_CLEAR_INTERRUPT 0x02
/** @brief Bit to set to clear the SI interrupt */
#define SI_CLEAR_INTERRUPT 0
/** @brief Bit to set to clear the SP interrupt */
#define SP_CLEAR_INTERRUPT 0x08
/** @brief Bit to set to clear the DP interrupt */
#define DP_CLEAR_INTERRUPT 0x0800
/** @brief Bit to set to clear the AI interrupt */
#define AI_CLEAR_INTERRUPT 0

/** @brief Number of nested disable interrupt calls
 *
 * This will represent the number of disable interrupt calls made on the system.
 * If this is set to 0, interrupts are enabled.  A number higher than 0 represents
 * that many disable calls that were nested, and consequently the number of
 * interrupt enable calls that need to be made to re-enable interrupts.  A negative
 * number means that the interrupt system hasn't been initialized yet.
 */
static int __interrupt_depth = -1;

/** @brief Value of the status register at the moment interrupts
 *         got disabled.
 */
static int __interrupt_sr = 0;

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
/** @brief Static structure to address VI registers */
static volatile struct VI_regs_s * const VI_regs = (struct VI_regs_s *)0xa4400000;
/** @brief Static structure to address PI registers */
static volatile struct PI_regs_s * const PI_regs = (struct PI_regs_s *)0xa4600000;
/** @brief Static structure to address SI registers */
static volatile struct SI_regs_s * const SI_regs = (struct SI_regs_s *)0xa4800000;
/** @brief Static structure to address SP registers */
static volatile struct SP_regs_s * const SP_regs = (struct SP_regs_s *)0xa4040000;

/** @brief Linked list of AI callbacks */
struct callback_link * AI_callback = 0;
/** @brief Linked list of VI callbacks */
struct callback_link * VI_callback = 0;
/** @brief Linked list of PI callbacks */
struct callback_link * PI_callback = 0;
/** @brief Linked list of DP callbacks */
struct callback_link * DP_callback = 0;
/** @brief Linked list of SI callbacks */
struct callback_link * SI_callback = 0;
/** @brief Linked list of SP callbacks */
struct callback_link * SP_callback = 0;
/** @brief Linked list of TI callbacks */
struct callback_link * TI_callback = 0;
/** @brief Linked list of CART callbacks */
struct callback_link * CART_callback = 0;

/** @brief Linked list of BB FLASH callbacks */
struct callback_link * BB_FLASH_callback = 0;
/** @brief Linked list of BB AES callbacks */
struct callback_link * BB_AES_callback = 0;
/** @brief Linked list of BB IDE callbacks */
struct callback_link * BB_IDE_callback = 0;
/** @brief Linked list of BB PI ERR callbacks */
struct callback_link * BB_PI_ERR_callback = 0;
/** @brief Linked list of BB USB0 callbacks */
struct callback_link * BB_USB0_callback = 0;
/** @brief Linked list of BB USB1 callbacks */
struct callback_link * BB_USB1_callback = 0;
/** @brief Linked list of BB BTN callbacks */
struct callback_link * BB_BTN_callback = 0;
/** @brief Linked list of BB MD callbacks */
struct callback_link * BB_MD_callback = 0;

/** @brief Maximum number of reset handlers that can be registered. */
#define MAX_RESET_HANDLERS 4

/** @brief Pre-NMI exception handlers */
static void (*__prenmi_handlers[MAX_RESET_HANDLERS])(void);
/** @brief Tick at which the pre-NMI was triggered */
static uint32_t __prenmi_tick;

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
        struct callback_link *next = *head;
        (*head) = malloc(sizeof(struct callback_link));

        if( *head )
        {
            (*head)->next=next;
            (*head)->callback=callback;
        }
    }
}

/**
 * @brief Remove a callback from a linked list of callbacks
 *
 * @param[in,out] head
 *                Pointer to the head of a linked list to remove from
 * @param[in]     callback
 *                Function to search for and remove from callback list
 */
static void __unregister_callback( struct callback_link ** head, void (*callback)() )
{
    if( head )
    {
        /* Try to find callback this matches */
        struct callback_link *last = 0;
        struct callback_link *cur  = *head;

        while( cur )
        {
            if( cur->callback == callback )
            {
                /* We found it!  Try to remove it from the list */
                if( last )
                {
                    /* This is somewhere in the linked list */
                    last->next = cur->next;
                }
                else
                {
                    /* This is the first node */
                    *head = cur->next;
                }

                /* Free memory */
                free( cur );

                /* Exit early */
                break;
            }

            /* Go to next entry */
            last = cur;
            cur = cur->next;
        }
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
    unsigned long status = *MI_INTERRUPT & *MI_MASK;

    if( status & MI_INTERRUPT_SP )
    {
        /* Clear interrupt */
        SP_regs->status=SP_CLEAR_INTERRUPT;

        __call_callback(SP_callback);
    }

    if( status & MI_INTERRUPT_SI )
    {
        /* Clear interrupt */
        SI_regs->status=SI_CLEAR_INTERRUPT;

        __call_callback(SI_callback);
    }

    if( status & MI_INTERRUPT_AI )
    {
        /* Clear interrupt */
    	AI_regs->status=AI_CLEAR_INTERRUPT;

	    __call_callback(AI_callback);
    }

    if( status & MI_INTERRUPT_VI )
    {
        /* Clear interrupt */
    	VI_regs->cur_line=VI_regs->cur_line;

    	__call_callback(VI_callback);
    }

    if( status & MI_INTERRUPT_PI )
    {
        /* Clear interrupt */
        PI_regs->status=PI_CLEAR_INTERRUPT;

        __call_callback(PI_callback);
    }

    if( status & MI_INTERRUPT_DP )
    {
        /* Clear interrupt */
        *MI_MODE = MI_WMODE_CLR_DPINT;

        __call_callback(DP_callback);
    }
}

#define PI_BB_NAND_CTRL                     ((volatile uint32_t*)0xA4600048)  ///< BB NAND control register
#define PI_BB_AES_CTRL                      ((volatile uint32_t*)0xA4600050)  ///< BB AES control register

/**
 * @brief Handle the extra MI interrupts in iQue
 */
void __MI_BB_handler(void)
{
    unsigned long status = *MI_BB_INTERRUPT & *MI_BB_MASK;

    if( status & MI_BB_INTERRUPT_FLASH )
    {
        /* Clear interrupt */
        *PI_BB_NAND_CTRL = 0;

        __call_callback(BB_FLASH_callback);
    }

    if( status & MI_BB_INTERRUPT_AES )
    {
        /* Clear interrupt */
        *PI_BB_AES_CTRL = 0;

        __call_callback(BB_AES_callback);
    }

    if( status & MI_BB_INTERRUPT_IDE )
    {
        assertf(0, "Unhandled iQue interrupt: IDE");

        __call_callback(BB_IDE_callback);
    }

    if( status & MI_BB_INTERRUPT_PI_ERR )
    {
        assertf(0, "Unhandled iQue interrupt: PI_ERR");

        __call_callback(BB_PI_ERR_callback);
    }

    if( status & MI_BB_INTERRUPT_USB0 )
    {
        /* USB interrupt handling requires clearing the interrupt signal only
           after some handling has begun, so we do not clear it here. It is
           therefore a requirement to either leave this interrupt disabled or
           have a handler ready to act on it. */
        assertf(BB_USB0_callback != NULL && BB_USB0_callback->callback != NULL,
                "No handler was registered for iQue USB0 interrupt despite being enabled");

        __call_callback(BB_USB0_callback);
    }

    if( status & MI_BB_INTERRUPT_USB1 )
    {
        /* USB interrupt handling requires clearing the interrupt signal only
           after some handling has begun, so we do not clear it here. It is
           therefore a requirement to either leave this interrupt disabled or
           have a handler ready to act on it. */
        assertf(BB_USB1_callback != NULL && BB_USB1_callback->callback != NULL,
                "No handler was registered for iQue USB1 interrupt despite being enabled");

        __call_callback(BB_USB1_callback);
    }

    if( status & MI_BB_INTERRUPT_BTN )
    {
        assertf(0, "Unhandled iQue interrupt: BTN");

        __call_callback(BB_BTN_callback);
    }

    if( status & MI_BB_INTERRUPT_MD )
    {
        /* Clear interrupt */
        *MI_BB_INTERRUPT = MI_BB_INTERRUPT_MD;

        __call_callback(BB_MD_callback);
    }
}


/**
 * @brief Handle a timer interrupt
 */
void __TI_handler(void)
{
	/* NOTE: the timer interrupt is already acknowledged in inthandler.S */
    __call_callback(TI_callback);
}

/**
 * @brief Handle a CART interrupt
 */
void __CART_handler(void)
{
    /* On iQue, this is actually second half of MI handler */
    if (sys_bbplayer()) {
        __MI_BB_handler();
        return;
    }

    /* Call the registered callbacks */
    __call_callback(CART_callback);

    #ifndef NDEBUG
     /* CART interrupts must be acknowledged by handlers. If the handler fails
       to do so, the console freezes because the interrupt will retrigger
       continuously. Since a freeze is always bad for debugging, try to 
       detect it, and show a proper assertion screen. */
    static int last_cart_interrupt_count = 0;
    if (!(C0_CAUSE() & C0_INTERRUPT_CART))
        last_cart_interrupt_count = 0;
    else
        assertf(++last_cart_interrupt_count < 128, "CART interrupt deadlock: a CART interrupt is continuously triggering, with no ack");
    #endif
}


/**
 * @brief Handle a RESET (pre-NMI) interrupt.
 * 
 * Calls the handlers registered by #register_RESET_handler.
 */
void __RESET_handler( void )
{
	/* This function will be called many times because there is no way
	   to acknowledge the pre-NMI interrupt. So make sure it does nothing
	   after the first call. */
	if (__prenmi_tick) return;

	/* Store the tick at which we saw the exception. Make sure
	 * we never store 0 as we use that for "no reset happened". */
	__prenmi_tick = TICKS_READ() | 1;

	/* Call the registered handlers. */
	for (int i=0;i<MAX_RESET_HANDLERS;i++)
	{
		if (__prenmi_handlers[i])
			__prenmi_handlers[i]();
	}
}

void register_AI_handler( void (*callback)() )
{
    __register_callback(&AI_callback,callback);
}

void unregister_AI_handler( void (*callback)() )
{
    __unregister_callback(&AI_callback,callback);
}

void register_VI_handler( void (*callback)() )
{
    __register_callback(&VI_callback,callback);
}

void unregister_VI_handler( void (*callback)() )
{
    __unregister_callback(&VI_callback,callback);
}

void register_PI_handler( void (*callback)() )
{
    __register_callback(&PI_callback,callback);
}

void unregister_PI_handler( void (*callback)() )
{
    __unregister_callback(&PI_callback,callback);
}

void register_DP_handler( void (*callback)() )
{
    __register_callback(&DP_callback,callback);
}

void unregister_DP_handler( void (*callback)() )
{
    __unregister_callback(&DP_callback,callback);
}

void register_SI_handler( void (*callback)() )
{
    __register_callback(&SI_callback,callback);
}

void unregister_SI_handler( void (*callback)() )
{
    __unregister_callback(&SI_callback,callback);
}

void register_SP_handler( void (*callback)() )
{
    __register_callback(&SP_callback,callback);
}

void unregister_SP_handler( void (*callback)() )
{
    __unregister_callback(&SP_callback,callback);
}


void register_TI_handler( void (*callback)() )
{
    __register_callback(&TI_callback,callback);
}

void unregister_TI_handler( void (*callback)() )
{
    __unregister_callback(&TI_callback,callback);
}

void register_CART_handler( void (*callback)() )
{
    __register_callback(&CART_callback,callback);
}

void unregister_CART_handler( void (*callback)() )
{
    __unregister_callback(&CART_callback,callback);
}

void register_RESET_handler( void (*callback)() )
{
	for (int i=0;i<MAX_RESET_HANDLERS;i++)
	{		
		if (!__prenmi_handlers[i])
		{
			__prenmi_handlers[i] = callback;
			return;
		}
	}
	assertf(0, "Too many pre-NMI handlers\n");
}

void unregister_RESET_handler( void (*callback)() )
{
    for (int i=0;i<MAX_RESET_HANDLERS;i++)
    {		
        if (__prenmi_handlers[i] == callback)
        {
            __prenmi_handlers[i] = NULL;
            return;
        }
    }
    assertf(0, "Reset handler not found\n");
}

void register_BB_FLASH_handler( void (*callback)() )
{
    __register_callback(&BB_FLASH_callback,callback);
}

void unregister_BB_FLASH_handler( void (*callback)() )
{
    __unregister_callback(&BB_FLASH_callback,callback);
}

void register_BB_AES_handler( void (*callback)() )
{
    __register_callback(&BB_AES_callback,callback);
}

void unregister_BB_AES_handler( void (*callback)() )
{
    __unregister_callback(&BB_AES_callback,callback);
}

void register_BB_IDE_handler( void (*callback)() )
{
    __register_callback(&BB_IDE_callback,callback);
}

void unregister_BB_IDE_handler( void (*callback)() )
{
    __unregister_callback(&BB_IDE_callback,callback);
}

void register_BB_PI_ERR_handler( void (*callback)() )
{
    __register_callback(&BB_PI_ERR_callback,callback);
}

void unregister_BB_PI_ERR_handler( void (*callback)() )
{
    __unregister_callback(&BB_PI_ERR_callback,callback);
}

void register_BB_USB0_handler( void (*callback)() )
{
    __register_callback(&BB_USB0_callback,callback);
}

void unregister_BB_USB0_handler( void (*callback)() )
{
    __unregister_callback(&BB_USB0_callback,callback);
}

void register_BB_USB1_handler( void (*callback)() )
{
    __register_callback(&BB_USB1_callback,callback);
}

void unregister_BB_USB1_handler( void (*callback)() )
{
    __unregister_callback(&BB_USB1_callback,callback);
}

void register_BB_BTN_handler( void (*callback)() )
{
    __register_callback(&BB_BTN_callback,callback);
}

void unregister_BB_BTN_handler( void (*callback)() )
{
    __unregister_callback(&BB_BTN_callback,callback);
}

void register_BB_MD_handler( void (*callback)() )
{
    __register_callback(&BB_MD_callback,callback);
}

void unregister_BB_MD_handler( void (*callback)() )
{
    __unregister_callback(&BB_MD_callback,callback);
}

void set_AI_interrupt(int active)
{
    *MI_MASK = active ? MI_WMASK_SET_AI : MI_WMASK_CLR_AI;
}

void set_VI_interrupt(int active, unsigned long line)
{
    if( active )
    {
    	*MI_MASK = MI_WMASK_SET_VI;
	    VI_regs->v_int=line;
    }
    else
    {
        *MI_MASK = MI_WMASK_CLR_VI;
    }
}

void set_PI_interrupt(int active)
{
    *MI_MASK = active ? MI_WMASK_SET_PI : MI_WMASK_CLR_PI;
}

void set_DP_interrupt(int active)
{
    *MI_MASK = active ? MI_WMASK_SET_DP : MI_WMASK_CLR_DP;
}

void set_SI_interrupt(int active)
{
    *MI_MASK = active ? MI_WMASK_SET_SI : MI_WMASK_CLR_SI;
}

void set_SP_interrupt(int active)
{
    *MI_MASK = active ? MI_WMASK_SET_SP : MI_WMASK_CLR_SP;
}

void set_TI_interrupt(int active)
{
    if( active )
    {
        C0_WRITE_STATUS(C0_STATUS() | C0_INTERRUPT_TIMER);
    }
    else
    {
        C0_WRITE_STATUS(C0_STATUS() & ~C0_INTERRUPT_TIMER);
    }
}

void set_CART_interrupt(int active)
{
    if( active )
    {
        C0_WRITE_STATUS(C0_STATUS() | C0_INTERRUPT_CART);
    }
    else
    {
        C0_WRITE_STATUS(C0_STATUS() & ~C0_INTERRUPT_CART);
    }
}

void set_RESET_interrupt(int active)
{
    if( active )
    {
        C0_WRITE_STATUS(C0_STATUS() | C0_INTERRUPT_PRENMI);
    }
    else
    {
        C0_WRITE_STATUS(C0_STATUS() & ~C0_INTERRUPT_PRENMI);
    }
}

void set_BB_FLASH_interrupt(int active)
{
    *MI_BB_MASK = active ? MI_BB_WMASK_SET_FLASH : MI_BB_WMASK_CLR_FLASH;
}

void set_BB_AES_interrupt(int active)
{
    *MI_BB_MASK = active ? MI_BB_WMASK_SET_AES : MI_BB_WMASK_CLR_AES;
}

void set_BB_IDE_interrupt(int active)
{
    *MI_BB_MASK = active ? MI_BB_WMASK_SET_IDE : MI_BB_WMASK_CLR_IDE;
}

void set_BB_PI_ERR_interrupt(int active)
{
    *MI_BB_MASK = active ? MI_BB_WMASK_SET_PI_ERR : MI_BB_WMASK_CLR_PI_ERR;
}

void set_BB_USB0_interrupt(int active)
{
    *MI_BB_MASK = active ? MI_BB_WMASK_SET_USB0 : MI_BB_WMASK_CLR_USB0;
}

void set_BB_USB1_interrupt(int active)
{
    *MI_BB_MASK = active ? MI_BB_WMASK_SET_USB1 : MI_BB_WMASK_CLR_USB1;
}

void set_BB_BTN_interrupt(int active)
{
    *MI_BB_MASK = active ? MI_BB_WMASK_SET_BTN : MI_BB_WMASK_CLR_BTN;
}

void set_BB_MD_interrupt(int active)
{
    *MI_BB_MASK = active ? MI_BB_WMASK_SET_MD : MI_BB_WMASK_CLR_MD;
}

/**
 * @brief Initialize the interrupt controller
 */
__attribute__((constructor)) void __init_interrupts()
{
    /* Make sure that we aren't initializing interrupts when they are already enabled */
    if( __interrupt_depth < 0 )
    {
        /* Clear and mask all interrupts on the system so we start with a clean slate */
        *MI_MASK = MI_WMASK_CLR_SP | MI_WMASK_CLR_SI | MI_WMASK_CLR_AI | MI_WMASK_CLR_VI | MI_WMASK_CLR_PI | MI_WMASK_CLR_DP;

        /* On iQue, also disable extra interrupts. Activate power button interrupt. */
        if (sys_bbplayer())
            *MI_BB_MASK = MI_BB_WMASK_CLR_FLASH | MI_BB_WMASK_CLR_AES | MI_BB_WMASK_CLR_IDE | 
                          MI_BB_WMASK_CLR_PI_ERR | MI_BB_WMASK_CLR_USB0 | MI_BB_WMASK_CLR_USB1 | 
                          MI_BB_WMASK_SET_BTN | MI_BB_WMASK_CLR_MD;

        /* Set that we are enabled */
        __interrupt_depth = 0;

        /* Enable interrupts systemwide. We set the global interrupt enable,
           and then specifically enable RCP interrupts. */
        uint32_t sr = C0_STATUS();
        C0_WRITE_STATUS(sr | C0_STATUS_IE | C0_INTERRUPT_RCP | C0_INTERRUPT_PRENMI);
    }
}

void disable_interrupts()
{
    /* Don't do anything if we haven't initialized */
    if( __interrupt_depth < 0 ) { return; }

    if( __interrupt_depth == 0 )
    {
        /* We must disable the interrupts now. */
        uint32_t sr = C0_STATUS();
        C0_WRITE_STATUS(sr & ~C0_STATUS_IE);

        /* Save the original SR value away, so that we know if
           interrupts were enabled and whether to restore them.
           NOTE: this memory write must happen now that interrupts
           are disabled, otherwise it could cause a race condition
           because an interrupt could trigger and overwrite it.
           So put an explicit barrier. */
        MEMORY_BARRIER();
        __interrupt_sr = sr;
    }

    /* Ensure that we remember nesting levels */
    __interrupt_depth++;
}

void enable_interrupts()
{
    /* Don't do anything if we aren't initialized */
    if( __interrupt_depth < 0 ) { return; }

    /* Check that we're not calling enable_interrupts() more than expected */
    assertf(__interrupt_depth > 0, "unbalanced enable_interrupts() call");

    /* Decrement the nesting level now that we are enabling interrupts */
    __interrupt_depth--;

    if( __interrupt_depth == 0 )
    {
        /* Restore the interrupt state that was active when interrupts got
           disabled.
           This is important to be done this way, as opposed to simply or-ing
           in the IE bit (| C0_STATUS_IE), because, within an interrupt handler,
           we don't want interrupts enabled, or we would allow reentrant
           interrupts which are not supported. */
        C0_WRITE_STATUS(C0_STATUS() | (__interrupt_sr & C0_STATUS_IE));
    }
}

interrupt_state_t get_interrupts_state()
{
    if( __interrupt_depth < 0 )
    {
        return INTERRUPTS_UNINITIALIZED;
    }
    else if( __interrupt_depth == 0 )
    {
        return INTERRUPTS_ENABLED;
    }
    else
    {
        return INTERRUPTS_DISABLED;
    }
}


uint32_t exception_reset_time( void )
{
	if (!__prenmi_tick) return 0;
	return TICKS_SINCE(__prenmi_tick);
}
