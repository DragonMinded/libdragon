#ifndef SKINNED_H
#define SKINNED_H

#include <libdragon.h>
#include <GL/gl.h>
#include <stdint.h>

#include "camera.h"

typedef struct {
    float position[3];
    float texcoord[2];
    float normal[3];
    uint8_t mtx_index;
} skinned_vertex_t;

static const skinned_vertex_t skinned_vertices[] = {
    { .position = {-2,  0, -1}, .texcoord = {0.f, 0.f}, .normal = { 0.f,  1.f,  0.f}, .mtx_index = 0 },
    { .position = {-2,  0,  1}, .texcoord = {1.f, 0.f}, .normal = { 0.f,  1.f,  0.f}, .mtx_index = 0 },
    { .position = {-1,  0, -1}, .texcoord = {0.f, 1.f}, .normal = { 0.f,  1.f,  0.f}, .mtx_index = 0 },
    { .position = {-1,  0,  1}, .texcoord = {1.f, 1.f}, .normal = { 0.f,  1.f,  0.f}, .mtx_index = 0 },
    { .position = { 1,  0, -1}, .texcoord = {0.f, 2.f}, .normal = { 0.f,  1.f,  0.f}, .mtx_index = 1 },
    { .position = { 1,  0,  1}, .texcoord = {1.f, 2.f}, .normal = { 0.f,  1.f,  0.f}, .mtx_index = 1 },
    { .position = { 2,  0, -1}, .texcoord = {0.f, 3.f}, .normal = { 0.f,  1.f,  0.f}, .mtx_index = 1 },
    { .position = { 2,  0,  1}, .texcoord = {1.f, 3.f}, .normal = { 0.f,  1.f,  0.f}, .mtx_index = 1 },
};

void draw_skinned()
{
    glEnable(GL_MATRIX_PALETTE_ARB);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_MATRIX_INDEX_ARRAY_ARB);

    glVertexPointer(        3,  GL_FLOAT,           sizeof(skinned_vertex_t),   skinned_vertices[0].position);
    glTexCoordPointer(      2,  GL_FLOAT,           sizeof(skinned_vertex_t),   skinned_vertices[0].texcoord);
    glNormalPointer(            GL_FLOAT,           sizeof(skinned_vertex_t),   skinned_vertices[0].normal);
    glMatrixIndexPointerARB(1,  GL_UNSIGNED_BYTE,   sizeof(skinned_vertex_t),   &skinned_vertices[0].mtx_index);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, sizeof(skinned_vertices)/sizeof(skinned_vertex_t));

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_MATRIX_INDEX_ARRAY_ARB);

    glDisable(GL_MATRIX_PALETTE_ARB);
}

void skinned_model_transform()
{
    glTranslatef(0, 3, -6);
    glScalef(2, 2, 2);
}

void render_skinned(const camera_t *camera, float animation)
{
    rdpq_debug_log_msg("Skinned");

    // Set bone transforms. Note that because there is no matrix stack in palette mode, we need
    // to apply the camera transform and model transform as well for each bone.
    glMatrixMode(GL_MATRIX_PALETTE_ARB);

    // Set transform of first bone
    glCurrentPaletteMatrixARB(0);
    camera_transform(camera);
    skinned_model_transform();
    glRotatef(sinf(animation*0.1f)*45, 0, 0, 1);

    // Set transform of second bone
    glCurrentPaletteMatrixARB(1);
    camera_transform(camera);
    skinned_model_transform();
    glRotatef(-sinf(animation*0.1f)*45, 0, 0, 1);

    glMatrixMode(GL_MODELVIEW);

    glDisable(GL_CULL_FACE);
    draw_skinned();
    glEnable(GL_CULL_FACE);
}

#endif