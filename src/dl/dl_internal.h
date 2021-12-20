#ifndef __DL_INTERNAL
#define __DL_INTERNAL

#define DL_DRAM_BUFFER_SIZE   0x1000
#define DL_DMEM_BUFFER_SIZE   0x100
#define DL_OVERLAY_TABLE_SIZE 0x10
#define DL_OVERLAY_DESC_SIZE  0x10
#define DL_MAX_OVERLAY_COUNT  8

// Size of the initial display list block size
#define DL_BLOCK_MIN_SIZE     64
#define DL_BLOCK_MAX_SIZE     4192

// Maximum number of nested block calls
#define DL_MAX_BLOCK_NESTING_LEVEL 8
#define DL_HIGHPRI_CALL_SLOT       (DL_MAX_BLOCK_NESTING_LEVEL+0)

#endif
