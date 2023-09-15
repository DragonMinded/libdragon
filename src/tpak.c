/**
 * @file tpak.c
 * @brief Transfer Pak interface
 * @ingroup transferpak
 */

#include "tpak.h"
#include "controller.h"
#include <string.h>

/**
 * @defgroup transferpak Transfer Pak interface
 * @ingroup controller
 * @brief Transfer Pak interface
 * 
 * The Transfer Pak interface allows access to Game Boy and Game Boy Color
 * cartridges connected through the accessory port of each controller.
 * 
 * Before accessing a Transfer Pak, first call #tpak_init to boot up the
 * accessory and ensure that it is in working order. For advanced use-cases,
 * #tpak_set_power and #tpak_set_access can also be called directly if you
 * need to put the Transfer Pak into a certain mode. You can verify that the
 * Transfer Pak is in the correct mode by inspecting the #tpak_get_status flags.
 * 
 * Whenever the Transfer Pak is not in use, it is recommended to power it off
 * by calling @ref tpak_set_power "`tpak_set_power(controller, false)`".
 *
 * You can read the connected Game Boy cartridge's ROM header by calling
 * #tpak_get_cartridge_header and validating the result with #tpak_check_header.
 * If the ROM header checksum does not match, it is likely that the cartridge
 * connection is poor.
 * 
 * You can use #tpak_read and #tpak_write to access the Game Boy cartridge.
 * Note that these functions do not account for cartridge bank switching.
 * For more information about Game Boy cartridge bank switching, refer to the
 * GBDev Pan Docs at https://gbdev.io/pandocs/
 */

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

/**
 * @brief Set Transfer Pak or Game Boy cartridge status/control value.
 *
 * This is an internal helper to set a Transfer Pak status or control setting.
 * This function is not suitable for setting individual bytes in Save RAM!
 *
 * @param[in] controller
 *            The controller (0-3) with Transfer Pak connected.
 * @param[in] address
 *            Address of the setting. Should be between 0x8000 and 0xBFE0
 * @param[in] value
 *            A byte of data to fill the write buffer with.
 */
int tpak_set_value(int controller, uint16_t address, uint8_t value)
{
    uint8_t block[TPAK_BLOCK_SIZE];
    memset(block, value, TPAK_BLOCK_SIZE);
    return joybus_accessory_write(controller, address, block);
}

/**
 * @brief Prepare a Transfer Pak for read/write commands.
 * 
 * Powers on the Transfer Pak and enables access to the Game Boy cartridge.
 * Also performs status checks to confirm the Transfer Pak can be accessed reliably.
 * 
 * @param[in]  controller
 *             The controller (0-3) with Transfer Pak connected.
 * @return 0 if successful or @ref TPAK_ERROR otherwise.
 */
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

/**
 * @brief Set the access mode flag for a Transfer Pak.
 * 
 * @param[in] controller
 *            The controller (0-3) with Transfer Pak connected.
 * @param[in] access_state
 *            Whether to allow access to the Game Boy cartridge.
 * @return 0 if successful or @ref TPAK_ERROR otherwise.
 */
int tpak_set_access(int controller, bool access_state)
{
    uint8_t value = access_state ? 1 : 0;
    return tpak_set_value(controller, TPAK_ADDRESS_STATUS, value);
}

/**
 * @brief Set the power enabled flag for a Transfer Pak.
 * 
 * @param[in] controller
 *            The controller (0-3) with Transfer Pak connected.
 * @param[in] power_state 
 *            True to power the Transfer Pak and cartridge on, false to turn it off.
 * @return 0 if successful or @ref TPAK_ERROR otherwise.
 */
int tpak_set_power(int controller, bool power_state)
{
    uint8_t value = power_state ? TPAK_POWER_ON : TPAK_POWER_OFF;
    return tpak_set_value(controller, TPAK_ADDRESS_POWER, value);
}

/**
 * @brief Set the cartridge data address memory bank for a Transfer Pak.
 *
 * Change the bank of address space that is available for #tpak_read and
 * #tpak_write between Transfer Pak addresses 0xC000 and 0xFFFF.
 * 
 * @param[in] controller
 *            The controller (0-3) with Transfer Pak connected.
 * @param[in] bank
 *            The bank (0-3) to switch to.
 * @return 0 if successful or @ref TPAK_ERROR otherwise.
 */
int tpak_set_bank(int controller, int bank)
{
    return tpak_set_value(controller, TPAK_ADDRESS_BANK, bank);
}

/**
 * @brief Get the status flags for a Transfer Pak.
 * 
 * @param[in] controller
 *            The controller (0-3) with Transfer Pak connected.
 * @return The status byte with @ref TPAK_STATUS flags
 */
uint8_t tpak_get_status(int controller)
{
    uint8_t block[TPAK_BLOCK_SIZE];
    joybus_accessory_read(controller, TPAK_ADDRESS_STATUS, block);

    return block[0];
}

/**
 * @brief Read the Game Boy cartridge ROM header from a Transfer Pak.
 *
 * @param[in] controller
 *            The controller (0-3) with Transfer Pak connected.
 * @param[out] header
 *             Pointer to destination Game Boy cartridge ROM header data structure.
 * @return 0 if successful or @ref TPAK_ERROR otherwise.
 */
int tpak_get_cartridge_header(int controller, struct gameboy_cartridge_header* header)
{
    // We're interested in 0x0000 - 0x3FFF of gb space.
    tpak_set_bank(controller, 0);
    // Header starts at 0x0100
    const uint16_t address = 0x0100;

    return tpak_read(controller, address, (uint8_t*) header, sizeof(*header));
}

/**
 * @brief Write data from a buffer to a Game Boy cartridge via Transfer Pak.
 *
 * Save RAM is located between gameboy addresses 0xA000 and 0xBFFF, which is in the Transfer Pak's bank 2.
 * This function does not account for cartridge bank switching, so to switch between MBC1 RAM banks, for example,
 * you'll need to switch to Tpak bank 1, and write to address 0xE000, which translates to address 0x6000 on the gameboy.
 *
 * @param[in] controller
 *            The controller (0-3) with Transfer Pak connected.
 * @param[in] address
 *            address in Game Boy cartridge space to write to.
 * @param[in] data
 *            buffer containing the data to write.
 * @param[in] size
 *            length of the buffer.
 * @return 0 if successful or @ref TPAK_ERROR otherwise.
 */
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

/**
 * @brief Read data from a Game Boy cartridge to a buffer via Transfer Pak.
 *
 * @param[in] controller
 *            The controller (0-3) with Transfer Pak connected.
 * @param[in] address
 *            address in Game Boy cartridge space to read from.
 * @param[in] buffer
 *            buffer to copy cartridge data into.
 * @param[in] size
 *            length of the data to be read.
 * @return 0 if successful or @ref TPAK_ERROR otherwise.
 */
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

/**
 * @brief Verify a Game Boy cartridge ROM header checksum.
 *
 * Confirms that the Transfer Pak is connected and working properly.
 *
 * @param[in] header The Game Boy ROM header to check.
 */
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
