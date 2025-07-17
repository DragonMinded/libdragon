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

void prepare_vertex_buffer(uint32_t first, uint32_t count)
{
    array_object_update(state->array_object, first, count);
    mg_bind_vertex_buffer(state->array_object->buffer);
}

void prepare_pipeline()
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
    mg_uniform_load(matrices_uniform, &state->uniform_data->matrices);
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    if (!state->is_drawing_anything || count == 0) return;
    
    prepare_vertex_buffer(first, count);
    prepare_pipeline();
    mg_draw_begin(); // TODO: detect if modes have actually changed to batch draw commands
    mg_draw(&(mg_input_assembly_parms_t) {
        .primitive_topology = get_primitive_topology(mode)
    }, count, first);
    mg_draw_end();
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    assertf(type == GL_UNSIGNED_SHORT, "Index type must be GL_UNSIGNED_SHORT");

    if (!state->is_drawing_anything || count == 0) return;

    if (state->element_array_buffer != NULL) {
        indices = state->element_array_buffer->storage.data + (uint32_t)indices;
    }

    const uint16_t *indices_i16 = indices;

    // TODO: cache these values in the element array buffer object
    uint16_t min_index = USHRT_MAX, max_index = 0;
    for (size_t i = 0; i < count; i++)
    {
        if (indices_i16[i] < min_index) min_index = indices_i16[i];
        if (indices_i16[i] > max_index) max_index = indices_i16[i];
    }

    prepare_vertex_buffer(min_index, max_index - min_index);
    prepare_pipeline();
    mg_draw_begin();
    mg_draw_indexed(&(mg_input_assembly_parms_t) {
        .primitive_topology = get_primitive_topology(mode)
    }, indices_i16, count, -min_index);
    mg_draw_end();
}

void update_viewport()
{
    const surface_t *fb = rdpq_get_attached();
    if (fb == NULL) return;

    mg_set_viewport(&(mg_viewport_t) {
        .x = state->viewport.x,
        .y = fb->height - (state->viewport.y + state->viewport.h),
        .width = state->viewport.w,
        .height = state->viewport.h,
        .minDepth = state->viewport.n,
        .maxDepth = state->viewport.f
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

mg_cull_mode_t get_cull_mode(GLenum cull_face)
{
    switch (cull_face)
    {
    case GL_BACK:
        return MG_CULL_MODE_BACK;
    case GL_FRONT:
        return MG_CULL_MODE_FRONT;
    default:
        return -1;
    }
}

mg_front_face_t get_front_face(GLenum front_face)
{
    switch (front_face)
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
        .cull_mode = get_cull_mode(state->cull_face_mode),
        .front_face = get_front_face(state->front_face)
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

