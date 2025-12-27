/**
 * @file cpak.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Controller Pak raw access
 * @ingroup controllerpak
 */

#include "cpak.h"
#include "joypad_accessory.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#ifdef N64
#include "../rand_internal.h"
#endif

#define BLOCK_SIZE 32           ///< Size of a block in the controller pak (32 bytes)

static int cpak_xfer(joypad_port_t port, uint8_t bank, uint16_t addr, void *data, int nbytes, joypad_accessory_xfer_t xfer)
{
    // Check validaty of the address and length. We cannot cross banks
    if (addr + nbytes > 0x8000) {
        errno = EINVAL;
        return -1;
    }

    // Switch the bank (if needed). If the cpak doesn't support bank switching at all,
    // it will return JOYPAD_ACCESSORY_ERROR_CONTROLLER_PAK_BANK_SWITCH that we ignore,
    // since we treat all transfers as if they were done to bank 0. 
    // This makes them behave just like bankswitching ones, where we can't anyway
    // return an error if the bank number is wrong.
    joypad_accessory_error_t err = joypad_controller_pak_set_bank(port, bank);
    if (err != JOYPAD_ACCESSORY_ERROR_NONE && err != JOYPAD_ACCESSORY_ERROR_CONTROLLER_PAK_BANK_SWITCH) {
        errno = EIO; // FIXME
        return -1; // Bank switch failed
    }
    
    // Perform the block transfer
    if (joypad_accessory_xfer(port, xfer, addr, data, nbytes) != JOYPAD_ACCESSORY_ERROR_NONE) {
        errno = EIO; // FIXME
        return -1; // Transfer failed
    }

    return nbytes;
}

int cpak_read(joypad_port_t port, uint8_t bank, uint16_t address, void *buffer, size_t len)
{
    return cpak_xfer(port, bank, address, buffer, len, JOYPAD_ACCESSORY_XFER_READ);
}

int cpak_write(joypad_port_t port, uint8_t bank, uint16_t address, const void *buffer, size_t len)
{
    return cpak_xfer(port, bank, address, (void *)buffer, len, JOYPAD_ACCESSORY_XFER_WRITE);
}


bool cpak_supports_bankswitching(joypad_port_t port)
{
    return joypad_controller_pak_supports_bankswitching(port);
}

int cpak_probe_banks(joypad_port_t port)
{
    if (!cpak_supports_bankswitching(port)) {
        return 1;
    }

    int retcode = -1;
    int nsave_banks = 16;
    uint8_t* save_label = malloc(nsave_banks * BLOCK_SIZE);
    assert(save_label);

    // Create a random probe label that we will use to mark banks that we have already probed.
    uint8_t probe_label[BLOCK_SIZE];
    __rand(probe_label, BLOCK_SIZE);

    int bnk;
    for (bnk = 0; bnk < 256; bnk++) {
        // Resize the save area if we need more banks
        if (bnk >= nsave_banks) {
            nsave_banks *= 2;
            save_label = realloc(save_label, nsave_banks * BLOCK_SIZE);
        }

        // Read the current label into the save area
        if (cpak_read(port, bnk, 0, save_label + bnk * BLOCK_SIZE, BLOCK_SIZE) < 0) {
            goto exit;
        }

        // If the label matches the probe label, it means that an already probed
        // bank was selected (no need to restore it as it doesn't exist).
        if (bnk > 0 && memcmp(save_label + bnk * BLOCK_SIZE, probe_label, BLOCK_SIZE) == 0)
            break;

        // Write the probe label to the current bank
        if (cpak_write(port, bnk, 0, probe_label, BLOCK_SIZE) < 0) {
            goto exit;
        }
    }

    // Return the number of banks found
    retcode = bnk;

exit:
    for (int i=0; i<bnk; i++) {
        // Restore the label area in the first page of each bank. Normally this
        // area is unused anyway and can be safely corrupted, but we do this
        // to preserve also the functionality of custom filesystems that might
        // instead use those areas.
        // Do a best effort, ignoring I/O errors.
        cpak_write(port, i, 0, save_label + i * BLOCK_SIZE, BLOCK_SIZE);
    }

    free(save_label);
    return retcode;
}
