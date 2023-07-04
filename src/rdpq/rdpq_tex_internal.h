#ifndef LIBDRAGON_RDPQ_TEX_INTERNAL_H
#define LIBDRAGON_RDPQ_TEX_INTERNAL_H

/**
 * @brief Helper function to draw a large surface that doesn't fit in TMEM.
 * 
 * This function analyzes the surface, finds the optimal splitting strategy to
 * divided into rectangles that fit TMEM, and then go through them one of by one,
 * loading them into TMEM and drawing them.
 * 
 * The actual drawing is done by the caller, through the draw_cb function. This
 * function will just call it with the information on the current rectangle
 * within the original surface.
 * 
 * @param tile          Hint of the tile to use. Note that this function is free to use
 *                      other tiles to perform its job.
 * @param tex           Surface to draw
 * @param s0            Starting X coordinate in the texture to draw
 * @param t0            Starting Y coordinate in the texture to draw
 * @param s1            Ending X coordinate in the texture to draw
 * @param t1            Ending Y coordinate in the texture to draw
 * @param draw_cb       Callback function to draw rectangle by rectangle. It will be called
 *                      with the tile to use for drawing, and the rectangle of the original
 *                      surface that has been loaded into TMEM.
 * @param filtering     Enable texture filtering workaround
 */
typedef void (*large_tex_draw)(rdpq_tile_t tile, const surface_t *tex, int s0, int t0, int s1, int t1, 
    void (*draw_cb)(rdpq_tile_t tile, int s0, int t0, int s1, int t1), bool filtering);

void __rdpq_tex_blit(const surface_t *surf, float x0, float y0, const rdpq_blitparms_t *parms, large_tex_draw ltd);

#endif
