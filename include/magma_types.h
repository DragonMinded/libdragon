/**
 * @file magma_types.h
 * @brief Magma type definitions
 * @ingroup magma
 */

#ifndef __LIBDRAGON_MAGMA_TYPES_H
#define __LIBDRAGON_MAGMA_TYPES_H

#include <stdint.h>
#include <rsp.h>

/** 
 * @brief An instance of a vertex shader that has been configured with a specific vertex layout.
 * 
 * @see #mg_pipeline_create
 * @see #mg_pipeline_bind
 */
typedef struct mg_pipeline_s    mg_pipeline_t;

/**
 * @brief Vertex attribute descriptor.
 * 
 * @see #mg_vertex_layout_t
 */
typedef struct
{
    uint32_t input;     ///< The input number of this attribute.
    uint32_t offset;    ///< The offset in bytes of this attribute relative to the start of a vertex.
} mg_vertex_attribute_t;

/**
 * @brief Configuration of a pipeline's vertex layout.
 * 
 * This configuration specifies how the data from a vertex buffer should be fed into the vertex shader.
 * 
 * A vertex shader defines the set of vertex inputs it supports. A vertex input is defined by:
 *  * Its "input number", which is a unique identifier
 *  * An alignment requirement
 *  * Whether it is optional or not
 * 
 * The vertex layout of a pipeline is defined by its overall stride (the distance between consecutive vertices in the buffer),
 * and a collection of vertex attributes. Each attribute is defined by:
 *  * Its offset relative from the start of a vertex
 *  * An input number, which creates a mapping to some vertex input
 * 
 * To create a valid pipeline, its vertex layout must be compatible with the vertex inputs defined by the vertex shader.
 * A vertex layout is compatible with a shader if, and only if, all of the following requirements are met:
 *  * All of its vertex attributes have an input number that is present in the shader
 *  * There are no duplicate input numbers among the vertex attributes
 *  * For each non-optional vertex input in the shader, there is a vertex attribute with the respective input number.
 *  * All vertex attributes satisfy the alignment requirements of their respective vertex inputs. This is the case
 *    if, and only if, the vertex attribute's offset is a multiple of the required alignment.
 * 
 * The above implies that optional vertex inputs may be omitted from the vertex layout.
 * However, there is no further guarantee about the behavior of the shader in case of omitted inputs.
 * 
 * Also note that, besides alignment, there are no further requirements on the offsets of each vertex attribute.
 * This means they don't need to be packed tightly and may even overlap.
 * How many bytes are actually read per vertex input is defined by the vertex shader.
 * 
 * @see #mg_pipeline_parms_t
 * @see #mg_pipeline_create
 */
typedef struct
{
    uint32_t stride;                    ///< The distance in bytes between two consecutive vertices.
    uint32_t attribute_count;           ///< The number of vertex attribute descriptors.
    mg_vertex_attribute_t *attributes;  ///< Pointer to the array of vertex attribute descriptors.
} mg_vertex_layout_t;

/**
 * @brief Parameters for #mg_pipeline_create.
 */
typedef struct
{
    /** 
     * @brief The ucode from which to create the pipeline.
     * 
     * This ucode must be compatible with being a magma vertex shader. In other words, it must adhere to a certain
     * contract that is defined by magma. The exact details of this are too complex to list here. In general,
     * the ucode should include the file `rsp_magma.inc` at the top and call some special macros to define the shader.
     * See that file for more details.
     */
    rsp_ucode_t *vertex_shader_ucode;
    mg_vertex_layout_t vertex_layout;   ///< Vertex layout configuration.
} mg_pipeline_parms_t;

/**
 * @brief Uniform descriptor.
 * 
 * A uniform is a piece of memory that can contain some input for a vertex shader which does not change per vertex.
 * More specifically, a uniform defines some region of memory that is reserved by a pipeline, which the pipeline's
 * vertex shader can access during its runtime. The combined reserved memory for all uniforms in a pipeline is known as 
 * the pipeline's "uniform memory". The uniform memory can be thought of as some virtual address space which starts 
 * at 0 and goes up to however many bytes a pipeline's uniforms take up in total.
 * 
 * A uniform is defined by:
 *  * Its starting offset in uniform memory
 *  * Its size
 *  * An identifier which is called the binding number.
 * 
 * The binding number must only be unique for each uniform within a pipeline. Otherwise, binding numbers can take on arbitrary
 * values and must not even be consecutive or otherwise correlated with a uniform's location in uniform memory.
 * 
 * Uniforms are defined by the vertex shader that is referenced by a pipeline. All pipelines that reference the same
 * shader will contain equivalent sets of uniforms. Uniforms can be queried from a pipeline using #mg_pipeline_get_uniform.
 * 
 * To provide inputs to a vertex shader via uniforms, they must be loaded first using one of the uniform loading functions,
 * for example #mg_uniform_load or #mg_uniform_load_inline. Those functions will load the entirety of a uniform's memory at once.
 * It is also possible to load values in a more advanced manner using #mg_uniform_load_raw and #mg_uniform_load_inline_raw.
 * 
 * @see #mg_pipeline_get_uniform
 * @see #mg_uniform_load
 * @see #mg_uniform_load_inline
 * @see #mg_uniform_load_raw
 * @see #mg_uniform_load_inline_raw
 */
typedef struct
{
    uint32_t binding;   ///< The uniform's binding number.
    uint32_t offset;    ///< The offset in bytes where this uniform is located, from the start of the pipeline's uniform memory.
    uint32_t size;      ///< The uniform's size in bytes.
} mg_uniform_t;

/**
 * @brief The set of bit flags that can be passed to #mg_set_geometry_flags.
 * 
 * These flags configure which geometry attributes will be passed to the hardware rasterizer
 * when drawing triangles. This is relevant in combination with #rdpq_mode_combiner and/or #rdpq_mode_zbuf.
 * 
 * @see #mg_set_geometry_flags
 */
typedef enum
{
    /**
     * @brief If set, Z values will be transmitted to the rasterizer.
     * 
     * Should be set if depth comparison or Z update were enabled in #rdpq_mode_zbuf.
     */
    MG_GEOMETRY_FLAGS_Z_ENABLED             = 1<<0,

    /**
     * @brief If set, texture coordinates will be transmitted to the rasterizer.
     * 
     * Should be set if #rdpq_mode_combiner was configured with any texture input.
     */
    MG_GEOMETRY_FLAGS_TEX_ENABLED           = 1<<1,

    /**
     * @brief If set, shade values will be transmitted to the rasterizer.
     * 
     * Should be set if #rdpq_mode_combiner was configured with any shade input.
     */
    MG_GEOMETRY_FLAGS_SHADE_ENABLED         = 1<<2,
} mg_geometry_flags_t;

/**
 * @brief Enumeration of possible face culling modes.
 * 
 * @see #mg_culling_parms_t
 * @see #mg_set_culling
 */
typedef enum
{
    MG_CULL_MODE_NONE                       = 0,    ///< No faces will be culled.
    MG_CULL_MODE_BACK                       = 1,    ///< Back faces will be culled.
    MG_CULL_MODE_FRONT                      = 2,    ///< Front faces will be culled.
} mg_cull_mode_t;

/**
 * @brief Enumeration of possible values for front face configuration.
 * 
 * @see #mg_culling_parms_t
 * @see #mg_set_culling
 */
typedef enum
{
    MG_FRONT_FACE_COUNTER_CLOCKWISE         = 0,    ///< Faces with counter clockwise winding direction are defined as front faces.
    MG_FRONT_FACE_CLOCKWISE                 = 1,    ///< Faces with clockwise winding direction are defined as front faces.
} mg_front_face_t;

/**
 * @brief Parameters for #mg_set_culling.
 * 
 * @see #mg_set_culling
 */
typedef struct
{
    mg_cull_mode_t cull_mode;       ///< Specifies which faces should be culled.
    mg_front_face_t front_face;     ///< Defines the winding direction of front faces.
} mg_culling_parms_t;

/**
 * @brief Description of the target area in the framebuffer that will be drawn to.
 * 
 * @see #mg_set_viewport
 */
typedef struct
{
    float x;            ///< X-coordinate of the viewport's upper left corner in pixels.
    float y;            ///< Y-coordinate of the viewport's upper left corner in pixels.
    float width;        ///< Width of the viewport in pixels.
    float height;       ///< Height of the viewport in pixels.
    float minDepth;     ///< Lower end of the viewport's depth range.
    float maxDepth;     ///< Higher end of the viewport's depth range.
    float z_near;       ///< Distance of the near clipping plane from the camera. Used for perspective normalization.
    float z_far;        ///< Distance of the far clipping plane from the camera. Used for perspective normalization.
} mg_viewport_t;

/**
 * @brief Enumeration of possible primitive construction modes.
 * 
 * @see #mg_input_assembly_parms_t
 * @see #mg_draw
 * @see #mg_draw_indexed
 */
typedef enum
{
    /**
     * @brief Separate triangles are constructed for every 3 indices in the list.
     * 
     * Triangles are defined the following equation, where `t{n}` is the nth triangle, and `v{n}` is the nth vertex:
     * @code
     *      t{i} = (v{3i}, v{3i+1}, v{3i+2})
     * @endcode
     * 
     * For a list of `n` indices, `floor(n/3)` triangles will be constructed.
     */
    MG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST     = 0,

    /**
     * @brief Connected triangles are constructed with consecutive triangles sharing an edge.
     * 
     * Triangles are defined the following equation, where `t{n}` is the nth triangle, and `v{n}` is the nth vertex:
     * @code
     *      t{i} = (v{i}, v{i+(1+i%2)}, v{i+(2-i%2)})
     * @endcode
     * 
     * For a list of `n` indices, `max(0,n-2)` triangles will be constructed.
     */
    MG_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP    = 1,

    /**
     * @brief Connected triangles are constructed with all triangles sharing a common vertex.
     * 
     * Triangles are defined the following equation, where `t{n}` is the nth triangle, and `v{n}` is the nth vertex:
     * @code
     *      t{i} = (v{i+1}, v{i+2}, v{0})
     * @endcode
     * 
     * For a list of `n` indices, `max(0,n-2)` triangles will be constructed.
     */
    MG_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN      = 2,
} mg_primitive_topology_t;

/**
 * @brief Describes how primitives are assembled from a list of indices.
 * 
 * @see #mg_draw
 * @see #mg_draw_indexed
 */
typedef struct
{
    mg_primitive_topology_t primitive_topology;     ///< The topology mode of the constructed primitives.
    bool primitive_restart_enabled;                 ///< If true, construction of primitives will restart whenever a special index value (-1) is encountered in the list.
    const void *mtx_indices;                        ///< Pointer to memory where unsigned 8-bit matrix indices are stored, or NULL if matrix indices are disabled.
    uint32_t mtx_indices_stride;                    ///< Number of bytes to advance to get to the next matrix index.
    const void *matrices;                           ///< Pointer to memory where matrix data is stored.
    uint32_t matrices_stride;                       ///< Number of bytes to advance to get to the next matrix.
    mg_uniform_t matrix_uniform;                    ///< Uniform that is used to load matrix data.
} mg_input_assembly_parms_t;

#endif
