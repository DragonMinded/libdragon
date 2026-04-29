/**
 * @file rsp_pipeline.c
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 * @brief OpenGL RSP-based rendering pipeline implementation.
 */
#include <limits.h>

#include "gl_internal.h"
#include "rsp_asm.h"
#include "magma_constants.h"
#include "rdpq_debug.h"
#include "../magma/magma_internal.h"
#include "mgfx_macros.h"
#include "indices.h"
#include "draw_call_cache.h"
#include "data_cache.h"
#include "array.h"
#include "array_object.h"
#include "buffer.h"
#include "fnv1a.h"
#include "array_convert.h"
#include "pipelines.h"

_Static_assert(BEGIN_END_BUFFER_SIZE <= MG_VERTEX_CACHE_COUNT);

extern gl_state_t *state;

#define DEFINE_READ_FUNC(name, dst_type, src_type, convert, max_size, default) \
    static void name(dst_type *dst, const src_type *src, uint32_t count) \
    { \
        uint32_t real_count = MIN(count, max_size); \
        for (uint32_t i = 0; i < real_count; i++) dst[i] = convert(src[i]); \
        for (uint32_t i = count; i < max_size; i++) dst[i] = default[i]; \
    }

#define DEFINE_FIXED_READ_FUNC(name, dst_type, precision, max_size, default) \
    static void name(dst_type *dst, const int16u_t *src, uint32_t count) \
    { \
        int shift = precision.shift_amount; \
        uint32_t real_count = MIN(count, max_size); \
        if (shift < 0) { \
            for (uint32_t i = 0; i < real_count; i++) dst[i] = src[i] >> -shift; \
        } else { \
            for (uint32_t i = 0; i < real_count; i++) { \
                int16_t value = src[i]; \
                assertf(value <= SHRT_MAX>>shift && value >= SHRT_MIN>>shift, "Fixed point overflow: %d << %d", value, shift); \
                dst[i] = value << shift; \
            } \
        } \
        for (uint32_t i = count; i < max_size; i++) dst[i] = default[i]; \
    }

#define GLP_NRM(x, y, z) MGFX_NRM(x, y, z);

#define DEFINE_NORMAL_FLT_READ_FUNC(name, src_type) \
    static void name(int16_t *dst, const src_type *src, uint32_t count) \
    { \
        int16_t x = CLAMP(roundf(src[0] * 15.5f), -16.0f, 15.0f); \
        int16_t y = CLAMP(roundf(src[1] * 31.5f), -32.0f, 31.0f); \
        int16_t z = CLAMP(roundf(src[2] * 15.5f), -16.0f, 15.0f); \
        *dst = GLP_NRM(x, y, z); \
    }

#define DEFINE_NORMAL_INT_READ_FUNC(name, src_type, shift) \
    static void name(int16_t *dst, const src_type *src, uint32_t count) \
    { \
        int16_t x = src[0] >> shift; \
        int16_t y = src[0] >> (shift-1); \
        int16_t z = src[0] >> shift; \
        *dst = GLP_NRM(x, y, z); \
    }

static const int16_t vtx_default[3] = {0, 0, 0};
static const uint8_t col_default[4] = {0, 0, 0, 255};

#define POS_CONVERT(v)  MGFX_S10_5(v)

DEFINE_READ_FUNC(vtx_read_i8,   int16_t, int8_t,    POS_CONVERT, 3, vtx_default)
DEFINE_READ_FUNC(vtx_read_i16,  int16_t, int16u_t,  POS_CONVERT, 3, vtx_default)
DEFINE_READ_FUNC(vtx_read_i32,  int16_t, int32u_t,  POS_CONVERT, 3, vtx_default)
DEFINE_READ_FUNC(vtx_read_f32,  int16_t, floatu,    POS_CONVERT, 3, vtx_default)
DEFINE_READ_FUNC(vtx_read_f64,  int16_t, doubleu,   POS_CONVERT, 3, vtx_default)
DEFINE_FIXED_READ_FUNC(vtx_read_x16, int16_t, state->vertex_halfx_precision, 3, vtx_default)

DEFINE_NORMAL_INT_READ_FUNC(nrm_read_i8,  int8_t,    3)
DEFINE_NORMAL_INT_READ_FUNC(nrm_read_i16, int16u_t,  11)
DEFINE_NORMAL_INT_READ_FUNC(nrm_read_i32, int32u_t,  27)
DEFINE_NORMAL_FLT_READ_FUNC(nrm_read_f32, floatu)
DEFINE_NORMAL_FLT_READ_FUNC(nrm_read_f64, doubleu)

static void nrm_read_packed565(int16_t *dst, const int16_t *src, uint32_t count)
{
    *dst = *src;
}

#define COL_CONVERT_U8(v) ((v))
#define COL_CONVERT_I8(v) (MAX(v, 0) << 1)
#define COL_CONVERT_U16(v) ((v) >> 8)
#define COL_CONVERT_I16(v) (MAX(v, 0) >> 7)
#define COL_CONVERT_U32(v) ((v) >> 24)
#define COL_CONVERT_I32(v) (MAX(v, 0) >> 23)
#define COL_CONVERT_F32(v) (FLOAT_TO_U8(v))
#define COL_CONVERT_F64(v) (FLOAT_TO_U8(v))

DEFINE_READ_FUNC(col_read_u8,   uint8_t,    uint8_t,   COL_CONVERT_U8,  4, col_default)
DEFINE_READ_FUNC(col_read_i8,   uint8_t,    int8_t,    COL_CONVERT_I8,  4, col_default)
DEFINE_READ_FUNC(col_read_u16,  uint8_t,    uint16u_t, COL_CONVERT_U16, 4, col_default)
DEFINE_READ_FUNC(col_read_i16,  uint8_t,    int16u_t,  COL_CONVERT_I16, 4, col_default)
DEFINE_READ_FUNC(col_read_u32,  uint8_t,    uint32u_t, COL_CONVERT_U32, 4, col_default)
DEFINE_READ_FUNC(col_read_i32,  uint8_t,    int32u_t,  COL_CONVERT_I32, 4, col_default)
DEFINE_READ_FUNC(col_read_f32,  uint8_t,    floatu,    COL_CONVERT_F32, 4, col_default)
DEFINE_READ_FUNC(col_read_f64,  uint8_t,    doubleu,   COL_CONVERT_F64, 4, col_default)

#define TEX_CONVERT(v)  MGFX_S8_8(v)

DEFINE_READ_FUNC(tex_read_i8,   int16_t, int8_t,    TEX_CONVERT, 2, vtx_default)
DEFINE_READ_FUNC(tex_read_i16,  int16_t, int16u_t,  TEX_CONVERT, 2, vtx_default)
DEFINE_READ_FUNC(tex_read_i32,  int16_t, int32u_t,  TEX_CONVERT, 2, vtx_default)
DEFINE_READ_FUNC(tex_read_f32,  int16_t, floatu,    TEX_CONVERT, 2, vtx_default)
DEFINE_READ_FUNC(tex_read_f64,  int16_t, doubleu,   TEX_CONVERT, 2, vtx_default)
DEFINE_FIXED_READ_FUNC(tex_read_x16, int16_t, state->texcoord_halfx_precision, 2, vtx_default)

#define MTX_INDEX_CONVERT(v) (v)

DEFINE_READ_FUNC(mtx_index_read_u8, uint8_t,  uint8_t,   MTX_INDEX_CONVERT, 1, vtx_default)
DEFINE_READ_FUNC(mtx_index_read_u16, uint8_t, uint16u_t, MTX_INDEX_CONVERT, 1, vtx_default)
DEFINE_READ_FUNC(mtx_index_read_u32, uint8_t, uint32u_t, MTX_INDEX_CONVERT, 1, vtx_default)

const rsp_read_attrib_func rsp_read_funcs[ATTRIB_COUNT][ATTRIB_TYPE_COUNT] = {
    {
        (rsp_read_attrib_func)vtx_read_i8,
        NULL,
        (rsp_read_attrib_func)vtx_read_i16,
        NULL,
        (rsp_read_attrib_func)vtx_read_i32,
        NULL,
        (rsp_read_attrib_func)vtx_read_f32,
        (rsp_read_attrib_func)vtx_read_f64,
        (rsp_read_attrib_func)vtx_read_x16,
        NULL,
    },
    {
        (rsp_read_attrib_func)nrm_read_i8,
        NULL,
        (rsp_read_attrib_func)nrm_read_i16,
        NULL,
        (rsp_read_attrib_func)nrm_read_i32,
        NULL,
        (rsp_read_attrib_func)nrm_read_f32,
        (rsp_read_attrib_func)nrm_read_f64,
        NULL,
        (rsp_read_attrib_func)nrm_read_packed565,
    },
    {
        (rsp_read_attrib_func)col_read_i8,
        (rsp_read_attrib_func)col_read_u8,
        (rsp_read_attrib_func)col_read_i16,
        (rsp_read_attrib_func)col_read_u16,
        (rsp_read_attrib_func)col_read_i32,
        (rsp_read_attrib_func)col_read_u32,
        (rsp_read_attrib_func)col_read_f32,
        (rsp_read_attrib_func)col_read_f64,
        NULL,
        NULL,
    },
    {
        (rsp_read_attrib_func)tex_read_i8,
        NULL,
        (rsp_read_attrib_func)tex_read_i16,
        NULL,
        (rsp_read_attrib_func)tex_read_i32,
        NULL,
        (rsp_read_attrib_func)tex_read_f32,
        (rsp_read_attrib_func)tex_read_f64,
        (rsp_read_attrib_func)tex_read_x16,
        NULL,
    },
    {
        NULL,
        (rsp_read_attrib_func)mtx_index_read_u8,
        NULL,
        (rsp_read_attrib_func)mtx_index_read_u16,
        NULL,
        (rsp_read_attrib_func)mtx_index_read_u32,
        NULL,
        NULL,
        NULL,
        NULL,
    },
};

static void begin_end_next_buffer()
{
    if (rspq_block_is_recording()) {
        state->begin_end_current_buffer = malloc_uncached(sizeof(native_vertex_t) * BEGIN_END_BUFFER_SIZE);
        rspq_block_atexit(free_uncached, state->begin_end_current_buffer);
    } else {
        state->begin_end_current_buffer = ringbuffer_alloc_next(&state->begin_end_buffer);
    }
    state->begin_end_index = 0;
    state->begin_end_load_index = 0;
    mg_bind_vertex_buffer(state->begin_end_current_buffer);
}

static native_vertex_t *begin_end_get_current_vertex()
{
    return state->begin_end_current_buffer + state->begin_end_index;
}

static uint32_t get_begin_end_multiple(GLenum mode)
{
    switch (mode)
    {
    case GL_POINTS:
    case GL_LINE_LOOP:
    case GL_LINE_STRIP:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_POLYGON:
        return 1;
    case GL_LINES:
    case GL_QUAD_STRIP:
        return 2;
    case GL_TRIANGLES:
        return 3;
    case GL_QUADS:
        return 4;
    default:
        return 0;
    }
}

static bool get_begin_end_need_save(GLenum mode)
{
    switch (mode)
    {
    case GL_LINE_LOOP:
    case GL_TRIANGLE_FAN:
    case GL_POLYGON:
        return true;
    case GL_POINTS:
    case GL_LINES:
    case GL_LINE_STRIP:
    case GL_TRIANGLES:
    case GL_TRIANGLE_STRIP:
    case GL_QUADS:
    case GL_QUAD_STRIP:
    default:
        return false;
    }
}

static uint32_t get_client_flags()
{
    uint32_t client_flags = 0;
    if (state->begin_end_active) client_flags |= CLIENT_FLAG_BEGIN_END;
    if (state->array_object->arrays[ATTRIB_COLOR].enabled) client_flags |= CLIENT_FLAG_COLOR_ARRAY;
    return client_flags;
}

static void magma_init()
{
    uint32_t client_flags = get_client_flags();
    gl2_write(GL_CMD_PRE_INIT_MAGMA, magma_rsp_state, client_flags);
}

static void gl_rsp_begin(GLenum mode)
{
    state->primitive_mode = mode;
    state->begin_end_multiple = get_begin_end_multiple(mode);
    state->begin_end_need_save = get_begin_end_need_save(mode);

    update_pipelines_from_layout(&state->begin_end_layout);
    magma_init();
    mg_set_vertex_stride(sizeof(native_vertex_t));

    mg_draw_begin();

    if (!rspq_block_is_recording() && state->begin_end_buffer.buffer == NULL) {
        ringbuffer_init(&state->begin_end_buffer, sizeof(native_vertex_t) * BEGIN_END_BUFFER_SIZE, BEGIN_END_BUFFER_COUNT);
    }

    begin_end_next_buffer();
}

__attribute__((noinline))
static void begin_end_load()
{
    if (state->begin_end_index > state->begin_end_load_index) {
        mg_load_vertices(state->begin_end_load_index, state->begin_end_load_index, state->begin_end_index - state->begin_end_load_index);
        state->begin_end_load_index = state->begin_end_index;
    }
}

static uint32_t draw_batch(GLenum mode, uint32_t count, uint32_t cache_offset)
{
    switch (mode) {
        case GL_TRIANGLES:
        {
            size_t prim_count = count / 3;
            for (size_t i = 0; i < prim_count; i++) mg_draw_triangle(3*i, 3*i+1, 3*i+2);
            return cache_offset;
        }
        case GL_TRIANGLE_STRIP:
        {
            size_t prim_count = MAX(0, count - 2);
            for (size_t i = 0; i < prim_count; i++) mg_draw_triangle(i, i + 1 + i%2, i + 2 - i%2);
            return cache_offset;
        }
        case GL_TRIANGLE_FAN:
        {
            size_t prim_count = MAX(0, count - 2 + cache_offset);
            for (size_t i = 0; i < prim_count; i++) mg_draw_triangle(i+1, i+2, 0);
            return 1;
        }
        default:
        {
            return cache_offset;
        }
    }
}

static void begin_end_draw_current_buffer()
{
    begin_end_load();
    draw_batch(state->primitive_mode, state->begin_end_index, 0);

    if (!rspq_block_is_recording()) ringbuffer_release_current(&state->begin_end_buffer);
}

static void gl_rsp_end()
{
    // TODO: line loops will need special handling (insert saved vtx at the end)

    if (state->begin_end_index > 0) {
        begin_end_draw_current_buffer();
    }

    mg_draw_end();
}

static void begin_end_append_vtx(const native_vertex_t *vtx)
{
    memcpy(begin_end_get_current_vertex(), vtx, sizeof(native_vertex_t));
    state->begin_end_index++;
}

static void begin_end_prep_next_buffer(const native_vertex_t *prev_end)
{
    // Appending these vertices is guaranteed to not overflow the buffer since we just started a fresh one
    switch (state->primitive_mode) {
    case GL_TRIANGLE_STRIP:
    {
        // The two previous vertices
        begin_end_append_vtx(prev_end - 2);
        begin_end_append_vtx(prev_end - 1);
        break;
    }
    case GL_TRIANGLE_FAN:
    case GL_POLYGON:
    {
        // The "hub" of the fan
        begin_end_append_vtx(&state->begin_end_saved_vtx);
        // The previous vertex
        begin_end_append_vtx(prev_end - 1);
        break;
    }
    }
}

static void begin_end_advance()
{
    begin_end_append_vtx(&state->current_attribs);

    // In some cases, we need to save the very first vertex for later (for example triangle fan, line loop)
    if (state->begin_end_need_save) {
        memcpy(&state->begin_end_saved_vtx, &state->current_attribs, sizeof(native_vertex_t));
        state->begin_end_need_save = false;
    }

    // Check if we have reached the required multiple of vertices and the next multiple would overflow the current buffer
    if (state->begin_end_index % state->begin_end_multiple == 0 && 
        state->begin_end_index + state->begin_end_multiple > BEGIN_END_BUFFER_SIZE) {
        begin_end_draw_current_buffer();
        native_vertex_t *prev_end = begin_end_get_current_vertex();
        begin_end_next_buffer();
        begin_end_prep_next_buffer(prev_end);
    }
}

static void *get_attrib_dst(gl_array_type_t array_type)
{
    switch (array_type)
    {
    case ATTRIB_VERTEX:
        return state->current_attribs.position;
    case ATTRIB_NORMAL:
        return &state->current_attribs.normal;
    case ATTRIB_COLOR:
        return &state->current_attribs.color;
    case ATTRIB_TEXCOORD:
        return state->current_attribs.texcoord;
    case ATTRIB_MTX_INDEX:
        return state->current_attribs.mtx_index;
    default:
        return NULL;
    }
}

void rsp_read_attrib(gl_array_type_t array_type, GLenum type, const void *value, uint32_t size)
{
    rsp_read_attrib_func read_func = rsp_read_funcs[array_type][gl_type_to_index(type)];
    assertf(read_func != NULL, "Could not find read func");
    void *dst = get_attrib_dst(array_type);
    assertf(dst != NULL, "Array type not supported");

    read_func(dst, value, size);
}

static void gl_rsp_vertex(const void *value, GLenum type, uint32_t size)
{
    rsp_read_attrib(ATTRIB_VERTEX, type, value, size);
    begin_end_advance();
}

static void load_matrix(uint8_t mtx_index)
{
    const mg_uniform_t *uniform = get_matrices_uniform();
    mg_uniform_load(uniform, state->matrix_palette + mtx_index);
}

static void gl_rsp_mtx_index(const uint8_t *mtx_index)
{
    if (state->begin_end_active) {
        begin_end_load();
        load_matrix(*mtx_index);
    }
}

static void get_array_element_convert_parms(array_convert_parms_t *parms, uint32_t index)
{
    static const data_layout_t layout = {
        .offsets = {
            offsetof(native_vertex_t, position),
            offsetof(native_vertex_t, normal),
            offsetof(native_vertex_t, color),
            offsetof(native_vertex_t, texcoord),
            offsetof(native_vertex_t, mtx_index),
        },
        .stride = sizeof(native_vertex_t)
    };

    for (gl_array_type_t i = 0; i < ATTRIB_COUNT; i++)
    {
        parms->arrays[i] = &state->array_object->arrays[i];
    }

    parms->array_count = ATTRIB_COUNT;
    parms->out_layout = &layout;
    parms->out_buffer = &state->current_attribs;
    parms->range.first = index;
    parms->range.count = 1;
}

static bool is_array_enabled(gl_array_type_t type)
{
    return state->array_object->arrays[type].enabled;
}

static bool is_vertex_array_enabled()
{
    return is_array_enabled(ATTRIB_VERTEX);
}

static void gl_rsp_array_element(uint32_t index)
{
    array_convert_parms_t convert_parms;
    get_array_element_convert_parms(&convert_parms, index);
    array_convert(&convert_parms);

    if (is_array_enabled(ATTRIB_MTX_INDEX)) {
        gl_rsp_mtx_index(state->current_attribs.mtx_index);
    }

    if (is_vertex_array_enabled()) {
        begin_end_advance();
    }
    // TODO: if vertex array is not enabled, send attributes to RSP?
}

static data_view_t get_vertex_data_for_block(gl_array_object_t *array_object, index_bounds_t bounds)
{
    uint32_t stride = data_source_get_stride(&array_object->vertex_data_source);

    void *buffer = malloc_uncached(stride * bounds.count);
    rspq_block_atexit(free_uncached, buffer);
    data_source_pull(&array_object->vertex_data_source, buffer, bounds);

    return (data_view_t) {
        .pointer = buffer,
        .stride = data_source_get_stride(&array_object->vertex_data_source)
    };
}

static void update_pipelines(gl_array_object_t *array_object)
{
    vertex_layout_cache_update(&array_object->layout_cache, array_object->arrays);
    vertex_layout *vertex_layout = vertex_layout_cache_get_layout(&array_object->layout_cache);
    update_pipelines_from_layout(vertex_layout);
}

static data_view_t get_vertex_data_view(gl_array_object_t *array_object, index_bounds_t bounds)
{
    if (rspq_block_is_recording()) {
        return get_vertex_data_for_block(array_object, bounds);
    }

    return data_cache_prepare_at_bounds(&array_object->vertex_data_cache, bounds);
}

static void prepare_vertex_data(gl_array_object_t *array_object, index_bounds_t bounds)
{
    data_view_t vertex_data_view = get_vertex_data_view(state->array_object, bounds);
    mg_set_vertex_stride(vertex_data_view.stride);
    mg_bind_vertex_buffer(vertex_data_view.pointer);
}

static void prepare_drawing(gl_array_object_t *array_object, index_bounds_t bounds)
{
    update_pipelines(array_object);
    magma_init();
    prepare_vertex_data(array_object, bounds);
}

static void gl_rsp_draw_arrays(GLenum mode, uint32_t first, uint32_t count)
{
    if (!is_vertex_array_enabled()) {
        return;
    }
    
    index_bounds_t range = {
        .first = first,
        .count = count
    };
    
    prepare_drawing(state->array_object, range);

    mg_draw_begin();
    mg_input_assembly_parms_t input_assembly_parms = array_object_get_input_assembly_parms(state->array_object, mode, range);
    mg_draw(&input_assembly_parms, count, first);
    mg_draw_end();
}

static const uint16_t *get_indices_from_buffer(gl_buffer_object_t *buffer_object, uint32_t offset)
{
    return (const uint16_t*)((const uint8_t*)buffer_object->storage.data + offset);
}

static const uint16_t *get_indices(gl_buffer_object_t *element_buffer, const void* indices)
{
    if (element_buffer == NULL) {
        return (const uint16_t*)indices;
    } else {
        return get_indices_from_buffer(element_buffer, (uint32_t)indices);
    }
}

static cached_draw_call_t *get_or_create_draw_call(gl_array_object_t *array_object, const draw_call_parms_t *parms)
{
    draw_call_cache_t *draw_call_cache = array_object_get_draw_call_cache(state->array_object);
    return draw_call_cache_get_or_create(draw_call_cache, parms);
}

static cached_draw_call_t *prepare_draw_call(gl_array_object_t *array_object, const draw_call_parms_t *parms, const void *index_data)
{
    cached_draw_call_t *draw_call = get_or_create_draw_call(array_object, parms);
    
    if (draw_call_needs_update(draw_call)) {
        draw_call_update(draw_call, index_data, array_object);
    }

    return draw_call;
}

static void draw_elements_from_buffer(GLenum mode, uint32_t count, const void* indices, gl_buffer_object_t *element_buffer)
{
    draw_call_parms_t parms = {
        .mode = mode,
        .offset = (uint32_t)indices,
        .count = count
    };

    const void *index_data = get_indices(element_buffer, indices);
    cached_draw_call_t *draw_call = prepare_draw_call(state->array_object, &parms, index_data);
    
    prepare_drawing(state->array_object, draw_call->index_range);

    mg_draw_begin();
    draw_call_run(draw_call);
    mg_draw_end();
}

static void draw_elements_from_pointer(GLenum mode, uint32_t count, const void* indices)
{
    index_bounds_t range = find_index_bounds(indices, count);

    prepare_drawing(state->array_object, range);

    mg_draw_begin();
    mg_input_assembly_parms_t input_assembly_parms = array_object_get_input_assembly_parms(state->array_object, mode, range);
    mg_draw_indexed(&input_assembly_parms, indices, count, -range.first);
    mg_draw_end();
}

static void gl_rsp_draw_elements(GLenum mode, uint32_t count, const void* indices, GLenum type)
{
    assertf(type == GL_UNSIGNED_SHORT, "Index type must be GL_UNSIGNED_SHORT");
    
    if (!is_vertex_array_enabled()) {
        return;
    }

    gl_buffer_object_t *element_buffer = state->array_object->element_array_buffer;
    
    if (element_buffer != NULL && !rspq_block_is_recording()) {
        draw_elements_from_buffer(mode, count, indices, element_buffer);
    } else {
        const void *index_data = get_indices(element_buffer, indices);
        draw_elements_from_pointer(mode, count, index_data);
    }
}

const gl_pipeline_t gl_rsp_pipeline = (gl_pipeline_t) {
    .begin = gl_rsp_begin,
    .end = gl_rsp_end,
    .vertex = gl_rsp_vertex,
    .mtx_index = gl_rsp_mtx_index,
    .array_element = gl_rsp_array_element,
    .draw_arrays = gl_rsp_draw_arrays,
    .draw_elements = gl_rsp_draw_elements,
};
