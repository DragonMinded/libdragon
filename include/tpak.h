/**
 * @file tpak.h
 * @brief Transfer Pak interface
 * @ingroup transferpak
 */

#ifndef __LIBDRAGON_TPAK_H
#define __LIBDRAGON_TPAK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @anchor TPAK_ERROR
 * @name Transfer Pak error values
 * @{
 */
/** @brief Transfer Pak error: Invalid argument */
#define TPAK_ERROR_INVALID_ARGUMENT     -1
/** @brief Transfer Pak error: No Transfer Pak */
#define TPAK_ERROR_NO_TPAK              -2
/** @brief Transfer Pak error: No controller */
#define TPAK_ERROR_NO_CONTROLLER        -3
/** @brief Transfer Pak error: Unknown behavior */
#define TPAK_ERROR_UNKNOWN_BEHAVIOUR    -4
/** @brief Transfer Pak error: No cartridge */
#define TPAK_ERROR_NO_CARTRIDGE         -5
/** @brief Transfer Pak error: Address overflow */
#define TPAK_ERROR_ADDRESS_OVERFLOW     -6
/** @} */

/**
 * @anchor TPAK_STATUS
 * @name Transfer Pak status bits
 * @{
 */
/**
 * @brief Transfer Pak status bit 0: Ready.
 * 
 * Also referred to as the "Access Mode" flag.
 * If not set, Transfer Pak cartridge reads/writes will fail.
 */
#define TPAK_STATUS_READY           0x01
/**
 * @brief Transfer Pak status bit 2: Reset status.
 * 
 * If set, the Game Boy cartridge is in the process of booting or resetting.
 */
#define TPAK_STATUS_WAS_RESET       0x04
/**
 * @brief Transfer Pak status bit 3: Reset detected.
 * 
 * If set, the Game Boy cartridge has been reset since the last status read.
 */
#define TPAK_STATUS_IS_RESETTING    0x08
/**
 * @brief Transfer Pak status bit 6: Cartridge removed.
 * 
 * If set, there is no Game Boy cartridge in the Transfer Pak.
 */
#define TPAK_STATUS_REMOVED         0x40
/**
 * @brief Transfer Pak status bit 7: Power status.
 * 
 * If set, the Transfer Pak has enabled power to the Game Boy cartridge.
 */
#define TPAK_STATUS_POWERED         0x80
/** @} */

/**
 * @brief Game Boy cartridge types.
 * 
 * Describes the Memory Bank Controller and other hardware on the cartridge.
 */
typedef enum __attribute__ ((packed))
{
    /** @brief ROM only (32 KiB ROM) */
    GB_ROM_ONLY = 0x00,
    /** @brief MBC1 (max 2 MiB ROM) */
    GB_MBC1 = 0x01,
    /** @brief MBC1 (max 2 MiB ROM) + RAM (32 KiB) */
    GB_MBC1_RAM = 0x02,
    /** @brief MBC1 (max 2 MiB ROM) + RAM (32 KiB) + Battery */
    GB_MBC1_RAM_BATTERY = 0x03,
    /** @brief MBC2 (max 256 KiB ROM; 512x4 bits RAM built-in) */
    GB_MBC2 = 0x05,
    /** @brief MBC2 (max 256 KiB ROM; 512x4 bits RAM built-in) + Battery */
    GB_MBC2_BATTERY = 0x06,
    /** @brief ROM (32 KiB) + RAM (max 8 KiB) */
    GB_ROM_RAM = 0x08,
    /** @brief ROM (32 KiB) + RAM (max 8 KiB) + Battery */
    GB_ROM_RAM_BATTERY = 0x09,
    /** @brief MMM01 ("Meta-mapper") */
    GB_MMM01 = 0x0B,
    /** @brief MMM01 ("Meta-mapper") + RAM */
    GB_MMM01_RAM = 0x0C,
    /** @brief MMM01 ("Meta-mapper") + RAM + Battery */
    GB_MMM01_RAM_BATTERY = 0x0D,
    /** @brief MBC3 (max 2 MiB ROM) */
    GB_MBC3 = 0x11,
    /** @brief MBC3 (max 2 MiB ROM) + RAM (64 KiB) */
    GB_MBC3_RAM = 0x12,
    /** @brief MBC3 (max 2 MiB ROM) + RAM (64 KiB) + Battery */
    GB_MBC3_RAM_BATTERY = 0x13,
    /** @brief MBC3 (max 2 MiB ROM) + Real-Time Clock + Battery */
    GB_MBC3_TIMER_BATTERY = 0x0F,
    /** @brief MBC3 (max 2 MiB ROM) + Real-Time Clock + RAM (64 KiB) + Battery */
    GB_MBC3_TIMER_RAM_BATTERY = 0x10,
    /** @brief MBC5 (max 8 MiB ROM) */
    GB_MBC5 = 0x19,
    /** @brief MBC5 (max 8 MiB ROM) + RAM (128 KiB) */
    GB_MBC5_RAM = 0x1A,
    /** @brief MBC5 (max 8 MiB ROM) + RAM (128 KiB) + Battery */
    GB_MBC5_RAM_BATTERY = 0x1B,
    /** @brief MBC5 (max 8 MiB ROM) + Rumble */
    GB_MBC5_RUMBLE = 0x1C,
    /** @brief MBC5 (max 8 MiB ROM) + Rumble + RAM (128 KiB) */
    GB_MBC5_RUMBLE_RAM = 0x1D,
    /** @brief MBC5 (max 8 MiB ROM) + Rumble + RAM (128 KiB) + Battery */
    GB_MBC5_RUMBLE_RAM_BATTERY = 0x1E,
    /** @brief MBC6 */
    GB_MBC6 = 0x20,
    /** @brief MBC7 + Tilt Sensor + Rumble + RAM + Battery */
    GB_MBC7_SENSOR_RUMBLE_RAM_BATTERY = 0x22,
    /** @brief Game Boy Camera */
    GB_POCKET_CAMERA = 0xFC,
    /** @brief Bandai TAMA5 */
    GB_BANDAI_TAMA5 = 0xFD,
    /** @brief Hudson HuC3 */
    GB_HUC3 = 0xFE,
    /** @brief Hudson HuC1 + RAM + Battery */
    GB_HUC1_RAM_BATTERY = 0xFF,
} gb_cart_type_t;

/**
 * @brief Game Boy cartridge ROM size types.
 * 
 * Describes how many ROM banks are available on the cartridge.
 */
typedef enum __attribute__ ((packed))
{
    /** @brief ROM size: 32 KiB (no banks) */
    GB_ROM_32KB = 0x00,
    /** @brief ROM size: 64 KiB (4 banks) */
    GB_ROM_64KB = 0x01,
    /** @brief ROM size: 128 KiB (8 banks) */
    GB_ROM_128KB = 0x02,
    /** @brief ROM size: 256 KiB (16 banks) */
    GB_ROM_256KB = 0x03,
    /** @brief ROM size: 512 KiB (32 banks) */
    GB_ROM_512KB = 0x04,
    /** @brief ROM size: 1 MiB (64 banks) */
    GB_ROM_1MB = 0x05,
    /** @brief ROM size: 2 MiB (128 banks) */
    GB_ROM_2MB = 0x06,
    /** @brief ROM size: 4 MiB (256 banks) */
    GB_ROM_4MB = 0x07,
    /** @brief ROM size: 8 MiB (512 banks) */
    GB_ROM_8MB = 0x08,
    /** @brief ROM size: 1.125 MiB (72 banks) */
    GB_ROM_1152KB = 0x52,
    /** @brief ROM size: 1.25 MiB (80 banks) */
    GB_ROM_1280KB = 0x53,
    /** @brief ROM size: 1.5 MiB (96 banks) */
    GB_ROM_1536KB = 0x54,
} gb_cart_rom_size_t;

/**
 * @brief Game Boy cartridge RAM size types.
 * 
 * Describes how much SRAM is available on the cartridge.
 */
typedef enum __attribute__ ((packed))
{
    /** @brief RAM not available */
    GB_RAM_NONE = 0x00,
    /** @brief RAM size: 2 KiB (no banks) */
    GB_RAM_2KB = 0x01,
    /** @brief RAM size: 8 KiB (no banks) */
    GB_RAM_8KB = 0x02,
    /** @brief RAM size: 32 KiB (4 banks) */
    GB_RAM_32KB = 0x03,
    /** @brief RAM size: 64 KiB (8 banks) */
    GB_RAM_64KB = 0x05,
    /** @brief RAM size: 128 KiB (16 banks) */
    GB_RAM_128KB = 0x04,
} gb_cart_ram_size_t;

/**
 * @brief Game Boy Color cartridge compatibility values.
 *
 * Found in the cartridge ROM header; describes whether the game should boot
 * into CGB mode or monochrome "Non CGB" compatibility mode.
 */
typedef enum __attribute__ ((packed))
{
    /**
     * @brief Game Boy Color not supported.
     *
     * Cartridge has no special support for Game Boy Color and will run in
     * original Game Boy mode. This is typically a grey Game Boy Game Pak
     * with a notch in the corner.
     */
    GBC_NOT_SUPPORTED = 0x00,
    /**
     * @brief Game Boy Color enhanced.
     *
     * Cartridge has special support for Game Boy Color, but still works on
     * original Game Boy. This is typically a black Game Boy Game Pak with
     * a notch in the corner.
     */
    GBC_DMG_SUPPORTED = 0x80,
    /**
     * @brief Game Boy Color required.
     *
     * Cartridge has special support for Game Boy Color and does not work on
     * original Game Boy. This is typically a black Game Boy Game Pak that
     * does not have a notch in the corner, which physically prevents it
     * from being played on original Game Boy.
     */
    GBC_ONLY_SUPPORTED = 0xC0
} gbc_support_type;

/**
 * @brief Super Game Boy cartridge compatibility values.
 *
 * Found in the cartridge ROM header; describes whether the game has
 * special enhancements for the Super Game Boy.
 */
typedef enum __attribute__ ((packed))
{
    /** @brief Not enhanced for Super Game Boy. */
    SGB_NOT_ENHANCED = 0x00,
    /** @brief Enhanced for Super Game Boy. */
    SGB_ENHANCED     = 0x03
} sgb_support_type;

/**
 * @brief "Old" Game Boy Color cartridge ROM header title structure.
 *
 * When the Game Boy Color was first introduced, games could use up to
 * 15 characters for the title, and 1 byte to signal CGB compatibility.
 */
struct old_gbc_title
{
    /** @brief Game title in ASCII. */
    uint8_t title[15];
    /** @brief Game Boy Color support. */
    gbc_support_type gbc_support;
};

/**
 * @brief "New" Game Boy Color cartridge ROM header title structure.
 *
 * Shortly after the Game Boy Color launched, games were limited to
 * 11 characters for the title, 4 bytes for a manufacturer code, and
 * 1 byte for CGB compatibility.
 */
struct new_gbc_title
{
    /** @brief Game title in ASCII. */
    uint8_t title[11];
    /** @brief Manufacturer identifier. */
    uint8_t manufacturer_code[4];
    /** @brief Game Boy Color support. */
    gbc_support_type gbc_support;
};

/**
 * @brief Game Boy cartridge ROM header structure.
 *
 * Data located at $0100-014F in each Game Boy cartridge ROM.
 */
struct gameboy_cartridge_header
{
    /** @brief Z80 instructions to boot the main program. */
    uint8_t entry_point[4];
    /** @brief Bitmap image data for the boot logo. */
    uint8_t logo[48];
    /** @brief Union of possible structures for the game title bytes. */
    union {
        /** @brief Game title in ASCII. */
        uint8_t title[16];
        /** @brief "Old" Game Boy Color title structure. */
        struct old_gbc_title old_title;
        /** @brief "New" Game Boy Color title structure. */
        struct new_gbc_title new_title;
    };
    /** @brief "New" publisher identifier. */
    uint16_t new_licensee_code;
    /** @brief Super Game Boy support. */
    sgb_support_type is_sgb_supported;
    /** @brief Cartridge type. */
    gb_cart_type_t cartridge_type;
    /** @brief ROM size identifier. */
    gb_cart_rom_size_t rom_size_code;
    /** @brief RAM size identifier. */
    gb_cart_ram_size_t ram_size_code;
    /** @brief Japan-only identifier. */
    uint8_t destination_code;
    /** @brief "Old" publisher identifier. */
    uint8_t old_licensee_code;
    /** @brief Version number of the game. */
    uint8_t version_number;
    /** @brief Checksum of cartridge ROM header. */
    uint8_t header_checksum;
    /** @brief Checksum of entire cartridge ROM. */
    uint16_t global_checksum;
    /** @brief Padding. */
    uint8_t overflow[16];
};

int tpak_init(int controller);
int tpak_set_value(int controller, uint16_t address, uint8_t value);
int tpak_set_bank(int controller, int bank);
int tpak_set_power(int controller, bool power_state);
int tpak_set_access(int controller, bool access_state);
uint8_t tpak_get_status(int controller);
int tpak_get_cartridge_header(int controller, struct gameboy_cartridge_header* header);
bool tpak_check_header(struct gameboy_cartridge_header* header);
int tpak_write(int controller, uint16_t address, uint8_t* data, uint16_t size);
int tpak_read(int controller, uint16_t address, uint8_t* buffer, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif
