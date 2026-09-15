/**
 * @file joyframe.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
/*
 * Joybus packet handling functions
 */

#include "joyframe.h"

joyframe_err_t joyframe_read(joyframe_t pkt, uint8_t *cmd, uint8_t **data, int *data_len)
{
    joyframe_err_t err = JOYFRAME_ERR_NONE;

    int idx = pkt[63];
    if (idx >= 63) return JOYFRAME_ERR_FINISHED;

    // Extract send length and check escape codes
    int send_len;
    while ((send_len = pkt[idx++]) == JOYFRAME_ESC_NOP) {}
    if (send_len == JOYFRAME_ESC_SKIP)  { err = JOYFRAME_SKIPPED;  goto exit; }
    if (send_len == JOYFRAME_ESC_RESET) { err = JOYFRAME_RESET;    goto exit; }
    if (send_len == JOYFRAME_ESC_END)   { err = JOYFRAME_ERR_FINISHED; idx = 63; goto exit; }

    // Not an escape code. Check TX flags.
    // We never encode these ourselves, but they are supported by PIF and this
    // function could be called on an user-provided packet.
    if (send_len & JOYFRAME_TX_RESET)   { err = JOYFRAME_RESET;    goto exit; }
    if (send_len & JOYFRAME_TX_SKIP)    { err = JOYFRAME_SKIPPED;  goto exit; }
    send_len &= 0x3F;

    // Read receive length and check RX flags
    int recv_len = pkt[idx++];
    if (recv_len & JOYFRAME_RX_NO_DEVICE) { err = JOYFRAME_ERR_NO_DEVICE; goto exit; }
    if (recv_len & JOYFRAME_RX_TIMEOUT)   { err = JOYFRAME_ERR_TIMEOUT;   goto exit; }
    recv_len &= 0x3F;

    // Move index to the end of this command's data
    if (cmd)      *cmd  = pkt[idx];
    if (data)     *data = &pkt[idx + send_len];
    if (data_len) *data_len = recv_len;
    idx += send_len + recv_len;

exit:
    assertf(idx < 64, "overflow in joybus frame parsing");
    pkt[63] = idx;
    return err;
}

/// @cond
extern inline void joyframe_write_RESET(joyframe_t pkt);
extern inline void joyframe_write_IDENTIFY(joyframe_t pkt);
extern inline void joyframe_write_N64_CONTROLLER_READ(joyframe_t pkt);
extern inline void joyframe_write_N64_ACCESSORY_READ(joyframe_t pkt, uint16_t address);
extern inline void joyframe_write_N64_ACCESSORY_WRITE(joyframe_t pkt, uint16_t address, const uint8_t data[32]);

extern inline bool joyframe_read_N64_ACCESSORY_READ(joyframe_t pkt, uint8_t **data, uint8_t *checksum);
extern inline bool joyframe_read_N64_ACCESSORY_WRITE(joyframe_t pkt, uint8_t *checksum);
/// @endcond
