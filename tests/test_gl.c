#include <surface.h>
#include <GL/gl.h>
#include <GL/gl_integration.h>
#include <rdpq_debug.h>
#include <magma.h>

#undef ABS
#include "../src/GL/gl_internal.h"

#define GL_INIT_SIZE(w,h) \
    RDPQ_INIT(); \
    surface_t test_surf = surface_alloc(FMT_RGBA16, w, h); \
    DEFER(surface_free(&test_surf)); \
    surface_t test_z = surface_alloc(FMT_RGBA16, w, h); \
    DEFER(surface_free(&test_z)); \
    mg_init(); \
    DEFER(mg_close()); \
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
        GLuint handle = 0;
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

void test_gl_list(TestContext *ctx)
{
    GL_INIT();

    ASSERT(!glIsList(1), "List index should not be used before glGenLists");
    GLuint tri_dlist = glGenLists(1);

    ASSERT_EQUAL_UNSIGNED(tri_dlist, 1, "First display list index is expected to be 1 in this test");
    ASSERT(glIsList(tri_dlist), "List index should be used after glGenLists");

    glCallList(tri_dlist); // no-op according to spec, must not crash
    ASSERT(glIsList(tri_dlist), "List index should still be used after glCallList");

    glNewList(tri_dlist, GL_COMPILE);
    ASSERT(glIsList(tri_dlist), "List index should still be used after glNewList");

    glBegin(GL_TRIANGLES);
    glNormal3f(1, 1, 0);
    glVertex3f(5, 5, 0);

    glColor3f(0, 1, 0);
    glNormal3f(-1, 1, 0);
    glVertex3f(-5, 5, 0);

    glColor3f(0, 0, 1);
    glNormal3f(1, -1, 0);
    glVertex3f(5, -5, 0);
    glEnd();

    glEndList();

    ASSERT(glIsList(tri_dlist), "List index should still be used after glEndList");

    glCallList(100); // no-op according to spec, must not crash

    glNewList(100, GL_COMPILE);
    ASSERT(!glIsList(100), "List index should not be used after glNewList without allocating it first with glGenLists");

    glCallList(100); // no-op according to spec, must not crash

    glBegin(GL_TRIANGLES);
    glVertex3f(0, 0, 0);
    glVertex3f(1, 0, 0);
    glVertex3f(0, 1, 0);
    glEnd();

    glEndList();
    ASSERT(glIsList(100), "List index should be used after glEndList without allocating it first with glGenLists");
}

void test_gl_cull(TestContext *ctx)
{
    GL_INIT();
    debug_rdp_stream_init();

    void draw_tri(void) {
        glBegin(GL_TRIANGLES);
        glVertex3f(0, 0, 0);
        glVertex3f(1, 0, 0);
        glVertex3f(0, 1, 0);
        glEnd();
        rspq_wait();
    }

    debug_rdp_stream_reset();
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    draw_tri();
    uint32_t tri_count = debug_rdp_stream_count_cmd(RDPQ_CMD_TRI_SHADE + 0xC0);
    ASSERT_EQUAL_UNSIGNED(tri_count, 0, "Triangle should not be drawn when culling front faces");

    debug_rdp_stream_reset();
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    draw_tri();
    tri_count = debug_rdp_stream_count_cmd(RDPQ_CMD_TRI_SHADE + 0xC0);
    ASSERT_EQUAL_UNSIGNED(tri_count, 1, "Triangle should be drawn when culling back faces");

    debug_rdp_stream_reset();
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT_AND_BACK);
    draw_tri();
    tri_count = debug_rdp_stream_count_cmd(RDPQ_CMD_TRI_SHADE + 0xC0);
    ASSERT_EQUAL_UNSIGNED(tri_count, 0, "Triangle should not be drawn when culling front and back faces");

    debug_rdp_stream_reset();
    glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    draw_tri();
    glCullFace(GL_FRONT);
    draw_tri();
    glCullFace(GL_FRONT_AND_BACK);
    draw_tri();
    tri_count = debug_rdp_stream_count_cmd(RDPQ_CMD_TRI_SHADE + 0xC0);
    ASSERT_EQUAL_UNSIGNED(tri_count, 3, "Triangles should be drawn when culling disabled");
}

static void assert_drawn_pixel_color(color_t expected_color, surface_t *fb, TestContext *ctx)
{
    glBegin(GL_TRIANGLE_STRIP);
        glVertex2f(-1, -1);
        glVertex2f(1, -1);
        glVertex2f(1, 1);
        glVertex2f(-1, 1);
    glEnd();

    glFinish();

    color_t actual_color = color_from_packed16(surface_get_pixel(fb, 0, 0));

    // Error introduced by converting from packed 16 bit color to 32 bit
    uint8_t threshold = 0xff - ((0xff>>3)<<3);

    for (size_t i = 0; i < 4; i++)
    {
        uint8_t actual_comp = ((uint8_t*)&actual_color)[i];
        uint8_t expected_comp = ((uint8_t*)&expected_color)[i];

        if (abs(actual_comp - expected_comp) > threshold) {
            uint32_t actual32 = color_to_packed32(actual_color);
            uint32_t expected32 = color_to_packed32(expected_color);
            ASSERT_EQUAL_HEX(actual32, expected32, "Output pixel does not match expected color within tolerance");
        }
    }
}

#define ASSERT_DRAWN_COLOR(expected_color) assert_drawn_pixel_color((expected_color), &test_surf, ctx)

static const float gl_color_black[] = { 0, 0, 0, 0 };
static const float gl_color_white[] = { 1, 1, 1, 1 };

void test_gl_pixel_color(TestContext *ctx)
{
    GL_INIT();

    glColor4fv(gl_color_white);

    ASSERT_DRAWN_COLOR(color_from_packed32(0xFFFFFFFF));
}

void test_gl_pixel_light_model_ambient(TestContext *ctx)
{
    GL_INIT();

    float light_model_ambient[] = { 1, 0, 1, 0 };
    float material_ambient[] = { 1, 1, 0, 0 };

    glEnable(GL_LIGHTING);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, light_model_ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, material_ambient);

    ASSERT_DRAWN_COLOR(color_from_packed32(0xFF0000FF));
}

void test_gl_pixel_material_emission(TestContext *ctx)
{
    GL_INIT();

    float material_emission[] = { 1, 0, 1, 0 };

    glEnable(GL_LIGHTING);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, gl_color_black);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, gl_color_black);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, material_emission);

    ASSERT_DRAWN_COLOR(color_from_packed32(0xFF00FFFF));
}

void test_gl_pixel_light_ambient(TestContext *ctx)
{
    GL_INIT();

    float light_position[] = { 0, 0, 1, 0 };
    float light_ambient[] = { 1, 0, 1, 0 };
    float material_ambient[] = { 1, 1, 0, 0 };

    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, gl_color_black);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, gl_color_black);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, material_ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, gl_color_black);

    glNormal3f(0, 0, 1);

    ASSERT_DRAWN_COLOR(color_from_packed32(0xFF0000FF));
}

void test_gl_pixel_light_ambient_backside(TestContext *ctx)
{
    GL_INIT();

    float light_position[] = { 0, 0, 1, 0 };
    float light_ambient[] = { 1, 0, 1, 0 };
    float material_ambient[] = { 1, 1, 0, 0 };

    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, gl_color_black);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, gl_color_black);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, material_ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, gl_color_black);

    glNormal3f(0, 0, -1);

    ASSERT_DRAWN_COLOR(color_from_packed32(0xFF0000FF));
}

void test_gl_pixel_material_diffuse(TestContext *ctx)
{
    GL_INIT();

    float light_position[] = { 0, 0, 1, 0 };
    float light_diffuse[] = { 1, 0, 1, 0 };
    float material_diffuse[] = { 1, 1, 0, 0 };

    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, gl_color_black);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_AMBIENT, gl_color_black);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, gl_color_black);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, material_diffuse);

    glNormal3f(0, 0, 1);

    ASSERT_DRAWN_COLOR(color_from_packed32(0xFF0000FF));
}

void test_gl_pixel_material_diffuse_backside(TestContext *ctx)
{
    GL_INIT();

    float light_position[] = { 0, 0, 1, 0 };
    float light_diffuse[] = { 1, 0, 1, 0 };
    float material_diffuse[] = { 1, 1, 0, 0 };

    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, gl_color_black);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_AMBIENT, gl_color_black);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, gl_color_black);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, material_diffuse);

    glNormal3f(0, 0, -1);

    ASSERT_DRAWN_COLOR(color_from_packed32(0x000000FF));
}

void test_gl_pixel_material_diffuse_backside_clamp(TestContext *ctx)
{
    GL_INIT();

    float light_position[] = { 0, 0, 1, 0 };
    float light_diffuse[] = { .5f, .5f, .5f, 0 };
    float light_ambient[] = { 1, 1, 1, 0 };
    float material_diffuse[] = { 1, 1, 1, 0 };
    float material_ambient[] = { 1, 1, 1, 0 };

    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, gl_color_black);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, material_ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, material_diffuse);

    glNormal3f(0, 0, -1);

    ASSERT_DRAWN_COLOR(color_from_packed32(0xFFFFFFFF));
}

void test_gl_pixel_light_ambient_attenuation(TestContext *ctx)
{
    GL_INIT();

    float light_position[] = { -1, 1, 0, 1 };
    float light_ambient[] = { 1, 0, 1, 0 };
    float material_ambient[] = { 1, 1, 0, 0 };

    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, gl_color_black);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, gl_color_black);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 2);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, material_ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, gl_color_black);

    glNormal3f(0, 0, 1);

    ASSERT_DRAWN_COLOR(color_from_packed32(0x7F0000FF));
}

void test_gl_pixel_material_diffuse_attenuation(TestContext *ctx)
{
    GL_INIT();

    float light_position[] = { -1, 1, 1, 1 };
    float light_diffuse[] = { 1, 0, 1, 0 };
    float material_diffuse[] = { 1, 1, 0, 0 };

    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, gl_color_black);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_AMBIENT, gl_color_black);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 2);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, gl_color_black);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, material_diffuse);

    glNormal3f(0, 0, 1);

    ASSERT_DRAWN_COLOR(color_from_packed32(0x7F0000FF));
}

#undef ASSERT_DRAWN_COLOR
