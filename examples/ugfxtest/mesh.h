#ifndef UGFXTEST_MESH_H
#define UGFXTEST_MESH_H

#include <ugfx.h>

#define make_vertex_n(vx, vy, vz, vs, vt, nx, ny, nz, va) \
{ \
    .x = float_to_fixed((vx), 5), \
    .y = float_to_fixed((vy), 5), \
    .z = float_to_fixed((vz), 5), \
    .s = float_to_fixed((vs * 32), 6), \
    .t = float_to_fixed((vt * 32), 6), \
    .attr.normal.x = (nx), \
    .attr.normal.y = (ny), \
    .attr.normal.z = (nz), \
    .attr.normal.a = (va), \
}

/* A constant vertex buffer. This could also be loaded from the cart. */
const ugfx_vertex_t mesh_vertices[] = {
    /* -Z */
    make_vertex_n(-1, -1, -1, 1, 1, 0, 0, -128, 255),
    make_vertex_n(+1, -1, -1, 0, 1, 0, 0, -128, 255),
    make_vertex_n(-1, +1, -1, 1, 0, 0, 0, -128, 255),
    make_vertex_n(+1, +1, -1, 0, 0, 0, 0, -128, 255),
    /* +Z */
    make_vertex_n(-1, -1, +1, 0, 1, 0, 0, +127, 255),
    make_vertex_n(+1, -1, +1, 1, 1, 0, 0, +127, 255),
    make_vertex_n(-1, +1, +1, 0, 0, 0, 0, +127, 255),
    make_vertex_n(+1, +1, +1, 1, 0, 0, 0, +127, 255),
    /* -Y */
    make_vertex_n(-1, -1, -1, 0, 1, 0, -128, 0, 255),
    make_vertex_n(+1, -1, -1, 1, 1, 0, -128, 0, 255),
    make_vertex_n(-1, -1, +1, 0, 0, 0, -128, 0, 255),
    make_vertex_n(+1, -1, +1, 1, 0, 0, -128, 0, 255),
    /* +Y */
    make_vertex_n(-1, +1, -1, 0, 0, 0, +127, 0, 255),
    make_vertex_n(+1, +1, -1, 1, 0, 0, +127, 0, 255),
    make_vertex_n(-1, +1, +1, 0, 1, 0, +127, 0, 255),
    make_vertex_n(+1, +1, +1, 1, 1, 0, +127, 0, 255),
    /* -X */
    make_vertex_n(-1, -1, -1, 0, 1, -128, 0, 0, 255),
    make_vertex_n(-1, +1, -1, 0, 0, -128, 0, 0, 255),
    make_vertex_n(-1, -1, +1, 1, 1, -128, 0, 0, 255),
    make_vertex_n(-1, +1, +1, 1, 0, -128, 0, 0, 255),
    /* +X */
    make_vertex_n(+1, -1, -1, 1, 1, +127, 0, 0, 255),
    make_vertex_n(+1, +1, -1, 1, 0, +127, 0, 0, 255),
    make_vertex_n(+1, -1, +1, 0, 1, +127, 0, 0, 255),
    make_vertex_n(+1, +1, +1, 0, 0, +127, 0, 0, 255),
};

/* A constant command list. This could also be loaded from the cart. */
const ugfx_command_t mesh_commands[] = {

    /* Load 24 vertices from address 0x0 to index 0 in the vertex cache, using address slot 1.
       The actual address the vertices will be dma'd from is determined by adding the
       address specified here to the value contained in the address slot.
       This means the command list can be constant at compile time, and the
       final address of the vertex buffer can be set dynamically at runtime. */
    ugfx_load_vertices(1, 0, 0, 24),

    /* Draw triangles. Note that the vertex indices are offsets into the vertex cache,
       and not into the original vertex buffer. The vertex cache can hold up to 32 vertices.
       So if your mesh consists of more than 32 vertices, multiple vertex loads are necessary. */
    ugfx_draw_triangle(0, 2, 1),
    ugfx_draw_triangle(1, 2, 3),

    ugfx_draw_triangle(4, 5, 6),
    ugfx_draw_triangle(5, 7, 6),

    ugfx_draw_triangle(8, 9, 10),
    ugfx_draw_triangle(9, 11, 10),

    ugfx_draw_triangle(12, 14, 13),
    ugfx_draw_triangle(13, 14, 15),

    ugfx_draw_triangle(16, 18, 17),
    ugfx_draw_triangle(17, 18, 19),

    ugfx_draw_triangle(20, 21, 22),
    ugfx_draw_triangle(21, 23, 22),

    /* Command lists always need to be terminated by 'ugfx_finalize'.
       This causes the command list processor to pop the current command list from the stack
       and either return to the one below, or finish processing if the stack is empty. */
    ugfx_finalize(),
};

const uint32_t mesh_commands_length = sizeof(mesh_commands) / sizeof(*mesh_commands);

#endif