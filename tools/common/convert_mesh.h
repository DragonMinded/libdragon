#ifndef LIBDRAGON_TOOLS_CONVERT_MESH_H
#define LIBDRAGON_TOOLS_CONVERT_MESH_H

#include <stdbool.h>
#include <stdint.h>

#include "utils.h"
#include "meshoptimizer/meshoptimizer.h"
#include "cgltf.h"

#include "../../include/mgfx_mesh_types.h"
#include "../../include/mgfx_constants.h"
#include "../../include/mgfx_macros.h"
#include "../../include/magma_constants.h"

#define MAX_ATTRIBUTE_COUNT    3
#define MAX_CONVERSION_COUNT   4

typedef void (*convert_func)(uint8_t*,const float*);

typedef struct
{
    cgltf_attribute *in_attr;
    float *buffer;
    uint32_t out_offset;
    uint32_t component_count;
    convert_func convert_func;
} attribute_conversion;

void mesh_free(mgfx_mesh_t *mesh)
{
    for (size_t j = 0; j < mesh->submesh_count; j++)
    {
        mgfx_submesh_t *submesh = &mesh->submeshes[j];
        if (submesh->vertex_layout.attributes != NULL) free(submesh->vertex_layout.attributes);
        if (submesh->vertices != NULL) free(submesh->vertices);
        if (submesh->indices != NULL) free(submesh->indices);
    }
    free(mesh->submeshes);
}

mg_primitive_topology_t convert_primitive_topology(cgltf_primitive_type in_type)
{
    switch (in_type) {
        case cgltf_primitive_type_triangles:
            return MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case cgltf_primitive_type_triangle_strip:
            return MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case cgltf_primitive_type_triangle_fan:
            return MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        default:
            return -1;
    }
}

cgltf_attribute *find_input_attribute(cgltf_primitive *primitive, cgltf_attribute_type type)
{
    for (size_t i = 0; i < primitive->attributes_count; i++)
    {
        if (primitive->attributes[i].type == type) return &primitive->attributes[i];
    }

    return NULL;
}

// Copy array of 16 bit values and convert to big endian
void cpybe16(uint8_t *dst, const int16_t *src, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        dst[i*2 + 0] = src[i] >> 8;
        dst[i*2 + 1] = src[i] & 0xFF;
    }
}

void convert_position(uint8_t *dst, const float *src)
{
    int16_t pos[3] = MGFX_POS(src[0], src[1], src[2]);
    cpybe16(dst, pos, 3);
}

void convert_normal(uint8_t *dst, const float *src)
{
    int16_t x = CLAMP(roundf(src[0] * 15.5f), -16.0f, 15.0f);
    int16_t y = CLAMP(roundf(src[1] * 31.5f), -32.0f, 31.0f);
    int16_t z = CLAMP(roundf(src[2] * 15.5f), -16.0f, 15.0f);
    int16_t packed = MGFX_NRM(x, y, z);
    cpybe16(dst, &packed, 1);
}

void convert_color(uint8_t *dst, const float *src)
{
    // Pre-gamma-correct vertex colors (excluding alpha)
    for (size_t i = 0; i < 3; i++) {
        dst[i] = powf(src[i], 1.0f/2.2f) * 0xFF;
    }
    dst[3] = src[3] * 0xFF;
}

void convert_texcoord(uint8_t *dst, const float *src)
{
    int16_t tex[2] = MGFX_TEX(src[0], src[1]);
    cpybe16(dst, tex, 2);
}

int optimize_submesh_buffers(mgfx_submesh_t *submesh, int flag_verbose)
{
    const uint32_t invalid_index = 0xFFFFFFFF;

    if (submesh->input_assembly_parms.primitive_topology != MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) {
        // Other modes not supported for now
        return 0;
    }

    // Create new buffers
    uint8_t *vertex_buffer = calloc(submesh->vertices_count*2, submesh->vertex_layout.stride);
    uint16_t *index_buffer = calloc(submesh->indices_count, sizeof(uint16_t));

    // Optimize buffers
    uint32_t triangle_count = submesh->indices_count / 3;

    uint32_t emitted_triangle_count = 0;
    uint32_t emitted_vtx_count = 0;
    bool *triangle_is_emitted_table = calloc(triangle_count, sizeof(bool));
    uint32_t *vtx_index_table = malloc(submesh->vertices_count * sizeof(uint32_t));
    memset(vtx_index_table, invalid_index, submesh->vertices_count * sizeof(uint32_t));

    uint32_t chunk_offset = 0;
    uint32_t chunk_vtx_count = 0;

    bool error = false;

    while (emitted_triangle_count < triangle_count) {
        // Find the first triangle with the most shared vertices
        uint32_t next_triangle = invalid_index;
        int max_found_shared = -1;

        for (size_t i = 0; i < triangle_count; i++)
        {
            // Skip triangles that have already been emitted
            if (triangle_is_emitted_table[i]) continue;
            
            // Count vertices that are shared with the current chunk
            int shared = 0;
            for (size_t j = 0; j < 3; j++) {
                uint32_t old_index = submesh->indices[i*3+j];
                uint32_t new_index = vtx_index_table[old_index];
                if (new_index != invalid_index && new_index >= chunk_offset) ++shared;
            }
            
            // find the first maximum
            if (shared > max_found_shared) {
                next_triangle = i;
                max_found_shared = shared;
            }

            // We won't find a triangle with more than 3 shared vertices, so stop searching immediately
            if (shared == 3) break;
        }

        if (next_triangle == invalid_index) {
            // This error would only occur because this function is buggy
            fprintf(stderr, "Error: ran out of triangles...?\n");
            error = true;
            break;
        }

        // Check if the new triangle fits the current chunk
        int new_vtx_count = 3 - max_found_shared;
        if ((chunk_vtx_count + new_vtx_count) > MG_VERTEX_CACHE_COUNT) {
            // Reset chunk and try again
            chunk_offset += chunk_vtx_count;
            chunk_vtx_count = 0;
            continue;
        }

        // Emit triangle
        for (size_t i = 0; i < 3; i++)
        {
            uint32_t old_index = submesh->indices[next_triangle*3+i];
            uint32_t new_index = vtx_index_table[old_index];
            if (new_index == invalid_index || new_index < chunk_offset) {
                new_index = emitted_vtx_count++;
                vtx_index_table[old_index] = new_index;
                ++chunk_vtx_count;

                // Emit vertex (copy to new buffer)
                size_t size = submesh->vertex_layout.stride;
                uint8_t *dst = vertex_buffer + new_index*size;
                const uint8_t *src = (const uint8_t*)submesh->vertices + old_index*size;
                memcpy(dst, src, size);
            }

            uint32_t index_buffer_offset = emitted_triangle_count * 3;
            index_buffer[index_buffer_offset + i] = new_index;
        }
        triangle_is_emitted_table[next_triangle] = true;
        ++emitted_triangle_count;
    }
    
    free(triangle_is_emitted_table);
    free(vtx_index_table);

    if (error) {
        free(vertex_buffer);
        free(index_buffer);
        return 1;
    }

    // Replace old buffers
    free(submesh->vertices);
    free(submesh->indices);
    submesh->vertices = vertex_buffer;
    submesh->indices = index_buffer;

    if (submesh->vertices_count != emitted_vtx_count) {
        if (flag_verbose) {
            printf("Vertex count changed during optimization: %d -> %d\n", submesh->vertices_count, emitted_vtx_count);
        }
        submesh->vertices_count = emitted_vtx_count;
    }
    
    return 0;
}

int convert_primitive(cgltf_primitive *in_primitive, mgfx_submesh_t *out_submesh, int flag_verbose)
{
    out_submesh->input_assembly_parms.primitive_restart_enabled = false;
    out_submesh->input_assembly_parms.primitive_topology = convert_primitive_topology(in_primitive->type);

    if (out_submesh->input_assembly_parms.primitive_topology == -1) {
        fprintf(stderr, "Error: only triangles are supported!\n");
        return 1;
    }

    // Read vertex layout and configure conversion

    uint32_t attribute_count = 0;
    uint32_t conversion_count = 0;
    uint32_t vertex_stride = 0;
    mg_vertex_attribute_t attributes[MAX_ATTRIBUTE_COUNT];
    attribute_conversion conversions[MAX_CONVERSION_COUNT] = {};

    cgltf_attribute *in_pos = find_input_attribute(in_primitive, cgltf_attribute_type_position);
    if (in_pos != NULL) {
        attributes[attribute_count++] = (mg_vertex_attribute_t) {
            .input = MGFX_ATTRIBUTE_POS_NORM,
            .offset = vertex_stride
        };
        conversions[conversion_count++] = (attribute_conversion) {
            .in_attr = in_pos,
            .out_offset = vertex_stride,
            .convert_func = (convert_func)convert_position
        };
        vertex_stride += sizeof(uint16_t) * 4;
    } else {
        fprintf(stderr, "Error: no position attribute found in primitive!\n");
        return 1;
    }

    cgltf_attribute *in_norm = find_input_attribute(in_primitive, cgltf_attribute_type_normal);
    if (in_norm != NULL) {
        conversions[conversion_count++] = (attribute_conversion) {
            .in_attr = in_norm,
            .out_offset = vertex_stride - sizeof(uint16_t),
            .convert_func = (convert_func)convert_normal
        };
    }

    cgltf_attribute *in_color = find_input_attribute(in_primitive, cgltf_attribute_type_color);
    if (in_color != NULL) {
        attributes[attribute_count++] = (mg_vertex_attribute_t) {
            .input = MGFX_ATTRIBUTE_COLOR,
            .offset = vertex_stride
        };
        conversions[conversion_count++] = (attribute_conversion) {
            .in_attr = in_color,
            .out_offset = vertex_stride,
            .convert_func = (convert_func)convert_color
        };
        vertex_stride += sizeof(uint32_t);
    }

    cgltf_attribute *in_tex = find_input_attribute(in_primitive, cgltf_attribute_type_texcoord);
    if (in_tex != NULL) {
        attributes[attribute_count++] = (mg_vertex_attribute_t) {
            .input = MGFX_ATTRIBUTE_TEXCOORD,
            .offset = vertex_stride
        };
        conversions[conversion_count++] = (attribute_conversion) {
            .in_attr = in_tex,
            .out_offset = vertex_stride,
            .convert_func = (convert_func)convert_texcoord
        };
        vertex_stride += sizeof(uint16_t) * 2;
    }

    bool has_error = false;

    for (size_t i = 0; i < conversion_count; i++)
    {
        // allocate buffers
        cgltf_accessor *in_accessor = conversions[i].in_attr->data;
        size_t num_components = cgltf_num_components(in_accessor->type);
        size_t num_values = num_components * in_accessor->count;
        conversions[i].component_count = num_components;
        conversions[i].buffer = malloc(sizeof(float) * num_values);
        if (cgltf_accessor_unpack_floats(in_accessor, conversions[i].buffer, num_values) == 0) {
            fprintf(stderr, "Error: failed reading attribute data\n");
            has_error = true;
            break;
        }
    }

    if (!has_error) {
        uint32_t vertex_count = in_pos->data->count;
        out_submesh->vertices_count = vertex_count;
        out_submesh->vertices = malloc(vertex_stride * vertex_count);

        for (size_t i = 0; i < vertex_count; i++)
        {
            uint8_t *dst = ((uint8_t*)out_submesh->vertices) + i * vertex_stride;
            for (size_t j = 0; j < conversion_count; j++)
            {
                const float *src = conversions[j].buffer + i * conversions[j].component_count;
                float tmp[4] = {0, 0, 0, 1}; // Make sure that missing components are always replaced with sensible defaults
                memcpy(tmp, src, sizeof(float) * conversions[j].component_count);
                conversions[j].convert_func(dst + conversions[j].out_offset, tmp);
            }
        }
    }
        
    for (size_t i = 0; i < conversion_count; i++)
    {
        if (conversions[i].buffer != NULL) free(conversions[i].buffer);
    }

    if (has_error) return 1;

    out_submesh->vertex_layout.stride = vertex_stride;
    out_submesh->vertex_layout.attribute_count = attribute_count;
    out_submesh->vertex_layout.attributes = calloc(attribute_count, sizeof(mg_vertex_attribute_t));
    memcpy(out_submesh->vertex_layout.attributes, attributes, attribute_count * sizeof(mg_vertex_attribute_t));

    if (in_primitive->indices != NULL) {
        cgltf_accessor *in_indices = in_primitive->indices;
        out_submesh->indices_count = in_indices->count;

        cgltf_uint *tmp_indices = malloc(sizeof(cgltf_uint) * in_indices->count);
        if (cgltf_accessor_unpack_indices(in_indices, tmp_indices, in_indices->count) == 0) {
            fprintf(stderr, "Error: failed reading index data\n");
            free(tmp_indices);
            return 1;
        }

        meshopt_optimizeVertexCache(tmp_indices, tmp_indices, in_indices->count, out_submesh->vertices_count);

        out_submesh->indices = malloc(sizeof(uint16_t) * in_indices->count);
        for (size_t i = 0; i < in_indices->count; i++) out_submesh->indices[i] = tmp_indices[i];
        free(tmp_indices);

        if (optimize_submesh_buffers(out_submesh, flag_verbose) != 0) {
            fprintf(stderr, "Error: failed optimizing vertex and index buffers\n");
            return 1;
        }
    }

    return 0;
}

int convert_mesh(const cgltf_mesh *in_mesh, mgfx_mesh_t *out_mesh, int flag_verbose)
{
    out_mesh->submesh_count = in_mesh->primitives_count;
    out_mesh->submeshes = calloc(in_mesh->primitives_count, sizeof(mgfx_submesh_t));

    for (size_t i = 0; i < in_mesh->primitives_count; i++)
    {
        if (flag_verbose) {
            printf("Converting primitive %zd\n", i);
        }

        if (convert_primitive(&in_mesh->primitives[i], &out_mesh->submeshes[i], flag_verbose) != 0) {
            fprintf(stderr, "Error: failed converting primitive %zd\n", i);
            return 1;
        }
    }

    return 0;
}

#endif
