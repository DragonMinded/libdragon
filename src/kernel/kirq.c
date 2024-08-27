#include "kernel.h"
#include "kernel_internal.h"

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

void kirq_wait_sp(void)
{
    if (__kernel)
        kcond_wait(&__kirq_cond_sp, NULL);
}

void kirq_wait_dp(void)
{
    if (__kernel)
        kcond_wait(&__kirq_cond_dp, NULL);
}

void kirq_wait_si(void)
{
    if (__kernel)
        kcond_wait(&__kirq_cond_si, NULL);
}

void kirq_wait_ai(void)
{
    if (__kernel)
        kcond_wait(&__kirq_cond_ai, NULL);
}

void kirq_wait_vi(void)
{
    if (__kernel)
        kcond_wait(&__kirq_cond_vi, NULL);
}

void kirq_wait_pi(void)
{
    if (__kernel)
        kcond_wait(&__kirq_cond_pi, NULL);
}
