/**
 * @file yuv.c
 * @brief Hardware accelerated YUV conversion
 * @ingroup video
 */

#include "yuv.h"
#include "yuv_internal.h"
#include "rsp.h"
#include "rdpq.h"
#include "rdpq_tex.h"
#include "../rdpq/rdpq_tex_internal.h"
#include "rdpq_mode.h"
#include "rdpq_rect.h"
#include "rdpq_debug.h"
#include "rspq.h"
#include "n64sys.h"
#include "debug.h"
#include "utils.h"
#include <math.h>

/** @brief Internal buffer used to interleave U and V components */
static surface_t internal_buffer;

// Calculated with: yuv_new_colorspace(0.299, 0.114, 16, 219, 224);
const yuv_colorspace_t YUV_BT601_TV = {
    .c0=1.16895, .c1=1.60229, .c2=-0.393299, .c3=-0.816156, .c4=2.02514, .y0=16,
    .k0=175, .k1=-43, .k2=-89, .k3=222, .k4=111, .k5=43
};

// Calculated with: yuv_new_colorspace(0.299, 0.114, 0, 256, 256);
const yuv_colorspace_t YUV_BT601_FULL = {
    .c0=1, .c1=1.402, .c2=-0.344136, .c3=-0.714136, .c4=1.772, .y0=0,
    .k0=179, .k1=-44, .k2=-91, .k3=227, .k4=0, .k5=0
};

// Calculated with: yuv_new_colorspace(0.2126, 0.0722, 16, 219, 224);
const yuv_colorspace_t YUV_BT709_TV = {
    .c0=1.16895, .c1=1.79977, .c2=-0.214085, .c3=-0.534999, .c4=2.12069, .y0=16,
    .k0=197, .k1=-23, .k2=-59, .k3=232, .k4=111, .k5=43
};

// Calculated with: yuv_new_colorspace(0.2126, 0.0722, 0, 256, 256);
const yuv_colorspace_t YUV_BT709_FULL = {
    .c0=1, .c1=1.5748, .c2=-0.187324, .c3=-0.468124, .c4=1.8556, .y0=0,
    .k0=202, .k1=-24, .k2=-60, .k3=238, .k4=0, .k5=0
};


static void resize_internal_buffer(int w, int h)
{
    if (internal_buffer.width != w || internal_buffer.height != h) {
        surface_free(&internal_buffer);
        internal_buffer = surface_alloc(FMT_IA16, w, h);
    }
}

static void yuv_assert_handler(rsp_snapshot_t *state, uint16_t code) {
    switch (code) {
    case ASSERT_INVALID_INPUT_Y:
        printf("Input buffer for Y plane was not configured\n");
        break;
    case ASSERT_INVALID_INPUT_CB:
        printf("Input buffer for CB plane was not configured\n");
        break;
    case ASSERT_INVALID_INPUT_CR:
        printf("Input buffer for CR plane was not configured\n");
        break;
    case ASSERT_INVALID_OUTPUT:
        printf("Output buffer was not configured\n");
        break;
    }
}

static int ovl_yuv;
DEFINE_RSP_UCODE(rsp_yuv,
    .assert_handler = yuv_assert_handler);

#define CMD_YUV_SET_INPUT          0x0
#define CMD_YUV_SET_OUTPUT         0x1
#define CMD_YUV_INTERLEAVE4_32X16  0x2
#define CMD_YUV_INTERLEAVE2_32X16  0x3

static bool yuv_initialized = false;

void yuv_init(void)
{
    if (yuv_initialized)
        return;

    rspq_init();
    ovl_yuv = rspq_overlay_register(&rsp_yuv);
    yuv_initialized = true;
    debugf("YUV initialized %x\n", ovl_yuv);
}

void yuv_close(void)
{
    surface_free(&internal_buffer);
    yuv_initialized = false;	
}

yuv_colorspace_t yuv_new_colorspace(float kr, float kb, int y0i, int yrangei, int crangei)
{
    yuv_colorspace_t cs;
    // Matrix from: https://en.wikipedia.org/wiki/YCbCr#YCbCr
    float kg = 1.0f - kr - kb;
    float m[3][3] = {
        {      kr,                      kg,                   kb,          }, 
        { -0.5f*kr/(1.0f-kb),    -0.5f*kg/(1.0f-kb),         0.5f,         },
        {    0.5f,               -0.5f*kg/(1.0f-kr),    -0.5f*kb/(1.0f-kr) },
    };

    // Invert matrix
    float idet = 1.0f / 
         (m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2]) -
          m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
          m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]));
    float im[3][3] = {
        {(m[1][1] * m[2][2] - m[2][1] * m[1][2]) * idet,
        (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * idet,
        (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * idet},
        {(m[1][2] * m[2][0] - m[1][0] * m[2][2]) * idet,
        (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * idet,
        (m[1][0] * m[0][2] - m[0][0] * m[1][2]) * idet},
        {(m[1][0] * m[2][1] - m[2][0] * m[1][1]) * idet,
        (m[2][0] * m[0][1] - m[0][0] * m[2][1]) * idet,
        (m[0][0] * m[1][1] - m[1][0] * m[0][1]) * idet}
    };

    // Bring range arguments into 0..1 range
    float y0 = y0i * (1.0f / 255.0f);
    float yrange = 256.0f / yrangei;
    float crange = 256.0f / crangei;

    // Using im, we can convert YUV to RGB using a standard
    // matrix multiplication:
    // 
    //   RGB = YUV * im
    //   
    // Fortunately, most elements of the matrix are 0, so we
    // can save a few multiplications and end up with this
    // formula:
    // 
    // Which simplify our formula:
    // 
    //   R = C0 * Y + C1*V
    //   G = C0 * Y + C2*U + C3*V
    //   B = C0 * Y + C4*U
    //   
    // This does not take the range into account. To do so,
    // we can adjust Y by y0, and then pre-multiply yrange
    // into C0, and crange into C1..C4. The final
    // formula will be:
    // 
    //   R = C0 * (Y-y0) + C1*V
    //   G = C0 * (Y-y0) + C2*U + C3*V
    //   B = C0 * (Y-y0) + C4*U
    //   
    // which is the one used by #yuv_to_rgb.
    // 
    cs.c0 = im[0][0] * yrange;
    cs.c1 = im[0][2] * crange;
    cs.c2 = im[1][1] * crange;
    cs.c3 = im[1][2] * crange;
    cs.c4 = im[2][1] * crange;
    cs.y0 = y0i;

    // Now calculate the RDP coefficients. 
    // The RDP cannot do exactly this formula. What the RDP does is
    // slightly different, and it does it in two steps. The first step is
    // the texture filter, which calculates:
    // 
    //    TF_R = Y + K0*V
    //    TF_G = Y + K1*U + K2*V
    //    TF_B = Y + K3*U
    // 
    // The second step is the color combiner, which will use the following
    // formula:
    // 
    //    R = (TF_R - K4) * K5 + TF_R = (TF_R - (K4*K5)/(1+K5)) * (1+K5)
    //    G = (TF_G - K4) * K5 + TF_G = (TF_G - (K4*K5)/(1+K5)) * (1+K5)
    //    B = (TF_B - K4) * K5 + TF_B = (TF_B - (K4*K5)/(1+K5)) * (1+K5)
    //    
    // By concatenating the two steps, we find:
    // 
    //    R = (Y + K0*V        - (K4*K5)/(1+K5))) * (1+K5)
    //    G = (Y + K1*U + K2*V - (K4*K5)/(1+K5))) * (1+K5)
    //    B = (Y + K3*U        - (K4*K5)/(1+K5))) * (1+K5)
    // 
    // So let's now compare this with the standard formula above. We need to find
    // a way to express K0..K5 in terms of C0..C4 (plus y0). Let's take
    // the standard formula and factor C0:
    // 
    //    R = (Y - y0 + C1*V/C0)           * C0
    //    G = (Y - y0 + C2*U/C0 + C3*V/C0) * C0
    //    B = (Y - y0 + C4*U/C0)           * C0
    //    
    // We can now derive all coefficients:
    //   
    //    1+K5 = C0              =>    K5 = C0 - 1
    //    (K4*K5)/(1+K5) = y0    =>    K4 = (y0 * (1+K5)) / K5) = y0/K5 + y0
    //
    //    K0 = C1 / C0
    //    K1 = C2 / C0
    //    K2 = C3 / C0
    //    K3 = C4 / C0
    // 
    float ic0 = 1.0f / cs.c0;
    float k5 = cs.c0 - 1;
    float k4 = k5 != 0 ? y0 / k5 + y0 : 0;
    float k0 = cs.c1 * ic0;
    float k1 = cs.c2 * ic0;
    float k2 = cs.c3 * ic0;
    float k3 = cs.c4 * ic0;
    cs.k0 = roundf(k0*128.f);
    cs.k1 = roundf(k1*128.f);
    cs.k2 = roundf(k2*128.f);
    cs.k3 = roundf(k3*128.f);
    cs.k4 = roundf(k4*255.f);
    cs.k5 = roundf(k5*255.f);
    return cs;
}

color_t yuv_to_rgb(uint8_t y, uint8_t u, uint8_t v, const yuv_colorspace_t *cs)
{
    float yp = (y - cs->y0) * cs->c0;
    float r = yp + cs->c1 * (v-128) + .5f;
    float g = yp + cs->c2 * (u-128) + cs->c3 * (v-128) + .5f;
    float b = yp + cs->c4 * (u-128) + .5f;

    debugf("%d,%d,%d => %f,%f,%f\n", y, u, v, r, g, b);

    return (color_t){
        .r = r > 255 ? 255.f : r < 0 ? 0 : r,
        .g = g > 255 ? 255.f : g < 0 ? 0 : g,
        .b = b > 255 ? 255.f : b < 0 ? 0 : b,
        .a = 0xFF,
    };
}

void rsp_yuv_set_input_buffer(uint8_t *y, uint8_t *cb, uint8_t *cr, int y_pitch)
{
    rspq_write(ovl_yuv, CMD_YUV_SET_INPUT, 
        PhysicalAddr(y), PhysicalAddr(cb), PhysicalAddr(cr), y_pitch);
}

void rsp_yuv_set_output_buffer(uint8_t *out, int out_pitch)
{
    rspq_write(ovl_yuv, CMD_YUV_SET_OUTPUT,
        PhysicalAddr(out), out_pitch);
}

void rsp_yuv_interleave4_block_32x16(int x0, int y0)
{
    rspq_write(ovl_yuv, CMD_YUV_INTERLEAVE4_32X16,
        (x0<<12) | y0);
}

void rsp_yuv_interleave2_block_32x16(int x0, int y0)
{
    rspq_write(ovl_yuv, CMD_YUV_INTERLEAVE2_32X16,
        (x0<<12) | y0);
}

static void yuv_tex_blit_setup(surface_t *yp, surface_t *up, surface_t *vp)
{
    assertf(yp->width == up->width*2 && yp->height == up->height*2, 
        "wrong plane sizes: only YUV 4:2:0 is supported (Y:%dx%d U:%dx%d)",
        yp->width, yp->height, up->width, up->height);
    assertf(yp->width == vp->width*2 && yp->height == vp->height*2, 
        "wrong plane sizes: only YUV 4:2:0 is supported (Y:%dx%d V:%dx%d)",
        yp->width, yp->height, vp->width, vp->height);

    // Make sure we have the internal buffer ready. We will interleave U and V
    // planes so we need a buffer that handles two of those planes at the same time.
    resize_internal_buffer(up->width, up->height);

    // Interleave U and V planes into the internal buffer, using RSP
    rsp_yuv_set_input_buffer(yp->buffer, up->buffer, vp->buffer, yp->width);
    rsp_yuv_set_output_buffer(internal_buffer.buffer, internal_buffer.stride);
    assert((yp->height % 16) == 0 && (yp->width % 32) == 0);
    for (int y=0; y < yp->height; y += 16) {
        for (int x=0; x < yp->width; x += 32) {
            // FIXME: for now this only works with subsampling 4:2:0
            rsp_yuv_interleave2_block_32x16(x, y);
        }
        rspq_flush();
    }

    // Setup the two buffers as RDP lookup addresses, that will be referenced
    // later. This way, we can compile yuv_tex_blit_run in a block.
    rdpq_set_lookup_address(1, yp->buffer);
    rdpq_set_lookup_address(2, internal_buffer.buffer);
}

static void yuv_tex_blit_run(int width, int height, float x0, float y0, 
    const rdpq_blitparms_t *parms, const yuv_colorspace_t *cs)
{
    rdpq_set_mode_yuv(false);
    if (cs) rdpq_set_yuv_parms(cs->k0, cs->k1, cs->k2, cs->k3, cs->k4, cs->k5);

    // To avoid the need of pre-interleaving Y and UV together, we load them
    // separately into TMEM using separate LOAD_BLOCK commands.

    // Tiles used to draw the two lines onto the screen. Notice that the second
    // line will not be preswapped, so we cannot use a single tile for both
    rdpq_set_tile(TILE0, FMT_YUV16, 0,         0, NULL);
    rdpq_set_tile(TILE1, FMT_YUV16, width,     0, NULL);

    // Tiles used to load the UV lines from the internal buffer into TMEM. We
    // load the first line at offset 0 in TMEM, and the second line immediately
    // after (after "width" texels).
    rdpq_set_tile(TILE4, FMT_IA16, 0,         0, NULL);
    rdpq_set_tile(TILE5, FMT_IA16, width,     0, NULL);

    // Tile used to load the Y line from the Y buffer into TMEM. The Y texels
    // are stored in the upper half of TMEM, so we need to load them at offset
    // 2048.
    rdpq_set_tile(TILE6, FMT_I8,   2048,      0, NULL);

    surface_t yp = surface_make_placeholder_linear(1, FMT_I8, width, height);
    surface_t uvp = surface_make_placeholder_linear(2, FMT_IA16, width/2, height/2);

    void ltd_yuv2(rdpq_tile_t tile, const surface_t *_, int s0, int t0, int s1, int t1, 
        void (*draw_cb)(rdpq_tile_t tile, int s0, int t0, int s1, int t1), bool filtering)
    {
        for (int y=t0; y<t1; y+=2) {
            // Load two Y lines with a single LOAD_BLOCK, from the surface configured
            // in lookup block 1. Notice that we will not byteswap the second line.
            rdpq_set_texture_image(&yp);
            rdpq_load_block_fx(TILE6, 0, y, width*2, 0);

            // Load one UV line two times, with two LOAD_BLOCK commands, from the
            // surface configured in lookup block 2. subsequent offsets in TMEM.
            rdpq_set_texture_image(&uvp);
            rdpq_load_block_fx(TILE4, 0, y/2, width, 0);
            rdpq_load_block_fx(TILE5, 0, y/2, width, 0);

            // Configure TILE0/1 to match the two YUV lines that we prepared in TMEM.
            rdpq_set_tile_size(TILE0, 0, y,   width, y+1);
            rdpq_set_tile_size(TILE1, 0, y+1, width, y+2);

            // Call the callback to draw the two lines (unless we are at the end of the screen)
            draw_cb(TILE0, 0, y+0, width, y+1);
            if (y+1 < t1)
                draw_cb(TILE1, 0, y+1, width, y+2);
        }
    }

    // Call rdpq_tex_blit with our custom large texture loader for YUV.
    // We pass a surface with a NULL pointer as our texloader will not need it anyway
    // (it uses the two surfaces configured in rdpq lookup slots 1 and 2).
    surface_t dummy = surface_make_linear(NULL, FMT_I8, width, height);
    __rdpq_tex_blit(&dummy, x0, y0, parms, ltd_yuv2);
}

void yuv_tex_blit(surface_t *yp, surface_t *up, surface_t *vp,
    float x0, float y0, const rdpq_blitparms_t *parms, const yuv_colorspace_t *cs) 
{
    yuv_tex_blit_setup(yp, up, vp);
    yuv_tex_blit_run(yp->width, yp->height, x0, y0, parms, cs);
}

yuv_blitter_t yuv_blitter_new(int video_width, int video_height, float x0, float y0, const rdpq_blitparms_t *parms,
    const yuv_colorspace_t *cs)
{
    // Compile the yuv_tex_blit_run into a block with the given parameters.
    rspq_block_begin();
        yuv_tex_blit_run(video_width, video_height, x0, y0, parms, cs);
    rspq_block_t *block = rspq_block_end();
    return (yuv_blitter_t){
        .block = block,
    };
}

yuv_blitter_t yuv_blitter_new_fmv(int video_width, int video_height,
    int screen_width, int screen_height, const yuv_fmv_parms_t *parms)
{
    static const yuv_fmv_parms_t default_parms = {0};
    if (!parms) parms = &default_parms;

    float scalew = 1.0f, scaleh = 1.0f;

    if (parms->zoom != YUV_ZOOM_NONE && video_width < screen_width && video_height < screen_height) {
        scalew = (float)screen_width / (float)video_width;
        scaleh = (float)screen_height / (float)video_height;
        if (parms->zoom == YUV_ZOOM_KEEP_ASPECT)
            scalew = scaleh = MIN(scalew, scaleh);
    }
    float final_width = video_width * scalew;
    float final_height = video_height * scaleh;

    int x0=0, y0=0;
    if (screen_width) {
        switch (parms->halign) {
        case YUV_ALIGN_CENTER: x0 = (screen_width - final_width) / 2;   break;
        case YUV_ALIGN_MIN:    x0 = 0;                                  break;
        case YUV_ALIGN_MAX:    x0 = screen_width - final_width;         break;
        default: assertf(0, "invalid yuv config: halign=%d", parms->halign);
        }
    }
    if (screen_height) {
        switch (parms->valign) {
        case YUV_ALIGN_CENTER: y0 = (screen_height - final_height) / 2; break;
        case YUV_ALIGN_MIN:    y0 = 0;                                  break;
        case YUV_ALIGN_MAX:    y0 = screen_height - final_height;       break;
        default: assertf(0, "invalid yuv config: valign=%d", parms->valign);
        }
    }

    rspq_block_begin();

        // Clear the screen. To save fillrate, we just clear the part outside
        // of the image that we will draw (if any).
        if (screen_height > final_height || screen_width > final_width) {
            rdpq_set_mode_fill(parms->bkg_color);
            if (y0 > 0)
                rdpq_fill_rectangle(0, 0, screen_width, y0);
            if (y0+final_height < screen_height)
                rdpq_fill_rectangle(0, y0+final_height, screen_width, screen_height);
            if (x0 > 0)
                rdpq_fill_rectangle(0, y0, x0, y0+final_height);
            if (x0+final_width < screen_width)
                rdpq_fill_rectangle(x0+final_width, y0, screen_width, y0+final_height);
        }

        // Do the blit (optionally scaling)
        yuv_tex_blit_run(video_width, video_height, x0, y0, &(rdpq_blitparms_t){
            .scale_x = scalew, .scale_y = scaleh,
        }, parms->cs);

    rspq_block_t *block = rspq_block_end();
    return (yuv_blitter_t){
        .block = block,
    };
}

void yuv_blitter_run(yuv_blitter_t *blitter, surface_t *yp, surface_t *up, surface_t *vp)
{
    yuv_tex_blit_setup(yp, up, vp);
    rspq_block_run(blitter->block);
}

void yuv_blitter_free(yuv_blitter_t *blitter)
{
    rspq_block_free(blitter->block);
    blitter->block = NULL;
}
