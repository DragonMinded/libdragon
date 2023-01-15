/**
 * @file rdpq_tex.h
 * @brief RDP Command queue: high-level texture/sprite loading and blitting
 * @ingroup rdpq
 */

#ifndef LIBDRAGON_RDPQ_TEX_H
#define LIBDRAGON_RDPQ_TEX_H

#include "rdpq.h"
#include <stdint.h>

///@cond
typedef struct surface_s surface_t;
///@endcond

#ifdef __cplusplus
extern "C" {
#endif

// Multi-pass optimized texture loader
// Not part of the public API yet
///@cond
enum tex_load_mode {
    TEX_LOAD_UNKNOWN,
    TEX_LOAD_TILE,
    TEX_LOAD_BLOCK,
};

typedef struct tex_loader_s {
    const surface_t *tex;
    rdpq_tile_t tile;
    struct {
        int width, height;
        int num_texels, tmem_pitch;
        int block_max_lines;
        bool can_load_block;
    } rect;
    int tmem_addr;
    int tlut;
    enum tex_load_mode load_mode;
    void (*load_block)(struct tex_loader_s *tload, int s0, int t0, int s1, int t1);
    void (*load_tile)(struct tex_loader_s *tload, int s0, int t0, int s1, int t1);
} tex_loader_t;
tex_loader_t tex_loader_init(rdpq_tile_t tile, const surface_t *tex);
int tex_loader_load(tex_loader_t *tload, int s0, int t0, int s1, int t1);
///@endcond

/**
 * @brief Load a CI4 texture into TMEM
 * 
 * This is the #FMT_CI4 variant of #rdpq_tex_load. Please refer to 
 * #rdpq_tex_load for more details.
 * 
 * In addition to the standard parameters, this variant also allows to
 * configure the palette number associated with the texture.
 * 
 * @note Remember to call #rdpq_mode_tlut before drawing a texture
 *       using a palette.
 * 
 * @param tile       Tile descriptor that will be initialized with this texture
 * @param tex        Surface containing the texture to load
 * @param tmem_addr  Address in TMEM where the texture will be loaded
 * @param tlut       Palette number to associate with this texture in the tile
 * @return           Number of bytes used in TMEM for this texture
 */
int rdpq_tex_load_ci4(rdpq_tile_t tile, surface_t *tex, int tmem_addr, int tlut);

/**
 * @brief Load a texture into TMEM
 * 
 * This function helps loading a texture into TMEM, which normally involves:
 *  
 *   * Configuring a tile descriptor (via #rdpq_set_tile)
 *   * Setting the source texture image (via #rdpq_set_texture_image)
 *   * Loading the texture (via #rdpq_load_tile or #rdpq_load_block)
 * 
 * After calling this function, the specified tile descriptor will be ready
 * to be used in drawing primitives like #rdpq_triangle or #rdpq_texture_rectangle.
 * 
 * If the texture uses a palette (#FMT_CI8 or #FMT_CI4), the tile descriptor
 * will be initialized pointing to palette 0. In the case of #FMT_CI4, this
 * might not be the correct palette; to specify a different palette number,
 * call #rdpq_tex_load_ci4 directly. Before drawing a texture with palette,
 * remember to call #rdpq_mode_tlut to activate palette mode.
 * 
 * If you want to load a portion of a texture rather than the full texture,
 * use #rdpq_tex_load_sub, or alternatively create a sub-surface using
 * #surface_make_sub and pass it to #rdpq_tex_load. See #rdpq_tex_load_sub
 * for an example of both techniques.
 * 
 * @param tile       Tile descriptor that will be initialized with this texture
 * @param tex        Surface containing the texture to load
 * @param tmem_addr  Address in TMEM where the texture will be loaded
 * @return           Number of bytes used in TMEM for this texture
 * 
 * @see #rdpq_tex_load_sub
 * @see #surface_make_sub
 */
int rdpq_tex_load(rdpq_tile_t tile, surface_t *tex, int tmem_addr);

/**
 * @brief Load a portion of texture into TMEM
 * 
 * This function is similar to #rdpq_tex_load, but only loads a portion of a texture
 * in TMEM. The portion is specified as a rectangle (with exclusive bounds) that must
 * be contained within the original texture.
 * 
 * Notice that, after calling this function, you must draw the polygon using texture
 * coordinates that are contained within the loaded ones. For instance:
 * 
 * @code{.c}
 *      // Load a 32x32 sprite starting at position (100,100) in the
 *      // "spritemap" surface.
 *      rdpq_tex_load_sub(TILE2, spritemap, 0, 100, 100, 132, 132);
 * 
 *      // Draw the sprite. Notice that we must refer to it using the
 *      // original texture coordinates, even if just that portion is in TMEM.
 *      rdpq_texture_rectangle(TILE2,
 *          pos_x, pos_y, pos_x+32, pos_y+32,   // screen coordinates of the sprite
 *          100, 100,                           // texture coordinates
 *          1.0, 1.0);                          // texture increments (= no scaling)
 * @endcode{.c}
 * 
 * An alternative to this function is to call #surface_make_sub on the texture
 * to create a sub-surface, and then call rdpq_tex_load on the sub-surface. 
 * The same data will be loaded into TMEM but this time the RDP ignores that
 * you are loading a portion of a larger texture:
 * 
 * @code{.c}
 *      // Create a sub-surface of spritemap texture. No memory allocations
 *      // or pixel copies are performed, this is just a rectangular "window"
 *      // into the original texture.
 *      surface_t hero = surface_make_sub(spritemap, 100, 100, 32, 32);
 * 
 *      // Load the sub-surface. Notice that the RDP is unaware that it is
 *      // a sub-surface; it will think that it is a whole texture.
 *      rdpq_tex_load(TILE2, &hero, 0);
 * 
 *      // Draw the sprite. Notice that we must refer to it using
 *      // texture coordinates (0,0).
 *      rdpq_texture_rectangle(TILE2,
 *          pos_x, pos_y, pos_x+32, pos_y+32,   // screen coordinates of the sprite
 *          0, 0,                               // texture coordinates
 *          1.0, 1.0);                          // texture increments (= no scaling)
 * @endcode{.c}
 * 
 * The only limit of this second solution is that the sub-surface pointer must
 * be 8-byte aligned (like all RDP textures), so it can only be used if the
 * rectangle that needs to be loaded respects such constraint as well.
 * 
 * There is also a variation for CI4 surfaces that lets you specify the palette number:
 * #rdpq_tex_load_sub_ci4. You can still use #rdpq_tex_load_sub for CI4 surfaces, but
 * the output tile descriptor will always be bound to palette 0.
 * 
 * @param tile       Tile descriptor that will be initialized with this texture
 * @param tex        Surface containing the texture to load
 * @param tmem_addr  Address in TMEM where the texture will be loaded
 * @param s0         Top-left X coordinate of the rectangle to load
 * @param t0         Top-left Y coordinate of the rectangle to load
 * @param s1         Bottom-right *exclusive* X coordinate of the rectangle
 * @param t1         Bottom-right *exclusive* Y coordinate of the rectangle
 * @return int       Number of bytes used in TMEM for this texture
 * 
 * @see #rdpq_tex_load
 * @see #rdpq_tex_load_sub_ci4
 * @see #surface_make_sub
 */
int rdpq_tex_load_sub(rdpq_tile_t tile, surface_t *tex, int tmem_addr, int s0, int t0, int s1, int t1);

/**
 * @brief Load a portion of a CI4 texture into TMEM
 * 
 * This is similar to #rdpq_tex_load_sub, but is specialized for CI4 textures, and allows
 * to specify the palette number to use.
 * 
 * See #rdpq_tex_load_sub for a detailed description.
 * 
 * @param tile       Tile descriptor that will be initialized with this texture
 * @param tex        Surface containing the texture to load
 * @param tmem_addr  Address in TMEM where the texture will be loaded
 * @param tlut       Palette number
 * @param s0         Top-left X coordinate of the rectangle to load
 * @param t0         Top-left Y coordinate of the rectangle to load
 * @param s1         Bottom-right *exclusive* X coordinate of the rectangle
 * @param t1         Bottom-right *exclusive* Y coordinate of the rectangle
 * @return int       Number of bytes used in TMEM for this texture
 * 
 * @see #rdpq_tex_load_sub
 * @see #rdpq_tex_load_ci4
 * @see #surface_make_sub
 */

int rdpq_tex_load_sub_ci4(rdpq_tile_t tile, surface_t *tex, int tmem_addr, int tlut, int s0, int t0, int s1, int t1);

/**
 * @brief Load one or more palettes into TMEM
 * 
 * This function allows to load one or more palettes into TMEM.
 * 
 * When using palettes, the upper half of TMEM is allocated to them. There is room
 * for 256 colors in total, which allows for one palette for a CI8 texture, or up
 * to 16 palettes for CI4 textures.
 * 
 * @param tlut          Pointer to the color entries to load 
 * @param color_idx     First color entry in TMEM that will be written to (0-255)
 * @param num_colors    Number of color entries to load (1-256)
 */
void rdpq_tex_load_tlut(uint16_t *tlut, int color_idx, int num_colors);

/**
 * @brief Blit a surface to the active framebuffer
 * 
 * This is the highest level function for drawing an arbitrary-sized surface
 * to the screen, possibly scaling it.
 * 
 * It handles all the required steps to blit the entire contents of a surface
 * to the framebuffer, that is:
 * 
 *   * Logically split the surface in chunks that fit the TMEM
 *   * Calculate an appropriate scaling factor for each chunk
 *   * Load each chunk into TMEM (via #rdpq_tex_load)
 *   * Draw each chunk to the framebuffer (via #rdpq_texture_rectangle)
 * 
 * Note that this function only performs the actual blits, it does not
 * configure the rendering mode or handle palettes. Before calling this
 * function, make sure to configure the render mode via
 * #rdpq_set_mode_standard (or #rdpq_set_mode_copy if no scaling and pixel
 * format conversion is required). If the surface uses a palette, you also
 * need to load the palette using #rdpq_tex_load_tlut.
 * 
 * @param tile           Tile to use for the blit
 * @param surf           Surface to draw
 * @param x0             Top-left X coordinate on the framebuffer
 * @param y0             Top-left Y coordinate on the framebuffer
 * @param draw_width     Width of the surface on the framebuffer
 * @param draw_height    Height of the surface on the framebuffer
 */
void rdpq_tex_blit(rdpq_tile_t tile, surface_t *surf, int x0, int y0, int draw_width, int draw_height);

#ifdef __cplusplus
}
#endif

#endif