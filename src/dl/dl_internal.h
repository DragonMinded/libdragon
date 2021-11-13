#ifndef __DL_INTERNAL
#define __DL_INTERNAL

#define DL_DRAM_BUFFER_SIZE   0x1000
#define DL_DMEM_BUFFER_SIZE   0x100
#define DL_OVERLAY_TABLE_SIZE 0x10
#define DL_OVERLAY_DESC_SIZE  0x10
#define DL_MAX_OVERLAY_COUNT  8

// This is not a hard limit. Adjust this value when bigger commands are added.
#define DL_MAX_COMMAND_SIZE   16

#endif
