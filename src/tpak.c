/**
 * @file tpak.c
 * @brief Transfer Pak interface
 * @ingroup transferpak
 */

#include "libdragon.h"
#include <string.h>

/**
 * @defgroup transferpak Transfer Pak interface
 * @ingroup controller
 * @brief Transfer Pak interface
 * 
 * The Transfer Pak interface allows the ROM and Save files of gameboy and gameboy color cartridges connected to the system to be accessed.
 * Each time you want to access a transfer pak, first call #tpak_init to boot up the accessory and ensure that it is in working order.
 * #tpak_set_power and #tpak_set_access can also be called directly if you need to put the pak in a certain mode and you
 * can verify for youself that the pak is ready for IO by calling #tpak_get_status and inspecting the bits.
 *
 * To read in the connected gameboy cartridge's header, call #tpak_get_cartridge_header which provides you with a struct defining each of the fields. 
 * Pass this through to #tpak_check_header to verify that the header checksum adds up and the data have been read correctly.
 * 
 * #tpak_read and #tpak_write do what you expect, switching banks as needed.
 * 
 * Whenever not using the transfer pak, it's recommended to power it off by calling #tpak_set_power(false).
 */

#define TPAK_POWER_ON 0x84
#define TPAK_POWER_OFF 0xFE
#define TPAK_POWER_ADDRESS 0x8000
#define TPAK_BANK_ADDRESS 0xA000
#define TPAK_STATUS_ADDRESS 0xB000
#define TPAK_STATUS_CARTRIDGE_ABSENT 0x40 // bit 6

#define TPAK_DATA_ADDRESS 0xC000

#define BLOCK_SIZE 0x20
#define BANK_SIZE 0x4000


/**
 * @brief Set transfer pak or gb cartridge controls/flags.
 *
 * Helper to set a transfer pak status or control setting.
 * Be aware that for simplicity's sake, this writes the same value 32 times, and should therefore not be used for
 * updating individual bytes in Save RAM.
 *
 * @param[in] controller
 *            The controller (0-3) with transfer pak connected.
 * @param[in] address
 *            Address of the setting. Should be between 0x8000 and 0xBFE0
 * @param[in] value
 *            The value to be set.
 */
int tpak_set_value(int controller, uint16_t address, uint8_t value)
{
    uint8_t block[BLOCK_SIZE];
    memset(block, value, BLOCK_SIZE);
    return write_mempak_address(controller, address, block);
}

/**
 * @brief Prepare transfer pak for I/O
 * 
 * Powers on the transfer pak and sets access mode to allow I/O to gameboy cartridge.
 * Will also perform a series of checks to confirm transfer pak can be accessed reliably.
 * 
 * @param[in]  controller
 *             The controller (0-3) with transfer pak connected.
 * @return TPAK_ERROR code or 0 if successful.
 */
int tpak_init(int controller)
{
    int accessory = identify_accessory(controller);

    if (accessory != ACCESSORY_TRANSFERPAK)
    {
        return TPAK_ERROR_NO_TPAK;
    }

    int result = tpak_set_power(controller, true);
    if (result)
    {
        return result;
    }

    tpak_set_access(controller, true);

    uint8_t status = tpak_get_status(controller);
    if (status & TPAK_STATUS_CARTRIDGE_ABSENT)
    {
        return TPAK_ERROR_NO_CARTRIDGE;
    }

    if (!(status & TPAK_STATUS_READY))
    {
        return TPAK_ERROR_UNKNOWN_BEHAVIOUR;
    }

    return 0;
}

/**
 * @brief Set transfer pak access mode.
 * 
 * @param[in] controller
 *            The controller (0-3) with transfer pak connected.
 * @param[in] access_state
 *            True to allow access to the gameboy cartridge, false to revoke it.
 * @return TPAK_ERROR code or 0 if successful.
 */
int tpak_set_access(int controller, bool access_state)
{
    uint8_t value = access_state ? 1 : 0;
    return tpak_set_value(controller, TPAK_STATUS_ADDRESS, value);
}

/**
 * @brief Toggle transfer pak power state.
 * 
 * @param[in] controller
 *            The controller (0-3) with transfer pak connected.
 * @param[in] power_state 
 *            True to power the transfer pak and cartridge on, false to turn it off.
 * @return TPAK_ERROR code or 0 if successful.
 */
int tpak_set_power(int controller, bool power_state)
{
    uint8_t value = power_state ? TPAK_POWER_ON : TPAK_POWER_OFF;
    return tpak_set_value(controller, TPAK_POWER_ADDRESS, value);
}

/**
 * @brief Change transfer pak banked memory
 *
 * Change the bank of address space that is available between transfer pak addresses 0xC000 and 0xFFFF
 * 
 * @param[in] controller
 *            The controller (0-3) with transfer pak connected.
 * @param[in] bank
 *            The bank (0-3) to switch to.
 * @return TPAK_ERROR code or 0 if successful.
 */
int tpak_set_bank(int controller, int bank)
{
    return tpak_set_value(controller, TPAK_BANK_ADDRESS, bank);
}

/**
 * @brief Gets transfer pak status flags.
 * 
 * @param[in] controller
 *            The controller (0-3) with transfer pak connected.
 * @return The status byte with below fit fields.
 * @retval bit 0 Access mode - must be 1 to communicate with a cartridge
 * @retval bit 2 Reset status - if set, indicates that the cartridge is in the process of booting up/resetting
 * @retval bit 3 Reset detected -  Indicates that the cartridge has been reset since the last IO
 * @retval bit 6 Cartridge presence If not set, there is no cartridge in the transfer pak.
 * @retval bit 7 Power mode - a 1 indicates there is power to the transfer pak.
 */
uint8_t tpak_get_status(int controller)
{
    uint8_t block[BLOCK_SIZE];
    read_mempak_address(controller, TPAK_STATUS_ADDRESS, block);

    return block[0];
}

/**
 * @brief Reads a gameboy cartridge header in to memory.
 *
 * @param[in] controller
 *            The controller (0-3) with transfer pak connected.
 * @param[out] header
 *             Gameboy header data such as game title, rom size and type of colour support.
 * @return TPAK_ERROR code or 0 if successful.
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
 * @brief Write data from a buffer to a gameboy cartridge.
 *
 * Save RAM is located between gameboy addresses 0xA000 and 0xBFFF, which is in the transfer pak's bank 2.
 * This function does not account for cartridge bank switching, so to switch between MBC1 RAM banks, for example,
 * you'll need to switch to Tpak bank 1, and write to address 0xE000, which translates to address 0x6000 on the gameboy.
 *
 * @param[in] controller
 *            The controller (0-3) with transfer pak connected.
 * @param[in] address
 *            address in gameboy space to write to.
 * @param[in] data
 *            buffer containing data to write.
 * @param[in] size
 *            length of the buffer.
 * @return TPAK_ERROR code or 0 if successful.
 */
int tpak_write(int controller, uint16_t address, uint8_t* data, uint16_t size)
{
    if (controller < 0 || controller > 3 || size % BLOCK_SIZE || address % BLOCK_SIZE)
    {
        return TPAK_ERROR_INVALID_ARGUMENT;
    }

    uint16_t adjusted_address = (address % BANK_SIZE) + TPAK_DATA_ADDRESS;
    uint16_t end_address = address + size;
    if (end_address < address)
    {
        return TPAK_ERROR_ADDRESS_OVERFLOW;
    }

    uint8_t* cursor = data;

    int bank = address / BANK_SIZE;
    tpak_set_value(controller, TPAK_BANK_ADDRESS, bank);

    while(address < end_address)
    {
        // Check if we need to change the bank.
        if (address / BANK_SIZE > bank) {
            bank = address / BANK_SIZE;
            tpak_set_value(controller, TPAK_BANK_ADDRESS, bank);
            adjusted_address = TPAK_DATA_ADDRESS;
        }

        write_mempak_address(controller, adjusted_address, cursor);
        address += BLOCK_SIZE;
        cursor += BLOCK_SIZE;
        adjusted_address += BLOCK_SIZE;
    }

    return 0;
}

/**
 * @brief Read data from gameboy cartridge to a buffer.
 *
 * @param[in] controller
 *            The controller (0-3) with transfer pak connected.
 * @param[in] address
 *            address in gameboy space to read from.
 * @param[in] buffer
 *            buffer to copy to
 * @param[in] size
 *            length of the data to be read.
 * @return TPAK_ERROR code or 0 if successful.
 */
int tpak_read(int controller, uint16_t address, uint8_t* buffer, uint16_t size)
{
    if (controller < 0 || controller > 3 || size % BLOCK_SIZE || address % BLOCK_SIZE)
    {
        return TPAK_ERROR_INVALID_ARGUMENT;
    }

    uint16_t adjusted_address = (address % BANK_SIZE) + TPAK_DATA_ADDRESS;
    uint16_t end_address = address + size;
    if (end_address < address)
    {
        return TPAK_ERROR_ADDRESS_OVERFLOW;
    }

    uint8_t* cursor = buffer;

    int bank = address / BANK_SIZE;
    tpak_set_value(controller, TPAK_BANK_ADDRESS, bank);

    while(address < end_address)
    {
        // Check if we need to change the bank.
        if (address / BANK_SIZE > bank) {
            bank = address / BANK_SIZE;
            tpak_set_value(controller, TPAK_BANK_ADDRESS, bank);
            adjusted_address = TPAK_DATA_ADDRESS;
        }

        read_mempak_address(controller, adjusted_address, cursor);
        address += BLOCK_SIZE;
        cursor += BLOCK_SIZE;
        adjusted_address += BLOCK_SIZE;
    }

    return 0;
}

/**
 * @brief Verify gb cartride header.
 *
 *  This will help you verify that the tpak is connected and working properly.
 *
 * @param[in] header The header to check.
 */
bool tpak_check_header(struct gameboy_cartridge_header* header)
{
    uint8_t sum = 0;
    uint8_t* data = (uint8_t*) header;

    // sum values from 0x0134 (title) to 0x014C (version number)
    const uint8_t start = 0x34;
    const uint8_t end = 0x4C;
    for(uint8_t i = start; i <= end; i++) {
        sum = sum - data[i] - 1;
    }

    return sum == header->header_checksum;
}