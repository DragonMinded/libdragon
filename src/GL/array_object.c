#include "array_object.h"
#include <stdbool.h>
#include "gl_internal.h"
#include "buffer.h"
#include "pipelines.h"

extern gl_state_t *state;

static draw_call_cache_t **get_current_draw_call_cache_pointer(gl_array_object_t *array_object)
{
    bool is_matrix_index_array_enabled = array_object->arrays[ATTRIB_MTX_INDEX].enabled;
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

    gl_array_t *mtx_index_array = &array_object->arrays[ATTRIB_MTX_INDEX];
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
