#include "gl_internal.h"
#include "gl_constants.h"
#include "rspq_constants.h"

extern gl_state_t state;

typedef enum {
    QUERY_BOOLEAN,
    QUERY_INTEGER,
    QUERY_FLOAT,
    QUERY_DOUBLE,
} query_type_t;

typedef void (*convert_func)(void*,const void*,uint32_t);

typedef struct {
    convert_func funcs[4];
} conversion_t;

static void convert_bool_to_boolean(GLboolean *dst, const bool *src, uint32_t count)
{
   for (uint32_t i = 0; i < count; i++) { dst[i] = src[i]; }
}
static void convert_bool_to_integer(GLint *dst, const bool *src, uint32_t count)
{
   for (uint32_t i = 0; i < count; i++) { dst[i] = src[i]; }
}
static void convert_bool_to_float(GLfloat *dst, const bool *src, uint32_t count)
{
   for (uint32_t i = 0; i < count; i++) { dst[i] = src[i]; }
}
static void convert_bool_to_double(GLdouble *dst, const bool *src, uint32_t count)
{
   for (uint32_t i = 0; i < count; i++) { dst[i] = src[i]; }
}
static const conversion_t from_bool = (conversion_t){
    .funcs = {
        (convert_func)convert_bool_to_boolean,
        (convert_func)convert_bool_to_integer,
        (convert_func)convert_bool_to_float,
        (convert_func)convert_bool_to_double,
    }
};

static void convert_u32_to_boolean(GLboolean *dst, const uint32_t *src, uint32_t count)
{
   for (uint32_t i = 0; i < count; i++) { dst[i] = src[i] == 0 ? GL_FALSE : GL_TRUE; }
}
static void convert_u32_to_integer(GLint *dst, const uint32_t *src, uint32_t count)
{
   for (uint32_t i = 0; i < count; i++) { dst[i] = src[i]; }
}
static void convert_u32_to_float(GLfloat *dst, const uint32_t *src, uint32_t count)
{
   for (uint32_t i = 0; i < count; i++) { dst[i] = src[i]; }
}
static void convert_u32_to_double(GLdouble *dst, const uint32_t *src, uint32_t count)
{
   for (uint32_t i = 0; i < count; i++) { dst[i] = src[i]; }
}
static const conversion_t from_u32 = (conversion_t){
    .funcs = {
        (convert_func)convert_u32_to_boolean,
        (convert_func)convert_u32_to_integer,
        (convert_func)convert_u32_to_float,
        (convert_func)convert_u32_to_double,
    }
};

static void convert_i32_to_boolean(GLboolean *dst, const int32_t *src, uint32_t count)
{
   for (uint32_t i = 0; i < count; i++) { dst[i] = src[i] == 0 ? GL_FALSE : GL_TRUE; }
}
static void convert_i32_to_integer(GLint *dst, const int32_t *src, uint32_t count)
{
   for (uint32_t i = 0; i < count; i++) { dst[i] = src[i]; }
}
static void convert_i32_to_float(GLfloat *dst, const int32_t *src, uint32_t count)
{
   for (uint32_t i = 0; i < count; i++) { dst[i] = src[i]; }
}
static void convert_i32_to_double(GLdouble *dst, const int32_t *src, uint32_t count)
{
   for (uint32_t i = 0; i < count; i++) { dst[i] = src[i]; }
}
static const conversion_t from_i32 = (conversion_t){
    .funcs = {
        (convert_func)convert_i32_to_boolean,
        (convert_func)convert_i32_to_integer,
        (convert_func)convert_i32_to_float,
        (convert_func)convert_i32_to_double,
    }
};

static bool gl_query_get_value_source(GLenum value, const void **src, uint32_t *count, const conversion_t **conversion)
{
    #define SET_RESULT(s, cnt, cnv) ({ \
        *src = (s); \
        *count = (cnt); \
        *conversion = (cnv); \
    })

    switch (value) {
    case GL_MAX_LIGHTS:
        static const GLint max_lights = LIGHT_COUNT;
        SET_RESULT(&max_lights, 1, &from_i32);
        break;
    case GL_MAX_LIST_NESTING:
        static const GLint max_list_nesting = RSPQ_MAX_BLOCK_NESTING_LEVEL;
        SET_RESULT(&max_list_nesting, 1, &from_i32);
        break;
    case GL_MAX_MODELVIEW_STACK_DEPTH:
        static const GLint max_modelview_stack_depth = MODELVIEW_STACK_SIZE;
        SET_RESULT(&max_modelview_stack_depth, 1, &from_i32);
        break;
    case GL_MAX_PIXEL_MAP_TABLE:
        static const GLint max_pixel_map_table = MAX_PIXEL_MAP_SIZE;
        SET_RESULT(&max_pixel_map_table, 1, &from_i32);
        break;
    case GL_MAX_PROJECTION_STACK_DEPTH:
        static const GLint max_projection_stack_depth = PROJECTION_STACK_SIZE;
        SET_RESULT(&max_projection_stack_depth, 1, &from_i32);
        break;
    case GL_MAX_TEXTURE_SIZE:
        static const GLint max_texture_size = MAX_TEXTURE_SIZE; // TODO: What is actually the correct value?
        SET_RESULT(&max_texture_size, 1, &from_i32);
        break;
    case GL_MAX_TEXTURE_STACK_DEPTH:
        static const GLint max_texture_stack_depth = TEXTURE_STACK_SIZE;
        SET_RESULT(&max_texture_stack_depth, 1, &from_i32);
        break;
    case GL_MAX_MATRIX_PALETTE_STACK_DEPTH_ARB:
        static const GLint max_matrix_palette_stack_depth = PALETTE_STACK_SIZE;
        SET_RESULT(&max_matrix_palette_stack_depth, 1, &from_i32);
        break;
    case GL_MAX_PALETTE_MATRICES_ARB:
        static const GLint max_palette_matrices = MATRIX_PALETTE_SIZE;
        SET_RESULT(&max_palette_matrices, 1, &from_i32);
        break;
    case GL_VERTEX_HALF_FIXED_PRECISION_N64:
        SET_RESULT(&state.vertex_halfx_precision.precision, 1, &from_u32);
        break;
    case GL_TEXTURE_COORD_HALF_FIXED_PRECISION_N64:
        SET_RESULT(&state.texcoord_halfx_precision.precision, 1, &from_u32);
        break;
    case GL_VERTEX_ARRAY:
        SET_RESULT(&state.array_object->arrays[ATTRIB_VERTEX].enabled, 1, &from_bool);
        break;
    case GL_VERTEX_ARRAY_SIZE:
        SET_RESULT(&state.array_object->arrays[ATTRIB_VERTEX].size, 1, &from_i32);
        break;
    case GL_VERTEX_ARRAY_STRIDE:
        SET_RESULT(&state.array_object->arrays[ATTRIB_VERTEX].stride, 1, &from_u32);
        break;
    case GL_VERTEX_ARRAY_TYPE:
        SET_RESULT(&state.array_object->arrays[ATTRIB_VERTEX].type, 1, &from_u32);
        break;
    case GL_VERTEX_ARRAY_BUFFER_BINDING_ARB:
        SET_RESULT(&state.array_object->arrays[ATTRIB_VERTEX].binding, 1, &from_u32);
        break;
    case GL_NORMAL_ARRAY:
        SET_RESULT(&state.array_object->arrays[ATTRIB_NORMAL].enabled, 1, &from_bool);
        break;
    case GL_NORMAL_ARRAY_STRIDE:
        SET_RESULT(&state.array_object->arrays[ATTRIB_NORMAL].stride, 1, &from_u32);
        break;
    case GL_NORMAL_ARRAY_TYPE:
        SET_RESULT(&state.array_object->arrays[ATTRIB_NORMAL].type, 1, &from_u32);
        break;
    case GL_NORMAL_ARRAY_BUFFER_BINDING_ARB:
        SET_RESULT(&state.array_object->arrays[ATTRIB_NORMAL].binding, 1, &from_u32);
        break;
    case GL_COLOR_ARRAY:
        SET_RESULT(&state.array_object->arrays[ATTRIB_COLOR].enabled, 1, &from_bool);
        break;
    case GL_COLOR_ARRAY_SIZE:
        SET_RESULT(&state.array_object->arrays[ATTRIB_COLOR].size, 1, &from_i32);
        break;
    case GL_COLOR_ARRAY_STRIDE:
        SET_RESULT(&state.array_object->arrays[ATTRIB_COLOR].stride, 1, &from_u32);
        break;
    case GL_COLOR_ARRAY_TYPE:
        SET_RESULT(&state.array_object->arrays[ATTRIB_COLOR].type, 1, &from_u32);
        break;
    case GL_COLOR_ARRAY_BUFFER_BINDING_ARB:
        SET_RESULT(&state.array_object->arrays[ATTRIB_COLOR].binding, 1, &from_u32);
        break;
    case GL_TEXTURE_COORD_ARRAY:
        SET_RESULT(&state.array_object->arrays[ATTRIB_TEXCOORD].enabled, 1, &from_bool);
        break;
    case GL_TEXTURE_COORD_ARRAY_SIZE:
        SET_RESULT(&state.array_object->arrays[ATTRIB_TEXCOORD].size, 1, &from_i32);
        break;
    case GL_TEXTURE_COORD_ARRAY_STRIDE:
        SET_RESULT(&state.array_object->arrays[ATTRIB_TEXCOORD].stride, 1, &from_u32);
        break;
    case GL_TEXTURE_COORD_ARRAY_TYPE:
        SET_RESULT(&state.array_object->arrays[ATTRIB_TEXCOORD].type, 1, &from_u32);
        break;
    case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB:
        SET_RESULT(&state.array_object->arrays[ATTRIB_TEXCOORD].binding, 1, &from_u32);
        break;
    case GL_MATRIX_INDEX_ARRAY_ARB:
        SET_RESULT(&state.array_object->arrays[ATTRIB_MTX_INDEX].enabled, 1, &from_bool);
        break;
    case GL_MATRIX_INDEX_ARRAY_SIZE_ARB:
        SET_RESULT(&state.array_object->arrays[ATTRIB_MTX_INDEX].size, 1, &from_i32);
        break;
    case GL_MATRIX_INDEX_ARRAY_STRIDE_ARB:
        SET_RESULT(&state.array_object->arrays[ATTRIB_MTX_INDEX].stride, 1, &from_u32);
        break;
    case GL_MATRIX_INDEX_ARRAY_TYPE_ARB:
        SET_RESULT(&state.array_object->arrays[ATTRIB_MTX_INDEX].type, 1, &from_u32);
        break;
    case GL_MATRIX_INDEX_ARRAY_BUFFER_BINDING_ARB:
        SET_RESULT(&state.array_object->arrays[ATTRIB_MTX_INDEX].binding, 1, &from_u32);
        break;
    case GL_ARRAY_BUFFER_BINDING_ARB:
        SET_RESULT(&state.array_buffer, 1, &from_u32);
        break;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB:
        SET_RESULT(&state.element_array_buffer, 1, &from_u32);
        break;
    case GL_VERTEX_ARRAY_BINDING:
        SET_RESULT(&state.array_object, 1, &from_u32);
        break;
    case GL_UNPACK_ALIGNMENT:
        SET_RESULT(&state.unpack_alignment, 1, &from_i32);
        break;
    case GL_UNPACK_LSB_FIRST:
        SET_RESULT(&state.unpack_lsb_first, 1, &from_bool);
        break;
    case GL_UNPACK_ROW_LENGTH:
        SET_RESULT(&state.unpack_row_length, 1, &from_i32);
        break;
    case GL_UNPACK_SKIP_PIXELS:
        SET_RESULT(&state.unpack_skip_pixels, 1, &from_i32);
        break;
    case GL_UNPACK_SKIP_ROWS:
        SET_RESULT(&state.unpack_skip_rows, 1, &from_i32);
        break;
    case GL_UNPACK_SWAP_BYTES:
        SET_RESULT(&state.unpack_swap_bytes, 1, &from_bool);
        break;
    case GL_PACK_ALIGNMENT:
    case GL_PACK_LSB_FIRST:
    case GL_PACK_ROW_LENGTH:
    case GL_PACK_SKIP_PIXELS:
    case GL_PACK_SKIP_ROWS:
    case GL_PACK_SWAP_BYTES:
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
    case GL_MAX_NAME_STACK_DEPTH:
    case GL_MAX_VIEWPORT_DIMS:
    case GL_MODELVIEW_MATRIX:
    case GL_MODELVIEW_STACK_DEPTH:
    case GL_NAME_STACK_DEPTH:
    case GL_NORMALIZE:
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
    case GL_MULTISAMPLE_ARB:
    case GL_SAMPLE_ALPHA_TO_COVERAGE_ARB:
    case GL_SAMPLE_ALPHA_TO_ONE_ARB:
    case GL_SAMPLE_COVERAGE_ARB:
    case GL_SAMPLE_BUFFERS_ARB:
    case GL_SAMPLES_ARB:
    case GL_SAMPLE_COVERAGE_VALUE_ARB:
    case GL_SAMPLE_COVERAGE_INVERT_ARB:
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
    case GL_TEXTURE_GEN_Q:
    case GL_TEXTURE_GEN_R:
    case GL_TEXTURE_GEN_S:
    case GL_TEXTURE_GEN_T:
    case GL_TEXTURE_MATRIX:
    case GL_TEXTURE_STACK_DEPTH:
    case GL_VIEWPORT:
    case GL_ZOOM_X:
    case GL_ZOOM_Y:
    case GL_INDEX_ARRAY_BUFFER_BINDING_ARB:
    case GL_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB:
    case GL_CLIP_PLANE0:
    case GL_CLIP_PLANE1:
    case GL_CLIP_PLANE2:
    case GL_CLIP_PLANE3:
    case GL_CLIP_PLANE4:
    case GL_CLIP_PLANE5:
    case GL_LIGHT0:
    case GL_LIGHT1:
    case GL_LIGHT2:
    case GL_LIGHT3:
    case GL_LIGHT4:
    case GL_LIGHT5:
    case GL_LIGHT6:
    case GL_LIGHT7:
        assertf(0, "querying %#04lx is not supported", value);
        return false;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid value name", value);
        return false;
    }

    return true;

    #undef SET_RESULT
}

static void gl_query_get_value(GLenum value, void *dst, query_type_t dst_type)
{
    const void *src;
    uint32_t count;
    const conversion_t *conversion;

    if (gl_query_get_value_source(value, &src, &count, &conversion))
    {
        assertf(conversion->funcs[dst_type], "Conversion function is missing");
        conversion->funcs[dst_type](dst, src, count);
    }
}

void glGetBooleanv(GLenum value, GLboolean *data)
{
    gl_query_get_value(value, data, QUERY_BOOLEAN);
}

void glGetIntegerv(GLenum value, GLint *data)
{
    gl_query_get_value(value, data, QUERY_INTEGER);
}

void glGetFloatv(GLenum value, GLfloat *data)
{
    gl_query_get_value(value, data, QUERY_FLOAT);
}

void glGetDoublev(GLenum value, GLdouble *data)
{
    gl_query_get_value(value, data, QUERY_DOUBLE);
}

GLboolean glIsEnabled(GLenum value)
{
    switch (value) {
    case GL_VERTEX_ARRAY:
        return state.array_object->arrays[ATTRIB_VERTEX].enabled;
    case GL_NORMAL_ARRAY:
        return state.array_object->arrays[ATTRIB_NORMAL].enabled;
    case GL_COLOR_ARRAY:
        return state.array_object->arrays[ATTRIB_COLOR].enabled;
    case GL_TEXTURE_COORD_ARRAY:
        return state.array_object->arrays[ATTRIB_TEXCOORD].enabled;
    case GL_MATRIX_INDEX_ARRAY_ARB:
        return state.array_object->arrays[ATTRIB_MTX_INDEX].enabled;
    case GL_ALPHA_TEST:
    case GL_AUTO_NORMAL:
    case GL_BLEND:
    case GL_CLIP_PLANE0:
    case GL_CLIP_PLANE1:
    case GL_CLIP_PLANE2:
    case GL_CLIP_PLANE3:
    case GL_CLIP_PLANE4:
    case GL_CLIP_PLANE5:
    case GL_COLOR_MATERIAL:
    case GL_CULL_FACE:
    case GL_DEPTH_TEST:
    case GL_DITHER:
    case GL_FOG:
    case GL_LIGHT0:
    case GL_LIGHT1:
    case GL_LIGHT2:
    case GL_LIGHT3:
    case GL_LIGHT4:
    case GL_LIGHT5:
    case GL_LIGHT6:
    case GL_LIGHT7:
    case GL_LIGHTING:
    case GL_LINE_SMOOTH:
    case GL_LINE_STIPPLE:
    case GL_LOGIC_OP:
    case GL_MAP1_COLOR_4:
    case GL_MAP1_INDEX:
    case GL_MAP1_NORMAL:
    case GL_MAP1_TEXTURE_COORD_1:
    case GL_MAP1_TEXTURE_COORD_2:
    case GL_MAP1_TEXTURE_COORD_3:
    case GL_MAP1_TEXTURE_COORD_4:
    case GL_MAP1_VERTEX_3:
    case GL_MAP1_VERTEX_4:
    case GL_MAP2_COLOR_4:
    case GL_MAP2_INDEX:
    case GL_MAP2_NORMAL:
    case GL_MAP2_TEXTURE_COORD_1:
    case GL_MAP2_TEXTURE_COORD_2:
    case GL_MAP2_TEXTURE_COORD_3:
    case GL_MAP2_TEXTURE_COORD_4:
    case GL_MAP2_VERTEX_3:
    case GL_MAP2_VERTEX_4:
    case GL_NORMALIZE:
    case GL_POINT_SMOOTH:
    case GL_POLYGON_SMOOTH:
    case GL_POLYGON_STIPPLE:
    case GL_SCISSOR_TEST:
    case GL_STENCIL_TEST:
    case GL_TEXTURE_1D:
    case GL_TEXTURE_2D:
    case GL_TEXTURE_GEN_Q:
    case GL_TEXTURE_GEN_R:
    case GL_TEXTURE_GEN_S:
    case GL_TEXTURE_GEN_T:
    case GL_EDGE_FLAG_ARRAY:
    case GL_INDEX_ARRAY:
        assertf(0, "querying %#04lx is not supported", value);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid value name", value);
    }
    return false;
}

void glGetPointerv(GLenum pname, GLvoid **params)
{
    switch (pname) {
    case GL_VERTEX_ARRAY_POINTER:
        *params = (GLvoid*)state.array_object->arrays[ATTRIB_VERTEX].pointer;
        break;
    case GL_NORMAL_ARRAY_POINTER:
        *params = (GLvoid*)state.array_object->arrays[ATTRIB_NORMAL].pointer;
        break;
    case GL_COLOR_ARRAY_POINTER:
        *params = (GLvoid*)state.array_object->arrays[ATTRIB_COLOR].pointer;
        break;
    case GL_TEXTURE_COORD_ARRAY_POINTER:
        *params = (GLvoid*)state.array_object->arrays[ATTRIB_TEXCOORD].pointer;
        break;
    case GL_MATRIX_INDEX_ARRAY_POINTER_ARB:
        *params = (GLvoid*)state.array_object->arrays[ATTRIB_MTX_INDEX].pointer;
        break;
    case GL_EDGE_FLAG_ARRAY_POINTER:
    case GL_INDEX_ARRAY_POINTER:
    case GL_FEEDBACK_BUFFER_POINTER:
    case GL_SELECTION_BUFFER_POINTER:
        assertf(0, "querying %#04lx is not supported", pname);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid pointer name", pname);
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
                                "GL_N64_reduced_aliasing "
                                "GL_N64_interpenetrating "
                                "GL_N64_copy_matrix "
                                "GL_N64_texture_flip";

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
