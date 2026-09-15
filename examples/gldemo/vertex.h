#ifndef VERTEX
#define VERTEX

#include <stdint.h>

typedef struct {
    float position[3];
    float texcoord[2];
    float normal[3];
    uint32_t color;
} vertex_t;

#endif
