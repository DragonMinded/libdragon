#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "../common/binout.h"

#include "../../include/GL/gl_enums.h"
#include "../../src/model64_internal.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#define VERTEX_PRECISION    5
#define TEXCOORD_PRECISION  8

typedef void (*component_convert_func_t)(void*,float*,size_t);
typedef void (*index_convert_func_t)(void*,cgltf_uint*,size_t);

int flag_verbose = 0;

void print_args( char * name )
{
    fprintf(stderr, "mkmodel -- Convert glTF 2.0 models into the model64 format for libdragon\n\n");
    fprintf(stderr, "Usage: %s [flags] <input files...>\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -o/--output <dir>         Specify output directory (default: .)\n");
    fprintf(stderr, "   -v/--verbose              Verbose output\n");
    fprintf(stderr, "\n");
}

model64_t* model64_alloc()
{
    model64_t *model = calloc(1, sizeof(model64_t));
    model->magic = MODEL64_MAGIC;
    model->version = MODEL64_VERSION;
    model->header_size = sizeof(model64_t);
    model->mesh_size = sizeof(mesh_t);
    model->primitive_size = sizeof(primitive_t);
    return model;
}

void primitive_free(primitive_t *primitive)
{
    if (primitive->vertices) free(primitive->vertices);
    if (primitive->indices) free(primitive->indices);
}

void mesh_free(mesh_t *mesh)
{
    for (size_t i = 0; i < mesh->num_primitives; i++)
        primitive_free(&mesh->primitives[i]);
    
    if (mesh->primitives) free(mesh->primitives);
}

void model64_free(model64_t *model)
{
    for (size_t i = 0; i < model->num_meshes; i++)
        mesh_free(&model->meshes[i]);
    
    if (model->meshes) free(model->meshes);
    free(model);
}

void attribute_write(FILE *out, attribute_t *attr)
{
    w32(out, attr->size);
    w32(out, attr->type);
    w32(out, attr->offset);
}

void vertex_write(FILE *out, attribute_t *attr, void *data)
{
    if (attr->size == 0) return;
    
    void *attr_data = data + attr->offset;
    switch (attr->type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
        for (size_t i = 0; i < attr->size; i++) w8(out, ((uint8_t*)attr_data)[i]);
        break;
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
    case GL_HALF_FIXED_N64:
        for (size_t i = 0; i < attr->size; i++) w16(out, ((uint16_t*)attr_data)[i]);
        break;
    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_FLOAT:
        for (size_t i = 0; i < attr->size; i++) w32(out, ((uint32_t*)attr_data)[i]);
        break;
    default:
        break;
    }
}

void indices_write(FILE *out, uint32_t type, void *data, uint32_t count)
{
    switch (type) {
    case GL_UNSIGNED_BYTE:
        for (size_t i = 0; i < count; i++) w8(out, ((uint8_t*)data)[i]);
        break;
    case GL_UNSIGNED_SHORT:
        for (size_t i = 0; i < count; i++) w16(out, ((uint16_t*)data)[i]);
        break;
    case GL_UNSIGNED_INT:
        for (size_t i = 0; i < count; i++) w32(out, ((uint32_t*)data)[i]);
        break;
    }
}

void model64_write(model64_t *model, FILE *out)
{
    // Write header
    w32(out, model->magic);
    w32(out, model->version);
    w32(out, model->header_size);
    w32(out, model->mesh_size);
    w32(out, model->primitive_size);
    w32(out, model->num_meshes);
    uint32_t meshes_placeholder = ftell(out);
    w32(out, (uint32_t)0); // placeholder

    uint32_t total_num_primitives = 0;
    uint32_t *primitives_placeholders = alloca(sizeof(uint32_t) * model->num_meshes);

    // Write meshes
    uint32_t offset_meshes = ftell(out);
    for (size_t i = 0; i < model->num_meshes; i++)
    {
        mesh_t *mesh = &model->meshes[i];
        total_num_primitives += mesh->num_primitives;
        w32(out, mesh->num_primitives);
        primitives_placeholders[i] = ftell(out);
        w32(out, (uint32_t)0); // placeholder
    }

    uint32_t *offset_primitives = alloca(sizeof(uint32_t) * model->num_meshes);
    uint32_t *vertices_placeholders = alloca(sizeof(uint32_t) * total_num_primitives);

    size_t cur_primitive = 0;

    // Write primitives
    for (size_t i = 0; i < model->num_meshes; i++)
    {
        offset_primitives[i] = ftell(out);
        mesh_t *mesh = &model->meshes[i];
        for (size_t j = 0; j < mesh->num_primitives; j++)
        {
            primitive_t *primitive = &mesh->primitives[j];
            w32(out, primitive->mode);
            w32(out, primitive->stride);
            attribute_write(out, &primitive->position);
            attribute_write(out, &primitive->color);
            attribute_write(out, &primitive->texcoord);
            attribute_write(out, &primitive->normal);
            attribute_write(out, &primitive->mtx_index);
            w32(out, primitive->vertex_precision);
            w32(out, primitive->texcoord_precision);
            w32(out, primitive->index_type);
            w32(out, primitive->num_vertices);
            w32(out, primitive->num_indices);
            vertices_placeholders[cur_primitive++] = ftell(out);
            w32(out, (uint32_t)0); // placeholder
            w32(out, (uint32_t)0); // placeholder
        }
    }

    uint32_t *offset_vertices = alloca(sizeof(uint32_t) * total_num_primitives);
    uint32_t *offset_indices = alloca(sizeof(uint32_t) * total_num_primitives);
    cur_primitive = 0;

    // Write data
    for (size_t i = 0; i < model->num_meshes; i++)
    {
        mesh_t *mesh = &model->meshes[i];
        for (size_t j = 0; j < mesh->num_primitives; j++)
        {
            walign(out, 8);
            offset_vertices[cur_primitive] = ftell(out);
            primitive_t *primitive = &mesh->primitives[j];
            for (size_t k = 0; k < primitive->num_vertices; k++)
            {
                void *vertex = primitive->vertices + k * primitive->stride;
                vertex_write(out, &primitive->position, vertex);
                vertex_write(out, &primitive->color, vertex);
                vertex_write(out, &primitive->texcoord, vertex);
                vertex_write(out, &primitive->normal, vertex);
                vertex_write(out, &primitive->mtx_index, vertex);
            }
            walign(out, 8);
            offset_indices[cur_primitive++] = ftell(out);
            indices_write(out, primitive->index_type, primitive->indices, primitive->num_indices);
        }
    }
    
    uint32_t offset_end = ftell(out);

    // Fill in placeholders
    fseek(out, meshes_placeholder, SEEK_SET);
    w32(out, offset_meshes);

    for (size_t i = 0; i < model->num_meshes; i++)
    {
        fseek(out, primitives_placeholders[i], SEEK_SET);
        w32(out, offset_primitives[i]);
    }

    for (size_t i = 0; i < total_num_primitives; i++)
    {
        fseek(out, vertices_placeholders[i], SEEK_SET);
        w32(out, offset_vertices[i]);
        w32(out, offset_indices[i]);
    }

    fseek(out, offset_end, SEEK_SET);
}

int convert_attribute_data(cgltf_accessor *accessor, void *out_data, attribute_t *attr, uint32_t stride, component_convert_func_t convert_func)
{
    size_t num_components = cgltf_num_components(accessor->type);
    size_t num_floats = num_components * accessor->count;
    float *temp_buffer = malloc(sizeof(float) * num_floats);

    // First, convert all data to floats (because cgltf provides this very convenient function)
    // TODO: More sophisticated conversion that doesn't always use floats as intermediate values
    //       Might not be worth it since the majority of tools will probably only export floats anyway?
    if (cgltf_accessor_unpack_floats(accessor, temp_buffer, num_floats) == 0) {
        fprintf(stderr, "Error: failed reading attribute data\n");
        free(temp_buffer);
        return 1;
    }

    // Second, convert them to the target format and place in the interleaved vertex data
    for (size_t i = 0; i < accessor->count; i++)
    {
        void *dst = out_data + attr->offset + stride * i;
        convert_func(dst, &temp_buffer[i*num_components], num_components);
    }

    free(temp_buffer);
    return 0;
}

void convert_position(int16_t *dst, float *value, size_t size)
{
    for (size_t i = 0; i < size; i++) dst[i] = value[i] * (1<<VERTEX_PRECISION);
}

void convert_color(uint8_t *dst, float *value, size_t size)
{
    for (size_t i = 0; i < size; i++) dst[i] = value[i] * 0xFF;
}

void convert_texcoord(int16_t *dst, float *value, size_t size)
{
    for (size_t i = 0; i < size; i++) dst[i] = value[i] * (1<<TEXCOORD_PRECISION);
}

void convert_normal(int8_t *dst, float *value, size_t size)
{
    for (size_t i = 0; i < size; i++) dst[i] = value[i] * 0x7F;
}

void convert_mtx_index(uint8_t *dst, float *value, size_t size)
{
    for (size_t i = 0; i < size; i++) dst[i] = value[i];
}

void convert_index_u8(uint8_t *dst, cgltf_uint *src, size_t count)
{
    for (size_t i = 0; i < count; i++) dst[i] = src[i];
}

void convert_index_u16(uint16_t *dst, cgltf_uint *src, size_t count)
{
    for (size_t i = 0; i < count; i++) dst[i] = src[i];
}

void convert_index_u32(uint32_t *dst, cgltf_uint *src, size_t count)
{
    for (size_t i = 0; i < count; i++) dst[i] = src[i];
}

int convert_primitive(cgltf_primitive *in_primitive, primitive_t *out_primitive)
{
    // Matches the values of GL_TRIANGLES, GL_TRIANGLE_STRIPS etc. exactly so just copy it over
    out_primitive->mode = in_primitive->type;

    // TODO: Perhaps make these configurable or automatically optimize them?
    out_primitive->vertex_precision = VERTEX_PRECISION;
    out_primitive->texcoord_precision = TEXCOORD_PRECISION;

    static const uint32_t attr_types[] = {
        GL_HALF_FIXED_N64,
        GL_UNSIGNED_BYTE,
        GL_HALF_FIXED_N64,
        GL_BYTE,
        GL_UNSIGNED_BYTE,
    };

    static const uint32_t attr_type_sizes[] = {
        sizeof(int16_t),
        sizeof(uint8_t),
        sizeof(int16_t),
        sizeof(int8_t),
        sizeof(uint8_t),
    };

    static const component_convert_func_t attr_convert_funcs[] = {
        (component_convert_func_t)convert_position,
        (component_convert_func_t)convert_color,
        (component_convert_func_t)convert_texcoord,
        (component_convert_func_t)convert_normal,
        (component_convert_func_t)convert_mtx_index
    };

    attribute_t *attrs[] = {
        &out_primitive->position,
        &out_primitive->color,
        &out_primitive->texcoord,
        &out_primitive->normal,
        &out_primitive->mtx_index,
    };

    cgltf_attribute *attr_map[5] = {NULL};

    // Search for attributes that we need
    for (size_t i = 0; i < in_primitive->attributes_count; i++)
    {
        cgltf_attribute *attr = &in_primitive->attributes[i];

        switch (attr->type) {
        case cgltf_attribute_type_position:
            attr_map[0] = attr;
            break;
        case cgltf_attribute_type_color:
            attr_map[1] = attr;
            break;
        case cgltf_attribute_type_texcoord:
            attr_map[2] = attr;
            break;
        case cgltf_attribute_type_normal:
            attr_map[3] = attr;
            break;
        case cgltf_attribute_type_joints:
            attr_map[4] = attr;
            break;
        default:
            continue;
        }
    }

    if (attr_map[0] == NULL || attr_map[0]->data->count <= 0) {
        fprintf(stderr, "Error: primitive contains no vertices\n");
        return 1;
    }

    out_primitive->num_vertices = attr_map[0]->data->count;

    // Compute stride and attribute offsets
    uint32_t stride = 0;

    for (size_t i = 0; i < 5; i++)
    {
        if (attr_map[i] == NULL) continue;

        attrs[i]->size = cgltf_num_components(attr_map[i]->data->type);

        if (attrs[i]->size > 0) {
            attrs[i]->type = attr_types[i];
            attrs[i]->offset = stride;
            stride += attr_type_sizes[i] * attrs[i]->size;
        }
    }

    out_primitive->stride = stride;

    // Allocate memory for vertex data
    out_primitive->vertices = calloc(stride, out_primitive->num_vertices);

    // Convert vertex data
    for (size_t i = 0; i < 5; i++)
    {
        if (attrs[i]->size == 0) continue;

        if (convert_attribute_data(attr_map[i]->data, out_primitive->vertices, attrs[i], stride, attr_convert_funcs[i]) != 0) {
            fprintf(stderr, "Error: failed converting data of attribute %d\n", attr_map[i]->index);
            return 1;
        }
    }

    // Convert index data if present
    if (in_primitive->indices != NULL) {
        cgltf_accessor *in_indices = in_primitive->indices;
        out_primitive->num_indices = in_indices->count;

        // Determine index type
        // TODO: Automatically detect if the type could be made smaller based on the actual index values
        size_t index_size;
        index_convert_func_t convert_func;
        switch (in_indices->component_type) {
        case cgltf_component_type_r_8u:
            index_size = sizeof(uint8_t);
            out_primitive->index_type = GL_UNSIGNED_BYTE;
            convert_func = (index_convert_func_t)convert_index_u8;
            break;
        case cgltf_component_type_r_16u:
            index_size = sizeof(uint16_t);
            out_primitive->index_type = GL_UNSIGNED_SHORT;
            convert_func = (index_convert_func_t)convert_index_u16;
            break;
        case cgltf_component_type_r_32u:
            index_size = sizeof(uint32_t);
            out_primitive->index_type = GL_UNSIGNED_INT;
            convert_func = (index_convert_func_t)convert_index_u32;
            break;
        default:
            abort();
        }

        // Allocate memory for index data
        out_primitive->indices = calloc(index_size, out_primitive->num_indices);

        // Read from cgltf
        // TODO: Directly copy them over instead? Maybe it's fine like this since it's lossless
        cgltf_uint *temp_indices = malloc(sizeof(cgltf_uint) * in_indices->count);
        if (cgltf_accessor_unpack_indices(in_indices, temp_indices, in_indices->count) == 0) {
            fprintf(stderr, "Error: failed reading index data\n");
            free(temp_indices);
            return 1;
        }

        // Convert indices
        convert_func(out_primitive->indices, temp_indices, in_indices->count);

        free(temp_indices);
    }

    return 0;
}

int convert_mesh(cgltf_mesh *in_mesh, mesh_t *out_mesh)
{
    // Convert primitives
    out_mesh->num_primitives = in_mesh->primitives_count;
    out_mesh->primitives = calloc(in_mesh->primitives_count, sizeof(primitive_t));
    for (size_t i = 0; i < in_mesh->primitives_count; i++)
    {
        if (flag_verbose) {
            printf("Converting primitive %ld\n", i);
        }

        if (convert_primitive(&in_mesh->primitives[i], &out_mesh->primitives[i]) != 0) {
            fprintf(stderr, "Error: failed converting primitive %ld\n", i);
            return 1;
        }
    }

    return 0;
}

int convert(const char *infn, const char *outfn)
{
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, infn, &data);
    if (result != cgltf_result_success) {
        fprintf(stderr, "Error: could not parse input file: %s\n", infn);
        return 1;
    }

    if (cgltf_validate(data) != cgltf_result_success) {
        fprintf(stderr, "Error: validation failed\n");
        cgltf_free(data);
        return 1;
    }

    cgltf_load_buffers(&options, data, infn);

    model64_t *model = model64_alloc();

    if (data->meshes_count <= 0) {
        fprintf(stderr, "Error: input file contains no meshes\n");
        goto error;
    }

    // Convert meshes
    model->num_meshes = data->meshes_count;
    model->meshes = calloc(data->meshes_count, sizeof(mesh_t));
    for (size_t i = 0; i < data->meshes_count; i++)
    {
        if (flag_verbose) {
            if (data->meshes[i].name != NULL) {
                printf("Converting mesh %s\n", data->meshes[i].name);
            } else {
                printf("Converting mesh %ld\n", i);
            }
        }

        if (convert_mesh(&data->meshes[i], &model->meshes[i]) != 0) {
            if (data->meshes[i].name != NULL) {
                fprintf(stderr, "Error: failed converting mesh %s\n", data->meshes[i].name);
            } else {
                fprintf(stderr, "Error: failed converting mesh %ld\n", i);
            }
            goto error;
        }
    }

    // Write output file
    FILE *out = fopen(outfn, "wb");
    if (!out) {
        fprintf(stderr, "cannot open output file: %s\n", outfn);
        goto error;
    }
    model64_write(model, out);
    fclose(out);

    model64_free(model);
    cgltf_free(data);
    return 0;

error:
    model64_free(model);
    cgltf_free(data);
    return 1;
}

int main(int argc, char *argv[])
{
    char *infn = NULL, *outdir = ".", *outfn = NULL;
    bool error = false;

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

        asprintf(&outfn, "%s/%s.model64", outdir, basename_noext);
        if (flag_verbose)
            printf("Converting: %s -> %s\n",
                infn, outfn);
        if (convert(infn, outfn) != 0) {
            error = true;
        }
        free(outfn);
    }

    return error ? 1 : 0;
}