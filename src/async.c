#include "async.h"
#include "async_.h"
#include "mi.h"
#include "n64sys.h"
#include "interrupt.h"
#include "cop0.h"
#include "debug.h"


extern void __TI_handler(void);

extern int __interrupt_depth;

void async_init(void)
{
    disable_interrupts();
}

void async_close(void)
{
    enable_interrupts();
}

bool async_poll_timer(uint32_t cause) {
    assertf(get_interrupts_state() == INTERRUPTS_DISABLED, "async routines not initialized");
    if (cause & C0_INTERRUPT_TIMER)
    {
        // Acknowledge timer interrupt by writing to C0_COMPARE.
        // This is necessary to clear the pending bit in CAUSE.
        uint32_t compare = C0_COMPARE();
        C0_WRITE_COMPARE(compare);
        __TI_handler();
        return true;
    }

    return false;
}

void async_schedule(async (*task)(void *st), void *initial_state)
{
    assertf(get_interrupts_state() == INTERRUPTS_DISABLED, "async routines not initialized");
    do {
        uint32_t cause = C0_CAUSE();
        async_poll_timer(cause);
    } while (task && task(initial_state) != ASYNC_DONE);
}
