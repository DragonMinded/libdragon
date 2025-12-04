/**
 * @file a3d.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Analogue3D support
 */

#include "a3d.h"
#include "mi.h"

bool a3d_detect(void)
{
    return (*MI_VERSION & 0xFF) == 0x3;
}
