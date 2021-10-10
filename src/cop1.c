/**
 * @file COP1.c
 * @brief N64 COP1 Interface
 * @ingroup n64sys
 */
#include "libdragon.h"

/**
 * @addtogroup n64sys
 * @{
 */

__attribute__((constructor)) void __init_cop1()
{
    /* Read initalized value from cop1 control register */
    uint32_t fcr31 = C1_FCR31();
    
    /* Set FS bit to allow flashing of denormalized floats */
    fcr31 |= C1_FS_BIT;

    /* Write back updated cop1 control register */
    C1_WRITE_FCR31(fcr31);
}

/** @} */ /* cop1 */
