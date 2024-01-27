#ifndef __LIBDRAGON_GL_H
#define __LIBDRAGON_GL_H

#include <stdint.h>
#include <stddef.h>

typedef struct surface_s surface_t;
typedef struct sprite_s sprite_t;
typedef struct rdpq_texparms_s rdpq_texparms_t;

#include <GL/gl_enums.h>
#include <rdpq_mode.h>

#define _GL_UNSUPPORTED(func) _Static_assert(0, #func " is not supported!")

#define GL_VERSION_1_1                  1
#define GL_ARB_multisample              1
#define GL_EXT_packed_pixels            1
#define GL_ARB_vertex_buffer_object     1
#define GL_ARB_texture_mirrored_repeat  1
#define GL_ARB_vertex_array_object      1
#define GL_ARB_matrix_palette           1
#define GL_N64_RDPQ_interop             1
#define GL_N64_surface_image            1
#define GL_N64_half_fixed_point         1
#define GL_N64_reduced_aliasing         1
#define GL_N64_interpenetrating         1
#define GL_N64_dither_mode              1
#define GL_N64_copy_matrix              1
#define GL_N64_texture_flip             1

/* Data types */

typedef uint8_t     GLboolean;
typedef int8_t      GLbyte;
typedef uint8_t     GLubyte;
typedef int16_t     GLshort;
typedef uint16_t    GLushort;
typedef int32_t     GLint;
typedef uint32_t    GLuint;
typedef uint32_t    GLsizei;
typedef uint32_t    GLenum;
typedef uint32_t    GLbitfield;
typedef float       GLfloat;
typedef float       GLclampf;
typedef double      GLdouble;
typedef double      GLclampd;
typedef void        GLvoid;

typedef intptr_t    GLintptrARB;
typedef size_t      GLsizeiptrARB;

typedef int16_t     GLhalfxN64;

#define GL_FALSE 0
#define GL_TRUE  1

#ifdef __cplusplus
extern "C" {
#endif

/* Errors */

GLenum glGetError(void);

/* Flags */

void glEnable(GLenum target);
void glDisable(GLenum target);

/* Immediate mode */

void glBegin(GLenum mode);
void glEnd(void);

#define glEdgeFlag(flag) _GL_UNSUPPORTED(glEdgeFlag)
#define glEdgeFlagv(flag) _GL_UNSUPPORTED(glEdgeFlagv)

void glVertex2s(GLshort x, GLshort y);
void glVertex2i(GLint x, GLint y);
void glVertex2f(GLfloat x, GLfloat y);
void glVertex2d(GLdouble x, GLdouble y);
void glVertex2hxN64(GLhalfxN64 x, GLhalfxN64 y);

void glVertex3s(GLshort x, GLshort y, GLshort z);
void glVertex3i(GLint x, GLint y, GLint z);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glVertex3d(GLdouble x, GLdouble y, GLdouble z);
void glVertex3hxN64(GLhalfxN64 x, GLhalfxN64 y, GLhalfxN64 z);

void glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w);
void glVertex4i(GLint x, GLint y, GLint z, GLint w);
void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
void glVertex4hxN64(GLhalfxN64 x, GLhalfxN64 y, GLhalfxN64 z, GLhalfxN64 w);

void glVertex2sv(const GLshort *v);
void glVertex2iv(const GLint *v);
void glVertex2fv(const GLfloat *v);
void glVertex2dv(const GLdouble *v);
void glVertex2hxvN64(const GLhalfxN64 *v);

void glVertex3sv(const GLshort *v);
void glVertex3iv(const GLint *v);
void glVertex3fv(const GLfloat *v);
void glVertex3dv(const GLdouble *v);
void glVertex3hxvN64(const GLhalfxN64 *v);

void glVertex4sv(const GLshort *v);
void glVertex4iv(const GLint *v);
void glVertex4fv(const GLfloat *v);
void glVertex4dv(const GLdouble *v);
void glVertex4hxvN64(const GLhalfxN64 *v);

void glTexCoord1s(GLshort s);
void glTexCoord1i(GLint s);
void glTexCoord1f(GLfloat s);
void glTexCoord1d(GLdouble s);
void glTexCoord1hxN64(GLhalfxN64 s);

void glTexCoord2s(GLshort s, GLshort t);
void glTexCoord2i(GLint s, GLint t);
void glTexCoord2f(GLfloat s, GLfloat t);
void glTexCoord2d(GLdouble s, GLdouble t);
void glTexCoord2hxN64(GLhalfxN64 s, GLhalfxN64 t);

void glTexCoord3s(GLshort s, GLshort t, GLshort r);
void glTexCoord3i(GLint s, GLint t, GLint r);
void glTexCoord3f(GLfloat s, GLfloat t, GLfloat r);
void glTexCoord3d(GLdouble s, GLdouble t, GLdouble r);
void glTexCoord3hxN64(GLhalfxN64 s, GLhalfxN64 t, GLhalfxN64 r);

void glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q);
void glTexCoord4i(GLint s, GLint t, GLint r, GLint q);
void glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q);
void glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q);
void glTexCoord4hxN64(GLhalfxN64 s, GLhalfxN64 t, GLhalfxN64 r, GLhalfxN64 q);

void glTexCoord1sv(const GLshort *v);
void glTexCoord1iv(const GLint *v);
void glTexCoord1fv(const GLfloat *v);
void glTexCoord1dv(const GLdouble *v);
void glTexCoord1hxvN64(const GLhalfxN64 *v);

void glTexCoord2sv(const GLshort *v);
void glTexCoord2iv(const GLint *v);
void glTexCoord2fv(const GLfloat *v);
void glTexCoord2dv(const GLdouble *v);
void glTexCoord2hxvN64(const GLhalfxN64 *v);

void glTexCoord3sv(const GLshort *v);
void glTexCoord3iv(const GLint *v);
void glTexCoord3fv(const GLfloat *v);
void glTexCoord3dv(const GLdouble *v);
void glTexCoord3hxvN64(const GLhalfxN64 *v);

void glTexCoord4sv(const GLshort *v);
void glTexCoord4iv(const GLint *v);
void glTexCoord4fv(const GLfloat *v);
void glTexCoord4dv(const GLdouble *v);
void glTexCoord4hxvN64(const GLhalfxN64 *v);

void glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz);
void glNormal3s(GLshort nx, GLshort ny, GLshort nz);
void glNormal3i(GLint nx, GLint ny, GLint nz);
void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz);
void glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz);

void glNormal3bv(const GLbyte *v);
void glNormal3sv(const GLshort *v);
void glNormal3iv(const GLint *v);
void glNormal3fv(const GLfloat *v);
void glNormal3dv(const GLdouble *v);

void glColor3b(GLbyte r, GLbyte g, GLbyte b);
void glColor3s(GLshort r, GLshort g, GLshort b);
void glColor3i(GLint r, GLint g, GLint b);
void glColor3f(GLfloat r, GLfloat g, GLfloat b);
void glColor3d(GLdouble r, GLdouble g, GLdouble b);
void glColor3ub(GLubyte r, GLubyte g, GLubyte b);
void glColor3us(GLushort r, GLushort g, GLushort b);
void glColor3ui(GLuint r, GLuint g, GLuint b);

void glColor4b(GLbyte r, GLbyte g, GLbyte b, GLbyte a);
void glColor4s(GLshort r, GLshort g, GLshort b, GLshort a);
void glColor4i(GLint r, GLint g, GLint b, GLint a);
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glColor4d(GLdouble r, GLdouble g, GLdouble b, GLdouble a);
void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a);
void glColor4us(GLushort r, GLushort g, GLushort b, GLushort a);
void glColor4ui(GLuint r, GLuint g, GLuint b, GLuint a);

void glColor3bv(const GLbyte *v);
void glColor3sv(const GLshort *v);
void glColor3iv(const GLint *v);
void glColor3fv(const GLfloat *v);
void glColor3dv(const GLdouble *v);
void glColor3ubv(const GLubyte *v);
void glColor3usv(const GLushort *v);
void glColor3uiv(const GLuint *v);

void glColor4bv(const GLbyte *v);
void glColor4sv(const GLshort *v);
void glColor4iv(const GLint *v);
void glColor4fv(const GLfloat *v);
void glColor4dv(const GLdouble *v);
void glColor4ubv(const GLubyte *v);
void glColor4usv(const GLushort *v);
void glColor4uiv(const GLuint *v);

#define glIndexs(c) _GL_UNSUPPORTED(glIndexs)
#define glIndexi(c) _GL_UNSUPPORTED(glIndexi)
#define glIndexf(c) _GL_UNSUPPORTED(glIndexf)
#define glIndexd(c) _GL_UNSUPPORTED(glIndexd)
#define glIndexub(c) _GL_UNSUPPORTED(glIndexub)
#define glIndexsv(v) _GL_UNSUPPORTED(glIndexsv)
#define glIndexiv(v) _GL_UNSUPPORTED(glIndexiv)
#define glIndexfv(v) _GL_UNSUPPORTED(glIndexfv)
#define glIndexdv(v) _GL_UNSUPPORTED(glIndexdv)
#define glIndexubv(v) _GL_UNSUPPORTED(glIndexubv)

void glMatrixIndexubvARB(GLint size, const GLubyte *v);
void glMatrixIndexusvARB(GLint size, const GLushort *v);
void glMatrixIndexuivARB(GLint size, const GLuint *v);

/* Fixed point */

void glVertexHalfFixedPrecisionN64(GLuint bits);
void glTexCoordHalfFixedPrecisionN64(GLuint bits);

/* Vertex arrays */

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer);
void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void glMatrixIndexPointerARB(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);

#define glEdgeFlagPointer(stride, pointer) _GL_UNSUPPORTED(glEdgeFlagPointer)
#define glIndexPointer(type, stride, pointer) _GL_UNSUPPORTED(glIndexPointer)

void glEnableClientState(GLenum array);
void glDisableClientState(GLenum array);

void glArrayElement(GLint i);

void glDrawArrays(GLenum mode, GLint first, GLsizei count);

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);

void glInterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer);

void glGenVertexArrays(GLsizei n, GLuint *arrays);
void glDeleteVertexArrays(GLsizei n, const GLuint *arrays);

void glBindVertexArray(GLuint array);

GLboolean glIsVertexArray(GLuint array);

/* Buffer Objects */

void glBindBufferARB(GLenum target, GLuint buffer);
void glDeleteBuffersARB(GLsizei n, const GLuint *buffers);
void glGenBuffersARB(GLsizei n, GLuint *buffers);
GLboolean glIsBufferARB(GLuint buffer);

void glBufferDataARB(GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage);
void glBufferSubDataARB(GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data);

void glGetBufferSubDataARB(GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid *data);

GLvoid * glMapBufferARB(GLenum target, GLenum access);
GLboolean glUnmapBufferARB(GLenum target);

void glGetBufferParameterivARB(GLenum target, GLenum pname, GLint *params);
void glGetBufferPointervARB(GLenum target, GLenum pname, GLvoid **params);

/* Rectangles */

void glRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2);
void glRecti(GLint x1, GLint y1, GLint x2, GLint y2);
void glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void glRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2);

void glRectsv(const GLshort *v1, const GLshort *v2);
void glRectiv(const GLint *v1, const GLint *v2);
void glRectfv(const GLfloat *v1, const GLfloat *v2);
void glRectdv(const GLdouble *v1, const GLdouble *v2);

/* Viewport */

void glDepthRange(GLclampd n, GLclampd f);

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h);

/* Matrices */

void glMatrixMode(GLenum mode);

void glLoadMatrixf(const GLfloat *m);
void glLoadMatrixd(const GLdouble *m);

void glMultMatrixf(const GLfloat *m);
void glMultMatrixd(const GLdouble *m);

void glLoadIdentity(void);

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);

void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void glTranslated(GLdouble x, GLdouble y, GLdouble z);

void glScalef(GLfloat x, GLfloat y, GLfloat z);
void glScaled(GLdouble x, GLdouble y, GLdouble z);

void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);

void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);

void glPushMatrix(void);
void glPopMatrix(void);

void glCurrentPaletteMatrixARB(GLint index);

void glCopyMatrixN64(GLenum source);

/* Texture coordinate generation */

void glTexGeni(GLenum coord, GLenum pname, GLint param);
void glTexGenf(GLenum coord, GLenum pname, GLfloat param);
void glTexGend(GLenum coord, GLenum pname, GLdouble param);

void glTexGeniv(GLenum coord, GLenum pname, const GLint *params);
void glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params);
void glTexGendv(GLenum coord, GLenum pname, const GLdouble *params);

/* Clipping planes */

#define glClipPlane(p, eqn) _GL_UNSUPPORTED(glClipPlane)

/* Raster position */

#define glRasterPos2s(x, y) _GL_UNSUPPORTED(glRasterPos2s)
#define glRasterPos2i(x, y) _GL_UNSUPPORTED(glRasterPos2i)
#define glRasterPos2f(x, y) _GL_UNSUPPORTED(glRasterPos2f)
#define glRasterPos2d(x, y) _GL_UNSUPPORTED(glRasterPos2d)
#define glRasterPos3s(x, y, z) _GL_UNSUPPORTED(glRasterPos3s)
#define glRasterPos3i(x, y, z) _GL_UNSUPPORTED(glRasterPos3i)
#define glRasterPos3f(x, y, z) _GL_UNSUPPORTED(glRasterPos3f)
#define glRasterPos3d(x, y, z) _GL_UNSUPPORTED(glRasterPos3d)
#define glRasterPos4s(x, y, z, w) _GL_UNSUPPORTED(glRasterPos4s)
#define glRasterPos4i(x, y, z, w) _GL_UNSUPPORTED(glRasterPos4i)
#define glRasterPos4f(x, y, z, w) _GL_UNSUPPORTED(glRasterPos4f)
#define glRasterPos4d(x, y, z, w) _GL_UNSUPPORTED(glRasterPos4d)
#define glRasterPos2sv(v) _GL_UNSUPPORTED(glRasterPos2sv)
#define glRasterPos2iv(v) _GL_UNSUPPORTED(glRasterPos2iv)
#define glRasterPos2fv(v) _GL_UNSUPPORTED(glRasterPos2fv)
#define glRasterPos2dv(v) _GL_UNSUPPORTED(glRasterPos2dv)
#define glRasterPos3sv(v) _GL_UNSUPPORTED(glRasterPos3sv)
#define glRasterPos3iv(v) _GL_UNSUPPORTED(glRasterPos3iv)
#define glRasterPos3fv(v) _GL_UNSUPPORTED(glRasterPos3fv)
#define glRasterPos3dv(v) _GL_UNSUPPORTED(glRasterPos3dv)
#define glRasterPos4sv(v) _GL_UNSUPPORTED(glRasterPos4sv)
#define glRasterPos4iv(v) _GL_UNSUPPORTED(glRasterPos4iv)
#define glRasterPos4fv(v) _GL_UNSUPPORTED(glRasterPos4fv)
#define glRasterPos4dv(v) _GL_UNSUPPORTED(glRasterPos4dv)

/* Shading and lighting */

void glMateriali(GLenum face, GLenum pname, GLint param);
void glMaterialf(GLenum face, GLenum pname, GLfloat param);

void glMaterialiv(GLenum face, GLenum pname, const GLint *params);
void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params);

void glLighti(GLenum light, GLenum pname, GLint param);
void glLightf(GLenum light, GLenum pname, GLfloat param);

void glLightiv(GLenum light, GLenum pname, const GLint *params);
void glLightfv(GLenum light, GLenum pname, const GLfloat *params);

void glLightModeli(GLenum pname, GLint param);
void glLightModelf(GLenum pname, GLfloat param);

void glLightModeliv(GLenum pname, const GLint *params);
void glLightModelfv(GLenum pname, const GLfloat *params);

void glColorMaterial(GLenum face, GLenum mode);

void glShadeModel(GLenum mode);

/* Points */

void glPointSize(GLfloat size);

/* Lines */

void glLineWidth(GLfloat width);

#define glLineStipple(factor, pattern) _GL_UNSUPPORTED(glLineStipple)

/* Polygons */

void glCullFace(GLenum mode);

void glFrontFace(GLenum dir);

void glPolygonMode(GLenum face, GLenum mode);

#define glPolygonStipple(pattern) _GL_UNSUPPORTED(glPolygonStipple)
#define glPolygonOffset(factor, units) _GL_UNSUPPORTED(glPolygonOffset)

/* Pixel rectangles */

void glPixelStorei(GLenum pname, GLint param);
void glPixelStoref(GLenum pname, GLfloat param);

void glPixelTransferi(GLenum pname, GLint value);
void glPixelTransferf(GLenum pname, GLfloat value);

void glPixelMapusv(GLenum map, GLsizei size, const GLushort *values);
void glPixelMapuiv(GLenum map, GLsizei size, const GLuint *values);
void glPixelMapfv(GLenum map, GLsizei size, const GLfloat *values);

#define glPixelZoom(zx, zy) _GL_UNSUPPORTED(glPixelZoom)
#define glDrawPixels(width, height, format, type, data) _GL_UNSUPPORTED(glDrawPixels)
#define glReadPixels(x, y, width, height, format, type, data) _GL_UNSUPPORTED(glReadPixels)
#define glCopyPixels(x, y, width, height, type) _GL_UNSUPPORTED(glCopyPixels)

/* Bitmaps */

#define glBitmap(w, h, xbo, ybo, xbi, ybi, data) _GL_UNSUPPORTED(glBitmap)

/* Texturing */

void glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *data);
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data);

void glSurfaceTexImageN64(GLenum target, GLint level, surface_t *surface, rdpq_texparms_t *texparms);
void glSpriteTextureN64(GLenum target, sprite_t *sprite, rdpq_texparms_t *texparms);

void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glTexParameterf(GLenum target, GLenum pname, GLfloat param);

void glTexParameteriv(GLenum target, GLenum pname, const GLint *params);
void glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params);

void glBindTexture(GLenum target, GLuint texture);

void glDeleteTextures(GLsizei n, const GLuint *textures);
void glGenTextures(GLsizei n, GLuint *textures);

GLboolean glAreTexturesResident(GLsizei n, const GLuint *textures, const GLboolean *residences);

void glPrioritizeTextures(GLsizei n, const GLuint *textures, const GLclampf *priorities);

void glTexEnvi(GLenum target, GLenum pname, GLint param);
void glTexEnvf(GLenum target, GLenum pname, GLfloat param);

void glTexEnviv(GLenum target, GLenum pname, const GLint *params);
void glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params);

GLboolean glIsTexture(GLuint texture);

#define glCopyTexImage1D(target, level, internalformat, x, y, width, border) _GL_UNSUPPORTED(glCopyTexImage1D)
#define glCopyTexImage2D(target, level, internalformat, x, y, width, height, border) _GL_UNSUPPORTED(glCopyTexImage2D)
#define glTexSubImage1D(target, level, xoffset, width, format, type, data) _GL_UNSUPPORTED(glTexSubImage1D)
#define glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, data) _GL_UNSUPPORTED(glTexSubImage2D)
#define glCopyTexSubImage1D(target, level, xoffset, x, y, width) _GL_UNSUPPORTED(glCopyTexSubImage1D)
#define glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height) _GL_UNSUPPORTED(glCopyTexSubImage2D)

/* Fog */

void glFogi(GLenum pname, GLint param);
void glFogf(GLenum pname, GLfloat param);

void glFogiv(GLenum pname, const GLint *params);
void glFogfv(GLenum pname, const GLfloat *params);

/* Dithering */

void glDitherModeN64(rdpq_dither_t mode);

/* Scissor test */

void glScissor(GLint left, GLint bottom, GLsizei width, GLsizei height);

/* Alpha test */

void glAlphaFunc(GLenum func, GLclampf ref);

/* Stencil test */

#define glStencilFunc(func, ref, mask) _GL_UNSUPPORTED(glStencilFunc)
#define glStencilOp(sfail, dpfail, dppass) _GL_UNSUPPORTED(glStencilOp)

/* Depth test */

void glDepthFunc(GLenum func);

/* Blending */

void glBlendFunc(GLenum src, GLenum dst);

/* Logical operation */

#define glLogicOp(op) _GL_UNSUPPORTED(glLogicOp)

/* Framebuffer selection */

#define glDrawBuffer(buf) _GL_UNSUPPORTED(glDrawBuffer)
#define glReadBuffer(src) _GL_UNSUPPORTED(glReadBuffer)

/* Masks */

void glDepthMask(GLboolean mask);

#define glIndexMask(mask) _GL_UNSUPPORTED(glIndexMask)
#define glColorMask(r, g, b, a) _GL_UNSUPPORTED(glColorMask)
#define glStencilMask(mask) _GL_UNSUPPORTED(glStencilMask)

/* Clearing */

void glClear(GLbitfield buf);

void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
void glClearDepth(GLclampd d);

#define glClearIndex(index) _GL_UNSUPPORTED(glClearIndex)
#define glClearStencil(s) _GL_UNSUPPORTED(glClearStencil)
#define glClearAccum(r, g, b, a) _GL_UNSUPPORTED(glClearAccum)

/* Accumulation buffer */

#define glAccum(op, value) _GL_UNSUPPORTED(glAccum)

/* Evaluators */

#define glMap1f(type, u1, u2, stride, order, points) _GL_UNSUPPORTED(glMap1f)
#define glMap1d(type, u1, u2, stride, order, points) _GL_UNSUPPORTED(glMap1d)
#define glMap2f(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points) _GL_UNSUPPORTED(glMap2f)
#define glMap2d(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points) _GL_UNSUPPORTED(glMap2d)
#define glEvalCoord1f(u) _GL_UNSUPPORTED(glEvalCoord1f)
#define glEvalCoord1d(u) _GL_UNSUPPORTED(glEvalCoord1d)
#define glEvalCoord2f(u, v) _GL_UNSUPPORTED(glEvalCoord2f)
#define glEvalCoord2d(u, v) _GL_UNSUPPORTED(glEvalCoord2d)
#define glEvalCoord1fv(v) _GL_UNSUPPORTED(glEvalCoord1fv)
#define glEvalCoord1dv(v) _GL_UNSUPPORTED(glEvalCoord1dv)
#define glEvalCoord2fv(v) _GL_UNSUPPORTED(glEvalCoord2fv)
#define glEvalCoord2dv(v) _GL_UNSUPPORTED(glEvalCoord2dv)
#define glMapGrid1f(n, u1, u2) _GL_UNSUPPORTED(glMapGrid1f)
#define glMapGrid1d(n, u1, u2) _GL_UNSUPPORTED(glMapGrid1d)
#define glMapGrid2f(nu, u1, u2, nv, v1, v2) _GL_UNSUPPORTED(glMapGrid2f)
#define glMapGrid2d(nu, u1, u2, nv, v1, v2) _GL_UNSUPPORTED(glMapGrid2d)
#define glEvalMesh1(mode, p1, p2) _GL_UNSUPPORTED(glEvalMesh1)
#define glEvalMesh2(mode, p1, p2, q1, q2) _GL_UNSUPPORTED(glEvalMesh2)
#define glEvalPoint1(p) _GL_UNSUPPORTED(glEvalPoint1)
#define glEvalPoint2(p, q) _GL_UNSUPPORTED(glEvalPoint2)

/* Render mode */

#define glRenderMode(mode) _GL_UNSUPPORTED(glRenderMode)

/* Selection */

#define glInitNames() _GL_UNSUPPORTED(glInitNames)
#define glPopName() _GL_UNSUPPORTED(glPopName)
#define glPushName(name) _GL_UNSUPPORTED(glPushName)
#define glLoadName(name) _GL_UNSUPPORTED(glLoadName)
#define glSelectBuffer(n, buffer) _GL_UNSUPPORTED(glSelectBuffer)

/* Feedback */

#define glFeedbackBuffer(n, type, buffer) _GL_UNSUPPORTED(glFeedbackBuffer)
#define glPassThrough(token) _GL_UNSUPPORTED(glPassThrough)

/* Display lists */

void glNewList(GLuint n, GLenum mode);
void glEndList(void);

void glCallList(GLuint n);
void glCallLists(GLsizei n, GLenum type, const GLvoid *lists);

void glListBase(GLuint base);

GLuint glGenLists(GLsizei s);

GLboolean glIsList(GLuint list);

void glDeleteLists(GLuint list, GLsizei range);

/* Synchronization */

void glFlush(void);
void glFinish(void);

/* Hints */

void glHint(GLenum target, GLenum hint);

/* Multisampling */

#define glSampleCoverageARB(value, invert) _GL_UNSUPPORTED(glSampleCoverageARB)

/* Queries */

void glGetBooleanv(GLenum value, GLboolean *data);
void glGetIntegerv(GLenum value, GLint *data);
void glGetFloatv(GLenum value, GLfloat *data);
void glGetDoublev(GLenum value, GLdouble *data);

GLboolean glIsEnabled(GLenum value);

void glGetPointerv(GLenum pname, GLvoid **params);

GLubyte *glGetString(GLenum name);

#define glGetClipPlane(plane, eqn) _GL_UNSUPPORTED(glGetClipPlane)
#define glGetLightiv(light, value, data) _GL_UNSUPPORTED(glGetLightiv)
#define glGetLightfv(light, value, data) _GL_UNSUPPORTED(glGetLightfv)
#define glGetMaterialiv(face, value, data) _GL_UNSUPPORTED(glGetMaterialiv)
#define glGetMaterialfv(face, value, data) _GL_UNSUPPORTED(glGetMaterialfv)
#define glGetTexEnviv(env, value, data) _GL_UNSUPPORTED(glGetTexEnviv)
#define glGetTexEnvfv(env, value, data) _GL_UNSUPPORTED(glGetTexEnvfv)
#define glGetTexGeniv(coord, value, data) _GL_UNSUPPORTED(glGetTexGeniv)
#define glGetTexGenfv(coord, value, data) _GL_UNSUPPORTED(glGetTexGenfv)
#define glGetTexParameteriv(target, value, data) _GL_UNSUPPORTED(glGetTexParameteriv)
#define glGetTexParameterfv(target, value, data) _GL_UNSUPPORTED(glGetTexParameterfv)
#define glGetTexLevelParameteriv(target, lod, value, data) _GL_UNSUPPORTED(glGetTexLevelParameteriv)
#define glGetTexLevelParameterfv(target, lod, value, data) _GL_UNSUPPORTED(glGetTexLevelParameterfv)
#define glGetPixelMapusv(map, data) _GL_UNSUPPORTED(glGetPixelMapusv)
#define glGetPixelMapuiv(map, data) _GL_UNSUPPORTED(glGetPixelMapuiv)
#define glGetPixelMapfv(map, data) _GL_UNSUPPORTED(glGetPixelMapfv)
#define glGetTexImage(tex, lod, format, type, img) _GL_UNSUPPORTED(glGetTexImage)
#define glGetMapiv(map, value, data) _GL_UNSUPPORTED(glGetMapiv)
#define glGetMapfv(map, value, data) _GL_UNSUPPORTED(glGetMapfv)
#define glGetMapdv(map, value, data) _GL_UNSUPPORTED(glGetMapdv)
#define glGetPolygonStipple(pattern) _GL_UNSUPPORTED(glGetPolygonStipple)

/* Attribute stack */

#define glPushAttrib(mask) _GL_UNSUPPORTED(glPushAttrib)
#define glPushClientAttrib(mask) _GL_UNSUPPORTED(glPushClientAttrib)

#define glPopAttrib() _GL_UNSUPPORTED(glPopAttrib)
#define glPopClientAttrib() _GL_UNSUPPORTED(glPopClientAttrib)

/* Deprecated functions (will be removed on trunk) */

__attribute__((deprecated("use glSurfaceTexImageN64 instead")))
inline void glTexImageN64(GLenum target, GLint level, surface_t *surface)
{
    glSurfaceTexImageN64(target, level, surface, NULL);
}

__attribute__((deprecated("use glSpriteTextureN64 instead")))
inline void glTexSpriteN64(GLenum target, sprite_t *sprite)
{
    glSpriteTextureN64(target, sprite, NULL);
}

#ifdef __cplusplus
}
#endif


#endif
