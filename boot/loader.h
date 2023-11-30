#ifndef BOOT_LOADER_H
#define BOOT_LOADER_H

// The loader is run directly from RDRAM for speed. We reserve a portion at
// the end of RDRAM for it.
#define LOADER_RESERVED_SIZE    (32*1024)

__attribute__((noreturn, far))
void loader(void);

#endif /* BOOT_LOADER_H */
