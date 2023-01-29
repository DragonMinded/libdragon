#include <surface.h>
#include <GL/gl.h>
#include <GL/gl_integration.h>
#include <rdpq_debug.h>

#define GL_INIT_SIZE(w,h) \
    RDPQ_INIT(); \
    surface_t test_surf = surface_alloc(FMT_RGBA16, w, h); \
    DEFER(surface_free(&test_surf)); \
    gl_init(); \
    DEFER(gl_close()); \
    rdpq_attach(&test_surf); \
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