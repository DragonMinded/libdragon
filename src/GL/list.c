#include "gl_internal.h"
#include "rspq.h"

#define EMPTY_LIST ((rspq_block_t*)1)

extern gl_state_t state;

typedef GLuint (*read_list_id_func)(const GLvoid*, GLsizei);

inline bool is_non_empty_list(rspq_block_t *block)
{
    return block != NULL && block != EMPTY_LIST;
}

void block_free_safe(rspq_block_t *block)
{
    // Silently ignore NULL and EMPTY_LIST
    if (!is_non_empty_list(block)) return;
    rdpq_call_deferred((void (*)(void*))rspq_block_free, block);
}

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
        block_free_safe((rspq_block_t*)list_iter.value);
    }

    obj_map_free(&state.list_objects);
}

void glNewList(GLuint n, GLenum mode)
{
    if (!gl_ensure_no_begin_end()) return;
    
    if (n == 0) {
        gl_set_error(GL_INVALID_VALUE, "Display list ID must not be 0");
        return;
    }

    switch (mode) {
    case GL_COMPILE:
        break;
    case GL_COMPILE_AND_EXECUTE:
        assertf(0, "Compile and execute is not supported!");
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid display list compilation mode", mode);
        return;
    }

    if (state.current_list != 0) {
        gl_set_error(GL_INVALID_OPERATION, "A display list is already being recorded");
        return;
    }

    state.current_list = n;

    rspq_block_begin();
}

void glEndList(void)
{
    if (!gl_ensure_no_begin_end()) return;
    
    if (state.current_list == 0) {
        gl_set_error(GL_INVALID_OPERATION, "No display list is currently being recorded");
        return;
    }

    rspq_block_t *block = rspq_block_end();

    block = obj_map_set(&state.list_objects, state.current_list, block);
    block_free_safe(block);

    state.current_list = 0;
}

void glCallList(GLuint n)
{
    // The spec allows glCallList in within glBegin/glEnd pairs, but our current architecture doesn't allow for this.
    // During display list recording, we cannot anticipate whether it will be called within a glBegin/glEnd pair or not.
    assertf(!state.begin_end_active, "glCallList between glBegin/glEnd is not supported!");

    rspq_block_t *block = obj_map_get(&state.list_objects, n);
    // Silently ignore NULL and EMPTY_LIST
    if (is_non_empty_list(block)) {
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

read_list_id_func get_read_list_id_func(GLenum type)
{
    switch (type) {
    case GL_BYTE:
        return gl_get_list_name_byte;
    case GL_UNSIGNED_BYTE:
        return gl_get_list_name_ubyte;
    case GL_SHORT:
        return gl_get_list_name_short;
    case GL_UNSIGNED_SHORT:
        return gl_get_list_name_ushort;
    case GL_INT:
        return gl_get_list_name_int;
    case GL_UNSIGNED_INT:
        return gl_get_list_name_uint;
    case GL_FLOAT:
        return gl_get_list_name_float;
    case GL_2_BYTES:
        return gl_get_list_name_2bytes;
    case GL_3_BYTES:
        return gl_get_list_name_3bytes;
    case GL_4_BYTES:
        return gl_get_list_name_4bytes;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid display list ID type", type);
        return NULL;
    }
}

void glCallLists(GLsizei n, GLenum type, const GLvoid *lists)
{
    // See glCallList for an explanation
    assertf(!state.begin_end_active, "glCallLists between glBegin/glEnd is not supported!");

    read_list_id_func func = get_read_list_id_func(type);
    if (func == NULL) return;

    for (GLsizei i = 0; i < n; i++)
    {
        GLuint l = func(lists, i);
        glCallList(l + state.list_base);
    }
}

void glListBase(GLuint base)
{
    if (!gl_ensure_no_begin_end()) return;
    
    state.list_base = base;
}

GLuint glGenLists(GLsizei s)
{
    if (!gl_ensure_no_begin_end()) return 0;

    if (s == 0) return 0;

    GLuint result = state.next_list_name;

    // Set newly used indices to empty lists (which marks them as used without actually creating a block)
    for (size_t i = 0; i < s; i++)
    {
        obj_map_set(&state.list_objects, state.next_list_name++, EMPTY_LIST);
    }
    
    return result;
}

GLboolean glIsList(GLuint list)
{
    if (!gl_ensure_no_begin_end()) return 0;
    
    // We do not check for EMPTY_LIST here because that also denotes a used list index
    return obj_map_get(&state.list_objects, list) != NULL;
}

void glDeleteLists(GLuint list, GLsizei range)
{
    if (!gl_ensure_no_begin_end()) return;
    
    for (GLuint i = 0; i < range; i++)
    {
        rspq_block_t *block = obj_map_remove(&state.list_objects, list + i);
        block_free_safe(block);
    }
}
