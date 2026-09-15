/**
 * @file rdpq_tex_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
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

/**
 * @brief Common one-time setup shared by BLIT-shaped frontends.
 *
 * This deliberately contains only source-rectangle and scalar normalization.
 * Backends keep their own traversal and hot draw loops: ordinary BLIT splits
 * source texture space, while BlendFX's DESTEX backend additionally splits
 * destination feedback space. Keeping this helper inline makes the generated
 * formerly duplicated expressions without adding a per-chunk callback or call.
 */
typedef struct rdpq_blit_plan_s {
    int s0, t0;
    int s1, t1;
    int width, height;
    int cx, cy;              ///< Hotspot in absolute source coordinates
    float scale_x, scale_y;  ///< Zero normalized to one; sign is preserved
} rdpq_blit_plan_t;

static inline rdpq_blit_plan_t __rdpq_blit_plan(
    const surface_t *surf, const rdpq_blitparms_t *parms)
{
    int width = parms->width ? parms->width : surf->width;
    int height = parms->height ? parms->height : surf->height;
    return (rdpq_blit_plan_t){
        .s0 = parms->s0,
        .t0 = parms->t0,
        .s1 = parms->s0 + width,
        .t1 = parms->t0 + height,
        .width = width,
        .height = height,
        .cx = parms->s0 + parms->cx,
        .cy = parms->t0 + parms->cy,
        .scale_x = parms->scale_x == 0.0f ? 1.0f : parms->scale_x,
        .scale_y = parms->scale_y == 0.0f ? 1.0f : parms->scale_y,
    };
}

void __rdpq_tex_blit(const surface_t *surf, float x0, float y0, const rdpq_blitparms_t *parms, large_tex_draw ltd);

#endif
