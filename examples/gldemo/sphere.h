#ifndef SPHERE_H
#define SPHERE_H

#include <GL/gl.h>
#include <math.h>

#include "vertex.h"

#define SPHERE_RADIUS       20.0f
#define SPHERE_MIN_RINGS    4
#define SPHERE_MAX_RINGS    64
#define SPHERE_MIN_SEGMENTS 4
#define SPHERE_MAX_SEGMENTS 64

static GLuint sphere_buffers[2];
static uint32_t sphere_rings;
static uint32_t sphere_segments;
static uint32_t sphere_vertex_count;
static uint32_t sphere_index_count;

void setup_sphere()
{
    glGenBuffersARB(2, sphere_buffers);
    sphere_rings = 8;
    sphere_segments = 8;
}

void make_sphere_vertex(vertex_t *dst, uint32_t ring, uint32_t segment)
{
    float r = SPHERE_RADIUS;
    float phi = (M_TWOPI * segment) / sphere_segments;
    float theta = (M_PI * ring) / (sphere_rings + 1);

    float sintheta = sin(theta);

    float x = r * cosf(phi) * sintheta;
    float y = r * sinf(phi) * sintheta;
    float z = r * cosf(theta);

    dst->position[0] = x;
    dst->position[1] = y;
    dst->position[2] = z;

    float mag2 = x*x + y*y + z*z;
    float mag = sqrtf(mag2);
    float inv_m = 1.0f / mag;

    dst->normal[0] = -x * inv_m;
    dst->normal[1] = -y * inv_m;
    dst->normal[2] = -z * inv_m;

    dst->texcoord[0] = segment & 1 ? 1.0f : 0.0f;
    dst->texcoord[1] = ring & 1 ? 1.0f : 0.0f;
}

void make_sphere_mesh()
{
    sphere_vertex_count = sphere_rings * sphere_segments + 2;

    glBindBufferARB(GL_ARRAY_BUFFER_ARB, sphere_buffers[0]);
    glBufferDataARB(GL_ARRAY_BUFFER_ARB, sphere_vertex_count * sizeof(vertex_t), NULL, GL_STATIC_DRAW_ARB);

    vertex_t *vertices = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);

    make_sphere_vertex(&vertices[0], 0, 0);
    
    for (uint32_t r = 0; r < sphere_rings; r++)
    {
        for (uint32_t s = 0; s < sphere_segments; s++)
        {
            make_sphere_vertex(&vertices[r * sphere_segments + s + 1], r + 1, s);
        }
    }

    make_sphere_vertex(&vertices[sphere_vertex_count - 1], sphere_rings + 1, 0);
    
    glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);

    uint32_t fan_index_count = sphere_segments + 2;
    uint32_t ring_index_count = sphere_segments * 6;

    sphere_index_count = fan_index_count * 2 + ring_index_count * (sphere_rings - 1);

    glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sphere_buffers[1]);
    glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sphere_index_count * sizeof(uint16_t), NULL, GL_STATIC_DRAW_ARB);

    uint16_t *indices = glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);

    // Ends
    for (uint32_t i = 0; i < fan_index_count - 1; i++)
    {
        indices[i] = i;
        indices[fan_index_count + i] = sphere_vertex_count - i - 1;
    }
    indices[sphere_segments + 1] = 1;
    indices[fan_index_count + sphere_segments + 1] = sphere_vertex_count - 2;

    uint32_t rings_index_offset = fan_index_count * 2;

    // Rings
    for (uint32_t r = 0; r < sphere_rings - 1; r++)
    {
        uint16_t *ring_indices = &indices[rings_index_offset + r * ring_index_count];
        uint16_t first_ring_index = 1 + r * sphere_segments;
        uint16_t second_ring_index = 1 + (r + 1) * sphere_segments;

        for (uint32_t s = 0; s < sphere_segments; s++)
        {
            uint16_t next_segment = (s + 1) % sphere_segments;
            ring_indices[s * 6 + 0] = first_ring_index + s;
            ring_indices[s * 6 + 1] = second_ring_index + s;
            ring_indices[s * 6 + 2] = first_ring_index + next_segment;
            ring_indices[s * 6 + 3] = second_ring_index + s;
            ring_indices[s * 6 + 4] = second_ring_index + next_segment;
            ring_indices[s * 6 + 5] = first_ring_index + next_segment;
        }
    }

    glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
}

void draw_sphere()
{
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, sphere_buffers[0]);
    glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sphere_buffers[1]);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    glVertexPointer(3, GL_FLOAT, sizeof(vertex_t), (void*)(0*sizeof(float)));
    glTexCoordPointer(2, GL_FLOAT, sizeof(vertex_t), (void*)(3*sizeof(float)));
    glNormalPointer(GL_FLOAT, sizeof(vertex_t), (void*)(5*sizeof(float)));
    //glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(vertex_t), (void*)(8*sizeof(float)));

    glDrawElements(GL_TRIANGLE_FAN, sphere_segments + 2, GL_UNSIGNED_SHORT, 0);
    glDrawElements(GL_TRIANGLE_FAN, sphere_segments + 2, GL_UNSIGNED_SHORT, (void*)((sphere_segments + 2) * sizeof(uint16_t)));
    glDrawElements(GL_TRIANGLES, (sphere_rings - 1) * (sphere_segments * 6), GL_UNSIGNED_SHORT, (void*)((sphere_segments + 2) * 2 * sizeof(uint16_t)));
}

#endif
