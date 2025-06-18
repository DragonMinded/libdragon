/**
 * @file tpak.c
 * @brief Transfer Pak interface
 * @ingroup transferpak
 */

#include "tpak.h"
#include "controller.h"
#include <string.h>

/**
 * @anchor TPAK_POWER
 * @name Transfer Pak power control values
 * 
 * @see #tpak_set_power
 * @{
 */
/** @brief Transfer Pak power on */
#define TPAK_POWER_ON   0x84
/** @brief Transfer Pak power off */
#define TPAK_POWER_OFF  0xFE
/** @} */

/**
 * @anchor TPAK_ADDRESS
 * @name Transfer Pak address values
 * @{
 */
/** @brief Transfer Pak address for power control */
#define TPAK_ADDRESS_POWER  0x8000
/** @brief Transfer Pak address for bank switching */
#define TPAK_ADDRESS_BANK   0xA000
/** @brief Transfer Pak address for status flags */
#define TPAK_ADDRESS_STATUS 0xB000
/** @brief Transfer Pak address for cartridge data */
#define TPAK_ADDRESS_DATA   0xC000
/** @} */

/** @brief Transfer Pak command block size (32 bytes) */
#define TPAK_BLOCK_SIZE  0x20
/** @brief Transfer Pak cartridge bank size (16 KiB) */
#define TPAK_BANK_SIZE   0x4000

int tpak_set_value(int controller, uint16_t address, uint8_t value)
{
    uint8_t block[TPAK_BLOCK_SIZE];
    memset(block, value, TPAK_BLOCK_SIZE);
    return joybus_accessory_write(controller, address, block);
}

int tpak_init(int controller)
{
    int result = 0;

    int accessory = joypad_get_accessory_type(controller);
    if (accessory != JOYPAD_ACCESSORY_TYPE_TRANSFER_PAK) { return TPAK_ERROR_NO_TPAK; }

    result = tpak_set_power(controller, true);
    if (result) { return result; }

    result = tpak_set_access(controller, true);
    if (result) { return result; }

    uint8_t status = tpak_get_status(controller);
    if (status & TPAK_STATUS_REMOVED) { return TPAK_ERROR_NO_CARTRIDGE; }
    if (!(status & TPAK_STATUS_READY)) { return TPAK_ERROR_UNKNOWN_BEHAVIOUR; }

    return 0;
}

int tpak_set_access(int controller, bool access_state)
{
    uint8_t value = access_state ? 1 : 0;
    return tpak_set_value(controller, TPAK_ADDRESS_STATUS, value);
}

int tpak_set_power(int controller, bool power_state)
{
    uint8_t value = power_state ? TPAK_POWER_ON : TPAK_POWER_OFF;
    return tpak_set_value(controller, TPAK_ADDRESS_POWER, value);
}

int tpak_set_bank(int controller, int bank)
{
    return tpak_set_value(controller, TPAK_ADDRESS_BANK, bank);
}

uint8_t tpak_get_status(int controller)
{
    uint8_t block[TPAK_BLOCK_SIZE];
    joybus_accessory_read(controller, TPAK_ADDRESS_STATUS, block);

    return block[0];
}

int tpak_get_cartridge_header(int controller, struct gameboy_cartridge_header* header)
{
    // We're interested in 0x0000 - 0x3FFF of gb space.
    tpak_set_bank(controller, 0);
    // Header starts at 0x0100
    const uint16_t address = 0x0100;

    return tpak_read(controller, address, (uint8_t*) header, sizeof(*header));
}

int tpak_write(int controller, uint16_t address, uint8_t* data, uint16_t size)
{
    if (controller < 0 || controller > 3 || size % TPAK_BLOCK_SIZE || address % TPAK_BLOCK_SIZE)
    {
        return TPAK_ERROR_INVALID_ARGUMENT;
    }

    uint16_t adjusted_address = (address % TPAK_BANK_SIZE) + TPAK_ADDRESS_DATA;
    uint16_t end_address = address + size;
    if (end_address < address)
    {
        return TPAK_ERROR_ADDRESS_OVERFLOW;
    }

    uint8_t* cursor = data;

    int bank = address / TPAK_BANK_SIZE;
    tpak_set_bank(controller, bank);

    while(address < end_address)
    {
        // Check if we need to change the bank.
        if (address / TPAK_BANK_SIZE > bank) {
            bank = address / TPAK_BANK_SIZE;
            tpak_set_bank(controller, bank);
            adjusted_address = TPAK_ADDRESS_DATA;
        }

        joybus_accessory_write(controller, adjusted_address, cursor);
        address += TPAK_BLOCK_SIZE;
        cursor += TPAK_BLOCK_SIZE;
        adjusted_address += TPAK_BLOCK_SIZE;
    }

    return 0;
}

int tpak_read(int controller, uint16_t address, uint8_t* buffer, uint16_t size)
{
    if (controller < 0 || controller > 3 || size % TPAK_BLOCK_SIZE || address % TPAK_BLOCK_SIZE)
    {
        return TPAK_ERROR_INVALID_ARGUMENT;
    }

    uint16_t adjusted_address = (address % TPAK_BANK_SIZE) + TPAK_ADDRESS_DATA;
    uint16_t end_address = address + size;
    if (end_address < address)
    {
        return TPAK_ERROR_ADDRESS_OVERFLOW;
    }

    uint8_t* cursor = buffer;

    int bank = address / TPAK_BANK_SIZE;
    tpak_set_bank(controller, bank);

    while(address < end_address)
    {
        // Check if we need to change the bank.
        if (address / TPAK_BANK_SIZE > bank) {
            bank = address / TPAK_BANK_SIZE;
            tpak_set_bank(controller, bank);
            adjusted_address = TPAK_ADDRESS_DATA;
        }

        joybus_accessory_read(controller, adjusted_address, cursor);
        address += TPAK_BLOCK_SIZE;
        cursor += TPAK_BLOCK_SIZE;
        adjusted_address += TPAK_BLOCK_SIZE;
    }

    return 0;
}

bool tpak_check_header(struct gameboy_cartridge_header* header)
{
    uint8_t sum = 0;
    uint8_t* data = (uint8_t*) header;

    // sum values from 0x0134 (title) to 0x014C (version number)
    const uint8_t start = 0x34;
    const uint8_t end = 0x4C;
    for (uint8_t i = start; i <= end; i++) {
        sum = sum - data[i] - 1;
    }

    return sum == header->header_checksum;
}
