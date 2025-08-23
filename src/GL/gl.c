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

static gl_dirty_flags_t state_to_dirty_flag_table[STATE_COUNT];
static gl_dirty_flags_t enable_to_dirty_flag_table[ENABLE_COUNT];
static gl_dirty_flags_t hint_to_dirty_flags_table[HINT_COUNT];

static void init_update_funcs()
{
    #define ADD_DIRTY_FLAG(i) t[i] |= f;

    #define ADD_DIRTY_FLAGS2(...) \
        __CALL_FOREACH(ADD_DIRTY_FLAG, ##__VA_ARGS__) \

    #define ADD_DIRTY_FLAGS(dirty_flag, table, list) { \
        gl_dirty_flags_t f = dirty_flag; \
        gl_dirty_flags_t *t = table; \
        (void)f; \
        (void)t; \
        ADD_DIRTY_FLAGS2 list \
    }

    #define STATE(func, dirty_flag, state_list, enable_list, hint_list) ({ \
        ADD_DIRTY_FLAGS(dirty_flag, state_to_dirty_flag_table, state_list) \
        ADD_DIRTY_FLAGS(dirty_flag, enable_to_dirty_flag_table, enable_list) \
        ADD_DIRTY_FLAGS(dirty_flag, hint_to_dirty_flags_table, hint_list) \
        func; \
    })

    /*
        List of update functions.
        Every update function represents some state that GL needs to keep up to date before every draw call:
            1. Computed states: These are purely for caching purposes and are then used by other states (e.g. active texture, z planes)
            2. Magma: Each one corresponds to a call to magma, like mg_set_culling or uniforms
            3. RDPQ:  Each one corresponds to a call to rdpq, like rdpq_set_scissor or texture upload
        
        Each update function is associated with a unique dirty flag. If this flag is set, 
        that means the state has been invalidated and needs to be updated before the next draw call.
        In turn this means that a state needs to be invalidated whenever any of the inputs to the
        update function changes, which means the corresponding dirty flag needs to be set.

        There are three different kinds of input:
            1. State
            2. Enable flag
            3. Hint flag
        For each kind of input, there is a corresponding table that maps the id of the input to the dirty flags that
        need to be set whenever it changes, to correctly invalidate all of the states that use it.
        These three tables are also being filled below with some macro tricks.
        For each state, the used inputs are listed in a declarative manner to make modifications and maintenance much easier.
    */
    gl_update_func_t funcs[] = {
        //    Update function           Dirty flag              Used states, enables and hints
        //------------------------------------------------------------------------------------------------------------------------------------
        STATE(update_active_texture,    DIRTY_ACTIVE_TEXTURE,   (STATE_BOUND_TEXTURES, STATE_TEXTURE_COMPLETE), (ENABLE_TEXTURE_1D, ENABLE_TEXTURE_2D), ()),
        STATE(update_z_planes,          DIRTY_Z_PLANES,         (STATE_MAT_PROJECTION), (), ()),
        STATE(update_pipeline,          DIRTY_PIPELINE,         (STATE_COLOR_MATERIAL, STATE_TEX_GEN, STATE_BOUND_VAO), (ENABLE_COLOR_MATERIAL, ENABLE_TEX_GEN_S, ENABLE_TEX_GEN_T, ENABLE_TEX_GEN_R, ENABLE_TEX_GEN_Q), ()),
        STATE(gl_upload_fog,            DIRTY_FOG_UNIFORM,      (STATE_FOG_RANGE), (ENABLE_FOG), ()),
        STATE(gl_upload_lighting,       DIRTY_LIGHTING,         (STATE_LIGHT), (ENABLE_LIGHTING, ENABLE_LIGHT0, ENABLE_LIGHT1, ENABLE_LIGHT2, ENABLE_LIGHT3, ENABLE_LIGHT4, ENABLE_LIGHT5, ENABLE_LIGHT6, ENABLE_LIGHT7), ()),
        STATE(gl_upload_texturing,      DIRTY_TEXTURING,        (STATE_MAT_TEXTURE, STATE_ACTIVE_TEXTURE, STATE_TEXTURE_SIZE, STATE_RDPQ_TEX_SIZE, STATE_TEXTURE_FILTER), (ENABLE_RDPQ_TEXTURING, ENABLE_TEX_FLIP), ()),
        STATE(gl_upload_matrices,       DIRTY_MATRICES,         (STATE_MAT_PROJECTION, STATE_MAT_MODELVIEW), (), ()),
        STATE(update_culling,           DIRTY_CULLING,          (STATE_CULL_FACE, STATE_FRONT_FACE), (ENABLE_CULL_FACE), ()),
        STATE(update_viewport,          DIRTY_VIEWPORT,         (STATE_VIEWPORT, STATE_DEPTH_RANGE, STATE_Z_PLANES), (), ()),
        STATE(update_geom_flags,        DIRTY_GEOM_FLAGS,       (STATE_DEPTH_FUNC, STATE_DEPTH_MASK, STATE_TEX_ENV_MODE, STATE_BEGIN_END, STATE_ACTIVE_TEXTURE, STATE_ARRAY_COLOR), (ENABLE_DEPTH_TEST, ENABLE_FOG, ENABLE_LIGHTING, ENABLE_RDPQ_TEXTURING), ()),
        STATE(apply_prim_color,         DIRTY_PRIM_COLOR,       (STATE_COLOR_MATERIAL, STATE_MATERIAL_DIFFUSE, STATE_COLOR), (ENABLE_LIGHTING, ENABLE_COLOR_MATERIAL, ENABLE_RDPQ_MATERIAL), ()),
        STATE(apply_fog_color,          DIRTY_FOG_COLOR,        (STATE_FOG_COLOR), (), ()),
        STATE(apply_scissor,            DIRTY_SCISSOR,          (STATE_SCISSOR), (ENABLE_SCISSOR_TEST), ()),
        STATE(apply_texture,            DIRTY_TEXTURE_UPLOAD,   (STATE_ACTIVE_TEXTURE, STATE_TEXTURE_BLOCK), (ENABLE_RDPQ_TEXTURING), ()),
        STATE(apply_antialias,          DIRTY_ANTIALIAS,        (), (ENABLE_MULTISAMPLE), (HINT_FULL_AA)),
        STATE(apply_dither,             DIRTY_DITHER,           (STATE_DITHER_MODE), (ENABLE_DITHER), ()),
        STATE(apply_combiner,           DIRTY_COMBINER,         (STATE_TEX_ENV_MODE, STATE_COLOR_MATERIAL, STATE_BEGIN_END, STATE_ACTIVE_TEXTURE), (ENABLE_LIGHTING, ENABLE_COLOR_MATERIAL, ENABLE_RDPQ_TEXTURING, ENABLE_RDPQ_MATERIAL), ()),
        STATE(apply_blender,            DIRTY_BLENDER,          (STATE_BLEND_FUNC), (ENABLE_BLEND, ENABLE_RDPQ_MATERIAL), ()),
        STATE(apply_fog,                DIRTY_FOG,              (), (ENABLE_FOG), ()),
        STATE(apply_alphacompare,       DIRTY_ALPHACOMPARE,     (STATE_ALPHA_FUNC), (ENABLE_ALPHA_TEST), ()),
        STATE(apply_zbuf,               DIRTY_ZBUF,             (STATE_DEPTH_FUNC, STATE_DEPTH_MASK), (ENABLE_DEPTH_TEST), ()),
        STATE(apply_zmode,              DIRTY_ZMODE,            (STATE_DEPTH_FUNC), (ENABLE_DEPTH_TEST), ()),
        STATE(apply_persp,              DIRTY_PERSP,            (), (), (HINT_PERSP_CORRECT)),
        STATE(apply_filter,             DIRTY_FILTER,           (STATE_ACTIVE_TEXTURE, STATE_TEXTURE_FILTER), (ENABLE_RDPQ_TEXTURING), ()),
    };

    state->update_func_count = sizeof(funcs) / sizeof(gl_update_func_t);
    state->update_funcs = malloc(sizeof(funcs));
    memcpy(state->update_funcs, funcs, sizeof(funcs));
}

void gl_init(void)
{
    mg_init();
    rdpq_init();

    state = calloc(1, sizeof(gl_state_t));
    init_update_funcs();

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

    free(state->update_funcs);
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

    // Reset all RDPQ related states in case they were modified outside of the GL context
    gl_set_dirty_flags(DIRTY_RDPQ);

    // Reset rendermode in case it was changed outside of the GL context
    rdpq_set_mode_standard();
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

void gl_set_state(gl_state_id_t id)
{
    gl_set_dirty_flags(state_to_dirty_flag_table[id]);
}


gl_enable_t get_enable_from_target(GLenum target)
{
    switch (target) {
    case GL_RDPQ_MATERIAL_N64:
        return ENABLE_RDPQ_MATERIAL;
    case GL_RDPQ_TEXTURING_N64:
        return ENABLE_RDPQ_TEXTURING;
    case GL_SCISSOR_TEST:
        return ENABLE_SCISSOR_TEST;
    case GL_DEPTH_TEST:
        return ENABLE_DEPTH_TEST;
    case GL_BLEND:
        return ENABLE_BLEND;
    case GL_ALPHA_TEST:
        return ENABLE_ALPHA_TEST;
    case GL_DITHER:
        return ENABLE_DITHER;
    case GL_FOG:
        return ENABLE_FOG;
    case GL_MULTISAMPLE_ARB:
        return ENABLE_MULTISAMPLE;
    case GL_TEXTURE_1D:
        return ENABLE_TEXTURE_1D;
    case GL_TEXTURE_2D:
        return ENABLE_TEXTURE_2D;
    case GL_CULL_FACE:
        return ENABLE_CULL_FACE;
    case GL_LIGHTING:
        return ENABLE_LIGHTING;
    case GL_LIGHT0:
        return ENABLE_LIGHT0;
    case GL_LIGHT1:
        return ENABLE_LIGHT1;
    case GL_LIGHT2:
        return ENABLE_LIGHT2;
    case GL_LIGHT3:
        return ENABLE_LIGHT3;
    case GL_LIGHT4:
        return ENABLE_LIGHT4;
    case GL_LIGHT5:
        return ENABLE_LIGHT5;
    case GL_LIGHT6:
        return ENABLE_LIGHT6;
    case GL_LIGHT7:
        return ENABLE_LIGHT7;
    case GL_COLOR_MATERIAL:
        return ENABLE_COLOR_MATERIAL;
    case GL_TEXTURE_GEN_S:
        return ENABLE_TEX_GEN_S;
    case GL_TEXTURE_GEN_T:
        return ENABLE_TEX_GEN_T;
    case GL_TEXTURE_GEN_R:
        return ENABLE_TEX_GEN_R;
    case GL_TEXTURE_GEN_Q:
        return ENABLE_TEX_GEN_Q;
    case GL_NORMALIZE:
        return ENABLE_NORMALIZE;
    case GL_MATRIX_PALETTE_ARB:
        return ENABLE_MATRIX_PALETTE;
    case GL_TEXTURE_FLIP_T_N64:
        return ENABLE_TEX_FLIP;
    case GL_CLIP_PLANE0:
    case GL_CLIP_PLANE1:
    case GL_CLIP_PLANE2:
    case GL_CLIP_PLANE3:
    case GL_CLIP_PLANE4:
    case GL_CLIP_PLANE5:
        assertf(0, "User clip planes are not supported!");
        break;
    case GL_STENCIL_TEST:
        assertf(0, "Stencil test is not supported!");
        break;
    case GL_COLOR_LOGIC_OP:
    case GL_INDEX_LOGIC_OP:
        assertf(0, "Logical pixel operation is not supported!");
        break;
    case GL_POINT_SMOOTH:
    case GL_LINE_SMOOTH:
    case GL_POLYGON_SMOOTH:
        assertf(0, "Smooth rendering is not supported (Use multisampling instead)!");
        break;
    case GL_LINE_STIPPLE:
    case GL_POLYGON_STIPPLE:
        assertf(0, "Stipple is not supported!");
        break;
    case GL_POLYGON_OFFSET_FILL:
    case GL_POLYGON_OFFSET_LINE:
    case GL_POLYGON_OFFSET_POINT:
        assertf(0, "Polygon offset is not supported!");
        break;
    case GL_SAMPLE_ALPHA_TO_COVERAGE_ARB:
    case GL_SAMPLE_ALPHA_TO_ONE_ARB:
    case GL_SAMPLE_COVERAGE_ARB:
        assertf(0, "Coverage value manipulation is not supported!");
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
        assertf(0, "Evaluators are not supported!");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid enable target", target);
        break;
    }

    return -1;
}

void glEnable(GLenum target)
{
    if (!gl_ensure_no_begin_end()) return;
    gl_enable_t enable = get_enable_from_target(target);
    if (enable < 0) return;
    state->enable_flags |= (1 << enable);
    gl_set_dirty_flags(enable_to_dirty_flag_table[enable]);
}

void glDisable(GLenum target)
{
    if (!gl_ensure_no_begin_end()) return;
    gl_enable_t enable = get_enable_from_target(target);
    if (enable < 0) return;
    state->enable_flags &= ~(1 << enable);
    gl_set_dirty_flags(enable_to_dirty_flag_table[enable]);
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

    if (gl_check_and_clear_dirty_flags(DIRTY_SCISSOR)) {
        apply_scissor();
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

void set_hint_flag(gl_hint_t hint, bool value)
{
    uint32_t flag = 1 << hint;
    if (value) {
        state->hint_flags |= flag;
    } else {
        state->hint_flags &= ~flag;
    }

    gl_set_dirty_flags(hint_to_dirty_flags_table[hint]);
}

void glHint(GLenum target, GLenum hint)
{
    if (!gl_ensure_no_begin_end()) return;

    switch (target)
    {
    case GL_PERSPECTIVE_CORRECTION_HINT:
        // Use perspective correction by default, unless it was explicitly turned off
        set_hint_flag(HINT_PERSP_CORRECT, hint != GL_FASTEST);
        break;
    case GL_MULTISAMPLE_HINT_N64:
        // Use full AA by default, unless RA has been requested
        set_hint_flag(HINT_FULL_AA, hint != GL_FASTEST);
        break;
    case GL_FOG_HINT:
        // TODO: per-pixel fog
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

void update_pipeline()
{
    const vertex_layout *layout = get_current_layout();
    mgfx_features_t features = get_pipeline_features();

    vertex_layout vl;
    if (gl_is_enabled(ENABLE_LIGHTING) && !gl_is_diffuse_tracking_color())
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
