#ifndef BOOT_LOADER_H
#define BOOT_LOADER_H

// The loader is run directly from RDRAM for speed. We reserve a portion at
// the end of RDRAM for it.
#define LOADER_SIZE              (32*1024)

#define TOTAL_RESERVED_SIZE      (LOADER_SIZE)

#define LOADER_BASE(memsize, stage2size)              (void*)(0x80000000 + (memsize) - (stage2size))
#define STACK2_TOP(memsize, stage2size)               (LOADER_BASE(memsize, stage2size) - 16)

__attribute__((noreturn, far))
void loader(void);

#endif /* BOOT_LOADER_H */
