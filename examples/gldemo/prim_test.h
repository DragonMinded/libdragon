#ifndef PRIM_TEST_H
#define PRIM_TEST_H

#include <libdragon.h>
#include <GL/gl.h>

void points()
{
    glBegin(GL_POINTS);
        glVertex2f(-1.f, -1.f);
        glVertex2f(1.f, -1.f);
        glVertex2f(1.f, 1.f);
        glVertex2f(-1.f, 1.f);
    glEnd();
}

void lines()
{
    glBegin(GL_LINES);
        glVertex2f(-1.f, -1.f);
        glVertex2f(1.f, -1.f);
        glVertex2f(-1.f, 0.f);
        glVertex2f(1.f, 0.f);
        glVertex2f(-1.f, 1.f);
        glVertex2f(1.f, 1.f);
    glEnd();
}

void line_strip()
{
    glBegin(GL_LINE_STRIP);
        glVertex2f(-1.f, -1.f);
        glVertex2f(1.f, -1.f);
        glVertex2f(1.f, 1.f);
        glVertex2f(-1.f, 1.f);
    glEnd();
}

void line_loop()
{
    glBegin(GL_LINE_LOOP);
        glVertex2f(-1.f, -1.f);
        glVertex2f(1.f, -1.f);
        glVertex2f(1.f, 1.f);
        glVertex2f(-1.f, 1.f);
    glEnd();
}

void triangles()
{
    glBegin(GL_TRIANGLES);
        glVertex2f(-1.f, -1.f);
        glVertex2f(0.f, -1.f);
        glVertex2f(-1.f, 0.f);

        glVertex2f(1.f, 1.f);
        glVertex2f(1.f, 0.f);
        glVertex2f(0.f, 1.f);
    glEnd();
}

void triangle_strip()
{
    glBegin(GL_TRIANGLE_STRIP);
        glVertex2f(-1.f, -1.f);
        glVertex2f(1.f, -1.f);
        glVertex2f(-1.f, 1.f);
        glVertex2f(1.f, 1.f);
    glEnd();
}

void triangle_fan()
{
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(0.f, 0.f);
        glVertex2f(-1.f, 0.f);
        glVertex2f(0.f, -1.f);
        glVertex2f(1.f, 0.f);
        glVertex2f(0.f, 1.f);
        glVertex2f(-1.f, 0.f);
    glEnd();
}

void quads()
{
    glBegin(GL_QUADS);
        glVertex2f(-1.f, -1.f);
        glVertex2f(0.f, -1.f);
        glVertex2f(0.f, 0.f);
        glVertex2f(-1.f, 0.f);

        glVertex2f(1.f, 1.f);
        glVertex2f(0.f, 1.f);
        glVertex2f(0.f, 0.f);
        glVertex2f(1.f, 0.f);
    glEnd();
}

void quad_strip()
{
    glBegin(GL_QUAD_STRIP);
        glVertex2f(-1.f, -1.f);
        glVertex2f(1.f, -1.f);
        glVertex2f(-0.5f, 0.f);
        glVertex2f(0.5f, 0.f);
        glVertex2f(-1.f, 1.f);
        glVertex2f(1.f, 1.f);
    glEnd();
}

void polygon()
{
    glBegin(GL_POLYGON);
        glVertex2f(-1.f, 0.f);
        glVertex2f(-0.75f, -0.75f);
        glVertex2f(0.f, -1.f);
        glVertex2f(0.75f, -0.75f);
        glVertex2f(1.f, 0.f);
        glVertex2f(0.75f, 0.75f);
        glVertex2f(0.f, 1.f);
        glVertex2f(-0.75f, 0.75f);
    glEnd();
}

void prim_test()
{
    glPushMatrix();
    glTranslatef(-6, 1.5f, 0);
    points();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-3, 1.5f, 0);
    lines();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0, 1.5f, 0);
    line_strip();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(3, 1.5f, 0);
    line_loop();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(6, 1.5f, 0);
    triangles();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-6, -1.5f, 0);
    triangle_strip();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-3, -1.5f, 0);
    triangle_fan();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0, -1.5f, 0);
    quads();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(3, -1.5f, 0);
    quad_strip();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(6, -1.5f, 0);
    polygon();
    glPopMatrix();
}

void render_primitives(float rotation)
{
    rdpq_debug_log_msg("Primitives");
    glPushMatrix();

    glTranslatef(0, 6, 0);
    glRotatef(-rotation*2.46f, 0, 1, 0);

    // Configure alpha blending (transparency)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Set a constant alpha for all vertices
    glColor4f(1, 1, 1, 0.4f);
    
    // We want to see back faces as well
    glDisable(GL_CULL_FACE);

    // Transparent polygons should not write to the depth buffer
    glDepthMask(GL_FALSE);

    prim_test();
    
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    glPopMatrix();
}

#endif
