#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include "../common/binout.c"
#include "../common/binout.h"
#include "../common/utils.h"
#include "../common/assetcomp.h"

#include "../../include/mgfx_mesh_types.h"
#include "../../src/magma/mgfx_mesh_internal.h"
#include "../../include/mgfx_constants.h"
#include "../../include/mgfx_macros.h"
#include "../../include/magma_constants.h"

#include "../common/meshoptimizer/meshoptimizer.h"

#define CGLTF_IMPLEMENTATION
#include "../common/cgltf.h"

#define MGFX_MAX_ATTRIBUTE_COUNT    3
#define MGFX_MAX_CONVERSION_COUNT   4

typedef void (*convert_func)(uint8_t*,const float*);

typedef struct
{
    cgltf_attribute *in_attr;
    float *buffer;
    uint32_t out_offset;
    uint32_t component_count;
    convert_func convert_func;
} attribute_conversion;

int flag_verbose = 0;

void meshdb_free(mgfx_meshdb_t *meshdb)
{
    for (size_t i = 0; i < meshdb->mesh_count; i++)
    {
        mgfx_mesh_t *mesh = meshdb->meshes[i].mesh;
        if (mesh == NULL) continue;

        for (size_t j = 0; j < mesh->submesh_count; j++)
        {
            mgfx_submesh_t *submesh = &mesh->submeshes[j];
            if (submesh->vertex_layout.attributes != NULL) free(submesh->vertex_layout.attributes);
            if (submesh->vertices != NULL) free(submesh->vertices);
            if (submesh->indices != NULL) free(submesh->indices);
        }
        free(mesh);
    }
    free(meshdb);
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

int optimize_submesh_buffers(mgfx_submesh_t *submesh)
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

int convert_primitive(cgltf_primitive *in_primitive, mgfx_submesh_t *out_submesh)
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
    mg_vertex_attribute_t attributes[MGFX_MAX_ATTRIBUTE_COUNT];
    attribute_conversion conversions[MGFX_MAX_CONVERSION_COUNT] = {};

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

        if (optimize_submesh_buffers(out_submesh) != 0) {
            fprintf(stderr, "Error: failed optimizing vertex and index buffers\n");
            return 1;
        }
    }

    return 0;
}

int convert_mesh(const cgltf_mesh *in_mesh, mgfx_mesh_t *out_mesh)
{
    out_mesh->submesh_count = in_mesh->primitives_count;

    for (size_t i = 0; i < in_mesh->primitives_count; i++)
    {
        if (flag_verbose) {
            printf("Converting primitive %zd\n", i);
        }

        if (convert_primitive(&in_mesh->primitives[i], &out_mesh->submeshes[i]) != 0) {
            fprintf(stderr, "Error: failed converting primitive %zd\n", i);
            return 1;
        }
    }

    return 0;
}

int convert_meshdb(const cgltf_data *data, mgfx_meshdb_t *out_meshdb)
{
    memcpy(out_meshdb->magic, MGFX_MESH_MAGIC, MGFX_MESH_MAGIC_LEN);
    out_meshdb->version = MGFX_MESH_VERSION;
    out_meshdb->mesh_count = data->meshes_count;

    for (size_t i = 0; i < data->meshes_count; i++)
    {
        const cgltf_mesh *in_mesh = &data->meshes[i];
        if (flag_verbose) {
            printf("Converting mesh %s\n", in_mesh->name);
        }
        
        mgfx_mesh_entry_t *entry = &out_meshdb->meshes[i];
        entry->name = in_mesh->name;
        entry->mesh = calloc(1, sizeof(mgfx_mesh_t) + sizeof(mgfx_submesh_t) * in_mesh->primitives_count);

        if (convert_mesh(in_mesh, entry->mesh) != 0) {
            fprintf(stderr, "Error: failed converting mesh %s\n", in_mesh->name);
            return 1;
        }
    }

    return 0;
}

void mesh_write(mgfx_mesh_t *mesh, FILE *out)
{
    w32(out, mesh->submesh_count);

    for (size_t i = 0; i < mesh->submesh_count; i++)
    {
        mgfx_submesh_t *submesh = &mesh->submeshes[i];

        w32(out, submesh->vertex_layout.stride);
        w32(out, submesh->vertex_layout.attribute_count);
        w32_placeholderf(out, "vtx_attributes%d", i);

        w32(out, submesh->input_assembly_parms.primitive_topology);
        w8(out, submesh->input_assembly_parms.primitive_restart_enabled);

        walign(out, sizeof(submesh->vertices_count));
        w32(out, submesh->vertices_count);
        w32(out, submesh->indices_count);
        w32_placeholderf(out, "vertices%d", i);
        if (submesh->indices != NULL) {
            w32_placeholderf(out, "indices%d", i);
        } else {
            w32(out, 0);
        }
    }

    for (size_t i = 0; i < mesh->submesh_count; i++)
    {
        mgfx_submesh_t *submesh = &mesh->submeshes[i];
        walign(out, sizeof(uint32_t));
        placeholder_set(out, "vtx_attributes%d", i);
        for (size_t j = 0; j < submesh->vertex_layout.attribute_count; j++)
        {
            mg_vertex_attribute_t *attr = &submesh->vertex_layout.attributes[j];
            w32(out, attr->input);
            w32(out, attr->offset);
        }
    }

    for (size_t i = 0; i < mesh->submesh_count; i++)
    {
        mgfx_submesh_t *submesh = &mesh->submeshes[i];
        size_t vertices_size = submesh->vertex_layout.stride * submesh->vertices_count;
        walign(out, 8);
        placeholder_set(out, "vertices%d", i);
        fwrite(submesh->vertices, vertices_size, 1, out);
    }

    for (size_t i = 0; i < mesh->submesh_count; i++)
    {
        mgfx_submesh_t *submesh = &mesh->submeshes[i];
        if (submesh->indices == NULL) continue;

        walign(out, 8);
        placeholder_set(out, "indices%d", i);
        for (size_t j = 0; j < submesh->indices_count; j++)
        {
            w16(out, submesh->indices[j]);
        }
    }
}

void meshdb_write(mgfx_meshdb_t *meshdb, FILE *out)
{
    w8(out, meshdb->magic[0]);
    w8(out, meshdb->magic[1]);
    w8(out, meshdb->magic[2]);
    w8(out, meshdb->version);
    w32(out, meshdb->mesh_count);

    for (size_t i = 0; i < meshdb->mesh_count; i++)
    {
        w32_placeholderf(out, "meshname%d", i);
        w32_placeholderf(out, "meshptr%d", i);
    }
    
    for (size_t i = 0; i < meshdb->mesh_count; i++)
    {
        mgfx_mesh_t *mesh = meshdb->meshes[i].mesh;
        walign(out, sizeof(mesh->submesh_count));
        placeholder_set(out, "meshptr%d", i);
        mesh_write(mesh, out);
    }

    for (size_t i = 0; i < meshdb->mesh_count; i++)
    {
        placeholder_set(out, "meshname%d", i);
        fwrite(meshdb->meshes[i].name, 1, strlen(meshdb->meshes[i].name) + 1, out);
    }
}

int convert(const char *infn, const char *outfn)
{
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, infn, &data);
    if (result == cgltf_result_file_not_found) {
        fprintf(stderr, "Error: could not find input file: %s\n", infn);
        return 1;
    }
    if (result != cgltf_result_success) {
        fprintf(stderr, "Error: could not parse input file: %s\n", infn);
        return 1;
    }

    if (cgltf_validate(data) != cgltf_result_success) {
        fprintf(stderr, "Error: validation failed\n");
        cgltf_free(data);
        return 1;
    }

    if (data->asset.generator && strstr(data->asset.generator, "Blender") && strstr(data->asset.generator, "v3.4.50")) {
        fprintf(stderr, "Error: Blender version v3.4.1 has buggy glTF export (vertex colors are wrong).\nPlease upgrade Blender and export the model again.\n");
        cgltf_free(data);
        return 1;
    }

    cgltf_load_buffers(&options, data, infn);

    mgfx_meshdb_t *out_meshdb = calloc(1, sizeof(mgfx_meshdb_t) + sizeof(mgfx_mesh_entry_t) * data->meshes_count);

    if (convert_meshdb(data, out_meshdb) != 0) {
        fprintf(stderr, "Error: failed converting mesh\n");
        meshdb_free(out_meshdb);
        cgltf_free(data);
        return 1;
    }

    FILE *out = fopen(outfn, "wb");
    if (!out) {
        fprintf(stderr, "could not open output file: %s\n", outfn);
        meshdb_free(out_meshdb);
        cgltf_free(data);
        return 1;
    }

    meshdb_write(out_meshdb, out);

    fclose(out);
    meshdb_free(out_meshdb);
    cgltf_free(data);
    return 0;
}

void print_args( char * name )
{
    fprintf(stderr, "mgfx -- Extract and convert meshes from glTF 2.0 files\n\n");
    fprintf(stderr, "Usage: %s [flags] <input files...>\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -o/--output <dir>       Specify output directory (default: .)\n");
    fprintf(stderr, "   -c/--compress <level>   Compress output files (default: %d)\n", DEFAULT_COMPRESSION);
    fprintf(stderr, "   -v/--verbose            Verbose output\n");
    fprintf(stderr, "\n");
}

int main(int argc, char *argv[])
{
    char *infn = NULL, *outdir = ".", *outfn = NULL;
    bool error = false;
    int compression = DEFAULT_COMPRESSION;

    if (argc < 2) {
        print_args(argv[0]);
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                print_args(argv[0]);
                return 0;
            } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
                flag_verbose++;
            } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--compress")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                char extra;
                if (sscanf(argv[i], "%d%c", &compression, &extra) != 1) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
                if (compression < 0 || compression > MAX_COMPRESSION) {
                    fprintf(stderr, "invalid compression level: %d\n", compression);
                    return 1;
                }
            } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                outdir = argv[i];
            } else {
                fprintf(stderr, "invalid flag: %s\n", argv[i]);
                return 1;
            }
            continue;
        }

        infn = argv[i];
        char *basename = strrchr(infn, '/');
        if (!basename) basename = infn; else basename += 1;
        char* basename_noext = strdup(basename);
        char* ext = strrchr(basename_noext, '.');
        if (ext) *ext = '\0';

        asprintf(&outfn, "%s/%s.mgfx", outdir, basename_noext);
        if (flag_verbose)
            printf("Converting: %s -> %s\n",
                infn, outfn);
        if (convert(infn, outfn) != 0) {
            error = true;
        }
        if (!error) {
            if (compression) {
                struct stat st_decomp = {0}, st_comp = {0};
                stat(outfn, &st_decomp);
                asset_compress(outfn, outfn, compression, 0);
                stat(outfn, &st_comp);
                if (flag_verbose)
                    printf("compressed: %s (%d -> %d, ratio %.1f%%)\n", outfn,
                    (int)st_decomp.st_size, (int)st_comp.st_size, 100.0 * (float)st_comp.st_size / (float)(st_decomp.st_size == 0 ? 1 :st_decomp.st_size));
            }
        }

        free(outfn);
    }

    return error ? 1 : 0;
}
