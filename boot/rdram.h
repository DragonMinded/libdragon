#ifndef BOOT_RDRAM_H
#define BOOT_RDRAM_H

int rdram_init(void (*bank_found)(int chip_id, bool last));

#endif
