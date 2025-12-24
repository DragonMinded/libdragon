/**
 * @file accounting.c
 * @brief Internal bucket-based time accounting (see accounting_internal.h)
 */
#include "accounting_internal.h"
#include "n64sys.h"
#include "cop0.h"
#include "debug.h"
#include "interrupt_internal.h"

int8_t __acct_current_category = ACCT_CAT_USER;
uint32_t __acct_last_tick = 0;
uint64_t __acct_ticks[ACCT_CAT_MAX] = {0};

/**
 * @brief Enter a category (IRQ-safe version, briefly disables interrupts).
 */
acct_category_t acct_switch(acct_category_t new_cat)
{
    uint32_t sr = __disable_interrupts();
    acct_category_t prev_cat = __acct_current_category;
    if (prev_cat != new_cat) {
        uint32_t now = C0_COUNT();
        __acct_ticks[prev_cat] += TICKS_DISTANCE(__acct_last_tick, now);
        __acct_last_tick = now;
    }
    __acct_current_category = new_cat;
    __enable_interrupts(sr);
    return prev_cat;
}
 
uint64_t acct_get_ticks(acct_category_t cat)
{
    assertf(cat < ACCT_CAT_MAX, "invalid category: %d", cat);
    return __acct_ticks[(unsigned)cat];
}
