#include "gl_internal.h"
#include "utils.h"
#include "rdpq.h"
#include "rdpq_mode.h"
#include <malloc.h>
#include <string.h>

extern gl_state_t state;

static const float clip_planes[CLIPPING_PLANE_COUNT][4] = {
    { 1, 0, 0, 1 },
    { 0, 1, 0, 1 },
    { 0, 0, 1, 1 },
    { 1, 0, 0, -1 },
    { 0, 1, 0, -1 },
    { 0, 0, 1, -1 },
};

void gl_clip_triangle();
void gl_clip_line();
void gl_clip_point();

uint8_t gl_points();
uint8_t gl_lines();
uint8_t gl_line_strip();
uint8_t gl_triangles();
uint8_t gl_triangle_strip();
uint8_t gl_triangle_fan();
uint8_t gl_quads();

void gl_reset_vertex_cache();

void gl_primitive_init()
{
    state.s_gen.mode = GL_EYE_LINEAR;
    state.s_gen.object_plane[0] = 1;
    state.s_gen.eye_plane[0] = 1;

    state.t_gen.mode = GL_EYE_LINEAR;
    state.t_gen.object_plane[1] = 1;
    state.t_gen.eye_plane[1] = 1;

    state.r_gen.mode = GL_EYE_LINEAR;
    state.q_gen.mode = GL_EYE_LINEAR;

    state.point_size = 1;
    state.line_width = 1;
    state.polygon_mode = GL_FILL;

    state.current_attribs[ATTRIB_COLOR][0] = 1;
    state.current_attribs[ATTRIB_COLOR][1] = 1;
    state.current_attribs[ATTRIB_COLOR][2] = 1;
    state.current_attribs[ATTRIB_COLOR][3] = 1;
    state.current_attribs[ATTRIB_TEXCOORD][3] = 1;
    state.current_attribs[ATTRIB_NORMAL][2] = 1;
}

void gl_primitive_close()
{
    for (uint32_t i = 0; i < ATTRIB_COUNT; i++)
    {
        gl_storage_free(&state.arrays[i].tmp_storage);
    }

    gl_storage_free(&state.tmp_index_storage);
}

bool gl_calc_is_points()
{
    switch (state.primitive_mode) {
    case GL_POINTS:
        return true;
    case GL_LINES:
    case GL_LINE_LOOP:
    case GL_LINE_STRIP:
        return false;
    default:
        return state.polygon_mode == GL_POINT;
    }
}

void glBegin(GLenum mode)
{
    if (state.immediate_active) {
        gl_set_error(GL_INVALID_OPERATION);
        return;
    }

    state.lock_next_vertex = false;

    switch (mode) {
    case GL_POINTS:
        state.prim_func = gl_points;
        state.prim_size = 1;
        break;
    case GL_LINES:
        state.prim_func = gl_lines;
        state.prim_size = 2;
        break;
    case GL_LINE_LOOP:
        // Line loop is equivalent to line strip, except for special case handled in glEnd
        state.prim_func = gl_line_strip;
        state.lock_next_vertex = true;
        state.prim_size = 2;
        break;
    case GL_LINE_STRIP:
        state.prim_func = gl_line_strip;
        state.prim_size = 2;
        break;
    case GL_TRIANGLES:
        state.prim_func = gl_triangles;
        state.prim_size = 3;
        break;
    case GL_TRIANGLE_STRIP:
        state.prim_func = gl_triangle_strip;
        state.prim_size = 3;
        break;
    case GL_TRIANGLE_FAN:
        state.prim_func = gl_triangle_fan;
        state.lock_next_vertex = true;
        state.prim_size = 3;
        break;
    case GL_QUADS:
        state.prim_func = gl_quads;
        state.prim_size = 3;
        break;
    case GL_QUAD_STRIP:
        // Quad strip is equivalent to triangle strip
        state.prim_func = gl_triangle_strip;
        state.prim_size = 3;
        break;
    case GL_POLYGON:
        // Polygon is equivalent to triangle fan
        state.prim_func = gl_triangle_fan;
        state.lock_next_vertex = true;
        state.prim_size = 3;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    state.immediate_active = true;
    state.primitive_mode = mode;
    state.prim_progress = 0;
    state.prim_counter = 0;

    if (gl_is_invisible()) {
        return;
    }

    gl_update_scissor();
    gl_update_texture();
    gl_update_blend_func();
    gl_update_fog();
    gl_update_rendermode();
    gl_update_combiner();
    gl_update_alpha_ref();
    gl_update_multisample();

    state.dirty_flags = 0;

    gl_reset_vertex_cache();
}

void glEnd(void)
{
    if (!state.immediate_active) {
        gl_set_error(GL_INVALID_OPERATION);
    }

    if (state.primitive_mode == GL_LINE_LOOP) {
        state.prim_indices[0] = state.prim_indices[1];
        state.prim_indices[1] = state.locked_vertex;
        state.prim_progress = 2;

        gl_clip_line();
    }

    state.immediate_active = false;
}

void gl_draw_point(gl_vertex_t *v0)
{
    GLfloat half_size = state.point_size * 0.5f;
    GLfloat p0[2] = { v0->screen_pos[0] - half_size, v0->screen_pos[1] - half_size };
    GLfloat p1[2] = { p0[0] + state.point_size, p0[1] + state.point_size };

    rdpq_set_prim_color(RGBA32(
        FLOAT_TO_U8(v0->color[0]),
        FLOAT_TO_U8(v0->color[1]),
        FLOAT_TO_U8(v0->color[2]),
        FLOAT_TO_U8(v0->color[3])
    ));

    if (state.depth_test || state.depth_mask) {
        rdpq_set_prim_depth(floorf(v0->depth), 0);
    }

    gl_texture_object_t *tex_obj = gl_get_active_texture();
    if (tex_obj != NULL && tex_obj->is_complete) {
        rdpq_texture_rectangle(0, p0[0], p0[1], p1[0], p1[1], v0->texcoord[0]/32.f, v0->texcoord[1]/32.f, 0, 0);
    } else {
        rdpq_fill_rectangle(p0[0], p0[1], p1[0], p1[1]);
    }
}

void gl_draw_line(gl_vertex_t *v0, gl_vertex_t *v1)
{
    uint8_t level = 0;
    int32_t tex_offset = -1;
    int32_t z_offset = -1;

    GLfloat perp[2] = { v0->screen_pos[1] - v1->screen_pos[1], v1->screen_pos[0] - v0->screen_pos[0] };
    GLfloat mag = sqrtf(perp[0]*perp[0] + perp[1]*perp[1]);
    if (mag == 0.0f) return;
    
    GLfloat width_factor = (state.line_width * 0.5f) / mag;
    perp[0] *= width_factor;
    perp[1] *= width_factor;

    gl_vertex_t line_vertices[4];

    line_vertices[0].screen_pos[0] = v0->screen_pos[0] + perp[0];
    line_vertices[0].screen_pos[1] = v0->screen_pos[1] + perp[1];
    line_vertices[1].screen_pos[0] = v0->screen_pos[0] - perp[0];
    line_vertices[1].screen_pos[1] = v0->screen_pos[1] - perp[1];

    line_vertices[2].screen_pos[0] = v1->screen_pos[0] + perp[0];
    line_vertices[2].screen_pos[1] = v1->screen_pos[1] + perp[1];
    line_vertices[3].screen_pos[0] = v1->screen_pos[0] - perp[0];
    line_vertices[3].screen_pos[1] = v1->screen_pos[1] - perp[1];
    
    memcpy(line_vertices[0].color, v0->color, sizeof(float) * 4);
    memcpy(line_vertices[1].color, v0->color, sizeof(float) * 4);
    memcpy(line_vertices[2].color, v1->color, sizeof(float) * 4);
    memcpy(line_vertices[3].color, v1->color, sizeof(float) * 4);

    gl_texture_object_t *tex_obj = gl_get_active_texture();
    if (tex_obj != NULL && tex_obj->is_complete) {
        tex_offset = 6;
        level = tex_obj->num_levels - 1;

        memcpy(line_vertices[0].texcoord, v0->texcoord, sizeof(float) * 3);
        memcpy(line_vertices[1].texcoord, v0->texcoord, sizeof(float) * 3);
        memcpy(line_vertices[2].texcoord, v1->texcoord, sizeof(float) * 3);
        memcpy(line_vertices[3].texcoord, v1->texcoord, sizeof(float) * 3);
    }

    if (state.depth_test) {
        z_offset = 9;

        line_vertices[0].depth = v0->depth;
        line_vertices[1].depth = v0->depth;
        line_vertices[2].depth = v1->depth;
        line_vertices[3].depth = v1->depth;
    }

    rdpq_triangle(0, level, 0, 2, tex_offset, z_offset, line_vertices[0].screen_pos, line_vertices[1].screen_pos, line_vertices[2].screen_pos);
    rdpq_triangle(0, level, 0, 2, tex_offset, z_offset, line_vertices[1].screen_pos, line_vertices[2].screen_pos, line_vertices[3].screen_pos);
}

void gl_draw_triangle(gl_vertex_t *v0, gl_vertex_t *v1, gl_vertex_t *v2)
{
    uint8_t level = 1;
    int32_t tex_offset = -1;

    gl_texture_object_t *tex_obj = gl_get_active_texture();
    if (tex_obj != NULL && tex_obj->is_complete) {
        tex_offset = 6;
        level = tex_obj->num_levels;
    }

    int32_t z_offset = state.depth_test ? 9 : -1;

    rdpq_triangle(0, level, 0, 2, tex_offset, z_offset, v0->screen_pos, v1->screen_pos, v2->screen_pos);
}

void gl_cull_triangle(gl_vertex_t *v0, gl_vertex_t *v1, gl_vertex_t *v2)
{
    if (state.cull_face_mode == GL_FRONT_AND_BACK) {
        return;
    }

    if (state.cull_face)
    {
        float winding = v0->screen_pos[0] * (v1->screen_pos[1] - v2->screen_pos[1]) +
                        v1->screen_pos[0] * (v2->screen_pos[1] - v0->screen_pos[1]) +
                        v2->screen_pos[0] * (v0->screen_pos[1] - v1->screen_pos[1]);
        
        bool is_front = (state.front_face == GL_CCW) ^ (winding > 0.0f);
        GLenum face = is_front ? GL_FRONT : GL_BACK;

        if (state.cull_face_mode == face) {
            return;
        }
    }
    
    switch (state.polygon_mode) {
    case GL_POINT:
        gl_draw_point(v0);
        gl_draw_point(v1);
        gl_draw_point(v2);
        break;
    case GL_LINE:
        gl_draw_line(v0, v1);
        gl_draw_line(v1, v2);
        gl_draw_line(v2, v0);
        break;
    case GL_FILL:
        gl_draw_triangle(v0, v1, v2);
        break;
    }
}

float dot_product4(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

void gl_vertex_calc_screenspace(gl_vertex_t *v)
{
    float inverse_w = 1.0f / v->position[3];

    v->screen_pos[0] = v->position[0] * inverse_w * state.current_viewport.scale[0] + state.current_viewport.offset[0];
    v->screen_pos[1] = v->position[1] * inverse_w * state.current_viewport.scale[1] + state.current_viewport.offset[1];

    v->depth = v->position[2] * inverse_w * state.current_viewport.scale[2] + state.current_viewport.offset[2];

    v->inverse_w = inverse_w;

    v->clip = 0;
    for (uint32_t i = 0; i < 3; i++)
    {
        if (v->position[i] < - v->position[3]) {
            v->clip |= 1 << i;
        } else if (v->position[i] > v->position[3]) {
            v->clip |= 1 << (i + 3);
        }
    }
}

void gl_intersect_line_plane(gl_vertex_t *intersection, const gl_vertex_t *p0, const gl_vertex_t *p1, const float *clip_plane)
{
    float d0 = dot_product4(p0->position, clip_plane);
    float d1 = dot_product4(p1->position, clip_plane);
    
    float a = d0 / (d0 - d1);

    assertf(a >= 0.f && a <= 1.f, "invalid a: %f", a);

    intersection->position[0] = lerp(p0->position[0], p1->position[0], a);
    intersection->position[1] = lerp(p0->position[1], p1->position[1], a);
    intersection->position[2] = lerp(p0->position[2], p1->position[2], a);
    intersection->position[3] = lerp(p0->position[3], p1->position[3], a);

    gl_vertex_calc_screenspace(intersection);

    intersection->color[0] = lerp(p0->color[0], p1->color[0], a);
    intersection->color[1] = lerp(p0->color[1], p1->color[1], a);
    intersection->color[2] = lerp(p0->color[2], p1->color[2], a);
    intersection->color[3] = lerp(p0->color[3], p1->color[3], a);

    intersection->texcoord[0] = lerp(p0->texcoord[0], p1->texcoord[0], a);
    intersection->texcoord[1] = lerp(p0->texcoord[1], p1->texcoord[1], a);
}

void gl_clip_triangle()
{
    gl_vertex_t *v0 = &state.vertex_cache[state.prim_indices[0]];
    gl_vertex_t *v1 = &state.vertex_cache[state.prim_indices[1]];
    gl_vertex_t *v2 = &state.vertex_cache[state.prim_indices[2]];

    if (v0->clip & v1->clip & v2->clip) {
        return;
    }

    // Flat shading
    if (state.shade_model == GL_FLAT) {
        v0->color[0] = v1->color[0] = v2->color[0];
        v0->color[1] = v1->color[1] = v2->color[1];
        v0->color[2] = v1->color[2] = v2->color[2];
        v0->color[3] = v1->color[3] = v2->color[3];
    }

    uint8_t any_clip = v0->clip | v1->clip | v2->clip;

    if (!any_clip) {
        gl_cull_triangle(v0, v1, v2);
        return;
    }

    // Polygon clipping using the Sutherland-Hodgman algorithm
    // See https://en.wikipedia.org/wiki/Sutherland%E2%80%93Hodgman_algorithm

    // Intersection points are stored in the clipping cache
    gl_vertex_t clipping_cache[CLIPPING_CACHE_SIZE];
    uint32_t cache_used = 0;

    gl_clipping_list_t lists[2];

    gl_clipping_list_t *in_list = &lists[0];
    gl_clipping_list_t *out_list = &lists[1];

    out_list->vertices[0] = v0;
    out_list->vertices[1] = v1;
    out_list->vertices[2] = v2;
    out_list->count = 3;
    
    for (uint32_t c = 0; c < CLIPPING_PLANE_COUNT; c++)
    {
        // If nothing clips this plane, skip it entirely
        if ((any_clip & (1<<c)) == 0) {
            continue;
        }

        const float *clip_plane = clip_planes[c];

        SWAP(in_list, out_list);
        out_list->count = 0;

        uint32_t cache_unused = 0;

        for (uint32_t i = 0; i < in_list->count; i++)
        {
            uint32_t prev_index = (i + in_list->count - 1) % in_list->count;

            gl_vertex_t *cur_point = in_list->vertices[i];
            gl_vertex_t *prev_point = in_list->vertices[prev_index];

            bool cur_inside = (cur_point->clip & (1<<c)) == 0;
            bool prev_inside = (prev_point->clip & (1<<c)) == 0;

            if (cur_inside ^ prev_inside) {
                gl_vertex_t *intersection = NULL;

                for (uint32_t n = 0; n < CLIPPING_CACHE_SIZE; n++)
                {
                    if ((cache_used & (1<<n)) == 0) {
                        intersection = &clipping_cache[n];
                        cache_used |= (1<<n);
                        break;
                    }
                }

                assertf(intersection, "clipping cache full!");
                assertf(intersection != cur_point, "invalid intersection");
                assertf(intersection != prev_point, "invalid intersection");

                gl_vertex_t *p0 = cur_point;
                gl_vertex_t *p1 = prev_point;

                // For consistent calculation of the intersection point
                if (prev_inside) {
                    SWAP(p0, p1);
                }

                gl_intersect_line_plane(intersection, p0, p1, clip_plane);

                out_list->vertices[out_list->count] = intersection;
                out_list->edge_flags[out_list->count] = cur_inside ? in_list->edge_flags[prev_index] : false;
                out_list->count++;
            }

            if (cur_inside) {
                out_list->vertices[out_list->count] = cur_point;
                out_list->edge_flags[out_list->count] = in_list->edge_flags[i];
                out_list->count++;
            } else {
                // If the point is in the clipping cache, remember it as unused
                uint32_t diff = cur_point - clipping_cache;
                if (diff >= 0 && diff < CLIPPING_CACHE_SIZE) {
                    cache_unused |= (1<<diff);
                }
            }
        }

        // Mark all points that were discarded as unused
        cache_used &= ~cache_unused;
    }

    for (uint32_t i = 2; i < out_list->count; i++)
    {
        gl_cull_triangle(out_list->vertices[0], out_list->vertices[i-1], out_list->vertices[i]);
    }
}

void gl_clip_line()
{
    gl_vertex_t *v0 = &state.vertex_cache[state.prim_indices[0]];
    gl_vertex_t *v1 = &state.vertex_cache[state.prim_indices[1]];

    if (v0->clip & v1->clip) {
        return;
    }

    // Flat shading
    if (state.shade_model == GL_FLAT) {
        v0->color[0] = v1->color[0];
        v0->color[1] = v1->color[1];
        v0->color[2] = v1->color[2];
        v0->color[3] = v1->color[3];
    }

    uint8_t any_clip = v0->clip | v1->clip;

    if (any_clip) {
        gl_vertex_t vertex_cache[2];

        for (uint32_t c = 0; c < CLIPPING_PLANE_COUNT; c++)
        {
            // If nothing clips this plane, skip it entirely
            if ((any_clip & (1<<c)) == 0) {
                continue;
            }

            bool v0_inside = (v0->clip & (1<<c)) == 0;
            bool v1_inside = (v1->clip & (1<<c)) == 0;

            if ((v0_inside ^ v1_inside) == 0) {
                continue;
            }

            gl_vertex_t *intersection = &vertex_cache[v0_inside ? 1 : 0];
            gl_intersect_line_plane(intersection, v0, v1, clip_planes[c]);

            if (v0_inside) {
                v1 = intersection;
            } else {
                v0 = intersection;
            }
        }
    }

    gl_draw_line(v0, v1);
}

void gl_clip_point()
{
    gl_vertex_t *v0 = &state.vertex_cache[state.prim_indices[0]];

    if (v0->clip) {
        return;
    }

    gl_draw_point(v0);
}

uint8_t gl_points()
{
    gl_clip_point();
    // Reset the progress to zero since we start with a completely new primitive that
    // won't share any vertices with the previous ones
    return 0;
}

uint8_t gl_lines()
{
    gl_clip_line();

    // Reset the progress to zero since we start with a completely new primitive that
    // won't share any vertices with the previous ones
    return 0;
}

uint8_t gl_line_strip()
{
    gl_clip_line();

    state.prim_indices[0] = state.prim_indices[1];

    return 1;
}

uint8_t gl_triangles()
{
    gl_clip_triangle();

    // Reset the progress to zero since we start with a completely new primitive that
    // won't share any vertices with the previous ones
    return 0;
}

uint8_t gl_triangle_strip()
{
    gl_clip_triangle();

    // Which vertices are shared depends on whether the primitive counter is odd or even
    state.prim_indices[state.prim_counter & 1] = state.prim_indices[2];

    // The next triangle will share two vertices with the previous one, so reset progress to 2
    return 2;
}

uint8_t gl_triangle_fan()
{
    gl_clip_triangle();

    state.prim_indices[1] = state.prim_indices[2];

    // The next triangle will share two vertices with the previous one, so reset progress to 2
    // It will always share the last one and the very first vertex that was specified.
    // To make sure the first vertex is not overwritten it was locked earlier (see glBegin)
    return 2;
}

uint8_t gl_quads()
{
    gl_clip_triangle();

    state.prim_indices[1] = state.prim_indices[2];

    // This is equivalent to state.prim_counter % 2 == 0 ? 2 : 0
    return ((state.prim_counter & 1) ^ 1) << 1;
}

void gl_prim_assembly(uint8_t cache_index)
{
    state.prim_indices[state.prim_progress] = cache_index;
    state.prim_progress++;

    if (state.prim_progress < state.prim_size) {
        return;
    }

    assert(state.prim_func != NULL);
    state.prim_progress = state.prim_func();
    state.prim_counter++;
}

void gl_calc_texture_coord(GLfloat *dest, const GLfloat *input, uint32_t coord_index, const gl_tex_gen_t *gen, const GLfloat *obj_pos, const GLfloat *eye_pos, const GLfloat *eye_normal)
{
    if (!gen->enabled) {
        dest[coord_index] = input[coord_index];
        return;
    }

    switch (gen->mode) {
    case GL_EYE_LINEAR:
        dest[coord_index] = eye_pos[0] * gen->eye_plane[0] +
                            eye_pos[1] * gen->eye_plane[1] +
                            eye_pos[2] * gen->eye_plane[2] +
                            eye_pos[3] * gen->eye_plane[3];
        break;
    case GL_OBJECT_LINEAR:
        dest[coord_index] = obj_pos[0] * gen->object_plane[0] +
                            obj_pos[1] * gen->object_plane[1] +
                            obj_pos[2] * gen->object_plane[2] +
                            obj_pos[3] * gen->object_plane[3];
        break;
    case GL_SPHERE_MAP:
        GLfloat norm_eye_pos[3];
        gl_normalize(norm_eye_pos, eye_pos);
        GLfloat d2 = 2.0f * dot_product3(norm_eye_pos, eye_normal);
        GLfloat r[3] = {
            norm_eye_pos[0] - eye_normal[0] * d2,
            norm_eye_pos[1] - eye_normal[1] * d2,
            norm_eye_pos[2] - eye_normal[2] * d2 + 1.0f,
        };
        GLfloat m = 1.0f / (2.0f * sqrtf(dot_product3(r, r)));
        dest[coord_index] = r[coord_index] * m + 0.5f;
        break;
    }
}

void gl_calc_texture_coords(GLfloat *dest, const GLfloat *input, const GLfloat *obj_pos, const GLfloat *eye_pos, const GLfloat *eye_normal)
{
    GLfloat tmp[4];

    gl_calc_texture_coord(tmp, input, 0, &state.s_gen, obj_pos, eye_pos, eye_normal);
    gl_calc_texture_coord(tmp, input, 1, &state.t_gen, obj_pos, eye_pos, eye_normal);
    gl_calc_texture_coord(tmp, input, 2, &state.r_gen, obj_pos, eye_pos, eye_normal);
    gl_calc_texture_coord(tmp, input, 3, &state.q_gen, obj_pos, eye_pos, eye_normal);

    // TODO: skip matrix multiplication if it is the identity
    gl_matrix_mult4x2(dest, gl_matrix_stack_get_matrix(&state.texture_stack), tmp);
}

void gl_vertex_t_l(uint8_t cache_index, const gl_matrix_t *mv, const gl_texture_object_t *tex_obj)
{
    gl_vertex_t *v = &state.vertex_cache[cache_index];

    GLfloat *pos = state.current_attribs[ATTRIB_VERTEX];
    GLfloat *color = state.current_attribs[ATTRIB_COLOR];
    GLfloat *texcoord = state.current_attribs[ATTRIB_TEXCOORD];
    GLfloat *normal = state.current_attribs[ATTRIB_NORMAL];

    GLfloat eye_pos[4];
    GLfloat eye_normal[3];

    bool is_texture_active = tex_obj != NULL && tex_obj->is_complete;

    if (state.lighting || state.fog || is_texture_active) {
        gl_matrix_mult(eye_pos, mv, pos);
    }

    if (state.lighting || is_texture_active) {
        gl_matrix_mult3x3(eye_normal, mv, normal);

        if (state.normalize) {
            gl_normalize(eye_normal, eye_normal);
        }
    }

    if (state.lighting) {
        gl_perform_lighting(v->color, color, eye_pos, eye_normal, &state.material);
    } else {
        v->color[0] = color[0];
        v->color[1] = color[1];
        v->color[2] = color[2];
        v->color[3] = color[3];
    }

    if (state.fog) {
        v->color[3] = (state.fog_end - fabsf(eye_pos[2])) / (state.fog_end - state.fog_start);
    }

    v->color[0] = CLAMP01(v->color[0]);
    v->color[1] = CLAMP01(v->color[1]);
    v->color[2] = CLAMP01(v->color[2]);
    v->color[3] = CLAMP01(v->color[3]);

    gl_matrix_mult(v->position, &state.final_matrix, pos);

    v->position[0] *= state.persp_norm_factor;
    v->position[1] *= state.persp_norm_factor;
    v->position[2] *= state.persp_norm_factor;
    v->position[3] *= state.persp_norm_factor;

    gl_vertex_calc_screenspace(v);

    if (is_texture_active) {
        gl_calc_texture_coords(v->texcoord, texcoord, pos, eye_pos, eye_normal);

        v->texcoord[0] *= tex_obj->levels[0].width;
        v->texcoord[1] *= tex_obj->levels[0].height;

        if (tex_obj->mag_filter == GL_LINEAR) {
            v->texcoord[0] -= 0.5f;
            v->texcoord[1] -= 0.5f;
        }
    }
}

typedef uint32_t (*read_index_func)(const void*,uint32_t);

uint32_t read_index_8(const uint8_t *src, uint32_t i)
{
    return src[i];
}

uint32_t read_index_16(const uint16_t *src, uint32_t i)
{
    return src[i];
}

uint32_t read_index_32(const uint32_t *src, uint32_t i)
{
    return src[i];
}

void gl_reset_vertex_cache()
{
    memset(state.vertex_cache_indices, 0xFF, sizeof(state.vertex_cache_indices));
    memset(state.lru_age_table, 0, sizeof(state.lru_age_table));
    state.lru_next_age = 1;
    state.locked_vertex = 0xFF;
}

bool gl_check_vertex_cache(uint32_t vert_index, uint8_t *cache_index)
{
    bool miss = true;

    uint32_t min_age = 0xFFFFFFFF;
    for (uint8_t ci = 0; ci < VERTEX_CACHE_SIZE; ci++)
    {
        if (state.vertex_cache_indices[ci] == vert_index) {
            miss = false;
            *cache_index = ci;
            break;
        }

        if (state.lru_age_table[ci] < min_age) {
            min_age = state.lru_age_table[ci];
            *cache_index = ci;
        }
    }

    if (state.lock_next_vertex) {
        state.lru_age_table[*cache_index] = 0xFFFFFFFF;
    } else {
        state.lru_age_table[*cache_index] = state.lru_next_age++;
    }

    state.vertex_cache_indices[*cache_index] = vert_index;

    return miss;
}

void gl_load_attribs(const gl_attrib_source_t *sources, const uint32_t index)
{
    static const GLfloat default_values[] = {0, 0, 0, 1};

    for (uint32_t i = 0; i < ATTRIB_COUNT; i++)
    {
        const gl_attrib_source_t *src = &sources[i];
        if (src->pointer == NULL) {
            continue;
        }

        GLfloat *dst = state.current_attribs[i];

        const void *p = src->pointer + (index - src->offset) * src->stride;
        src->read_func(dst, p, src->size);

        // Fill in the rest with default values
        for (uint32_t r = 3; r >= src->size; r--)
        {
            dst[r] = default_values[r];
        }
    }
}

void gl_draw(const gl_attrib_source_t *sources, uint32_t offset, uint32_t count, const void *indices, read_index_func read_index)
{
    if (sources[ATTRIB_VERTEX].pointer == NULL) {
        return;
    }

    const gl_matrix_t *mv = gl_matrix_stack_get_matrix(&state.modelview_stack);

    gl_texture_object_t *tex_obj = gl_get_active_texture();

    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t index;
        uint8_t cache_index = 0;
        bool miss = true;

        if (indices != NULL) {
            index = read_index(indices, i);
            miss = gl_check_vertex_cache(index, &cache_index);
        } else {
            index = offset + i;
            do {
                cache_index = state.next_cache_index;
                state.next_cache_index = (state.next_cache_index + 1) % VERTEX_CACHE_SIZE;
            } while (cache_index == state.locked_vertex);
        }
        
        if (miss) {
            gl_load_attribs(sources, index);
            gl_vertex_t_l(cache_index, mv, tex_obj);
        }

        if (state.lock_next_vertex) {
            state.locked_vertex = cache_index;
            state.lock_next_vertex = false;
        }

        gl_prim_assembly(cache_index);
    }
}

void read_u8(GLfloat *dst, const uint8_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = U8_TO_FLOAT(src[i]);
}

void read_i8(GLfloat *dst, const int8_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = I8_TO_FLOAT(src[i]);
}

void read_u16(GLfloat *dst, const uint16_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = U16_TO_FLOAT(src[i]);
}

void read_i16(GLfloat *dst, const int16_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = I16_TO_FLOAT(src[i]);
}

void read_u32(GLfloat *dst, const uint32_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = U32_TO_FLOAT(src[i]);
}

void read_i32(GLfloat *dst, const int32_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = I32_TO_FLOAT(src[i]);
}

void read_u8n(GLfloat *dst, const uint8_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

void read_i8n(GLfloat *dst, const int8_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

void read_u16n(GLfloat *dst, const uint16_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

void read_i16n(GLfloat *dst, const int16_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

void read_u32n(GLfloat *dst, const uint32_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

void read_i32n(GLfloat *dst, const int32_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

void read_f32(GLfloat *dst, const float *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

void read_f64(GLfloat *dst, const double *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

bool gl_array_copy_data(gl_array_t *array, uint32_t offset, uint32_t count, uint32_t elem_size, uint32_t stride)
{
    uint32_t buffer_size = elem_size * count;

    if (!gl_storage_resize(&array->tmp_storage, buffer_size)) {
        gl_set_error(GL_OUT_OF_MEMORY);
        return false;
    }

    for (uint32_t e = 0; e < count; e++)
    {
        void *dst_ptr = array->tmp_storage.data + e * elem_size;
        const void *src_ptr = array->pointer + (e + offset) * stride;
        memcpy(dst_ptr, src_ptr, elem_size);
    }

    return true;
}

bool gl_prepare_attrib_source(gl_attrib_source_t *attrib_src, gl_array_t *array, uint32_t offset, uint32_t count)
{
    if (!array->enabled) {
        attrib_src->pointer = NULL;
        return true;
    }

    uint32_t size_shift = 0;
    
    switch (array->type) {
    case GL_BYTE:
        attrib_src->read_func = array->normalize ? (read_attrib_func)read_i8n : (read_attrib_func)read_i8;
        size_shift = 0;
        break;
    case GL_UNSIGNED_BYTE:
        attrib_src->read_func = array->normalize ? (read_attrib_func)read_u8n : (read_attrib_func)read_u8;
        size_shift = 0;
        break;
    case GL_SHORT:
        attrib_src->read_func = array->normalize ? (read_attrib_func)read_i16n : (read_attrib_func)read_i16;
        size_shift = 1;
        break;
    case GL_UNSIGNED_SHORT:
        attrib_src->read_func = array->normalize ? (read_attrib_func)read_u16n : (read_attrib_func)read_u16;
        size_shift = 1;
        break;
    case GL_INT:
        attrib_src->read_func = array->normalize ? (read_attrib_func)read_i32n : (read_attrib_func)read_i32;
        size_shift = 2;
        break;
    case GL_UNSIGNED_INT:
        attrib_src->read_func = array->normalize ? (read_attrib_func)read_u32n : (read_attrib_func)read_u32;
        size_shift = 2;
        break;
    case GL_FLOAT:
        attrib_src->read_func = (read_attrib_func)read_f32;
        size_shift = 3;
        break;
    case GL_DOUBLE:
        attrib_src->read_func = (read_attrib_func)read_f64;
        size_shift = 3;
        break;
    }

    uint32_t elem_size = array->size << size_shift;

    attrib_src->size = array->size;

    uint32_t stride = array->stride;
    if (stride == 0) {
        stride = elem_size;
    }

    if (array->binding != NULL) {
        attrib_src->pointer = array->binding->storage.data + (uint32_t)array->pointer;
        attrib_src->offset = 0;
        attrib_src->stride = stride;
    } else {
        if (!gl_array_copy_data(array, offset, count, elem_size, stride)) {
            gl_set_error(GL_OUT_OF_MEMORY);
            return false;
        }

        attrib_src->pointer = array->tmp_storage.data;
        attrib_src->offset = offset;
        attrib_src->stride = elem_size;
    }

    return true;
}

bool gl_prepare_attrib_sources(uint32_t offset, uint32_t count)
{
    for (uint32_t i = 0; i < ATTRIB_COUNT; i++)
    {
        if (!gl_prepare_attrib_source(&state.attrib_sources[i], &state.arrays[i], offset, count)) {
            return false;
        }
    }

    return true;
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    switch (mode) {
    case GL_POINTS:
    case GL_LINES:
    case GL_LINE_LOOP:
    case GL_LINE_STRIP:
    case GL_TRIANGLES:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_QUADS:
    case GL_QUAD_STRIP:
    case GL_POLYGON:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    if (gl_prepare_attrib_sources(first, count)) {
        return;
    }

    glBegin(mode);
    gl_draw(state.attrib_sources, first, count, NULL, NULL);
    glEnd();
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    switch (mode) {
    case GL_POINTS:
    case GL_LINES:
    case GL_LINE_LOOP:
    case GL_LINE_STRIP:
    case GL_TRIANGLES:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_QUADS:
    case GL_QUAD_STRIP:
    case GL_POLYGON:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    read_index_func read_index;
    uint32_t index_size_shift = 0;

    switch (type) {
    case GL_UNSIGNED_BYTE:
        read_index = (read_index_func)read_index_8;
        index_size_shift = 0;
        break;
    case GL_UNSIGNED_SHORT:
        read_index = (read_index_func)read_index_16;
        index_size_shift = 1;
        break;
    case GL_UNSIGNED_INT:
        read_index = (read_index_func)read_index_32;
        index_size_shift = 2;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    if (state.element_array_buffer != NULL) {
        indices = state.element_array_buffer->storage.data + (uint32_t)indices;
    } else {
        uint32_t index_buffer_size = count << index_size_shift;

        if (!gl_storage_resize(&state.tmp_index_storage, index_buffer_size)) {
            gl_set_error(GL_OUT_OF_MEMORY);
            return;
        }

        memcpy(state.tmp_index_storage.data, indices, index_buffer_size);
        indices = state.tmp_index_storage.data;
    }

    uint32_t min_index = UINT32_MAX, max_index = 0;

    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t index = read_index(indices, i);
        min_index = MIN(min_index, index);
        max_index = MAX(max_index, index);
    }

    if (!gl_prepare_attrib_sources(min_index, max_index - min_index + 1)) {
        return;
    }

    glBegin(mode);
    gl_draw(state.attrib_sources, 0, count, indices, read_index);
    glEnd();
}

void glArrayElement(GLint i)
{
    // TODO: batch these

    if (!gl_prepare_attrib_sources(i, 1)) {
        return;
    }

    gl_draw(state.attrib_sources, i, 1, NULL, NULL);
}

static GLfloat vertex_tmp[4];
static gl_attrib_source_t dummy_sources[ATTRIB_COUNT] = {
    { .pointer = vertex_tmp, .size = 4, .stride = sizeof(GLfloat) * 4, .offset = 0, .read_func = (read_attrib_func)read_f32 },
    { .pointer = NULL },
    { .pointer = NULL },
    { .pointer = NULL },
};

void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    // TODO: batch these (?)

    vertex_tmp[0] = x;
    vertex_tmp[1] = y;
    vertex_tmp[2] = z;
    vertex_tmp[3] = w;

    gl_draw(dummy_sources, 0, 1, NULL, NULL);
}

void glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w)     { glVertex4f(x, y, z, w); }
void glVertex4i(GLint x, GLint y, GLint z, GLint w)             { glVertex4f(x, y, z, w); }
void glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) { glVertex4f(x, y, z, w); }

void glVertex3f(GLfloat x, GLfloat y, GLfloat z)    { glVertex4f(x, y, z, 1); }
void glVertex3s(GLshort x, GLshort y, GLshort z)    { glVertex3f(x, y, z); }
void glVertex3i(GLint x, GLint y, GLint z)          { glVertex3f(x, y, z); }
void glVertex3d(GLdouble x, GLdouble y, GLdouble z) { glVertex3f(x, y, z); }

void glVertex2f(GLfloat x, GLfloat y)   { glVertex4f(x, y, 0, 1); }
void glVertex2s(GLshort x, GLshort y)   { glVertex2f(x, y); }
void glVertex2i(GLint x, GLint y)       { glVertex2f(x, y); }
void glVertex2d(GLdouble x, GLdouble y) { glVertex2f(x, y); }

void glVertex2sv(const GLshort *v)  { glVertex2s(v[0], v[1]); }
void glVertex2iv(const GLint *v)    { glVertex2i(v[0], v[1]); }
void glVertex2fv(const GLfloat *v)  { glVertex2f(v[0], v[1]); }
void glVertex2dv(const GLdouble *v) { glVertex2d(v[0], v[1]); }

void glVertex3sv(const GLshort *v)  { glVertex3s(v[0], v[1], v[2]); }
void glVertex3iv(const GLint *v)    { glVertex3i(v[0], v[1], v[2]); }
void glVertex3fv(const GLfloat *v)  { glVertex3f(v[0], v[1], v[2]); }
void glVertex3dv(const GLdouble *v) { glVertex3d(v[0], v[1], v[2]); }

void glVertex4sv(const GLshort *v)  { glVertex4s(v[0], v[1], v[2], v[3]); }
void glVertex4iv(const GLint *v)    { glVertex4i(v[0], v[1], v[2], v[3]); }
void glVertex4fv(const GLfloat *v)  { glVertex4f(v[0], v[1], v[2], v[3]); }
void glVertex4dv(const GLdouble *v) { glVertex4d(v[0], v[1], v[2], v[3]); }

void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    state.current_attribs[ATTRIB_COLOR][0] = r;
    state.current_attribs[ATTRIB_COLOR][1] = g;
    state.current_attribs[ATTRIB_COLOR][2] = b;
    state.current_attribs[ATTRIB_COLOR][3] = a;
}

void glColor4d(GLdouble r, GLdouble g, GLdouble b, GLdouble a)  { glColor4f(r, g, b, a); }
void glColor4b(GLbyte r, GLbyte g, GLbyte b, GLbyte a)          { glColor4f(I8_TO_FLOAT(r), I8_TO_FLOAT(g), I8_TO_FLOAT(b), I8_TO_FLOAT(a)); }
void glColor4s(GLshort r, GLshort g, GLshort b, GLshort a)      { glColor4f(I16_TO_FLOAT(r), I16_TO_FLOAT(g), I16_TO_FLOAT(b), I16_TO_FLOAT(a)); }
void glColor4i(GLint r, GLint g, GLint b, GLint a)              { glColor4f(I32_TO_FLOAT(r), I32_TO_FLOAT(g), I32_TO_FLOAT(b), I32_TO_FLOAT(a)); }
void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)     { glColor4f(U8_TO_FLOAT(r), U8_TO_FLOAT(g), U8_TO_FLOAT(b), U8_TO_FLOAT(a)); }
void glColor4us(GLushort r, GLushort g, GLushort b, GLushort a) { glColor4f(U16_TO_FLOAT(r), U16_TO_FLOAT(g), U16_TO_FLOAT(b), U16_TO_FLOAT(a)); }
void glColor4ui(GLuint r, GLuint g, GLuint b, GLuint a)         { glColor4f(U32_TO_FLOAT(r), U32_TO_FLOAT(g), U32_TO_FLOAT(b), U32_TO_FLOAT(a)); }

void glColor3f(GLfloat r, GLfloat g, GLfloat b)     { glColor4f(r, g, b, 1.f); }
void glColor3d(GLdouble r, GLdouble g, GLdouble b)  { glColor3f(r, g, b); }
void glColor3b(GLbyte r, GLbyte g, GLbyte b)        { glColor3f(I8_TO_FLOAT(r), I8_TO_FLOAT(g), I8_TO_FLOAT(b)); }
void glColor3s(GLshort r, GLshort g, GLshort b)     { glColor3f(I16_TO_FLOAT(r), I16_TO_FLOAT(g), I16_TO_FLOAT(b)); }
void glColor3i(GLint r, GLint g, GLint b)           { glColor3f(I32_TO_FLOAT(r), I32_TO_FLOAT(g), I32_TO_FLOAT(b)); }
void glColor3ub(GLubyte r, GLubyte g, GLubyte b)    { glColor3f(U8_TO_FLOAT(r), U8_TO_FLOAT(g), U8_TO_FLOAT(b)); }
void glColor3us(GLushort r, GLushort g, GLushort b) { glColor3f(U16_TO_FLOAT(r), U16_TO_FLOAT(g), U16_TO_FLOAT(b)); }
void glColor3ui(GLuint r, GLuint g, GLuint b)       { glColor3f(U32_TO_FLOAT(r), U32_TO_FLOAT(g), U32_TO_FLOAT(b)); }

void glColor3bv(const GLbyte *v)    { glColor3b(v[0], v[1], v[2]); }
void glColor3sv(const GLshort *v)   { glColor3s(v[0], v[1], v[2]); }
void glColor3iv(const GLint *v)     { glColor3i(v[0], v[1], v[2]); }
void glColor3fv(const GLfloat *v)   { glColor3f(v[0], v[1], v[2]); }
void glColor3dv(const GLdouble *v)  { glColor3d(v[0], v[1], v[2]); }
void glColor3ubv(const GLubyte *v)  { glColor3ub(v[0], v[1], v[2]); }
void glColor3usv(const GLushort *v) { glColor3us(v[0], v[1], v[2]); }
void glColor3uiv(const GLuint *v)   { glColor3ui(v[0], v[1], v[2]); }

void glColor4bv(const GLbyte *v)    { glColor4b(v[0], v[1], v[2], v[3]); }
void glColor4sv(const GLshort *v)   { glColor4s(v[0], v[1], v[2], v[3]); }
void glColor4iv(const GLint *v)     { glColor4i(v[0], v[1], v[2], v[3]); }
void glColor4fv(const GLfloat *v)   { glColor4f(v[0], v[1], v[2], v[3]); }
void glColor4dv(const GLdouble *v)  { glColor4d(v[0], v[1], v[2], v[3]); }
void glColor4ubv(const GLubyte *v)  { glColor4ub(v[0], v[1], v[2], v[3]); }
void glColor4usv(const GLushort *v) { glColor4us(v[0], v[1], v[2], v[3]); }
void glColor4uiv(const GLuint *v)   { glColor4ui(v[0], v[1], v[2], v[3]); }

void glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
    state.current_attribs[ATTRIB_TEXCOORD][0] = s;
    state.current_attribs[ATTRIB_TEXCOORD][1] = t;
    state.current_attribs[ATTRIB_TEXCOORD][2] = r;
    state.current_attribs[ATTRIB_TEXCOORD][3] = q;
}

void glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q)       { glTexCoord4f(s, t, r, q); }
void glTexCoord4i(GLint s, GLint t, GLint r, GLint q)               { glTexCoord4f(s, t, r, q); }
void glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q)   { glTexCoord4f(s, t, r, q); }

void glTexCoord3f(GLfloat s, GLfloat t, GLfloat r)      { glTexCoord4f(s, t, r, 1.0f); }
void glTexCoord3s(GLshort s, GLshort t, GLshort r)      { glTexCoord3f(s, t, r); }
void glTexCoord3i(GLint s, GLint t, GLint r)            { glTexCoord3f(s, t, r); }
void glTexCoord3d(GLdouble s, GLdouble t, GLdouble r)   { glTexCoord3f(s, t, r); }

void glTexCoord2f(GLfloat s, GLfloat t)     { glTexCoord4f(s, t, 0.0f, 1.0f); }
void glTexCoord2s(GLshort s, GLshort t)     { glTexCoord2f(s, t); }
void glTexCoord2i(GLint s, GLint t)         { glTexCoord2f(s, t); }
void glTexCoord2d(GLdouble s, GLdouble t)   { glTexCoord2f(s, t); }

void glTexCoord1f(GLfloat s)    { glTexCoord4f(s, 0.0f, 0.0f, 1.0f); }
void glTexCoord1s(GLshort s)    { glTexCoord1f(s); }
void glTexCoord1i(GLint s)      { glTexCoord1f(s); }
void glTexCoord1d(GLdouble s)   { glTexCoord1f(s); }

void glTexCoord1sv(const GLshort *v)    { glTexCoord1s(v[0]); }
void glTexCoord1iv(const GLint *v)      { glTexCoord1i(v[0]); }
void glTexCoord1fv(const GLfloat *v)    { glTexCoord1f(v[0]); }
void glTexCoord1dv(const GLdouble *v)   { glTexCoord1d(v[0]); }

void glTexCoord2sv(const GLshort *v)    { glTexCoord2s(v[0], v[1]); }
void glTexCoord2iv(const GLint *v)      { glTexCoord2i(v[0], v[1]); }
void glTexCoord2fv(const GLfloat *v)    { glTexCoord2f(v[0], v[1]); }
void glTexCoord2dv(const GLdouble *v)   { glTexCoord2d(v[0], v[1]); }

void glTexCoord3sv(const GLshort *v)    { glTexCoord3s(v[0], v[1], v[2]); }
void glTexCoord3iv(const GLint *v)      { glTexCoord3i(v[0], v[1], v[2]); }
void glTexCoord3fv(const GLfloat *v)    { glTexCoord3f(v[0], v[1], v[2]); }
void glTexCoord3dv(const GLdouble *v)   { glTexCoord3d(v[0], v[1], v[2]); }

void glTexCoord4sv(const GLshort *v)    { glTexCoord4s(v[0], v[1], v[2], v[3]); }
void glTexCoord4iv(const GLint *v)      { glTexCoord4i(v[0], v[1], v[2], v[3]); }
void glTexCoord4fv(const GLfloat *v)    { glTexCoord4f(v[0], v[1], v[2], v[3]); }
void glTexCoord4dv(const GLdouble *v)   { glTexCoord4d(v[0], v[1], v[2], v[3]); }

void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
    state.current_attribs[ATTRIB_NORMAL][0] = nx;
    state.current_attribs[ATTRIB_NORMAL][1] = ny;
    state.current_attribs[ATTRIB_NORMAL][2] = nz;
}

void glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz)        { glNormal3f(I8_TO_FLOAT(nx), I8_TO_FLOAT(ny), I8_TO_FLOAT(nz)); }
void glNormal3s(GLshort nx, GLshort ny, GLshort nz)     { glNormal3f(I16_TO_FLOAT(nx), I16_TO_FLOAT(ny), I16_TO_FLOAT(nz)); }
void glNormal3i(GLint nx, GLint ny, GLint nz)           { glNormal3f(I32_TO_FLOAT(nx), I32_TO_FLOAT(ny), I32_TO_FLOAT(nz)); }
void glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz)  { glNormal3f(nx, ny, nz); }

void glNormal3bv(const GLbyte *v)   { glNormal3b(v[0], v[1], v[2]); }
void glNormal3sv(const GLshort *v)  { glNormal3s(v[0], v[1], v[2]); }
void glNormal3iv(const GLint *v)    { glNormal3i(v[0], v[1], v[2]); }
void glNormal3fv(const GLfloat *v)  { glNormal3f(v[0], v[1], v[2]); }
void glNormal3dv(const GLdouble *v) { glNormal3d(v[0], v[1], v[2]); }

void glPointSize(GLfloat size)
{
    if (size <= 0.0f) {
        gl_set_error(GL_INVALID_VALUE);
        return;
    }

    state.point_size = size;
}

void glLineWidth(GLfloat width)
{
    if (width <= 0.0f) {
        gl_set_error(GL_INVALID_VALUE);
        return;
    }

    state.line_width = width;
}

void glPolygonMode(GLenum face, GLenum mode)
{
    switch (face) {
    case GL_FRONT:
    case GL_BACK:
        assertf(0, "Separate polygon modes for front and back faces are not supported!");
        break;
    case GL_FRONT_AND_BACK:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    switch (mode) {
    case GL_POINT:
    case GL_LINE:
    case GL_FILL:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    GL_SET_STATE_FLAG(state.polygon_mode, mode, DIRTY_FLAG_RENDERMODE | DIRTY_FLAG_COMBINER);
}

void glDepthRange(GLclampd n, GLclampd f)
{
    state.current_viewport.scale[2] = (f - n) * 0.5f;
    state.current_viewport.offset[2] = n + (f - n) * 0.5f;
}

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    uint32_t fbh = state.cur_framebuffer->color_buffer->height;

    state.current_viewport.scale[0] = w * 0.5f;
    state.current_viewport.scale[1] = h * -0.5f;
    state.current_viewport.offset[0] = x + w * 0.5f;
    state.current_viewport.offset[1] = fbh - y - h * 0.5f;
}

gl_tex_gen_t *gl_get_tex_gen(GLenum coord)
{
    switch (coord) {
    case GL_S:
        return &state.s_gen;
    case GL_T:
        return &state.t_gen;
    case GL_R:
        return &state.r_gen;
    case GL_Q:
        return &state.q_gen;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return NULL;
    }
}

void gl_tex_gen_set_mode(gl_tex_gen_t *gen, GLenum coord, GLint param)
{
    switch (param) {
    case GL_OBJECT_LINEAR:
    case GL_EYE_LINEAR:
        break;
    case GL_SPHERE_MAP:
        if (coord == GL_R || coord == GL_Q) {
            gl_set_error(GL_INVALID_ENUM);
            return;
        }
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    gen->mode = param;
}

void glTexGeni(GLenum coord, GLenum pname, GLint param)
{
    gl_tex_gen_t *gen = gl_get_tex_gen(coord);
    if (gen == NULL) {
        return;
    }

    if (pname != GL_TEXTURE_GEN_MODE) {
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    gl_tex_gen_set_mode(gen, coord, param);
}

void glTexGenf(GLenum coord, GLenum pname, GLfloat param) { glTexGeni(coord, pname, param); }
void glTexGend(GLenum coord, GLenum pname, GLdouble param) { glTexGeni(coord, pname, param); }

void glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params)
{
    gl_tex_gen_t *gen = gl_get_tex_gen(coord);
    if (gen == NULL) {
        return;
    }

    switch (pname) {
    case GL_TEXTURE_GEN_MODE:
        gl_tex_gen_set_mode(gen, coord, params[0]);
        break;
    case GL_OBJECT_PLANE:
        gen->object_plane[0] = params[0];
        gen->object_plane[1] = params[1];
        gen->object_plane[2] = params[2];
        gen->object_plane[3] = params[3];
        break;
    case GL_EYE_PLANE:
        gen->eye_plane[0] = params[0];
        gen->eye_plane[1] = params[1];
        gen->eye_plane[2] = params[2];
        gen->eye_plane[3] = params[3];
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glTexGeniv(GLenum coord, GLenum pname, const GLint *params)
{
    gl_tex_gen_t *gen = gl_get_tex_gen(coord);
    if (gen == NULL) {
        return;
    }

    switch (pname) {
    case GL_TEXTURE_GEN_MODE:
        gl_tex_gen_set_mode(gen, coord, params[0]);
        break;
    case GL_OBJECT_PLANE:
        gen->object_plane[0] = params[0];
        gen->object_plane[1] = params[1];
        gen->object_plane[2] = params[2];
        gen->object_plane[3] = params[3];
        break;
    case GL_EYE_PLANE:
        gen->eye_plane[0] = params[0];
        gen->eye_plane[1] = params[1];
        gen->eye_plane[2] = params[2];
        gen->eye_plane[3] = params[3];
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glTexGendv(GLenum coord, GLenum pname, const GLdouble *params)
{
    gl_tex_gen_t *gen = gl_get_tex_gen(coord);
    if (gen == NULL) {
        return;
    }

    switch (pname) {
    case GL_TEXTURE_GEN_MODE:
        gl_tex_gen_set_mode(gen, coord, params[0]);
        break;
    case GL_OBJECT_PLANE:
        gen->object_plane[0] = params[0];
        gen->object_plane[1] = params[1];
        gen->object_plane[2] = params[2];
        gen->object_plane[3] = params[3];
        break;
    case GL_EYE_PLANE:
        gen->eye_plane[0] = params[0];
        gen->eye_plane[1] = params[1];
        gen->eye_plane[2] = params[2];
        gen->eye_plane[3] = params[3];
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glCullFace(GLenum mode)
{
    switch (mode) {
    case GL_BACK:
    case GL_FRONT:
    case GL_FRONT_AND_BACK:
        state.cull_face_mode = mode;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glFrontFace(GLenum dir)
{
    switch (dir) {
    case GL_CW:
    case GL_CCW:
        state.front_face = dir;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}
