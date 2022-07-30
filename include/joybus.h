/**
 * @file joybus.h
 * @brief Joybus Subsystem
 * @ingroup lowlevel
 */
#ifndef __LIBDRAGON_JOYBUS_H
#define __LIBDRAGON_JOYBUS_H

#include <stddef.h>
#include <stdint.h>

/**
 * @addtogroup lowlevel
 * @{
 */

/**
 * @brief Size of a Joybus input/output block in bytes.
 */
#define JOYBUS_BLOCK_SIZE 64
/**
 * @brief Size of a Joybus input/output block in double-words.
 */
#define JOYBUS_BLOCK_DWORDS ( JOYBUS_BLOCK_SIZE / sizeof(uint64_t) )


#ifdef __cplusplus
extern "C" {
#endif

void joybus_exec( const void * inblock, void * outblock );

#ifdef __cplusplus
}
#endif

/** @} */ /* joybus */

#endif
