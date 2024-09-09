/**
 * @file graphics.c
 * @brief 2D Graphics
 * @ingroup graphics
 */
#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include "display.h"
#include "graphics.h"
#include "sprite.h"
#include "font.h"
#include "surface.h"
#include "sprite_internal.h"

/**
 * @brief Struct that holds the current loaded font. We load the default font on
 * the first #graphics_draw_character call if #graphics_set_font_sprite was not called.
 */
static struct {
    sprite_t *sprite;
    int font_width;
    int font_height;
} sprite_font = { .sprite = NULL, .font_width = 8, .font_height = 8 };


/**
 * @brief Macro to set a pixel to a certain color in a buffer
 *
 * @note This macro uses the currently calculated video width as the implicit
 * width of the video buffer, so it is not applicable to be used on arbitrary
 * buffers.
 *
 * @param[out] buffer
 *             Pointer to a uint16_t or uint32_t array treated as a video buffer
 * @param[in]  x
 *             X coordinate in pixels to place the color
 * @param[in]  y
 *             Y coordinate in pixels to place the color 
 * @param[in]  color
 *             A 16 or 32 bit color value to set the requested pixel to.  Must match
 *             the buffer in pixel bit width.
 */
#define __set_pixel( buffer, x, y, color ) \
    (buffer)[(x) + ((y) * pix_stride)] = color

/**
 * @brief Macro to get a pixel color from a buffer
 *
 * @param[in] buffer
 *            Pointer to a uint16_t or uint32_t array treated as a video buffer
 * @param[in] x
 *            X coordinate in pixels to grab the color from
 * @param[in] y
 *            Y coordinate in pixels to grab the color from
 *
 * @return The 16 or 32 bit color of the pixel at (x, y)
 */
#define __get_pixel( buffer, x, y ) \
    (buffer)[(x) + ((y) * pix_stride)]

/**
 * @brief Get the correct video buffer given a display context
 *
 * @param[in] disp
 *            The current display context
 *
 * @return A pointer to the current drawing surface for the display context
 */
#define __get_buffer( disp ) ((disp)->buffer)

/**
 * @brief Generic foreground color
 *
 * This is white on 16 and 32 BPP modes 
 */
static uint32_t f_color = 0xFFFFFFFF;
/** 
 * @brief Generic background color
 *
 * This is transparent on 16 and 32 BPP modes 
 */
static uint32_t b_color = 0x00000000;

uint32_t graphics_make_color( int r, int g, int b, int a )
{
    color_t color;

    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;

    return graphics_convert_color( color );
}

uint32_t graphics_convert_color( color_t color )
{
    if( display_get_bitdepth() == 2 )
    {
        // Pack twice for compatibility with RDP packed colors and the old deprecated RDP API.
        uint32_t conv = color_to_packed16(color);
        return conv | (conv << 16);
    }
    else
    {
        return color_to_packed32(color);
    }
}

void graphics_set_color( uint32_t forecolor, uint32_t backcolor )
{
    f_color = forecolor;
    b_color = backcolor;
}

/**
 * @brief Return whether a color is fully transparent at a particular bit depth
 *
 * @param[in] bitdepth
 *            Either 2 or 4 for 16 bpp or 32 bpp mode.
 * @param[in] color
 *            32-bit RBA color.  Use #graphics_convert_color  or #graphics_make_color to generate this value.
 *
 * @retval 1 if the color is fully transparent
 * @retval 0 if the color is translucent or opaque
 */
static int __is_transparent( int bitdepth, uint32_t color )
{
    if( bitdepth == 2 )
    {
        /* Is alpha bit set? */
        if( (color & 0x1) == 0x0 ) { return 1; }
    }
    else
    {
        /* Is alpha byte set? */
        if( (color & 0xFF) == 0x00 ) { return 1; }
    }

    return 0;
}

void graphics_draw_pixel( surface_t* disp, int x, int y, uint32_t color )
{
    if( disp == 0 ) { return; }
    int pix_stride = TEX_FORMAT_BYTES2PIX(surface_get_format(disp), disp->stride);

    if( TEX_FORMAT_BITDEPTH(surface_get_format( disp )) == 16 )
    {
        __set_pixel( (uint16_t *)__get_buffer( disp ), x, y, color );
    }
    else
    {
        __set_pixel( (uint32_t *)__get_buffer( disp ), x, y, color );
    }
}

void graphics_draw_pixel_trans( surface_t* disp, int x, int y, uint32_t color )
{
    if( disp == 0 ) { return; }
    int pix_stride = TEX_FORMAT_BYTES2PIX(surface_get_format(disp), disp->stride);

    if( TEX_FORMAT_BITDEPTH(surface_get_format( disp )) == 16 )
    {
        /* Only display the pixel if alpha bit is set */
        if( !__is_transparent( 2, color ) )
        {
            __set_pixel( (uint16_t *)__get_buffer( disp ), x, y, color );
        }
    }
    else
    {
        /* Get 32bit representations */
        uint32_t cur_color = __get_pixel( (uint32_t *)__get_buffer( disp ), x, y );

        /* Get current color */
        uint32_t cr = (cur_color >> 24) & 0xFF;
        uint32_t cg = (cur_color >> 16) & 0xFF;
        uint32_t cb = (cur_color >> 8) & 0xFF;

        /* Get new color */
        uint32_t sr = (color >> 24) & 0xFF;
        uint32_t sg = (color >> 16) & 0xFF;
        uint32_t sb = (color >> 8) & 0xFF;

        /* Transparencies */
        uint32_t st = color & 0xFF;
        uint32_t ct = 255 - st;

        /* Mixed color */
        uint32_t mixed_color;
        uint8_t *final_color = (uint8_t *)(&mixed_color);

        final_color[0] = ((cr * ct) + (sr * st)) >> 8;
        final_color[1] = ((cg * ct) + (sg * st)) >> 8;
        final_color[2] = ((cb * ct) + (sb * st)) >> 8;

        /* Since we are doing mixing anyway */
        final_color[3] = 255;

        __set_pixel( (uint32_t *)__get_buffer( disp ), x, y, mixed_color );
    }
}

void graphics_draw_line( surface_t* disp, int x0, int y0, int x1, int y1, uint32_t color )
{
	int dy = y1 - y0;
	int dx = x1 - x0;
	int sx, sy;

	if(dy < 0)
	{
		dy = -dy;
		sy = -1;
	}
	else
		sy = 1;

	if(dx < 0)
	{
		dx = -dx;
		sx = -1;
	}
	else
		sx = 1;

	dy <<= 1;
	dx <<= 1;

	graphics_draw_pixel(disp, x0, y0, color);
	if(dx > dy)
	{
		int frac = dy - (dx >> 1);
		while(x0 != x1)
		{
			if(frac >= 0)
			{
				y0 += sy;
				frac -= dx;
			}
			x0 += sx;
			frac += dy;
			graphics_draw_pixel(disp, x0, y0 , color);
		}
	}
	else
	{
		int frac = dx - (dy >> 1);
		while(y0 != y1)
		{
			if(frac >= 0)
			{
				x0 += sx;
				frac -= dy;
			}
			y0 += sy;
			frac += dx;
			graphics_draw_pixel(disp, x0, y0 , color);
		}
	}
}

void graphics_draw_line_trans( surface_t* disp, int x0, int y0, int x1, int y1, uint32_t color )
{
	int dy = y1 - y0;
	int dx = x1 - x0;
	int sx, sy;

	if(dy < 0)
	{
		dy = -dy;
		sy = -1;
	}
	else
		sy = 1;

	if(dx < 0)
	{
		dx = -dx;
		sx = -1;
	}
	else
		sx = 1;

	dy <<= 1;
	dx <<= 1;

	graphics_draw_pixel_trans(disp, x0, y0, color);
	if(dx > dy)
	{
		int frac = dy - (dx >> 1);
		while(x0 != x1)
		{
			if(frac >= 0)
			{
				y0 += sy;
				frac -= dx;
			}
			x0 += sx;
			frac += dy;
			graphics_draw_pixel_trans(disp, x0, y0 , color);
		}
	}
	else
	{
		int frac = dx - (dy >> 1);
		while(y0 != y1)
		{
			if(frac >= 0)
			{
				x0 += sx;
				frac -= dy;
			}
			y0 += sy;
			frac += dx;
			graphics_draw_pixel_trans(disp, x0, y0 , color);
		}
	}
}

void graphics_draw_box( surface_t* disp, int x, int y, int width, int height, uint32_t color )
{
    if( disp == 0 ) { return; }

    int pix_stride = TEX_FORMAT_BYTES2PIX(surface_get_format(disp), disp->stride);
    if( TEX_FORMAT_BITDEPTH(surface_get_format( disp )) == 16 )
    {
        uint16_t *buffer16 = (uint16_t *)__get_buffer( disp );

        for(int j = y; j < y + height; j++)
        {
            for(int i = x; i < x + width; i++)
            {
                __set_pixel( buffer16, i, j, color );
            }
        }
    }
    else
    {
        uint32_t *buffer32 = (uint32_t *)__get_buffer( disp );

        for(int j = y; j < y + height; j++)
        {
            for(int i = x; i < x + width; i++)
            {
                __set_pixel( buffer32, i, j, color );
            }
        }
    }
}

void graphics_draw_box_trans( surface_t* disp, int x, int y, int width, int height, uint32_t color )
{
    if( disp == 0 ) { return; }

    int pix_stride = TEX_FORMAT_BYTES2PIX(surface_get_format(disp), disp->stride);
    if( TEX_FORMAT_BITDEPTH(surface_get_format( disp )) == 16 )
    {
        uint16_t *buffer16 = (uint16_t *)__get_buffer( disp );

        for(int j = y; j < y + height; j++)
        {
            for(int i = x; i < x + width; i++)
            {
                /* Only display the pixel if alpha bit is set */
                if( !__is_transparent( 2, color ) )
                {
                    __set_pixel( buffer16, i, j, color );
                }
            }
        }
    }
    else
    {
        uint32_t *buffer32 = (uint32_t *)__get_buffer( disp );

        for(int j = y; j < y + height; j++)
        {
            for(int i = x; i < x + width; i++)
            {
                /* Get 32bit representations */
                uint32_t cur_color = __get_pixel( buffer32, i, j );

                /* Get current color */
                uint32_t cr = (cur_color >> 24) & 0xFF;
                uint32_t cg = (cur_color >> 16) & 0xFF;
                uint32_t cb = (cur_color >> 8) & 0xFF;

                /* Get new color */
                uint32_t sr = (color >> 24) & 0xFF;
                uint32_t sg = (color >> 16) & 0xFF;
                uint32_t sb = (color >> 8) & 0xFF;

                /* Transparencies */
                uint32_t st = color & 0xFF;
                uint32_t ct = 255 - st;

                /* Mixed color */
                uint32_t mixed_color;
                uint8_t *final_color = (uint8_t *)(&mixed_color);

                final_color[0] = ((cr * ct) + (sr * st)) >> 8;
                final_color[1] = ((cg * ct) + (sg * st)) >> 8;
                final_color[2] = ((cb * ct) + (sb * st)) >> 8;

                /* Since we are doing mixing anyway */
                final_color[3] = 255;

                __set_pixel( buffer32, i, j, mixed_color );
            }
        }
    }
}

void graphics_fill_screen( surface_t* disp, uint32_t c )
{
    if( disp == 0 ) { return; }

    int len = TEX_FORMAT_PIX2BYTES(surface_get_format(disp), disp->width * disp->height) / 8;

    uint64_t c64 = ((uint64_t)c << 32) | c;
    uint64_t *buffer = (uint64_t *)__get_buffer(disp);
    for( int i = 0; i < len; i++ )
        buffer[i] = c64;
}

void graphics_set_default_font( void )
{
    sprite_font.sprite = NULL;
    sprite_font.font_width = 8;
    sprite_font.font_height = 8;
}

void graphics_set_font_sprite( sprite_t *font )
{
    sprite_font.sprite = font;
    sprite_font.font_width = sprite_font.sprite->width / sprite_font.sprite->hslices;
    sprite_font.font_height = sprite_font.sprite->height / sprite_font.sprite->vslices;
}

void graphics_draw_character( surface_t* disp, int x, int y, char ch )
{
    if( disp == 0 ) { return; }

    int pix_stride = TEX_FORMAT_BYTES2PIX(surface_get_format(disp), disp->stride);
    int depth = display_get_bitdepth();

    // resetting to default font if bit depth has been changed
    if( sprite_font.sprite != NULL && depth*8 != TEX_FORMAT_BITDEPTH(sprite_get_format(sprite_font.sprite)) )
    {
        graphics_set_default_font();
    }

    /* Figure out if they want the background to be transparent */
    int trans = __is_transparent( depth, b_color );

    if ( sprite_font.sprite != NULL )
    {
        // Use custom font
        const int sx = ( ch % sprite_font.sprite->hslices ) * sprite_font.font_width;
        const int sy = ( ch / sprite_font.sprite->hslices ) * sprite_font.font_height;
        const int ex = sx + sprite_font.font_width;
        const int ey = sy + sprite_font.font_height;

        const int tx = x - sx;
        const int ty = y - sy;

        if( depth == 2 )
        {
            uint16_t *buffer = (uint16_t *)__get_buffer( disp );
            uint16_t *sp_data = (uint16_t *)sprite_font.sprite->data;

            for( int yp = sy; yp < ey; yp++ )
            {
                const register int run = yp * sprite_font.sprite->width;

                for( int xp = sx; xp < ex; xp++ )
                {
                    const char c = sp_data[xp + run];
                    int active = !__is_transparent (depth, c);
                    if( trans )
                    {
                        if( active )
                        {
                            /* Only draw it if it is active */
                            __set_pixel( buffer, tx + xp, ty + yp, f_color );
                        }
                    }
                    else
                    {
                        /* Display foreground or background depending on font data */
                        __set_pixel( buffer, tx + xp, ty + yp, active ? f_color : b_color );
                    }
                }
            }
        }
        else
        {
            uint32_t *buffer = (uint32_t *)__get_buffer( disp );
            uint32_t *sp_data = (uint32_t *)sprite_font.sprite->data;

            for( int yp = sy; yp < ey; yp++ )
            {
                const register int run = yp * sprite_font.sprite->width;

                for( int xp = sx; xp < ex; xp++ )
                {
                    const char c = sp_data[xp + run];
                    int active = !__is_transparent (depth, c);
                    if( trans )
                    {
                        if( active )
                        {
                            /* Only draw it if it is active */
                            __set_pixel( buffer, tx + xp, ty + yp, f_color );
                        }
                    }
                    else
                    {
                        /* Display foreground or background depending on font data */
                        __set_pixel( buffer, tx + xp, ty + yp, active ? f_color : b_color );
                    }
                }
            }
        }
    } else {
        // Use 1bpp default font
        int trans = __is_transparent( depth, b_color );

        if( depth == 2 )
        {
            uint16_t *buffer = (uint16_t *)__get_buffer( disp );

            for( int row = 0; row < 8; row++ )
            {
                unsigned char c = __font_data[(ch * 8) + row];

                for( int col = 0; col < 8; col++ )
                {
                    int active = (c & 0x80) != 0;
                    if( trans )
                    {
                        if( active )
                        {
                            /* Only draw it if it is active */
                            __set_pixel( buffer, x + col, y + row, f_color );
                        }
                    }
                    else
                    {
                        /* Display foreground or background depending on font data */
                        __set_pixel( buffer, x + col, y + row, active ? f_color : b_color );
                    }

                    c <<= 1;
                }
            }
        }
        else
        {
            uint32_t *buffer = (uint32_t *)__get_buffer( disp );

            for( int row = 0; row < 8; row++ )
            {
                unsigned char c = __font_data[(ch * 8) + row];

                for( int col = 0; col < 8; col++ )
                {
                    int active = (c & 0x80) != 0;
                    if( trans )
                    {
                        if( active )
                        {
                            /* Only draw it if it is active */
                            __set_pixel( buffer, x + col, y + row, f_color );
                        }
                    }
                    else
                    {
                        /* Display foreground or background depending on font data */
                        __set_pixel( buffer, x + col, y + row, active ? f_color : b_color );
                    }

                    c <<= 1;
                }
            }
        }
    }
}

void graphics_draw_text( surface_t* disp, int x, int y, const char * const msg )
{
    if( disp == 0 ) { return; }
    if( msg == 0 ) { return; }

    int tx = x;
    int ty = y;
    const char *text = (const char *)msg;

    while( *text )
    {
        switch( *text )
        {
            case '\r':
            case '\n':
                tx = x;
                ty += sprite_font.font_height;
                break;
            case ' ':
                tx += sprite_font.font_width;
                break;
            case '\t':
                tx += sprite_font.font_width * 5;
                break;
            default:
                graphics_draw_character( disp, tx, ty, *text );
                tx += sprite_font.font_width;
                break;
        }

        text++;
    }
}

void graphics_draw_sprite( surface_t* disp, int x, int y, sprite_t *sprite )
{
    /* Simply a wrapper to call the original functionality */
    graphics_draw_sprite_stride( disp, x, y, sprite, -1 );
}

void graphics_draw_sprite_stride( surface_t* disp, int x, int y, sprite_t *sprite, int offset )
{
    /* Sanity checking */
    if( disp == 0 ) { return; }
    if( sprite == 0 ) { return; }
    __sprite_upgrade(sprite);

    /* For spritemaps */
    int tx = x;
    int ty = y;

    /* Calculate the location size of the actual sprite we will be blitting */
    int sx, sy, ex, ey;

    if( offset >= 0 )
    {
        /* For sprites that are not spritemaps, this evaluates to the original */
        int twidth = sprite->width / sprite->hslices;
        int theight = sprite->height / sprite->vslices;

        sx = (offset % sprite->hslices) * twidth;
        sy = (offset / sprite->hslices) * theight;
        ex = sx + twidth;
        ey = sy + theight;

        tx -= sx;
        ty -= sy;
    }
    else
    {
        /* Retain original functionality */
        sx = 0;
        sy = 0;
        ex = sprite->width;
        ey = sprite->height;
    }

    /* Too far left */
    if( (tx + ex) <= 0 ) { return; }

    /* Too far up */
    if( (ty + ey) <= 0 ) { return; }

    /* Too far right */
    if( tx >= (int)disp->width ) { return; }

    /* Too far down */
    if( ty >= (int)disp->height ) { return; }

    /* Clipping left */
    if( x < 0 )
    {
        sx += (x * -1);
    }

    /* Clipping top */
    if( y < 0 )
    {
        sy += (y * -1);
    }

    /* Clipping right */
    if( (tx + ex) >= (int)disp->width )
    {
        ex = disp->width - tx;
    }

    /* Clipping bottom */
    if( (ty + ey) >= disp->height )
    {
        ey = disp->height - ty;
    }

    int pix_stride = TEX_FORMAT_BYTES2PIX(surface_get_format(disp), disp->stride);
    int depth = TEX_FORMAT_BITDEPTH(surface_get_format( disp ));

    /* Only display sprite if it matches the bitdepth */
    if( depth == 16 && TEX_FORMAT_BITDEPTH(sprite_get_format(sprite)) == 16 )
    {
        uint16_t *buffer = (uint16_t *)__get_buffer( disp );
        uint16_t *sp_data = (uint16_t *)sprite->data;

        for( int yp = sy; yp < ey; yp++ )
        {
            const register int run = yp * sprite->width;

            for( int xp = sx; xp < ex; xp++ )
            {
                __set_pixel( buffer, tx + xp, ty + yp, sp_data[xp + run] );
            }
        }
    }
    else if( depth == 32 && TEX_FORMAT_BITDEPTH(sprite_get_format(sprite)) == 32 )
    {
        uint32_t *buffer = (uint32_t *)__get_buffer( disp );
        uint32_t *sp_data = (uint32_t *)sprite->data;

        for( int yp = sy; yp < ey; yp++ )
        {
            const register int run = yp * sprite->width;

            for( int xp = sx; xp < ex; xp++ )
            {
                __set_pixel( buffer, tx + xp, ty + yp, sp_data[xp + run] );
            }
        }
    }
}

void graphics_draw_sprite_trans( surface_t* disp, int x, int y, sprite_t *sprite )
{
    /* Simply a wrapper to call the original functionality */
    graphics_draw_sprite_trans_stride( disp, x, y, sprite, -1 );
}

void graphics_draw_sprite_trans_stride( surface_t* disp, int x, int y, sprite_t *sprite, int offset )
{
    /* Sanity checking */
    if( disp == 0 ) { return; }
    if( sprite == 0 ) { return; }
    __sprite_upgrade(sprite);

    /* For spritemaps */
    int tx = x;
    int ty = y;

    /* Calculate the location size of the actual sprite we will be blitting */
    int sx, sy, ex, ey;

    if( offset >= 0 )
    {
        /* For sprites that are not spritemaps, this evaluates to the original */
        int twidth = sprite->width / sprite->hslices;
        int theight = sprite->height / sprite->vslices;

        sx = (offset % sprite->hslices) * twidth;
        sy = (offset / sprite->hslices) * theight;
        ex = sx + twidth;
        ey = sy + theight;

        tx -= sx;
        ty -= sy;
    }
    else
    {
        /* Retain original functionality */
        sx = 0;
        sy = 0;
        ex = sprite->width;
        ey = sprite->height;
    }

    /* Too far left */
    if( (tx + ex) <= 0 ) { return; }

    /* Too far up */
    if( (ty + ey) <= 0 ) { return; }

    /* Too far right */
    if( tx >= (int)disp->width ) { return; }

    /* Too far down */
    if( ty >= (int)disp->height ) { return; }

    /* Clipping left */
    if( x < 0 )
    {
        sx += (x * -1);
    }

    /* Clipping top */
    if( y < 0 )
    {
        sy += (y * -1);
    }

    /* Clipping right */
    if( (tx + ex) >= (int)disp->width )
    {
        ex = disp->width - tx;
    }

    /* Clipping bottom */
    if( (ty + ey) >= disp->height )
    {
        ey = disp->height - ty;
    }

    int pix_stride = TEX_FORMAT_BYTES2PIX(surface_get_format(disp), disp->stride);
    int depth = TEX_FORMAT_BITDEPTH(surface_get_format( disp ));

    /* Only display sprite if it matches the bitdepth */
    if( depth == 16 && TEX_FORMAT_BITDEPTH(sprite_get_format(sprite)) == 16 )
    {
        uint16_t *buffer = (uint16_t *)__get_buffer( disp );
        uint16_t *sp_data = (uint16_t *)sprite->data;

        for( int yp = sy; yp < ey; yp++ )
        {
            const register int run = yp * sprite->width;

            for( int xp = sx; xp < ex; xp++ )
            {
                /* Only display the pixel if alpha bit is set */
                if( !__is_transparent( 2, sp_data[xp + run] ) )
                {
                    __set_pixel( buffer, tx + xp, ty + yp, sp_data[xp + run] );
                }
            }
        }
    }
    else if( depth == 32 && TEX_FORMAT_BITDEPTH(sprite_get_format(sprite)) == 32 )
    {
        uint32_t *buffer = (uint32_t *)__get_buffer( disp );
        uint32_t *sp_data = (uint32_t *)sprite->data;

        for( int yp = sy; yp < ey; yp++ )
        {
            const int run = yp * sprite->width;

            for( int xp = sx; xp < ex; xp++ )
            {
                /* Get 32bit representations */
                uint8_t *new_color = (uint8_t *)(&sp_data[xp + run]);
                uint32_t cur_color = __get_pixel( buffer, tx + xp, ty + yp );

                /* Get current color */
                uint32_t cr = (cur_color >> 24) & 0xFF;
                uint32_t cg = (cur_color >> 16) & 0xFF;
                uint32_t cb = (cur_color >> 8) & 0xFF;

                /* Get new color */
                uint32_t sr = new_color[0];
                uint32_t sg = new_color[1];
                uint32_t sb = new_color[2];

                /* Transparencies */
                uint32_t st = new_color[3];
                uint32_t ct = 255 - st;

                /* Mixed color */
                uint32_t mixed_color;
                uint8_t *final_color = (uint8_t *)(&mixed_color);

                final_color[0] = ((cr * ct) + (sr * st)) >> 8;
                final_color[1] = ((cg * ct) + (sg * st)) >> 8;
                final_color[2] = ((cb * ct) + (sb * st)) >> 8;

                /* Since we are doing mixing anyway */
                final_color[3] = 255;

                __set_pixel( buffer, tx + xp, ty + yp, mixed_color );
            }
        }
    }
}

extern inline uint16_t color_to_packed16(color_t c);
extern inline uint32_t color_to_packed32(color_t c);
extern inline color_t color_from_packed16(uint16_t c);
extern inline color_t color_from_packed32(uint32_t c);
