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
    hashtable_init(&state->pipeline_cache, MAX_PIPELINE_COUNT, NULL);

    gl_rendermode_init();
    gl_array_init();
    gl_primitive_init();
    gl_matrix_init();
    gl_lighting_init();
    gl_texture_init();
    gl_list_init();

    glClearColor(0, 0, 0, 0);
    glClearDepth(1);
}

static void free_pipeline_visitor(uint32_t key, void *value, int refcount)
{
    mg_pipeline_free((mg_pipeline_t*)value);
}

void gl_close(void)
{
    // First wait for all pending commands to finish
    rspq_wait();

    gl_list_close();
    gl_texture_close();
    gl_lighting_close();
    gl_primitive_close();
    gl_array_close();
    gl_rendermode_close();

    // Wait again for any rspq calls that may have been made during cleanup
    rspq_wait();

    hashtable_visit(&state->pipeline_cache, free_pipeline_visitor);
    hashtable_free(&state->pipeline_cache);

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
        state->rdpq_texture = value;
        gl_set_texturing_dirty();
        gl_set_combiner_dirty();
        gl_set_geom_flags_dirty();
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
        gl_set_combiner_dirty();
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
        gl_set_tex_gen_enabled(target, value);
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

mgfx_features_t get_pipeline_features()
{
    return gl_is_env_map_enabled() ? MGFX_FEATURE_ENV_MAP : 0;
}

const vertex_layout *get_current_layout()
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

static uint32_t get_pipeline_key(const mg_vertex_layout_t *layout, mgfx_features_t features)
{
    // Get pipeline key by creating a hash from all pipeline parameters using FNV-1a hash
    uint32_t key = 0x811c9dc5; // FNV offset basis
    for (size_t i = 0; i < layout->attribute_count; i++)
    {
        fnv1a(&key, layout->attributes[i].input);
        fnv1a(&key, layout->attributes[i].offset);
    }
    fnv1a(&key, layout->stride);
    fnv1a(&key, features);
    return key;
}

void gl_set_pipeline_dirty()
{
    state->is_pipeline_dirty = true;    
}

void update_pipeline()
{
    if (!state->is_pipeline_dirty) return;
    state->is_pipeline_dirty = false;

    const vertex_layout *layout = get_current_layout();
    mgfx_features_t features = get_pipeline_features();

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

    uint32_t new_key = get_pipeline_key(&layout->vertex_layout, features);
    if (new_key == state->current_pipeline_key) return;

    mg_pipeline_t *pipeline = hashtable_lookup(&state->pipeline_cache, new_key);
    if (pipeline == NULL) {
        pipeline = mg_pipeline_create(&(mg_pipeline_parms_t) {
            .vertex_shader_ucode = mgfx_get_shader_ucode(features),
            .vertex_layout = layout->vertex_layout
        });
        hashtable_insert(&state->pipeline_cache, new_key, pipeline);
    }

    state->current_pipeline_key = new_key;
    mg_pipeline_bind(pipeline);

    state->fog_uniform = mg_pipeline_get_uniform(pipeline, MGFX_BINDING_FOG);
    state->lighting_uniform = mg_pipeline_get_uniform(pipeline, MGFX_BINDING_LIGHTING);
    state->texturing_uniform = mg_pipeline_get_uniform(pipeline, MGFX_BINDING_TEXTURING);
    state->matrices_uniform = mg_pipeline_get_uniform(pipeline, MGFX_BINDING_MATRICES);
}
