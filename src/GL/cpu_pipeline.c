/**
 * @file cpu_pipeline.c
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 * @brief OpenGL CPU-based rendering pipeline implementation.
 */
#include "gl_internal.h"
#include "rdpq_rect.h"

extern gl_state_t *state;

static const float clip_planes[CLIPPING_PLANE_COUNT][4] = {
    { 1, 0, 0, GUARD_BAND_FACTOR },
    { 0, 1, 0, GUARD_BAND_FACTOR },
    { 0, 0, 1, 1 },
    { 1, 0, 0, -GUARD_BAND_FACTOR },
    { 0, 1, 0, -GUARD_BAND_FACTOR },
    { 0, 0, 1, -1 },
};

static void read_i8(GLfloat *dst, const int8_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

static void read_i16(GLfloat *dst, const int16u_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

static void read_i32(GLfloat *dst, const int32u_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

static void read_u8n(GLfloat *dst, const uint8_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = U8_TO_FLOAT(src[i]);
}

static void read_i8n(GLfloat *dst, const int8_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = I8_TO_FLOAT(src[i]);
}

static void read_u16n(GLfloat *dst, const uint16u_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = U16_TO_FLOAT(src[i]);
}

static void read_i16n(GLfloat *dst, const int16u_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = I16_TO_FLOAT(src[i]);
}

static void read_u32n(GLfloat *dst, const uint32u_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = U32_TO_FLOAT(src[i]);
}

static void read_i32n(GLfloat *dst, const int32u_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = I32_TO_FLOAT(src[i]);
}

static void read_f32(GLfloat *dst, const floatu *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

static void read_f64(GLfloat *dst, const doubleu *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

static void read_x16_vtx(GLfloat *dst, const int16u_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i] * state->vertex_halfx_precision.to_float_factor;
}

static void read_x16_tex(GLfloat *dst, const int16u_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i] * state->texcoord_halfx_precision.to_float_factor;
}

static void read_u8_i(GLubyte *dst, const uint8_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

static void read_u16_i(GLubyte *dst, const uint16u_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

static void read_u32_i(GLubyte *dst, const uint32u_t *src, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) dst[i] = src[i];
}

static void read_packed565n(GLfloat *dst, const uint16u_t *src, uint32_t count)
{
    uint16_t v = *src;
    dst[0] = ((v>>11)&0x1F) / 15.5f;
    dst[1] = ((v>>5)&0x3F) / 31.5f;
    dst[2] = ((v>>0)&0x1F) / 15.5f;
}

const cpu_read_attrib_func cpu_read_funcs[ATTRIB_COUNT][ATTRIB_TYPE_COUNT] = {
    {
        (cpu_read_attrib_func)read_i8,
        NULL,
        (cpu_read_attrib_func)read_i16,
        NULL,
        (cpu_read_attrib_func)read_i32,
        NULL,
        (cpu_read_attrib_func)read_f32,
        (cpu_read_attrib_func)read_f64,
        (cpu_read_attrib_func)read_x16_vtx,
        NULL,
    },
    {
        (cpu_read_attrib_func)read_i8n,
        NULL,
        (cpu_read_attrib_func)read_i16n,
        NULL,
        (cpu_read_attrib_func)read_i32n,
        NULL,
        (cpu_read_attrib_func)read_f32,
        (cpu_read_attrib_func)read_f64,
        NULL,
        (cpu_read_attrib_func)read_packed565n,
    },
    {
        (cpu_read_attrib_func)read_i8n,
        (cpu_read_attrib_func)read_u8n,
        (cpu_read_attrib_func)read_i16n,
        (cpu_read_attrib_func)read_u16n,
        (cpu_read_attrib_func)read_i32n,
        (cpu_read_attrib_func)read_u32n,
        (cpu_read_attrib_func)read_f32,
        (cpu_read_attrib_func)read_f64,
        NULL,
        NULL,
    },
    {
        (cpu_read_attrib_func)read_i8,
        NULL,
        (cpu_read_attrib_func)read_i16,
        NULL,
        (cpu_read_attrib_func)read_i32,
        NULL,
        (cpu_read_attrib_func)read_f32,
        (cpu_read_attrib_func)read_f64,
        (cpu_read_attrib_func)read_x16_tex,
        NULL,
    },
    {
        NULL,
        (cpu_read_attrib_func)read_u8_i,
        NULL,
        (cpu_read_attrib_func)read_u16_i,
        NULL,
        (cpu_read_attrib_func)read_u32_i,
        NULL,
        NULL,
        NULL,
        NULL,
    },
};

uint8_t gl_points();
uint8_t gl_lines();
uint8_t gl_line_strip();
uint8_t gl_triangles();
uint8_t gl_triangle_strip();
uint8_t gl_triangle_fan();
uint8_t gl_quads();

void gl_reset_vertex_cache();

static void gl_clip_triangle();
static void gl_clip_line();
static void gl_clip_point();

bool gl_init_prim_assembly(GLenum mode)
{
    state->lock_next_vertex = false;

    switch (mode) {
    case GL_POINTS:
        state->prim_func = gl_points;
        state->prim_size = 1;
        break;
    case GL_LINES:
        state->prim_func = gl_lines;
        state->prim_size = 2;
        break;
    case GL_LINE_LOOP:
        // Line loop is equivalent to line strip, except for special case handled in glEnd
        state->prim_func = gl_line_strip;
        state->prim_size = 2;
        state->lock_next_vertex = true;
        break;
    case GL_LINE_STRIP:
        state->prim_func = gl_line_strip;
        state->prim_size = 2;
        break;
    case GL_TRIANGLES:
        state->prim_func = gl_triangles;
        state->prim_size = 3;
        break;
    case GL_TRIANGLE_STRIP:
        state->prim_func = gl_triangle_strip;
        state->prim_size = 3;
        break;
    case GL_TRIANGLE_FAN:
        state->prim_func = gl_triangle_fan;
        state->prim_size = 3;
        state->lock_next_vertex = true;
        break;
    case GL_QUADS:
        state->prim_func = gl_quads;
        state->prim_size = 3;
        break;
    case GL_QUAD_STRIP:
        // Quad strip is equivalent to triangle strip
        state->prim_func = gl_triangle_strip;
        state->prim_size = 3;
        break;
    case GL_POLYGON:
        // Polygon is equivalent to triangle fan
        state->prim_func = gl_triangle_fan;
        state->prim_size = 3;
        state->lock_next_vertex = true;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid primitive mode", mode);
        return false;
    }

    state->primitive_mode = mode;
    state->prim_progress = 0;
    state->prim_counter = 0;
    state->prim_id = 0x80000000;

    return true;
}

void gl_reset_vertex_cache()
{
    memset(state->vertex_cache_ids, 0, sizeof(state->vertex_cache_ids));
    memset(state->lru_age_table, 0, sizeof(state->lru_age_table));
    state->lru_next_age = 1;
}

bool gl_check_vertex_cache(uint32_t id, uint8_t *cache_index, bool lock)
{
    const uint32_t INFINITE_AGE = 0xFFFFFFFF; // infinitely recent

    bool miss = true;

    uint32_t min_age = INFINITE_AGE;
    for (uint8_t ci = 0; ci < VERTEX_CACHE_SIZE; ci++)
    {
        if (state->vertex_cache_ids[ci] == id) {
            miss = false;
            *cache_index = ci;
            break;
        }

        if (state->lru_age_table[ci] < min_age) {
            min_age = state->lru_age_table[ci];
            *cache_index = ci;
        }
    }

    uint32_t age = lock ? INFINITE_AGE : state->lru_next_age++;
    state->lru_age_table[*cache_index] = age;
    state->vertex_cache_ids[*cache_index] = id;

    return miss;
}

bool gl_get_cache_index(uint32_t vertex_id, uint8_t *cache_index)
{
    bool result = gl_check_vertex_cache(vertex_id + 1, cache_index, state->lock_next_vertex);

    if (state->lock_next_vertex) {
        state->lock_next_vertex = false;
        state->locked_vertex = *cache_index;
    }

    return result;
}

uint8_t gl_points()
{
    // Reset the progress to zero since we start with a completely new primitive that
    // won't share any vertices with the previous ones
    return 0;
}

uint8_t gl_lines()
{
    // Reset the progress to zero since we start with a completely new primitive that
    // won't share any vertices with the previous ones
    return 0;
}

uint8_t gl_line_strip()
{
    state->prim_indices[0] = state->prim_indices[1];

    return 1;
}

uint8_t gl_triangles()
{
    // Reset the progress to zero since we start with a completely new primitive that
    // won't share any vertices with the previous ones
    return 0;
}

uint8_t gl_triangle_strip()
{
    // Which vertices are shared depends on whether the primitive counter is odd or even
    state->prim_indices[state->prim_counter] = state->prim_indices[2];
    state->prim_counter ^= 1;

    // The next triangle will share two vertices with the previous one, so reset progress to 2
    return 2;
}

uint8_t gl_triangle_fan()
{
    state->prim_indices[1] = state->prim_indices[2];

    // The next triangle will share two vertices with the previous one, so reset progress to 2
    // It will always share the last one and the very first vertex that was specified.
    // To make sure the first vertex is not overwritten it was locked earlier (see glBegin)
    return 2;
}

uint8_t gl_quads()
{
    state->prim_indices[1] = state->prim_indices[2];

    state->prim_counter ^= 1;
    return state->prim_counter << 1;
}

bool gl_prim_assembly(uint8_t cache_index, uint8_t *indices)
{
    if (state->lock_next_vertex) {
        state->lock_next_vertex = false;
        state->locked_vertex = cache_index;
    }

    state->prim_indices[state->prim_progress] = cache_index;
    state->prim_progress++;

    if (state->prim_progress < state->prim_size) {
        return false;
    }

    memcpy(indices, state->prim_indices, state->prim_size * sizeof(uint8_t));

    assert(state->prim_func != NULL);
    state->prim_progress = state->prim_func();
    return true;
}

static void gl_init_cpu_pipe()
{
    gl_texture_object_t *tex_obj = gl_get_active_texture();
    if (tex_obj != NULL && gl_tex_is_complete(tex_obj)) {
        state->prim_texture = true;
        state->prim_mipmaps = gl_tex_get_levels(tex_obj);
        state->prim_tex_width = tex_obj->srv_object->levels[0].width;
        state->prim_tex_height = tex_obj->srv_object->levels[0].height;
        state->prim_bilinear = tex_obj->srv_object->mag_filter == GL_LINEAR ||
                            tex_obj->srv_object->min_filter == GL_LINEAR ||
                            tex_obj->srv_object->min_filter == GL_LINEAR_MIPMAP_NEAREST ||
                            tex_obj->srv_object->min_filter == GL_LINEAR_MIPMAP_LINEAR;
    } else {
        state->prim_texture = false;
        state->prim_mipmaps = 0;
        state->prim_tex_width = 0;
        state->prim_tex_height = 0;
        state->prim_bilinear = false;
    }

    state->trifmt = (rdpq_trifmt_t){
        .pos_offset = VTX_SCREEN_POS_OFFSET,
        .shade_offset = VTX_SHADE_OFFSET,
        .shade_flat = state->shade_model == GL_FLAT,
        .tex_offset = state->prim_texture ? VTX_TEXCOORD_OFFSET : -1,
        .tex_mipmaps = state->prim_mipmaps,
        .z_offset = state->depth_test ? VTX_DEPTH_OFFSET : -1,
    };

    gl_update_matrix_targets();
}

static float dot_product4(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

static float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

static uint8_t gl_get_clip_codes(GLfloat *pos, GLfloat *ref)
{
    // This corresponds to vcl + vch on RSP
    uint8_t codes = 0;
    for (uint32_t i = 0; i < 3; i++)
    {
        if (pos[i] < - ref[i]) {
            codes |= 1 << i;
        } else if (pos[i] > ref[i]) {
            codes |= 1 << (i + 3);
        }
    }
    return codes;
}

static gl_matrix_target_t* gl_get_matrix_target(uint8_t mtx_index)
{
    if (state->matrix_palette_enabled) {
        return &state->palette_matrix_targets[mtx_index];
    }

    return &state->default_matrix_target;
}

static void gl_vertex_pre_tr(uint8_t cache_index)
{
    gl_vtx_t *v = &state->vertex_cache[cache_index];

    memcpy(&v->obj_attributes, &state->current_attributes, sizeof(gl_obj_attributes_t));

    gl_matrix_target_t* mtx_target = gl_get_matrix_target(v->obj_attributes.mtx_index[0]);
    gl_matrix_mult(v->cs_pos, &mtx_target->mvp, v->obj_attributes.position);

#if 0
    debugf("VTX ID: %d\n", id);
    debugf("     OBJ: %8.2f %8.2f %8.2f %8.2f\n", v->obj_pos[0], v->obj_pos[1],v->obj_pos[2], v->obj_pos[3]);
    debugf("          [%08lx %08lx %08lx %08lx]\n",
        fx16(OBJ_SCALE*v->obj_pos[0]), fx16(OBJ_SCALE*v->obj_pos[1]), fx16(OBJ_SCALE*v->obj_pos[2]), fx16(OBJ_SCALE*v->obj_pos[3]));
    debugf("   CSPOS: %8.2f %8.2f %8.2f %8.2f\n", v->cs_pos[0], v->cs_pos[1], v->cs_pos[2], v->cs_pos[3]);
    debugf("          [%08lx %08lx %08lx %08lx]\n", fx16(OBJ_SCALE*v->cs_pos[0]), fx16(OBJ_SCALE*v->cs_pos[1]), fx16(OBJ_SCALE*v->cs_pos[2]), fx16(OBJ_SCALE*v->cs_pos[3]));
#endif

    GLfloat tr_ref[] = {
        v->cs_pos[3],
        v->cs_pos[3],
        v->cs_pos[3]
    };
    
    v->tr_code = gl_get_clip_codes(v->cs_pos, tr_ref);
    v->t_l_applied = false;
}

static void gl_calc_texture_coord(GLfloat *dest, const GLfloat *input, uint32_t coord_index, const gl_tex_gen_t *gen, const GLfloat *obj_pos, const GLfloat *eye_pos, const GLfloat *eye_normal)
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

static void gl_calc_texture_coords(GLfloat *dest, const GLfloat *input, const GLfloat *obj_pos, const GLfloat *eye_pos, const GLfloat *eye_normal)
{
    GLfloat tmp[TEX_COORD_COUNT];
    GLfloat result[TEX_COORD_COUNT];

    for (uint32_t i = 0; i < TEX_GEN_COUNT; i++)
    {
        gl_calc_texture_coord(tmp, input, i, &state->tex_gen[i], obj_pos, eye_pos, eye_normal);
    }

    // TODO: skip matrix multiplication if it is the identity
    gl_matrix_mult(result, gl_matrix_stack_get_matrix(&state->texture_stack), tmp);

    GLfloat inv_q = 1.0f / result[3];

    dest[0] = result[0] * inv_q;
    dest[1] = result[1] * inv_q;
}

static void gl_vertex_calc_clip_code(gl_vtx_t *v)
{
    GLfloat clip_ref[] = { 
        v->cs_pos[3] * GUARD_BAND_FACTOR,
        v->cs_pos[3] * GUARD_BAND_FACTOR,
        v->cs_pos[3]
    };

    v->clip_code = gl_get_clip_codes(v->cs_pos, clip_ref);
}

static void gl_vertex_calc_screenspace(gl_vtx_t *v)
{
    v->inv_w = v->cs_pos[3] != 0.0f ? 1.0f / v->cs_pos[3] : 0x7FFF;

    v->screen_pos[0] = v->cs_pos[0] * v->inv_w * state->current_viewport.scale[0] + state->current_viewport.offset[0];
    v->screen_pos[1] = v->cs_pos[1] * v->inv_w * state->current_viewport.scale[1] + state->current_viewport.offset[1];

    v->depth = v->cs_pos[2] * v->inv_w * state->current_viewport.scale[2] + state->current_viewport.offset[2];
}

static void gl_vertex_t_l(gl_vtx_t *vtx)
{
    gl_matrix_target_t* mtx_target = gl_get_matrix_target(vtx->obj_attributes.mtx_index[0]);
    gl_matrix_t *mv = gl_matrix_stack_get_matrix(mtx_target->mv_stack);

    GLfloat eye_pos[4];
    GLfloat eye_normal[3];

    if (state->lighting || state->fog || state->prim_texture) {
        gl_matrix_mult(eye_pos, mv, vtx->obj_attributes.position);
    }

    if (state->lighting || state->prim_texture) {
        // TODO: use inverse transpose matrix
        gl_matrix_mult3x3(eye_normal, mv, vtx->obj_attributes.normal);

        if (state->normalize) {
            gl_normalize(eye_normal, eye_normal);
        }
    }

    if (state->lighting) {
        gl_perform_lighting(vtx->shade, vtx->obj_attributes.color, eye_pos, eye_normal, &state->material);
    } else {
        memcpy(vtx->shade, vtx->obj_attributes.color, sizeof(GLfloat) * 4);
    }

    if (state->fog) {
        vtx->shade[3] = (state->fog_offset - fabsf(eye_pos[2])) * state->fog_factor;
    }

    vtx->shade[0] = CLAMP01(vtx->shade[0]);
    vtx->shade[1] = CLAMP01(vtx->shade[1]);
    vtx->shade[2] = CLAMP01(vtx->shade[2]);
    vtx->shade[3] = CLAMP01(vtx->shade[3]);

    if (state->prim_texture) {
        gl_calc_texture_coords(vtx->texcoord, vtx->obj_attributes.texcoord, vtx->obj_attributes.position, eye_pos, eye_normal);

        vtx->texcoord[0] = vtx->texcoord[0] * state->prim_tex_width;
        vtx->texcoord[1] = (state->tex_flip_t ? 1.f - vtx->texcoord[1] : vtx->texcoord[1]) * state->prim_tex_height;

        if (state->prim_bilinear) {
            vtx->texcoord[0] -= 0.5f;
            vtx->texcoord[1] -= 0.5f;
        }
    }

    gl_vertex_calc_screenspace(vtx);
    gl_vertex_calc_clip_code(vtx);
}

static gl_vtx_t * gl_get_screen_vtx(uint8_t cache_index)
{
    gl_vtx_t *vtx = &state->vertex_cache[cache_index];

    if (!vtx->t_l_applied) {
        // If there was a cache miss, perform T&L
        gl_vertex_t_l(vtx);
        vtx->t_l_applied = true;
    }

    return vtx;
}

static void gl_draw_primitive(const uint8_t *indices)
{
    uint8_t tr_codes = 0xFF;
    for (uint8_t i = 0; i < state->prim_size; i++)
    {
        tr_codes &= state->vertex_cache[indices[i]].tr_code;
    }
    
    // Trivial rejection
    if (tr_codes) {
        return;
    }

    for (uint8_t i = 0; i < state->prim_size; i++)
    {
        state->primitive_vertices[i] = gl_get_screen_vtx(indices[i]);
        #if 0
        gl_vtx_t *v = state->primitive_vertices[i];
        debugf("VTX %d:\n", i);
        debugf("    cpos: (%.4f, %.4f, %.4f, %.4f) [%08lx, %08lx, %08lx, %08lx]\n", 
            v->cs_pos[0],v->cs_pos[1],v->cs_pos[2],v->cs_pos[3],
            fx16(v->cs_pos[0]*65536), fx16(v->cs_pos[1]*65536),
            fx16(v->cs_pos[2]*65536), fx16(v->cs_pos[3]*65536));
        debugf("    screen: (%.2f, %.2f) [%08lx, %08lx]\n", 
            v->screen_pos[0], v->screen_pos[1],
            (uint32_t)(int32_t)(v->screen_pos[0] * 4),
            (uint32_t)(int32_t)(v->screen_pos[1] * 4));
        if (state->prim_texture) {
            debugf("      tex: (%.2f, %.2f) [%08lx, %08lx]\n", 
                v->texcoord[0], v->texcoord[1],
                (uint32_t)(int32_t)(v->texcoord[0] * 32),
                (uint32_t)(int32_t)(v->texcoord[1] * 32));
            rdpq_debug_log(true);
            state->cull_face = 0;
        }
        #endif
    }
    
    switch (state->prim_size) {
    case 1:
        gl_clip_point();
        break;
    case 2:
        gl_clip_line();
        break;
    case 3:
        gl_clip_triangle();
        break;
    }
}

static void gl_draw_point(gl_vtx_t *v0)
{
    GLfloat half_size = state->point_size * 0.5f;
    GLfloat p0[2] = { v0->screen_pos[0] - half_size, v0->screen_pos[1] - half_size };
    GLfloat p1[2] = { p0[0] + state->point_size, p0[1] + state->point_size };

    rdpq_set_prim_color(RGBA32(
        FLOAT_TO_U8(v0->shade[0]),
        FLOAT_TO_U8(v0->shade[1]),
        FLOAT_TO_U8(v0->shade[2]),
        FLOAT_TO_U8(v0->shade[3])
    ));

    if (state->depth_test) {
        rdpq_set_prim_depth_raw(v0->depth * 0x7FFF, 0);
    }

    if (state->prim_texture) {
        rdpq_texture_rectangle_scaled(0, p0[0], p0[1], p1[0], p1[1], v0->texcoord[0]/32.f, v0->texcoord[1]/32.f, v0->texcoord[0]/32.f+1, v0->texcoord[0]/32.f+1);
    } else {
        rdpq_fill_rectangle(p0[0], p0[1], p1[0], p1[1]);
    }
}

static void gl_draw_line(gl_vtx_t *v0, gl_vtx_t *v1)
{
    GLfloat perp[2] = { v0->screen_pos[1] - v1->screen_pos[1], v1->screen_pos[0] - v0->screen_pos[0] };
    GLfloat mag = sqrtf(perp[0]*perp[0] + perp[1]*perp[1]);
    if (mag == 0.0f) return;
    
    GLfloat width_factor = (state->line_width * 0.5f) / mag;
    perp[0] *= width_factor;
    perp[1] *= width_factor;

    gl_vtx_t line_vertices[4];

    line_vertices[0].screen_pos[0] = v0->screen_pos[0] + perp[0];
    line_vertices[0].screen_pos[1] = v0->screen_pos[1] + perp[1];
    line_vertices[1].screen_pos[0] = v0->screen_pos[0] - perp[0];
    line_vertices[1].screen_pos[1] = v0->screen_pos[1] - perp[1];

    line_vertices[2].screen_pos[0] = v1->screen_pos[0] + perp[0];
    line_vertices[2].screen_pos[1] = v1->screen_pos[1] + perp[1];
    line_vertices[3].screen_pos[0] = v1->screen_pos[0] - perp[0];
    line_vertices[3].screen_pos[1] = v1->screen_pos[1] - perp[1];
    
    if (state->shade_model == GL_FLAT) {
        memcpy(line_vertices[0].shade, v1->shade, sizeof(float) * 4);
        memcpy(line_vertices[1].shade, v1->shade, sizeof(float) * 4);
    } else {
        memcpy(line_vertices[0].shade, v0->shade, sizeof(float) * 4);
        memcpy(line_vertices[1].shade, v0->shade, sizeof(float) * 4);
    }
    
    memcpy(line_vertices[2].shade, v1->shade, sizeof(float) * 4);
    memcpy(line_vertices[3].shade, v1->shade, sizeof(float) * 4);

    if (state->prim_texture) {
        memcpy(line_vertices[0].texcoord, v0->texcoord, sizeof(float) * 3);
        memcpy(line_vertices[1].texcoord, v0->texcoord, sizeof(float) * 3);
        memcpy(line_vertices[2].texcoord, v1->texcoord, sizeof(float) * 3);
        memcpy(line_vertices[3].texcoord, v1->texcoord, sizeof(float) * 3);
    }

    if (state->depth_test) {
        line_vertices[0].depth = v0->depth;
        line_vertices[1].depth = v0->depth;
        line_vertices[2].depth = v1->depth;
        line_vertices[3].depth = v1->depth;
    }

    rdpq_triangle(&state->trifmt, (const float*)&line_vertices[0], (const float*)&line_vertices[1], (const float*)&line_vertices[2]);
    rdpq_triangle(&state->trifmt, (const float*)&line_vertices[1], (const float*)&line_vertices[2], (const float*)&line_vertices[3]);
}

static void gl_draw_triangle(gl_vtx_t *v0, gl_vtx_t *v1, gl_vtx_t *v2)
{
    rdpq_triangle(&state->trifmt, (const float*)v2, (const float*)v0, (const float*)v1);
}

static void gl_cull_triangle(gl_vtx_t *v0, gl_vtx_t *v1, gl_vtx_t *v2)
{
    if (state->cull_face)
    {
        if (state->cull_face_mode == GL_FRONT_AND_BACK) {
            return;
        }
        
        float winding = v0->screen_pos[0] * (v1->screen_pos[1] - v2->screen_pos[1]) +
                        v1->screen_pos[0] * (v2->screen_pos[1] - v0->screen_pos[1]) +
                        v2->screen_pos[0] * (v0->screen_pos[1] - v1->screen_pos[1]);
        
        bool is_front = (state->front_face == GL_CCW) ^ (winding > 0.0f);
        GLenum face = is_front ? GL_FRONT : GL_BACK;

        if (state->cull_face_mode == face) {
            return;
        }
    }

    if (state->shade_model == GL_FLAT) {
        memcpy(v2->shade, state->flat_color, sizeof(state->flat_color));
    }
    
    switch (state->polygon_mode) {
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

static void gl_intersect_line_plane(gl_vtx_t *intersection, const gl_vtx_t *p0, const gl_vtx_t *p1, const float *clip_plane)
{
    float d0 = dot_product4(p0->cs_pos, clip_plane);
    float d1 = dot_product4(p1->cs_pos, clip_plane);
    
    float a = d0 / (d0 - d1);

    assertf(a >= 0.f && a <= 1.f, "invalid a: %f", a);

    intersection->cs_pos[0] = lerp(p0->cs_pos[0], p1->cs_pos[0], a);
    intersection->cs_pos[1] = lerp(p0->cs_pos[1], p1->cs_pos[1], a);
    intersection->cs_pos[2] = lerp(p0->cs_pos[2], p1->cs_pos[2], a);
    intersection->cs_pos[3] = lerp(p0->cs_pos[3], p1->cs_pos[3], a);

    intersection->shade[0] = lerp(p0->shade[0], p1->shade[0], a);
    intersection->shade[1] = lerp(p0->shade[1], p1->shade[1], a);
    intersection->shade[2] = lerp(p0->shade[2], p1->shade[2], a);
    intersection->shade[3] = lerp(p0->shade[3], p1->shade[3], a);

    intersection->texcoord[0] = lerp(p0->texcoord[0], p1->texcoord[0], a);
    intersection->texcoord[1] = lerp(p0->texcoord[1], p1->texcoord[1], a);

    gl_vertex_calc_clip_code(intersection);
}

static void gl_clip_triangle()
{
    gl_vtx_t *v0 = state->primitive_vertices[0];
    gl_vtx_t *v1 = state->primitive_vertices[1];
    gl_vtx_t *v2 = state->primitive_vertices[2];

    // Flat shading
    if (state->shade_model == GL_FLAT) {
        memcpy(state->flat_color, v2->shade, sizeof(state->flat_color));
    }

    uint8_t any_clip = v0->clip_code | v1->clip_code | v2->clip_code;

    if (!any_clip) {
        gl_cull_triangle(v0, v1, v2);
        return;
    }

    // Polygon clipping using the Sutherland-Hodgman algorithm
    // See https://en.wikipedia.org/wiki/Sutherland%E2%80%93Hodgman_algorithm

    // Intersection points are stored in the clipping cache
    gl_vtx_t clipping_cache[CLIPPING_CACHE_SIZE];
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

        for (uint32_t i = 0; i < in_list->count; i++)
        {
            uint32_t prev_index = (i + in_list->count - 1) % in_list->count;

            gl_vtx_t *cur_point = in_list->vertices[i];
            gl_vtx_t *prev_point = in_list->vertices[prev_index];

            bool cur_inside = (cur_point->clip_code & (1<<c)) == 0;
            bool prev_inside = (prev_point->clip_code & (1<<c)) == 0;

            if (cur_inside ^ prev_inside) {
                gl_vtx_t *intersection = NULL;

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

                gl_vtx_t *p0 = cur_point;
                gl_vtx_t *p1 = prev_point;

                // For consistent calculation of the intersection point
                if (prev_inside) {
                    SWAP(p0, p1);
                }

                gl_intersect_line_plane(intersection, p0, p1, clip_plane);

                out_list->vertices[out_list->count] = intersection;
                out_list->count++;
            }

            if (cur_inside) {
                out_list->vertices[out_list->count] = cur_point;
                out_list->count++;
            } else {
                // If the point is in the clipping cache, remember it as unused
                uint32_t diff = cur_point - clipping_cache;
                if (diff >= 0 && diff < CLIPPING_CACHE_SIZE) {
                    cache_used &= ~(1<<diff);
                }
            }
        }
    }

    for (uint32_t i = 0; i < out_list->count; i++)
    {
        gl_vertex_calc_screenspace(out_list->vertices[i]);
        
        if (i > 1) {
            gl_cull_triangle(out_list->vertices[0], out_list->vertices[i-1], out_list->vertices[i]);
        }
    }
}

static void gl_clip_line()
{
    gl_vtx_t *v0 = state->primitive_vertices[0];
    gl_vtx_t *v1 = state->primitive_vertices[1];
    gl_vtx_t vertex_cache[2];

    uint8_t any_clip = v0->clip_code | v1->clip_code;

    uint8_t clipped = 0;
    if (any_clip) {

        for (uint32_t c = 0; c < CLIPPING_PLANE_COUNT; c++)
        {
            // If nothing clips this plane, skip it entirely
            if ((any_clip & (1<<c)) == 0) {
                continue;
            }

            bool v0_inside = (v0->clip_code & (1<<c)) == 0;
            bool v1_inside = (v1->clip_code & (1<<c)) == 0;

            if ((v0_inside ^ v1_inside) == 0) {
                continue;
            }

            gl_vtx_t *intersection = &vertex_cache[v0_inside ? 1 : 0];
            gl_intersect_line_plane(intersection, v0, v1, clip_planes[c]);

            if (v0_inside) {
                v1 = intersection;
                clipped |= 1<<1;
            } else {
                v0 = intersection;
                clipped |= 1<<0;
            }
        }
    }

    if (clipped & (1<<0)) gl_vertex_calc_screenspace(v0);
    if (clipped & (1<<1)) gl_vertex_calc_screenspace(v1);
    gl_draw_line(v0, v1);
}

static void gl_clip_point()
{
    gl_vtx_t *v0 = state->primitive_vertices[0];
    gl_draw_point(v0);
}

static void submit_vertex(uint32_t cache_index)
{
    uint8_t indices[3];
    if (gl_prim_assembly(cache_index, indices))
    {
        gl_draw_primitive(indices);
    }
}

static void draw_vertex_from_arrays(const gl_array_t *arrays, uint32_t id, uint32_t index)
{
    uint8_t cache_index;
    if (gl_get_cache_index(id, &cache_index))
    {
        gl_load_attribs(arrays, index);
        gl_vertex_pre_tr(cache_index);
    }

    submit_vertex(cache_index);
}

static void gl_cpu_begin(GLenum mode)
{
    // FIXME: This is pessimistically marking everything as used, even if textures are turned off
    //        CAUTION: texture state is owned by the RSP currently, so how can we determine this?
    __rdpq_autosync_use(AUTOSYNC_PIPE | AUTOSYNC_TILES | AUTOSYNC_TMEM(0));

    gl_init_prim_assembly(mode);
    gl_reset_vertex_cache();
    gl_init_cpu_pipe();
}

static void gl_cpu_end()
{
    if (state->primitive_mode == GL_LINE_LOOP) {
        // Close line loop
        state->prim_indices[0] = state->prim_indices[1];
        state->prim_indices[1] = state->locked_vertex;

        gl_draw_primitive(state->prim_indices);
    }

    gl_set_current_color(state->current_attributes.color);
    gl_set_current_texcoords(state->current_attributes.texcoord);
    gl_set_current_normal(state->current_attributes.normal);
    gl_set_current_mtx_index(state->current_attributes.mtx_index);
}

void gl_read_attrib(gl_array_type_t array_type, const void *value, GLenum type, uint32_t size)
{
    cpu_read_attrib_func read_func = cpu_read_funcs[array_type][gl_type_to_index(type)];
    void *dst = gl_get_attrib_pointer(&state->current_attributes, array_type);
    read_func(dst, value, size);
    if (array_type != ATTRIB_MTX_INDEX) {
        gl_fill_attrib_defaults(array_type, size);
    }
}

static void gl_cpu_vertex(const void *value, GLenum type, uint32_t size)
{
    uint8_t cache_index;
    if (gl_get_cache_index(next_prim_id(), &cache_index)) {

        gl_fill_attrib_defaults(ATTRIB_VERTEX, size);
        gl_read_attrib(ATTRIB_VERTEX, value, type, size);
        gl_vertex_pre_tr(cache_index);
    }

    submit_vertex(cache_index);
}

static void gl_cpu_mtx_index(const uint8_t *mtx_index)
{
    // do nothing
}

static void gl_cpu_array_element(uint32_t index)
{
    gl_fill_all_attrib_defaults(state->array_object->arrays);
    draw_vertex_from_arrays(state->array_object->arrays, index, index);
}

static void gl_cpu_draw_arrays(GLenum mode, uint32_t first, uint32_t count)
{
    gl_cpu_begin(mode);

    gl_fill_all_attrib_defaults(state->array_object->arrays);

    if (state->array_object->arrays[ATTRIB_VERTEX].enabled) {
        for (uint32_t i = 0; i < count; i++)
        {
            draw_vertex_from_arrays(state->array_object->arrays, next_prim_id(), first + i);
        }
    } else {
        // If the vertex array is disabled, nothing is drawn. However, all other attributes are still applied.
        // So in effect, we just need to load the last set of attributes.
        gl_load_attribs(state->array_object->arrays, first + count - 1);
    }

    gl_cpu_end();
}

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

static void gl_cpu_draw_elements(GLenum mode, uint32_t count, const void* indices, GLenum type)
{
    read_index_func read_index;

    switch (type) {
    case GL_UNSIGNED_BYTE:
        read_index = (read_index_func)read_index_8;
        break;
    case GL_UNSIGNED_SHORT:
        read_index = (read_index_func)read_index_16;
        break;
    case GL_UNSIGNED_INT:
        read_index = (read_index_func)read_index_32;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid index type", type);
        return;
    }

    if (state->array_object->element_array_buffer != NULL) {
        indices = state->array_object->element_array_buffer->storage.data + (uint32_t)indices;
    }

    gl_cpu_begin(mode);

    gl_fill_all_attrib_defaults(state->array_object->arrays);

    if (state->array_object->arrays[ATTRIB_VERTEX].enabled) {
        for (uint32_t i = 0; i < count; i++)
        {
            uint32_t index = read_index(indices, i);
            draw_vertex_from_arrays(state->array_object->arrays, index, index);
        }
    } else {
        // If the vertex array is disabled, nothing is drawn. However, all other attributes are still applied.
        // So in effect, we just need to load the last set of attributes.
        gl_load_attribs(state->array_object->arrays, read_index(indices, count - 1));
    }

    gl_cpu_end();
}

const gl_pipeline_t gl_cpu_pipeline = (gl_pipeline_t) {
    .begin = gl_cpu_begin,
    .end = gl_cpu_end,
    .vertex = gl_cpu_vertex,
    .mtx_index = gl_cpu_mtx_index,
    .array_element = gl_cpu_array_element,
    .draw_arrays = gl_cpu_draw_arrays,
    .draw_elements = gl_cpu_draw_elements,
};
