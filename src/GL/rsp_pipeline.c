/**
 * @file rsp_pipeline.c
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 * @brief OpenGL RSP-based rendering pipeline implementation.
 */
#include <limits.h>

#include "gl_internal.h"
#include "rsp_asm.h"
#include "magma_constants.h"
#include "mgfx.h"
#include "rdpq_debug.h"
#include "../magma/magma_internal.h"

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

#define DEFINE_NORMAL_FLT_READ_FUNC(name, src_type) \
    static void name(int16_t *dst, const src_type *src, uint32_t count) \
    { \
        int16_t x = CLAMP(roundf(src[0] * 15.5f), -16.0f, 15.0f); \
        int16_t y = CLAMP(roundf(src[1] * 31.5f), -32.0f, 31.0f); \
        int16_t z = CLAMP(roundf(src[2] * 15.5f), -16.0f, 15.0f); \
        *dst = MGFX_NRM(x, y, z); \
    }

#define DEFINE_NORMAL_INT_READ_FUNC(name, src_type, shift) \
    static void name(int16_t *dst, const src_type *src, uint32_t count) \
    { \
        int16_t x = src[0] >> shift; \
        int16_t y = src[0] >> (shift-1); \
        int16_t z = src[0] >> shift; \
        *dst = MGFX_NRM(x, y, z); \
    }

static const int16_t vtx_default[3] = {0, 0, 0};
static const uint8_t col_default[4] = {0, 0, 0, 255};

DEFINE_READ_FUNC(vtx_read_i8, int16_t, int8_t, MGFX_S10_5, 3, vtx_default)
DEFINE_READ_FUNC(vtx_read_i16, int16_t, int16u_t, MGFX_S10_5, 3, vtx_default)
DEFINE_READ_FUNC(vtx_read_i32, int16_t, int32u_t, MGFX_S10_5, 3, vtx_default)
DEFINE_READ_FUNC(vtx_read_f32, int16_t, floatu, MGFX_S10_5, 3, vtx_default)
DEFINE_READ_FUNC(vtx_read_f64, int16_t, doubleu, MGFX_S10_5, 3, vtx_default)
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

DEFINE_READ_FUNC(col_read_u8, uint8_t,  uint8_t,   COL_CONVERT_U8, 4, col_default)
DEFINE_READ_FUNC(col_read_i8, uint8_t,  int8_t,    COL_CONVERT_I8, 4, col_default)
DEFINE_READ_FUNC(col_read_u16, uint8_t, uint16u_t, COL_CONVERT_U16, 4, col_default)
DEFINE_READ_FUNC(col_read_i16, uint8_t, int16u_t,  COL_CONVERT_I16, 4, col_default)
DEFINE_READ_FUNC(col_read_u32, uint8_t, uint32u_t, COL_CONVERT_U32, 4, col_default)
DEFINE_READ_FUNC(col_read_i32, uint8_t, int32u_t,  COL_CONVERT_I32, 4, col_default)
DEFINE_READ_FUNC(col_read_f32, uint8_t, floatu,    COL_CONVERT_F32, 4, col_default)
DEFINE_READ_FUNC(col_read_f64, uint8_t, doubleu,   COL_CONVERT_F64, 4, col_default)

DEFINE_READ_FUNC(tex_read_i8, int16_t, int8_t, MGFX_S8_8, 2, vtx_default)
DEFINE_READ_FUNC(tex_read_i16, int16_t, int16u_t, MGFX_S8_8, 2, vtx_default)
DEFINE_READ_FUNC(tex_read_i32, int16_t, int32u_t, MGFX_S8_8, 2, vtx_default)
DEFINE_READ_FUNC(tex_read_f32, int16_t, floatu, MGFX_S8_8, 2, vtx_default)
DEFINE_READ_FUNC(tex_read_f64, int16_t, doubleu, MGFX_S8_8, 2, vtx_default)
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

static mg_primitive_topology_t get_primitive_topology(GLenum mode)
{
    switch (mode)
    {
    case GL_TRIANGLES:
        return MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case GL_TRIANGLE_STRIP:
        return MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case GL_TRIANGLE_FAN:
    case GL_POLYGON:
        return MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;

    case GL_POINTS:
    case GL_LINES:
    case GL_LINE_LOOP:
    case GL_LINE_STRIP:
    case GL_QUADS:
    case GL_QUAD_STRIP:
        assertf(0, "Draw mode %ld is not supported", mode);
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid primitive mode", mode);
        return -1;
    }
}

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

static const vertex_layout *get_current_layout()
{
    if (state->begin_end_active) {
        return &state->begin_end_layout;
    } else {
        return &state->array_object->layout;
    }
}

inline void fnv1a(uint32_t *hash, uint32_t v)
{
    *hash ^= v;
    *hash *= 0x01000193; // FNV prime
}

static uint32_t get_pipeline_key(const mg_vertex_layout_t *layout)
{
    // Get pipeline key by creating a hash from all pipeline parameters using FNV-1a hash
    uint32_t key = 0x811c9dc5; // FNV offset basis
    for (size_t i = 0; i < layout->attribute_count; i++)
    {
        fnv1a(&key, layout->attributes[i].input);
        fnv1a(&key, layout->attributes[i].offset);
    }
    fnv1a(&key, layout->stride);
    return key;
}

static mg_pipeline_t **create_pipelines(const mg_vertex_layout_t *layout)
{
    mg_pipeline_t **pipelines = calloc(PIPELINE_COUNT, sizeof(mg_pipeline_t*));

    for (size_t i = 0; i < PIPELINE_COUNT; i++)
    {
        // This will iterate over all possible combinations of features
        mgfx_features_t features = i;

        pipelines[i] = mg_pipeline_create(&(mg_pipeline_parms_t) {
            .vertex_shader_ucode = mgfx_get_shader_ucode(features),
            .vertex_layout = *layout
        });
    }

    return pipelines;
}

static void update_pipeline(const vertex_layout *layout)
{
    /*
    vertex_layout vl;
    if (state->lighting && !gl_is_diffuse_tracking_color())
    {
        // Special case: The vertex array has color as input, but the current material configuration ignores it (instead using the material color).
        // To avoid having to re-configure the vertex array (which would involve re-converting data), instead we "hide" the color attribute
        // from the vertex shader by copying the vertex layout and omitting the color attribute.
        // All other attributes will keep their original offsets, so we can use the existing data as-is.
        vertex_layout_init(&vl);
        vertex_layout_copy_without(&vl, layout, MGFX_ATTRIBUTE_COLOR);
        layout = &vl;
    }
    */

    uint32_t key = get_pipeline_key(&layout->vertex_layout);

    mg_pipeline_t **pipelines = hashtable_lookup(&state->pipeline_cache, key);
    if (pipelines == NULL) {
        pipelines = create_pipelines(&layout->vertex_layout);
        hashtable_insert(&state->pipeline_cache, key, pipelines);
    }

    for (size_t i = 0; i < PIPELINE_COUNT; i++)
    {
        uint32_t packed = ((sizeof(gl_pipeline_data_t)*i) << 16) | (ROUND_UP(pipelines[i]->shader_code_size, 8) - 1);
        gl2_write(GL_CMD_SET_PIPELINE, PhysicalAddr(pipelines[i]->shader_code), packed);
    }

    if (state->matrices_uniform == NULL) {
        state->matrices_uniform = mg_pipeline_get_uniform(pipelines[0], MGFX_BINDING_MATRICES);
    }
}

static void prepare_drawing_with_magma()
{
    const vertex_layout *layout = get_current_layout();
    update_pipeline(layout);
    
    gl2_write(GL_CMD_PRE_INIT_MAGMA, magma_rsp_state);
    
    mg_set_vertex_stride(layout->vertex_layout.stride);
}

static void gl_rsp_begin(GLenum mode)
{
    state->primitive_mode = mode;
    state->begin_end_multiple = get_begin_end_multiple(mode);
    state->begin_end_need_save = get_begin_end_need_save(mode);

    prepare_drawing_with_magma();

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
    mg_uniform_load(state->matrices_uniform, state->matrix_palette + mtx_index);
}

static void gl_rsp_mtx_index(const uint8_t *mtx_index)
{
    if (state->begin_end_active) {
        begin_end_load();
        load_matrix(*mtx_index);
    }
}

static void gl_rsp_array_element(uint32_t index)
{
    static const uint32_t out_offsets[ATTRIB_COUNT] = {
        offsetof(native_vertex_t, position),
        offsetof(native_vertex_t, normal),
        offsetof(native_vertex_t, color),
        offsetof(native_vertex_t, texcoord),
        offsetof(native_vertex_t, mtx_index),
    };
    array_convert(state->array_object, out_offsets, &state->current_attribs, index, 1, sizeof(native_vertex_t));

    if (state->array_object->arrays[ATTRIB_VERTEX].enabled) {
        begin_end_advance();
    }
}

static void prepare_drawing_from_arrays()
{
    array_object_update(state->array_object);
    prepare_drawing_with_magma();
}

static void prepare_array_vertex_data(uint32_t first, uint32_t count)
{
    if (rspq_block_is_recording()) {
        void *buffer = malloc_uncached(state->array_object->layout.vertex_layout.stride * count);
        rspq_block_atexit(free_uncached, buffer);
        array_object_convert_into(state->array_object, first, count, buffer);
        mg_bind_vertex_buffer(buffer);
    } else {
        array_object_fill_cache(state->array_object, first, count);
        // It's possible that we are now accessing a sub-range of a previously cached buffer.
        // In that case we need to apply an offset, since the draw command expects the first vertex at offset 0.
        uint32_t buffer_offset = first - state->array_object->cached_first;
        mg_bind_vertex_buffer(((uint8_t*)state->array_object->buffer) + buffer_offset * state->array_object->layout.vertex_layout.stride);
    }
}

static void get_input_assembly_parms(GLenum mode, mg_input_assembly_parms_t *parms)
{
    parms->primitive_topology = get_primitive_topology(mode);
    parms->primitive_restart_enabled = false;

    gl_array_t *mtx_index_array = &state->array_object->arrays[ATTRIB_MTX_INDEX];
    if (mtx_index_array->enabled) {
        // TODO: might not be the correct format
        parms->mtx_indices = mtx_index_array->final_pointer;
        parms->mtx_indices_stride = mtx_index_array->final_stride;
        parms->matrices = state->matrix_palette;
        parms->matrices_stride = sizeof(state->matrix_palette[0]);
        parms->matrix_uniform = *state->matrices_uniform;
    }
}

static void gl_rsp_draw_arrays(GLenum mode, uint32_t first, uint32_t count)
{
    prepare_drawing_from_arrays();
    prepare_array_vertex_data(first, count);
    mg_draw_begin();

    mg_input_assembly_parms_t input_assembly_parms;
    get_input_assembly_parms(mode, &input_assembly_parms);

    // TODO: record into block?
    mg_draw(&input_assembly_parms, count, first);
    mg_draw_end();
}

static bool input_assembly_parms_equal(const mg_input_assembly_parms_t *lh, const mg_input_assembly_parms_t *rh)
{
    return lh->primitive_topology == rh->primitive_topology && lh->primitive_restart_enabled == rh->primitive_restart_enabled;
}

static void find_index_bounds(const uint16_t *indices, uint32_t count, uint16_t *min_index, uint16_t *max_index)
{
    uint16_t min = USHRT_MAX;
    uint16_t max = 0;

    for (size_t i = 0; i < count; i++)
    {
        if (indices[i] < min) min = indices[i];
        if (indices[i] > max) max = indices[i];
    }

    *min_index = min;
    *max_index = max;
}

static void update_element_array_cache(gl_buffer_object_t *element_buffer, uint32_t count, uint32_t offset, const mg_input_assembly_parms_t *input_assembly_parms, uint16_t *min_index, uint16_t *max_index)
{
    // TODO: throw INVALID OPERATION if buffer is currently mapped

    if (element_buffer->element_cache == NULL) {
        element_buffer->element_cache = calloc(1, sizeof(gl_element_array_cache_t));
    }

    const uint16_t *indices_i16 = (const uint16_t*)((const uint8_t*)element_buffer->storage.data + offset);

    gl_element_array_cache_t *cache = element_buffer->element_cache;
    bool is_dirty = false;
    if (cache->is_data_dirty || cache->count != count || cache->offset != offset) {
        find_index_bounds(indices_i16, count, &cache->min_index, &cache->max_index);
        cache->count = count;
        cache->offset = offset;
        is_dirty = true;
    }
    if (is_dirty || !input_assembly_parms_equal(&cache->parms, input_assembly_parms)) {
        cache->parms = *input_assembly_parms;

        if (cache->block != NULL) rspq_call_deferred((void(*)(void*))rspq_block_free, cache->block);

        rspq_block_begin();
        // TODO: keep track of matrix indices
        mg_draw_indexed(input_assembly_parms, indices_i16, count, -cache->min_index);
        cache->block = rspq_block_end();
        cache->is_data_dirty = false;
    }

    *min_index = cache->min_index;
    *max_index = cache->max_index;
}

static const uint16_t *get_indices(gl_buffer_object_t *element_buffer, const void* indices)
{
    if (element_buffer == NULL) {
        return (const uint16_t*)indices;
    } else {
        return (const uint16_t*)((const uint8_t*)element_buffer->storage.data + (uint32_t)indices);
    }
}

static void gl_rsp_draw_elements(GLenum mode, uint32_t count, const void* indices, GLenum type)
{
    assertf(type == GL_UNSIGNED_SHORT, "Index type must be GL_UNSIGNED_SHORT");

    prepare_drawing_from_arrays();

    uint16_t min_index, max_index;
    mg_input_assembly_parms_t input_assembly_parms;
    get_input_assembly_parms(mode, &input_assembly_parms);

    gl_buffer_object_t *element_buffer = state->array_object->element_array_buffer;
    if (element_buffer != NULL && !rspq_block_is_recording()) {
        update_element_array_cache(element_buffer, count, (uint32_t)indices, &input_assembly_parms, &min_index, &max_index);
    } else {
        find_index_bounds(get_indices(element_buffer, indices), count, &min_index, &max_index);
    }

    prepare_array_vertex_data(min_index, max_index - min_index + 1);
    mg_draw_begin();
    if (element_buffer != NULL && !rspq_block_is_recording()) {
        rspq_block_run(element_buffer->element_cache->block);
    } else {
        mg_draw_indexed(&input_assembly_parms, get_indices(element_buffer, indices), count, -min_index);
    }
    mg_draw_end();
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
