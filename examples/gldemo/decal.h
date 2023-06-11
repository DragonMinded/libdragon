#ifndef DECAL_H
#define DECAL_H

#include <libdragon.h>
#include <GL/gl.h>

void draw_quad()
{
    glBegin(GL_TRIANGLE_STRIP);
        glNormal3f(0, 1, 0);
        glTexCoord2f(0, 0);
        glVertex3f(-0.5f, 0, -0.5f);
        glTexCoord2f(0, 1);
        glVertex3f(-0.5f, 0, 0.5f);
        glTexCoord2f(1, 0);
        glVertex3f(0.5f, 0, -0.5f);
        glTexCoord2f(1, 1);
        glVertex3f(0.5f, 0, 0.5f);
    glEnd();
}

void render_decal()
{
    rdpq_debug_log_msg("Decal");
    glPushMatrix();
    glTranslatef(0, 0, 6);
    glRotatef(35, 0, 1, 0);
    glScalef(3, 3, 3);

    // Decals are drawn with the depth func set to GL_EQUAL. Note that glPolygonOffset is not supported on N64.
    glDepthFunc(GL_EQUAL);

    // Disable writing to depth buffer, because the depth value will be the same anyway
    glDepthMask(GL_FALSE);

    // Apply vertex color as material color.
    // This time, we set one vertex color for the entire model.
    glEnable(GL_COLOR_MATERIAL);
    glColor4f(1.0f, 0.4f, 0.2f, 0.5f);

    draw_quad();

    glDisable(GL_COLOR_MATERIAL);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    
    glPopMatrix();
}

#endif
