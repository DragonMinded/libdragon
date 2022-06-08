#ifndef __LIBDRAGON_GL_H
#define __LIBDRAGON_GL_H

#include <stdint.h>

#define GL_VERSION_1_1       1
#define GL_EXT_packed_pixels 1

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

#define GL_BYTE                 0x1400
#define GL_UNSIGNED_BYTE        0x1401
#define GL_SHORT                0x1402
#define GL_UNSIGNED_SHORT       0x1403
#define GL_INT                  0x1404
#define GL_UNSIGNED_INT         0x1405
#define GL_FLOAT                0x1406
#define GL_DOUBLE               0x140A

#define GL_FALSE 0
#define GL_TRUE  1

#ifdef __cplusplus
extern "C" {
#endif

/* Errors */

#define GL_NO_ERROR             0
#define GL_INVALID_ENUM         0x0500
#define GL_INVALID_VALUE        0x0501
#define GL_INVALID_OPERATION    0x0502
#define GL_STACK_OVERFLOW       0x0503
#define GL_STACK_UNDERFLOW      0x0504
#define GL_OUT_OF_MEMORY        0x0505

GLenum glGetError(void);

/* Flags */

#define GL_DITHER               0x0BD0

void glEnable(GLenum target);
void glDisable(GLenum target);

/* Immediate mode */

#define GL_POINTS                           0x0000
#define GL_LINES                            0x0001
#define GL_LINE_LOOP                        0x0002
#define GL_LINE_STRIP                       0x0003
#define GL_TRIANGLES                        0x0004
#define GL_TRIANGLE_STRIP                   0x0005
#define GL_TRIANGLE_FAN                     0x0006
#define GL_QUADS                            0x0007
#define GL_QUAD_STRIP                       0x0008
#define GL_POLYGON                          0x0009

#define GL_NORMALIZE                        0x0BA1

#define GL_CURRENT_COLOR                    0x0B00
#define GL_CURRENT_INDEX                    0x0B01
#define GL_CURRENT_NORMAL                   0x0B02
#define GL_CURRENT_TEXTURE_COORDS           0x0B03
#define GL_CURRENT_RASTER_COLOR             0x0B04
#define GL_CURRENT_RASTER_INDEX             0x0B05
#define GL_CURRENT_RASTER_TEXTURE_COORDS    0x0B06
#define GL_CURRENT_RASTER_POSITION          0x0B07
#define GL_CURRENT_RASTER_POSITION_VALID    0x0B08
#define GL_CURRENT_RASTER_DISTANCE          0x0B09

#define GL_EDGE_FLAG                        0x0B43

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

#define GL_VERTEX_ARRAY                 0x8074
#define GL_NORMAL_ARRAY                 0x8075
#define GL_COLOR_ARRAY                  0x8076
#define GL_INDEX_ARRAY                  0x8077
#define GL_TEXTURE_COORD_ARRAY          0x8078
#define GL_EDGE_FLAG_ARRAY              0x8079

#define GL_V2F                          0x2A20
#define GL_V3F                          0x2A21
#define GL_C4UB_V2F                     0x2A22
#define GL_C4UB_V3F                     0x2A23
#define GL_C3F_V3F                      0x2A24
#define GL_N3F_V3F                      0x2A25
#define GL_C4F_N3F_V3F                  0x2A26
#define GL_T2F_V3F                      0x2A27
#define GL_T4F_V4F                      0x2A28
#define GL_T2F_C4UB_V3F                 0x2A29
#define GL_T2F_C3F_V3F                  0x2A2A
#define GL_T2F_N3F_V3F                  0x2A2B
#define GL_T2F_C4F_N3F_V3F              0x2A2C
#define GL_T4F_C4F_N3F_V4F              0x2A2D

#define GL_VERTEX_ARRAY_SIZE            0x807A
#define GL_VERTEX_ARRAY_TYPE            0x807B
#define GL_VERTEX_ARRAY_STRIDE          0x807C

#define GL_NORMAL_ARRAY_TYPE            0x807E
#define GL_NORMAL_ARRAY_STRIDE          0x807F

#define GL_COLOR_ARRAY_SIZE             0x8081
#define GL_COLOR_ARRAY_TYPE             0x8082
#define GL_COLOR_ARRAY_STRIDE           0x8083

#define GL_INDEX_ARRAY_TYPE             0x8085
#define GL_INDEX_ARRAY_STRIDE           0x8086

#define GL_TEXTURE_COORD_ARRAY_SIZE     0x8088
#define GL_TEXTURE_COORD_ARRAY_TYPE     0x8089
#define GL_TEXTURE_COORD_ARRAY_STRIDE   0x808A

#define GL_EDGE_FLAG_ARRAY_STRIDE       0x808C

#define GL_VERTEX_ARRAY_POINTER         0x808E
#define GL_NORMAL_ARRAY_POINTER         0x808F
#define GL_COLOR_ARRAY_POINTER          0x8090
#define GL_INDEX_ARRAY_POINTER          0x8091
#define GL_TEXTURE_COORD_ARRAY_POINTER  0x8092
#define GL_EDGE_FLAG_ARRAY_POINTER      0x8093

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

#define GL_DEPTH_RANGE                  0x0B70
#define GL_VIEWPORT                     0x0BA2

#define GL_MAX_VIEWPORT_DIMS            0x0D3A

void glDepthRange(GLclampd n, GLclampd f);

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h);

/* Matrices */

#define GL_MODELVIEW                    0x1700
#define GL_PROJECTION                   0x1701
#define GL_TEXTURE                      0x1702

#define GL_MATRIX_MODE                  0x0BA0

#define GL_MODELVIEW_STACK_DEPTH        0x0BA3
#define GL_PROJECTION_STACK_DEPTH       0x0BA4
#define GL_TEXTURE_STACK_DEPTH          0x0BA5

#define GL_MODELVIEW_MATRIX             0x0BA6
#define GL_PROJECTION_MATRIX            0x0BA7
#define GL_TEXTURE_MATRIX               0x0BA8

#define GL_MAX_MODELVIEW_STACK_DEPTH    0x0D36
#define GL_MAX_PROJECTION_STACK_DEPTH   0x0D38
#define GL_MAX_TEXTURE_STACK_DEPTH      0x0D39

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

#define GL_TEXTURE_GEN_S        0x0C60
#define GL_TEXTURE_GEN_T        0x0C61
#define GL_TEXTURE_GEN_R        0x0C62
#define GL_TEXTURE_GEN_Q        0x0C63

#define GL_TEXTURE_GEN_MODE     0x2500
#define GL_OBJECT_PLANE         0x2501
#define GL_EYE_PLANE            0x2502

#define GL_EYE_LINEAR           0x2400
#define GL_OBJECT_LINEAR        0x2401
#define GL_SPHERE_MAP           0x2402

void glTexGeni(GLenum coord, GLenum pname, GLint param);
void glTexGenf(GLenum coord, GLenum pname, GLfloat param);
void glTexGend(GLenum coord, GLenum pname, GLdouble param);

void glTexGeniv(GLenum coord, GLenum pname, const GLint *params);
void glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params);
void glTexGendv(GLenum coord, GLenum pname, const GLdouble *params);

/* Clipping planes */

#define GL_CLIP_PLANE0          0x3000
#define GL_CLIP_PLANE1          0x3001
#define GL_CLIP_PLANE2          0x3002
#define GL_CLIP_PLANE3          0x3003
#define GL_CLIP_PLANE4          0x3004
#define GL_CLIP_PLANE5          0x3005

#define GL_MAX_CLIP_PLANES      0x0D32

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

#define GL_LIGHTING                     0x0B50
#define GL_LIGHT_MODEL_LOCAL_VIEWER     0x0B51
#define GL_LIGHT_MODEL_TWO_SIDE         0x0B52
#define GL_LIGHT_MODEL_AMBIENT          0x0B53
#define GL_SHADE_MODEL                  0x0B54
#define GL_COLOR_MATERIAL_FACE          0x0B55
#define GL_COLOR_MATERIAL_PARAMETER     0x0B56
#define GL_COLOR_MATERIAL               0x0B57

#define GL_LIGHT0                       0x4000
#define GL_LIGHT1                       0x4001
#define GL_LIGHT2                       0x4002
#define GL_LIGHT3                       0x4003
#define GL_LIGHT4                       0x4004
#define GL_LIGHT5                       0x4005
#define GL_LIGHT6                       0x4006
#define GL_LIGHT7                       0x4007

#define GL_MAX_LIGHTS                   0x0D31

#define GL_AMBIENT                      0x1200
#define GL_DIFFUSE                      0x1201
#define GL_SPECULAR                     0x1202
#define GL_POSITION                     0x1203
#define GL_SPOT_DIRECTION               0x1204
#define GL_SPOT_EXPONENT                0x1205
#define GL_SPOT_CUTOFF                  0x1206
#define GL_CONSTANT_ATTENUATION         0x1207
#define GL_LINEAR_ATTENUATION           0x1208
#define GL_QUADRATIC_ATTENUATION        0x1209

#define GL_EMISSION                     0x1600
#define GL_SHININESS                    0x1601
#define GL_AMBIENT_AND_DIFFUSE          0x1602
#define GL_COLOR_INDEXES                0x1603

#define GL_FLAT                         0x1D00
#define GL_SMOOTH                       0x1D01


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

#define GL_POINT_SMOOTH             0x0B10
#define GL_POINT_SIZE               0x0B11
#define GL_POINT_SIZE_GRANULARITY   0x0B12
#define GL_POINT_SIZE_RANGE         0x0B13


void glPointSize(GLfloat size);

/* Lines */

#define GL_LINE_SMOOTH              0x0B20
#define GL_LINE_WIDTH               0x0B21
#define GL_LINE_WIDTH_RANGE         0x0B22
#define GL_LINE_WIDTH_GRANULARITY   0x0B23
#define GL_LINE_STIPPLE             0x0B24
#define GL_LINE_STIPPLE_PATTERN     0x0B25
#define GL_LINE_STIPPLE_REPEAT      0x0B26

void glLineWidth(GLfloat width);
void glLineStipple(GLint factor, GLushort pattern);

/* Polygons */

#define GL_POLYGON_MODE             0x0B40
#define GL_POLYGON_SMOOTH           0x0B41
#define GL_POLYGON_STIPPLE          0x0B42
#define GL_CULL_FACE                0x0B44
#define GL_CULL_FACE_MODE           0x0B45
#define GL_FRONT_FACE               0x0B46

#define GL_CW                       0x0900
#define GL_CCW                      0x0901

#define GL_POINT                    0x1B00
#define GL_LINE                     0x1B01
#define GL_FILL                     0x1B02

#define GL_POLYGON_OFFSET_UNITS     0x2A00
#define GL_POLYGON_OFFSET_POINT     0x2A01
#define GL_POLYGON_OFFSET_LINE      0x2A02
#define GL_POLYGON_OFFSET_FILL      0x8037
#define GL_POLYGON_OFFSET_FACTOR    0x8038

void glCullFace(GLenum mode);

void glFrontFace(GLenum dir);

void glPolygonStipple(const GLubyte *pattern);
void glPolygonMode(GLenum face, GLenum mode);
void glPolygonOffset(GLfloat factor, GLfloat units);

/* Pixel rectangles */

#define GL_UNPACK_SWAP_BYTES        0x0CF0
#define GL_UNPACK_LSB_FIRST         0x0CF1
#define GL_UNPACK_ROW_LENGTH        0x0CF2
#define GL_UNPACK_SKIP_ROWS         0x0CF3
#define GL_UNPACK_SKIP_PIXELS       0x0CF4
#define GL_UNPACK_ALIGNMENT         0x0CF5

#define GL_PACK_SWAP_BYTES          0x0D00
#define GL_PACK_LSB_FIRST           0x0D01
#define GL_PACK_ROW_LENGTH          0x0D02
#define GL_PACK_SKIP_ROWS           0x0D03
#define GL_PACK_SKIP_PIXELS         0x0D04
#define GL_PACK_ALIGNMENT           0x0D05


#define GL_MAP_COLOR                0x0D10
#define GL_MAP_STENCIL              0x0D11
#define GL_INDEX_SHIFT              0x0D12
#define GL_INDEX_OFFSET             0x0D13
#define GL_RED_SCALE                0x0D14
#define GL_RED_BIAS                 0x0D15
#define GL_ZOOM_X                   0x0D16
#define GL_ZOOM_Y                   0x0D17
#define GL_GREEN_SCALE              0x0D18
#define GL_GREEN_BIAS               0x0D19
#define GL_BLUE_SCALE               0x0D1A
#define GL_BLUE_BIAS                0x0D1B
#define GL_ALPHA_SCALE              0x0D1C
#define GL_ALPHA_BIAS               0x0D1D
#define GL_DEPTH_SCALE              0x0D1E
#define GL_DEPTH_BIAS               0x0D1F

#define GL_PIXEL_MAP_I_TO_I         0x0C70
#define GL_PIXEL_MAP_S_TO_S         0x0C71
#define GL_PIXEL_MAP_I_TO_R         0x0C72
#define GL_PIXEL_MAP_I_TO_G         0x0C73
#define GL_PIXEL_MAP_I_TO_B         0x0C74
#define GL_PIXEL_MAP_I_TO_A         0x0C75
#define GL_PIXEL_MAP_R_TO_R         0x0C76
#define GL_PIXEL_MAP_G_TO_G         0x0C77
#define GL_PIXEL_MAP_B_TO_B         0x0C78
#define GL_PIXEL_MAP_A_TO_A         0x0C79

#define GL_COLOR                    0x1800
#define GL_DEPTH                    0x1801
#define GL_STENCIL                  0x1802

#define GL_READ_BUFFER              0x0C02

#define GL_PIXEL_MAP_I_TO_I_SIZE    0x0CB0
#define GL_PIXEL_MAP_S_TO_S_SIZE    0x0CB1
#define GL_PIXEL_MAP_I_TO_R_SIZE    0x0CB2
#define GL_PIXEL_MAP_I_TO_G_SIZE    0x0CB3
#define GL_PIXEL_MAP_I_TO_B_SIZE    0x0CB4
#define GL_PIXEL_MAP_I_TO_A_SIZE    0x0CB5
#define GL_PIXEL_MAP_R_TO_R_SIZE    0x0CB6
#define GL_PIXEL_MAP_G_TO_G_SIZE    0x0CB7
#define GL_PIXEL_MAP_B_TO_B_SIZE    0x0CB8
#define GL_PIXEL_MAP_A_TO_A_SIZE    0x0CB9

#define GL_MAX_PIXEL_MAP_TABLE      0x0D34

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

#define GL_BITMAP   0x1234

void glBitmap(GLsizei w, GLsizei h, GLfloat xbo, GLfloat ybo, GLfloat xbi, GLfloat ybi, const GLubyte *data);

/* Texturing */

#define GL_COLOR_INDEX                  0x1900
#define GL_STENCIL_INDEX                0x1901
#define GL_DEPTH_COMPONENT              0x1902
#define GL_RED                          0x1903
#define GL_GREEN                        0x1904
#define GL_BLUE                         0x1905
#define GL_ALPHA                        0x1906
#define GL_RGB                          0x1907
#define GL_RGBA                         0x1908
#define GL_LUMINANCE                    0x1909
#define GL_LUMINANCE_ALPHA              0x190A

#define GL_R3_G3_B2                     0x2A10
#define GL_ALPHA4                       0x803B
#define GL_ALPHA8                       0x803C
#define GL_ALPHA12                      0x803D
#define GL_ALPHA16                      0x803E
#define GL_LUMINANCE4                   0x803F
#define GL_LUMINANCE8                   0x8040
#define GL_LUMINANCE12                  0x8041
#define GL_LUMINANCE16                  0x8042
#define GL_LUMINANCE4_ALPHA4            0x8043
#define GL_LUMINANCE6_ALPHA2            0x8044
#define GL_LUMINANCE8_ALPHA8            0x8045
#define GL_LUMINANCE12_ALPHA4           0x8046
#define GL_LUMINANCE12_ALPHA12          0x8047
#define GL_LUMINANCE16_ALPHA16          0x8048
#define GL_INTENSITY                    0x8049
#define GL_INTENSITY4                   0x804A
#define GL_INTENSITY8                   0x804B
#define GL_INTENSITY12                  0x804C
#define GL_INTENSITY16                  0x804D
#define GL_RGB4                         0x804F
#define GL_RGB5                         0x8050
#define GL_RGB8                         0x8051
#define GL_RGB10                        0x8052
#define GL_RGB12                        0x8053
#define GL_RGB16                        0x8054
#define GL_RGBA2                        0x8055
#define GL_RGBA4                        0x8056
#define GL_RGB5_A1                      0x8057
#define GL_RGBA8                        0x8058
#define GL_RGB10_A2                     0x8059
#define GL_RGBA12                       0x805A
#define GL_RGBA16                       0x805B

#define GL_UNSIGNED_BYTE_3_3_2_EXT      0x8032
#define GL_UNSIGNED_SHORT_4_4_4_4_EXT   0x8033
#define GL_UNSIGNED_SHORT_5_5_5_1_EXT   0x8034
#define GL_UNSIGNED_INT_8_8_8_8_EXT     0x8035
#define GL_UNSIGNED_INT_10_10_10_2_EXT  0x8036

#define GL_TEXTURE_1D                   0x0DE0
#define GL_TEXTURE_2D                   0x0DE1
#define GL_PROXY_TEXTURE_1D             0x8063
#define GL_PROXY_TEXTURE_2D             0x8064

#define GL_TEXTURE_MAG_FILTER           0x2800
#define GL_TEXTURE_MIN_FILTER           0x2801
#define GL_TEXTURE_WRAP_S               0x2802
#define GL_TEXTURE_WRAP_T               0x2803
#define GL_TEXTURE_WIDTH                0x1000
#define GL_TEXTURE_HEIGHT               0x1001
#define GL_TEXTURE_INTERNAL_FORMAT      0x1003
#define GL_TEXTURE_BORDER_COLOR         0x1004
#define GL_TEXTURE_BORDER               0x1005
#define GL_TEXTURE_RED_SIZE             0x805C
#define GL_TEXTURE_GREEN_SIZE           0x805D
#define GL_TEXTURE_BLUE_SIZE            0x805E
#define GL_TEXTURE_ALPHA_SIZE           0x805F
#define GL_TEXTURE_LUMINANCE_SIZE       0x8060
#define GL_TEXTURE_INTENSITY_SIZE       0x8061
#define GL_TEXTURE_PRIORITY             0x8066
#define GL_TEXTURE_RESIDENT             0x8067

#define GL_NEAREST                      0x2600
#define GL_LINEAR                       0x2601
#define GL_NEAREST_MIPMAP_NEAREST       0x2700
#define GL_LINEAR_MIPMAP_NEAREST        0x2701
#define GL_NEAREST_MIPMAP_LINEAR        0x2702
#define GL_LINEAR_MIPMAP_LINEAR         0x2703

#define GL_CLAMP                        0x2900
#define GL_REPEAT                       0x2901

#define GL_TEXTURE_ENV                  0x2300
#define GL_TEXTURE_ENV_MODE             0x2200
#define GL_TEXTURE_ENV_COLOR            0x2201
#define GL_MODULATE                     0x2100
#define GL_DECAL                        0x2101
#define GL_BLEND                        0x0BE2
#define GL_REPLACE                      0x1E01

#define GL_S                            0x2000
#define GL_T                            0x2001
#define GL_R                            0x2002
#define GL_Q                            0x2003

#define GL_MAX_TEXTURE_SIZE             0x0D33

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

#define GL_FOG              0x0B60
#define GL_FOG_INDEX        0x0B61
#define GL_FOG_DENSITY      0x0B62
#define GL_FOG_START        0x0B63
#define GL_FOG_END          0x0B64
#define GL_FOG_MODE         0x0B65
#define GL_FOG_COLOR        0x0B66

#define GL_EXP              0x0800
#define GL_EXP2             0x0801


void glFogi(GLenum pname, GLint param);
void glFogf(GLenum pname, GLfloat param);

void glFogiv(GLenum pname, const GLint *params);
void glFogfv(GLenum pname, const GLfloat *params);

/* Scissor test */

#define GL_SCISSOR_BOX      0x0C10
#define GL_SCISSOR_TEST     0x0C11

void glScissor(GLint left, GLint bottom, GLsizei width, GLsizei height);

/* Alpha test */

#define GL_ALPHA_TEST       0x0BC0
#define GL_ALPHA_TEST_FUNC  0x0BC1
#define GL_ALPHA_TEST_REF   0x0BC2

#define GL_NEVER            0x0200
#define GL_LESS             0x0201
#define GL_EQUAL            0x0202
#define GL_LEQUAL           0x0203
#define GL_GREATER          0x0204
#define GL_NOTEQUAL         0x0205
#define GL_GEQUAL           0x0206
#define GL_ALWAYS           0x0207

void glAlphaFunc(GLenum func, GLclampf ref);

/* Stencil test */

#define GL_STENCIL_TEST             0x0B90
#define GL_STENCIL_FUNC             0x0B92
#define GL_STENCIL_VALUE_MASK       0x0B93
#define GL_STENCIL_FAIL             0x0B94
#define GL_STENCIL_PASS_DEPTH_FAIL  0x0B95
#define GL_STENCIL_PASS_DEPTH_PASS  0x0B96
#define GL_STENCIL_REF              0x0B97

#define GL_KEEP                     0x1E00
#define GL_INCR                     0x1E02
#define GL_DECR                     0x1E03

void glStencilFunc(GLenum func, GLint ref, GLuint mask);
void glStencilOp(GLenum sfail, GLenum dpfail, GLenum dppass);

/* Depth test */

#define GL_DEPTH_TEST               0x0B71
#define GL_DEPTH_FUNC               0x0B74

void glDepthFunc(GLenum func);

/* Blending */

#define GL_BLEND_DST                0x0BE0
#define GL_BLEND_SRC                0x0BE1
#define GL_BLEND                    0x0BE2

#define GL_ZERO                     0
#define GL_ONE                      1
#define GL_SRC_COLOR                0x0300
#define GL_ONE_MINUS_SRC_COLOR      0x0301
#define GL_SRC_ALPHA                0x0302
#define GL_ONE_MINUS_SRC_ALPHA      0x0303
#define GL_DST_COLOR                0x0304
#define GL_ONE_MINUS_DST_COLOR      0x0305
#define GL_DST_ALPHA                0x0306
#define GL_ONE_MINUS_DST_ALPHA      0x0307
#define GL_SRC_ALPHA_SATURATE       0x0308

void glBlendFunc(GLenum src, GLenum dst);

/* Logical operation */

#define GL_CLEAR                    0x1500
#define GL_AND                      0x1501
#define GL_AND_REVERSE              0x1502
#define GL_COPY                     0x1503
#define GL_AND_INVERTED             0x1504
#define GL_NOOP                     0x1505
#define GL_XOR                      0x1506
#define GL_OR                       0x1507
#define GL_NOR                      0x1508
#define GL_EQUIV                    0x1509
#define GL_INVERT                   0x150A
#define GL_OR_REVERSE               0x150B
#define GL_COPY_INVERTED            0x150C
#define GL_OR_INVERTED              0x150D
#define GL_NAND                     0x150E
#define GL_SET                      0x150F

#define GL_LOGIC_OP_MODE            0x0BF0
#define GL_INDEX_LOGIC_OP           0x0BF1
#define GL_LOGIC_OP                 0x0BF1
#define GL_COLOR_LOGIC_OP           0x0BF3

void glLogicOp(GLenum op);

/* Framebuffer selection */

#define GL_NONE                     0
#define GL_FRONT_LEFT               0x0400
#define GL_FRONT_RIGHT              0x0401
#define GL_BACK_LEFT                0x0402
#define GL_BACK_RIGHT               0x0403
#define GL_FRONT                    0x0404
#define GL_BACK                     0x0405
#define GL_LEFT                     0x0406
#define GL_RIGHT                    0x0407
#define GL_FRONT_AND_BACK           0x0408
#define GL_AUX0                     0x0409
#define GL_AUX1                     0x040A
#define GL_AUX2                     0x040B
#define GL_AUX3                     0x040C

#define GL_AUX_BUFFERS              0x0C00
#define GL_DRAW_BUFFER              0x0C01

void glDrawBuffer(GLenum buf);

/* Masks */

#define GL_INDEX_WRITEMASK          0x0C21
#define GL_COLOR_WRITEMASK          0x0C23
#define GL_DEPTH_WRITEMASK          0x0B72
#define GL_STENCIL_WRITEMASK        0x0B98

void glIndexMask(GLuint mask);
void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a);
void glDepthMask(GLboolean mask);
void glStencilMask(GLuint mask);

/* Clearing */

#define GL_DEPTH_BUFFER_BIT     0x00000100
#define GL_ACCUM_BUFFER_BIT     0x00000200
#define GL_STENCIL_BUFFER_BIT   0x00000400
#define GL_COLOR_BUFFER_BIT     0x00004000

#define GL_COLOR_CLEAR_VALUE    0x0C22
#define GL_DEPTH_CLEAR_VALUE    0x0B73
#define GL_INDEX_CLEAR_VALUE    0x0C20
#define GL_STENCIL_CLEAR_VALUE  0x0B91
#define GL_ACCUM_CLEAR_VALUE    0x0B80

void glClear(GLbitfield buf);

void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
void glClearIndex(GLfloat index);
void glClearDepth(GLclampd d);
void glClearStencil(GLint s);
void glClearAccum(GLfloat r, GLfloat g, GLfloat b, GLfloat a);

/* Accumulation buffer */

#define GL_ACCUM                0x0100
#define GL_LOAD                 0x0101
#define GL_RETURN               0x0102
#define GL_MULT                 0x0103
#define GL_ADD                  0x0104

#define GL_ACCUM_RED_BITS       0x0D58
#define GL_ACCUM_GREEN_BITS     0x0D59
#define GL_ACCUM_BLUE_BITS      0x0D5A
#define GL_ACCUM_ALPHA_BITS     0x0D5B

void glAccum(GLenum op, GLfloat value);

/* Evaluators */

#define GL_AUTO_NORMAL              0x0D80

#define GL_MAP1_COLOR_4             0x0D90
#define GL_MAP1_INDEX               0x0D91
#define GL_MAP1_NORMAL              0x0D92
#define GL_MAP1_TEXTURE_COORD_1     0x0D93
#define GL_MAP1_TEXTURE_COORD_2     0x0D94
#define GL_MAP1_TEXTURE_COORD_3     0x0D95
#define GL_MAP1_TEXTURE_COORD_4     0x0D96
#define GL_MAP1_VERTEX_3            0x0D97
#define GL_MAP1_VERTEX_4            0x0D98

#define GL_MAP2_COLOR_4             0x0DB0
#define GL_MAP2_INDEX               0x0DB1
#define GL_MAP2_NORMAL              0x0DB2
#define GL_MAP2_TEXTURE_COORD_1     0x0DB3
#define GL_MAP2_TEXTURE_COORD_2     0x0DB4
#define GL_MAP2_TEXTURE_COORD_3     0x0DB5
#define GL_MAP2_TEXTURE_COORD_4     0x0DB6
#define GL_MAP2_VERTEX_3            0x0DB7
#define GL_MAP2_VERTEX_4            0x0DB8

#define GL_MAP1_GRID_DOMAIN         0x0DD0
#define GL_MAP1_GRID_SEGMENTS       0x0DD1
#define GL_MAP2_GRID_DOMAIN         0x0DD2
#define GL_MAP2_GRID_SEGMENTS       0x0DD3

#define GL_MAX_EVAL_ORDER           0x0D30

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

#define GL_RENDER           0x1C00
#define GL_FEEDBACK         0x1C01
#define GL_SELECT           0x1C02

void glRenderMode(GLenum mode);

/* Selection */

#define GL_SELECTION_BUFFER_POINTER     0x0DF3
#define GL_NAME_STACK_DEPTH             0x0D70
#define GL_MAX_NAME_STACK_DEPTH         0x0D37

void glInitNames(void);
void glPopName(void);
void glPushName(GLint name);
void glLoadName(GLint name);

void glSelectBuffer(GLsizei n, GLuint *buffer);

/* Feedback */

#define GL_2D                       0x0600
#define GL_3D                       0x0601
#define GL_3D_COLOR                 0x0602
#define GL_3D_COLOR_TEXTURE         0x0603
#define GL_4D_COLOR_TEXTURE         0x0604

#define GL_PASS_THROUGH_TOKEN       0x0700
#define GL_POINT_TOKEN              0x0701
#define GL_LINE_TOKEN               0x0702
#define GL_POLYGON_TOKEN            0x0703
#define GL_BITMAP_TOKEN             0x0704
#define GL_DRAW_PIXEL_TOKEN         0x0705
#define GL_COPY_PIXEL_TOKEN         0x0706
#define GL_LINE_RESET_TOKEN         0x0707

#define GL_FEEDBACK_BUFFER_POINTER  0x0DF0

void glFeedbackBuffer(GLsizei n, GLenum type, GLfloat *buffer);

void glPassThrough(GLfloat token);

/* Display lists */

#define GL_COMPILE                  0x1300
#define GL_COMPILE_AND_EXECUTE      0x1301

#define GL_LIST_MODE                0x0B30
#define GL_MAX_LIST_NESTING         0x0B31
#define GL_LIST_BASE                0x0B32
#define GL_LIST_INDEX               0x0B33


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

#define GL_PERSPECTIVE_CORRECTION_HINT      0x0C50
#define GL_POINT_SMOOTH_HINT                0x0C51
#define GL_LINE_SMOOTH_HINT                 0x0C52
#define GL_POLYGON_SMOOTH_HINT              0x0C53
#define GL_FOG_HINT                         0x0C54

#define GL_DONT_CARE                        0x1100
#define GL_FASTEST                          0x1101
#define GL_NICEST                           0x1102

void glHint(GLenum target, GLenum hint);

/* Queries */

#define GL_SUBPIXEL_BITS                    0x0D50
#define GL_INDEX_BITS                       0x0D51
#define GL_RED_BITS                         0x0D52
#define GL_GREEN_BITS                       0x0D53
#define GL_BLUE_BITS                        0x0D54
#define GL_ALPHA_BITS                       0x0D55
#define GL_DEPTH_BITS                       0x0D56
#define GL_STENCIL_BITS                     0x0D57

#define GL_COEFF                            0x0A00
#define GL_ORDER                            0x0A01
#define GL_DOMAIN                           0x0A02

#define GL_INDEX_MODE                       0x0C30
#define GL_RGBA_MODE                        0x0C31
#define GL_DOUBLEBUFFER                     0x0C32
#define GL_STEREO                           0x0C33


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

#define GL_VENDOR                           0x1F00
#define GL_RENDERER                         0x1F01
#define GL_VERSION                          0x1F02
#define GL_EXTENSIONS                       0x1F03

GLubyte *glGetString(GLenum name);

/* Attribute stack */

#define GL_CURRENT_BIT                      0x00000001
#define GL_POINT_BIT                        0x00000002
#define GL_LINE_BIT                         0x00000004
#define GL_POLYGON_BIT                      0x00000008
#define GL_POLYGON_STIPPLE_BIT              0x00000010
#define GL_PIXEL_MODE_BIT                   0x00000020
#define GL_LIGHTING_BIT                     0x00000040
#define GL_FOG_BIT                          0x00000080
#define GL_VIEWPORT_BIT                     0x00000800
#define GL_TRANSFORM_BIT                    0x00001000
#define GL_ENABLE_BIT                       0x00002000
#define GL_HINT_BIT                         0x00008000
#define GL_EVAL_BIT                         0x00010000
#define GL_LIST_BIT                         0x00020000
#define GL_TEXTURE_BIT                      0x00040000
#define GL_SCISSOR_BIT                      0x00080000
#define GL_ALL_ATTRIB_BITS                  0xFFFFFFFF

#define GL_CLIENT_PIXEL_STORE_BIT           0x00000001
#define GL_CLIENT_VERTEX_ARRAY_BIT          0x00000002
#define GL_CLIENT_ALL_ATTRIB_BITS           0xFFFFFFFF

#define GL_ATTRIB_STACK_DEPTH               0x0BB0
#define GL_CLIENT_ATTRIB_STACK_DEPTH        0x0BB1

#define GL_MAX_ATTRIB_STACK_DEPTH           0x0D35
#define GL_MAX_CLIENT_ATTRIB_STACK_DEPTH    0x0D36

void glPushAttrib(GLbitfield mask);
void glPushClientAttrib(GLbitfield mask);

void glPopAttrib(void);
void glPopClientAttrib(void);

#ifdef __cplusplus
}
#endif


#endif
