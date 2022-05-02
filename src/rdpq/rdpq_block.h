#ifndef __LIBDRAGON_RDPQ_BLOCK_H
#define __LIBDRAGON_RDPQ_BLOCK_H

extern bool __rdpq_inited;

typedef struct rdpq_block_s rdpq_block_t;

void rdpq_reset_buffer();
rdpq_block_t* rdpq_block_begin();
void rdpq_block_end();
void rdpq_block_free(rdpq_block_t *block);

#endif
