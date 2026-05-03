/**
 * @file magma.h
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 * @brief Magma core API
 * @ingroup magma
 */

/**
 * @defgroup magma Magma: Hardware-accelerated 3D graphics API
 * @brief Interface for transforming and drawing 3D geometry with a focus on performance and customizability.
 * @ingroup display
 * 
 * Magma is a library that allows users to perform T&L (https://en.wikipedia.org/wiki/Transform,_clipping,_and_lighting)
 * of 3D geometry in a highly customizable, hardware-accelerated fashion.
 * 
 * The key feature of magma that allows for most of its customizability is the support for vertex shaders, 
 * since they can be written to implement any arbitrary method of transformation and lighting (within the limits of the hardware of course).
 * Vertex shaders run on the RSP ("Reality Signal Processor") and can therefore make use of its special vector instructions
 * to speed up many of the necessary matrix and vector calculations.
 * 
 * The transformed and shaded triangles that result from the vertex shader are then directly sent to the RDP ("Reality Display Processor") 
 * for rasterization  without a roundtrip to the CPU. Other than that, magma does not communicate with the RDP. To configure things like 
 * render modes, textures, etc. the @ref rdpq library should be used.
 * 
 * Magma uses the rspq library (see @ref rspq.h) to interface with the RSP internally. In fact, many of magma's functions are just
 * wrappers around rspq commands and therefore support being recorded into rspq blocks (#rspq_block_t). These functions are marked as
 * such in their documentation with the sentence "Can be recorded into blocks".
 * 
 * ## Overview of magma's architecture
 * 
 * This is a list of frequently used terms and their definitions:
 * 
 * ### Vertex shader
 * This is a special type of ucode (#rsp_ucode_t) that implements the magma vertex shader interface. It contains some metadata to describe itself to magma,
 * and implements the vertex processing loop to perform the T&L. Magma comes with a builtin default shader that is fit for most purposes: @ref mgfx.h
 * 
 * ### Vertex input
 * Every vertex shader defines a set of vertex inputs. A vertex input is some value that a shader expects per vertex, for example its 
 * position, color, texture coordinates etc. Each vertex input is identified with an "input number", which is unique within the shader. 
 * The shader may define certain inputs to be optional.
 * 
 * ### Vertex layout
 * While the set of a shader's vertex inputs can be thought of as an interface, a vertex layout is like an implementation of that interface.
 * It is the physical memory layout of some vertex data that is passed to the vertex shader as input. Shaders may be configured to accept 
 * a range of different vertex layouts as their input, provided they are compatible. See #mg_vertex_layout_t for details.
 * 
 * ### Pipeline
 * A pipeline is an instance of a vertex shader that has been configured with a certain vertex layout. To actually draw any 3D geometry, 
 * a pipeline must first be created and then "bound". After a pipeline has been bound, all subsequent drawing commands will use its vertex shader.
 * A program can create as many pipelines as desired and switch between them at will by binding them. This is useful for both supporting
 * multiple vertex layouts and achieving different visual effects. See #mg_pipeline_t.
 * 
 * ### Uniform
 * Besides vertex inputs, a vertex shader also defines a set of uniforms. Uniforms are inputs to the shader that don't change per vertex.
 * Common examples are matrices and lights. Like vertex inputs, they have a unique identifier each, called the "binding number". 
 * Uniforms can be queried from a pipeline using the binding number. To make the uniform's values accessible to the vertex shader, 
 * they need to reside in the RSP's DMEM. That means to assign a value to a uniform, it must be loaded first. See #mg_uniform_t for details.
 * 
 * ### Vertex buffer
 * A vertex buffer is the source of vertex data that is to be transformed by a vertex shader. It is just a regular region of memory in RDRAM
 * and no special functions are required to create it. A vertex buffer must be "bound" to make its contents available to a vertex shader.
 * See #mg_bind_vertex_buffer for details.
 * 
 * ### Vertex cache
 * This is an internal memory buffer that the results of the vertex shader are stored into. Drawing geometry with magma actually happens in 
 * two distinct steps: First, vertices are "loaded" into the vertex cache, running them through the vertex shader in the process.
 * Triangles can then be drawn by referencing the tranformed vertices inside the cache. This way, vertices don't need to be transformed 
 * redundantly in case they are shared by multiple triangles. See #mg_load_vertices and #mg_draw_triangle for details.
 * 
 * ### Drawing command
 * These are functions that will actually draw geometry to the framebuffer. The most simple example is #mg_draw_triangle, which will draw
 * a single triangle. There are also more advanced drawing commands which can draw entire meshes at once: #mg_draw and #mg_draw_indexed.
 * Internally they will just call #mg_load_vertices and #mg_draw_triangle repeatedly until the entire mesh has been drawn.
 * Translating a list of indices to a sequence of vertex loads and triangle draws is expensive though, so it's best practice to record
 * them into rspq blocks (#rspq_block_t) if the indices don't change.
 * 
 * ### Fixed function states
 * There are some states that are built into magma. They can be set at any time to configure how geometry is being drawn.
 * Some of them are related to drawing triangles only, others are accessible to every vertex shader automatically. 
 * This is the list of functions that set these states. See their respective documentation for more information:
 *  * #mg_set_viewport
 *  * #mg_set_geometry_flags
 *  * #mg_set_culling
 *  * #mg_set_clip_factor
 * 
 * ## How to draw geometry: Quick guide
 * At initialization:
 *  1. Initialize magma using #mg_init.
 *  2. Create a pipeline from a vertex shader (for example mgfx) and a vertex layout that corresponds to your vertex data using #mg_pipeline_create.
 *  3. Query uniforms from the pipeline with #mg_pipeline_get_uniform.
 * 
 * During the rendering loop:
 *  1. Bind the pipeline using #mg_pipeline_bind.
 *  2. Load uniforms using e.g. #mg_uniform_load.
 *  3. Set fixed function states:
 *      1. The viewport, which is the area that geometry is drawn to (#mg_set_viewport).
 *      2. Geometry flags, which specify what type of triangles should be sent to the RDP (#mg_set_geometry_flags).
 *      3. The face culling mode (#mg_set_culling).
 *  4. Bind a pointer to your vertex data as the vertex buffer with #mg_bind_vertex_buffer.
 *  5. Draw the geometry by using one of the drawing commands, or by calling a rspq block with recorded drawing commands.
 * 
 * The order of steps in the rendering loop is not fixed, with some exceptions. Step 5 should obviously always happen last.
 * Step 2 should usually happen after step 1, though there are exceptions (see #mg_pipeline_bind).
 * 
 * If there are different objects in your scene that share some properties, not all of the above steps need to be repeated for each object.
 * For example: If two objects use the same "material" (same vertex shader, same textures etc.) but only differ in their position, it is sufficient
 * to, after the first object has been drawn, only load the uniform that corresponds to "position" before drawing the second object. 
 * Or you could have two objects that share their geometry, but use a different vertex shader. In this case you would have created two pipelines
 * at initialization. After drawing the first object, you would bind the second pipeline, load its uniforms, and then draw the second object
 * without re-binding the vertex buffer. 
 * 
 * To put the above in more formal terms: Most functions that set some state work orthogonally to each other. The only exception is uniform loading
 * with respect to pipeline binding, since uniforms may be invalidated after binding a pipeline. With respect to other uniforms and other states, 
 * uniform loading can still be viewed as orthogonal.
 * 
 * The "mghello" example demonstrates the above steps in a minimal fashion. A more complicated example can be found in "mgdemo".
 */

#ifndef __LIBDRAGON_MAGMA_H
#define __LIBDRAGON_MAGMA_H

#include <stdbool.h>
#include <stdint.h>
#include "graphics.h"
#include "display.h"
#include "rsp.h"
#include "rspq.h"
#include "magma_types.h"
#include "debug.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the magma library.
 * 
 * This function must be called before any other functions of this library are used.
 * It is safe to call this function multiple times. If the library is already
 * initialized, this function has no effect.
 */
void mg_init(void);

/**
 * @brief Shut down the magma library.
 * 
 * It is safe to call this function multiple times. If the library is not
 * initialized, this function has no effect.
 */
void mg_close(void);

/**
 * @brief Creates a new pipeline from a vertex shader and vertex layout.
 * 
 * A copy of the ucode will be created internally, which is then patched automatically
 * to work with the specified vertex layout (provided it is compatible).
 * 
 * See #mg_pipeline_parms_t and #mg_vertex_layout_t for more details.
 * Must be freed with #mg_pipeline_free.
 * 
 * @param[in]   parms   Pointer to a struct containing parameters for creating a pipeline.
 * @return              The newly created pipeline.
 * 
 * @see #mg_pipeline_parms_t
 * @see #mg_vertex_layout_t
 * @see #mg_pipeline_free
 */
mg_pipeline_t *mg_pipeline_create(const mg_pipeline_parms_t *parms);

/**
 * @brief Destructs and frees a pipeline.
 * 
 * Must not be called while any drawing operations using this pipeline are still ongoing.
 * 
 * @param[in]   pipeline    The pipeline to be freed.
 */
void mg_pipeline_free(mg_pipeline_t *pipeline);

/**
 * @brief Returns a struct describing the uniform with the given binding number.
 * 
 * The struct can be used directly to load uniform values using #mg_uniform_load or #mg_uniform_load_inline
 * This function crashes if no uniform with the given binding number exists in the pipeline.
 * Note that binding numbers are not required to be contiguous.
 * 
 * @param[in]   pipeline    The pipeline to query the uniform from.
 * @param[in]   binding     The binding number of the uniform that should be queried.
 * @return                  The uniform in the pipeline with the specified binding number.
 * 
 * @see #mg_uniform_load
 * @see #mg_uniform_load_inline
 */
const mg_uniform_t *mg_pipeline_get_uniform(mg_pipeline_t *pipeline, uint32_t binding);

/**
 * @brief Checks if a uniform is compatible with the pipeline.
 * 
 * If the uniform has been queried from the pipeline using #mg_pipeline_get_uniform,
 * it is by definition always compatible.
 * 
 * If the uniform has been queried from a different pipeline that was created with the
 * same vertex shader, it is guaranteed to be compatible.
 * 
 * Uniforms from other pipelines that do not share vertex shaders may still be compatible
 * if this pipeline contains a uniform with the same binding number which is located in
 * the same region of memory.
 * 
 * @param[in]   pipeline    The pipeline to be checked for compatibility with the uniform.
 * @param[in]   uniform     The uniform to be checked for compatibility with the pipeline.
 * @return                  True if the uniform is compatible with the pipeline, otherwise false.
 * 
 * @see #mg_pipeline_get_uniform
 */
bool mg_pipeline_is_uniform_compatible(mg_pipeline_t *pipeline, const mg_uniform_t *uniform);

/** 
 * @brief Bind the pipeline for subsequent use.
 * 
 * All following drawing commands will use the newly bound pipeline for transforming vertices.
 * This function must be called at least once before any drawing commands are issued.
 * 
 * Binding a pipeline will invalidate any previously loaded uniforms that are not compatible with the pipeline.
 * This compatibility can be checked using #mg_pipeline_is_uniform_compatible.
 * Inversely this means that compatible uniforms will not be invalidated and their values will be preserved
 * after binding the new pipeline.
 * 
 * Can be recorded into blocks.
 * 
 * @param[in]   pipeline    The pipeline to be bound.
 * 
 * @see #mg_pipeline_is_uniform_compatible
 */
void mg_pipeline_bind(mg_pipeline_t *pipeline);

/**
 * @brief Load data from the given pointer into a uniform.
 * 
 * The data will not be read immediately. Instead, this operation is performed asynchronously
 * in the background. This means that the source data must remain valid until the loading has actually 
 * been performed. This will generally be the case after the current frame has been fully drawn. 
 * If you need to load different data to the same uniform every frame, it is advised to keep 
 * multiple source buffers and alternate between them. A good default for the number of buffers is the
 * number of frame buffers that was passed to #display_init.
 * 
 * The actual loading is performed by a DMA operation. That means you need to ensure that
 * the source data is either in uncached memory, or has been flushed back to RDRAM before calling this function.
 * 
 * This function only has defined behavior if the given uniform descriptor has been queried
 * from the pipeline that is currently bound (see #mg_pipeline_bind), or is compatible with
 * the currently bound pipeline. The latter can be checked using #mg_pipeline_is_uniform_compatible.
 * 
 * Can be recorded into blocks. Note that only the pointer value itself will be recorded.
 * Therefore it is still possible to modify the data afterwards and have it correctly loaded.
 * 
 * @param[in]   uniform     The uniform that the data should be loaded into.
 * @param[in]   data        Pointer to the data to be loaded.
 *                          The number of bytes that will be read is equal to the size of the uniform.
 * 
 * @see #mg_pipeline_bind
 * @see #mg_pipeline_is_uniform_compatible
 */
inline void mg_uniform_load(const mg_uniform_t *uniform, const void *data);

/** 
 * @brief Load data from the given pointer into uniform memory.
 * 
 * This function works like #mg_uniform_load but takes the offset and size of the memory region to be loaded into directly.
 * It should be used with care, since it is possible to corrupt the system's state permanently 
 * and cause crashes by specifying an invalid region. The region must not exceed the size of the
 * pipeline's uniform memory, which is defined as the offset plus size of the pipeline's "last" uniform.
 * Note that the last uniform is that with the largest offset, and not necessarily that with the largest binding number.
 * 
 * Can be recorded into blocks.
 * 
 * @param[in]   offset      The offset in bytes of the region in uniform memory that data should be loaded to.
 * @param[in]   size        The size in bytes of the region in uniform memory that data should be loaded to.
 * @param[in]   data        Pointer to the data to be loaded.
 * 
 * @see #mg_uniform_load
 */
void mg_uniform_load_raw(uint32_t offset, uint32_t size, const void *data);

/**
 * @brief Load inline data into a uniform.
 * 
 * This works just like #mg_uniform_load, except that the data is read immediately
 * and embedded into the internal stream of commands. This means there is no need
 * to keep the data around until the asynchronous loading operation is complete.
 * However, this function comes with more performance overhead compared to #mg_uniform_load.
 * 
 * @param[in]   uniform     The uniform that the data should be loaded into.
 * @param[in]   data        Pointer to the data to be embedded and then loaded.
 *                          The number of bytes that will be read is equal to the size of the uniform.
 * 
 * @see #mg_uniform_load
 */
inline void mg_uniform_load_inline(const mg_uniform_t *uniform, const void *data);

/**
 * @brief Load inline data into uniform memory.
 * 
 * This works just like #mg_uniform_load_raw, except that the data is read immediately
 * and embedded into the internal stream of commands. This means there is no need
 * to keep the data around until the asynchronous loading operation is complete.
 * However, this function comes with more performance overhead compared to #mg_uniform_load_raw.
 * 
 * Can be recorded into blocks.
 * 
 * @param[in]   offset      The offset in bytes of the region in uniform memory that data should be loaded to.
 * @param[in]   size        The size in bytes of the region in uniform memory that data should be loaded to.
 * @param[in]   data        Pointer to the data to be embedded and then loaded.
 * 
 * @see #mg_uniform_load_raw
 */
void mg_uniform_load_inline_raw(uint32_t offset, uint32_t size, const void *data);

/** 
 * @brief Set the culling mode for 3D geometry.
 * 
 * Use this function to specify whether to cull back faces, front faces or turn off culling.
 * It is also possible to configure the winding direction of triangles.
 * See #mg_culling_parms_t for details.
 * 
 * Can be recorded into blocks.
 * 
 * @param[in]   culling     Pointer to a struct containing the culling configuration.
 * 
 * @see #mg_culling_parms_t
 */ 
inline void mg_set_culling(const mg_culling_parms_t *culling);

/** 
 * @brief Set the geometry flags for 3D geometry.
 * 
 * The geometry flags specify which attributes should be transmitted to the RDP, the hardware rasterizer.
 * See #mg_geometry_flags_t for details.
 * 
 * Can be recorded into blocks.
 * 
 * @param[in]   flags       Geometry flags.
 * 
 * @see #mg_geometry_flags_t
 */
inline void mg_set_geometry_flags(mg_geometry_flags_t flags);

/** 
 * @brief Set the viewport, which is the region in screen space that geometry will be drawn to.
 * 
 * Can be recorded into blocks.
 * 
 * @param[in]   viewport    Pointer to a struct containing the viewport configuration.
 * 
 * @see #mg_viewport_t
 * @see #mg_set_viewport_fullscreen
 */
void mg_set_viewport(const mg_viewport_t *viewport);

/** 
 * @brief Set the viewport to fullscreen, using the provided display resolution.
 * 
 * Can be recorded into blocks.
 * 
 * @param[in]   res         The display resolution.
 * 
 * @see #mg_viewport_t
 * @see #mg_set_viewport
 */
inline void mg_set_viewport_fullscreen(resolution_t res);

/** 
 * @brief Set the clipping guard factor.
 * 
 * This factor specifies the size of the clipping guard band in multiples of the viewport size.
 * The clipping guard band is a screen space region centered around the actual viewport.
 * Triangles which stick outside the viewport but are still within the guard band will not be clipped
 * as a performance improvement, since clipping is very costly.
 * 
 * A factor of 1 will effectively disable the clipping guard, since it will be exactly as big as the viewport itself.
 * 
 * The default value is #MG_DEFAULT_GUARD_BAND.
 * 
 * Can be recorded into blocks.
 * 
 * @param[in]   factor      The clipping guard size factor.
 */
inline void mg_set_clip_factor(uint32_t factor);

/** 
 * @brief Bind the vertex buffer to be used by subsequent vertex loading commands.
 * 
 * All vertex load commands (issued using #mg_load_vertices) issued after this command will
 * use the specified buffer as their source, until this function is called again with
 * a different buffer.
 * 
 * Vertex loading commands take as parameter an offset into the vertex buffer as opposed to a direct pointer. 
 * When they are executed, they will load vertices at that offset from the pointer that was given to this function. 
 * Note that it is therefore possible to swap out the vertex buffer without needing to adjust vertex loading commands, 
 * for example if they were recorded into a block.
 * 
 * Care must be taken that the source data remains valid until all vertex loading commands that use it have been completed.
 * 
 * Vertex loading commands use hardware DMAs internally, which means the vertex data must
 * either be located in uncached memory or flushed back to RDRAM before calling this function.
 * 
 * Can be recorded into blocks.
 * 
 * @param[in]   buffer      Pointer to the vertex buffer to be used by subsequent vertex loading commands.
 * 
 * @see #mg_load_vertices
 */
inline void mg_bind_vertex_buffer(const void *buffer);

/** 
 * @brief Begin a batch of drawing commands.
 * 
 * This function must be called before a batch of drawing commands that use the same render modes.
 * Use #mg_draw_end to end this batch.
 * 
 * Render modes are configured using the @ref rdpq library. Magma's fixed function settings such as #mg_set_geometry_flags are
 * not required to be separated from drawing batches. There are some exceptions in rdpq as well, such as #rdpq_set_prim_color.
 * 
 * To ensure proper synchronization of render modes, these rules should be followed:
 *  * Functions that change render modes must not be called 
 *    between a pair of calls to #mg_draw_begin and #mg_draw_end in that order.
 *  * Functions that issue drawing commands must only be called 
 *    between a pair of calls to #mg_draw_begin and #mg_draw_end in that order.
 * 
 * Can be recorded into blocks.
 * 
 * @see #mg_draw_end
 */
void mg_draw_begin(void);

/** 
 * @brief End a batch of drawing commands. 
 * 
 * This function must be called after a batch of drawing commands that use the same render modes.
 * See #mg_draw_begin for more details.
 * 
 * Can be recorded into blocks.
 * 
 * @see #mg_draw_begin
 */
void mg_draw_end(void);

/** 
 * @brief Load vertices from the vertex buffer, run the current pipeline's vertex shader on them, and save the result to the vertex cache.
 * 
 * The source vertex data will be read starting from the specified index into the vertex buffer.
 * The transformed vertices will be stored at the specified index into the vertex cache.
 * 
 * After the transformed vertices have been stored into the cache, they can be used to draw triangles
 * by issuing drawing commands (see #mg_draw_triangle).
 * 
 * The cache index plus the vertex count must not exceed the maximum number of vertices that can
 * be held by the vertex cache, which is #MG_VERTEX_CACHE_COUNT.
 * 
 * The vertex loading is not carried out immediately after calling this function. Instead, it runs asynchronously 
 * in the background. This means that the vertex source data must remain valid until it has actually been read. This will 
 * generally be the case after the current frame has been fully drawn. If you need to change the vertex data every frame
 * (for example dynamically generated geometry or "morphing" based animation), it is advised to keep multiple 
 * source buffers and alternate between them. A good default for the number of buffers is the number of 
 * frame buffers that was passed to #display_init. If you only need to apply some linear transformation to the
 * entire geometry, you should use matrices instead, since they can be stored in uniforms.
 * 
 * Can be recorded into blocks.
 * 
 * @param[in]   buffer_index    The offset into the vertex buffer from which to start loading.
 *                              The actual offset in bytes is this number multiplied by the vertex stride of the currently bound pipeline.
 * @param[in]   cache_index     The offset into the vertex cache where to store the transformed vertices, 
 *                              in multiples of the internal vertex size.
 * @param[in]   count           The number of vertices to load. The total number of bytes read from the 
 *                              vertex buffer is this number multiplied by the vertex stride of the currently bound pipeline.
 * 
 * @see #mg_draw_triangle
 * @see #MG_VERTEX_CACHE_COUNT
 */
void mg_load_vertices(uint32_t buffer_index, uint8_t cache_index, uint32_t count);

/** 
 * @brief Draw a triangle with vertices that have previously been stored in the vertex cache.
 * 
 * The values of all indices must be less than the maximum number of vertices that can be held by 
 * the vertex cache, which is #MG_VERTEX_CACHE_COUNT.
 * 
 * Use #mg_load_vertices to populate the vertex cache.
 * 
 * This is a drawing command and must be placed between a pair of calls to #mg_draw_begin and #mg_draw_end.
 * 
 * Can be recorded into blocks.
 * 
 * @param[in]   index0      The index into the vertex cache at which the first vertex is stored.
 * @param[in]   index1      The index into the vertex cache at which the second vertex is stored.
 * @param[in]   index2      The index into the vertex cache at which the third vertex is stored.
 * 
 * @see #mg_load_vertices
 * @see #MG_VERTEX_CACHE_COUNT
 * @see #mg_draw_begin
 * @see #mg_draw_end
 */
void mg_draw_triangle(uint8_t index0, uint8_t index1, uint8_t index2);

/** 
 * @brief Draw multiple triangles from consecutive vertices in the vertex buffer.
 * 
 * The exact algorithm that is used to construct these triangles is specified by #mg_input_assembly_parms_t.
 * This function will feed a linear sequence of indices to that algorithm, which starts at the specified first
 * vertex and has the specified length. In effect this means that triangles are constructed from vertices in
 * the same order in which they appear in the vertex buffer.
 * 
 * The algorithm will translate the list of indices to a sequence of multiple vertex loading and
 * triangle drawing commands. This translation is optimized for efficient loading, as opposed to run time
 * of the algorithm itself. Therefore it is advised to record the generated commands into a block whenever possible,
 * to avoid the translation to be reduntantly computed on every frame, even though the result will always be the same.
 * 
 * This is a drawing command and must be placed between a pair of calls to #mg_draw_begin and #mg_draw_end.
 * 
 * Can be recorded into blocks.
 * 
 * @param[in]   input_assembly_parms    Pointer to a struct containing parameters for input assembly.
 * @param[in]   vertex_count            The number of vertices to draw triangles with.
 * @param[in]   first_vertex            The index into the vertex buffer to start loading vertices from.
 * 
 * @see #mg_input_assembly_parms_t
 * @see #mg_draw_begin
 * @see #mg_draw_end
 */
void mg_draw(const mg_input_assembly_parms_t *input_assembly_parms, uint32_t vertex_count, uint32_t first_vertex);

/** 
 * @brief Draw multiple triangles from a list of indices that specify offsets into the vertex buffer.
 * 
 * The exact algorithm that is used to construct these triangles is specified by #mg_input_assembly_parms_t.
 * This function will read the specified number of indices from the list and feed them to the algorithm 
 * in the order in which they appear in the list. Optionally, a constant offset can be applied to each index.
 * 
 * The algorithm will translate the list of indices to a sequence of multiple vertex loading and
 * triangle drawing commands. This translation is optimized for efficient loading, as opposed to run time
 * of the algorithm itself. Therefore it is advised to record the generated commands into a block whenever possible,
 * to avoid the translation to be reduntantly computed on every frame, even though the result will always be the same.
 * 
 * This is a drawing command and must be placed between a pair of calls to #mg_draw_begin and #mg_draw_end.
 * 
 * Can be recorded into blocks.
 * 
 * @param[in]   input_assembly_parms    Pointer to a struct containing parameters for input assembly.
 * @param[in]   indices                 Pointer to the list of indices.
 * @param[in]   index_count             The number of indices that should be read.
 * @param[in]   vertex_offset           A constant offset that will be added to all index values.
 * 
 * @see #mg_input_assembly_parms_t
 * @see #mg_draw_begin
 * @see #mg_draw_end
 */
void mg_draw_indexed(const mg_input_assembly_parms_t *input_assembly_parms, const uint16_t *indices, uint32_t index_count, int32_t vertex_offset);

/// @cond

/* Inline function definitions and prerequisites */

#define mg_cmd_write(cmd_id, ...)   rspq_write(mg_overlay_id, cmd_id, ##__VA_ARGS__)
#define mg_rdpq_write(cmd_id, ...)  rdpq_write(-1, mg_overlay_id, cmd_id, ##__VA_ARGS__)

extern uint32_t mg_overlay_id;

enum
{
    MG_CMD_SET_BYTE             = 0x0,
    MG_CMD_SET_SHORT            = 0x1,
    MG_CMD_SET_WORD             = 0x2,
    MG_CMD_SET_QUAD             = 0x3,
    MG_CMD_SET_SHADER           = 0x4,
    MG_CMD_LOAD_VERTICES        = 0x5,
    MG_CMD_DRAW_INDICES         = 0x6,
    MG_CMD_DRAW_END             = 0x7,
    MG_CMD_LOAD_UNIFORM         = 0x8,
    MG_CMD_INLINE_UNIFORM_8     = 0x9,
    MG_CMD_INLINE_UNIFORM_16    = 0xA,
    MG_CMD_INLINE_UNIFORM_32    = 0xB,
    MG_CMD_INLINE_UNIFORM_64    = 0xC,
    MG_CMD_INLINE_UNIFORM_128   = 0xD,
    MG_CMD_INLINE_UNIFORM_MAX   = 0xE,
};

typedef struct
{
    uint16_t scale[4];
    uint16_t offset[4];
} mg_rsp_viewport_t;

typedef struct
{
    mg_rsp_viewport_t viewport;
    uint16_t clip_factors[4];
    int16_t vertex_size[4];
    uint32_t shader_code;
    uint32_t shader_code_size;
    uint32_t vertex_buffer;
    uint16_t tri_cmd;
    uint8_t cull_mode;
    uint8_t output_offset;
} __attribute__((packed)) mg_rsp_state_t;

inline void mg_cmd_set_byte(uint32_t offset, uint8_t value)
{
    mg_cmd_write(MG_CMD_SET_BYTE, offset, value);
}

inline void mg_cmd_set_short(uint32_t offset, uint16_t value)
{
    mg_cmd_write(MG_CMD_SET_SHORT, offset, value);
}

inline void mg_cmd_set_word(uint32_t offset, uint32_t value)
{
    mg_cmd_write(MG_CMD_SET_WORD, offset, value);
}

inline void mg_cmd_set_quad(uint32_t offset, uint32_t value0, uint32_t value1, uint32_t value2, uint32_t value3)
{
    mg_cmd_write(MG_CMD_SET_QUAD, offset, value0, value1, value2, value3);
}

inline uint8_t mg_culling_parms_to_rsp_state(const mg_culling_parms_t *culling)
{
    uint8_t cull_mode;
    uint8_t is_front_cw;

    switch (culling->cull_mode) {
    case MG_CULL_MODE_NONE:
        cull_mode = 2;
        break;
    case MG_CULL_MODE_BACK:
        cull_mode = 1;
        break;
    case MG_CULL_MODE_FRONT:
        cull_mode = 0;
        break;
    default:
        assertf(0, "%d is not a valid cull mode!", culling->cull_mode);
    }

    switch (culling->front_face) {
    case MG_FRONT_FACE_COUNTER_CLOCKWISE:
        is_front_cw = 0;
        break;
    case MG_FRONT_FACE_CLOCKWISE:
        is_front_cw = 1;
        break;
    default:
        assertf(0, "%d is not a valid front face winding direction!", culling->front_face);
    }

    // If the front face is clockwise, flip the cull mode. 
    // If the cull mode is NONE anyway, this has no effect because the final value is still > 1
    return cull_mode ^ is_front_cw;
}

inline void mg_set_culling(const mg_culling_parms_t *culling)
{
    mg_cmd_set_byte(offsetof(mg_rsp_state_t, cull_mode), mg_culling_parms_to_rsp_state(culling));
}

inline void mg_set_geometry_flags(mg_geometry_flags_t flags)
{
    uint16_t tricmd = 0x8 | (flags&0x7);
    mg_cmd_set_short(offsetof(mg_rsp_state_t, tri_cmd), tricmd << 8);
}

inline void mg_set_viewport_fullscreen(resolution_t res)
{
    mg_viewport_t viewport = {
        .x = 0,
        .y = 0,
        .width = res.width,
        .height = res.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    mg_set_viewport(&viewport);
}

inline void mg_set_clip_factor(uint32_t factor)
{
    mg_cmd_set_word(offsetof(mg_rsp_state_t, clip_factors) + sizeof(uint16_t)*2, (factor<<16) | factor);
}

inline void mg_uniform_load(const mg_uniform_t *uniform, const void *data)
{
    mg_uniform_load_raw(uniform->offset, uniform->size, data);
}

inline void mg_uniform_load_inline(const mg_uniform_t *uniform, const void *data)
{
    mg_uniform_load_inline_raw(uniform->offset, uniform->size, data);
}

inline void mg_bind_vertex_buffer(const void *buffer)
{
    mg_cmd_set_word(offsetof(mg_rsp_state_t, vertex_buffer), PhysicalAddr(buffer));
}
/// @endcond

#ifdef __cplusplus
}
#endif

#endif
