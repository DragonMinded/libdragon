#ifndef __LIBDRAGON_RDPQ_CONSTANTS_H
#define __LIBDRAGON_RDPQ_CONSTANTS_H

#define RDPQ_ADDRESS_TABLE_SIZE 16

#define RDPQ_DYNAMIC_BUFFER_SIZE (1024 * 64)

// Asserted if #rdpq_mode_blender was called in fill/copy mode
#define RDPQ_ASSERT_FILLCOPY_BLENDING  0xC003

// Asserted if a 2-pass combiner is set with #rdpq_mode_combiner while mipmap is enabled.
#define RDPQ_ASSERT_MIPMAP_COMB2  0xC004

// Asserted if RDPQCmd_Triangle is called with RDPQ_TRIANGLE_REFERENCE == 0
#define RDPQ_ASSERT_INVALID_CMD_TRI  0xC005

// Asserted if RDPQ_Send is called with invalid parameters (begin > end)
#define RDPQ_ASSERT_SEND_INVALID_SIZE  0xC006

// Asserted if the TMEM is full during an auto-TMEM operation
#define RDPQ_ASSERT_AUTOTMEM_FULL  0xC007

// Asserted if the TMEM is full during an auto-TMEM operation
#define RDPQ_ASSERT_AUTOTMEM_UNPAIRED  0xC008

#define RDPQ_MAX_COMMAND_SIZE 44
#define RDPQ_BLOCK_MIN_SIZE   64    ///< RDPQ block minimum size (in 32-bit words)
#define RDPQ_BLOCK_MAX_SIZE   4192  ///< RDPQ block minimum size (in 32-bit words)

/** @brief Set to 1 for the reference implementation of RDPQ_TRIANGLE (on CPU) */
#define RDPQ_TRIANGLE_REFERENCE    0

#endif
