#include "gl_internal.h"
#include <n64sys.h>
#include <malloc.h>
#include <string.h>

extern gl_state_t state;


GLboolean glIsBufferARB(GLuint buffer)
{
    // FIXME: This doesn't actually guarantee that it's a valid buffer object, but just uses the heuristic of
    //        "is it somewhere in the heap memory?". This way we can at least rule out arbitrarily chosen integer constants,
    //        which used to be valid buffer IDs in legacy OpenGL.
    return is_valid_object_id(buffer);
}

void glBindBufferARB(GLenum target, GLuint buffer)
{
    if (!gl_ensure_no_begin_end()) return;
    assertf(buffer == 0 || is_valid_object_id(buffer), 
        "Not a valid buffer object: %#lx. Make sure to allocate IDs via glGenBuffersARB", buffer);

    gl_buffer_object_t *obj = (gl_buffer_object_t*)buffer;

    switch (target) {
    case GL_ARRAY_BUFFER_ARB:
        state.array_buffer = obj;
        break;
    case GL_ELEMENT_ARRAY_BUFFER_ARB:
        state.element_array_buffer = obj;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid buffer target", target);
        return;
    }
}

void gl_unbind_buffer(gl_buffer_object_t *obj, gl_buffer_object_t **binding)
{
    if (*binding == obj) {
        *binding = NULL;
    }
}

void glDeleteBuffersARB(GLsizei n, const GLuint *buffers)
{
    if (!gl_ensure_no_begin_end()) return;

    for (GLsizei i = 0; i < n; i++)
    {
        assertf(buffers[i] == 0 || is_valid_object_id(buffers[i]), 
            "Not a valid buffer object: %#lx. Make sure to allocate IDs via glGenBuffersARB", buffers[i]);

        gl_buffer_object_t *obj = (gl_buffer_object_t*)buffers[i];
        if (obj == NULL) {
            continue;
        }
        
        gl_unbind_buffer(obj, &state.array_buffer);
        gl_unbind_buffer(obj, &state.element_array_buffer);

        for (uint32_t a = 0; a < ATTRIB_COUNT; a++)
        {
            // FIXME: From the spec:
            // (2) What happens when a buffer object that is attached to a non-current
            // VAO is deleted?
            // RESOLUTION: Nothing (though a reference count may be decremented). 
            // A buffer object that is deleted while attached to a non-current VAO
            // is treated just like a buffer object bound to another context (or to
            // a current VAO in another context).
            gl_unbind_buffer(obj, &state.array_object->arrays[a].binding);
        }

        // TODO: keep alive until no longer in use

        if (obj->storage.data != NULL)
        {
            free_uncached(obj->storage.data);
        }

        free(obj);
    }
}

void glGenBuffersARB(GLsizei n, GLuint *buffers)
{
    if (!gl_ensure_no_begin_end()) return;

    for (GLsizei i = 0; i < n; i++)
    {
        gl_buffer_object_t *new_obj = calloc(sizeof(gl_buffer_object_t), 1);
        new_obj->usage = GL_STATIC_DRAW_ARB;
        new_obj->access = GL_READ_WRITE_ARB;
        buffers[i] = (GLuint)new_obj;
    }
}

bool gl_get_buffer_object(GLenum target, gl_buffer_object_t **obj)
{
    switch (target) {
    case GL_ARRAY_BUFFER_ARB:
        *obj = state.array_buffer;
        break;
    case GL_ELEMENT_ARRAY_BUFFER_ARB:
        *obj = state.element_array_buffer;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid buffer target", target);
        return false;
    }

    if (*obj == NULL) {
        gl_set_error(GL_INVALID_OPERATION, "No buffer object is currently bound");
        return false;
    }

    return true;
}

void glBufferDataARB(GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage)
{
    if (!gl_ensure_no_begin_end()) return;

    gl_buffer_object_t *obj = NULL;
    if (!gl_get_buffer_object(target, &obj)) {
        return;
    }

    switch (usage) {
    case GL_STREAM_DRAW_ARB:
    case GL_STREAM_READ_ARB:
    case GL_STREAM_COPY_ARB:
    case GL_STATIC_DRAW_ARB:
    case GL_STATIC_READ_ARB:
    case GL_STATIC_COPY_ARB:
    case GL_DYNAMIC_DRAW_ARB:
    case GL_DYNAMIC_READ_ARB:
    case GL_DYNAMIC_COPY_ARB:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid buffer usage", usage);
        return;
    }

    if (size < 0) {
        gl_set_error(GL_INVALID_VALUE, "Size must not be negative");
        return;
    }

    if (!gl_storage_resize(&obj->storage, size)) {
        gl_set_error(GL_OUT_OF_MEMORY, "Failed to allocate buffer storage");
        return;
    }

    if (data != NULL) {
        memcpy(obj->storage.data, data, size);
    }

    obj->usage = usage;
    obj->access = GL_READ_WRITE_ARB;
    obj->mapped = false;
    obj->pointer = NULL;
}

void glBufferSubDataARB(GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data)
{
    if (!gl_ensure_no_begin_end()) return;

    gl_buffer_object_t *obj = NULL;
    if (!gl_get_buffer_object(target, &obj)) {
        return;
    }

    if (obj->mapped) {
        gl_set_error(GL_INVALID_OPERATION, "The buffer object is currently mapped");
        return;
    }

    if (offset < 0) {
        gl_set_error(GL_INVALID_VALUE, "Offset must not be negative");
        return;
    }

    if ((offset >= obj->storage.size) || (offset + size > obj->storage.size)) {
        gl_set_error(GL_INVALID_VALUE, "Offset and size define a memory region that is beyond the buffer storage");
        return;
    }

    memcpy(obj->storage.data + offset, data, size);
}

void glGetBufferSubDataARB(GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid *data)
{
    if (!gl_ensure_no_begin_end()) return;

    gl_buffer_object_t *obj = NULL;
    if (!gl_get_buffer_object(target, &obj)) {
        return;
    }

    if (obj->mapped) {
        gl_set_error(GL_INVALID_OPERATION, "The buffer object is currently mapped");
        return;
    }

    if (offset < 0) {
        gl_set_error(GL_INVALID_VALUE, "Offset must not be negative");
        return;
    }

    if ((offset >= obj->storage.size) || (offset + size > obj->storage.size)) {
        gl_set_error(GL_INVALID_VALUE, "Offset and size define a memory region that is beyond the buffer storage");
        return;
    }

    memcpy(data, obj->storage.data + offset, size);
}

GLvoid * glMapBufferARB(GLenum target, GLenum access)
{
    if (!gl_ensure_no_begin_end()) return 0;

    gl_buffer_object_t *obj = NULL;
    if (!gl_get_buffer_object(target, &obj)) {
        return NULL;
    }

    switch (access) {
    case GL_READ_ONLY_ARB:
    case GL_WRITE_ONLY_ARB:
    case GL_READ_WRITE_ARB:
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid buffer access", access);
        return NULL;
    }

    if (obj->mapped) {
        gl_set_error(GL_INVALID_OPERATION, "The buffer object is already mapped");
        return NULL;
    }

    obj->access = access;
    obj->mapped = true;
    obj->pointer = obj->storage.data;

    return obj->pointer;
}

GLboolean glUnmapBufferARB(GLenum target)
{
    if (!gl_ensure_no_begin_end()) return 0;

    gl_buffer_object_t *obj = NULL;
    if (!gl_get_buffer_object(target, &obj)) {
        return GL_FALSE;
    }

    if (!obj->mapped) {
        gl_set_error(GL_INVALID_OPERATION, "The buffer object has not been mapped");
        return GL_FALSE;
    }

    obj->mapped = false;
    obj->pointer = NULL;

    return GL_TRUE;
}

void glGetBufferParameterivARB(GLenum target, GLenum pname, GLint *params)
{
    if (!gl_ensure_no_begin_end()) return;

    gl_buffer_object_t *obj = NULL;
    if (!gl_get_buffer_object(target, &obj)) {
        return;
    }

    switch (pname) {
    case GL_BUFFER_SIZE_ARB:
        *params = obj->storage.size;
        break;
    case GL_BUFFER_USAGE_ARB:
        *params = obj->usage;
        break;
    case GL_BUFFER_ACCESS_ARB:
        *params = obj->access;
        break;
    case GL_BUFFER_MAPPED_ARB:
        *params = obj->mapped;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid buffer parameter", pname);
        return;
    }
}

void glGetBufferPointervARB(GLenum target, GLenum pname, GLvoid **params)
{
    if (!gl_ensure_no_begin_end()) return;

    gl_buffer_object_t *obj = NULL;
    if (!gl_get_buffer_object(target, &obj)) {
        return;
    }

    if (pname != GL_BUFFER_MAP_POINTER_ARB) {
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid buffer pointer", pname);
        return;
    }

    *params = obj->pointer;
}
