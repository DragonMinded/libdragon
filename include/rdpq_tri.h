/**
 * @file rdpq.h
 * @brief RDP Command queue
 * @ingroup rdpq
 * 
 */

#ifndef LIBDRAGON_RDPQ_TRI_H
#define LIBDRAGON_RDPQ_TRI_H

#include "rdpq.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * @brief Format descriptor of a triangle
 * 
 * This structure holds the parameters required to draw triangles.
 * It contains both a description of the vertex format, and some
 * configuration parameters for the triangle rasterizer.
 * 
 * This library provides a few predefined formats (such as #TRIFMT_FILL,
 * #TRIFMT_TEX, etc.) but you are free to define your own format.
 * 
 * There is no overhead in using a custom format or even switching
 * format from a triangle to another (besides the required mode changes),
 * so feel free to define as many formats are required for your application.
 * 
 * Refer to #rdpq_triangle for a description of the different vertex
 * components.
 */
typedef struct rdpq_trifmt_s {
    /** 
     * @brief Index of the position component within the vertex arrays.
     * 
     * For instance, if `pos_offset == 4`, `v1[4]` and `v1[5]` must be the X and Y
     * coordinates of the first vertex.
     */
    int pos_offset;

    /**
     * @brief Index of the shade component within the vertex arrays.
     * 
     * For instance, if `shade_offset == 4`, `v1[4]`, `v1[5]`, `v1[6]`, `v1[7]` must be
     * the R, G, B, A values associated to the first vertex. If shade_offset is less
     * than 0, no shade component will be used to draw the triangle.
     */
    int shade_offset;

    /**
     * @brief If true, draw the triangle with flat shading (instead of gouraud shading).
     * 
     * This parameter is ignored if the shade component does not exist (`shade_offset < 0`).
     * Normally, gouraud shading is used to draw triangles, which means that the shading
     * of each vertex is interpolated across the triangle. If flat shading is enabled, the
     * shading of the first vertex is used for the whole triangle.
     */
    bool shade_flat;

    /**
     * @brief Index of the texture component within the vertex arrays.
     * 
     * For instance, if `tex_offset == 4`, `v1[4]`, `v1[5]`, `v1[6]` must be the S, T, W
     * values associated to the first vertex. If tex_offset is less than 0, no texture
     * component will be used to draw the triangle.
     */
    int tex_offset;

    /**
     * @brief RDP tile descriptor that describes the texture (0-7).
     * 
     * This parameter is ignored if the texture component does not exist (`tex_offset < 0`).
     * In case of multi-texturing, `tile + 1` will be used for the second texture.
     * Notice that the tile descriptor must be configured before drawing the triangle.
     */
    rdpq_tile_t tex_tile;

    /**
     * @brief Number of mipmaps to use for the texture.
     * 
     * This parameter is ignored if the texture component does not exist (`tex_offset < 0`),
     * or if mipmapping has not been configured.
     * 
     * Notice that when using the mode API (#rdpq_mode_mipmap), the number of mipmaps
     * is specified there, so this parameter should be left to zero.
     */
    int tex_mipmaps;

    /**
     * @brief Index of the depth component within the vertex array.
     * 
     * For instance, if `z_offset == 4`, `v1[4]` must be the Z coordinate of the first
     * vertex. If z_offset is less than 0, no depth component will be used to
     * draw the triangle.
     */
    int z_offset;
} rdpq_trifmt_t;

/** 
 * @brief Format descriptor for a solid-filled triangle.
 * 
 * Vertex array format: `(float){X, Y}` (2 floats)
 * 
 * Given that only position is provided, the triangle is drawn with a solid color,
 * which is the output of the color combiner. See #rdpq_mode_combiner for more
 * information. 
 * 
 * A common choice for a combiner formula is #RDPQ_COMBINER_FLAT, that will
 * simply output whatever color is configured via #rdpq_set_prim_color.
 */
extern const rdpq_trifmt_t TRIFMT_FILL;

/** 
 * @brief Format descriptor for a shaded triangle.
 * 
 * Vertex array format: `(float){X, Y, R, G, B, A}` (6 floats)
 * 
 * The suggested standard color combiner for this format is #RDPQ_COMBINER_SHADE.
 */
extern const rdpq_trifmt_t TRIFMT_SHADE;

/** 
 * @brief Format descriptor for a textured triangle.
 * 
 * Vertex array format: `(float){X, Y, S, T, INV_W}` (5 floats)
 * 
 * The suggested standard color combiner for this format is #RDPQ_COMBINER_TEX.
 */
extern const rdpq_trifmt_t TRIFMT_TEX;

/** 
 * @brief Format descriptor for a shaded, textured triangle.
 * 
 * Vertex array format: `(float){X, Y, R, G, B, A, S, T, INV_W}` (9 floats)
 * 
 * The suggested standard color combiner for this format is #RDPQ_COMBINER_TEX_SHADE.
 */
extern const rdpq_trifmt_t TRIFMT_SHADE_TEX;

/** 
 * @brief Format descriptor for a solid-filled, z-buffered triangle.
 * 
 * Vertex array format: `(float){X, Y, Z}` (3 floats)
 */
extern const rdpq_trifmt_t TRIFMT_ZBUF;

/** 
 * @brief Format descriptor for a z-buffered, shaded triangle.
 * 
 * Vertex array format: `(float){X, Y, Z, R, G, B, A}` (7 floats)
 */
extern const rdpq_trifmt_t TRIFMT_ZBUF_SHADE;

/** 
 * @brief Format descriptor for a z-buffered, textured triangle.
 * 
 * Vertex array format: `(float){X, Y, Z, S, T, INV_W}` (6 floats)
 */
extern const rdpq_trifmt_t TRIFMT_ZBUF_TEX;

/** 
 * @brief Format descriptor for a z-buffered, shaded, textured triangle.
 * 
 * Vertex array format: `(float){X, Y, Z, R, G, B, A, S, T, INV_W}` (10 floats)
 */
extern const rdpq_trifmt_t TRIFMT_ZBUF_SHADE_TEX;

/**
 * @brief Draw a triangle (RDP command: TRI_*)
 * 
 * This function allows to draw a triangle into the framebuffer using RDP, in screen coordinates.
 * RDP does not handle transform and lightning, so it only reasons of screen level coordinates.
 *
 * Each vertex of a triangle is made of up to 4 components:
 * 
 *   * Position. 2 values: X, Y. The values must be in screen coordinates, that is they refer
 *     to the framebuffer pixels. Fractional values allow for subpixel precision. Supported
 *     range is [-4096..4095] (numbers outside that range will be clamped).
 *   * Depth. 1 value: Z. Supported range in [0..1].
 *   * Shade. 4 values: R, G, B, A. The values must be in the 0..1 range.
 *   * Texturing. 3 values: S, T, INV_W. The values S,T address the texture specified by the tile
 *     descriptor. INV_W is the inverse of the W vertex coordinate in clip space (after
 *     projection), a value commonly used to do the final perspective division. This value is
 *     required to do perspective-corrected texturing.
 * 
 * Only the position is mandatory, all other components are optionals, depending on the kind of
 * triangle that needs to be drawn. For instance, specifying only position and shade will allow
 * to draw a gouraud-shaded triangle with no texturing and no z-buffer usage.
 * 
 * The vertex components must be provided via arrays of floating point values. The order of
 * the components within the array is flexible, and can be specified at call time via the
 * #rdpq_trifmt_t structure.
 * 
 * Notice that it is important to configure the correct render modes before calling this function.
 * Specifically:
 * 
 *    * To use the depth component, you must activate the z-buffer via #rdpq_mode_zbuf.
 *    * To use the shade component, you must configure a color combiner formula via #rdpq_mode_combiner.
 *      The formula must use the SHADE slot, to specify the exact pixel formula that will combine the
 *      per-pixel color value with other components, like the texture.
 *    * To use the texturing component, you must configure a color combiner formula via #rdpq_mode_combiner
 *      that uses the TEX0 (and/or TEX1) slot, such as #RDPQ_COMBINER_TEX or #RDPQ_COMBINER_SHADE,
 *      to specify the exact pixel formula that will combine the per-pixel color value with other
 *      components, like the shade. Moreover, you can activate perspective texturing via #rdpq_mode_persp.
 * 
 * If you fail to activate a specific render mode for a provided component, the component will be ignored
 * by RDP. For instance, if you provide S,T,W but do not configure a combiner formula that accesses
 * TEX0, the texture will not be rendered. On the contrary, if you activate a specific render mode
 * but then fail to provide the component (eg: activate z buffering but then fail to provide a depth
 * component), RDP will fall into undefined behavior that can vary from nothing being rendered, garbage
 * on the screen or even a freeze. The rdpq validator will do its best to help you catching these mistakes,
 * so remember to activate it via #rdpq_debug_start whenever you get a surprising result.
 * 
 * For instance, this code snippet will draw a filled triangle, with a flat green color:
 * 
 * @code
 *      // Reset to standard rendering mode.
 *      rdpq_set_mode_standard();
 * 
 *      // Configure the combiner for flat-color rendering
 *      rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
 * 
 *      // Configure the flat color
 *      rdpq_set_prim_color(RGBA32(0, 255, 0, 255));
 * 
 *      // Draw the triangle
 *      float v1[] = { 100, 100 };
 *      float v2[] = { 200, 200 };
 *      float v3[] = { 100, 200 };
 *      rdpq_triangle(&TRIFMT_FILL, v1, v2, v3);
 * @endcode
 * 
 * The three vertices (v1, v2, v3) can be provided in any order (clockwise or counter-clockwise). The
 * function will render the triangle in any case (so back-face culling must be handled before calling
 * it).
 * 
 * @param fmt            Format of the triangle being drawn. This structure specifies the order of the
 *                       components within the vertex arrays, and also some additional rasterization
 *                       parameters. You can pass one of the predefined formats (#TRIFMT_FILL,
 *                       #TRIFMT_TEX, etc.), or a custom one.
 * @param v1             Array of components for vertex 1
 * @param v2             Array of components for vertex 2
 * @param v3             Array of components for vertex 3
 */
void rdpq_triangle(const rdpq_trifmt_t *fmt, const float *v1, const float *v2, const float *v3);

#ifdef __cplusplus
}
#endif

#endif
