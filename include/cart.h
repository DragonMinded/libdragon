/**
 * @file cart.h
 * @brief Cartridge
 * @ingroup cart
 */
#ifndef __LIBDRAGON_CART_H
#define __LIBDRAGON_CART_H

#include <stdint.h>

typedef enum cart_save_type_t
{
    /** @brief No cartridge save capabilities detected. */
    SAVE_TYPE_NONE           = 0x00,
    /** @brief 4 kilobit EEPROM present */
    SAVE_TYPE_EEPROM_4KBIT   = 0x10,
    /** @brief 16 kilobit EEPROM present */
    SAVE_TYPE_EEPROM_16KBIT  = 0x20,
    /** @brief 256 kilobit SRAM present */
    SAVE_TYPE_SRAM_256KBIT   = 0x30,
    /** @brief 768 kilobit SRAM present */
    SAVE_TYPE_SRAM_768KBIT   = 0x40,
    /** @brief 1 megabit SRAM present */
    SAVE_TYPE_SRAM_1MBIT     = 0x60,
    /** @brief 1 megabit FlashRAM present */
    SAVE_TYPE_FLASHRAM_1MBIT = 0x50,
} cart_save_type_t;

typedef enum flashram_type_t
{
    /** @brief No FlashRAM chip detected */
    FLASHRAM_TYPE_NONE        = 0x00000000,
    /** @brief Macronix MX29L0000 */
    FLASHRAM_TYPE_MX29L0000   = 0x00C20000,
    /** @brief Macronix MX29L0001 */
    FLASHRAM_TYPE_MX29L0001   = 0x00C20001,
    /** @brief Macronix MX29L1100 */
    FLASHRAM_TYPE_MX29L1100   = 0x00C2001E,
    /** @brief Macronix MX29L1101 */
    FLASHRAM_TYPE_MX29L1101_A = 0x00C2001D,
    /**
     * @brief Actual chip is labeled 29L1100KC-15B0, but is compatible with MX29L1101.
     * NOTE: Vendor code `0x00C2` is not 100% certain for this part; needs verification.
     */
    FLASHRAM_TYPE_MX29L1101_B = 0x00C20084,
    /**
     * @brief Actual chip is labeled 29L1100KC-15B0, but is compatible with MX29L1101.
     * NOTE: Vendor code `0x00C2` is not 100% certain for this part; needs verification.
     */
    FLASHRAM_TYPE_MX29L1101_C = 0x00C2008E,
    /** @brief Matsushita MN63F81MPN */
    FLASHRAM_TYPE_MN63F81MPN  = 0x003200F1,
} flashram_type_t;

#ifdef __cplusplus
extern "C" {
#endif

cart_save_type_t cart_detect_save_type( void );
flashram_type_t cart_detect_flashram( void );
bool cart_detect_sram(uint32_t offset);
bool cart_detect_sram_768kbit(uint8_t bank, uint32_t offset);

void cart_rom_read(void * dest, uint32_t offset, uint32_t len);
void cart_rom_write(const void * src, uint32_t offset, uint32_t len);

void cart_ram_read(void * dest, uint32_t offset, uint32_t len);
void cart_ram_write(const void * src, uint32_t offset, uint32_t len);

void cart_sram_768kbit_read(void * dest, uint8_t bank, uint32_t offset, uint32_t len);
void cart_sram_768kbit_write(const void * src, uint8_t bank, uint32_t offset, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif
