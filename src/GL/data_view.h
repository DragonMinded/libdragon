#ifndef __DATA_VIEW_H
#define __DATA_VIEW_H

#include <stdint.h>

typedef struct data_view_s {
    void *pointer;
    uint32_t stride;
} data_view_t;

#endif
