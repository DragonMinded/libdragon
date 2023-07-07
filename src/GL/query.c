#include "gl_internal.h"

extern gl_state_t state;

/*
static uint32_t max_texture_size;
typedef enum {
    CONVERT_BOOL,
    CONVERT_INT,
    CONVERT_FLOAT,
    CONVERT_DOUBLE,
} convert_target_t;

typedef void (*convert_func)(void*,void*,uint32_t);

//static convert_func from_bool[4];
//static convert_func from_u8[4];
//static convert_func from_i8[4];
//static convert_func from_u16[4];
//static convert_func from_i16[4];
//static convert_func from_u32[4];
//static convert_func from_i32[4];
//static convert_func from_float[4];
//static convert_func from_double[4];
//static convert_func from_clampf[4];
//static convert_func from_clampd[4];

void gl_get_values(GLenum value, void *data, convert_target_t target_type)
{
    void *src;
    convert_func *conv = NULL;
    uint32_t count = 1;

    switch (value) {
    case GL_ACCUM_ALPHA_BITS:
    case GL_ACCUM_BLUE_BITS:
    case GL_ACCUM_CLEAR_VALUE:
    case GL_ACCUM_GREEN_BITS:
    case GL_ACCUM_RED_BITS:
    case GL_ALPHA_BIAS:
    case GL_ALPHA_BITS:
    case GL_ALPHA_SCALE:
    case GL_ALPHA_TEST:
    case GL_ALPHA_TEST_FUNC:
    case GL_ALPHA_TEST_REF:
    case GL_ATTRIB_STACK_DEPTH:
    case GL_AUTO_NORMAL:
    case GL_AUX_BUFFERS:
    case GL_BLEND:
    case GL_BLEND_DST:
    case GL_BLEND_SRC:
    case GL_BLUE_BIAS:
    case GL_BLUE_BITS:
    case GL_BLUE_SCALE:
    case GL_CLIENT_ATTRIB_STACK_DEPTH:
    case GL_COLOR_ARRAY:
    case GL_COLOR_ARRAY_SIZE:
    case GL_COLOR_ARRAY_STRIDE:
    case GL_COLOR_ARRAY_TYPE:
    case GL_COLOR_CLEAR_VALUE:
    case GL_COLOR_LOGIC_OP:
    case GL_COLOR_MATERIAL:
    case GL_COLOR_MATERIAL_FACE:
    case GL_COLOR_MATERIAL_PARAMETER:
    case GL_COLOR_WRITEMASK:
    case GL_CULL_FACE:
    case GL_CULL_FACE_MODE:
    case GL_CURRENT_COLOR:
    case GL_CURRENT_INDEX:
    case GL_CURRENT_NORMAL:
    case GL_CURRENT_RASTER_COLOR:
    case GL_CURRENT_RASTER_DISTANCE:
    case GL_CURRENT_RASTER_INDEX:
    case GL_CURRENT_RASTER_POSITION:
    case GL_CURRENT_RASTER_POSITION_VALID:
    case GL_CURRENT_RASTER_TEXTURE_COORDS:
    case GL_CURRENT_TEXTURE_COORDS:
    case GL_DEPTH_BIAS:
    case GL_DEPTH_BITS:
    case GL_DEPTH_CLEAR_VALUE:
    case GL_DEPTH_FUNC:
    case GL_DEPTH_RANGE:
    case GL_DEPTH_SCALE:
    case GL_DEPTH_TEST:
    case GL_DEPTH_WRITEMASK:
    case GL_DITHER:
    case GL_DOUBLEBUFFER:
    case GL_DRAW_BUFFER:
    case GL_EDGE_FLAG:
    case GL_EDGE_FLAG_ARRAY:
    case GL_EDGE_FLAG_ARRAY_STRIDE:
    case GL_FOG:
    case GL_FOG_COLOR:
    case GL_FOG_DENSITY:
    case GL_FOG_END:
    case GL_FOG_HINT:
    case GL_FOG_INDEX:
    case GL_FOG_MODE:
    case GL_FOG_START:
    case GL_FRONT_FACE:
    case GL_GREEN_BIAS:
    case GL_GREEN_BITS:
    case GL_GREEN_SCALE:
    case GL_INDEX_ARRAY:
    case GL_INDEX_ARRAY_STRIDE:
    case GL_INDEX_ARRAY_TYPE:
    case GL_INDEX_BITS:
    case GL_INDEX_CLEAR_VALUE:
    case GL_INDEX_LOGIC_OP:
    case GL_INDEX_MODE:
    case GL_INDEX_OFFSET:
    case GL_INDEX_SHIFT:
    case GL_INDEX_WRITEMASK:
    case GL_LIGHTING:
    case GL_LIGHT_MODEL_AMBIENT:
    case GL_LIGHT_MODEL_LOCAL_VIEWER:
    case GL_LIGHT_MODEL_TWO_SIDE:
    case GL_LINE_SMOOTH:
    case GL_LINE_SMOOTH_HINT:
    case GL_LINE_STIPPLE:
    case GL_LINE_STIPPLE_PATTERN:
    case GL_LINE_STIPPLE_REPEAT:
    case GL_LINE_WIDTH:
    case GL_LINE_WIDTH_GRANULARITY:
    case GL_LINE_WIDTH_RANGE:
    case GL_LIST_BASE:
    case GL_LIST_INDEX:
    case GL_LIST_MODE:
    case GL_LOGIC_OP_MODE:
    case GL_MAP1_COLOR_4:
    case GL_MAP1_GRID_DOMAIN:
    case GL_MAP1_GRID_SEGMENTS:
    case GL_MAP1_INDEX:
    case GL_MAP1_NORMAL:
    case GL_MAP1_TEXTURE_COORD_1:
    case GL_MAP1_TEXTURE_COORD_2:
    case GL_MAP1_TEXTURE_COORD_3:
    case GL_MAP1_TEXTURE_COORD_4:
    case GL_MAP1_VERTEX_3:
    case GL_MAP1_VERTEX_4:
    case GL_MAP2_COLOR_4:
    case GL_MAP2_GRID_DOMAIN:
    case GL_MAP2_GRID_SEGMENTS:
    case GL_MAP2_INDEX:
    case GL_MAP2_NORMAL:
    case GL_MAP2_TEXTURE_COORD_1:
    case GL_MAP2_TEXTURE_COORD_2:
    case GL_MAP2_TEXTURE_COORD_3:
    case GL_MAP2_TEXTURE_COORD_4:
    case GL_MAP2_VERTEX_3:
    case GL_MAP2_VERTEX_4:
    case GL_MAP_COLOR:
    case GL_MAP_STENCIL:
    case GL_MATRIX_MODE:
    case GL_MAX_CLIENT_ATTRIB_STACK_DEPTH:
    case GL_MAX_ATTRIB_STACK_DEPTH:
    case GL_MAX_CLIP_PLANES:
    case GL_MAX_EVAL_ORDER:
    case GL_MAX_LIGHTS:
    case GL_MAX_LIST_NESTING:
    case GL_MAX_MODELVIEW_STACK_DEPTH:
    case GL_MAX_NAME_STACK_DEPTH:
    case GL_MAX_PIXEL_MAP_TABLE:
    case GL_MAX_PROJECTION_STACK_DEPTH:
    case GL_MAX_TEXTURE_SIZE:
        src = &max_texture_size;
        //conv = from_u32;
        break;
    case GL_MAX_TEXTURE_STACK_DEPTH:
    case GL_MAX_VIEWPORT_DIMS:
    case GL_MODELVIEW_MATRIX:
    case GL_MODELVIEW_STACK_DEPTH:
    case GL_NAME_STACK_DEPTH:
    case GL_NORMAL_ARRAY:
    case GL_NORMAL_ARRAY_STRIDE:
    case GL_NORMAL_ARRAY_TYPE:
    case GL_NORMALIZE:
    case GL_PACK_ALIGNMENT:
    case GL_PACK_LSB_FIRST:
    case GL_PACK_ROW_LENGTH:
    case GL_PACK_SKIP_PIXELS:
    case GL_PACK_SKIP_ROWS:
    case GL_PACK_SWAP_BYTES:
    case GL_PERSPECTIVE_CORRECTION_HINT:
    case GL_PIXEL_MAP_A_TO_A_SIZE:
    case GL_PIXEL_MAP_B_TO_B_SIZE:
    case GL_PIXEL_MAP_G_TO_G_SIZE:
    case GL_PIXEL_MAP_I_TO_A_SIZE:
    case GL_PIXEL_MAP_I_TO_B_SIZE:
    case GL_PIXEL_MAP_I_TO_G_SIZE:
    case GL_PIXEL_MAP_I_TO_I_SIZE:
    case GL_PIXEL_MAP_I_TO_R_SIZE:
    case GL_PIXEL_MAP_R_TO_R_SIZE:
    case GL_PIXEL_MAP_S_TO_S_SIZE:
    case GL_POINT_SIZE:
    case GL_POINT_SIZE_GRANULARITY:
    case GL_POINT_SIZE_RANGE:
    case GL_POINT_SMOOTH:
    case GL_POINT_SMOOTH_HINT:
    case GL_POLYGON_MODE:
    case GL_POLYGON_OFFSET_FACTOR:
    case GL_POLYGON_OFFSET_UNITS:
    case GL_POLYGON_OFFSET_FILL:
    case GL_POLYGON_OFFSET_LINE:
    case GL_POLYGON_OFFSET_POINT:
    case GL_POLYGON_SMOOTH:
    case GL_POLYGON_SMOOTH_HINT:
    case GL_POLYGON_STIPPLE:
    case GL_PROJECTION_MATRIX:
    case GL_PROJECTION_STACK_DEPTH:
    case GL_READ_BUFFER:
    case GL_RED_BIAS:
    case GL_RED_BITS:
    case GL_RED_SCALE:
    case GL_RENDER_MODE:
    case GL_RGBA_MODE:
    case GL_SCISSOR_BOX:
    case GL_SCISSOR_TEST:
    case GL_SHADE_MODEL:
    case GL_STENCIL_BITS:
    case GL_STENCIL_CLEAR_VALUE:
    case GL_STENCIL_FAIL:
    case GL_STENCIL_FUNC:
    case GL_STENCIL_PASS_DEPTH_FAIL:
    case GL_STENCIL_PASS_DEPTH_PASS:
    case GL_STENCIL_REF:
    case GL_STENCIL_TEST:
    case GL_STENCIL_VALUE_MASK:
    case GL_STENCIL_WRITEMASK:
    case GL_STEREO:
    case GL_SUBPIXEL_BITS:
    case GL_TEXTURE_1D:
    case GL_TEXTURE_2D:
    case GL_TEXTURE_COORD_ARRAY:
    case GL_TEXTURE_COORD_ARRAY_SIZE:
    case GL_TEXTURE_COORD_ARRAY_STRIDE:
    case GL_TEXTURE_COORD_ARRAY_TYPE:
    case GL_TEXTURE_GEN_Q:
    case GL_TEXTURE_GEN_R:
    case GL_TEXTURE_GEN_S:
    case GL_TEXTURE_GEN_T:
    case GL_TEXTURE_MATRIX:
    case GL_TEXTURE_STACK_DEPTH:
    case GL_UNPACK_ALIGNMENT:
    case GL_UNPACK_LSB_FIRST:
    case GL_UNPACK_ROW_LENGTH:
    case GL_UNPACK_SKIP_PIXELS:
    case GL_UNPACK_SKIP_ROWS:
    case GL_UNPACK_SWAP_BYTES:
    case GL_VERTEX_ARRAY:
    case GL_VERTEX_ARRAY_SIZE:
    case GL_VERTEX_ARRAY_STRIDE:
    case GL_VERTEX_ARRAY_TYPE:
    case GL_VIEWPORT:
    case GL_ZOOM_X:
    case GL_ZOOM_Y:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    conv[target_type](data, src, count);
}
*/
void glGetBooleanv(GLenum value, GLboolean *data)
{
    switch (value) {
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx cannot be queried with this function", value);
        break;
    }
}

void glGetIntegerv(GLenum value, GLint *data)
{
    switch (value) {
    case GL_VERTEX_HALF_FIXED_PRECISION_N64:
        data[0] = state.vertex_halfx_precision.precision;
        break;
    case GL_TEXTURE_COORD_HALF_FIXED_PRECISION_N64:
        data[0] = state.texcoord_halfx_precision.precision;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx cannot be queried with this function", value);
        break;
    }
}

void glGetFloatv(GLenum value, GLfloat *data)
{
    switch (value) {
    case GL_VERTEX_HALF_FIXED_PRECISION_N64:
        data[0] = state.vertex_halfx_precision.precision;
        break;
    case GL_TEXTURE_COORD_HALF_FIXED_PRECISION_N64:
        data[0] = state.texcoord_halfx_precision.precision;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx cannot be queried with this function", value);
        break;
    }
}

void glGetDoublev(GLenum value, GLdouble *data)
{
    switch (value) {
    case GL_VERTEX_HALF_FIXED_PRECISION_N64:
        data[0] = state.vertex_halfx_precision.precision;
        break;
    case GL_TEXTURE_COORD_HALF_FIXED_PRECISION_N64:
        data[0] = state.texcoord_halfx_precision.precision;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx cannot be queried with this function", value);
        break;
    }
}

static const char *extensions = "GL_ARB_multisample "
                                "GL_EXT_packed_pixels "
                                "GL_ARB_vertex_buffer_object "
                                "GL_ARB_texture_mirrored_repeat "
                                "GL_ARB_texture_non_power_of_two "
                                "GL_ARB_vertex_array_object "
                                "GL_ARB_matrix_palette "
                                "GL_N64_RDPQ_interop "
                                "GL_N64_surface_image "
                                "GL_N64_half_fixed_point "
                                "GL_N64_reduced_aliasing";

GLubyte *glGetString(GLenum name)
{
    switch (name) {
    case GL_VENDOR:
        return (GLubyte*)"Libdragon";
    case GL_RENDERER:
        return (GLubyte*)"N64";
    case GL_VERSION:
        return (GLubyte*)"1.1";
    case GL_EXTENSIONS:
        return (GLubyte*)extensions;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid string name", name);
        return NULL;
    }
}
