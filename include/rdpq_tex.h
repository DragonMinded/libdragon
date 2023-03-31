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


// Enable mirroring when wrapping the texture, used in #rdpq_texparms_t
#define MIRROR_REPEAT true
// Disable mirroring when wrapping the texture, used in #rdpq_texparms_t
#define MIRROR_NONE   false
// Enable infinite repeat for the texture, used in #rdpq_texparms_t
#define REPEAT_INFINITE 2048

/**
 * @brief Texture sampling parameters for #rdpq_tex_load.
 * 
 * This structure contains all possible parameters for #rdpq_tex_load.
 * All fields have been made so that the 0 value is always the most
 * reasonable default. This means that you can simply initialize the structure
 * to 0 and then change only the fields you need (for instance, through a 
 * compound literal).
 * 
 */
typedef struct {
    int tmem_addr;           // TMEM address where to load the texture (default: 0)
    int palette;             // Palette number where TLUT is stored (used only for CI4 textures)

    struct {
        float   translate;      // Translate the texture in pixels
        int     scale_log;      // Power of 2 scale modifier of the texture (default: 0)

        float   repeats;        // Number of repetitions (default: unlimited)
        bool    mirror;         // Repetition mode (default: MIRROR_NONE)
    } s, t;

} rdpq_texparms_t;

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
    const rdpq_texparms_t *texparms;
    rdpq_tileparms_t tileparms;
    struct {
        int width, height;
        int num_texels, tmem_pitch;
        int block_max_lines;
        bool can_load_block;
        int s0fx, t0fx, s1fx, t1fx;
    } rect;
    int tmem_addr;
    enum tex_load_mode load_mode;
    void (*load_block)(struct tex_loader_s *tload, int s0, int t0, int s1, int t1);
    void (*load_tile)(struct tex_loader_s *tload, int s0, int t0, int s1, int t1);
} tex_loader_t;
tex_loader_t tex_loader_init(rdpq_tile_t tile, const surface_t *tex);
int tex_loader_load(tex_loader_t *tload, int s0, int t0, int s1, int t1);
void tex_loader_set_tmem_addr(tex_loader_t *tload, int tmem_addr);
int tex_loader_calc_max_height(tex_loader_t *tload, int width);
///@endcond


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
 * will be by default pointing to palette 0. In the case of #FMT_CI4, this
 * might not be the correct palette; to specify a different palette number,
 * add .palette = X to the tex parms. Before drawing a texture with palette,
 * remember to call #rdpq_mode_tlut to activate palette mode.
 * 
 * If you want to load a portion of a texture rather than the full texture,
 * use #rdpq_tex_load_sub, or alternatively create a sub-surface using
 * #surface_make_sub and pass it to #rdpq_tex_load. See #rdpq_tex_load_sub
 * for an example of both techniques.
 * 
 * @param tile       Tile descriptor that will be initialized with this texture
 * @param tex        Surface containing the texture to load
 * @param parms      All optional parameters on where to load the texture and how to sample it. Refer to #rdpq_texparms_t for more information.
 * @return           Number of bytes used in TMEM for this texture
 * 
 * @see #rdpq_tex_load_sub
 * @see #surface_make_sub
 */
int rdpq_tex_load(rdpq_tile_t tile, surface_t *tex, const rdpq_texparms_t *parms);

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
 * @endcode
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
 * @endcode
 * 
 * The only limit of this second solution is that the sub-surface pointer must
 * be 8-byte aligned (like all RDP textures), so it can only be used if the
 * rectangle that needs to be loaded respects such constraint as well.
 * 
 * 
 * @param tile       Tile descriptor that will be initialized with this texture
 * @param tex        Surface containing the texture to load
 * @param parms      All optional parameters on where to load the texture and how to sample it. Refer to #rdpq_texparms_t for more information.
 * @param s0         Top-left X coordinate of the rectangle to load
 * @param t0         Top-left Y coordinate of the rectangle to load
 * @param s1         Bottom-right *exclusive* X coordinate of the rectangle
 * @param t1         Bottom-right *exclusive* Y coordinate of the rectangle
 * @return int       Number of bytes used in TMEM for this texture
 * 
 * @see #rdpq_tex_load
 * @see #surface_make_sub
 */
int rdpq_tex_load_sub(rdpq_tile_t tile, surface_t *tex, const rdpq_texparms_t *parms, int s0, int t0, int s1, int t1);

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
 * @brief Blitting parameters for #rdpq_tex_blit.
 * 
 * This structure contains all possible parameters for #rdpq_tex_blit.
 * The various fields have been designed so that the 0 value is always the most
 * reasonable default. This means that you can simply initialize the structure
 * to 0 and then change only the fields you need (for instance, through a 
 * compound literal).
 * 
 * See #rdpq_tex_blit for several examples.
 */
typedef struct {
    rdpq_tile_t tile;   ///< Base tile descriptor to use (default: TILE_0); notice that two tiles will often be used to do the upload (tile and tile+1).
    int s0;             ///< Source sub-rect top-left X coordinate
    int t0;             ///< Source sub-rect top-left Y coordinate
    int width;          ///< Source sub-rect width. If 0, the width of the surface is used
    int height;         ///< Source sub-rect height. If 0, the height of the surface is used
    bool flip_x;        ///< Flip horizontally. If true, the source sub-rect is treated as horizontally flipped (so flipping is performed before all other transformations)
    bool flip_y;        ///< Flip vertically. If true, the source sub-rect is treated as vertically flipped (so flipping is performed before all other transformations)

    int cx;             ///< Transformation center (aka "hotspot") X coordinate, relative to (s0, t0). Used for all transformations
    int cy;             ///< Transformation center (aka "hotspot") X coordinate, relative to (s0, t0). Used for all transformations
    float scale_x;      ///< Horizontal scale factor to apply to the surface. If 0, no scaling is performed (the same as 1.0f)
    float scale_y;      ///< Vertical scale factor to apply to the surface. If 0, no scaling is performed (the same as 1.0f)
    float theta;        ///< Rotation angle in radians

    // FIXME: replace this with CPU tracking of filtering mode?
    bool filtering;     ///< True if texture filtering is enabled (activates workaround for filtering artifacts when splitting textures in chunks)

    // FIXME: remove this?
    int nx;             ///< Texture horizontal repeat count. If 0, no repetition is performed (the same as 1)
    int ny;             ///< Texture vertical repeat count. If 0, no repetition is performed (the same as 1)
} rdpq_blitparms_t;

/**
 * @brief Blit a surface to the active framebuffer
 * 
 * This is the highest level function for drawing an arbitrary-sized surface
 * to the screen, possibly scaling and rotating it.
 * 
 * It handles all the required steps to blit the entire contents of a surface
 * to the framebuffer, that is:
 * 
 *   * Logically split the surface in chunks that fit the TMEM
 *   * Calculate an appropriate scaling factor for each chunk
 *   * Load each chunk into TMEM (via #rdpq_tex_load)
 *   * Draw each chunk to the framebuffer (via #rdpq_texture_rectangle or #rdpq_triangle)
 * 
 * Note that this function only performs the actual blits, it does not
 * configure the rendering mode or handle palettes. Before calling this
 * function, make sure to configure the render mode via
 * #rdpq_set_mode_standard (or #rdpq_set_mode_copy if no scaling and pixel
 * format conversion is required). If the surface uses a palette, you also
 * need to load the palette using #rdpq_tex_load_tlut.
 * 
 * This function is able to perform many different complex transformations. The
 * implementation has been tuned to try to be as fast as possible for simple
 * blits, but it scales up nicely for more complex operations.
 * 
 * The parameters that describe the transformations to perform are passed in
 * the @p parms structure. The structure contains a lot of fields, but it has
 * been designed so that most of them can be simply initalized to zero to
 * disable advanced behaviors (and thus simply left unmentioned in an inline
 * initialization). 
 * 
 * For instance, this blits a large image to the screen, aligning it to the
 * top-left corner (eg: a splashscreen).
 * 
 * @code{.c}
 *     rdpq_tex_blit(splashscreen, 0, 0, NULL);
 * @endcode
 * 
 * This is the same, but the image will be centered on the screen. To do this,
 * we specify the center of the screen as position, and then we set the hotspost
 * of the image ("cx" and "cy" fields) to its center:
 * 
 * @code{.c}
 *      rdpq_tex_blit(splashscreen, 320/2, 160/2, &(rdpq_blitparms_t){
 *          .cx = splashscreen->width / 2,
 *          .cy = splashscreen->height / 2,
 *      });
 * @endcode
 * 
 * This examples scales a 64x64 image to 256x256, putting its center near the
 * top-left of the screen (so part of resulting image will be offscreen):
 * 
 * @code{.c}
 *      rdpq_tex_blit(splashscreen, 20, 20, &(rdpq_blitparms_t){
 *          .cx = splashscreen->width / 2, .cy = splashscreen->height / 2,
 *          .scale_x = 4.0f, .scale_y = 4.0f,
 *      });
 * @endcode
 * 
 * This example assumes that the surface is a spritemap with frames of size
 * 32x32. It selects the sprite at row 4, column 2, and draws it centered
 * at position 100,100 on the screen applying a rotation of 45 degrees around its center:
 * 
 * @code{.c}
 *     rdpq_tex_blit(splashscreen, 100, 100, &(rdpq_blitparms_t){
 *          .s0 = 32*2, .t0 = 32*4,
 *          .width = 32, .height = 32,
 *          .cx = 16, .cy = 16,
 *          .theta = M_PI/4,
 *     });
 * @endcode
 * 
 * @param surf           Surface to draw
 * @param x0             X coordinate on the framebuffer where to draw the surface
 * @param y0             Y coordinate on the framebuffer where to draw the surface
 * @param parms          Parameters for the blit operation (or NULL for default)
 */
void rdpq_tex_blit(const surface_t *surf, float x0, float y0, const rdpq_blitparms_t *parms);


#ifdef __cplusplus
}
#endif

#endif
