#ifndef LIBDRAGON_BB_SAVE_H
#define LIBDRAGON_BB_SAVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief BBPlayer Save error codes */
typedef enum
{
    BB_SAVE_ERROR_NONE = 0,
    BB_SAVE_ERROR_BAD_TYPE,
    BB_SAVE_ERROR_BAD_SIZE,
    BB_SAVE_ERROR_BAD_XFER,
} bb_save_error_t;

/** @brief BBPlayer Save transfer modes */
typedef enum
{
    BB_SAVE_XFER_READ = 0,
    BB_SAVE_XFER_WRITE,
} bb_save_xfer_t;

/** @brief BBPlayer Save types */
typedef enum
{
    BB_SAVE_PAK1 = 0,
    BB_SAVE_PAK2,
    BB_SAVE_PAK3,
    BB_SAVE_PAK4,
    BB_SAVE_EEPROM,
    BB_SAVE_FLASHRAM,
    BB_SAVE_SRAM,
} bb_save_t;

/** @brief Get the size of a BBPlayer save (in bytes). */
uint32_t bb_save_size( bb_save_t save_type );

/** @brief Read or write to a BBPlayer save. */
bb_save_error_t bb_save_xfer( bb_save_t save_type, bb_save_xfer_t xfer, uint32_t addr, void *data, uint32_t nbytes );

#ifdef __cplusplus
}
#endif

#endif // LIBDRAGON_BB_SAVE_H
