/**
 * @file buffer.c
 * @author Dennis Heinze <dennisjp.heinze@gmail.com>
 * @brief OpenGL buffer object management and data transfer.
 */
#include "buffer.h"
#include "gl_internal.h"
#include "array_object.h"
#include "draw_call_cache.h"
#include <n64sys.h>
#include <malloc.h>
#include <string.h>

extern gl_state_t *state;


GLboolean glIsBufferARB(GLuint buffer)
{
    // FIXME: This doesn't actually guarantee that it's a valid buffer object, but just uses the heuristic of
    //        "is it somewhere in the heap memory?". This way we can at least rule out arbitrarily chosen integer constants,
    //        which used to be valid buffer IDs in legacy OpenGL.
    return is_valid_object_id(buffer);
}

static void buffer_object_free(gl_buffer_object_t *obj)
{
    gl_storage_free(&obj->storage);

    if (obj->element_cache != NULL)
    {
        draw_call_cache_free(obj->element_cache);
    }

    assertf(obj->array_obj_ref == NULL, "Freeing a buffer object that still had references to array objects: %p", obj);

    free(obj);
}

void buffer_object_refcount_incr(gl_buffer_object_t *obj)
{
    ++obj->ref_count;
}

void buffer_object_refcount_decr(gl_buffer_object_t *obj)
{
    assertf(obj->ref_count > 0, "Decreasing reference count of a buffer object that already has 0 references: %p", obj);

    if (--obj->ref_count == 0) {
        buffer_object_free(obj);
    }
}

void buffer_object_validate_not_mapped(gl_buffer_object_t *obj)
{
    if (obj->mapped) {
        gl_set_error(GL_INVALID_OPERATION, "Accessing buffer object for drawing while it is mapped: %p", obj);
    }
}

void buffer_object_set_binding(gl_buffer_object_t *obj, gl_buffer_object_t **binding)
{
    if (obj == *binding) return;

    gl_buffer_object_t *old_binding = *binding;
    if (old_binding != NULL) {
        buffer_object_refcount_decr(old_binding);
    }
    if (obj != NULL) {
        buffer_object_refcount_incr(obj);
    }
    *binding = obj;
}

void glBindBufferARB(GLenum target, GLuint buffer)
{
    if (!gl_ensure_no_begin_end()) return;
    assertf(buffer == 0 || is_valid_object_id(buffer), 
        "Not a valid buffer object: %#lx. Make sure to allocate IDs via glGenBuffersARB", buffer);

    gl_buffer_object_t *obj = (gl_buffer_object_t*)buffer;

    switch (target) {
    case GL_ARRAY_BUFFER_ARB:
        buffer_object_set_binding(obj, &state->array_buffer);
        break;
    case GL_ELEMENT_ARRAY_BUFFER_ARB:
        buffer_object_set_binding(obj, &state->array_object->element_array_buffer);
        break;
    default:
        gl_set_error(GL_INVALID_ENUM, "%#04lx is not a valid buffer target", target);
        return;
    }
}

static void buffer_object_unbind(gl_buffer_object_t *obj, gl_buffer_object_t **binding)
{
    if (*binding == obj) {
        buffer_object_set_binding(NULL, binding);
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
        
        // Deleting a buffer object will automatically unbind it from GL_ARRAY_BUFFER and GL_ELEMENT_ARRAY_BUFFER
        buffer_object_unbind(obj, &state->array_buffer);
        buffer_object_unbind(obj, &state->array_object->element_array_buffer);

        // Deleting a buffer object will automatically unbind it from all arrays in the currently bound VAO
        for (gl_array_type_t a = 0; a < ATTRIB_COUNT; a++) {
            if (state->array_object->arrays[a].binding == obj) {
                array_object_set_buffer_binding(state->array_object, a, NULL);
            }
        }

        // Remove the reference that corresponds to the name that is being deleted
        buffer_object_refcount_decr(obj);
    }
}

void glGenBuffersARB(GLsizei n, GLuint *buffers)
{
    if (!gl_ensure_no_begin_end()) return;

    for (GLsizei i = 0; i < n; i++)
    {
        gl_buffer_object_t *new_obj = calloc(1, sizeof(gl_buffer_object_t));
        assertf(new_obj, "Out of memory");
        new_obj->usage = GL_STATIC_DRAW_ARB;
        new_obj->access = GL_READ_WRITE_ARB;
        // Being assigned a "name" (the ID returned by this function) counts as a reference.
        // Deleting the name using glDeleteBuffersARB removes that reference.
        new_obj->ref_count = 1;
        buffers[i] = (GLuint)new_obj;
    }
}

bool gl_get_buffer_object(GLenum target, gl_buffer_object_t **obj)
{
    switch (target) {
    case GL_ARRAY_BUFFER_ARB:
        *obj = state->array_buffer;
        break;
    case GL_ELEMENT_ARRAY_BUFFER_ARB:
        *obj = state->array_object->element_array_buffer;
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

void buffer_set_data_dirty(gl_buffer_object_t *obj)
{
    if (obj->element_cache != NULL) {
        draw_call_cache_set_data_dirty(obj->element_cache);
    }

    // Inform all array objects that are bound to this buffer
    gl_array_object_ref_t *array_ref = obj->array_obj_ref;
    while (array_ref != NULL) {
        array_object_set_buffer_dirty(array_ref->array_object, obj);
        array_ref = array_ref->next;
    }
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

    buffer_set_data_dirty(obj);
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

    buffer_set_data_dirty(obj);
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

    if (obj->access != GL_READ_ONLY_ARB) {
        buffer_set_data_dirty(obj);
    }
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

void gl_buffer_add_array_ref(gl_buffer_object_t *buffer, gl_array_object_t *array)
{
    // Add to linked list of references, if not already present
    gl_array_object_ref_t **ref = &buffer->array_obj_ref;

    while (*ref != NULL) {
        // array object is already in the list
        if ((*ref)->array_object == array) return;
        ref = &(*ref)->next;
    }

    // Append to the end of the list
    *ref = malloc(sizeof(gl_array_object_ref_t));
    (*ref)->array_object = array;
    (*ref)->next = NULL;

    buffer_object_refcount_incr(buffer);
}

void gl_buffer_remove_array_ref(gl_buffer_object_t *buffer, gl_array_object_t *array)
{
    // Remove from linked list of references
    gl_array_object_ref_t **ref = &buffer->array_obj_ref;

    while (*ref != NULL) {
        if ((*ref)->array_object == array) {
            // If found, remove item
            gl_array_object_ref_t *current = *ref;
            *ref = current->next;
            free(current);
            buffer_object_refcount_decr(buffer);
            break;
        }
        ref = &(*ref)->next;
    }
}
