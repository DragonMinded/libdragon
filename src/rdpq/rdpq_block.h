#ifndef __LIBDRAGON_RDPQ_BLOCK_H
#define __LIBDRAGON_RDPQ_BLOCK_H

extern bool __rdpq_inited;
extern bool __rdpq_zero_blocks;

typedef struct rdpq_block_s rdpq_block_t;

typedef struct rdpq_block_s {
    rdpq_block_t *next;
    uint32_t autosync_state;
    uint32_t cmds[] __attribute__((aligned(8)));
} rdpq_block_t;

void __rdpq_reset_buffer();
void __rdpq_block_begin();
rdpq_block_t* __rdpq_block_end();
void __rdpq_block_free(rdpq_block_t *block);
void __rdpq_block_run(rdpq_block_t *block);

#endif
