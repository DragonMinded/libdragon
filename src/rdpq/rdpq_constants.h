#ifndef __LIBDRAGON_RDPQ_CONSTANTS_H
#define __LIBDRAGON_RDPQ_CONSTANTS_H

#define RDPQ_ADDRESS_TABLE_SIZE 16

// Asserted if #rdpq_mode_blending was called in fill/copy mode
#define RDPQ_ASSERT_FILLCOPY_BLENDING  0xC003

#define RDPQ_MAX_COMMAND_SIZE 44
#define RDPQ_BLOCK_MIN_SIZE   64
#define RDPQ_BLOCK_MAX_SIZE   4192

#endif
