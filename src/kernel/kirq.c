#include "kirq.h"
#include "kernel.h"
#include "kernel_internal.h"
#include "interrupt.h"

extern volatile int64_t __interrupt_counter;

kcond_t __kirq_cond_sp;
kcond_t __kirq_cond_dp;
kcond_t __kirq_cond_si;
kcond_t __kirq_cond_ai;
kcond_t __kirq_cond_vi;
kcond_t __kirq_cond_pi;

void __kirq_init(void)
{
    kcond_init(&__kirq_cond_sp);
    kcond_init(&__kirq_cond_dp);
    kcond_init(&__kirq_cond_si);
    kcond_init(&__kirq_cond_ai);
    kcond_init(&__kirq_cond_vi);
    kcond_init(&__kirq_cond_pi);
}


kirq_wait_t kirq_begin_wait_sp(void)
{
    return (kirq_wait_t){ 
        .counter = __interrupt_counter,  
        .cond = &__kirq_cond_sp,
    };
}

kirq_wait_t kirq_begin_wait_dp(void)
{
    return (kirq_wait_t){ 
        .counter = __interrupt_counter,  
        .cond = &__kirq_cond_dp,
    };
}

kirq_wait_t kirq_begin_wait_si(void)
{
    return (kirq_wait_t){ 
        .counter = __interrupt_counter,  
        .cond = &__kirq_cond_si,
    };
}

kirq_wait_t kirq_begin_wait_ai(void)
{
    return (kirq_wait_t){ 
        .counter = __interrupt_counter,  
        .cond = &__kirq_cond_ai,
    };
}

kirq_wait_t kirq_begin_wait_vi(void)
{
    return (kirq_wait_t){ 
        .counter = __interrupt_counter,  
        .cond = &__kirq_cond_vi,
    };
}

kirq_wait_t kirq_begin_wait_pi(void)
{
    return (kirq_wait_t){ 
        .counter = __interrupt_counter,  
        .cond = &__kirq_cond_pi,
    };
}

void kirq_wait(kirq_wait_t* wait)
{
    if (!__kernel) return;

    disable_interrupts();    
    if (wait->counter == __interrupt_counter)
        kcond_wait(wait->cond, NULL);
    wait->counter = __interrupt_counter;
    enable_interrupts();
}
