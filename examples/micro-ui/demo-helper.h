#ifndef DEMO_HELPER_H
#define DEMO_HELPER_H

void draw_cube()
{
  glEnable(GL_COLOR_MATERIAL);
  glEnable(GL_LIGHTING);
  glBegin(GL_QUADS);

  glColor3f(1.0f, 1.0f, 1.0f);
  glVertex3f(1.0f, 1.0f, -1.0f);
  glVertex3f(-1.0f, 1.0f, -1.0f);
  glVertex3f(-1.0f, 1.0f, 1.0f);
  glVertex3f(1.0f, 1.0f, 1.0f);

  glColor3f(1.0f, 0.5f, 0.0f);
  glVertex3f(1.0f, -1.0f, 1.0f);
  glVertex3f(-1.0f, -1.0f, 1.0f);
  glVertex3f(-1.0f, -1.0f, -1.0f);
  glVertex3f(1.0f, -1.0f, -1.0f);

  glColor3f(1.0f, 0.0f, 0.0f);
  glVertex3f(1.0f, 1.0f, 1.0f);
  glVertex3f(-1.0f, 1.0f, 1.0f);
  glVertex3f(-1.0f, -1.0f, 1.0f);
  glVertex3f(1.0f, -1.0f, 1.0f);

  glColor3f(1.0f, 1.0f, 0.0f);
  glVertex3f(1.0f, -1.0f, -1.0f);
  glVertex3f(-1.0f, -1.0f, -1.0f);
  glVertex3f(-1.0f, 1.0f, -1.0f);
  glVertex3f(1.0f, 1.0f, -1.0f);

  glColor3f(0.0f, 0.0f, 1.0f);
  glVertex3f(-1.0f, 1.0f, 1.0f);
  glVertex3f(-1.0f, 1.0f, -1.0f);
  glVertex3f(-1.0f, -1.0f, -1.0f);
  glVertex3f(-1.0f, -1.0f, 1.0f);

  glColor3f(1.0f, 0.0f, 1.0f);
  glVertex3f(1.0f, 1.0f, -1.0f);
  glVertex3f(1.0f, 1.0f, 1.0f);
  glVertex3f(1.0f, -1.0f, 1.0f);
  glVertex3f(1.0f, -1.0f, -1.0f);
  glEnd();
}

int create_cube_dpl() 
{
  int dlCube = glGenLists(1);
  glNewList(dlCube, GL_COMPILE);

  draw_cube();

  glEndList();
  return dlCube;
}

void rainbow(float ratio, float *r, float *g, float *b)
{
    float lum = 0.68f;
    float normalized = ratio * 6.0f;
    float x = fmodf(normalized, 1.0f) * lum;
    switch ((int)normalized)
    {
        case 0: *r = lum; *g = 0;   *b = 0;   *g += x; break;
        case 1: *r = lum; *g = lum; *b = 0;   *r -= x; break;
        case 2: *r = 0;   *g = lum; *b = 0;   *b += x; break;
        case 3: *r = 0;   *g = lum; *b = lum; *g -= x; break;
        case 4: *r = 0;   *g = 0;   *b = lum; *r += x; break;
        case 5: *r = lum; *g = 0;   *b = lum; *b -= x; break;
    }
}

#endif