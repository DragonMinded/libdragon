#include <libdragon.h>
#include <GL/gl.h>
#include <GL/gl_integration.h>
#include <malloc.h>
#include <math.h>

#include "cube.h"

static uint32_t animation = 3283;
static uint32_t texture_index = 0;
static bool near = false;

static GLuint buffers[2];
static GLuint textures[4];

static const char *texture_path[4] = {
    "rom:/circle0.sprite",
    "rom:/diamond0.sprite",
    "rom:/pentagon0.sprite",
    "rom:/triangle0.sprite",
};

static sprite_t *sprites[4];

void load_texture(GLenum target, sprite_t *sprite)
{
    for (uint32_t i = 0; i < 4; i++)
    {
        surface_t surf = sprite_get_lod_pixels(sprite, i);
        if (!surf.buffer) break;

        data_cache_hit_writeback(surf.buffer, surf.stride * surf.height);

        glTexImageN64(GL_TEXTURE_2D, i, &surf);
    }
}

void setup()
{
    for (uint32_t i = 0; i < 4; i++)
    {
        sprites[i] = sprite_load(texture_path[i]);
    }

    glGenBuffersARB(2, buffers);

    glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers[0]);
    glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW_ARB);

    glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffers[1]);
    glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW_ARB);

    glEnable(GL_LIGHT0);
    //glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    //glEnable(GL_MULTISAMPLE_ARB);

    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float aspect_ratio = (float)display_get_width() / (float)display_get_height();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1*aspect_ratio, 1*aspect_ratio, -1, 1, 1, 20);
    //glOrtho(-2*aspect_ratio, 2*aspect_ratio, -2, 2, 5, -5);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    GLfloat light_pos[] = { 0, 0, 0, 1 };
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

    GLfloat light_diffuse[] = { 1, 1, 1, 1 };
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 0.0f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 1.0f/10.0f);

    GLfloat mat_diffuse[] = { 1, 1, 1, 0.6f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);

    //glEnable(GL_FOG);

    GLfloat fog_color[] = { 1, 0, 0, 1 };

    glFogfv(GL_FOG_COLOR, fog_color);
    glFogf(GL_FOG_START, 1.0f);
    glFogf(GL_FOG_END, 6.0f);

    glGenTextures(4, textures);

    #if 0
    GLenum min_filter = GL_LINEAR_MIPMAP_LINEAR;
    #else
    GLenum min_filter = GL_LINEAR;
    #endif


    for (uint32_t i = 0; i < 4; i++)
    {
        glBindTexture(GL_TEXTURE_2D, textures[i]);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);

        load_texture(GL_TEXTURE_2D, sprites[i]);
    }
}

void draw_cube()
{
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glVertexPointer(3, GL_FLOAT, sizeof(vertex_t), (void*)(0*sizeof(float)));
    glTexCoordPointer(2, GL_FLOAT, sizeof(vertex_t), (void*)(3*sizeof(float)));
    glNormalPointer(GL_FLOAT, sizeof(vertex_t), (void*)(5*sizeof(float)));
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(vertex_t), (void*)(8*sizeof(float)));

    glDrawElements(GL_TRIANGLES, sizeof(cube_indices) / sizeof(uint16_t), GL_UNSIGNED_SHORT, 0);
}

void draw_band()
{
    glBegin(GL_QUAD_STRIP);

    const uint32_t segments = 16;

    for (uint32_t i = 0; i <= segments; i++)
    {
        float angle = (2*M_PI / segments) * (i % segments);

        float x = cosf(angle) * 2;
        float z = sinf(angle) * 2;

        glVertex3f(x, -0.2f, z);
        glVertex3f(x, 0.2f, z);
    }

    glEnd();
}

void draw_circle()
{
    glBegin(GL_POINTS);

    const uint32_t segments = 16;

    for (uint32_t i = 0; i < segments; i++)
    {
        float angle = (2*M_PI / segments) * (i % segments);

        float x = cosf(angle);
        float z = sinf(angle);

        glVertex3f(x, 1.5f, z);
        glVertex3f(x, 1.5f, z);
    }

    glEnd();
}

void draw_quads()
{
    glBegin(GL_QUADS);

    glVertex3f(0, 0, 0);
    glVertex3f(1, 0, 0);
    glVertex3f(1, 1, 0);
    glVertex3f(0, 1, 0);

    glVertex3f(0, 0, 1);
    glVertex3f(1, 0, 1);
    glVertex3f(1, 1, 1);
    glVertex3f(0, 1, 1);

    glVertex3f(0, 0, 2);
    glVertex3f(1, 0, 2);
    glVertex3f(1, 1, 2);
    glVertex3f(0, 1, 2);

    glEnd();
}

void render()
{
    glClearColor(0.3f, 0.1f, 0.6f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float rotation = animation * 0.5f;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, sinf(rotation*0.02f), (near ? -2.2f : -3.5f) - 5 - cosf(rotation*0.01f)*5);

    glPushMatrix();

    glRotatef(rotation*0.46f, 0, 1, 0);
    glRotatef(rotation*1.35f, 1, 0, 0);
    glRotatef(rotation*1.81f, 0, 0, 1);

    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);

    glColor3f(1.f, 1.f, 1.f);
    draw_band();
    glColor3f(0.f, 1.f, 1.f);
    draw_circle();

    draw_quads();

    glPopMatrix();

    glPushMatrix();

    glRotatef(rotation*0.23f, 1, 0, 0);
    glRotatef(rotation*0.98f, 0, 0, 1);
    glRotatef(rotation*1.71f, 0, 1, 0);

    glEnable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);

    glBindTexture(GL_TEXTURE_2D, textures[texture_index]);

    draw_cube();

    glPopMatrix();
}

int main()
{
	debug_init_isviewer();
	debug_init_usblog();
    
    dfs_init(DFS_DEFAULT_LOCATION);

    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 1, GAMMA_NONE, ANTIALIAS_RESAMPLE_FETCH_ALWAYS);

    gl_init();

    setup();

    controller_init();

    while (1)
    {
        controller_scan();
        struct controller_data pressed = get_keys_pressed();
        struct controller_data down = get_keys_down();

        if (pressed.c[0].A) {
            animation++;
        }

        if (pressed.c[0].B) {
            animation--;
        }

        if (down.c[0].start) {
            debugf("%ld\n", animation);
        }

        if (down.c[0].C_down) {
            near = !near;
        }

        if (down.c[0].C_right) {
            texture_index = (texture_index + 1) % 4;
        }

        render();

        gl_swap_buffers();
    }
}
