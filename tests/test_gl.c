#include <surface.h>
#include <GL/gl.h>
#include <GL/gl_integration.h>
#include <rdpq_debug.h>

#undef ABS
#include "../src/GL/gl_internal.h"

#define GL_INIT_SIZE(w,h) \
    RDPQ_INIT(); \
    surface_t test_surf = surface_alloc(FMT_RGBA16, w, h); \
    DEFER(surface_free(&test_surf)); \
    surface_t test_z = surface_alloc(FMT_RGBA16, w, h); \
    DEFER(surface_free(&test_z)); \
    gl_init(); \
    DEFER(gl_close()); \
    rdpq_attach(&test_surf, &test_z); \
    DEFER(rdpq_detach_wait()); \
    gl_context_begin(); \
    DEFER(gl_context_end());

#define GL_INIT() GL_INIT_SIZE(64, 64)

void test_gl_clear(TestContext *ctx)
{
    uint32_t rect_count;

    GL_INIT();

    debug_rdp_stream_init();

    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();

    rect_count = debug_rdp_stream_count_cmd(RDPQ_CMD_FILL_RECTANGLE + 0xC0);
    ASSERT_EQUAL_UNSIGNED(rect_count, 1, "Wrong number of rectangles!");

    debug_rdp_stream_reset();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glFinish();

    rect_count = debug_rdp_stream_count_cmd(RDPQ_CMD_FILL_RECTANGLE + 0xC0);
    ASSERT_EQUAL_UNSIGNED(rect_count, 2, "Wrong number of rectangles!");
}

void test_gl_draw_arrays(TestContext *ctx)
{
    GL_INIT();

    debug_rdp_stream_init();

    static const GLfloat vertices[] = {
        0.0f, 0.0f,
        0.5f, 0.0f,
        0.5f, 0.5f
    };

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glFinish();

    uint32_t tri_count = debug_rdp_stream_count_cmd(RDPQ_CMD_TRI_SHADE + 0xC0);
    ASSERT_EQUAL_UNSIGNED(tri_count, 1, "Wrong number of triangles!");
}

void test_gl_draw_elements(TestContext *ctx)
{
    GL_INIT();

    debug_rdp_stream_init();

    static const GLfloat vertices[] = {
        0.0f, 0.0f,
        0.5f, 0.0f,
        0.5f, 0.5f
    };

    static const GLushort indices[] = {0, 1, 2};

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, indices);
    glFinish();

    uint32_t tri_count = debug_rdp_stream_count_cmd(RDPQ_CMD_TRI_SHADE + 0xC0);
    ASSERT_EQUAL_UNSIGNED(tri_count, 1, "Wrong number of triangles!");
}

void test_gl_texture_completeness(TestContext *ctx)
{
    GL_INIT();

    void run_test(int width, int height)
    {
        LOG("Testing %dx%d texture\n", width, height);
        surface_t tex = surface_alloc(FMT_RGBA16, width, height);
        DEFER(surface_free(&tex));

        glEnable(GL_TEXTURE_2D);
        GLuint handle;
        glGenTextures(1, &handle);
        DEFER(glDeleteTextures(1, &handle));

        glBindTexture(GL_TEXTURE_2D, handle);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glSurfaceTexImageN64(GL_TEXTURE_2D, 0, &tex, NULL);
        glFinish();
        gl_texture_object_t *texobj = gl_get_active_texture();
        ASSERT(texobj != NULL, "No active texture object!");

        // Check that the texture is not complete.
        glBindTexture(GL_TEXTURE_2D, 0);
        glFinish();
        ASSERT(!gl_tex_is_complete(texobj), "Texture should not be complete!");

        for (int i=1; i<MAX_TEXTURE_LEVELS; i++) {
            width /= 2;  if (!width) width = 1;
            height /= 2; if (!height) height = 1;
            surface_t mipmap = surface_make_sub(&tex, 0, 0, width, height);
            glBindTexture(GL_TEXTURE_2D, handle);
            glSurfaceTexImageN64(GL_TEXTURE_2D, i, &mipmap, NULL);

            // Check that the texture is not complete.
            glBindTexture(GL_TEXTURE_2D, 0);
            glFinish();
            if (width == 1 && height == 1) {
                ASSERT(gl_tex_is_complete(texobj), "Texture should be complete!");
                break;
            } else {
                ASSERT(!gl_tex_is_complete(texobj), "Texture should not be complete!");
            }
        }
    }

    // square, pow-2
    run_test(4, 4);
    if (ctx->result == TEST_FAILED) return;

    // rectangle, pow-2
    run_test(64, 4);
    if (ctx->result == TEST_FAILED) return;

    // square, non-pow-2
    run_test(24, 24);
    if (ctx->result == TEST_FAILED) return;

    // rectangle, non-pow-2
    run_test(57, 17);
    if (ctx->result == TEST_FAILED) return;
}
