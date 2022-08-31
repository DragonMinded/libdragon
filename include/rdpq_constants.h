#ifndef __LIBDRAGON_RDPQ_CONSTANTS_H
#define __LIBDRAGON_RDPQ_CONSTANTS_H

#define RDPQ_ADDRESS_TABLE_SIZE 16

#define RDPQ_DYNAMIC_BUFFER_SIZE   0x800

// Asserted if #rdpq_mode_blender was called in fill/copy mode
#define RDPQ_ASSERT_FILLCOPY_BLENDING  0xC003

// Asserted if a 2-pass combiner is set with #rdpq_mode_combiner while mipmap is enabled.
#define RDPQ_ASSERT_MIPMAP_COMB2  0xC004

#define RDPQ_MAX_COMMAND_SIZE 44
#define RDPQ_BLOCK_MIN_SIZE   64    ///< RDPQ block minimum size (in 32-bit words)
#define RDPQ_BLOCK_MAX_SIZE   4192  ///< RDPQ block minimum size (in 32-bit words)

#endif
