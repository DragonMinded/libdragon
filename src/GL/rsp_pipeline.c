#include <limits.h>

#include "gl_internal.h"
#include "gl_rsp_asm.h"

extern gl_state_t state;

#define DEFINE_BYTE_READ_FUNC(name, src_type, convert) \
    static void name(gl_cmd_stream_t *s, const src_type *src, uint32_t count) \
    { \
        for (uint32_t i = 0; i < count; i++) gl_cmd_stream_put_byte(s, convert(src[i])); \
    }

#define DEFINE_HALF_READ_FUNC(name, src_type, convert) \
    static void name(gl_cmd_stream_t *s, const src_type *src, uint32_t count) \
    { \
        for (uint32_t i = 0; i < count; i++) gl_cmd_stream_put_half(s, convert(src[i])); \
    }

static void read_fixed_point(gl_cmd_stream_t *s, const int16u_t *src, uint32_t count, int shift)
{
    if (shift > 0) {
        for (uint32_t i = 0; i < count; i++) {
            int16_t value = src[i];
            assertf(value <= SHRT_MAX>>shift && value >= SHRT_MIN>>shift, "Fixed point overflow: %d << %d", value, shift);
            gl_cmd_stream_put_half(s, value << shift);
        }
    } else {
        for (uint32_t i = 0; i < count; i++) {
            gl_cmd_stream_put_half(s, src[i] >> -shift);
        }
    }
}

#define DEFINE_HALF_FIXED_READ_FUNC(name, precision) \
    static void name(gl_cmd_stream_t *s, const int16u_t *src, uint32_t count) \
    { \
        read_fixed_point(s, src, count, precision.shift_amount); \
    }

#define VTX_CONVERT_INT(v) ((v) << VTX_SHIFT)
#define VTX_CONVERT_FLT(v) ((v) * (1<<VTX_SHIFT))

DEFINE_HALF_READ_FUNC(vtx_read_i8,    int8_t,    VTX_CONVERT_INT)
DEFINE_HALF_READ_FUNC(vtx_read_i16,   int16u_t,  VTX_CONVERT_INT)
DEFINE_HALF_READ_FUNC(vtx_read_i32,   int32u_t,  VTX_CONVERT_INT)
DEFINE_HALF_READ_FUNC(vtx_read_f32,   floatu,    VTX_CONVERT_FLT)
DEFINE_HALF_READ_FUNC(vtx_read_f64,   doubleu,   VTX_CONVERT_FLT)
DEFINE_HALF_FIXED_READ_FUNC(vtx_read_x16, state.vertex_halfx_precision)

#define COL_CONVERT_U8(v) ((v) << 7)
#define COL_CONVERT_I8(v) ((v) << 8)
#define COL_CONVERT_U16(v) ((v) >> 1)
#define COL_CONVERT_I16(v) ((v))
#define COL_CONVERT_U32(v) ((v) >> 17)
#define COL_CONVERT_I32(v) ((v) >> 16)
#define COL_CONVERT_F32(v) (FLOAT_TO_I16(v))
#define COL_CONVERT_F64(v) (FLOAT_TO_I16(v))

DEFINE_HALF_READ_FUNC(col_read_u8,  uint8_t,   COL_CONVERT_U8)
DEFINE_HALF_READ_FUNC(col_read_i8,  int8_t,    COL_CONVERT_I8)
DEFINE_HALF_READ_FUNC(col_read_u16, uint16u_t, COL_CONVERT_U16)
DEFINE_HALF_READ_FUNC(col_read_i16, int16u_t,  COL_CONVERT_I16)
DEFINE_HALF_READ_FUNC(col_read_u32, uint32u_t, COL_CONVERT_U32)
DEFINE_HALF_READ_FUNC(col_read_i32, int32u_t,  COL_CONVERT_I32)
DEFINE_HALF_READ_FUNC(col_read_f32, floatu,    COL_CONVERT_F32)
DEFINE_HALF_READ_FUNC(col_read_f64, doubleu,   COL_CONVERT_F64)

#define TEX_CONVERT_INT(v) ((v) << TEX_SHIFT)
#define TEX_CONVERT_FLT(v) ((v) * (1<<TEX_SHIFT))

DEFINE_HALF_READ_FUNC(tex_read_i8,   int8_t,    TEX_CONVERT_INT)
DEFINE_HALF_READ_FUNC(tex_read_i16,  int16u_t,  TEX_CONVERT_INT)
DEFINE_HALF_READ_FUNC(tex_read_i32,  int32u_t,  TEX_CONVERT_INT)
DEFINE_HALF_READ_FUNC(tex_read_f32,  floatu,    TEX_CONVERT_FLT)
DEFINE_HALF_READ_FUNC(tex_read_f64,  doubleu,   TEX_CONVERT_FLT)
DEFINE_HALF_FIXED_READ_FUNC(tex_read_x16, state.texcoord_halfx_precision)

#define NRM_CONVERT_U8(v) ((v) >> 1)
#define NRM_CONVERT_I8(v) ((v))
#define NRM_CONVERT_U16(v) ((v) >> 9)
#define NRM_CONVERT_I16(v) ((v) >> 8)
#define NRM_CONVERT_U32(v) ((v) >> 25)
#define NRM_CONVERT_I32(v) ((v) >> 24)
#define NRM_CONVERT_F32(v) ((v) * 0x7F)
#define NRM_CONVERT_F64(v) ((v) * 0x7F)

DEFINE_BYTE_READ_FUNC(nrm_read_i8,  int8_t,    NRM_CONVERT_I8)
DEFINE_BYTE_READ_FUNC(nrm_read_i16, int16u_t,  NRM_CONVERT_I16)
DEFINE_BYTE_READ_FUNC(nrm_read_i32, int32u_t,  NRM_CONVERT_I32)
DEFINE_BYTE_READ_FUNC(nrm_read_f32, floatu,    NRM_CONVERT_F32)
DEFINE_BYTE_READ_FUNC(nrm_read_f64, doubleu,   NRM_CONVERT_F64)

#define MTX_INDEX_CONVERT(v) (v)

DEFINE_BYTE_READ_FUNC(mtx_index_read_u8,  uint8_t,   MTX_INDEX_CONVERT)
DEFINE_BYTE_READ_FUNC(mtx_index_read_u16, uint16u_t, MTX_INDEX_CONVERT)
DEFINE_BYTE_READ_FUNC(mtx_index_read_u32, uint32u_t, MTX_INDEX_CONVERT)

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
    },
};

static const gl_array_t dummy_arrays[ATTRIB_COUNT] = {
    { .enabled = true, .size = 4 }
};

typedef enum {
    BEGIN_END_INDETERMINATE,
    BEGIN_END_VERTEX,
    BEGIN_END_ARRAY_ELEMENT,
} begin_end_type_t;

static begin_end_type_t begin_end_type;
static uint32_t vtx_cmd_size;

static void upload_current_attributes(const gl_array_t *arrays)
{
    if (arrays[ATTRIB_COLOR].enabled) {
        gl_set_current_color(state.current_attributes.color);
    }

    if (arrays[ATTRIB_TEXCOORD].enabled) {
        gl_set_current_texcoords(state.current_attributes.texcoord);
    }

    if (arrays[ATTRIB_NORMAL].enabled) {
        gl_set_current_normal(state.current_attributes.normal);
    }

    if (arrays[ATTRIB_MTX_INDEX].enabled) {
        gl_set_current_mtx_index(state.current_attributes.mtx_index);
    }
}

static void load_attribs_at_index(const gl_array_t *arrays, uint32_t index)
{
    gl_fill_all_attrib_defaults(arrays);
    gl_load_attribs(arrays, index);
}

static void load_last_attributes(const gl_array_t *arrays, uint32_t last_index)
{
    load_attribs_at_index(arrays, last_index);
    upload_current_attributes(arrays);
}

static void glp_set_attrib(gl_array_type_t array_type, const void *value, GLenum type, uint32_t size)
{
    static const glp_command_t cmd_table[] = { GLP_CMD_SET_LONG, GLP_CMD_SET_LONG, GLP_CMD_SET_WORD, GLP_CMD_SET_BYTE };
    static const uint32_t cmd_size_table[] = { 3, 3, 2, 2 };
    static const uint32_t offset_table[] = {
        offsetof(gl_server_state_t, color),
        offsetof(gl_server_state_t, tex_coords),
        offsetof(gl_server_state_t, normal),
        offsetof(gl_server_state_t, mtx_index)
    };
    static const int16_t default_value_table[][4] = {
        { 0, 0, 0, 0x7FFF },
        { 0, 0, 0, 1 }
    };

    uint32_t table_index = array_type - 1;

    const uint32_t offset = offset_table[table_index];

    const rsp_read_attrib_func *read_funcs = rsp_read_funcs[array_type];
    const rsp_read_attrib_func read_func = read_funcs[gl_type_to_index(type)];

    gl_cmd_stream_t s = gl_cmd_stream_begin(glp_overlay_id, cmd_table[table_index], cmd_size_table[table_index]);
    gl_cmd_stream_put_half(&s, offset);

    switch (array_type) {
    case ATTRIB_COLOR:
    case ATTRIB_TEXCOORD:
        read_func(&s, value, size);
        read_funcs[gl_type_to_index(GL_SHORT)](&s, default_value_table[table_index] + size, 4 - size);
        break;
    case ATTRIB_NORMAL:
        read_func(&s, value, size);
        break;
    case ATTRIB_MTX_INDEX:
        for (uint32_t i = 0; i < 3; i++) gl_cmd_stream_put_byte(&s, 0);
        read_func(&s, value, size);
        break;
    default:
        assert(!"Unexpected array type");
        break;
    }

    gl_cmd_stream_end(&s);
}

static void set_attrib(gl_array_type_t array_type, const void *value, GLenum type, uint32_t size)
{
    glp_set_attrib(array_type, value, type, size);
    gl_read_attrib(array_type, value, type, size);
}

static bool check_last_array_element(int32_t *index)
{
    if (state.last_array_element >= 0) {
        *index = state.last_array_element;
        state.last_array_element = -1;
        return true;
    }

    return false;
}

static void require_array_element(const gl_array_t *arrays)
{
    int32_t index;
    if (check_last_array_element(&index)) {
        for (uint32_t i = 0; i < ATTRIB_COUNT; i++)
        {
            const gl_array_t *array = &arrays[i];
            const void *value = gl_get_attrib_element(array, index);
            set_attrib(i, value, array->type, array->size);
        }
    }
}

static inline gl_cmd_stream_t write_vertex_begin(uint32_t cache_index)
{
    gl_cmd_stream_t s = gl_cmd_stream_begin(glp_overlay_id, GLP_CMD_SET_PRIM_VTX, vtx_cmd_size>>2);
    gl_cmd_stream_put_half(&s, cache_index * PRIM_VTX_SIZE);
    return s;
}

static inline void write_vertex_end(gl_cmd_stream_t *s)
{
    gl_cmd_stream_end(s);
}

static void write_vertex_from_arrays(const gl_array_t *arrays, uint32_t index, uint8_t cache_index)
{
    gl_cmd_stream_t s = write_vertex_begin(cache_index);

    for (uint32_t i = 0; i < ATTRIB_COUNT; i++)
    {
        const gl_array_t *array = &arrays[i];
        if (!array->enabled) {
            continue;
        }

        const void *src = gl_get_attrib_element(array, index);
        array->rsp_read_func(&s, src, array->size);
    }

    write_vertex_end(&s);
}

static void submit_vertex(uint32_t cache_index)
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

static void gl_asm_vtx_loader(const gl_array_t *arrays)
{
    extern uint8_t rsp_gl_pipeline_text_start[];

    rspq_write_t w = rspq_write_begin(glp_overlay_id, GLP_CMD_SET_VTX_LOADER, 3 + VTX_LOADER_MAX_COMMANDS);
    rspq_write_arg(&w, PhysicalAddr(rsp_gl_pipeline_text_start) - 0x1000);

    uint32_t pointer = PhysicalAddr(w.pointer);
    bool aligned = (pointer & 0x7) == 0;

    rspq_write_arg(&w, aligned ? pointer + 8 : pointer + 4);

    if (aligned) {
        rspq_write_arg(&w, 0);
    }

    const uint8_t default_reg = 16;
    const uint8_t current_reg = 17;
    const uint8_t cmd_ptr_reg = 20;
    const uint8_t norm_reg = 2;
    const uint8_t mtx_index_reg = 3;
    const uint8_t dst_vreg_base = 24;
    const uint32_t current_normal_offset = offsetof(gl_server_state_t, normal) - offsetof(gl_server_state_t, color);
    const uint32_t current_mtx_index_offset = offsetof(gl_server_state_t, mtx_index) - offsetof(gl_server_state_t, color);

    uint32_t cmd_offset = 0;

    for (uint32_t i = 0; i < ATTRIB_NORMAL; i++)
    {
        const uint32_t dst_vreg = dst_vreg_base + i;
        const gl_array_t *array = &arrays[i];

        if (!array->enabled) {
            rspq_write_arg(&w, rsp_asm_lwc2(VLOAD_DOUBLE, dst_vreg, 0, i-1, current_reg));
        } else {
            uint32_t cmd_size = array->size * 2;
            uint32_t alignment = next_pow2(cmd_size);
            if (cmd_offset & (alignment-1)) {
                rspq_write_arg(&w, rsp_asm_addi(cmd_ptr_reg, cmd_ptr_reg, cmd_offset));
                cmd_offset = 0;
            }

            switch (array->size)
            {
            case 1:
                rspq_write_arg(&w, rsp_asm_lwc2(VLOAD_DOUBLE, dst_vreg, 0, (i*8)>>3, default_reg));
                rspq_write_arg(&w, rsp_asm_lwc2(VLOAD_HALF, dst_vreg, 0, cmd_offset>>1, cmd_ptr_reg));
                break;
            case 2:
                rspq_write_arg(&w, rsp_asm_lwc2(VLOAD_LONG, dst_vreg, 0, cmd_offset>>2, cmd_ptr_reg));
                rspq_write_arg(&w, rsp_asm_lwc2(VLOAD_LONG, dst_vreg, 4, ((i*8)>>2) + 1, default_reg));
                break;
            case 3:
                rspq_write_arg(&w, rsp_asm_lwc2(VLOAD_DOUBLE, dst_vreg, 0, cmd_offset>>3, cmd_ptr_reg));
                rspq_write_arg(&w, rsp_asm_lwc2(VLOAD_HALF, dst_vreg, 6, ((i*8)>>1) + 3, default_reg));
                break;
            case 4:
                rspq_write_arg(&w, rsp_asm_lwc2(VLOAD_DOUBLE, dst_vreg, 0, cmd_offset>>3, cmd_ptr_reg));
                break;
            }

            cmd_offset += cmd_size;
        }
    }

    // TODO: optimize for when both normal and matrix index com from the same source (They fit into a single word)

    if (!arrays[ATTRIB_NORMAL].enabled) {
        rspq_write_arg(&w, rsp_asm_lw(norm_reg, current_normal_offset, current_reg));
    } else {
        rspq_write_arg(&w, rsp_asm_lw(norm_reg, cmd_offset, cmd_ptr_reg));
        cmd_offset += 3;
    }

    if (!arrays[ATTRIB_MTX_INDEX].enabled) {
        rspq_write_arg(&w, rsp_asm_lbu(mtx_index_reg, current_mtx_index_offset, current_reg));
    } else {
        rspq_write_arg(&w, rsp_asm_lbu(mtx_index_reg, cmd_offset, cmd_ptr_reg));
    }

    rspq_write_end(&w);
}

static uint32_t get_vertex_cmd_size(const gl_array_t *arrays)
{
    uint32_t cmd_size = 4;

    for (uint32_t i = 0; i < ATTRIB_NORMAL; i++)
    {
        if (arrays[i].enabled) {
            cmd_size += arrays[i].size * 2;
        }
    }
    if (arrays[ATTRIB_NORMAL].enabled) {
        cmd_size += 3;
    }
    if (arrays[ATTRIB_MTX_INDEX].enabled) {
        cmd_size += 1;
    }

    return ROUND_UP(cmd_size, 4);
}

static void gl_update_vertex_cmd_size(const gl_array_t *arrays)
{
    vtx_cmd_size = get_vertex_cmd_size(arrays);

    // TODO: This is dependent on the layout of data structures internal to rspq.
    //       How can we make it more robust?

    extern uint8_t rsp_queue_data_start[];
    extern uint8_t rsp_queue_data_end[0];
    extern uint8_t rsp_gl_pipeline_data_start[];

    uint32_t ovl_data_offset = rsp_queue_data_end - rsp_queue_data_start;
    uint8_t *rsp_gl_pipeline_ovl_header = rsp_gl_pipeline_data_start + ovl_data_offset;

    #define CMD_DESC_SIZE   2

    uint16_t *cmd_descriptor = (uint16_t*)(rsp_gl_pipeline_ovl_header + RSPQ_OVERLAY_HEADER_SIZE + GLP_CMD_SET_PRIM_VTX*CMD_DESC_SIZE);
    uint16_t patched_cmd_descriptor = (*cmd_descriptor & 0x3FF) | ((vtx_cmd_size & 0xFC) << 8);

    data_cache_hit_writeback_invalidate(cmd_descriptor, CMD_DESC_SIZE);
    glpipe_set_vtx_cmd_size(patched_cmd_descriptor, cmd_descriptor);
}

static void gl_prepare_vtx_cmd(const gl_array_t *arrays)
{
    gl_asm_vtx_loader(arrays);
    gl_update_vertex_cmd_size(arrays);
}

static void gl_rsp_begin()
{
    glpipe_init();
    state.last_array_element = -1;
    begin_end_type = BEGIN_END_INDETERMINATE;
}

static void gl_rsp_end()
{
    int32_t index;
    if (check_last_array_element(&index)) {
        load_last_attributes(state.array_object->arrays, index);
    }

    if (state.begin_end_active) {
        // TODO: Load from arrays
        gl_set_current_color(state.current_attributes.color);
        gl_set_current_texcoords(state.current_attributes.texcoord);
        gl_set_current_normal(state.current_attributes.normal);
        gl_set_current_mtx_index(state.current_attributes.mtx_index);
    }
}

static void gl_rsp_vertex(const void *value, GLenum type, uint32_t size)
{
    if (begin_end_type != BEGIN_END_VERTEX) {
        gl_prepare_vtx_cmd(dummy_arrays);
        begin_end_type = BEGIN_END_VERTEX;
    }

    static const int16_t default_values[] = { 0, 0, 0, 1 };

    uint8_t cache_index;
    if (gl_get_cache_index(next_prim_id(), &cache_index))
    {
        require_array_element(state.array_object->arrays);

        rsp_read_attrib_func read_func = rsp_read_funcs[ATTRIB_VERTEX][gl_type_to_index(type)];

        gl_cmd_stream_t s = write_vertex_begin(cache_index);
        read_func(&s, value, size);
        vtx_read_i16(&s, default_values + size, 4 - size);
        write_vertex_end(&s);
    }

    submit_vertex(cache_index);
}

static void gl_rsp_color(const void *value, GLenum type, uint32_t size)
{
    set_attrib(ATTRIB_COLOR, value, type, size);
}

static void gl_rsp_tex_coord(const void *value, GLenum type, uint32_t size)
{
    set_attrib(ATTRIB_TEXCOORD, value, type, size);
}

static void gl_rsp_normal(const void *value, GLenum type, uint32_t size)
{
    set_attrib(ATTRIB_NORMAL, value, type, size);
}

static void gl_rsp_mtx_index(const void *value, GLenum type, uint32_t size)
{
    set_attrib(ATTRIB_MTX_INDEX, value, type, size);
}

static void gl_rsp_array_element(uint32_t index)
{
    if (begin_end_type != BEGIN_END_ARRAY_ELEMENT) {
        gl_prepare_vtx_cmd(state.array_object->arrays);
        begin_end_type = BEGIN_END_ARRAY_ELEMENT;
    }

    draw_vertex_from_arrays(state.array_object->arrays, index, index);
    state.last_array_element = index;
}

static void gl_rsp_draw_arrays(uint32_t first, uint32_t count)
{
    if (state.array_object->arrays[ATTRIB_VERTEX].enabled) {
        gl_prepare_vtx_cmd(state.array_object->arrays);
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
        gl_prepare_vtx_cmd(state.array_object->arrays);
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
    .color = gl_rsp_color,
    .tex_coord = gl_rsp_tex_coord,
    .normal = gl_rsp_normal,
    .mtx_index = gl_rsp_mtx_index,
    .array_element = gl_rsp_array_element,
    .draw_arrays = gl_rsp_draw_arrays,
    .draw_elements = gl_rsp_draw_elements,
};
