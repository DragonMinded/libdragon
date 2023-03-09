#include "gl_internal.h"

extern gl_state_t state;

#define VTX_SHIFT 5
#define TEX_SHIFT 5

#define DEFINE_SIMPLE_READ_FUNC(name, src_type, convert) \
    void name(gl_cmd_stream_t *s, const src_type *src, uint32_t count) \
    { \
        for (uint32_t i = 0; i < count; i++) gl_cmd_stream_put_half(s, convert(src[i])); \
    }

#define DEFINE_NORMAL_READ_FUNC(name, src_type, convert) \
    void name(gl_cmd_stream_t *s, const src_type *src, uint32_t count) \
    { \
        gl_cmd_stream_put_half(s, ((uint8_t)(convert(src[0])) << 8) | (uint8_t)(convert(src[1]))); \
        gl_cmd_stream_put_half(s, (uint8_t)(convert(src[2])) << 8); \
    }

#define VTX_CONVERT_INT(v) ((v) << VTX_SHIFT)
#define VTX_CONVERT_FLT(v) ((v) * (1<<VTX_SHIFT))

DEFINE_SIMPLE_READ_FUNC(vtx_read_u8,  uint8_t,  VTX_CONVERT_INT)
DEFINE_SIMPLE_READ_FUNC(vtx_read_i8,  int8_t,   VTX_CONVERT_INT)
DEFINE_SIMPLE_READ_FUNC(vtx_read_u16, uint16_t, VTX_CONVERT_INT)
DEFINE_SIMPLE_READ_FUNC(vtx_read_i16, int16_t,  VTX_CONVERT_INT)
DEFINE_SIMPLE_READ_FUNC(vtx_read_u32, uint32_t, VTX_CONVERT_INT)
DEFINE_SIMPLE_READ_FUNC(vtx_read_i32, int32_t,  VTX_CONVERT_INT)
DEFINE_SIMPLE_READ_FUNC(vtx_read_f32, float,    VTX_CONVERT_FLT)
DEFINE_SIMPLE_READ_FUNC(vtx_read_f64, double,   VTX_CONVERT_FLT)

#define COL_CONVERT_U8(v) ((v) << 7)
#define COL_CONVERT_I8(v) ((v) << 8)
#define COL_CONVERT_U16(v) ((v) >> 1)
#define COL_CONVERT_I16(v) ((v))
#define COL_CONVERT_U32(v) ((v) >> 17)
#define COL_CONVERT_I32(v) ((v) >> 16)
#define COL_CONVERT_F32(v) (FLOAT_TO_I16(v))
#define COL_CONVERT_F64(v) (FLOAT_TO_I16(v))

DEFINE_SIMPLE_READ_FUNC(col_read_u8,  uint8_t,  COL_CONVERT_U8)
DEFINE_SIMPLE_READ_FUNC(col_read_i8,  int8_t,   COL_CONVERT_I8)
DEFINE_SIMPLE_READ_FUNC(col_read_u16, uint16_t, COL_CONVERT_U16)
DEFINE_SIMPLE_READ_FUNC(col_read_i16, int16_t,  COL_CONVERT_I16)
DEFINE_SIMPLE_READ_FUNC(col_read_u32, uint32_t, COL_CONVERT_U32)
DEFINE_SIMPLE_READ_FUNC(col_read_i32, int32_t,  COL_CONVERT_I32)
DEFINE_SIMPLE_READ_FUNC(col_read_f32, float,    COL_CONVERT_F32)
DEFINE_SIMPLE_READ_FUNC(col_read_f64, double,   COL_CONVERT_F64)

#define TEX_CONVERT_INT(v) ((v) << TEX_SHIFT)
#define TEX_CONVERT_FLT(v) ((v) * (1<<TEX_SHIFT))

DEFINE_SIMPLE_READ_FUNC(tex_read_u8,  uint8_t,  TEX_CONVERT_INT)
DEFINE_SIMPLE_READ_FUNC(tex_read_i8,  int8_t,   TEX_CONVERT_INT)
DEFINE_SIMPLE_READ_FUNC(tex_read_u16, uint16_t, TEX_CONVERT_INT)
DEFINE_SIMPLE_READ_FUNC(tex_read_i16, int16_t,  TEX_CONVERT_INT)
DEFINE_SIMPLE_READ_FUNC(tex_read_u32, uint32_t, TEX_CONVERT_INT)
DEFINE_SIMPLE_READ_FUNC(tex_read_i32, int32_t,  TEX_CONVERT_INT)
DEFINE_SIMPLE_READ_FUNC(tex_read_f32, float,    TEX_CONVERT_FLT)
DEFINE_SIMPLE_READ_FUNC(tex_read_f64, double,   TEX_CONVERT_FLT)

#define NRM_CONVERT_U8(v) ((v) >> 1)
#define NRM_CONVERT_I8(v) ((v))
#define NRM_CONVERT_U16(v) ((v) >> 9)
#define NRM_CONVERT_I16(v) ((v) >> 8)
#define NRM_CONVERT_U32(v) ((v) >> 25)
#define NRM_CONVERT_I32(v) ((v) >> 24)
#define NRM_CONVERT_F32(v) ((v) * 0x7F)
#define NRM_CONVERT_F64(v) ((v) * 0x7F)

DEFINE_NORMAL_READ_FUNC(nrm_read_u8,  uint8_t,  NRM_CONVERT_U8)
DEFINE_NORMAL_READ_FUNC(nrm_read_i8,  int8_t,   NRM_CONVERT_I8)
DEFINE_NORMAL_READ_FUNC(nrm_read_u16, uint16_t, NRM_CONVERT_U16)
DEFINE_NORMAL_READ_FUNC(nrm_read_i16, int16_t,  NRM_CONVERT_I16)
DEFINE_NORMAL_READ_FUNC(nrm_read_u32, uint32_t, NRM_CONVERT_U32)
DEFINE_NORMAL_READ_FUNC(nrm_read_i32, int32_t,  NRM_CONVERT_I32)
DEFINE_NORMAL_READ_FUNC(nrm_read_f32, float,    NRM_CONVERT_F32)
DEFINE_NORMAL_READ_FUNC(nrm_read_f64, double,   NRM_CONVERT_F64)

const rsp_read_attrib_func rsp_read_funcs[ATTRIB_COUNT][8] = {
    {
        (rsp_read_attrib_func)vtx_read_i8,
        (rsp_read_attrib_func)vtx_read_u8,
        (rsp_read_attrib_func)vtx_read_i16,
        (rsp_read_attrib_func)vtx_read_u16,
        (rsp_read_attrib_func)vtx_read_i32,
        (rsp_read_attrib_func)vtx_read_u32,
        (rsp_read_attrib_func)vtx_read_f32,
        (rsp_read_attrib_func)vtx_read_f64,
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
    },
    {
        (rsp_read_attrib_func)tex_read_i8,
        (rsp_read_attrib_func)tex_read_u8,
        (rsp_read_attrib_func)tex_read_i16,
        (rsp_read_attrib_func)tex_read_u16,
        (rsp_read_attrib_func)tex_read_i32,
        (rsp_read_attrib_func)tex_read_u32,
        (rsp_read_attrib_func)tex_read_f32,
        (rsp_read_attrib_func)tex_read_f64,
    },
    {
        (rsp_read_attrib_func)nrm_read_i8,
        (rsp_read_attrib_func)nrm_read_u8,
        (rsp_read_attrib_func)nrm_read_i16,
        (rsp_read_attrib_func)nrm_read_u16,
        (rsp_read_attrib_func)nrm_read_i32,
        (rsp_read_attrib_func)nrm_read_u32,
        (rsp_read_attrib_func)nrm_read_f32,
        (rsp_read_attrib_func)nrm_read_f64,
    },
};

static void upload_current_attributes(const gl_array_t *arrays)
{
    if (arrays[ATTRIB_COLOR].enabled) {
        gl_set_current_color(state.current_attribs[ATTRIB_COLOR]);
    }

    if (arrays[ATTRIB_TEXCOORD].enabled) {
        gl_set_current_texcoords(state.current_attribs[ATTRIB_TEXCOORD]);
    }

    if (arrays[ATTRIB_NORMAL].enabled) {
        gl_set_current_normal(state.current_attribs[ATTRIB_NORMAL]);
    }
}

static void load_last_attributes(const gl_array_t *arrays, uint32_t last_index)
{
    gl_fill_all_attrib_defaults(arrays);
    gl_load_attribs(arrays, last_index);
    upload_current_attributes(arrays);
}

static void require_array_element(const gl_array_t *arrays)
{
    if (state.last_array_element >= 0) {
        load_last_attributes(arrays, state.last_array_element);
        state.last_array_element = -1;
    }
}

static inline gl_cmd_stream_t write_vertex_begin(uint32_t cache_index)
{
    gl_cmd_stream_t s = gl_cmd_stream_begin(glp_overlay_id, GLP_CMD_SET_PRIM_VTX, 8 /* TODO: replace with actual size */);
    gl_cmd_stream_put_half(&s, cache_index * PRIM_VTX_SIZE);
    return s;
}

static inline void write_vertex_end(gl_cmd_stream_t *s)
{
    gl_cmd_stream_end(s);
}

static void write_vertex_from_arrays(const gl_array_t *arrays, uint32_t index, uint8_t cache_index)
{
    static const GLfloat default_attribute_value[] = {0.0f, 0.0f, 0.0f, 1.0f};

    gl_load_attribs(arrays, index);

    gl_cmd_stream_t s = write_vertex_begin(cache_index);

    for (uint32_t i = 0; i < ATTRIB_COUNT; i++)
    {
        const gl_array_t *array = &arrays[i];
        if (!array->enabled) {
            rsp_read_funcs[i][6](&s, state.current_attribs[i], 4);
            continue;
        }

        const void *src = gl_get_attrib_element(array, index);
        array->rsp_read_func(&s, src, array->size);

        if (i != ATTRIB_NORMAL) {
            rsp_read_funcs[i][6](&s, default_attribute_value + array->size, 4-array->size);
        }
    }

    write_vertex_end(&s);
}

static inline void submit_vertex(uint32_t cache_index)
{
    uint8_t indices[3];
    if (gl_prim_assembly(cache_index, indices))
    {
        glpipe_draw_triangle(indices[0], indices[1], indices[2]);
    }
}

static void draw_vertex_from_arrays(const gl_array_t *arrays, uint32_t id, uint32_t index)
{
    uint8_t cache_index;
    if (gl_get_cache_index(id, &cache_index))
    {
        write_vertex_from_arrays(arrays, index, cache_index);
    }

    submit_vertex(cache_index);
}

static void gl_rsp_begin()
{
    glpipe_init();
    state.last_array_element = -1;
}

static void gl_rsp_end()
{
    require_array_element(state.array_object->arrays);
}

static void gl_rsp_vertex(const void *value, GLenum type, uint32_t size)
{
    uint8_t cache_index;
    if (gl_get_cache_index(next_prim_id(), &cache_index))
    {
        require_array_element(state.array_object->arrays);

        rsp_read_attrib_func read_func = rsp_read_funcs[ATTRIB_VERTEX][gl_type_to_index(type)];

        gl_cmd_stream_t s = write_vertex_begin(cache_index);
        read_func(&s, value, size);
        write_vertex_end(&s);
    }

    submit_vertex(cache_index);
}

static void gl_rsp_array_element(uint32_t index)
{
    draw_vertex_from_arrays(state.array_object->arrays, index, index);
    state.last_array_element = index;
}

static void gl_rsp_draw_arrays(uint32_t first, uint32_t count)
{
    if (state.array_object->arrays[ATTRIB_VERTEX].enabled) {
        for (uint32_t i = 0; i < count; i++)
        {
            draw_vertex_from_arrays(state.array_object->arrays, next_prim_id(), first + i);
        }
    }

    load_last_attributes(state.array_object->arrays, first + count - 1);
}

static void gl_rsp_draw_elements(uint32_t count, const void* indices, read_index_func read_index)
{
    gl_fill_all_attrib_defaults(state.array_object->arrays);

    if (state.array_object->arrays[ATTRIB_VERTEX].enabled) {
        for (uint32_t i = 0; i < count; i++)
        {
            uint32_t index = read_index(indices, i);
            draw_vertex_from_arrays(state.array_object->arrays, index, index);
        }
    }

    load_last_attributes(state.array_object->arrays, read_index(indices, count - 1));
}

const gl_pipeline_t gl_rsp_pipeline = (gl_pipeline_t) {
    .begin = gl_rsp_begin,
    .end = gl_rsp_end,
    .vertex = gl_rsp_vertex,
    .array_element = gl_rsp_array_element,
    .draw_arrays = gl_rsp_draw_arrays,
    .draw_elements = gl_rsp_draw_elements,
};
