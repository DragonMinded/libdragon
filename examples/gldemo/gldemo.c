#include <libdragon.h>
#include <GL/gl.h>
#include <GL/gl_integration.h>
#include <malloc.h>

static sprite_t *circle_sprite;

static float rotation = 0.0f;
static float aspect_ratio;

void render()
{
    glClearColor(0.4f, 0.1f, 0.5f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-3*aspect_ratio, 3*aspect_ratio, -3, 3, -3, 3);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(0.3f, 1, 0, 0);
    glRotatef(rotation, 0, 1, 0);

    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, circle_sprite->width, circle_sprite->height, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1_EXT, circle_sprite->data);

    glBegin(GL_TRIANGLE_STRIP);

    glColor3f(1.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(1.f, -1.f, -1.f);

    glColor3f(1.0f, 1.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.f, 1.f, -1.f);

    glColor3f(1.0f, 0.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(1.f, -1.f, 1.f);

    glColor3f(1.0f, 1.0f, 1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.f, 1.f, 1.f);

    glColor3f(0.0f, 0.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.f, -1.f, 1.f);

    glColor3f(0.0f, 1.0f, 1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(-1.f, 1.f, 1.f);

    glColor3f(0.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.f, -1.f, -1.f);

    glColor3f(0.0f, 1.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(-1.f, 1.f, -1.f);

    glColor3f(1.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(1.f, -1.f, -1.f);

    glColor3f(1.0f, 1.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.f, 1.f, -1.f);

    glEnd();

    glBegin(GL_TRIANGLE_STRIP);

    glColor3f(0.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.f, -1.f, -1.f);

    glColor3f(1.0f, 0.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.f, -1.f, -1.f);

    glColor3f(0.0f, 0.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.f, -1.f, 1.f);

    glColor3f(1.0f, 0.0f, 1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.f, -1.f, 1.f);

    glEnd();

    glBegin(GL_TRIANGLE_STRIP);

    glColor3f(0.0f, 1.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.f, 1.f, -1.f);

    glColor3f(0.0f, 1.0f, 1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(-1.f, 1.f, 1.f);

    glColor3f(1.0f, 1.0f, 0.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(1.f, 1.f, -1.f);

    glColor3f(1.0f, 1.0f, 1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.f, 1.f, 1.f);

    glEnd();
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

    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 1, GAMMA_NONE, ANTIALIAS_RESAMPLE);

    gl_init();

    aspect_ratio = (float)display_get_width() / (float)display_get_height();

    while (1)
    {
        rotation += 0.1f;

        render();

        gl_swap_buffers();
    }
}
