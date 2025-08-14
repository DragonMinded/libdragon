#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>


#define __randn(n)  (\
    __builtin_constant_p(n) ? \
        rand() % (n) : \
        ((uint64_t)rand() * (n)) >> 32)

static void __rand(void *buf, size_t n)
{
    uint8_t *p = (uint8_t *)buf;
    for (size_t i = 0; i < n; i++) {
        p[i] = rand() % 256; // Simple random byte generation
    }    
}


#define assertf(cond, fmt, ...) \
    do { if (!(cond)) { fprintf(stderr, "Assertion failed: " fmt "\n", ##__VA_ARGS__); exit(EXIT_FAILURE); } } while (0)

#ifdef __MINGW32__
#define EFSCORRUPTED 84
#endif
#define EFTYPE  EFSCORRUPTED
#include "../../src/joybus/cpak.c"
#include "../../src/joybus/cpakfs.c"
#include "../../src/joybus/cpakfs_fsck.c"

static filesystem_t *gfs = NULL;
static int cur_bank = 0;
int g_num_banks = 1;
FILE *g_pak = NULL;
int g_pak_offset = 0;

int attach_filesystem(const char *prefix, filesystem_t* fs) {
    gfs = fs;
    return 0;
}

int detach_filesystem(const char *prefix) {
    gfs = NULL;
    return 0;
}

bool joypad_controller_pak_supports_bankswitching(joypad_port_t port) {
    return true;
}

joypad_accessory_type_t joypad_get_accessory_type(joypad_port_t port) {
    return JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK;
}

joypad_accessory_error_t joypad_controller_pak_set_bank(joypad_port_t port, uint8_t bank)
{
    cur_bank = bank % g_num_banks;
    return 0;
}

joypad_accessory_error_t joypad_accessory_xfer(joypad_port_t port, joypad_accessory_xfer_t xfer, uint16_t addr, void *buf, size_t len){
    fseek(g_pak, addr + cur_bank*32768 + g_pak_offset, SEEK_SET);
    assert(addr + len <= g_num_banks * 32768 + g_pak_offset);

    if (xfer == JOYPAD_ACCESSORY_XFER_READ) {
        return fread(buf, len, 1, g_pak) == 1 ? 0 : -1;
    } else if (xfer == JOYPAD_ACCESSORY_XFER_WRITE) {
        return fwrite(buf, len, 1, g_pak) == 1 ? 0 : -1;
    } else {
        assert(0 && "Invalid xfer type");
    }
}
