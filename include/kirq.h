#ifndef LIBDRAGON_KERNEL_KIRQ_H
#define LIBDRAGON_KERNEL_KIRQ_H

void kirq_wait_sp(void);
void kirq_wait_dp(void);
void kirq_wait_si(void);
void kirq_wait_ai(void);
void kirq_wait_vi(void);
void kirq_wait_pi(void);

#endif
