/**
 * @file ktls.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */

#include "ktls_internal.h"

/** @brief Current thread TLS pointer */
void *__th_cur_tp;

void __ktls_init(void)
{
    // Set the initial thread pointer to the TLS area in the data section,
    // which will be used for the main thread
    __th_cur_tp = __tls_base + TP_OFFSET;
}

void __ktls_close(void)
{
    __th_cur_tp = KERNEL_TP_INVALID;
}
