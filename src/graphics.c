#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include "display.h"
#include "graphics.h"

/* @todo DO THIS RIGHT */
#include "font.c"

#define __set_pixel( buffer, x, y, color ) \
    (buffer)[(x) + ((y) * __width)] = color

#define __get_pixel( buffer, x, y ) \
    (buffer)[(x) + ((y) * __width)]

#define __get_buffer( x ) __safe_buffer[(x)-1]

extern uint32_t __bitdepth;
extern uint32_t __width;
extern uint32_t __height;
extern void *__safe_buffer[];

/* White on both bitdepths, transparent on both bitdepths */
static uint32_t f_color = 0xFFFFFFFF;
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
    if( __bitdepth == 2 )
    {
        // 8 to 5 bit;
        int r = color.r >> 3;
        int g = color.g >> 3;
        int b = color.b >> 3;

        // Pack twice for compatibility with RDP packed colors
        uint32_t conv = ((r & 0x1F) << 11) | ((g & 0x1F) << 6) | ((b & 0x1F) << 1) | (color.a >> 7);

        return conv | (conv << 16);
    }
    else
    {
        return (color.r << 24) | (color.g << 16) | (color.b << 8) | (color.a);
    }
}

void graphics_set_color( uint32_t forecolor, uint32_t backcolor )
{
    f_color = forecolor;
    b_color = backcolor;
}

void graphics_draw_pixel( display_context_t disp, int x, int y, uint32_t color )
{
    if( disp == 0 ) { return; }

    if( __bitdepth == 2 )
    {
        __set_pixel( (uint16_t *)__get_buffer( disp ), x, y, color );
    }
    else
    {
        __set_pixel( (uint32_t *)__get_buffer( disp ), x, y, color );
    }
}

void graphics_draw_line( display_context_t disp, int x0, int y0, int x1, int y1, uint32_t color )
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

void graphics_draw_box( display_context_t disp, int x, int y, int width, int height, uint32_t color )
{
    if( disp == 0 ) { return; }

    if( __bitdepth == 2 )
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

void graphics_fill_screen( display_context_t disp, uint32_t c )
{
    if( disp == 0 ) { return; }

    if( __bitdepth == 2 )
    {
        uint16_t *buffer = (uint16_t *)__get_buffer( disp );

        for( int i = 0; i < __width * __height; i++ )
        {
            buffer[i] = c;
        }
    }
    else
    {
        uint32_t *buffer = (uint32_t *)__get_buffer( disp );

        for( int i = 0; i < __width * __height; i++ )
        {
            buffer[i] = c;
        }
    }
}

int __is_transparent( int bitdepth, uint32_t color )
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

void graphics_draw_character( display_context_t disp, int x, int y, uint32_t fc, uint32_t bc, char ch )
{
    if( disp == 0 ) { return; }

    int depth = __bitdepth;

    /* Figure out if they want the background to be transparent */
    int trans = __is_transparent( depth, bc );

    if( depth == 2 )
    {
        uint16_t *buffer = (uint16_t *)__get_buffer( disp );

        for( int row = 0; row < 8; row++ )
        {
            unsigned char c = __font_data[(ch * 8) + row];

            for( int col = 0; col < 8; col++ )
            {
                if( trans )
                {
                    if( c & 0x80 )
                    {
                        /* Only draw it if it is active */
                        __set_pixel( buffer, x + col, y + row, fc );
                    }
                }
                else
                {
                    /* Display foreground or background depending on font data */
                    __set_pixel( buffer, x + col, y + row, (c & 0x80) ? fc : bc );
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
                if( trans )
                {
                    if( c & 0x80 )
                    {
                        /* Only draw it if it is active */
                        __set_pixel( buffer, x + col, y + row, fc );
                    }
                }
                else
                {
                    /* Display foreground or background depending on font data */
                    __set_pixel( buffer, x + col, y + row, (c & 0x80) ? fc : bc );
                }

                c <<= 1;
            }
        }
    }
}

void graphics_draw_text( display_context_t disp, int x, int y, char *msg )
{
    if( disp == 0 ) { return; }
    if( msg == 0 ) { return; }

    int tx = x;
    int ty = y;

    while( *msg )
    {
        switch( *msg )
        {
            case '\r':
            case '\n':
                tx = x;
                ty += 8;
                break;
            case ' ':
                tx += 8;
                break;
            case '\t':
                tx += 8 * 5;
                break;
            default:
                graphics_draw_character( disp, tx, ty, f_color, b_color, *msg );
                tx += 8;
                break;
        }

        msg++;
    }
}

void graphics_draw_sprite( display_context_t disp, int x, int y, sprite_t *sprite )
{
    if( disp == 0 ) { return; }
    if( sprite == 0 ) { return; }

    int sx = 0;
    int sy = 0;
    int ex = sprite->width;
    int ey = sprite->height;

    /* Too far left */
    if( (x + ex) <= 0 ) { return; }

    /* Too far up */
    if( (y + ey) <= 0 ) { return; }

    /* Too far right */
    if( x >= (int)__width ) { return; }

    /* Too far down */
    if( y >= (int)__height ) { return; }

    /* Clipping left */
    if( x < 0 )
    {
        sx = x * -1;
    }

    /* Clipping top */
    if( y < 0 )
    {
        sy = y * -1;
    }

    /* Clipping right */
    if( (x + ex) >= (int)__width )
    {
        ex = __width - x;
    }

    /* Clipping bottom */
    if( (y + ey) >= __height )
    {
        ey = __height - y;
    }

    /* Only display sprite if it matches the bitdepth */
    if( __bitdepth == 2 && sprite->bitdepth == 2 )
    {
        uint16_t *buffer = (uint16_t *)__get_buffer( disp );
        uint16_t *sp_data = (uint16_t *)sprite->data;

        for( int yp = sy; yp < ey; yp++ )
        {
            const register int run = yp * sprite->width;

            for( int xp = sx; xp < ex; xp++ )
            {
                __set_pixel( buffer, x + xp, y + yp, sp_data[xp + run] );
            }
        }
    }
    else if( __bitdepth == 4 && sprite->bitdepth == 4 )
    {
        uint32_t *buffer = (uint32_t *)__get_buffer( disp );
        uint32_t *sp_data = (uint32_t *)sprite->data;

        for( int yp = sy; yp < ey; yp++ )
        {
            const register int run = yp * sprite->width;

            for( int xp = sx; xp < ex; xp++ )
            {
                __set_pixel( buffer, x + xp, y + yp, sp_data[xp + run] );
            }
        }
    }
}

void graphics_draw_sprite_trans( display_context_t disp, int x, int y, sprite_t *sprite )
{
    if( disp == 0 ) { return; }
    if( sprite == 0 ) { return; }

    int sx = 0;
    int sy = 0;
    int ex = sprite->width;
    int ey = sprite->height;

    /* Too far left */
    if( (x + ex) <= 0 ) { return; }

    /* Too far up */
    if( (y + ey) <= 0 ) { return; }

    /* Too far right */
    if( x >= (int)__width ) { return; }

    /* Too far down */
    if( y >= (int)__height ) { return; }

    /* Clipping left */
    if( x < 0 )
    {
        sx = x * -1;
    }

    /* Clipping top */
    if( y < 0 )
    {
        sy = y * -1;
    }

    /* Clipping right */
    if( (x + ex) >= (int)__width )
    {
        ex = __width - x;
    }

    /* Clipping bottom */
    if( (y + ey) >= __height )
    {
        ey = __height - y;
    }

    /* Only display sprite if it matches the bitdepth */
    if( __bitdepth == 2 && sprite->bitdepth == 2 )
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
                    __set_pixel( buffer, x + xp, y + yp, sp_data[xp + run] );
                }
            }
        }
    }
    else if( __bitdepth == 4 && sprite->bitdepth == 4 )
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
                uint32_t cur_color = __get_pixel( buffer, x + xp, y + yp );

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

                __set_pixel( buffer, x + xp, y + yp, mixed_color );
            }
        }
    }
}
