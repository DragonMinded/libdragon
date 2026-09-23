/**
 * @file fnv1a.c
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 */
#include "fnv1a.h"

extern inline uint32_t fnv1a_init();
extern inline void fnv1a_step(uint32_t *hash, uint32_t v);
