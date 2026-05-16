#ifndef __LIBDRAGON_MAGMA_INTERNAL_H
#define __LIBDRAGON_MAGMA_INTERNAL_H

#include "magma.h"

/** @brief A pipeline instance */
typedef struct mg_pipeline_s 
{
    void *shader_code;          ///< Pointer to the duplicated and patched shader ucode text.
    uint32_t shader_code_size;  ///< Size of the duplicated and patched shader ucode text.
    uint32_t vertex_stride;     ///< Stride of the vertex layout.
    uint32_t uniform_count;     ///< Number of uniforms.
    mg_uniform_t *uniforms;     ///< List of uniforms.
} mg_pipeline_t;

/**
 * @brief Returns a pointer to the magma ucode state in RDRAM.
 */
mg_rsp_state_t *mg_get_rsp_state();

/** 
 * @brief Sets the current vertex stride that is used to load vertices. 
 * 
 * This is an internal method that is normally implicitly called by #mg_pipeline_bind.
 */
void mg_set_vertex_stride(uint32_t vertex_stride);

#endif
