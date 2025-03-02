#include "magma.h"
#include "magma_constants.h"
#include "rspq.h"
#include "rdpq.h"
#include "utils.h"
#include "rsp_magma.h"
#include "../rdpq/rdpq_internal.h"
#include "rsp_asm.h"
#include <malloc.h>

// TODO: Documentation on how magma works internally

/** @brief A pipeline instance */
typedef struct mg_pipeline_s 
{
    void *shader_code;          ///< Pointer to the duplicated and patched shader ucode text.
    uint32_t shader_code_size;  ///< Size of the duplicated and patched shader ucode text.
    uint32_t vertex_stride;     ///< Stride of the vertex layout.
    uint32_t uniform_count;     ///< Number of uniforms.
    mg_uniform_t *uniforms;     ///< List of uniforms.
} mg_pipeline_t;

/** @brief Metadata about a uniform defined by a shader. */
typedef struct
{
    uint32_t binding;   ///< Binding number of the uniform.
    uint32_t start;     ///< Start address of the uniform in DMEM, relative to the start of uniform memory.
    uint32_t end;       ///< End address of the uniform in DMEM, relative to the start of uniform memory.
} mg_meta_uniform_t;

/** @brief Metadata about a patch of a vertex attribute defined by a shader. */
typedef struct
{
    uint32_t address;       ///< Address in IMEM of the instruction to be patched, relative to the overlay address.
    uint32_t replacement;   ///< New instruction to be substituted, in case this patch is invoked.
} mg_meta_attribute_patch_t;

/** @brief Metadata about a vertex attribute defined by a shader. */
typedef struct
{
    uint32_t input;             ///< Input number of this attribute.
    uint32_t is_optional;       ///< Non-zero if this attribute is optional.
    uint32_t loaders_offset;    ///< Offset of the array of loaders, relative to the list of attributes.
    uint32_t patches_offset;    ///< Offset of the array of patches, relative to the list of attributes.
    uint32_t loader_count;      ///< Number of loaders.
    uint32_t patches_count;     ///< Number of patches.
} mg_meta_attribute_t;


/** @brief Metadata header defined by a shader. */
typedef struct
{
    uint32_t uniform_count;         ///< Number of uniforms.
    uint32_t uniforms_offset;       ///< Offset of the list of uniforms, relative to the metadata header.
    uint32_t attribute_count;       ///< Number of attributes.
    uint32_t attributes_offset;     ///< Offset of the list of attributes, relative ot the metadata header.
} mg_meta_header_t;

DEFINE_RSP_UCODE(rsp_magma);
DEFINE_RSP_UCODE(rsp_magma_clipping);

static bool is_initialized = false;

uint32_t mg_overlay_id;

void mg_uniform_load_raw(uint32_t offset, uint32_t size, const void *data)
{
    assertf(size > 0, "size must be greater than 0");
    mg_cmd_write(MG_CMD_LOAD_UNIFORM, PhysicalAddr(data), ((size-1) << 16) | offset);
}

void mg_draw_begin(void)
{
    __rdpq_autosync_use(AUTOSYNC_PIPE | AUTOSYNC_TILES | AUTOSYNC_TMEMS);
}

void mg_draw_end(void)
{
    mg_cmd_write(MG_CMD_DRAW_END);
}

void mg_load_vertices(uint32_t buffer_index, uint8_t cache_index, uint32_t count)
{
    uint32_t count2 = ROUND_UP(count, 2);
    assertf(count > 0, "count must be greater than 0");
    assertf(count2 <= MG_VERTEX_CACHE_COUNT, "too many vertices");
    assertf(cache_index + count2 <= MG_VERTEX_CACHE_COUNT, "offset out of range");
    mg_cmd_write(MG_CMD_LOAD_VERTICES, buffer_index, (cache_index<<16) | count);
}

void mg_draw_triangle(uint8_t index0, uint8_t index1, uint8_t index2)
{
    assertf(index0 <= MG_VERTEX_CACHE_COUNT, "index0 is out of range");
    assertf(index1 <= MG_VERTEX_CACHE_COUNT, "index1 is out of range");
    assertf(index2 <= MG_VERTEX_CACHE_COUNT, "index2 is out of range");

    uint16_t i0 = index0 * MG_VTX_SIZE + RSP_MAGMA_MG_VERTEX_CACHE;
    uint16_t i1 = index1 * MG_VTX_SIZE + RSP_MAGMA_MG_VERTEX_CACHE;
    uint16_t i2 = index2 * MG_VTX_SIZE + RSP_MAGMA_MG_VERTEX_CACHE;

    mg_cmd_write(MG_CMD_DRAW_INDICES, i0, (i1 << 16) | i2);
}

static void get_overlay_span(rsp_ucode_t *ucode, void **code, uint32_t *code_size)
{
    uint32_t overlay_offset = RSP_MAGMA__MG_OVERLAY - RSP_MAGMA__start;
    uint32_t ucode_size = (uint8_t*)ucode->code_end - ucode->code;

    *code = (uint8_t*)ucode->code + overlay_offset;
    *code_size = ucode_size - overlay_offset;
}

void mg_init(void)
{
    if (is_initialized) return;

    mg_overlay_id = rspq_overlay_register(&rsp_magma);

    // Pass the location and size of the clipping code overlay to the RSP state
    void* clipping_code;
    uint32_t clipping_code_size;
    get_overlay_span(&rsp_magma_clipping, &clipping_code, &clipping_code_size);
    mg_cmd_set_word(offsetof(mg_rsp_state_t, clipping_code), PhysicalAddr(clipping_code));
    mg_cmd_set_word(offsetof(mg_rsp_state_t, clipping_code_size), clipping_code_size);

    is_initialized = true;
}

void mg_close(void)
{
    if (!is_initialized) return;

    rspq_overlay_unregister(mg_overlay_id);
    is_initialized = false;
}

static void check_shader_binary_compatibility(rsp_ucode_t *shader_ucode)
{
    // FIXME: This currently doesn't work because .bss addresses can vary in shaders.
    //        The core problem is that in shaders, uniforms rarely take up exactly the maximum allowed amount of memory,
    //        so the .bss addresses that follow them will shift towards 0.
#if 0
    assertf(memcmp(shader_ucode->code, rsp_magma.code, RSP_MAGMA__MG_OVERLAY - RSP_MAGMA__start) == 0, "Common code of shader %s does not match!", shader_ucode->name);
    assertf(memcmp(shader_ucode->data, rsp_magma.data, RSP_MAGMA__MG_VTX_SHADER_UNIFORMS - RSP_MAGMA__data_start) == 0, "Common data of shader %s does not match!", shader_ucode->name);
#endif
}

static const mg_meta_attribute_t *find_meta_attribute_by_input(const mg_meta_attribute_t *attributes, uint32_t attribute_count, uint32_t input)
{
    for (size_t i = 0; i < attribute_count; i++)
    {
        if (attributes[i].input == input) {
            return &attributes[i];
        }
    }
    
    return NULL;
}

static const mg_vertex_attribute_t *find_vertex_attribute_by_input(const mg_vertex_layout_t *input_parms, uint32_t input)
{
    for (size_t i = 0; i < input_parms->attribute_count; i++)
    {
        if (input_parms->attributes[i].input == input) {
            return &input_parms->attributes[i];
        }
    }

    return NULL;
}

static uint32_t get_vector_load_offset_shift(uint32_t opcode)
{
    switch (opcode) {
    case VLOAD_BYTE:
        return 0;
    case VLOAD_HALF:
        return 1;
    case VLOAD_LONG:
        return 2;
    case VLOAD_DOUBLE:
    case VLOAD_PACK:
    case VLOAD_UPACK:
        return 3;
    case VLOAD_QUAD:
    case VLOAD_REST:
    case VLOAD_HPACK:
    case VLOAD_FPACK:
    case VLOAD_TRANSPOSE:
        return 4;
    default:
        assertf(0, "Invalid vector loader opcode!");
    }
}

static void patch_vertex_attribute_loader(void *shader_code, uint32_t loader_offset, const mg_vertex_attribute_t *vertex_attribute)
{
    uint32_t *loader_ptr = (uint32_t*)((uint8_t*)shader_code + loader_offset);
    uint32_t loader_op = *loader_ptr;

    uint32_t opcode = loader_op >> 26;
    
    switch (opcode) {
    case LB:
    case LH:
    case LW:
    case LBU:
    case LHU:
    case LWU:
        *loader_ptr = (loader_op & 0xFFFF0000) | (vertex_attribute->offset & 0xFFFF);
        break;
    case LWC2:
        uint32_t vl_opcode = (loader_op >> 11) & 0x1F;
        uint32_t offset_shift = get_vector_load_offset_shift(vl_opcode);
        assertf((vertex_attribute->offset & ((1<<offset_shift)-1)) == 0, "Offset of attribute with input number %ld must be aligned to %d bytes!", vertex_attribute->input, 1<<offset_shift);
        uint32_t offset = vertex_attribute->offset >> offset_shift;
        *loader_ptr = (loader_op & 0xFFFFFF80) | (offset & 0x7F);
        break;
    default:
        assertf(0, "Invalid loader opcode!");
    }
}

static void patch_shader_with_vertex_layout(void *shader_code, const mg_meta_header_t *meta_header, const mg_vertex_layout_t *parms)
{
    // Check that all attributes in the configuration are valid
    const mg_meta_attribute_t *attributes = (const mg_meta_attribute_t*)((uint8_t*)meta_header + meta_header->attributes_offset);
    for (size_t i = 0; i < parms->attribute_count; i++)
    {
        const mg_meta_attribute_t *meta_attribute = find_meta_attribute_by_input(attributes, meta_header->attribute_count, parms->attributes[i].input);
        assertf(meta_attribute != NULL, "Vertex attribute with input number %ld could not be found!", parms->attributes[i].input);
    }

    for (size_t i = 0; i < meta_header->attribute_count; i++)
    {
        const mg_vertex_attribute_t *vertex_attribute = find_vertex_attribute_by_input(parms, attributes[i].input);
        if (vertex_attribute != NULL) {
            // If the vertex attribute is enabled, patch all loaders with the correct offset
            uint32_t *loaders = (uint32_t*)((uint8_t*)attributes + attributes[i].loaders_offset);
            for (size_t j = 0; j < attributes[i].loader_count; j++)
            {
                patch_vertex_attribute_loader(shader_code, loaders[j], vertex_attribute);
            }
        } else {
            assertf(attributes[i].is_optional, "The vertex attribute with input number %ld is not optional!", attributes[i].input);
            // Otherwise, if the vertex attribute is disabled, apply all patches
            mg_meta_attribute_patch_t *patches = (mg_meta_attribute_patch_t*)((uint8_t*)attributes + attributes[i].patches_offset);
            for (size_t j = 0; j < attributes[i].patches_count; j++)
            {
                *(uint32_t*)((uint8_t*)shader_code + patches[j].address) = patches[j].replacement;
            }
        }
    }
}

static void extract_pipeline_uniforms(mg_pipeline_t *pipeline, mg_meta_header_t *meta_header)
{
    const mg_meta_uniform_t *uniforms = (const mg_meta_uniform_t*)((uint8_t*)meta_header + meta_header->uniforms_offset);

    pipeline->uniform_count = meta_header->uniform_count;

    if (pipeline->uniform_count > 0) {
        pipeline->uniforms = calloc(pipeline->uniform_count, sizeof(mg_uniform_t));
        
        for (size_t i = 0; i < pipeline->uniform_count; i++)
        {
            pipeline->uniforms[i].binding = uniforms[i].binding;
            pipeline->uniforms[i].offset = uniforms[i].start;
            pipeline->uniforms[i].size = uniforms[i].end - uniforms[i].start;
        }
    }
}

mg_pipeline_t *mg_pipeline_create(const mg_pipeline_parms_t *parms)
{
    check_shader_binary_compatibility(parms->vertex_shader_ucode);

    mg_pipeline_t *pipeline = malloc(sizeof(mg_pipeline_t));

    pipeline->vertex_stride = parms->vertex_layout.stride;
    
    get_overlay_span(parms->vertex_shader_ucode, &pipeline->shader_code, &pipeline->shader_code_size);

    // Copy the shader ucode to a new buffer where it will be patched
    // This is cached memory so copying and patching are faster
    void *code_copy = memalign(16, ROUND_UP(pipeline->shader_code_size, 16));
    memcpy(code_copy, pipeline->shader_code, pipeline->shader_code_size);

    mg_meta_header_t *meta_header = (mg_meta_header_t*)parms->vertex_shader_ucode->meta;

    // Patch shader ucode according to configured vertex layout
    patch_shader_with_vertex_layout(code_copy, meta_header, &parms->vertex_layout);
    data_cache_hit_writeback(code_copy, pipeline->shader_code_size);
    pipeline->shader_code = code_copy;

    // Extract uniform definitions from ucode metadata
    extract_pipeline_uniforms(pipeline, meta_header);

    return pipeline;
}

void mg_pipeline_free(mg_pipeline_t *pipeline)
{
    if (pipeline->uniforms) {
        free(pipeline->uniforms);
    }
    free(pipeline->shader_code);
    free(pipeline);
}

const mg_uniform_t *mg_pipeline_try_get_uniform(mg_pipeline_t *pipeline, uint32_t binding)
{
    for (uint32_t i = 0; i < pipeline->uniform_count; i++)
    {
        if (pipeline->uniforms[i].binding == binding) {
            return &pipeline->uniforms[i];
        }
    }
    
    return NULL;
}

const mg_uniform_t *mg_pipeline_get_uniform(mg_pipeline_t *pipeline, uint32_t binding)
{
    const mg_uniform_t *uniform = mg_pipeline_try_get_uniform(pipeline, binding);
    assertf(uniform != NULL, "Uniform with binding number %ld was not found", binding);
    return uniform;
}

bool mg_pipeline_is_uniform_compatible(mg_pipeline_t *pipeline, const mg_uniform_t *uniform)
{
    const mg_uniform_t *matching_uniform = mg_pipeline_try_get_uniform(pipeline, uniform->binding);
    return matching_uniform != NULL && matching_uniform->offset == uniform->offset && matching_uniform->size == uniform->size;
}

void mg_pipeline_bind(mg_pipeline_t *pipeline)
{
    uint32_t code = PhysicalAddr(pipeline->shader_code);
    uint32_t code_size = pipeline->shader_code_size;
    mg_cmd_write(MG_CMD_SET_SHADER, code, ROUND_UP(code_size, 8) - 1);

    int16_t v0 = pipeline->vertex_stride;
    int16_t v1 = MG_VTX_SIZE;
    int16_t v2 = -pipeline->vertex_stride;
    int16_t v3 = pipeline->vertex_stride;
    mg_cmd_set_word(offsetof(mg_rsp_state_t, vertex_size), (v0 << 16) | v1);
    mg_cmd_set_word(offsetof(mg_rsp_state_t, vertex_size) + sizeof(int16_t)*2, (v2 << 16) | v3);
}

static mg_rsp_viewport_t viewport_to_rsp_state(const mg_viewport_t *viewport)
{
    float half_width = viewport->width / 2;
    float half_height = viewport->height / 2;
    float depth_diff = viewport->maxDepth - viewport->minDepth;
    float half_depth = depth_diff / 2;
    float z_planes_sum = viewport->z_near + viewport->z_far;
    float w_norm_factor = z_planes_sum > 0.0f ? 2.0f / z_planes_sum : 1.0f;
    return (mg_rsp_viewport_t) {
        .scale = {
            half_width * 8,
            half_height * 8,
            half_depth * 0x7FFF * 2,
            (uint16_t)(w_norm_factor * 0xFFFF)
        },
        .offset = {
            (viewport->x + half_width) * 4,
            (viewport->y + half_height) * 4,
            (viewport->minDepth + half_depth) * 0x7FFF,
            0
        }
    };
}


void mg_set_viewport(const mg_viewport_t *viewport)
{
    mg_rsp_viewport_t rsp_viewport = viewport_to_rsp_state(viewport);
    uint32_t value0 = (rsp_viewport.scale[0] << 16) | rsp_viewport.scale[1];
    uint32_t value1 = (rsp_viewport.scale[2] << 16) | rsp_viewport.scale[3];
    uint32_t value2 = (rsp_viewport.offset[0] << 16) | rsp_viewport.offset[1];
    uint32_t value3 = (rsp_viewport.offset[2] << 16) | rsp_viewport.offset[3];
    mg_cmd_set_quad(offsetof(mg_rsp_state_t, viewport), value0, value1, value2, value3);
}

void mg_uniform_load_inline_raw(uint32_t offset, uint32_t size, const void *data)
{
    assertf((offset&7) == 0, "offset must be a multiple of 8");
    assertf((size&3) == 0, "size must be a multiple of 4");
    assertf(size <= MG_MAX_UNIFORM_PAYLOAD_SIZE, "size must not be greater than %d", MG_MAX_UNIFORM_PAYLOAD_SIZE);

    uint32_t aligned_size = ROUND_UP(size, 8);

    uint32_t command_id = MG_CMD_INLINE_UNIFORM_MAX;
    uint32_t command_size = MG_MAX_UNIFORM_PAYLOAD_SIZE;

    if (aligned_size <= 8) {
        command_id = MG_CMD_INLINE_UNIFORM_8;
        command_size = 8;
    } else if (aligned_size <= 16) {
        command_id = MG_CMD_INLINE_UNIFORM_16;
        command_size = 16;
    } else if (aligned_size <= 32) {
        command_id = MG_CMD_INLINE_UNIFORM_32;
        command_size = 32;
    } else if (aligned_size <= 64) {
        command_id = MG_CMD_INLINE_UNIFORM_64;
        command_size = 64;
    } else if (aligned_size <= 128) {
        command_id = MG_CMD_INLINE_UNIFORM_128;
        command_size = 128;
    }

    rspq_write_t w = rspq_write_begin(mg_overlay_id, command_id, (MG_INLINE_UNIFORM_HEADER + command_size)/4);

    // We want to place the payload at an 8-byte aligned address after the end of the command itself
    // w.pointer is already +1 from the actual start of the command
    // the command itself is 2 words, so advance it by one more word
    uint32_t pointer = PhysicalAddr(w.pointer + 1);
    bool aligned = (pointer & 0x7) == 0;

    // If the address right after the command is not aligned, advance by another word
    rspq_write_arg(&w, aligned ? pointer : pointer + 4);
    rspq_write_arg(&w, ((aligned_size-1) << 16) | offset);

    // Write padding for alignment
    if (!aligned) {
        rspq_write_arg(&w, 0);
    }

    const uint32_t *words = data;

    uint32_t word_count = size / sizeof(uint32_t);
    for (uint32_t i = 0; i < word_count; i++)
    {
        rspq_write_arg(&w, words[i]);
    }

    rspq_write_end(&w);
}

#define TRI_LIST_ADVANCE_COUNT ROUND_DOWN(MG_VERTEX_CACHE_COUNT, 3)

void mg_draw(const mg_input_assembly_parms_t *input_assembly_parms, uint32_t vertex_count, uint32_t first_vertex)
{
    uint32_t cache_offset = 0;
    uint32_t next_cache_offset = 0;
    uint32_t advance_count = 0;
    uint32_t batch_size = 0;
    switch (input_assembly_parms->primitive_topology) {
    case MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        advance_count = TRI_LIST_ADVANCE_COUNT;
        batch_size = TRI_LIST_ADVANCE_COUNT;
        break;
    case MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
        advance_count = MG_VERTEX_CACHE_COUNT - 2;
        batch_size = MG_VERTEX_CACHE_COUNT;
        break;
    case MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
        advance_count = MG_VERTEX_CACHE_COUNT - 1;
        batch_size = MG_VERTEX_CACHE_COUNT;
        break;
    }

    for (uint32_t current_vertex = 0; current_vertex < vertex_count; current_vertex += advance_count - cache_offset)
    {
        cache_offset = next_cache_offset;
        uint32_t load_count = MIN(batch_size - cache_offset, vertex_count - current_vertex);
        mg_load_vertices(current_vertex + first_vertex, cache_offset, load_count);

        switch (input_assembly_parms->primitive_topology) {
            case MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
            {
                size_t prim_count = load_count / 3;
                for (size_t i = 0; i < prim_count; i++) mg_draw_triangle(3*i, 3*i+1, 3*i+2);
                break;
            }
            case MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
            {
                size_t prim_count = MAX(0, load_count - 2);
                for (size_t i = 0; i < prim_count; i++) mg_draw_triangle(i, i + 1 + i%2, i + 2 - i%2);
                break;
            }
            case MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
            {
                size_t prim_count = MAX(0, load_count - 2 + cache_offset);
                for (size_t i = 0; i < prim_count; i++) mg_draw_triangle(i+1, i+2, 0);
                next_cache_offset = 1;
                break;
            }
        }
    }
}

#define SPECIAL_INDEX UINT16_MAX

typedef struct vertex_cache_block_s vertex_cache_block;

/** @brief Represents a block of consecutive vertices loaded into the vertex cache at some offset. */
typedef struct vertex_cache_block_s
{
    uint16_t start;             ///< The index (in the vertex buffer) of the first vertex in this block.
    uint16_t count;             ///< The number of vertices in this block.
    vertex_cache_block *next;   ///< Pointer to the next block to form a linked list that is sorted by "start".
} vertex_cache_block;

/** @brief Represents a simulated state of the vertex cache. */
typedef struct 
{
    vertex_cache_block blocks[MG_VERTEX_CACHE_COUNT];   ///< Array of blocks.
    vertex_cache_block *head;                           ///< Linked list of currently loaded blocks.
    vertex_cache_block *unused;                         ///< Linked list of unused blocks.
    uint32_t total_count;                               ///< Total count of vertices in the cache.

} vertex_cache;

static void vertex_cache_clear(vertex_cache *cache)
{
    cache->total_count = 0;
    for (size_t i = 0; i < MG_VERTEX_CACHE_COUNT-1; i++)
    {
        cache->blocks[i].next = &cache->blocks[i+1];
    }
    cache->blocks[MG_VERTEX_CACHE_COUNT-1].next = NULL;
    cache->unused = &cache->blocks[0];
    cache->head = NULL;
}

static vertex_cache_block* vertex_cache_insert_after(vertex_cache *cache, vertex_cache_block *block)
{
    // Make sure there are still unused blocks remaining!
    assertf(cache->unused, "Maximum number of blocks reached! This is a bug within magma.");

    // Remove an entry from the list of unused blocks and use it as the new one
    vertex_cache_block *new_block = cache->unused;
    cache->unused = new_block->next;

    if (block) {
        // If block is set, insert after it
        new_block->next = block->next;
        block->next = new_block;
    } else {
        // Otherwise insert at the start of the list
        new_block->next = cache->head;
        cache->head = new_block;
    }

    return new_block;
}

static void vertex_cache_merge_with_next(vertex_cache *cache, vertex_cache_block *block)
{
    vertex_cache_block *next = block->next;
    if (!next) {
        // Nothing to do if there is no next block
        return;
    }

    // Check if the two blocks are exactly bordering each other.
    // This is suffient since blocks can normally only grow by one at a time.
    uint16_t block_end = block->start + block->count;

    if (block_end == next->start) {
        // Grow this block to encompass the next one
        block->count += next->count;

        // Remove the next block from the list
        block->next = next->next;

        // Return it to the list of unused blocks
        next->next = cache->unused;
        cache->unused = next;
    } else {
        // Check invariant
        assertf(block_end < next->start, "Blocks are overlapping! This is a bug within magma.");
    }
}

static bool vertex_cache_try_insert(vertex_cache *cache, uint16_t index)
{
    if (cache->total_count >= MG_VERTEX_CACHE_COUNT) {
        return false;
    }

    // Find existing block to insert into
    vertex_cache_block *block = cache->head;
    vertex_cache_block *prev = NULL;
    while (block)
    {
        if (block->count > 0) {
            uint16_t block_end = block->start + block->count;
            if (index >= block->start && index < block_end) {
                // Index is already contained in this block
                return true;
            }

            if (index == block_end) {
                // Index is the next one after this block. Grow it by one.
                block->count++;
                cache->total_count++;

                // Try to merge with the next block in case they are now bordering
                vertex_cache_merge_with_next(cache, block);

                return true;
            }

            if (block->start - index == 1) {
                // Index is the previous one before this block. Grow it backwards by one.
                block->count++;
                block->start--;
                cache->total_count++;

                // Try to merge with the previous block in case they are now bordering
                if (prev) vertex_cache_merge_with_next(cache, prev);

                return true;
            }

            if (block->start > index) {
                // Blocks are always sorted. That means if the current block's start
                // is after the index, we already know that none of the following blocks
                // will contain the index. The new block we need to create has to be inserted
                // before the current block.
                break;
            }
        }

        prev = block;
        block = block->next;
    }

    // If no existing block contained the index, insert new block after "prev". 
    // If prev is null, there are no blocks yet and this will create the new block at the start of the list.
    vertex_cache_block *new_block = vertex_cache_insert_after(cache, prev);
    new_block->start = index;
    new_block->count = 1;
    cache->total_count++;
    return true;
}

static bool vertex_cache_find(const vertex_cache *cache, uint16_t index, uint8_t *cache_index)
{
    uint16_t cur_offset = 0;
    vertex_cache_block *block = cache->head;
    while (block)
    {
        uint16_t diff = index - block->start;
        if (diff < block->count) {
            *cache_index = cur_offset + diff;
            return true;
        }
        cur_offset += block->count;
        block = block->next;
    }

    return false;
}

static uint8_t vertex_cache_get(const vertex_cache *cache, uint16_t index)
{
    uint8_t cache_index;
    bool found = vertex_cache_find(cache, index, &cache_index);
    assertf(found, "Index %d not found in vertex batch! This is a bug within magma.", index);
    return cache_index;
}

static void vertex_cache_load(const vertex_cache *cache, int32_t offset, uint32_t cache_offset)
{
    vertex_cache_block *block = cache->head;
    while (block)
    {
        if (block->count > 0) {
            mg_load_vertices(block->start + offset, cache_offset, block->count);
            cache_offset += block->count;
        }

        block = block->next;
    }
}

#ifdef MG_DEBUG_VERTEX_CACHE
static void vertex_cache_dump(const vertex_cache *cache)
{
    debugf("vertex cache dump:\n");
    vertex_cache_block *block = cache->head;
    while (block)
    {
        debugf("%d %d\n", block->start, block->count);
        block = block->next;
    }
}
#endif

static uint32_t prepare_batch(const uint16_t *indices, int32_t vertex_offset, uint32_t max_count, vertex_cache *cache, uint32_t windup, uint32_t advance, bool restart_enabled, uint32_t cache_offset)
{
    vertex_cache_clear(cache);

    uint32_t count = 0;
    uint32_t required = windup + advance;

    while (count + required <= max_count)
    {
        bool restart = false;
        uint32_t need_insertion = 0;
        for (size_t i = 0; i < required; i++)
        {
            uint16_t index = indices[count + i];
            if (restart_enabled && index == SPECIAL_INDEX) {
                required = windup + advance;
                need_insertion = 0;
                count++;
                restart = true;
                break;
            }

            uint8_t dummy;
            need_insertion += vertex_cache_find(cache, index, &dummy) ? 0 : 1;
        }

        if (restart) {
            continue;
        }

        if (cache->total_count + need_insertion + cache_offset > MG_VERTEX_CACHE_COUNT) {
            break;
        }

        for (size_t i = 0; i < required; i++)
        {
            vertex_cache_try_insert(cache, indices[count + i]);
        }

        count += required;
        required = advance;
    }

    assertf(cache->total_count + cache_offset <= MG_VERTEX_CACHE_COUNT, "Vertex batch is too big! This is a bug within magma.");

    #ifdef MG_DEBUG_VERTEX_CACHE
    vertex_cache_dump(cache);
    #endif

    vertex_cache_load(cache, vertex_offset, cache_offset);
    return count;
}

static void draw_triangle_list_batch(const uint16_t *indices, uint32_t current_index, uint32_t batch_count, const vertex_cache *cache)
{
    uint8_t prim_indices[3];
    for (size_t i = 0; i < batch_count; i++)
    {
        uint8_t cache_index = vertex_cache_get(cache, indices[current_index + i]);

        prim_indices[i%3] = cache_index;
        if (i%3 == 2) {
            mg_draw_triangle(prim_indices[0], prim_indices[1], prim_indices[2]);
        }
    }
}

static void draw_triangle_strip_batch(const uint16_t *indices, uint32_t current_index, uint32_t batch_count, const vertex_cache *cache, bool restart_enabled)
{
    size_t prim_counter = 0;
    uint8_t prim_indices[3];
    for (size_t i = 0; i < batch_count; i++)
    {
        uint16_t index = indices[current_index + i];

        if (restart_enabled && index == SPECIAL_INDEX) {
            prim_counter = 0;
            continue;
        }

        uint8_t cache_index = vertex_cache_get(cache, index);

        prim_indices[prim_counter%3] = cache_index;
        if (prim_counter>1) {
            int p = prim_counter-2;
            int p0 = p;
            int p1 = p+(1+p%2);
            int p2 = p+(2-p%2);
            mg_draw_triangle(prim_indices[p0%3], prim_indices[p1%3], prim_indices[p2%3]);
        }

        prim_counter++;
    }
}

static void draw_triangle_fan_batch(const uint16_t *indices, uint32_t current_index, uint32_t batch_count, const vertex_cache *cache, bool restart_enabled, uint32_t cache_offset)
{
    uint8_t prim_indices[3] = {0};
    size_t prim_counter = cache_offset;
    for (size_t i = 0; i < batch_count; i++)
    {
        uint16_t index = indices[current_index + i];

        if (restart_enabled && index == SPECIAL_INDEX) {
            prim_counter = 0;
            continue;
        }

        uint8_t cache_index = vertex_cache_get(cache, index) + cache_offset;

        if (prim_counter == 0) {
            prim_indices[2] = cache_index;
        } else {
            prim_indices[prim_counter%2] = cache_index;
        }

        if (prim_counter>1) {
            int p = prim_counter-2;
            int p0 = p+1;
            int p1 = p+2;
            mg_draw_triangle(prim_indices[p0%2], prim_indices[p1%2], prim_indices[2]);
        }

        prim_counter++;
    }
}

static uint32_t get_advance_count(mg_primitive_topology_t topology)
{
    switch (topology) {
    case MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        return 3;
    case MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
    case MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
        return 1;
    default:
        assertf(0, "Unknown topology");
    }
}

static uint32_t get_windup_count(mg_primitive_topology_t topology)
{
    switch (topology) {
    case MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        return 0;
    case MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
    case MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
        return 2;
    default:
        assertf(0, "Unknown topology");
    }
}

void mg_draw_indexed(const mg_input_assembly_parms_t *input_assembly_parms, const uint16_t *indices, uint32_t index_count, int32_t vertex_offset)
{
    vertex_cache cache;
    uint32_t current_index = 0;

    mg_primitive_topology_t topology = input_assembly_parms ? input_assembly_parms->primitive_topology : MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    bool restart_enabled = input_assembly_parms != NULL && input_assembly_parms->primitive_restart_enabled;

    assertf(!(restart_enabled && topology == MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST), "Primitive restart is not supported for triangle lists!");

    uint32_t windup = get_windup_count(topology);
    uint32_t advance = get_advance_count(topology);

    uint32_t cache_offset = 0;

    while (current_index < index_count - windup + cache_offset)
    {
        uint32_t batch_index_count = prepare_batch(indices + current_index, vertex_offset, index_count - current_index, &cache, windup, advance, restart_enabled, cache_offset);

        switch (topology) {
        case MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
            draw_triangle_list_batch(indices, current_index, batch_index_count, &cache);
            break;
        case MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
            draw_triangle_strip_batch(indices, current_index, batch_index_count, &cache, restart_enabled);
            break;
        case MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
            draw_triangle_fan_batch(indices, current_index, batch_index_count, &cache, restart_enabled, cache_offset);
            cache_offset = 1;
            break;
        }

        current_index += batch_index_count - windup + cache_offset;
    }
}

/* TODO: Instanced draw calls?
    There is probably no real benefit other than having a fancy API.
    On modern hardware, instancing is a technique to reduce the number of draw calls,
    thus reducing the amount of data sent from CPU to GPU.

    On N64, this doesn't really apply
    unless the RSP would drive the input assembly. We could investigate that possibility,
    but it is questionably how much performance this could realistically save, since this workload
    can't really be vectorized and the vertex/index data still needs to be transferred from RDRAM anyway.

    As it stands, the assembly of vertex loading and triangle drawing commands is
    done on the CPU and therefore instancing would not lead to a performance gain.
*/

/* TODO: Indirect draw calls?
    As opposed to instancing, indirect rendering could have some actual benefits.
    For example, one could compute visibility of objects on the RSP and dynamically emit
    draw calls only for visible objects, which would then be run indirectly.

    The implementation might simply use dynamically assembled rspq blocks internally.
    So perhaps this could be a generic feature of rspq instead of being specific to magma.
*/       


extern inline void mg_cmd_set_byte(uint32_t offset, uint8_t value);
extern inline void mg_cmd_set_short(uint32_t offset, uint16_t value);
extern inline void mg_cmd_set_word(uint32_t offset, uint32_t value);
extern inline void mg_cmd_set_quad(uint32_t offset, uint32_t value0, uint32_t value1, uint32_t value2, uint32_t value3);
extern inline uint8_t mg_culling_parms_to_rsp_state(const mg_culling_parms_t *culling);
extern inline void mg_set_culling(const mg_culling_parms_t *culling);
extern inline void mg_set_geometry_flags(mg_geometry_flags_t flags);
extern inline void mg_set_clip_factor(uint32_t factor);
extern inline void mg_uniform_load(const mg_uniform_t *uniform, const void *data);
extern inline void mg_uniform_load_inline(const mg_uniform_t *uniform, const void *data);
extern inline void mg_bind_vertex_buffer(const void *buffer);
