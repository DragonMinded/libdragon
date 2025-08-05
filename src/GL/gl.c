/**
 * @file gl.c
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 * @brief OpenGL core initialization and state management.
 */
#include "GL/gl.h"
#include "gl_internal.h"
#include "magma.h"
#include "mgfx.h"
#include "rdpq.h"
#include "rdpq_attach.h"

gl_state_t *state;

void gl_init(void)
{
    mg_init();
    rdpq_init();

    state = calloc(1, sizeof(gl_state_t));
    state->is_pipeline_dirty = true;
    state->uniform_data = malloc_uncached(sizeof(gl_uniform_data));

    mgfx_get_fog(&state->uniform_data->fog, &(mgfx_fog_parms_t) {});

    gl_rendermode_init();
    gl_array_init();
    gl_primitive_init();
    gl_matrix_init();
    gl_lighting_init();
    gl_texture_init();

    glClearColor(0, 0, 0, 0);
    glClearDepth(1);
}

void gl_close(void)
{
    gl_array_close();
    gl_texture_close();
    rspq_wait();
    free(state->uniform_data);
    free(state);

    mg_close();
    rdpq_close();
}

void gl_context_begin()
{
    const surface_t *old_color_buffer = state->color_buffer;
    
    state->color_buffer = rdpq_get_attached();
    assertf(state->color_buffer, "GL: Tried to begin rendering without framebuffer attached");

    uint32_t width = state->color_buffer->width;
    uint32_t height = state->color_buffer->height;

    if (old_color_buffer == NULL || old_color_buffer->width != width || old_color_buffer->height != height) {
        glViewport(0, 0, width, height);
        glScissor(0, 0, width, height);
    }

    gl_set_rendermode_dirty();
    gl_set_texturing_dirty();
}

void gl_context_end()
{
}

GLenum glGetError(void)
{
    if (!gl_ensure_no_begin_end()) return 0;

    GLenum error = state->current_error;
    state->current_error = GL_NO_ERROR;
    return error;
}

void set_enable_flag(GLenum target, bool value)
{
    switch (target) {
    case GL_RDPQ_MATERIAL_N64:
        break;
    case GL_RDPQ_TEXTURING_N64:
        break;
    case GL_SCISSOR_TEST:
        state->scissor_test = value;
        break;
    case GL_DEPTH_TEST:
        state->depth_test = value;
        gl_set_geom_flags_dirty();
        gl_set_rendermode_dirty();
        break;
    case GL_BLEND:
        state->blend = value;
        gl_set_rendermode_dirty();
        break;
    case GL_ALPHA_TEST:
        state->alpha_test = value;
        gl_set_rendermode_dirty();
        break;
    case GL_DITHER:
        state->dither = value;
        gl_set_rendermode_dirty();
        break;
    case GL_FOG:
        gl_set_fog_enabled(value);
        break;
    case GL_MULTISAMPLE_ARB:
        state->multisample = value;
        gl_set_rendermode_dirty();
        break;
    case GL_TEXTURE_1D:
    case GL_TEXTURE_2D:
        gl_set_texture_enabled(target, value);
        break;
    case GL_CULL_FACE:
        state->cull_face = value;
        update_culling();
        break;
    case GL_LIGHTING:
        state->lighting = value;
        gl_set_lighting_dirty();
        gl_set_geom_flags_dirty();
        break;
    case GL_LIGHT0:
    case GL_LIGHT1:
    case GL_LIGHT2:
    case GL_LIGHT3:
    case GL_LIGHT4:
    case GL_LIGHT5:
    case GL_LIGHT6:
    case GL_LIGHT7:
        gl_set_light_enabled(target, value);
        break;
    case GL_COLOR_MATERIAL:
        state->color_material = value;
        gl_set_combiner_dirty();
        break;
    case GL_TEXTURE_GEN_S:
    case GL_TEXTURE_GEN_T:
    case GL_TEXTURE_GEN_R:
    case GL_TEXTURE_GEN_Q:
        break;
    case GL_NORMALIZE:
        break;
    case GL_MATRIX_PALETTE_ARB:
        state->matrix_palette_enabled = value;
        break;
    case GL_TEXTURE_FLIP_T_N64:
        break;
    case GL_CLIP_PLANE0:
    case GL_CLIP_PLANE1:
    case GL_CLIP_PLANE2:
    case GL_CLIP_PLANE3:
    case GL_CLIP_PLANE4:
    case GL_CLIP_PLANE5:
        assertf(!value, "User clip planes are not supported!");
        break;
    case GL_STENCIL_TEST:
        assertf(!value, "Stencil test is not supported!");
        break;
    case GL_COLOR_LOGIC_OP:
    case GL_INDEX_LOGIC_OP:
        assertf(!value, "Logical pixel operation is not supported!");
        break;
    case GL_POINT_SMOOTH:
    case GL_LINE_SMOOTH:
    case GL_POLYGON_SMOOTH:
        assertf(!value, "Smooth rendering is not supported (Use multisampling instead)!");
        break;
    case GL_LINE_STIPPLE:
    case GL_POLYGON_STIPPLE:
        assertf(!value, "Stipple is not supported!");
        break;
    case GL_POLYGON_OFFSET_FILL:
    case GL_POLYGON_OFFSET_LINE:
    case GL_POLYGON_OFFSET_POINT:
        assertf(!value, "Polygon offset is not supported!");
        break;
    case GL_SAMPLE_ALPHA_TO_COVERAGE_ARB:
    case GL_SAMPLE_ALPHA_TO_ONE_ARB:
    case GL_SAMPLE_COVERAGE_ARB:
        assertf(!value, "Coverage value manipulation is not supported!");
        break;
    case GL_MAP1_COLOR_4:
    case GL_MAP1_INDEX:
    case GL_MAP1_NORMAL:
    case GL_MAP1_TEXTURE_COORD_1:
    case GL_MAP1_TEXTURE_COORD_2:
    case GL_MAP1_TEXTURE_COORD_3:
    case GL_MAP1_TEXTURE_COORD_4:
    case GL_MAP1_VERTEX_3:
    case GL_MAP1_VERTEX_4:
    case GL_MAP2_COLOR_4:
    case GL_MAP2_INDEX:
    case GL_MAP2_NORMAL:
    case GL_MAP2_TEXTURE_COORD_1:
    case GL_MAP2_TEXTURE_COORD_2:
    case GL_MAP2_TEXTURE_COORD_3:
    case GL_MAP2_TEXTURE_COORD_4:
    case GL_MAP2_VERTEX_3:
    case GL_MAP2_VERTEX_4:
        assertf(!value, "Evaluators are not supported!");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid enable target", target);
        return;
    }
}

void glEnable(GLenum target)
{
    if (!gl_ensure_no_begin_end()) return;
    set_enable_flag(target, true);
}

void glDisable(GLenum target)
{
    if (!gl_ensure_no_begin_end()) return;
    set_enable_flag(target, false);
}

void glClear(GLbitfield buf)
{
    if (!gl_ensure_no_begin_end()) return;
    
    if (!buf) {
        return;
    }

    if (buf & (GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT)) {
        assertf(0, "Only color and depth buffers are supported!");
    }

    if (buf & GL_DEPTH_BUFFER_BIT) {
        rdpq_clear_z(state->clear_depth);
    }

    if (buf & GL_COLOR_BUFFER_BIT) {
        rdpq_clear(state->clear_color);
    }
}

void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a)
{
    if (!gl_ensure_no_begin_end()) return;
    state->clear_color = RGBA32(CLAMPF_TO_U8(r), CLAMPF_TO_U8(g), CLAMPF_TO_U8(b), CLAMPF_TO_U8(a));
}

void glClearDepth(GLclampd d)
{
    if (!gl_ensure_no_begin_end()) return;
    state->clear_depth = ZBUF_VAL(d);
}

void glDitherModeN64(rdpq_dither_t mode)
{
    state->dither_mode = mode;
    gl_set_rendermode_dirty();
}

void glFlush(void)
{
    if (!gl_ensure_no_begin_end()) return;
    
    rspq_flush();
}

void glFinish(void)
{
    if (!gl_ensure_no_begin_end()) return;
    
    rspq_wait();
}

void glHint(GLenum target, GLenum hint)
{
    if (!gl_ensure_no_begin_end()) return;
    
    switch (target)
    {
    case GL_PERSPECTIVE_CORRECTION_HINT:
        // Use perspective correction by default, unless it was explicitly turned off
        state->persp_correct = hint != GL_FASTEST;
        gl_set_rendermode_dirty();
        break;
    case GL_FOG_HINT:
        // TODO: per-pixel fog
        break;
    case GL_MULTISAMPLE_HINT_N64:
        // Use full AA by default, unless RA has been requested
        state->reduced_aa = hint == GL_FASTEST;
        gl_set_rendermode_dirty();
        break;
    case GL_POINT_SMOOTH_HINT:
    case GL_LINE_SMOOTH_HINT:
    case GL_POLYGON_SMOOTH_HINT:
        // Ignored
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid hint target", target);
        break;
    }
}

bool gl_storage_alloc(gl_storage_t *storage, uint32_t size)
{
    GLvoid *mem = malloc_uncached(size);
    if (mem == NULL) {
        return false;
    }

    storage->data = mem;
    storage->size = size;

    return true;
}

void gl_storage_free(gl_storage_t *storage)
{
    // TODO: need to wait until buffer is no longer used!

    if (storage->data != NULL) {
        free(storage->data);
        storage->data = NULL;
    }
}

bool gl_storage_resize(gl_storage_t *storage, uint32_t new_size)
{
    if (storage->size >= new_size) {
        return true;
    }

    GLvoid *mem = malloc(new_size);
    if (mem == NULL) {
        return false;
    }

    gl_storage_free(storage);

    storage->data = mem;
    storage->size = new_size;

    return true;
}

bool are_vertex_layouts_equal(const mg_vertex_layout_t *p0, const mg_vertex_layout_t *p1)
{
    if (p0->stride != p1->stride) return false;
    if (p0->attribute_count != p1->attribute_count) return false;

    for (size_t i = 0; i < p0->attribute_count; i++)
    {
        const mg_vertex_attribute_t *a0 = &p0->attributes[i];
        const mg_vertex_attribute_t *a1 = &p1->attributes[i];

        // TODO: handle differently ordered attributes
        if (a0->input != a1->input) return false;
        if (a0->offset != a1->offset) return false;
    }
    
    return true;
}

uint32_t pipeline_get_or_create(const mg_vertex_layout_t *submesh_layout, mgfx_features_t features)
{
    // Try to find a pipeline with the same vertex layout and feature set
    for (uint32_t i = 0; i < state->pipelines_count; i++)
    {
        if (features != state->pipelines[i].features) {
            continue;
        }

        if (are_vertex_layouts_equal(submesh_layout, &state->pipelines[i].layout.vertex_layout)) {
            return i;
        }
    }

    // If none was found, create a new pipeline with the vertex layout.
    // Internally, magma will patch the shader ucode to be compatible with the configured vertex layout,
    // which is why a separate pipeline needs to be created for each layout.
    pipeline_data *new_pipeline = &state->pipelines[state->pipelines_count];
    new_pipeline->pipeline = mg_pipeline_create(&(mg_pipeline_parms_t) {
        .vertex_shader_ucode = mgfx_get_shader_ucode(features),
        .vertex_layout = *submesh_layout
    });
    new_pipeline->features = features;

    // Store the vertex layout in the cache
    vertex_layout *new_layout = &new_pipeline->layout;
    memcpy(new_layout->attributes, submesh_layout->attributes, sizeof(mg_vertex_attribute_t) * submesh_layout->attribute_count);
    memcpy(&new_layout->vertex_layout, submesh_layout, sizeof(mg_vertex_layout_t));
    new_layout->vertex_layout.attributes = new_layout->attributes;

    return state->pipelines_count++;
}
