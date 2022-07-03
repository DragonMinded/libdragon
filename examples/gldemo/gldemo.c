#include <libdragon.h>
#include <GL/gl.h>
#include <GL/gl_integration.h>
#include <malloc.h>
#include <math.h>

static sprite_t *circle_sprite;

static uint32_t animation = 3283;
static bool near = false;

void setup()
{
    glEnable(GL_DITHER);
    glEnable(GL_LIGHT0);
    //glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);

    float aspect_ratio = (float)display_get_width() / (float)display_get_height();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1*aspect_ratio, 1*aspect_ratio, -1, 1, 1, 10);
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

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, circle_sprite->width, circle_sprite->height, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1_EXT, circle_sprite->data);
}

void draw_test()
{
    glBegin(GL_TRIANGLES);

    glColor3f(0, 1, 1);

    glVertex3f(1.f, -1.f, -1.f);
    glVertex3f(1.f, -1.f, 1.f);
    glVertex3f(1.f, 1.f, 1.f);

    glVertex3f(1.f, -1.f, -1.f);
    glVertex3f(1.f, 1.f, 1.f);
    glVertex3f(1.f, 1.f, -1.f);

    glEnd();
}

void draw_cube()
{
    glBegin(GL_QUADS);

    // +X

    glNormal3f(1.0f, 0.0f, 0.0f);
    glColor3f(1, 0, 0);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(1.f, -1.f, -1.f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.f, 1.f, -1.f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.f, 1.f, 1.f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(1.f, -1.f, 1.f);

    // -X

    glNormal3f(-1.0f, 0.0f, 0.0f);
    glColor3f(0, 1, 1);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.f, -1.f, -1.f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.f, -1.f, 1.f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(-1.f, 1.f, 1.f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(-1.f, 1.f, -1.f);

    // +Y

    glNormal3f(0.0f, 1.0f, 0.0f);
    glColor3f(0, 1, 0);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.f, 1.f, -1.f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.f, 1.f, 1.f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.f, 1.f, 1.f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.f, 1.f, -1.f);

    // -Y

    glNormal3f(0.0f, -1.0f, 0.0f);
    glColor3f(1, 0, 1);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.f, -1.f, -1.f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.f, -1.f, -1.f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.f, -1.f, 1.f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.f, -1.f, 1.f);

    // +Z

    glNormal3f(0.0f, 0.0f, 1.0f);
    glColor3f(0, 0, 1);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.f, -1.f, 1.f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.f, -1.f, 1.f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.f, 1.f, 1.f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.f, 1.f, 1.f);

    // -Z

    glNormal3f(0.0f, 0.0f, -1.0f);
    glColor3f(1, 1, 0);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.f, -1.f, -1.f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.f, 1.f, -1.f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.f, 1.f, -1.f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.f, -1.f, -1.f);

    glEnd();
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
    glBegin(GL_POLYGON);

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

void render()
{
    glClearColor(0.3f, 0.1f, 0.6f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float rotation = animation * 0.01f;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, sinf(rotation*0.8f), near ? -2.2f : -3.5f);

    glPushMatrix();

    glRotatef(rotation*0.46f, 0, 1, 0);
    glRotatef(rotation*1.35f, 1, 0, 0);
    glRotatef(rotation*1.81f, 0, 0, 1);

    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);

    glColor3f(1.f, 1.f, 1.f);
    draw_band();

    glPopMatrix();

    glPushMatrix();

    glRotatef(rotation*0.23f, 1, 0, 0);
    glRotatef(rotation*0.98f, 0, 0, 1);
    glRotatef(rotation*1.71f, 0, 1, 0);

    glEnable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);

    draw_cube();

    glPopMatrix();
}

int main()
{
	debug_init_isviewer();
	debug_init_usblog();
    
    dfs_init(DFS_DEFAULT_LOCATION);

    int fp = dfs_open("circle.sprite");
    circle_sprite = malloc(dfs_size(fp));
    dfs_read(circle_sprite, 1, dfs_size(fp), fp);
    dfs_close(fp);

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

        render();
        gl_swap_buffers();
    }
}
