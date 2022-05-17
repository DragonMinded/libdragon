#ifndef __LIBDRAGON_RDPQ_BLOCK_H
#define __LIBDRAGON_RDPQ_BLOCK_H

extern bool __rdpq_inited;

typedef struct rdpq_block_s rdpq_block_t;

void __rdpq_reset_buffer();
void __rdpq_block_begin();
void __rdpq_block_end();
void __rdpq_block_free(rdpq_block_t *block);

#endif
