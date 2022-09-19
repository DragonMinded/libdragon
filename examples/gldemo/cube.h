#ifndef CUBE_H
#define CUBE_H

#include "vertex.h"

static const vertex_t cube_vertices[] = {
    // +X
    { .position = { 1.f, -1.f, -1.f}, .texcoord = {0.f, 0.f}, .normal = { 1.f,  0.f,  0.f}, .color = 0xFF0000FF },
    { .position = { 1.f,  1.f, -1.f}, .texcoord = {1.f, 0.f}, .normal = { 1.f,  0.f,  0.f}, .color = 0xFF0000FF },
    { .position = { 1.f,  1.f,  1.f}, .texcoord = {1.f, 1.f}, .normal = { 1.f,  0.f,  0.f}, .color = 0xFF0000FF },
    { .position = { 1.f, -1.f,  1.f}, .texcoord = {0.f, 1.f}, .normal = { 1.f,  0.f,  0.f}, .color = 0xFF0000FF },

    // -X
    { .position = {-1.f, -1.f, -1.f}, .texcoord = {0.f, 0.f}, .normal = {-1.f,  0.f,  0.f}, .color = 0x00FFFFFF },
    { .position = {-1.f, -1.f,  1.f}, .texcoord = {0.f, 1.f}, .normal = {-1.f,  0.f,  0.f}, .color = 0x00FFFFFF },
    { .position = {-1.f,  1.f,  1.f}, .texcoord = {1.f, 1.f}, .normal = {-1.f,  0.f,  0.f}, .color = 0x00FFFFFF },
    { .position = {-1.f,  1.f, -1.f}, .texcoord = {1.f, 0.f}, .normal = {-1.f,  0.f,  0.f}, .color = 0x00FFFFFF },

    // +Y
    { .position = {-1.f,  1.f, -1.f}, .texcoord = {0.f, 0.f}, .normal = { 0.f,  1.f,  0.f}, .color = 0x00FF00FF },
    { .position = {-1.f,  1.f,  1.f}, .texcoord = {0.f, 1.f}, .normal = { 0.f,  1.f,  0.f}, .color = 0x00FF00FF },
    { .position = { 1.f,  1.f,  1.f}, .texcoord = {1.f, 1.f}, .normal = { 0.f,  1.f,  0.f}, .color = 0x00FF00FF },
    { .position = { 1.f,  1.f, -1.f}, .texcoord = {1.f, 0.f}, .normal = { 0.f,  1.f,  0.f}, .color = 0x00FF00FF },

    // -Y
    { .position = {-1.f, -1.f, -1.f}, .texcoord = {0.f, 0.f}, .normal = { 0.f, -1.f,  0.f}, .color = 0xFF00FFFF },
    { .position = { 1.f, -1.f, -1.f}, .texcoord = {1.f, 0.f}, .normal = { 0.f, -1.f,  0.f}, .color = 0xFF00FFFF },
    { .position = { 1.f, -1.f,  1.f}, .texcoord = {1.f, 1.f}, .normal = { 0.f, -1.f,  0.f}, .color = 0xFF00FFFF },
    { .position = {-1.f, -1.f,  1.f}, .texcoord = {0.f, 1.f}, .normal = { 0.f, -1.f,  0.f}, .color = 0xFF00FFFF },

    // +Z
    { .position = {-1.f, -1.f,  1.f}, .texcoord = {0.f, 0.f}, .normal = { 0.f,  0.f,  1.f}, .color = 0x0000FFFF },
    { .position = { 1.f, -1.f,  1.f}, .texcoord = {1.f, 0.f}, .normal = { 0.f,  0.f,  1.f}, .color = 0x0000FFFF },
    { .position = { 1.f,  1.f,  1.f}, .texcoord = {1.f, 1.f}, .normal = { 0.f,  0.f,  1.f}, .color = 0x0000FFFF },
    { .position = {-1.f,  1.f,  1.f}, .texcoord = {0.f, 1.f}, .normal = { 0.f,  0.f,  1.f}, .color = 0x0000FFFF },

    // -Z
    { .position = {-1.f, -1.f, -1.f}, .texcoord = {0.f, 0.f}, .normal = { 0.f,  0.f, -1.f}, .color = 0xFFFF00FF },
    { .position = {-1.f,  1.f, -1.f}, .texcoord = {0.f, 1.f}, .normal = { 0.f,  0.f, -1.f}, .color = 0xFFFF00FF },
    { .position = { 1.f,  1.f, -1.f}, .texcoord = {1.f, 1.f}, .normal = { 0.f,  0.f, -1.f}, .color = 0xFFFF00FF },
    { .position = { 1.f, -1.f, -1.f}, .texcoord = {1.f, 0.f}, .normal = { 0.f,  0.f, -1.f}, .color = 0xFFFF00FF },
};

static const uint16_t cube_indices[] = {
     0,  1,  2,  0,  2,  3,
     4,  5,  6,  4,  6,  7,
     8,  9, 10,  8, 10, 11,
    12, 13, 14, 12, 14, 15,
    16, 17, 18, 16, 18, 19,
    20, 21, 22, 20, 22, 23,
};

static GLuint buffers[2];

void setup_cube()
{
    glGenBuffersARB(2, buffers);

    glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers[0]);
    glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW_ARB);

    glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffers[1]);
    glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW_ARB);
}

void draw_cube()
{
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers[0]);
    glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffers[1]);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glVertexPointer(3, GL_FLOAT, sizeof(vertex_t), (void*)(0*sizeof(float)));
    glTexCoordPointer(2, GL_FLOAT, sizeof(vertex_t), (void*)(3*sizeof(float)));
    glNormalPointer(GL_FLOAT, sizeof(vertex_t), (void*)(5*sizeof(float)));
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(vertex_t), (void*)(8*sizeof(float)));

    glDrawElements(GL_TRIANGLES, sizeof(cube_indices) / sizeof(uint16_t), GL_UNSIGNED_SHORT, 0);
}

#endif
