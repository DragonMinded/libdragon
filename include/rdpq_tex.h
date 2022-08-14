/**
 * @file rdpq_tex.h
 * @brief RDP Command queue: texture loading
 * @ingroup rdp
 */

#ifndef LIBDRAGON_RDPQ_TEX_H
#define LIBDRAGON_RDPQ_TEX_H

#include "rdpq.h"
#include <stdint.h>

typedef struct surface_s surface_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load a CI4 texture into TMEM
 * 
 * This is the #FMT_CI4 variant of #rdpq_tex_load. Please refer to 
 * #rdpq_tex_load for more details.
 * 
 * In addition to the standard parameters, this variant also allows to
 * configure the palette number associated with the texture.
 * 
 * @note Remember to call #rdpq_mode_tlut before drawing a palettized
 *       texture.
 * 
 * @param tile       Tile descriptor that will be initialized with this texture
 * @param tex        Surface containing the texture to load
 * @param tmem_addr  Address in TMEM where the texture will be loaded
 * @param tlut       Palette number to associate with this texture in the tile
 * @return           Number of bytes used in TMEM for this texture
 */
int rdpq_tex_load_ci4(int tile, surface_t *tex, int tmem_addr, int tlut);

/**
 * @brief Load a texture into TMEM
 * 
 * This function helps loading a (portion of a) texture into TMEM, which
 * normally involves:
 *  
 *   * Configuring a tile descriptor (via #rdpq_set_tile)
 *   * Setting the source texture image (via #rdpq_set_texutre_image)
 *   * Loading the texture (via #rdpq_load_tile or #rdpq_load_block)
 * 
 * This function works with all pixel formats, by dispatching the actual
 * implementations to several variants (eg: #rdpq_tex_load_rgba16). If you
 * know the format of your texture, feel free to call directly the correct
 * variant to save a bit of overhead.
 * 
 * After calling this function, the specified tile descriptor will be ready
 * to be used in drawing primitives like #rdpq_triangle or #rdpq_texture_rectangle.
 * 
 * If the texture is palettized (#FMT_CI8 or #FMT_CI4), the tile descriptor
 * will be initialized pointing to palette 0. In the case of #FMT_CI4, this
 * might not be the correct palette; to specify a different palette number,
 * call #rdpq_tex_load_ci4 directly. Before drawing a palettized texture,
 * remember to call #rdpq_mode_tlut to activate palette mode.
 * 
 * @param tile       Tile descriptor that will be initialized with this texture
 * @param tex        Surface containing the texture to load
 * @param tmem_addr  Address in TMEM where the texture will be loaded
 * @return           Number of bytes used in TMEM for this texture
 */
int rdpq_tex_load(int tile, surface_t *tex, int tmem_addr);

/**
 * @brief Load one or more palettes into TMEM
 * 
 * This function allows to load one or more palettes into TMEM.
 * 
 * When using palettes, the upper half of TMEM is allocated to them. There is room
 * for 256 colors in total, which allows for one pallete for a CI8 texture, or up
 * to 16 paletter for CI4 textures.
 * 
 * @param tlut          Pointer to the color entries to load 
 * @param color_idx     First color entry in TMEM that will be written to (0-255)
 * @param num_colors    Number of color entries to load (1-256)
 */
void rdpq_tex_load_tlut(uint16_t *tlut, int color_idx, int num_colors);

#ifdef __cplusplus
}
#endif

#endif