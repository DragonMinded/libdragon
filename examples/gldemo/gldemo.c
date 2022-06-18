#include <libdragon.h>
#include <GL/gl.h>
#include <GL/gl_integration.h>
#include <malloc.h>

static sprite_t *circle_sprite;

static float rotation = 1.0f;
static float aspect_ratio;

void setup()
{
    aspect_ratio = (float)display_get_width() / (float)display_get_height();

    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DITHER);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1*aspect_ratio, 1*aspect_ratio, -1, 1, 1, 10);
    glTranslatef(0, 0, -3);
    //glOrtho(-2*aspect_ratio, 2*aspect_ratio, -2, 2, -5, 5);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    GLfloat light_pos[] = { 0, 0, 4, 1 };
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

    GLfloat spot_dir[] = { 0, 0, -2 };
    glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, spot_dir);

    GLfloat diffuse[] = { 1, 1, 1, 1 };
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 0.0f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 1.0f/6.0f);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, circle_sprite->width, circle_sprite->height, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1_EXT, circle_sprite->data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
}

void render()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(rotation, 0, 1, 0);
    glRotatef(rotation*1.35f, 1, 0, 0);
    glRotatef(rotation*0.62f, 0, 0, 1);

    glBegin(GL_TRIANGLE_STRIP);

    glNormal3f(1.0f, 0.0f, 0.0f);
    glColor3f(1, 0, 0);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(1.f, -1.f, -1.f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.f, 1.f, -1.f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(1.f, -1.f, 1.f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.f, 1.f, 1.f);

    glEnd();

    glBegin(GL_TRIANGLE_STRIP);

    glNormal3f(-1.0f, 0.0f, 0.0f);
    glColor3f(0, 1, 1);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.f, -1.f, -1.f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.f, -1.f, 1.f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(-1.f, 1.f, -1.f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(-1.f, 1.f, 1.f);

    glEnd();

    glBegin(GL_TRIANGLE_STRIP);

    glNormal3f(0.0f, 1.0f, 0.0f);
    glColor3f(0, 1, 0);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.f, 1.f, -1.f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.f, 1.f, 1.f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.f, 1.f, -1.f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.f, 1.f, 1.f);

    glEnd();

    glBegin(GL_TRIANGLE_STRIP);

    glNormal3f(0.0f, -1.0f, 0.0f);
    glColor3f(1, 0, 1);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.f, -1.f, -1.f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.f, -1.f, -1.f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.f, -1.f, 1.f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.f, -1.f, 1.f);

    glEnd();

    glBegin(GL_TRIANGLE_STRIP);

    glNormal3f(0.0f, 0.0f, 1.0f);
    glColor3f(0, 0, 1);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.f, -1.f, 1.f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.f, -1.f, 1.f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.f, 1.f, 1.f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.f, 1.f, 1.f);

    glEnd();

    glBegin(GL_TRIANGLE_STRIP);

    glNormal3f(0.0f, 0.0f, -1.0f);
    glColor3f(1, 1, 0);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.f, -1.f, -1.f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.f, 1.f, -1.f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.f, -1.f, -1.f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.f, 1.f, -1.f);

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

    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE_FETCH_ALWAYS);

    gl_init();

    setup();

    while (1)
    {
        rotation += 0.01f;

        render();

        gl_swap_buffers();
    }
}
