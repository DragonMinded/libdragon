#include "gl_internal.h"
#include "rdpq_attach.h"

#include <limits.h>

extern gl_state_t *state;

void update_culling();

void gl_primitive_init()
{
    state->viewport.n = 0;
    state->viewport.f = 1;

    state->cull_face_mode = GL_BACK;
    state->front_face = GL_CCW;
    update_culling();
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
        return MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;

    case GL_POINTS:
    case GL_LINES:
    case GL_LINE_LOOP:
    case GL_LINE_STRIP:
    case GL_QUADS:
    case GL_QUAD_STRIP:
    case GL_POLYGON:
    default:
        assertf(0, "Draw mode %ld is not supported", mode);
    }
}

static void prepare_pipeline()
{
    mg_pipeline_t *pipeline = state->pipelines[state->array_object->pipeline_index].pipeline;
    mg_pipeline_bind(pipeline);

    // TODO: cache these
    const mg_uniform_t *fog_uniform = mg_pipeline_get_uniform(pipeline, MGFX_BINDING_FOG);
    const mg_uniform_t *lighting_uniform = mg_pipeline_get_uniform(pipeline, MGFX_BINDING_LIGHTING);
    const mg_uniform_t *texturing_uniform = mg_pipeline_get_uniform(pipeline, MGFX_BINDING_TEXTURING);
    const mg_uniform_t *matrices_uniform = mg_pipeline_get_uniform(pipeline, MGFX_BINDING_MATRICES);

    mg_uniform_load(fog_uniform, &state->uniform_data->fog);
    mg_uniform_load(lighting_uniform, &state->uniform_data->lighting);
    mg_uniform_load(texturing_uniform, &state->uniform_data->texturing);

    gl_update_matrix_targets();

    gl_matrix_target_t *mtx_target = &state->default_matrix_target;
    fm_mat4_t *mv = gl_matrix_stack_get_matrix(mtx_target->mv_stack);

    mgfx_set_matrices_inline(matrices_uniform, &(mgfx_matrices_parms_t) {
        .model_view_projection = mtx_target->mvp.m[0],
        .model_view = mv->m[0],
        .normal = mv->m[0] // TODO: transpose inverse
    });
}

static void prepare_vertex_buffer(uint32_t first, uint32_t count)
{
    array_object_update(state->array_object, first, count);

    // It's possible that we are now accessing a sub-range of a previously cached buffer.
    // In that case we need to apply an offset, since the draw command expects the first vertex at offset 0.
    uint32_t buffer_offset = first - state->array_object->cached_first;
    mg_bind_vertex_buffer(((uint8_t*)state->array_object->buffer) + buffer_offset * state->array_object->layout.vertex_layout.stride);

    prepare_pipeline();
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    if (!state->is_drawing_anything || count == 0) return;
    
    prepare_vertex_buffer(first, count);
    mg_draw_begin(); // TODO: detect if modes have actually changed to batch draw commands
    mg_draw(&(mg_input_assembly_parms_t) {
        .primitive_topology = get_primitive_topology(mode)
    }, count, first);
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
        mg_draw_indexed(input_assembly_parms, indices_i16, count, -cache->min_index);
        cache->block = rspq_block_end();
        cache->is_data_dirty = false;
    }

    *min_index = cache->min_index;
    *max_index = cache->max_index;
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    assertf(type == GL_UNSIGNED_SHORT, "Index type must be GL_UNSIGNED_SHORT");

    if (!state->is_drawing_anything || count == 0) return;

    uint16_t min_index, max_index;
    mg_input_assembly_parms_t input_assembly_parms = {
        .primitive_topology = get_primitive_topology(mode)
    };

    gl_buffer_object_t *element_buffer = state->array_object->element_array_buffer;
    if (element_buffer != NULL) {
        update_element_array_cache(element_buffer, count, (uint32_t)indices, &input_assembly_parms, &min_index, &max_index);
    } else {
        find_index_bounds(indices, count, &min_index, &max_index);
    }

    prepare_vertex_buffer(min_index, max_index - min_index + 1);

    mg_draw_begin();
    if (element_buffer != NULL) {
        rspq_block_run(element_buffer->element_cache->block);
    } else {
        mg_draw_indexed(&input_assembly_parms, indices, count, -min_index);
    }
    mg_draw_end();
}

void update_viewport()
{
    const surface_t *fb = rdpq_get_attached();
    if (fb == NULL) return;

    mg_set_viewport(&(mg_viewport_t) {
        .x = state->viewport.x,
        .y = fb->height - state->viewport.y,
        .width = state->viewport.w,
        .height = -state->viewport.h,
        .minDepth = state->viewport.n,
        .maxDepth = state->viewport.f,
        .z_near = state->near_plane,
        .z_far = state->far_plane
    });
}

void glDepthRange(GLclampd n, GLclampd f)
{
    state->viewport.n = n;
    state->viewport.f = f;
    update_viewport();
}

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    state->viewport.x = x;
    state->viewport.y = y;
    state->viewport.w = w;
    state->viewport.h = h;
    update_viewport();
}

mg_cull_mode_t get_cull_mode()
{
    if (!state->cull_face) {
        return MG_CULL_MODE_NONE;
    }

    switch (state->cull_face_mode)
    {
    case GL_BACK:
        return MG_CULL_MODE_BACK;
    case GL_FRONT:
        return MG_CULL_MODE_FRONT;
    default:
        return -1;
    }
}

mg_front_face_t get_front_face()
{
    switch (state->front_face)
    {
    case GL_CW:
        return MG_FRONT_FACE_CLOCKWISE;
    case GL_CCW:
        return MG_FRONT_FACE_COUNTER_CLOCKWISE;
    default:
        return -1;
    }
}

void update_culling()
{
    state->is_drawing_anything = state->cull_face_mode != GL_FRONT_AND_BACK;
    if (!state->is_drawing_anything) return;

    mg_set_culling(&(mg_culling_parms_t) {
        .cull_mode = get_cull_mode(),
        .front_face = get_front_face()
    });
}

void glCullFace(GLenum mode)
{
    if (!gl_ensure_no_begin_end()) return;

    switch (mode) {
    case GL_BACK:
    case GL_FRONT:
    case GL_FRONT_AND_BACK:
        state->cull_face_mode = mode;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid face culling mode", mode);
        return;
    }

    update_culling();
}

void glFrontFace(GLenum dir)
{
    if (!gl_ensure_no_begin_end()) return;

    switch (dir) {
    case GL_CW:
    case GL_CCW:
        state->front_face = dir;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid front face winding direction", dir);
        return;
    }

    update_culling();
}

