/**
 * @file bb_save.c
 * @author Christopher Bonhage <me@christopherbonhage.com>
 * @brief iQue Player (BB) Save Data interface.
 */

#include <string.h>
#include "bb_save.h"

#define BB_SAVE_EEPROM_ADDR    ((uint32_t *)0x8000035c)   ///< BB EEPROM address register
#define BB_SAVE_EEPROM_SIZE    ((uint32_t *)0x80000360)   ///< BB EEPROM size register

#define BB_SAVE_FLASHRAM_ADDR  ((uint32_t *)0x80000364)   ///< BB FlashRAM address register
#define BB_SAVE_FLASHRAM_SIZE  ((uint32_t *)0x80000368)   ///< BB FlashRAM size register

#define BB_SAVE_SRAM_ADDR      ((uint32_t *)0x8000036c)   ///< BB SRAM address register
#define BB_SAVE_SRAM_SIZE      ((uint32_t *)0x80000370)   ///< BB SRAM size register

#define BB_SAVE_PAK_ADDR       ((uint32_t *)0x80000374)   ///< BB Pak base address register
#define BB_SAVE_PAK_SIZE       ((uint32_t *)0x80000384)   ///< BB Pak size register (all Paks)


uint32_t bb_save_size( bb_save_t save_type )
{
    switch( save_type )
    {
        case BB_SAVE_PAK1:
        case BB_SAVE_PAK2:
        case BB_SAVE_PAK3:
        case BB_SAVE_PAK4:     return *BB_SAVE_PAK_SIZE;
        case BB_SAVE_EEPROM:   return *BB_SAVE_EEPROM_SIZE;
        case BB_SAVE_FLASHRAM: return *BB_SAVE_FLASHRAM_SIZE;
        case BB_SAVE_SRAM:     return *BB_SAVE_SRAM_SIZE;
        default:               return 0;
    }
}

bb_save_error_t bb_save_xfer( bb_save_t save_type, bb_save_xfer_t xfer, uint32_t addr, void *data, uint32_t nbytes )
{
    uint32_t save_size = bb_save_size( save_type );
    if( addr + nbytes > save_size ) return BB_SAVE_ERROR_BAD_SIZE;

    uint32_t *save_base = NULL;
    switch( save_type )
    {
        case BB_SAVE_PAK1:
        case BB_SAVE_PAK2:
        case BB_SAVE_PAK3:
        case BB_SAVE_PAK4:     save_base = BB_SAVE_PAK_ADDR + (sizeof(uint32_t) * (int)save_type); break;
        case BB_SAVE_EEPROM:   save_base = BB_SAVE_EEPROM_ADDR;   break;
        case BB_SAVE_FLASHRAM: save_base = BB_SAVE_FLASHRAM_ADDR; break;
        case BB_SAVE_SRAM:     save_base = BB_SAVE_SRAM_ADDR;     break;
        default:               return BB_SAVE_ERROR_BAD_TYPE;
    }

    void *save_buf = (void *)(save_base + addr);
    switch( xfer )
    {
        case BB_SAVE_XFER_READ:
            memcpy( data, save_buf, nbytes );
            return BB_SAVE_ERROR_NONE;
        case BB_SAVE_XFER_WRITE:
            memcpy( save_buf, data, nbytes );
            return BB_SAVE_ERROR_NONE;
        default:
            return BB_SAVE_ERROR_BAD_XFER;
    }
}
