#include <surface.h>
#include <GL/gl.h>
#include <GL/gl_integration.h>
#include <rdpq_debug.h>

static surface_t test_surf;

surface_t *open_test_surf()
{
    return &test_surf;
}

void close_test_surf(surface_t *surf)
{
}

#define GL_INIT_SIZE(w,h) \
    RDPQ_INIT(); \
    test_surf = surface_alloc(FMT_RGBA16, w, h); \
    DEFER(surface_free(&test_surf)); \
    gl_init_with_callbacks(open_test_surf, close_test_surf); \
    DEFER(gl_close());

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