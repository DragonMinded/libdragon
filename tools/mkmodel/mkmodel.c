#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "../common/binout.c"
#include "../common/binout.h"

#include "../../include/GL/gl_enums.h"
#include "../../src/model64_internal.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

//Macros copied from utils.h in libdragon src directory
#define ROUND_UP(n, d) ({ \
    typeof(n) _n = n; typeof(d) _d = d; \
    (((_n) + (_d) - 1) / (_d) * (_d)); \
})
#define MAX(a,b)  ({ typeof(a) _a = a; typeof(b) _b = b; _a > _b ? _a : _b; })

// Update these when changing code that writes to the output file
// IMPORTANT: Do not attempt to move these values to a header that is shared by mkmodel and runtime code!
//            These values must reflect what the tool actually outputs.
#define HEADER_SIZE         80
#define MESH_SIZE           8
#define PRIMITIVE_SIZE      108
#define NODE_SIZE           128
#define SKIN_SIZE           8
#define ANIM_SIZE           28


#define ATTRIBUTE_COUNT     5

#define VERTEX_PRECISION    5
#define TEXCOORD_PRECISION  8

#define RSP_PRECISION       (1.0f/65536.0f)

typedef void (*component_convert_func_t)(void*,float*,size_t);
typedef void (*index_convert_func_t)(void*,cgltf_uint*,size_t);

typedef struct gltf_anim_channel_s {
    float *time;
    float *output;
    float *sampled_output;
    size_t out_count;
    size_t out_num_components;
    size_t num_keyframes;
    cgltf_interpolation_type interpolation;
    cgltf_animation_path_type target_path;
    size_t node_index;
} gltf_anim_channel_t;

float flag_anim_fps = 30.0f;
int flag_anim_stream = 1;
int flag_verbose = 0;

uint32_t get_type_size(uint32_t type)
{
    switch (type) {
    case GL_BYTE:
        return sizeof(int8_t);
    case GL_UNSIGNED_BYTE:
        return sizeof(uint8_t);
    case GL_SHORT:
        return sizeof(int16_t);
    case GL_UNSIGNED_SHORT:
        return sizeof(uint16_t);
    case GL_INT:
        return sizeof(int32_t);
    case GL_UNSIGNED_INT:
        return sizeof(uint32_t);
    case GL_FLOAT:
        return sizeof(float);
    case GL_DOUBLE:
        return sizeof(double);
    case GL_HALF_FIXED_N64:
        return sizeof(int16_t);
    default:
        return 0;
    }
}

void print_args( char * name )
{
    fprintf(stderr, "mkmodel -- Convert glTF 2.0 models into the model64 format for libdragon\n\n");
    fprintf(stderr, "Usage: %s [flags] <input files...>\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -o/--output <dir>       Specify output directory (default: .)\n");
    fprintf(stderr, "   --anim-no-stream        Output animation data for not streaming.\n");
    fprintf(stderr, "   --anim-fps <value>      Frame rate of the animation (default: 30.0)\n");
    fprintf(stderr, "   -v/--verbose            Verbose output\n");
    fprintf(stderr, "\n");
}

model64_data_t* model64_alloc()
{
    model64_data_t *model = calloc(1, sizeof(model64_data_t));
    model->magic = MODEL64_MAGIC;
    model->version = MODEL64_VERSION;
    model->header_size = HEADER_SIZE;
    model->mesh_size = MESH_SIZE;
    model->primitive_size = PRIMITIVE_SIZE;
    model->node_size = NODE_SIZE;
    model->skin_size = SKIN_SIZE;
    model->anim_size = ANIM_SIZE;
    return model;
}

void primitive_free(primitive_t *primitive)
{
    if (primitive->position.pointer) free(primitive->position.pointer);
    if (primitive->color.pointer) free(primitive->color.pointer);
    if (primitive->texcoord.pointer) free(primitive->texcoord.pointer);
    if (primitive->normal.pointer) free(primitive->normal.pointer);
    if (primitive->mtx_index.pointer) free(primitive->mtx_index.pointer);
    if (primitive->indices) free(primitive->indices);
}

void mesh_free(mesh_t *mesh)
{
    for (size_t i = 0; i < mesh->num_primitives; i++) {
        primitive_free(&mesh->primitives[i]);
    }
    if (mesh->primitives) {
        free(mesh->primitives);
    }
}

void node_free(model64_node_t *node)
{
    if(node->name) {
        free(node->name);
    }
    if(node->children) {
        free(node->children);
    }
}

void anim_free(model64_anim_t *anim)
{
    if(anim->name) {
        free(anim->name);
    }
    if(anim->data) {
        free(anim->data);
    }
    if(anim->channels) {
        free(anim->channels);
    }
}


void model64_free(model64_data_t *model)
{
    for (size_t i = 0; i < model->num_nodes; i++) {
        node_free(&model->nodes[i]);
    }  
    for (size_t i = 0; i < model->num_skins; i++) {
        free(model->skins[i].joints);
    }
    for (size_t i = 0; i < model->num_meshes; i++) {
        mesh_free(&model->meshes[i]);
    }
    for (size_t i = 0; i < model->num_anims; i++) {
        anim_free(&model->anims[i]);
    }
    if (model->meshes) {
        free(model->meshes);
    }
    if (model->skins) {
        free(model->skins);
    }
    if (model->nodes) {
        free(model->nodes);
    }
    if (model->anims) {
        free(model->anims);
    }
    free(model);
}

void attribute_write(FILE *out, attribute_t *attr, const char *name_format, ...)
{
    va_list args;
    va_start(args, name_format);
    w32(out, attr->size);
    w32(out, attr->type);
    w32(out, attr->stride);
    w32_placeholdervf(out, name_format, args);
    va_end(args);
}

uint32_t attribute_get_data_size(attribute_t *attr)
{
    if(!attr->pointer) {
        return 0;
    }
    return get_type_size(attr->type) * attr->size;
}

void vertex_write(FILE *out, attribute_t *attr, uint32_t index)
{
    if (attr->size == 0) return;
    
    switch (attr->type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
        for (size_t i = 0; i < attr->size; i++) w8(out, ((uint8_t*)attr->pointer)[index * attr->size + i]);
        break;
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
    case GL_HALF_FIXED_N64:
        for (size_t i = 0; i < attr->size; i++) w16(out, ((uint16_t*)attr->pointer)[index * attr->size + i]);
        break;
    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_FLOAT:
        for (size_t i = 0; i < attr->size; i++) w32(out, ((uint32_t*)attr->pointer)[index * attr->size + i]);
        break;
    default:
        break;
    }
}

uint32_t indices_get_data_size(uint32_t type, uint32_t count)
{
    switch (type) {
        case GL_UNSIGNED_BYTE:
            return count;
            
        case GL_UNSIGNED_INT:
            return count*4;
            
        default:
            return count*2;
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

uint32_t get_mesh_index(model64_data_t *model, mesh_t *mesh)
{
    assert(mesh);
    uint32_t index = mesh-(model->meshes);
    assert(index < model->num_meshes);
    return index;
}

uint32_t get_skin_index(model64_data_t *model, model64_skin_t *skin)
{
    assert(skin);
    uint32_t index = skin-(model->skins);
    assert(index < model->num_skins);
    return index;
}

void write_matrix(float *mtx, FILE *out)
{
    for(int i=0; i<16; i++) {
        wf32approx(out, mtx[i], RSP_PRECISION);
    }
}

void model64_write_header(model64_data_t *model, FILE *out)
{
    int start_ofs = ftell(out);
    w32(out, model->magic);
    w32(out, 0);
    w32(out, model->version);
    w32(out, model->header_size);
    w32(out, model->mesh_size);
    w32(out, model->primitive_size);
    w32(out, model->node_size);
    w32(out, model->skin_size);
    w32(out, model->anim_size);
    w32(out, model->num_nodes);
    w32_placeholderf(out, "nodes");
    w32(out, model->root_node);
    w32(out, model->num_skins);
    w32_placeholderf(out, "skins");
    w32(out, model->num_meshes);
    w32_placeholderf(out, "meshes");
    w32(out, model->num_anims);
    w32_placeholderf(out, "anims");
    w32(out, model->stream_buf_size);
    w32(out, 0);
    assert(ftell(out)-start_ofs == HEADER_SIZE);
}

void model64_write_skins(model64_data_t *model, FILE *out)
{
    walign(out, 4);
    placeholder_set(out, "skins");
    for(uint32_t i=0; i<model->num_skins; i++) {
        int skin_ofs = ftell(out);
        placeholder_set(out, "skin%d", i);
        w32(out, model->skins[i].num_joints);
        w32_placeholderf(out, "skin%d_joints", i);
        assert(ftell(out)-skin_ofs == SKIN_SIZE);
    }
    for(uint32_t i=0; i<model->num_skins; i++) {
        placeholder_set(out, "skin%d_joints", i);
        for(uint32_t j=0; j<model->skins[i].num_joints; j++) {
            w32(out, model->skins[i].joints[j].node_idx);
            write_matrix(model->skins[i].joints[j].inverse_bind_mtx, out);
        }
    }
}

uint32_t model64_get_node_children_total(model64_data_t *model)
{
    uint32_t num_children = 0;
    for(uint32_t i=0; i<model->num_nodes; i++) {
        num_children += model->nodes[i].num_children;
    }
    return num_children;
}

uint32_t model64_get_primitive_total(model64_data_t *model)
{
    uint32_t num_primitives = 0;
    for(uint32_t i=0; i<model->num_meshes; i++) {
        num_primitives += model->meshes[i].num_primitives;
    }
    return num_primitives;
}

void write_node_transform(node_transform_t *transform, FILE *out)
{
    for(int i=0; i<3; i++) {
        wf32approx(out, transform->pos[i], RSP_PRECISION);
    }
    for(int i=0; i<4; i++) {
        wf32approx(out, transform->rot[i], RSP_PRECISION);
    }
    for(int i=0; i<3; i++) {
        wf32approx(out, transform->scale[i], RSP_PRECISION);
    }
    write_matrix(transform->mtx, out);
}

void model64_write_node(model64_data_t *model, FILE *out, uint32_t index)
{
    int start_ofs = ftell(out);
    w32_placeholderf(out, "node%d_name", index);
    if(model->nodes[index].mesh) {
        w32_placeholderf(out, "mesh%d", get_mesh_index(model, model->nodes[index].mesh));
    } else {
        w32(out, 0);
    }
    if(model->nodes[index].skin) {
        w32_placeholderf(out, "skin%d", get_skin_index(model, model->nodes[index].skin));
    } else {
        w32(out, 0);
    }
    write_node_transform(&model->nodes[index].transform, out);
    w32(out, model->nodes[index].parent);
    w32(out, model->nodes[index].num_children);
    w32_placeholderf(out, "node%d_children", index);
    assert(ftell(out)-start_ofs == NODE_SIZE);
}

void model64_write_nodes(model64_data_t *model, FILE *out)
{
    walign(out, 4);
    placeholder_set(out, "nodes");
    for(uint32_t i=0; i<model->num_nodes; i++) {
        model64_write_node(model, out, i);
    }
    for(uint32_t i=0; i<model->num_nodes; i++) {
        placeholder_set(out, "node%d_children", i);
        for(uint32_t j=0; j<model->nodes[i].num_children; j++) {
            w32(out, model->nodes[i].children[j]);
        }
    }
    for(uint32_t i=0; i<model->num_nodes; i++)
    {
        if(model->nodes[i].name) {
            placeholder_set(out, "node%d_name", i);
            fwrite(model->nodes[i].name, strlen(model->nodes[i].name)+1, 1, out);
        }
    }
}

void model64_write_meshes(model64_data_t *model, FILE *out)
{
    walign(out, 4);
    placeholder_set(out, "meshes");
    for(uint32_t i=0; i<model->num_meshes; i++) {
        int start_ofs = ftell(out);
        placeholder_set(out, "mesh%d", i);
        w32(out, model->meshes[i].num_primitives);
        w32_placeholderf(out, "mesh%d_primitives", i);
        assert(ftell(out)-start_ofs == MESH_SIZE);
    }
    for(uint32_t i=0; i<model->num_meshes; i++) {
        placeholder_set(out, "mesh%d_primitives", i);
        for(uint32_t j=0; j<model->meshes[i].num_primitives; j++) {
            primitive_t *primitive = &model->meshes[i].primitives[j];
            int start_ofs = ftell(out);
            w32(out, primitive->mode);
            
            attribute_write(out, &primitive->position, "mesh%d_primitive%d_position", i, j);
            attribute_write(out, &primitive->color, "mesh%d_primitive%d_color", i, j);
            attribute_write(out, &primitive->texcoord, "mesh%d_primitive%d_texcoord", i, j);
            attribute_write(out, &primitive->normal, "mesh%d_primitive%d_normal", i, j);
            attribute_write(out, &primitive->mtx_index, "mesh%d_primitive%d_mtx_index", i, j);
            
            w32(out, primitive->vertex_precision);
            w32(out, primitive->texcoord_precision);
            w32(out, primitive->index_type);
            w32(out, primitive->num_vertices);
            w32(out, primitive->num_indices);
            w32_placeholderf(out, "mesh%d_primitive%d_index", i, j);
            assert(ftell(out)-start_ofs == PRIMITIVE_SIZE);
        }
    }
    for(uint32_t i=0; i<model->num_meshes; i++) {
        for(uint32_t j=0; j<model->meshes[i].num_primitives; j++) {
            primitive_t *primitive = &model->meshes[i].primitives[j];
            for (size_t k = 0; k < primitive->num_vertices; k++) {
                if(primitive->position.pointer && k == 0) {
                    placeholder_set(out, "mesh%d_primitive%d_position", i, j);
                }
                vertex_write(out, &primitive->position, k);
                if(primitive->color.pointer && k == 0) {
                    placeholder_set(out, "mesh%d_primitive%d_color", i, j);
                }
                vertex_write(out, &primitive->color, k);
                if(primitive->texcoord.pointer && k == 0) {
                    placeholder_set(out, "mesh%d_primitive%d_texcoord", i, j);
                }
                vertex_write(out, &primitive->texcoord, k);
                if(primitive->normal.pointer && k == 0) {
                    placeholder_set(out, "mesh%d_primitive%d_normal", i, j);
                }
                vertex_write(out, &primitive->normal, k);
                if(primitive->mtx_index.pointer && k == 0) {
                    placeholder_set(out, "mesh%d_primitive%d_mtx_index", i, j);
                }
                vertex_write(out, &primitive->mtx_index, k);
                
            }
            if(primitive->num_indices > 0) {
                walign(out, 4);
                placeholder_set(out, "mesh%d_primitive%d_index", i, j);
                indices_write(out, primitive->index_type, primitive->indices, primitive->num_indices);
            }
        }
    }
}

void model64_write_anim_data(model64_anim_t *anim, FILE *out)
{
    uint8_t *data = anim->data;
    for(uint32_t i=0; i<anim->num_frames; i++) {
        for(uint32_t j=0; j<anim->num_channels; j++) {
            float *float_data = (float *)data;
            switch(anim->channels[j] >> 30) {
                case ANIM_COMPONENT_POS:
                    wf32approx(out, float_data[0], RSP_PRECISION);
                    wf32approx(out, float_data[1], RSP_PRECISION);
                    wf32approx(out, float_data[2], RSP_PRECISION);
                    data += 3*sizeof(float);
                    break;
                    
                case ANIM_COMPONENT_ROT:
                    wf32approx(out, float_data[0], RSP_PRECISION);
                    wf32approx(out, float_data[1], RSP_PRECISION);
                    wf32approx(out, float_data[2], RSP_PRECISION);
                    wf32approx(out, float_data[3], RSP_PRECISION);
                    data += 4*sizeof(float);
                    break;
                    
                case ANIM_COMPONENT_SCALE:
                    wf32approx(out, float_data[0], RSP_PRECISION);
                    wf32approx(out, float_data[1], RSP_PRECISION);
                    wf32approx(out, float_data[2], RSP_PRECISION);
                    data += 3*sizeof(float);
                    break;
            }
        }
    }
}

void model64_write_anims(model64_data_t *model, FILE *out, FILE *anim_out)
{
    if(!anim_out) {
        anim_out = out;
    }
    walign(out, 4);
    placeholder_set(out, "anims");
    for(uint32_t i=0; i<model->num_anims; i++) {
        w32_placeholderf(out, "anim%d_name", i);
        wf32(out, model->anims[i].frame_rate);
        w32(out, model->anims[i].num_frames);
        w32(out, model->anims[i].frame_size);
        w32_placeholderf(out, "anim%d_data", i);
        w32_placeholderf(out, "anim%d_channels", i);
        w32(out, model->anims[i].num_channels);
    }
    for(uint32_t i=0; i<model->num_anims; i++) {
        placeholder_set(out, "anim%d_channels", i);
        for(uint32_t j=0; j<model->anims[i].num_channels; j++) {
            w32(out, model->anims[i].channels[j]);
        }
    }
    for(uint32_t i=0; i<model->num_anims; i++) {
        char *anim_name = model->anims[i].name;
        if(anim_name) {
            placeholder_set(out, "anim%d_name", i);
            fwrite(anim_name, strlen(anim_name)+1, 1, out);
        }
    }
    for(uint32_t i=0; i<model->num_anims; i++) {
        walign(anim_out, 4);
        placeholder_set_offset(out, ftell(anim_out), "anim%d_data", i);
        model64_write_anim_data(&model->anims[i], anim_out);
    }
}

void model64_write(model64_data_t *model, FILE *out, FILE *anim_out)
{
    model64_write_header(model, out);
    model64_write_meshes(model, out);
    model64_write_nodes(model, out);
    model64_write_skins(model, out);
    model64_write_anims(model, out, anim_out);
    placeholder_clear();
}

int convert_attribute_data(cgltf_accessor *accessor, attribute_t *attr, component_convert_func_t convert_func)
{
    size_t num_components = cgltf_num_components(accessor->type);
    size_t num_values = num_components * accessor->count;
    float *temp_buffer = malloc(sizeof(float) * num_values);

    // Convert all data to floats (because cgltf provides this very convenient function)
    // TODO: More sophisticated conversion that doesn't always use floats as intermediate values
    //       Might not be worth it since the majority of tools will probably only export floats anyway?
    if (cgltf_accessor_unpack_floats(accessor, temp_buffer, num_values) == 0) {
        fprintf(stderr, "Error: failed reading attribute data\n");
        free(temp_buffer);
        return 1;
    }

    // Allocate storage for converted values
    uint32_t component_size = get_type_size(attr->type);
    attr->pointer = calloc(num_values, component_size);
    attr->stride = num_components * component_size;

    // Convert floats to the target format
    for (size_t i = 0; i < accessor->count; i++)
    {
        uint8_t *dst = (uint8_t*)attr->pointer + num_components * component_size * i;
        float *src = &temp_buffer[i * num_components];
        convert_func(dst, src, num_components);
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
    for (size_t i = 0; i < size; i++) {
        // Pre-gamma-correct vertex colors (excluding alpha)
        float v = i < 3 ? powf(value[i], 1.0f/2.2f) : value[i];
        dst[i] = v * 0xFF;
    }
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

void convert_weights(float *dst, float *value, size_t size)
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

int is_rigid_skinned(attribute_t *weight_attr, uint32_t num_vertices)
{
    if(!weight_attr->pointer || weight_attr->size == 0)
    {
        return 1;
    }
    float *data = weight_attr->pointer;
    for(uint32_t i=0; i<num_vertices; i++)
    {
        float *buffer = &data[i*weight_attr->size];
        uint32_t num_used_weights = 0;
        for(uint32_t j=0; j<weight_attr->size; j++)
        {
            if(buffer[j] != 0)
            {
                num_used_weights++;
            }
        }
        if(num_used_weights > 1)
        {
            return 0;
        }
    }
    return 1;
}

void simplify_mtx_index_buffer(attribute_t *mtx_index_attr, attribute_t *weight_attr, uint32_t num_vertices)
{
    if(!mtx_index_attr->pointer || mtx_index_attr->size == 0)
    {
        return;
    }
    uint8_t *new_buffer = calloc(num_vertices, 1);
    uint8_t *old_buffer = mtx_index_attr->pointer;
    float *weight_buffer = weight_attr->pointer;
    for(uint32_t i=0; i<num_vertices; i++)
    {
        uint32_t max_weight_idx = 0;
        float *weights = &weight_buffer[i*weight_attr->size];
        for(uint32_t i=0; i<weight_attr->size; i++)
        {
            if(weights[i] > weights[max_weight_idx])
            {
                max_weight_idx = i;
            }
        }
        new_buffer[i] = old_buffer[(i*mtx_index_attr->size)+max_weight_idx];
    }
    free(mtx_index_attr->pointer);
    mtx_index_attr->pointer = new_buffer;
    mtx_index_attr->size = 1;
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

    static const component_convert_func_t attr_convert_funcs[] = {
        (component_convert_func_t)convert_position,
        (component_convert_func_t)convert_color,
        (component_convert_func_t)convert_texcoord,
        (component_convert_func_t)convert_normal,
        (component_convert_func_t)convert_mtx_index
    };
    
    attribute_t weight_attr = {};
    attribute_t *attrs[] = {
        &out_primitive->position,
        &out_primitive->color,
        &out_primitive->texcoord,
        &out_primitive->normal,
        &out_primitive->mtx_index,
    };

    cgltf_attribute *attr_map[ATTRIBUTE_COUNT] = {NULL};
    cgltf_attribute *gltf_weight_attr = NULL;
    
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
            
        case cgltf_attribute_type_weights:
            gltf_weight_attr = attr;
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

    uint32_t stride = 0;

    // Convert vertex data
    for (size_t i = 0; i < ATTRIBUTE_COUNT; i++)
    {
        if (attr_map[i] == NULL) continue;
        attrs[i]->size = cgltf_num_components(attr_map[i]->data->type);
        
        if (attrs[i]->size == 0) continue;
        attrs[i]->type = attr_types[i];

        if (convert_attribute_data(attr_map[i]->data, attrs[i], attr_convert_funcs[i]) != 0) {
            fprintf(stderr, "Error: failed converting data of attribute %d\n", attr_map[i]->index);
            return 1;
        }

        stride += attrs[i]->stride;
    }

    for (size_t i = 0; i < ATTRIBUTE_COUNT; i++)
    {
        if (attrs[i]->size == 0) continue;
        attrs[i]->stride = stride;
    }
    if(gltf_weight_attr)
    {
        weight_attr.size = cgltf_num_components(gltf_weight_attr->data->type);
        
        if (weight_attr.size != 0)
        {
            weight_attr.type = GL_FLOAT;

            if (convert_attribute_data(gltf_weight_attr->data, &weight_attr, (component_convert_func_t)convert_weights) != 0) {
                fprintf(stderr, "Error: failed converting data of attribute %d\n", gltf_weight_attr->index);
                return 1;
            }
        }
        
    }
    
    if(!is_rigid_skinned(&weight_attr, out_primitive->num_vertices))
    {
        fprintf(stderr, "Error: Model is not rigidly skinned\n");
        free(weight_attr.pointer);
        return 1;
    }
    else
    {
        uint32_t mtxindex_orig_size = attrs[4]->size;
        simplify_mtx_index_buffer(attrs[4], &weight_attr, out_primitive->num_vertices);
        if(attrs[4]->pointer && attrs[4]->size > 0)
        {
            for(int i=0; i<ATTRIBUTE_COUNT; i++) {
                attrs[i]->stride -= mtxindex_orig_size-1;
            }
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
            free(weight_attr.pointer);
            return 1;
        }

        // Convert indices
        convert_func(out_primitive->indices, temp_indices, in_indices->count);

        free(temp_indices);
    }
    free(weight_attr.pointer);
    return 0;
}

void make_node_idx_list(cgltf_data *data, cgltf_node **node_list, cgltf_size num_nodes, uint32_t **idx_list)
{
    uint32_t *list = calloc(num_nodes, sizeof(uint32_t));
    for(size_t i=0; i<num_nodes; i++) {
        list[i] = cgltf_node_index(data, node_list[i]);
    }
    *idx_list = list;
}

int convert_mesh(cgltf_mesh *in_mesh, mesh_t *out_mesh)
{
    // Convert primitives
    out_mesh->num_primitives = in_mesh->primitives_count;
    out_mesh->primitives = calloc(in_mesh->primitives_count, sizeof(primitive_t));
    for (size_t i = 0; i < in_mesh->primitives_count; i++)
    {
        if (flag_verbose) {
            printf("Converting primitive %zd\n", i);
        }

        if (convert_primitive(&in_mesh->primitives[i], &out_mesh->primitives[i]) != 0) {
            fprintf(stderr, "Error: failed converting primitive %zd\n", i);
            return 1;
        }
    }

    return 0;
}

void convert_node(cgltf_data *data, model64_data_t *model_data, cgltf_node *in_node, model64_node_t *out_node)
{
    if(in_node->name && in_node->name[0] != '\0') {
        out_node->name = strdup(in_node->name);
    }
    if(in_node->mesh) {
        out_node->mesh = &model_data->meshes[cgltf_mesh_index(data, in_node->mesh)];
    }
    if(in_node->skin) {
        out_node->skin = &model_data->skins[cgltf_skin_index(data, in_node->skin)];
    }
    if(in_node->parent) {
        out_node->parent = cgltf_node_index(data, in_node->parent);
    } else {
        out_node->parent = model_data->num_nodes;
    }
    out_node->num_children = in_node->children_count;
    if(in_node->children_count > 0) {
        make_node_idx_list(data, in_node->children, in_node->children_count, &out_node->children);
    }
    //Copy translation
    out_node->transform.pos[0] = in_node->translation[0];
    out_node->transform.pos[1] = in_node->translation[1];
    out_node->transform.pos[2] = in_node->translation[2];
    //Copy rotation
    out_node->transform.rot[0] = in_node->rotation[0];
    out_node->transform.rot[1] = in_node->rotation[1];
    out_node->transform.rot[2] = in_node->rotation[2];
    out_node->transform.rot[3] = in_node->rotation[3];
    //Copy scale
    out_node->transform.scale[0] = in_node->scale[0];
    out_node->transform.scale[1] = in_node->scale[1];
    out_node->transform.scale[2] = in_node->scale[2];
    //Set local transform
    cgltf_node_transform_local(in_node, out_node->transform.mtx);
}

void convert_root_node(cgltf_scene *scene, cgltf_data *data, model64_data_t *model)
{
    if(scene->nodes_count > 1) {
        //Generate a node for grouping the scene
        model->root_node = data->nodes_count;
        model->nodes[model->root_node].parent = model->num_nodes;
        model->nodes[model->root_node].num_children = scene->nodes_count;
        model->nodes[model->root_node].children = calloc(scene->nodes_count, sizeof(uint32_t));
        //Initialize rotation to identity quaternion
        model->nodes[model->root_node].transform.rot[3] = 1.0f;
        //Initialize scale to default
        model->nodes[model->root_node].transform.scale[0] = 1.0f;
        model->nodes[model->root_node].transform.scale[1] = 1.0f;
        model->nodes[model->root_node].transform.scale[2] = 1.0f;
        //Initialize local matrix to identity
        model->nodes[model->root_node].transform.mtx[0] = 1.0f;
        model->nodes[model->root_node].transform.mtx[5] = 1.0f;
        model->nodes[model->root_node].transform.mtx[10] = 1.0f;
        model->nodes[model->root_node].transform.mtx[15] = 1.0f;
        make_node_idx_list(data, scene->nodes, scene->nodes_count, &model->nodes[model->root_node].children);
        //Reassign parent nodes of scene nodes to generated node
        for(uint32_t i=0; i<scene->nodes_count; i++) {
            model->nodes[cgltf_node_index(data, scene->nodes[i])].parent = model->root_node;
        }
    } else {
        model->root_node = cgltf_node_index(data, scene->nodes[0]);
    }
}

int convert_skin(cgltf_data *data, cgltf_skin *in_skin, model64_skin_t *out_skin)
{
    if(in_skin->joints_count > 24) {
        if (in_skin->name != NULL) {
            fprintf(stderr, "Error: Found %zd joints in skin %s.\n", in_skin->joints_count, in_skin->name);
        } else {
            fprintf(stderr, "Error: Found %zd joints in skin %zd.\n", in_skin->joints_count, cgltf_skin_index(data, in_skin));
        }
        fprintf(stderr, "Error: A maximum of 24 joints are allowed in a skin.\n");
        return 1;
    }
    out_skin->num_joints = in_skin->joints_count;
    out_skin->joints = calloc(in_skin->joints_count, sizeof(model64_joint_t));
    cgltf_accessor *ibm_accessor = in_skin->inverse_bind_matrices;
    float *ibm_buffer = NULL;
    if(ibm_accessor) {
        size_t num_components = cgltf_num_components(ibm_accessor->type);
        size_t num_values = num_components * ibm_accessor->count;
        ibm_buffer = malloc(sizeof(float) * num_values);

        // Convert all data to floats (because cgltf provides this very convenient function)
        // TODO: More sophisticated conversion that doesn't always use floats as intermediate values
        //       Might not be worth it since the majority of tools will probably only export floats anyway?
        if (cgltf_accessor_unpack_floats(ibm_accessor, ibm_buffer, num_values) == 0) {
            fprintf(stderr, "Error: failed reading inverse bind matrices.\n");
            free(ibm_buffer);
            return 1;
        }
    }
    
    for(uint32_t i=0; i<out_skin->num_joints; i++) {
        out_skin->joints[i].node_idx = cgltf_node_index(data, in_skin->joints[i]);
        if(ibm_buffer) {
            memcpy(out_skin->joints[i].inverse_bind_mtx, &ibm_buffer[i*16], sizeof(float)*16);
        } else {
            out_skin->joints[i].inverse_bind_mtx[0] = 1.0f;
            out_skin->joints[i].inverse_bind_mtx[5] = 1.0f;
            out_skin->joints[i].inverse_bind_mtx[10] = 1.0f;
            out_skin->joints[i].inverse_bind_mtx[15] = 1.0f;
        }
    }
    if(ibm_buffer) {
        free(ibm_buffer);
    }
    return 0;
}

void normalize_vector(float *vec, size_t num_elements)
{
    float mag2 = 0.0f;
    for(size_t i=0; i<num_elements; i++) {
        mag2 += vec[i]*vec[i];
    }
    float scale = 1.0f/sqrtf(mag2);
    for(size_t i=0; i<num_elements; i++) {
        vec[i] *= scale;
    }
}


void lerp_vector(float *out, float *in1, float *in2, size_t num_elements, float time)
{
    for(size_t i=0; i<num_elements; i++) {
        out[i] = ((1-time)*in1[i])+(time*in2[i]);
    }
}

void slerp_vector(float *out, float *in1, float *in2, float time)
{
    float cosTheta = (in1[0]*in2[0])+(in1[1]*in2[1])+(in1[2]*in2[2])+(in1[3]*in2[3]);
    if(fabs(cosTheta) >= 1.0f) {
        memcpy(out, in1, 4*sizeof(float));
        return;
    }
    float alpha = acosf(fabsf(cosTheta));
    float sign = 1;
    if(cosTheta < 0) {
        sign = -1;
    }
    if(sqrtf(1-(cosTheta*cosTheta)) < 0.001f) {
        lerp_vector(out, in1, in2, 4, time);
        return;
    }
    for(int i=0; i<4; i++) {
        float outvk = (sinf(alpha*(1-time))/sinf(alpha))*in1[i];
        float outvk_next = sign*(sinf(alpha*time)/sinf(alpha))*in2[i];
        out[i] = outvk+outvk_next;
    }
}

void spline_interpolate_vector(float *out, float *in1, float *in2, float *in_tangent, float *out_tangent, size_t num_elements, float time, float duration)
{
    float t2 = time*time;
    float t3 = t2*time;
    for(size_t i=0; i<num_elements; i++) {
        float term1, term2, term3, term4;
        term1 = ((2*t3)-(3*t2)+1)*in1[i];
        term2 = duration*(t3-(2*t2)+time)*out_tangent[i];
        term3 = ((-2*t3)+(3*t2))*in2[i];
        term4 = duration*(t3-t2)*in_tangent[i];
        out[i] = term1+term2+term3+term4;
    }
}

int read_anim_channel(cgltf_data *data, cgltf_animation_channel *in_channel, gltf_anim_channel_t *out_channel)
{
    cgltf_accessor *time_accessor = in_channel->sampler->input;
    cgltf_accessor *output_accessor = in_channel->sampler->output;
    size_t num_values = time_accessor->count;
    out_channel->num_keyframes = time_accessor->count;
    out_channel->time = calloc(time_accessor->count, sizeof(float));
    if(cgltf_accessor_unpack_floats(time_accessor, out_channel->time, time_accessor->count) == 0) {
        fprintf(stderr, "Error: failed reading channel time accessor.\n");
        return 1;
    }
    out_channel->interpolation = in_channel->sampler->interpolation;
    out_channel->target_path = in_channel->target_path;
    out_channel->node_index = cgltf_node_index(data, in_channel->target_node);
    out_channel->out_num_components = cgltf_num_components(output_accessor->type);
    if(out_channel->out_num_components != 4 && out_channel->target_path == cgltf_animation_path_type_rotation) {
        fprintf(stderr, "Error: Rotation paths must be made of 4-component vectors.\n");
        return 1;
    }
    num_values *= cgltf_num_components(output_accessor->type);
    out_channel->out_count = 1;
    if(out_channel->interpolation == cgltf_interpolation_type_cubic_spline) {
        out_channel->out_count = 3;
        num_values *= 3;
    }
    out_channel->output = calloc(num_values, sizeof(float));
    if(cgltf_accessor_unpack_floats(output_accessor, out_channel->output, num_values) == 0) {
        fprintf(stderr, "Error: failed reading channel output accessor.\n");
        return 1;
    }
    return 0;
}

void sample_anim_channel(gltf_anim_channel_t *channel, float time, float *out)
{
    size_t src_pitch = channel->out_num_components*channel->out_count;
    size_t num_components = channel->out_num_components;
    if(time <= channel->time[0]) {
        float *src = channel->output;
        if(channel->interpolation == cgltf_interpolation_type_cubic_spline) {
            src += num_components;
        }
        memcpy(out, src, num_components*sizeof(float));
        return;
    }
    if(time >= channel->time[channel->num_keyframes-1]) {
        float *src = &channel->output[src_pitch*(channel->num_keyframes-1)];
        if(channel->interpolation == cgltf_interpolation_type_cubic_spline) {
            src += num_components;
        }
        memcpy(out, src, num_components*sizeof(float));
        return;
    }
    for(size_t i=0; i<channel->num_keyframes-1; i++) {
        if(time >= channel->time[i] && time <= channel->time[i+1]) {
            float *curr = &channel->output[src_pitch*i];
            float *next = &channel->output[src_pitch*(i+1)];
            float duration = channel->time[i+1]-channel->time[i];
            float time_factor = (time-channel->time[i])/duration;
            switch(channel->interpolation) {
                case cgltf_interpolation_type_linear:
                    if(channel->target_path == cgltf_animation_path_type_rotation) {
                        slerp_vector(out, curr, next, time_factor);
                    } else {
                        lerp_vector(out, curr, next, num_components, time_factor);
                    }
                    break;
                    
                case cgltf_interpolation_type_cubic_spline:
                    spline_interpolate_vector(out, curr, next, &curr[2*num_components], next, num_components, time, duration);
                    if(channel->target_path == cgltf_animation_path_type_rotation) {
                        normalize_vector(out, num_components);
                    }
                    break;
                    
                case cgltf_interpolation_type_step:
                default:
                    memcpy(out, curr, num_components*sizeof(float));
                    break;
            }
        }
    }
}

void make_anim_channel_samples(gltf_anim_channel_t *channel, float fps, uint32_t num_frames)
{
    float sample[16] = {0};
    size_t num_components = channel->out_num_components;
    channel->sampled_output = calloc(num_frames, sizeof(float)*num_components);
    for(size_t i=0; i<num_frames; i++) {
        sample_anim_channel(channel, (float)i/fps, sample);
        memcpy(&channel->sampled_output[i*num_components], sample, num_components*sizeof(float));
    }
}

void delete_gltf_anim_channel(gltf_anim_channel_t *channel)
{
    if(channel->time) {
        free(channel->time);
    }
    if(channel->output) {
        free(channel->output);
    }
    if(channel->sampled_output) {
        free(channel->sampled_output);
    }
}

void delete_gltf_anim_channel_array(gltf_anim_channel_t *channels, size_t num_channels)
{
    for(size_t i=0; i<num_channels; i++) {
        delete_gltf_anim_channel(&channels[i]);
    }
    free(channels);
}

uint32_t get_frame_size(gltf_anim_channel_t *channels, size_t num_channels)
{
    uint32_t frame_size = 0;
    for(size_t i=0; i<num_channels; i++) {
        frame_size += channels[i].out_num_components*sizeof(float);
    }
    return frame_size;
}

void make_anim_data(model64_anim_t *out_anim, gltf_anim_channel_t *channels, size_t num_channels)
{
    out_anim->frame_size = get_frame_size(channels, num_channels);
    out_anim->data = calloc(out_anim->num_frames, out_anim->frame_size);
    uint8_t *dst = out_anim->data;
    for(uint32_t i=0; i<out_anim->num_frames; i++) {
        for(size_t j=0; j<num_channels; j++) {
            size_t data_size = 0;
            size_t num_components = channels[j].out_num_components;
            switch(out_anim->channels[j] >> 30) {
                case ANIM_COMPONENT_POS:
                    data_size = 3*sizeof(float);
                    break;
                    
                case ANIM_COMPONENT_ROT:
                    data_size = 4*sizeof(float);
                    break;
                    
                case ANIM_COMPONENT_SCALE:
                    data_size = 3*sizeof(float);
                    break;
            }
            memcpy(dst, &channels[j].sampled_output[i*num_components], data_size);
            dst += data_size;
        }
    }
}

int convert_animation(cgltf_data *data, cgltf_animation *in_anim, model64_anim_t *out_anim)
{
    gltf_anim_channel_t *channels = calloc(in_anim->channels_count, sizeof(gltf_anim_channel_t));
    float max_time = 0;
    if(in_anim->name && in_anim->name[0] != '\0') {
        out_anim->name = strdup(in_anim->name);
    }
    for(size_t i=0; i<in_anim->channels_count; i++) {
        if(flag_verbose) {
            printf("Converting animation channel %zd\n", i);
        }
        if(read_anim_channel(data, &in_anim->channels[i], &channels[i]) != 0) {
            fprintf(stderr, "Error: failed converting animation channel %zd\n", i);
            goto error;
        }
        if(max_time <= channels[i].time[channels[i].num_keyframes-1]) {
            max_time = channels[i].time[channels[i].num_keyframes-1];
        }
    }
    out_anim->num_channels = in_anim->channels_count;
    out_anim->channels = calloc(out_anim->num_channels, sizeof(uint32_t));
    for(uint32_t i=0; i<out_anim->num_channels; i++) {
        uint32_t component;
        switch(channels[i].target_path) {
            case cgltf_animation_path_type_translation:
                component = ANIM_COMPONENT_POS;
                break;
                
            case cgltf_animation_path_type_rotation:
                component = ANIM_COMPONENT_ROT;
                break;
                
            case cgltf_animation_path_type_scale:
                component = ANIM_COMPONENT_SCALE;
                break;
                
            default:
                fprintf(stderr, "Error: invalid animation channel target path %d\n", channels[i].target_path);
                goto error;
        }
        out_anim->channels[i] = (component << 30)|channels[i].node_index;
    }
    out_anim->frame_rate = flag_anim_fps;
    out_anim->num_frames = ceil(max_time*flag_anim_fps);
    for(size_t i=0; i<in_anim->channels_count; i++) {
        if(flag_verbose) {
            printf("Sampling animation channel %zd\n", i);
        }
        make_anim_channel_samples(&channels[i], flag_anim_fps, out_anim->num_frames);
    }
    make_anim_data(out_anim, channels, in_anim->channels_count);
    
    delete_gltf_anim_channel_array(channels, in_anim->channels_count);
    return 0;
    
    error:
    delete_gltf_anim_channel_array(channels, in_anim->channels_count);
    return 1;
}

uint32_t get_anim_stream_buf_size(model64_data_t *model)
{
    uint32_t size = 0;
    if(flag_anim_stream) {
        for(uint32_t i=0; i<model->num_anims; i++) {
            if(model->anims[i].frame_size >= size) {
                size = model->anims[i].frame_size;
            }
        }
    }
    return size;
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

    cgltf_load_buffers(&options, data, infn);

    model64_data_t *model = model64_alloc();

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
                printf("Converting mesh %zd\n", i);
            }
        }

        if (convert_mesh(&data->meshes[i], &model->meshes[i]) != 0) {
            if (data->meshes[i].name != NULL) {
                fprintf(stderr, "Error: failed converting mesh %s\n", data->meshes[i].name);
            } else {
                fprintf(stderr, "Error: failed converting mesh %zd\n", i);
            }
            goto error;
        }
    }
    // Convert skins
    model->num_skins = data->skins_count;
    if(model->num_skins != 0) {
        model->skins = calloc(data->skins_count, sizeof(model64_skin_t));
        for(size_t i=0; i<data->skins_count; i++) {
            if(data->skins[i].joints_count == 0) {
                continue;
            }
            if (flag_verbose) {
                if (data->skins[i].name != NULL) {
                    printf("Converting skin %s\n", data->skins[i].name);
                } else {
                    printf("Converting skin %zd\n", i);
                }
            }
            if(convert_skin(data, &data->skins[i], &model->skins[i]) != 0) {
                if (data->skins[i].name != NULL) {
                    fprintf(stderr, "Error: failed converting skin %s\n", data->skins[i].name);
                } else {
                    fprintf(stderr, "Error: failed converting skin %zd\n", i);
                }
                goto error;
            }
        }
    }
    // Convert nodes
    model->num_nodes = data->nodes_count;
    // Add extra node if scene has multiple root nodes
    if(data->scene->nodes_count > 1) {
        model->num_nodes++;
    }
    if(model->num_nodes != 0) {
        model->nodes = calloc(model->num_nodes, sizeof(model64_node_t));
        for(size_t i=0; i<data->nodes_count; i++) {
            if (flag_verbose) {
                if (data->nodes[i].name != NULL) {
                    printf("Converting node %s\n", data->nodes[i].name);
                } else {
                    printf("Converting node %zd\n", i);
                }
            }
            convert_node(data, model, &data->nodes[i], &model->nodes[i]);
        }
        if (flag_verbose) {
            printf("Converting root node\n");
        }
        convert_root_node(data->scene, data, model);
    }
    model->num_anims = data->animations_count;
    if(model->num_anims != 0) {
        model->anims = calloc(model->num_anims, sizeof(model64_anim_t));
        for(size_t i=0; i<data->animations_count; i++) {
            if (flag_verbose) {
                if (data->animations[i].name != NULL) {
                    printf("Converting animation %s\n", data->animations[i].name);
                } else {
                    printf("Converting animation %zd\n", i);
                }
            }
            if(convert_animation(data, &data->animations[i], &model->anims[i]) != 0) {
                if (data->animations[i].name != NULL) {
                    fprintf(stderr, "Error: failed converting animation %s\n", data->animations[i].name);
                } else {
                    fprintf(stderr, "Error: failed converting animation %zd\n", i);
                }
                goto error;
            }
        }
        model->stream_buf_size = get_anim_stream_buf_size(model);
    }
    
    // Write output file
    FILE *out = fopen(outfn, "wb");
    if (!out) {
        fprintf(stderr, "could not open output file: %s\n", outfn);
        goto error;
    }
    FILE *anim_out = NULL;
    if(flag_anim_stream && model->num_anims > 0) {
        char *animfn = NULL;
        asprintf(&animfn, "%s.anim", outfn);
        anim_out = fopen(animfn, "wb");
        if(!anim_out) {
            fprintf(stderr, "could not open output file: %s\n", animfn);
            goto error;
        }
        free(animfn);
    }
    model64_write(model, out, anim_out);
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
            } else if (!strcmp(argv[i], "--anim-no-stream")) {
                flag_anim_stream = 0;
            } else if (!strcmp(argv[i], "--anim-fps")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                char extra;
                if (sscanf(argv[i], "%f%c", &flag_anim_fps, &extra) != 1) {
                    fprintf(stderr, "invalid argument for %s: %s\n", argv[i-1], argv[i]);
                    return 1;
                }
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