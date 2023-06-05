#include "gl_internal.h"
#include "rspq.h"

extern gl_state_t state;

void gl_list_init()
{
    // TODO: Get rid of the hash map. This will be difficult due to the semantics of glGenLists (it's guaranteed to generate consecutive IDs)
    obj_map_new(&state.list_objects);
    state.next_list_name = 1;
}

void gl_list_close()
{
    obj_map_iter_t list_iter = obj_map_iterator(&state.list_objects);
    while (obj_map_iterator_next(&list_iter)) {
        rspq_block_free((rspq_block_t*)list_iter.value);
    }

    obj_map_free(&state.list_objects);
}

void glNewList(GLuint n, GLenum mode)
{
    if (!gl_ensure_no_immediate()) return;
    
    if (n == 0) {
        gl_set_error(GL_INVALID_VALUE);
        return;
    }

    switch (mode) {
    case GL_COMPILE:
        break;
    case GL_COMPILE_AND_EXECUTE:
        assertf(0, "Compile and execute is not supported!");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    if (state.current_list != 0) {
        gl_set_error(GL_INVALID_OPERATION);
        return;
    }

    state.current_list = n;

    rspq_block_begin();
}

void glEndList(void)
{
    if (!gl_ensure_no_immediate()) return;
    
    if (state.current_list == 0) {
        gl_set_error(GL_INVALID_OPERATION);
        return;
    }

    rspq_block_t *block = rspq_block_end();

    block = obj_map_set(&state.list_objects, state.current_list, block);

    if (block != NULL) {
        rspq_block_free(block);
    }

    state.current_list = 0;
}

void glCallList(GLuint n)
{
    // The spec allows glCallList in immediate mode, but our current architecture doesn't allow for this.
    // During display list recording, we cannot anticipate whether it will be called during immediate mode or not.
    assertf(!state.immediate_active, "glCallList between glBegin/glEnd is not supported!");

    rspq_block_t *block = obj_map_get(&state.list_objects, n);
    if (block != NULL) {
        rspq_block_run(block);
    }
}

GLuint gl_get_list_name_byte(const GLvoid *lists, GLsizei n)
{
    return ((const GLbyte*)lists)[n];
}

GLuint gl_get_list_name_ubyte(const GLvoid *lists, GLsizei n)
{
    return ((const GLubyte*)lists)[n];
}

GLuint gl_get_list_name_short(const GLvoid *lists, GLsizei n)
{
    return ((const GLshort*)lists)[n];
}

GLuint gl_get_list_name_ushort(const GLvoid *lists, GLsizei n)
{
    return ((const GLushort*)lists)[n];
}

GLuint gl_get_list_name_int(const GLvoid *lists, GLsizei n)
{
    return ((const GLint*)lists)[n];
}

GLuint gl_get_list_name_uint(const GLvoid *lists, GLsizei n)
{
    return ((const GLuint*)lists)[n];
}

GLuint gl_get_list_name_float(const GLvoid *lists, GLsizei n)
{
    return ((const GLfloat*)lists)[n];
}

GLuint gl_get_list_name_2bytes(const GLvoid *lists, GLsizei n)
{
    GLubyte l0 = ((const GLubyte*)lists)[n*2+0];
    GLubyte l1 = ((const GLubyte*)lists)[n*2+1];
    return ((GLuint)l0) * 255 + ((GLuint)l1);
}

GLuint gl_get_list_name_3bytes(const GLvoid *lists, GLsizei n)
{
    GLubyte l0 = ((const GLubyte*)lists)[n*3+0];
    GLubyte l1 = ((const GLubyte*)lists)[n*3+1];
    GLubyte l2 = ((const GLubyte*)lists)[n*3+2];
    return ((GLuint)l0) * 65536 + ((GLuint)l1) * 255 + ((GLuint)l2);
}

GLuint gl_get_list_name_4bytes(const GLvoid *lists, GLsizei n)
{
    GLubyte l0 = ((const GLubyte*)lists)[n*4+0];
    GLubyte l1 = ((const GLubyte*)lists)[n*4+1];
    GLubyte l2 = ((const GLubyte*)lists)[n*4+2];
    GLubyte l3 = ((const GLubyte*)lists)[n*4+3];
    return ((GLuint)l0) * 16777216 + ((GLuint)l1) * 65536 + ((GLuint)l2) * 255 + ((GLuint)l3);
}

void glCallLists(GLsizei n, GLenum type, const GLvoid *lists)
{
    // See glCallList for an explanation
    assertf(!state.immediate_active, "glCallLists between glBegin/glEnd is not supported!");
    GLuint (*func)(const GLvoid*, GLsizei);

    switch (type) {
    case GL_BYTE:
        func = gl_get_list_name_byte;
        break;
    case GL_UNSIGNED_BYTE:
        func = gl_get_list_name_ubyte;
        break;
    case GL_SHORT:
        func = gl_get_list_name_short;
        break;
    case GL_UNSIGNED_SHORT:
        func = gl_get_list_name_ushort;
        break;
    case GL_INT:
        func = gl_get_list_name_int;
        break;
    case GL_UNSIGNED_INT:
        func = gl_get_list_name_uint;
        break;
    case GL_FLOAT:
        func = gl_get_list_name_float;
        break;
    case GL_2_BYTES:
        func = gl_get_list_name_2bytes;
        break;
    case GL_3_BYTES:
        func = gl_get_list_name_3bytes;
        break;
    case GL_4_BYTES:
        func = gl_get_list_name_4bytes;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    for (GLsizei i = 0; i < n; i++)
    {
        GLuint l = func(lists, i);
        glCallList(l + state.list_base);
    }
}

void glListBase(GLuint base)
{
    if (!gl_ensure_no_immediate()) return;
    
    state.list_base = base;
}

GLuint glGenLists(GLsizei s)
{
    if (!gl_ensure_no_immediate()) return 0;
    
    GLuint result = state.next_list_name;
    state.next_list_name += s;
    return result;
}

GLboolean glIsList(GLuint list)
{
    if (!gl_ensure_no_immediate()) return 0;
    
    return obj_map_get(&state.list_objects, list) != NULL;
}

void glDeleteLists(GLuint list, GLsizei range)
{
    if (!gl_ensure_no_immediate()) return;
    
    for (GLuint i = 0; i < range; i++)
    {
        rspq_block_t *block = obj_map_remove(&state.list_objects, list + i);
        if (block != NULL) {
            rspq_block_free(block);
        }
    }
}
