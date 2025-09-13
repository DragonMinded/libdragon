/**
 * @file ktls.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */

#include "ktls_internal.h"

/** Initialize TLS pointer to the tls_base area in RAM */
void *th_cur_tp = __tls_base + TP_OFFSET;

/** @brief Current thread TLS pointer */
void *__th_cur_tp;
