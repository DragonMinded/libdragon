#ifndef __LIBDRAGON_GL_H
#define __LIBDRAGON_GL_H

#include <stdint.h>

#define GL_VERSION_1_1 1

/* Data types */

typedef uint8_t	    GLboolean;
typedef int8_t	    GLbyte;
typedef uint8_t	    GLubyte;
typedef int16_t     GLshort;
typedef uint16_t	GLushort;
typedef int32_t     GLint;
typedef uint32_t	GLuint;
typedef uint32_t    GLsizei;
typedef uint32_t	GLenum;
typedef uint32_t	GLbitfield;
typedef float	    GLfloat;
typedef float	    GLclampf;
typedef double	    GLdouble;
typedef double	    GLclampd;
typedef void	    GLvoid;

#define GL_BYTE
#define GL_SHORT
#define GL_INT
#define GL_FLOAT
#define GL_DOUBLE
#define GL_UNSIGNED_BYTE
#define GL_UNSIGNED_SHORT
#define GL_UNSIGNED_INT

#define GL_FALSE 0
#define GL_TRUE  1

#ifdef __cplusplus
extern "C" {
#endif

/* Errors */

#define GL_NO_ERROR             0
#define GL_INVALID_ENUM         1
#define GL_INVALID_VALUE        2
#define GL_INVALID_OPERATION    3
#define GL_STACK_OVERFLOW       4
#define GL_STACK_UNDERFLOW      5
#define GL_OUT_OF_MEMORY        6

GLenum glGetError(void);

/* Flags */

#define GL_DITHER

void glEnable(GLenum target);
void glDisable(GLenum target);

/* Immediate mode */

#define GL_POINTS
#define GL_LINE_STRIP
#define GL_LINE_LOOPS
#define GL_LINES
#define GL_POLYGON
#define GL_TRIANGLE_STRIP
#define GL_TRIANGLE_FAN
#define GL_TRIANGLES
#define GL_QUAD_STRIP
#define GL_QUADS

#define GL_NORMALIZE

#define GL_CURRENT_COLOR
#define GL_CURRENT_INDEX
#define GL_CURRENT_NORMAL
#define GL_CURRENT_TEXTURE_COORDS
#define GL_CURRENT_RASTER_COLOR
#define GL_CURRENT_RASTER_DISTANCE
#define GL_CURRENT_RASTER_INDEX
#define GL_CURRENT_RASTER_POSITION
#define GL_CURRENT_RASTER_POSITION_VALID
#define GL_CURRENT_RASTER_TEXTURE_COORDS

void glBegin(GLenum mode);
void glEnd(void);

void glEdgeFlag(GLboolean flag);
void glEdgeFlagv(GLboolean *flag);

void glVertex2s(GLshort x, GLshort y);
void glVertex2i(GLint x, GLint y);
void glVertex2f(GLfloat x, GLfloat y);
void glVertex2d(GLdouble x, GLdouble y);

void glVertex3s(GLshort x, GLshort y, GLshort z);
void glVertex3i(GLint x, GLint y, GLint z);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glVertex3d(GLdouble x, GLdouble y, GLdouble z);

void glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w);
void glVertex4i(GLint x, GLint y, GLint z, GLint w);
void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w);

void glVertex2sv(const GLshort *v);
void glVertex2iv(const GLint *v);
void glVertex2fv(const GLfloat *v);
void glVertex2dv(const GLdouble *v);

void glVertex3sv(const GLshort *v);
void glVertex3iv(const GLint *v);
void glVertex3fv(const GLfloat *v);
void glVertex3dv(const GLdouble *v);

void glVertex4sv(const GLshort *v);
void glVertex4iv(const GLint *v);
void glVertex4fv(const GLfloat *v);
void glVertex4dv(const GLdouble *v);

void glTexCoord1s(GLshort s);
void glTexCoord1i(GLint s);
void glTexCoord1f(GLfloat s);
void glTexCoord1d(GLdouble s);

void glTexCoord2s(GLshort s, GLshort t);
void glTexCoord2i(GLint s, GLint t);
void glTexCoord2f(GLfloat s, GLfloat t);
void glTexCoord2d(GLdouble s, GLdouble t);

void glTexCoord3s(GLshort s, GLshort t, GLshort r);
void glTexCoord3i(GLint s, GLint t, GLint r);
void glTexCoord3f(GLfloat s, GLfloat t, GLfloat r);
void glTexCoord3d(GLdouble s, GLdouble t, GLdouble r);

void glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q);
void glTexCoord4i(GLint s, GLint t, GLint r, GLint q);
void glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q);
void glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q);

void glTexCoord1sv(const GLshort *v);
void glTexCoord1iv(const GLint *v);
void glTexCoord1fv(const GLfloat *v);
void glTexCoord1dv(const GLdouble *v);

void glTexCoord2sv(const GLshort *v);
void glTexCoord2iv(const GLint *v);
void glTexCoord2fv(const GLfloat *v);
void glTexCoord2dv(const GLdouble *v);

void glTexCoord3sv(const GLshort *v);
void glTexCoord3iv(const GLint *v);
void glTexCoord3fv(const GLfloat *v);
void glTexCoord3dv(const GLdouble *v);

void glTexCoord4sv(const GLshort *v);
void glTexCoord4iv(const GLint *v);
void glTexCoord4fv(const GLfloat *v);
void glTexCoord4dv(const GLdouble *v);

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

void glIndexs(GLshort c);
void glIndexi(GLint c);
void glIndexf(GLfloat c);
void glIndexd(GLdouble c);
void glIndexub(GLubyte c);

void glIndexsv(const GLshort *v);
void glIndexiv(const GLint *v);
void glIndexfv(const GLfloat *v);
void glIndexdv(const GLdouble *v);
void glIndexubv(const GLubyte *v);

/* Vertex arrays */

#define GL_EDGE_FLAG_ARRAY
#define GL_VERTEX_ARRAY
#define GL_TEXTURE_COORD_ARRAY
#define GL_NORMAL_ARRAY
#define GL_COLOR_ARRAY
#define GL_INDEX_ARRAY

#define GL_V2F
#define GL_V3F
#define GL_C4UB_V2F
#define GL_C4UB_V3F
#define GL_C3F_V3F
#define GL_N3F_V3F
#define GL_C4F_N3F_V3F
#define GL_T2F_V3F
#define GL_T4F_V4F
#define GL_T2F_C4UB_V3F
#define GL_T2F_C3F_V3F
#define GL_T2F_N3F_V3F
#define GL_T2F_C4F_N3F_V3F
#define GL_T4F_C4F_N3F_V4F

#define GL_VERTEX_ARRAY_SIZE
#define GL_VERTEX_ARRAY_STRIDE
#define GL_VERTEX_ARRAY_TYPE

#define GL_EDGE_FLAG
#define GL_EDGE_FLAG_ARRAY_STRIDE

#define GL_COLOR_ARRAY_SIZE
#define GL_COLOR_ARRAY_STRIDE
#define GL_COLOR_ARRAY_TYPE

#define GL_INDEX_ARRAY_STRIDE
#define GL_INDEX_ARRAY_TYPE

#define GL_NORMAL_ARRAY_STRIDE
#define GL_NORMAL_ARRAY_TYPE

#define GL_TEXTURE_COORD_ARRAY_SIZE
#define GL_TEXTURE_COORD_ARRAY_STRIDE
#define GL_TEXTURE_COORD_ARRAY_TYPE

#define GL_VERTEX_ARRAY_POINTER
#define GL_EDGE_FLAG_ARRAY_POINTER
#define GL_COLOR_ARRAY_POINTER
#define GL_NORMAL_ARRAY_POINTER
#define GL_TEXTURE_COORD_ARRAY_POINTER
#define GL_INDEX_ARRAY_POINTER

void glEdgeFlagPointer(GLsizei stride, const GLvoid *pointer);
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void glNormalPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void glIndexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);

void glEnableClientState(GLenum array);
void glDisableClientState(GLenum array);

void glArrayElement(GLint i);

void glDrawArrays(GLenum mode, GLint first, GLsizei count);

void glDrawElements(GLenum mode, GLsizei count, GLenum type, GLvoid *indices);

void glInterleavedArrays(GLenum format, GLsizei stride, GLvoid *pointer);

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

#define GL_DEPTH_RANGE
#define GL_VIEWPORT

#define GL_MAX_VIEWPORT_DIMS

void glDepthRange(GLclampd n, GLclampd f);

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h);

/* Matrices */

#define GL_TEXTURE
#define GL_MODELVIEW
#define GL_PROJECTION

#define GL_MATRIX_MODE

#define GL_MODELVIEW_MATRIX
#define GL_PROJECTION_MATRIX
#define GL_TEXTURE_MATRIX

#define GL_MODELVIEW_STACK_DEPTH
#define GL_PROJECTION_STACK_DEPTH
#define GL_TEXTURE_STACK_DEPTH

#define GL_MAX_MODELVIEW_STACK_DEPTH
#define GL_MAX_PROJECTION_STACK_DEPTH
#define GL_MAX_TEXTURE_STACK_DEPTH

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

/* Texture coordinate generation */

#define GL_TEXTURE_GEN_S
#define GL_TEXTURE_GEN_T
#define GL_TEXTURE_GEN_R
#define GL_TEXTURE_GEN_Q

#define GL_TEXTURE_GEN_MODE
#define GL_OBJECT_PLANE
#define GL_EYE_PLANE

#define GL_OBJECT_LINEAR
#define GL_EYE_LINEAR
#define GL_SPHERE_MAP

void glTexGeni(GLenum coord, GLenum pname, GLint param);
void glTexGenf(GLenum coord, GLenum pname, GLfloat param);
void glTexGend(GLenum coord, GLenum pname, GLdouble param);

void glTexGeniv(GLenum coord, GLenum pname, const GLint *params);
void glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params);
void glTexGendv(GLenum coord, GLenum pname, const GLdouble *params);

/* Clipping planes */

#define GL_CLIP_PLANE0
#define GL_CLIP_PLANE1
#define GL_CLIP_PLANE2
#define GL_CLIP_PLANE3
#define GL_CLIP_PLANE4
#define GL_CLIP_PLANE5

#define GL_MAX_CLIP_PLANES 6

void glClipPlane(GLenum p, const GLdouble *eqn);

/* Raster position */

void glRasterPos2s(GLshort x, GLshort y);
void glRasterPos2i(GLint x, GLint y);
void glRasterPos2f(GLfloat x, GLfloat y);
void glRasterPos2d(GLdouble x, GLdouble y);

void glRasterPos3s(GLshort x, GLshort y, GLshort z);
void glRasterPos3i(GLint x, GLint y, GLint z);
void glRasterPos3f(GLfloat x, GLfloat y, GLfloat z);
void glRasterPos3d(GLdouble x, GLdouble y, GLdouble z);

void glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w);
void glRasterPos4i(GLint x, GLint y, GLint z, GLint w);
void glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w);

void glRasterPos2sv(const GLshort *v);
void glRasterPos2iv(const GLint *v);
void glRasterPos2fv(const GLfloat *v);
void glRasterPos2dv(const GLdouble *v);

void glRasterPos3sv(const GLshort *v);
void glRasterPos3iv(const GLint *v);
void glRasterPos3fv(const GLfloat *v);
void glRasterPos3dv(const GLdouble *v);

void glRasterPos4sv(const GLshort *v);
void glRasterPos4iv(const GLint *v);
void glRasterPos4fv(const GLfloat *v);
void glRasterPos4dv(const GLdouble *v);

/* Shading and lighting */

#define GL_COLOR_MATERIAL

#define GL_CCW
#define GL_CW

#define GL_FRONT
#define GL_BACK
#define GL_FRONT_AND_BACK

#define GL_LIGHTING

#define GL_LIGHT0
#define GL_LIGHT1
#define GL_LIGHT2
#define GL_LIGHT3
#define GL_LIGHT4
#define GL_LIGHT5
#define GL_LIGHT6
#define GL_LIGHT7

#define GL_MAX_LIGHTS

#define GL_AMBIENT
#define GL_DIFFUSE
#define GL_AMBIENT_DIFFUSE
#define GL_SPECULAR
#define GL_EMISSION
#define GL_SHININESS
#define GL_COLOR_INDEXES
#define GL_POSITION
#define GL_SPOT_DIRECTION
#define GL_SPOT_EXPONENT
#define GL_SPOT_CUTOFF
#define GL_CONSTANT_ATTENUATION
#define GL_LINEAR_ATTENUATION
#define GL_QUADRATIC_ATTENUATION
#define GL_LIGHT_MODEL_AMBIENT
#define GL_LIGHT_MODEL_LOCAL_VIEWER
#define GL_LIGHT_MODEL_TWO_SIDE

#define GL_SMOOTH
#define GL_FLAT

#define GL_SHADE_MODEL

#define GL_FRONT_FACE

#define GL_COLOR_MATERIAL_FACE
#define GL_COLOR_MATERIAL_PARAMETER

void glFrontFace(GLenum dir);

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

#define GL_POINT_SIZE
#define GL_POINT_SIZE_GRANULARITY
#define GL_POINT_SIZE_RANGE

#define GL_POINT_SMOOTH

void glPointSize(GLfloat size);

/* Lines */

#define GL_LINE_WIDTH
#define GL_LINE_WIDTH_GRANULARITY
#define GL_LINE_WIDTH_RANGE

#define GL_LINE_STIPPLE_PATTERN
#define GL_LINE_STIPPLE_REPEAT

#define GL_LINE_SMOOTH
#define GL_LINE_STIPPLE

void glLineWidth(GLfloat width);
void glLineStipple(GLint factor, GLushort pattern);

/* Polygons */

#define GL_CULL_FACE

#define GL_POINT
#define GL_LINE
#define GL_FILL

#define GL_CULL_FACE_MODE

#define GL_POLYGON_MODE
#define GL_POLYGON_OFFSET_FACTOR
#define GL_POLYGON_OFFSET_UNITS
#define GL_POLYGON_OFFSET_POINT
#define GL_POLYGON_OFFSET_LINE
#define GL_POLYGON_OFFSET_FILL
#define GL_POLYGON_SMOOTH
#define GL_POLYGON_STIPPLE

void glCullFace(GLenum mode);

void glPolygonStipple(const GLubyte *pattern);
void glPolygonMode(GLenum face, GLenum mode);
void glPolygonOffset(GLfloat factor, GLfloat units);

/* Pixel rectangles */

#define GL_UNPACK_SWAP_BYTES
#define GL_UNPACK_LSB_FIRST
#define GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_SKIP_ROWS
#define GL_UNPACK_SKIP_PIXELS
#define GL_UNPACK_ALIGNMENT

#define GL_PACK_SWAP_BYTES
#define GL_PACK_LSB_FIRST
#define GL_PACK_ROW_LENGTH
#define GL_PACK_SKIP_ROWS
#define GL_PACK_SKIP_PIXELS
#define GL_PACK_ALIGNMENT

#define GL_MAP_COLOR
#define GL_MAP_STENCIL
#define GL_INDEX_SHIFT
#define GL_INDEX_OFFSET
#define GL_RED_SCALE
#define GL_GREEN_SCALE
#define GL_BLUE_SCALE
#define GL_ALPHA_SCALE
#define GL_DEPTH_SCALE
#define GL_RED_BIAS
#define GL_GREEN_BIAS
#define GL_BLUE_BIAS
#define GL_ALPHA_BIAS
#define GL_DEPTH_BIAS

#define GL_PIXEL_MAP_I_TO_I
#define GL_PIXEL_MAP_S_TO_S
#define GL_PIXEL_MAP_I_TO_R
#define GL_PIXEL_MAP_I_TO_G
#define GL_PIXEL_MAP_I_TO_B
#define GL_PIXEL_MAP_I_TO_A
#define GL_PIXEL_MAP_R_TO_R
#define GL_PIXEL_MAP_G_TO_G
#define GL_PIXEL_MAP_B_TO_B
#define GL_PIXEL_MAP_A_TO_A

#define GL_COLOR
#define GL_STENCIL
#define GL_DEPTH

#define GL_ZOOM_X
#define GL_ZOOM_Y

#define GL_READ_BUFFER

#define GL_PIXEL_MAP_R_TO_R_SIZE
#define GL_PIXEL_MAP_G_TO_G_SIZE
#define GL_PIXEL_MAP_B_TO_B_SIZE
#define GL_PIXEL_MAP_A_TO_A_SIZE
#define GL_PIXEL_MAP_I_TO_R_SIZE
#define GL_PIXEL_MAP_I_TO_G_SIZE
#define GL_PIXEL_MAP_I_TO_B_SIZE
#define GL_PIXEL_MAP_I_TO_A_SIZE
#define GL_PIXEL_MAP_I_TO_I_SIZE
#define GL_PIXEL_MAP_S_TO_S_SIZE

#define GL_MAX_PIXEL_MAP_TABLE

void glPixelStorei(GLenum pname, GLint param);
void glPixelStoref(GLenum pname, GLfloat param);

void glPixelTransferi(GLenum pname, GLint value);
void glPixelTransferf(GLenum pname, GLfloat value);

void glPixelMapusv(GLenum map, GLsizei size, const GLushort *values);
void glPixelMapuiv(GLenum map, GLsizei size, const GLuint *values);
void glPixelMapfv(GLenum map, GLsizei size, const GLfloat *values);

void glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *data);

void glPixelZoom(GLfloat zx, GLfloat zy);

void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *data);

void glReadBuffer(GLenum src);

void glCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);

/* Bitmaps */

#define GL_BITMAP

void glBitmap(GLsizei w, GLsizei h, GLfloat xbo, GLfloat ybo, GLfloat xbi, GLfloat ybi, const GLubyte *data);

/* Texturing */

#define GL_COLOR_INDEX
#define GL_STENCIL_INDEX
#define GL_DEPTH_COMPONENT
#define GL_RED
#define GL_GREEN
#define GL_BLUE
#define GL_ALPHA
#define GL_RGB
#define GL_RGBA
#define GL_LUMINANCE
#define GL_LUMINANCE_ALPHA

#define GL_ALPHA4
#define GL_ALPHA8
#define GL_ALPHA12
#define GL_ALPHA16
#define GL_LUMINANCE4
#define GL_LUMINANCE8
#define GL_LUMINANCE12
#define GL_LUMINANCE16
#define GL_LUMINANCE4_ALPHA4
#define GL_LUMINANCE6_ALPHA2
#define GL_LUMINANCE8_ALPHA8
#define GL_LUMINANCE12_ALPHA4
#define GL_LUMINANCE12_ALPHA12
#define GL_LUMINANCE16_ALPHA16
#define GL_INTENSITY4
#define GL_INTENSITY8
#define GL_INTENSITY12
#define GL_INTENSITY16
#define GL_R3_G3_B2
#define GL_RGB4
#define GL_RGB5
#define GL_RGB8
#define GL_RGB10
#define GL_RGB12
#define GL_RGB16
#define GL_RGBA2
#define GL_RGBA4
#define GL_RGB5_A1
#define GL_RGBA8
#define GL_RGB10_A2
#define GL_RGBA12
#define GL_RGBA16

#define GL_TEXTURE_1D
#define GL_TEXTURE_2D
#define GL_PROXY_TEXTURE_1D
#define GL_PROXY_TEXTURE_2D

#define GL_TEXTURE_WRAP_S
#define GL_TEXTURE_WRAP_T
#define GL_TEXTURE_MIN_FILTER
#define GL_TEXTURE_MAG_FILTER
#define GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_PRIORITY
#define GL_TEXTURE_RESIDENT

#define GL_NEAREST
#define GL_LINEAR
#define GL_NEAREST_MIPMAP_NEAREST
#define GL_LINEAR_MIPMAP_NEAREST
#define GL_NEAREST_MIPMAP_LINEAR
#define GL_LINEAR_MIPMAP_LINEAR

#define GL_CLAMP
#define GL_REPEAT

#define GL_TEXTURE_ENV
#define GL_TEXTURE_ENV_MODE
#define GL_TEXTURE_ENV_COLOR
#define GL_REPLACE
#define GL_MODULATE
#define GL_DECAL
#define GL_BLEND

#define GL_S
#define GL_T
#define GL_R
#define GL_Q

#define GL_TEXTURE_WIDTH
#define GL_TEXTURE_HEIGHT
#define GL_TEXTURE_INTERNAL_FORMAT
#define GL_TEXTURE_BORDER
#define GL_TEXTURE_RED_SIZE
#define GL_TEXTURE_GREEN_SIZE
#define GL_TEXTURE_BLUE_SIZE
#define GL_TEXTURE_ALPHA_SIZE
#define GL_TEXTURE_LUMINANCE_SIZE
#define GL_TEXTURE_INTENSITY_SIZE

#define GL_TEXTURE_1D_BINDING
#define GL_TEXTURE_2D_BINDING

#define GL_MAX_TEXTURE_SIZE

void glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *data);
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data);

void glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
void glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);

void glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *data);
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *data);

void glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLint width);
void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);

void glTexParameteriv(GLenum target, GLenum pname, const GLint *params);
void glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params);

void glBindTexture(GLenum target, GLuint texture);
void glBindTexture(GLenum target, GLuint texture);

void glDeleteTextures(GLsizei n, const GLuint *textures);
void glGenTextures(GLsizei n, const GLuint *textures);

GLboolean glAreTexturesResident(GLsizei n, const GLuint *textures, const GLboolean *residences);

void glPrioritizeTextures(GLsizei n, const GLuint *textures, const GLclampf *priorities);

void glTexEnvi(GLenum target, GLenum pname, GLint param);
void glTexEnvf(GLenum target, GLenum pname, GLfloat param);

void glTexEnviv(GLenum target, GLenum pname, const GLint *params);
void glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params);

/* Fog */

#define GL_FOG_MODE
#define GL_FOG_DENSITY
#define GL_FOG_START
#define GL_FOG_END
#define GL_FOG_INDEX
#define GL_FOG_EXP
#define GL_FOG_EXP2
#define GL_FOG_LINEAR

#define GL_FOG_COLOR

#define GL_FOG

void glFogi(GLenum pname, GLint param);
void glFogf(GLenum pname, GLfloat param);

void glFogiv(GLenum pname, const GLint *params);
void glFogfv(GLenum pname, const GLfloat *params);

/* Scissor test */

#define GL_SCISSOR_TEST
#define GL_SCISSOR_BOX

void glScissor(GLint left, GLint bottom, GLsizei width, GLsizei height);

/* Alpha test */

#define GL_ALPHA_TEST

#define GL_NEVER
#define GL_ALWAYS
#define GL_LESS
#define GL_LEQUAL
#define GL_EQUAL
#define GL_GEQUAL
#define GL_GREATER
#define GL_NOTEQUAL

#define GL_ALPHA_TEST_FUNC
#define GL_ALPHA_TEST_REF

void glAlphaFunc(GLenum func, GLclampf ref);

/* Stencil test */

#define GL_STENCIL_TEST

#define GL_KEEP
#define GL_INCR
#define GL_DECR

#define GL_STENCIL_FUNC
#define GL_STENCIL_FAIL
#define GL_STENCIL_PASS_DEPTH_FAIL
#define GL_STENCIL_PASS_DEPTH_PASS

#define GL_STENCIL_REF
#define GL_STENCIL_VALUE_MASK

void glStencilFunc(GLenum func, GLint ref, GLuint mask);
void glStencilOp(GLenum sfail, GLenum dpfail, GLenum dppass);

/* Depth test */

#define GL_DEPTH_TEST

#define GL_DEPTH_FUNC

void glDepthFunc(GLenum func);

/* Blending */

#define GL_BLEND

#define GL_ZERO
#define GL_ONE
#define GL_DST_COLOR
#define GL_ONE_MINUS_DST_COLOR
#define GL_SRC_ALPHA
#define GL_ONE_MINUS_SRC_ALPHA
#define GL_DST_ALPHA
#define GL_ONE_MINUS_DST_ALPHA
#define GL_SRC_ALPHA_SATURATE

#define GL_BLEND_DST
#define GL_BLEND_SRC

void glBlendFunc(GLenum src, GLenum dst);

/* Logical operation */

#define GL_CLEAR
#define GL_AND
#define GL_AND_REVERSE
#define GL_COPY
#define GL_AND_INVERTED
#define GL_NOOP
#define GL_XOR
#define GL_OR
#define GL_NOR
#define GL_EQUIV
#define GL_INVERT
#define GL_OR_REVERSE
#define GL_COPY_INVERTED
#define GL_OR_INVERTED
#define GL_NAND
#define GL_SET

#define GL_LOGIC_OP
#define GL_LOGIC_OP_MODE
#define GL_INDEX_LOGIC_OP
#define GL_COLOR_LOGIC_OP

void glLogicOp(GLenum op);

/* Framebuffer selection */

#define GL_NONE
#define GL_FRONT
#define GL_BACK
#define GL_LEFT
#define GL_RIGHT
#define GL_FRONT_AND_BACK
#define GL_FRONT_LEFT
#define GL_FRONT_RIGHT
#define GL_BACK_LEFT
#define GL_BACK_RIGHT
#define GL_AUX0
#define GL_AUX1
#define GL_AUX2
#define GL_AUX3

#define GL_AUX_BUFFERS

#define GL_DRAW_BUFFER

void glDrawBuffer(GLenum buf);

/* Masks */

#define GL_INDEX_WRITEMASK
#define GL_COLOR_WRITEMASK
#define GL_DEPTH_WRITEMASK
#define GL_STENCIL_WRITEMASK

void glIndexMask(GLuint mask);
void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a);
void glDepthMask(GLboolean mask);
void glStencilMask(GLuint mask);

/* Clearing */

#define GL_COLOR_BUFFER_BIT
#define GL_DEPTH_BUFFER_BIT
#define GL_STENCIL_BUFFER_BIT
#define GL_ACCUM_BUFFER_BIT

#define GL_COLOR_CLEAR_VALUE
#define GL_DEPTH_CLEAR_VALUE
#define GL_INDEX_CLEAR_VALUE
#define GL_STENCIL_CLEAR_VALUE
#define GL_ACCUM_CLEAR_VALUE

void glClear(GLbitfield buf);

void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
void glClearIndex(GLfloat index);
void glClearDepth(GLclampd d);
void glClearStencil(GLint s);
void glClearAccum(GLfloat r, GLfloat g, GLfloat b, GLfloat a);

/* Accumulation buffer */

#define GL_ACCUM
#define GL_LOAD
#define GL_RETURN
#define GL_MULT
#define GL_ADD

#define GL_ACCUM_RED_BITS
#define GL_ACCUM_GREEN_BITS
#define GL_ACCUM_BLUE_BITS
#define GL_ACCUM_ALPHA_BITS

void glAccum(GLenum op, GLfloat value);

/* Evaluators */

#define GL_AUTO_NORMAL

#define GL_MAP1_VERTEX_3
#define GL_MAP1_VERTEX_4
#define GL_MAP1_INDEX
#define GL_MAP1_COLOR_4
#define GL_MAP1_NORMAL
#define GL_MAP1_TEXTURE_COORD_1
#define GL_MAP1_TEXTURE_COORD_2
#define GL_MAP1_TEXTURE_COORD_3
#define GL_MAP1_TEXTURE_COORD_4

#define GL_MAP2_VERTEX_3
#define GL_MAP2_VERTEX_4
#define GL_MAP2_INDEX
#define GL_MAP2_COLOR_4
#define GL_MAP2_NORMAL
#define GL_MAP2_TEXTURE_COORD_1
#define GL_MAP2_TEXTURE_COORD_2
#define GL_MAP2_TEXTURE_COORD_3
#define GL_MAP2_TEXTURE_COORD_4

#define GL_MAP1_GRID_DOMAIN
#define GL_MAP1_GRID_SEGMENTS

#define GL_MAP2_GRID_DOMAIN
#define GL_MAP2_GRID_SEGMENTS

#define GL_MAX_EVAL_ORDER

void glMap1f(GLenum type, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points);
void glMap1d(GLenum type, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points);

void glMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points);
void glMap2d(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points);

void glEvalCoord1f(GLfloat u);
void glEvalCoord1d(GLdouble u);

void glEvalCoord2f(GLfloat u, GLfloat v);
void glEvalCoord2d(GLdouble u, GLdouble v);

void glEvalCoord1fv(const GLfloat *v);
void glEvalCoord1dv(const GLdouble *v);

void glEvalCoord2fv(const GLfloat *v);
void glEvalCoord2dv(const GLdouble *v);

void glMapGrid1f(GLint n, GLfloat u1, GLfloat u2);
void glMapGrid1d(GLint n, GLdouble u1, GLdouble u2);

void glMapGrid2f(GLint nu, GLfloat u1, GLfloat u2, GLint nv, GLfloat v1, GLfloat v2);
void glMapGrid2d(GLint nu, GLdouble u1, GLdouble u2, GLint nv, GLdouble v1, GLdouble v2);

void glEvalMesh1(GLenum mode, GLint p1, GLint p2);
void glEvalMesh2(GLenum mode, GLint p1, GLint p2, GLint q1, GLint q2);

void glEvalPoint1(GLint p);
void glEvalPoint2(GLint p, GLint q);

/* Render mode */

#define GL_RENDER
#define GL_SELECT
#define GL_FEEDBACK

void glRenderMode(GLenum mode);

/* Selection */

#define GL_SELECTION_BUFFER_POINTER
#define GL_NAME_STACK_DEPTH
#define GL_MAX_NAME_STACK_DEPTH

void glInitNames(void);
void glPopName(void);
void glPushName(GLint name);
void glLoadName(GLint name);

void glSelectBuffer(GLsizei n, GLuint *buffer);

/* Feedback */

#define GL_2D
#define GL_3D
#define GL_3D_COLOR
#define GL_3D_COLOR_TEXTURE
#define GL_4D_COLOR_TEXTURE

#define GL_POINT_TOKEN
#define GL_LINE_TOKEN
#define GL_LINE_RESET_TOKEN
#define GL_POLYGON_TOKEN
#define GL_BITMAP_TOKEN
#define GL_DRAW_PIXEL_TOKEN
#define GL_COPY_PIXEL_TOKEN
#define GL_PASS_THROUGH_TOKEN

#define GL_FEEDBACK_BUFFER_POINTER

void glFeedbackBuffer(GLsizei n, GLenum type, GLfloat *buffer);

void glPassThrough(GLfloat token);

/* Display lists */

#define GL_COMPILE
#define GL_COMPILE_AND_EXECUTE

#define GL_LIST_BASE
#define GL_LIST_INDEX
#define GL_LIST_MODE

#define GL_MAX_LIST_NESTING

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

#define GL_PERSPECTIVE_CORRECTION_HINT
#define GL_POINT_SMOOTH_HINT
#define GL_LINE_SMOOTH_HINT
#define GL_POLYGON_SMOOTH_HINT
#define GL_FOG_HINT

#define GL_FASTEST
#define GL_NICEST
#define GL_DONT_CARE

void glHint(GLenum target, GLenum hint);

/* Queries */

#define GL_RED_BITS
#define GL_GREEN_BITS
#define GL_BLUE_BITS
#define GL_ALPHA_BITS
#define GL_DEPTH_BITS
#define GL_INDEX_BITS
#define GL_STENCIL_BITS

#define GL_COEFF
#define GL_ORDER
#define GL_DOMAIN

#define GL_RGBA_MODE
#define GL_INDEX_MODE

#define GL_DOUBLEBUFFER
#define GL_STEREO

#define GL_SUBPIXEL_BITS

void glGetBooleanv(GLenum value, GLboolean *data);
void glGetIntegerv(GLenum value, GLint *data);
void glGetFloatv(GLenum value, GLfloat *data);
void glGetDoublev(GLenum value, GLdouble *data);

GLboolean glIsEnabled(GLenum value);

void glGetClipPlane(GLenum plane, GLdouble *eqn);

void glGetLightiv(GLenum light, GLenum value, GLint *data);
void glGetLightfv(GLenum light, GLenum value, GLfloat *data);

void glGetMaterialiv(GLenum face, GLenum value, GLint *data);
void glGetMaterialfv(GLenum face, GLenum value, GLfloat *data);

void glGetTexEnviv(GLenum env, GLenum value, GLint *data);
void glGetTexEnvfv(GLenum env, GLenum value, GLfloat *data);

void glGetTexGeniv(GLenum coord, GLenum value, GLint *data);
void glGetTexGenfv(GLenum coord, GLenum value, GLfloat *data);

void glGetTexParameteriv(GLenum target, GLenum value, GLint *data);
void glGetTexParameterfv(GLenum target, GLenum value, GLfloat *data);

void glGetTexLevelParameteriv(GLenum target, GLint lod, GLenum value, GLint *data);
void glGetTexLevelParameterfv(GLenum target, GLint lod, GLenum value, GLfloat *data);

void glGetPixelMapusv(GLenum map, GLushort *data);
void glGetPixelMapuiv(GLenum map, GLuint *data);
void glGetPixelMapfv(GLenum map, GLfloat *data);

void glGetMapiv(GLenum map, GLenum value, GLint *data);
void glGetMapfv(GLenum map, GLenum value, GLfloat *data);
void glGetMapdv(GLenum map, GLenum value, GLdouble *data);

void glGetTexImage(GLenum tex, GLint lod, GLenum format, GLenum type, GLvoid *img);

GLboolean glIsTexture(GLuint texture);

void glGetPolygonStipple(GLvoid *pattern);

void glGetPointerv(GLenum pname, GLvoid **params);

#define GL_VENDOR
#define GL_RENDERER
#define GL_VERSION
#define GL_EXTENSIONS

GLubyte *glGetString(GLenum name);

/* Attribute stack */

#define GL_CURRENT_BIT
#define GL_ENABLE_BIT
#define GL_EVAL_BIT
#define GL_FOG_BIT
#define GL_HINT_BIT
#define GL_LIGHTING_BIT
#define GL_LINE_BIT
#define GL_LIST_BIT
#define GL_PIXEL_MODE_BIT
#define GL_POINT_BIT
#define GL_POLYGON_BIT
#define GL_POLYGON_STIPPLE_BIT
#define GL_SCISSOR_BIT
#define GL_TEXTURE_BIT
#define GL_TRANSFORM_BIT
#define GL_VIEWPORT_BIT

#define GL_CLIENT_PIXEL_STORE_BIT
#define GL_CLIENT_VERTEX_ARRAY_BIT
#define GL_CLIENT_ALL_ATTRIB_BITS

#define GL_ATTRIB_STACK_DEPTH
#define GL_CLIENT_ATTRIB_STACK_DEPTH

#define GL_MAX_ATTRIB_STACK_DEPTH
#define GL_MAX_CLIENT_ATTRIB_STACK_DEPTH

void glPushAttrib(GLbitfield mask);
void glPushClientAttrib(GLbitfield mask);

void glPopAttrib(void);
void glPopClientAttrib(void);

#ifdef __cplusplus
}
#endif


#endif
