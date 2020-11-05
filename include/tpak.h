/**
 * @file tpak.h
 * @brief Transfer Pak interface
 * @ingroup transferpak
 */

#ifndef __LIBDRAGON_TPAK_H
#define __LIBDRAGON_TPAK_H

#include "libdragon.h"

#define TPAK_ERROR_INVALID_ARGUMENT -1
#define TPAK_ERROR_NO_TPAK  -2
#define TPAK_ERROR_NO_CONTROLLER -3
#define TPAK_ERROR_UNKNOWN_BEHAVIOUR -4
#define TPAK_ERROR_NO_CARTRIDGE -5
#define TPAK_ERROR_ADDRESS_OVERFLOW -6

#define TPAK_STATUS_READY           0x01
#define TPAK_STATUS_WAS_RESET       0x04
#define TPAK_STATUS_IS_RESETTING    0x08
#define TPAK_STATUS_REMOVED         0x40
#define TPAK_STATUS_POWERED         0x80

typedef enum __attribute__ ((packed))
{
    GBC_NOT_SUPPORTED = 0x00,
    GBC_DMG_SUPPORTED = 0x80,
    GBC_ONLY_SUPPORTED = 0xC0
} gbc_support_type;

struct old_gbc_title
{
    uint8_t title[15];
    gbc_support_type gbc_support;
};

struct new_gbc_title
{
    uint8_t title[11];
    uint8_t manufacturer_code[4];
    gbc_support_type gbc_support;
};

struct gameboy_cartridge_header
{
    uint8_t entry_point[4];
    uint8_t logo[48];
    union {
        uint8_t title[16];
        struct old_gbc_title old_title;
        struct new_gbc_title new_title;
    };
    uint16_t new_licensee_code;
    bool is_sgb_supported;
    uint8_t cartridge_type;
    uint8_t rom_size_code;
    uint8_t ram_size_code;
    uint8_t destination_code;
    uint8_t old_licensee_code;
    uint8_t version_number;
    uint8_t header_checksum;
    uint16_t global_checksum;
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

#endif
