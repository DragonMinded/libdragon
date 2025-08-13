/**
 * @file cpak.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Controller Pak raw access
 * @ingroup controllerpak
 */

#include "cpak.h"
#include "cpakfs_internal.h"
#include "joypad_accessory.h"
#include "../rand_internal.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static int cpak_xfer(joypad_port_t port, uint8_t bank, uint16_t addr, void *data, int nbytes, joypad_accessory_xfer_t xfer)
{
    // Check validaty of the address and length. We cannot cross banks
    if (addr + nbytes >= 0x8000) {
        errno = EINVAL;
        return -1;
    }

    // Switch the bank (if needed)
    joypad_accessory_error_t err = joypad_controller_pak_set_bank(port, bank);
    if (err != JOYPAD_ACCESSORY_ERROR_NONE) {
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

int cpak_read(joypad_port_t port, void *buffer, uint8_t bank, uint16_t address, size_t len)
{
    return cpak_xfer(port, bank, address, buffer, len, JOYPAD_ACCESSORY_XFER_READ);
}

int cpak_write(joypad_port_t port, const void *buffer, uint8_t bank, uint16_t address, size_t len)
{
    return cpak_xfer(port, bank, address, (void *)buffer, len, JOYPAD_ACCESSORY_XFER_WRITE);
}


bool cpak_is_multibank(joypad_port_t port)
{
    return joypad_controller_pak_is_multibank(port);
}

int cpak_probe_banks(joypad_port_t port)
{
    if (!cpak_is_multibank(port)) {
        return 1;
    }

    int retcode = -1;
    uint8_t* save_label = malloc(MAX_BANKS * BLOCK_SIZE);

    // Create a random probe label that we will use to mark banks that we have already probed.
    uint8_t probe_label[BLOCK_SIZE];
    __rand(probe_label, BLOCK_SIZE);

    int bnk;
    for (bnk = 0; bnk < MAX_BANKS; bnk++) {

        // Read the current label into the save area
        if (block_read(port, bnk * BANK_SIZE, save_label + bnk * BLOCK_SIZE, BLOCK_SIZE) < 0) {
            goto exit;
        }

        // If the label matches the probe label, it means that an already probed
        // bank was selected (no need to restore it as it doesn't exist).
        if (bnk > 0 && memcmp(save_label + bnk * BLOCK_SIZE, probe_label, BLOCK_SIZE) == 0)
            break;

        // Write the probe label to the current bank
        if (block_write(port, bnk * BANK_SIZE, probe_label, BLOCK_SIZE) < 0) {
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
        block_write(port, i * BANK_SIZE, save_label + i * BLOCK_SIZE, BLOCK_SIZE);
    }

    free(save_label);
    return retcode;
}
