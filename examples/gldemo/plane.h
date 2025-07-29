#ifndef PLANE_H
#define PLANE_H

#include <libdragon.h>
#include <GL/gl.h>
#include <math.h>

#include "vertex.h"

#define PLANE_SIZE       20.0f
#define PLANE_SEGMENTS   16

static GLuint plane_buffers[2];
static GLuint plane_array;
static uint32_t plane_vertex_count;
static uint32_t plane_index_count;

static uint32_t first_idx, end_idx;

void setup_plane()
{
    glGenBuffersARB(2, plane_buffers);

    glGenVertexArrays(1, &plane_array);
    glBindVertexArray(plane_array);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, plane_buffers[0]);

    glVertexPointer(3, GL_FLOAT, sizeof(vertex_t), (void*)(offsetof(vertex_t, position)));
    glNormalPointer(GL_FLOAT, sizeof(vertex_t), (void*)(offsetof(vertex_t, normal)));
    glTexCoordPointer(2, GL_FLOAT, sizeof(vertex_t), (void*)(offsetof(vertex_t, texcoord)));
    
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

    glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, plane_buffers[1]);

    glBindVertexArray(0);
}

void make_plane_mesh()
{
    plane_vertex_count = (PLANE_SEGMENTS + 1) * (PLANE_SEGMENTS + 1);

    glBindBufferARB(GL_ARRAY_BUFFER_ARB, plane_buffers[0]);
    glBufferDataARB(GL_ARRAY_BUFFER_ARB, plane_vertex_count * sizeof(vertex_t), NULL, GL_STATIC_DRAW_ARB);

    const float p0 = - (PLANE_SIZE / 2);
    const float incr = PLANE_SIZE / PLANE_SEGMENTS;

    vertex_t *vertices = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);

    for (uint32_t y = 0; y < PLANE_SEGMENTS + 1; y++)
    {
        for (uint32_t x = 0; x < PLANE_SEGMENTS + 1; x++)
        {
            uint32_t i = y * (PLANE_SEGMENTS + 1) + x;
            vertex_t *v = &vertices[i];

            v->position[0] = p0 + incr * x;
            v->position[1] = 0;
            v->position[2] = p0 + incr * y;

            v->normal[0] = 0;
            v->normal[1] = 1;
            v->normal[2] = 0;

            v->texcoord[0] = x;
            v->texcoord[1] = y;
        }
    }
    
    glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

    plane_index_count = PLANE_SEGMENTS * PLANE_SEGMENTS * 6;
    first_idx = 0;
    end_idx = plane_index_count;

    glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, plane_buffers[1]);
    glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, plane_index_count * sizeof(uint16_t), NULL, GL_STATIC_DRAW_ARB);

    uint16_t *indices = glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);

    for (uint32_t y = 0; y < PLANE_SEGMENTS; y++)
    {
        for (uint32_t x = 0; x < PLANE_SEGMENTS; x++)
        {
            uint32_t i = (y * PLANE_SEGMENTS + x) * 6;

            uint32_t row_start = y * (PLANE_SEGMENTS + 1);
            uint32_t next_row_start = (y + 1) * (PLANE_SEGMENTS + 1);

            indices[i + 0] = x + row_start;
            indices[i + 1] = x + next_row_start;
            indices[i + 2] = x + row_start + 1;
            indices[i + 3] = x + next_row_start;
            indices[i + 4] = x + next_row_start + 1;
            indices[i + 5] = x + row_start + 1;
        }
    }

    glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
    glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
}

void modify_vertices(float t)
{
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, plane_buffers[0]);
    vertex_t *vertices = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_READ_WRITE_ARB);
    for (size_t i = 0; i < plane_vertex_count; i++)
    {
        fm_vec3_t xz = {{ vertices[i].position[0], 0.f, vertices[i].position[2] }};
        float len = fm_vec3_len(&xz);
        vertices[i].position[1] = fm_sinf(len + t*10);
    }
    glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
}

void draw_plane()
{
    glBindVertexArray(plane_array);
    glDrawElements(GL_TRIANGLES, end_idx - first_idx, GL_UNSIGNED_SHORT, (const GLvoid*)(sizeof(uint16_t) * first_idx));
    glBindVertexArray(0);
}

void render_plane()
{
    rdpq_debug_log_msg("Plane");

    draw_plane();
}

#endif
