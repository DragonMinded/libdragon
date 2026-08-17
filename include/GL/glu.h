/**
 * @file glu.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief OpenGL Utility Library (GLU) function declarations for N64.
 * @preview
 */
#ifndef __LIBDRAGON_GLU_H
#define __LIBDRAGON_GLU_H

#include "preview.h"
LIBDRAGON_PREVIEW_HEADER

#ifdef __cplusplus
extern "C" {
#endif

void gluLookAt(float eyex, float eyey, float eyez, 
               float centerx, float centery, float centerz,
               float upx, float upy, float upz);

void gluPerspective(float fovy, float aspect, float zNear, float zFar);

#ifdef __cplusplus
}
#endif

#endif
