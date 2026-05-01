#include "array_object.h"
#include <stdbool.h>
#include "gl_internal.h"
#include "buffer.h"
#include "pipelines.h"
#include "draw_call_cache.h"

extern gl_state_t *state;

extern const cpu_read_attrib_func cpu_read_funcs[ARRAY_COUNT][ATTRIB_TYPE_COUNT];
extern const rsp_read_attrib_func rsp_read_funcs[ARRAY_COUNT][ATTRIB_TYPE_COUNT];

static uint16_t get_stride_from_size_and_type(GLint size, GLenum type)
{
    if (type == GL_SHORT_5_6_5_N64) {
        return sizeof(GLshort);
    }

    uint32_t size_shift = 0;
    
    switch (type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
        size_shift = 0;
        break;
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
    case GL_HALF_FIXED_N64:
        size_shift = 1;
        break;
    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_FLOAT:
        size_shift = 2;
        break;
    case GL_DOUBLE:
        size_shift = 3;
        break;
    }

    return size << size_shift;
}

static void update_array(array_t *array, array_type_t array_type)
{
    array->final_stride = array->stride == 0 ? get_stride_from_size_and_type(array->size, array->type) : array->stride;

    uint32_t func_index = gl_type_to_index(array->type);
    array->cpu_read_func = cpu_read_funcs[array_type][func_index];
    array->rsp_read_func = rsp_read_funcs[array_type][func_index];

    assertf(array->cpu_read_func != NULL, "CPU read function is missing");
    assertf(array->rsp_read_func != NULL, "RSP read function is missing");
}

void array_object_init(gl_array_object_t *obj)
{
    obj->arrays[ARRAY_VERTEX].size = 4;
    obj->arrays[ARRAY_VERTEX].type = GL_FLOAT;
    obj->arrays[ARRAY_NORMAL].size = 3;
    obj->arrays[ARRAY_NORMAL].type = GL_FLOAT;
    obj->arrays[ARRAY_NORMAL].normalize = true;
    obj->arrays[ARRAY_COLOR].size = 4;
    obj->arrays[ARRAY_COLOR].type = GL_FLOAT;
    obj->arrays[ARRAY_COLOR].normalize = true;
    obj->arrays[ARRAY_TEXCOORD].size = 4;
    obj->arrays[ARRAY_TEXCOORD].type = GL_FLOAT;
    obj->arrays[ARRAY_MTX_INDEX].size = 0;
    obj->arrays[ARRAY_MTX_INDEX].type = GL_UNSIGNED_BYTE;

    for (array_type_t i = 0; i < ARRAY_COUNT; i++)
    {
        update_array(&obj->arrays[i], i);
    }

    data_source_init(&obj->vertex_data_source, obj->arrays, ARRAY_MASK_VERTEX | ARRAY_MASK_NORMAL | ARRAY_MASK_COLOR | ARRAY_MASK_TEXCOORD);
    data_source_init(&obj->mtx_index_data_source, obj->arrays, ARRAY_MASK_MTX_INDEX);

    data_cache_init(&obj->vertex_data_cache, &obj->vertex_data_source);
    data_cache_init(&obj->mtx_index_data_cache, &obj->mtx_index_data_source);

    vertex_layout_cache_init(&obj->layout_cache);
}

void array_object_destroy(gl_array_object_t *obj)
{
    if (obj->draw_call_cache != NULL) {
        draw_call_cache_free(obj->draw_call_cache);
    }

    data_cache_destroy(&obj->mtx_index_data_cache);
    data_cache_destroy(&obj->vertex_data_cache);

    buffer_object_set_binding(NULL, &obj->element_array_buffer);

    for (array_type_t i = 0; i < ARRAY_COUNT; i++)
    {
        array_object_set_buffer_binding(obj, i, NULL);
    }
}

static data_source_t *get_data_source(gl_array_object_t *obj, array_type_t array_type)
{
    switch (array_type)
    {
        case ARRAY_VERTEX:
        case ARRAY_NORMAL:
        case ARRAY_COLOR:
        case ARRAY_TEXCOORD:
            return &obj->vertex_data_source;
        case ARRAY_MTX_INDEX:
            return &obj->mtx_index_data_source;
        default:
            assertf(0, "invalid array type");
    }
}

static data_cache_t *get_data_cache(gl_array_object_t *obj, array_type_t array_type)
{
    switch (array_type)
    {
        case ARRAY_VERTEX:
        case ARRAY_NORMAL:
        case ARRAY_COLOR:
        case ARRAY_TEXCOORD:
            return &obj->vertex_data_cache;
        case ARRAY_MTX_INDEX:
            return &obj->mtx_index_data_cache;
        default:
            assertf(0, "invalid array type");
    }
}

static void set_array_data_dirty(gl_array_object_t *obj, array_type_t array_type)
{
    data_cache_t *data_cache = get_data_cache(obj, array_type);
    data_cache_set_data_dirty(data_cache);

    if (data_cache == &obj->mtx_index_data_cache && obj->draw_call_cache != NULL) {
        draw_call_cache_set_data_dirty(obj->draw_call_cache);
    }
}

static void set_array_dirty(gl_array_object_t *obj, array_type_t array_type)
{
    data_source_t *data_source = get_data_source(obj, array_type);
    data_source_set_arrays_dirty(data_source);

    set_array_data_dirty(obj, array_type);
}

void array_object_set_array_enabled(gl_array_object_t *obj, array_type_t array_type, bool enabled)
{
    array_t *array = &obj->arrays[array_type];
    if (array->enabled != enabled) {
        array->enabled = enabled;
        set_array_dirty(obj, array_type);
        vertex_layout_cache_set_dirty(&obj->layout_cache);
    }
}

void array_object_set_array_params(gl_array_object_t *obj, array_type_t array_type, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    if (stride < 0) {
        gl_set_error(GL_INVALID_VALUE, "Stride must not be negative");
        return;
    }

    // From the spec (https://registry.khronos.org/OpenGL/extensions/ARB/ARB_vertex_array_object.txt):
    // An INVALID_OPERATION error is generated if any of the *Pointer commands
    // specifying the location and organization of vertex data are called while
    // a non-zero vertex array object is bound, zero is bound to the
    // ARRAY_BUFFER buffer object, and the pointer is not NULL[fn].
    //     [fn: This error makes it impossible to create a vertex array
    //           object containing client array pointers.]
    if (obj != &state->default_array_object && state->array_buffer == NULL && pointer != NULL) {
        gl_set_error(GL_INVALID_OPERATION, "Vertex array objects can only be used in conjunction with vertex buffer objects");
        return;
    }

    array_t *array = &obj->arrays[array_type];

    array->size = size;
    array->type = type;
    array->stride = stride;
    array->pointer = pointer;
    array_object_set_buffer_binding(obj, array_type, state->array_buffer);

    update_array(array, array_type);
    set_array_dirty(obj, array_type);
}

void array_object_set_buffer_binding(gl_array_object_t *obj, array_type_t array_type, gl_buffer_object_t *buffer)
{
    array_t *array = &obj->arrays[array_type];
    if (array->binding == buffer) return;

    gl_buffer_object_t *old_binding = array->binding;
    if (old_binding != NULL) {
        gl_buffer_remove_array_ref(old_binding, obj);
    }
    if (buffer != NULL) {
        gl_buffer_add_array_ref(buffer, obj);
    }
    array->binding = buffer;

    set_array_dirty(obj, array_type);
}

void array_object_set_buffer_dirty(gl_array_object_t *obj, gl_buffer_object_t *buffer)
{
    for (array_type_t i = 0; i < ARRAY_COUNT; i++)
    {
        if (obj->arrays[i].binding == buffer) {
            set_array_data_dirty(obj, i);
        }
    }

    if (obj->element_array_buffer == buffer && obj->draw_call_cache != NULL) {
        draw_call_cache_set_data_dirty(obj->draw_call_cache);
    }
}

void array_object_validate_drawing(gl_array_object_t *array_object, bool indexed)
{
    for (array_type_t i = 0; i < ARRAY_COUNT; i++)
    {
        array_t *array = &array_object->arrays[i];
        if (array->enabled && array->binding != NULL) {
            buffer_object_validate_not_mapped(array_object->arrays[i].binding);
        }
    }

    if (indexed && array_object->element_array_buffer != NULL) {
        buffer_object_validate_not_mapped(array_object->element_array_buffer);
    }
}

static void update_array_pointer(array_t *array)
{
    if (array->binding != NULL) {
        array->final_pointer = array->binding->storage.data + (uint32_t)array->pointer;
    } else {
        array->final_pointer = array->pointer;
    }
}

void array_object_update_pointers(gl_array_object_t *obj)
{
    for (array_type_t i = 0; i < ARRAY_COUNT; i++)
    {
        update_array_pointer(&obj->arrays[i]);
    }
}

static draw_call_cache_t **get_current_draw_call_cache_pointer(gl_array_object_t *array_object)
{
    bool is_matrix_index_array_enabled = array_object->arrays[ARRAY_MTX_INDEX].enabled;
    if (is_matrix_index_array_enabled) {
        return &array_object->draw_call_cache;
    }

    assertf(array_object->element_array_buffer != NULL, "element array buffer is not bound!");
    return &array_object->element_array_buffer->element_cache;
}

draw_call_cache_t *array_object_get_draw_call_cache(gl_array_object_t *array_object)
{
    draw_call_cache_t **draw_call_cache = get_current_draw_call_cache_pointer(array_object);
    if (*draw_call_cache == NULL) {
        *draw_call_cache = draw_call_cache_create();
    }
    return *draw_call_cache;
}

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

mg_input_assembly_parms_t array_object_get_input_assembly_parms(gl_array_object_t *array_object, GLenum mode, index_bounds_t bounds)
{
    mg_input_assembly_parms_t parms;

    parms.primitive_topology = get_primitive_topology(mode);
    parms.primitive_restart_enabled = false;

    array_t *mtx_index_array = &array_object->arrays[ARRAY_MTX_INDEX];
    if (mtx_index_array->enabled) {
        data_view_t mtx_index_data = data_cache_prepare_at_bounds(&array_object->mtx_index_data_cache, bounds);

        parms.mtx_indices = mtx_index_data.pointer;
        parms.mtx_indices_stride = mtx_index_data.stride;
        parms.matrices = state->matrix_palette;
        parms.matrices_stride = sizeof(state->matrix_palette[0]);
        parms.matrix_uniform = *get_matrices_uniform();
    } else {
        parms.mtx_indices = NULL;
    }

    return parms;
}
