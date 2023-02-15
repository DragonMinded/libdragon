#include "GL/glu.h"
#include "gl_internal.h"

void gluLookAt(float eyex, float eyey, float eyez, 
               float centerx, float centery, float centerz,
               float upx, float upy, float upz)
{
    GLfloat eye[3] = {eyex, eyey, eyez};
    GLfloat f[3] = {centerx - eyex, centery - eyey, centerz - eyez};
    GLfloat u[3] = {upx, upy, upz};
    GLfloat s[3];
    gl_cross(s, f, u);

    gl_normalize(f, f);
    gl_normalize(u, u);
    gl_normalize(s, s);
    gl_cross(u, s, f);

    GLfloat m[4][4];
    m[0][0] = s[0];
    m[1][0] = s[1];
    m[2][0] = s[2];
    m[3][0] = 0;

    m[0][1] = u[0];
    m[1][1] = u[1];
    m[2][1] = u[2];
    m[3][1] = 0;

    m[0][2] = -f[0];
    m[1][2] = -f[1];
    m[2][2] = -f[2];
    m[3][2] = 0;

    m[0][3] = -dot_product3(s, eye);
    m[1][3] = -dot_product3(u, eye);
    m[2][3] = -dot_product3(f, eye);
    m[3][3] = 1;

    glMultMatrixf(&m[0][0]);
};
