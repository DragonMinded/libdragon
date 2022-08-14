#include "gl_internal.h"
#include <n64sys.h>
#include <malloc.h>
#include <string.h>

extern gl_state_t state;

void gl_buffer_object_init(gl_buffer_object_t *obj, GLuint name)
{
    memset(obj, 0, sizeof(gl_buffer_object_t));

    obj->name = name;
    obj->usage = GL_STATIC_DRAW_ARB;
    obj->access = GL_READ_WRITE_ARB;
}

void gl_buffer_object_free(gl_buffer_object_t *obj)
{
    if (obj->data != NULL)
    {
        free_uncached(obj->data);
    }

    free(obj);
}

void gl_buffer_init()
{
    obj_map_new(&state.buffer_objects);
    state.next_buffer_name = 1;
}

void gl_buffer_close()
{
    obj_map_iter_t buffer_iter = obj_map_iterator(&state.buffer_objects);
    while (obj_map_iterator_next(&buffer_iter)) {
        gl_buffer_object_free((gl_buffer_object_t*)buffer_iter.value);
    }

    obj_map_free(&state.buffer_objects);
}

void glBindBufferARB(GLenum target, GLuint buffer)
{
    gl_buffer_object_t **obj = NULL;

    switch (target) {
    case GL_ARRAY_BUFFER_ARB:
        obj = &state.array_buffer;
        break;
    case GL_ELEMENT_ARRAY_BUFFER_ARB:
        obj = &state.element_array_buffer;
        break;
    default:
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    if (buffer == 0) {
        *obj = NULL;
        return;
    }

    *obj = obj_map_get(&state.buffer_objects, buffer);

    if (*obj == NULL) {
        *obj = malloc(sizeof(gl_buffer_object_t));
        obj_map_set(&state.buffer_objects, buffer, *obj);
        gl_buffer_object_init(*obj, buffer);
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
    for (GLsizei i = 0; i < n; i++)
    {
        gl_buffer_object_t *obj = obj_map_remove(&state.buffer_objects, buffers[i]);
        if (obj == NULL) {
            continue;
        }
        
        gl_unbind_buffer(obj, &state.array_buffer);
        gl_unbind_buffer(obj, &state.element_array_buffer);

        gl_unbind_buffer(obj, &state.vertex_array.binding);
        gl_unbind_buffer(obj, &state.color_array.binding);
        gl_unbind_buffer(obj, &state.texcoord_array.binding);
        gl_unbind_buffer(obj, &state.normal_array.binding);

        // TODO: keep alive until no longer in use

        gl_buffer_object_free(obj);
    }
}

void glGenBuffersARB(GLsizei n, GLuint *buffers)
{
    for (GLsizei i = 0; i < n; i++)
    {
        buffers[i] = state.next_buffer_name++;
    }
}

GLboolean glIsBufferARB(GLuint buffer)
{
    return obj_map_get(&state.buffer_objects, buffer) != NULL;
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
        gl_set_error(GL_INVALID_ENUM);
        return false;
    }

    if (*obj == NULL) {
        gl_set_error(GL_INVALID_OPERATION);
        return false;
    }

    return true;
}

void glBufferDataARB(GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage)
{
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
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    void *new_data = malloc_uncached(size);
    if (new_data == NULL) {
        gl_set_error(GL_OUT_OF_MEMORY);
        return;
    }

    if (obj->data != NULL) {
        // TODO: keep around until not used anymore
        free_uncached(obj->data);
    }

    if (data != NULL) {
        memcpy(new_data, data, size);
    }

    obj->size = size;
    obj->usage = usage;
    obj->access = GL_READ_WRITE_ARB;
    obj->mapped = false;
    obj->pointer = NULL;
    obj->data = new_data;
}

void glBufferSubDataARB(GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data)
{
    gl_buffer_object_t *obj = NULL;
    if (!gl_get_buffer_object(target, &obj)) {
        return;
    }

    if (obj->mapped) {
        gl_set_error(GL_INVALID_OPERATION);
        return;
    }

    if ((offset < 0) || (offset >= obj->size) || (offset + size > obj->size)) {
        gl_set_error(GL_INVALID_VALUE);
        return;
    }

    memcpy(obj->data + offset, data, size);
}

void glGetBufferSubDataARB(GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid *data)
{
    gl_buffer_object_t *obj = NULL;
    if (!gl_get_buffer_object(target, &obj)) {
        return;
    }

    if (obj->mapped) {
        gl_set_error(GL_INVALID_OPERATION);
        return;
    }

    if ((offset < 0) || (offset >= obj->size) || (offset + size > obj->size)) {
        gl_set_error(GL_INVALID_VALUE);
        return;
    }

    memcpy(data, obj->data + offset, size);
}

GLvoid * glMapBufferARB(GLenum target, GLenum access)
{
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
        gl_set_error(GL_INVALID_ENUM);
        return NULL;
    }

    if (obj->mapped) {
        gl_set_error(GL_INVALID_OPERATION);
        return NULL;
    }

    obj->access = access;
    obj->mapped = true;
    obj->pointer = obj->data;

    return obj->pointer;
}

GLboolean glUnmapBufferARB(GLenum target)
{
    gl_buffer_object_t *obj = NULL;
    if (!gl_get_buffer_object(target, &obj)) {
        return GL_FALSE;
    }

    if (!obj->mapped) {
        gl_set_error(GL_INVALID_OPERATION);
        return GL_FALSE;
    }

    obj->mapped = false;
    obj->pointer = NULL;

    return GL_TRUE;
}

void glGetBufferParameterivARB(GLenum target, GLenum pname, GLint *params)
{
    gl_buffer_object_t *obj = NULL;
    if (!gl_get_buffer_object(target, &obj)) {
        return;
    }

    switch (pname) {
    case GL_BUFFER_SIZE_ARB:
        *params = obj->size;
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
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
}

void glGetBufferPointervARB(GLenum target, GLenum pname, GLvoid **params)
{
    gl_buffer_object_t *obj = NULL;
    if (!gl_get_buffer_object(target, &obj)) {
        return;
    }

    if (pname != GL_BUFFER_MAP_POINTER_ARB) {
        gl_set_error(GL_INVALID_ENUM);
        return;
    }

    *params = obj->pointer;
}
