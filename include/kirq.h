#ifndef LIBDRAGON_KERNEL_KIRQ_H
#define LIBDRAGON_KERNEL_KIRQ_H

#include <stdint.h>

typedef struct kcond_s kcond_t;

typedef struct {
    int64_t counter;
    kcond_t *cond;
} kirq_wait_t;

kirq_wait_t kirq_begin_wait_sp(void);
kirq_wait_t kirq_begin_wait_dp(void);
kirq_wait_t kirq_begin_wait_si(void);
kirq_wait_t kirq_begin_wait_ai(void);
kirq_wait_t kirq_begin_wait_vi(void);
kirq_wait_t kirq_begin_wait_pi(void);

void kirq_wait(kirq_wait_t *wait);

#endif
