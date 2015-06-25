#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <png.h>
#include <sys/types.h>
#include <sys/param.h>

#define BITDEPTH_16BPP      16
#define BITDEPTH_32BPP      32

#define FORMAT_UNCOMPRESSED 0

#if BYTE_ORDER == BIG_ENDIAN
#define SWAP_WORD(x) (x)
#else
#define SWAP_WORD(x) ((((x)>>8) & 0x00FF) | (((x)<<8) & 0xFF00))
#endif

void write_value( uint8_t *colorbuf, FILE *fp, int bitdepth )
{
    if( bitdepth == BITDEPTH_16BPP )
    {
        uint16_t out = SWAP_WORD((((colorbuf[0] >> 3) & 0x1F) << 11) | (((colorbuf[1] >> 3) & 0x1F) << 6) |
                       (((colorbuf[2] >> 3) & 0x1F) << 1) | (colorbuf[3] >> 7));

        fwrite( &out, 1, 2, fp );
    }
    else
    {
        /* Just write out */
        fwrite( colorbuf, 1, 4, fp );
    }
}

int read_png( char *png_file, char *spr_file, int depth, int hslices, int vslices )
{
    png_structp png_ptr;
    png_infop info_ptr;
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type;
    uint8_t wval8;
    uint16_t wval16;
    FILE *fp;
    FILE *op;
    int err = 0;

    /* Open file descriptors for read and write */
    if ((fp = fopen(png_file, "rb")) == NULL)
    {
        return -ENOENT;
    }

    if ((op = fopen(spr_file, "wb")) == NULL)
    {
        fclose(fp);

        return -ENOENT;
    }

    /* Allocate/initialize the memory for the PNG library. */
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (png_ptr == NULL)
    {
        fclose(fp);
        fclose(op);

        err = -ENOMEM;
        goto exitfiles;
    }

    /* Allocate/initialize the memory for image information. */
    info_ptr = png_create_info_struct( png_ptr );
    if (info_ptr == NULL)
    {
        err = -ENOMEM;
        goto exitpng;
    }

    /* Error handler to gracefully exit */
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        /* Free all of the memory associated with the png_ptr and info_ptr */
        err = -EINTR;
        goto exitpng;
    }

    /* Tie input to file opened earlier */
    png_init_io(png_ptr, fp);

    /* Read PNG header to populate below entries */
    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);

    /* Write sprite header widht and height */
    wval16 = SWAP_WORD((uint16_t)width);
    fwrite( &wval16, sizeof( wval16 ), 1, op );
    wval16 = SWAP_WORD((uint16_t)height);
    fwrite( &wval16, sizeof( wval16 ), 1, op );

    /* Bitdepth */
    wval8 = (depth == BITDEPTH_32BPP) ? 4 : 2;
    fwrite( &wval8, sizeof( wval8 ), 1, op );

    /* Format */
    wval8 = FORMAT_UNCOMPRESSED;
    fwrite( &wval8, sizeof( wval8 ), 1, op );

    /* Horizontal and vertical slices */
    wval8 = hslices;
    fwrite( &wval8, sizeof( wval8 ), 1, op );
    wval8 = vslices;
    fwrite( &wval8, sizeof( wval8 ), 1, op );

    /* Change pallete to RGB */
    if(color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

    /* Change bit-packed grayscale images to 8bit */
    if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    /* Go from 16 to 8 bits per channel */
    if(bit_depth == 16)
        png_set_strip_16(png_ptr);

    /* Change transparency to alpha value */
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

    /* Convert single channel grayscale to RGB */
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);
    /* Ensure interlacing works and then update the color info since we changed things */
    png_set_interlace_handling(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    /* Update the color type from the above re-read */
    color_type = png_get_color_type(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    /* Keep the variably sized array scoped so we can goto past it */
    {
        /* The easiest way to read the image (all at once) */
        png_bytep row_pointers[height];
        memset( row_pointers, 0, sizeof( png_bytep ) * height );

        for( int row = 0; row < height; row++ )
        {
            row_pointers[row] = malloc(png_get_rowbytes(png_ptr, info_ptr));

            if( row_pointers[row] == NULL )
            {
                fprintf(stderr, "Unable to allocate space for row pointers!\n");

                err = -ENOMEM;
                goto exitmem;
            }
        }

        /* Now it's time to read the image. */
        png_read_image(png_ptr, row_pointers);

        /* Translate out to sprite format */
        switch( color_type )
        {
            case PNG_COLOR_TYPE_RGB:
                /* No alpha channel, must set to default full opaque */
                fprintf(stderr, "No alpha channel, substituting full opaque!\n");

                for( int j = 0; j < height; j++)
                {
                    for( int i = 0; i < width; i++ )
                    {
                        uint8_t buf[4];

                        buf[0] = row_pointers[j][(i * 3)];
                        buf[1] = row_pointers[j][(i * 3) + 1];
                        buf[2] = row_pointers[j][(i * 3) + 2];
                        buf[3] = 255;

                        write_value( buf, op, depth );
                    }
                }

                break;
            case PNG_COLOR_TYPE_RGB_ALPHA:
                /* Easy, just dump rows or convert */
                for( int row = 0; row < height; row++ )
                {
                    for( int col = 0; col < width; col++ )
                    {
                        write_value( &row_pointers[row][col * 4], op, depth );
                    }
                }

                break;
        }

exitmem:
        /* Free the row pointers memory */
        for( int row = 0; row < height; row++ )
        {
            if( row_pointers[row] )
            {
                free( row_pointers[row] );
                row_pointers[row] = 0;
            }
        }
    }

exitpng:
    /* Clean up after the read, and free any memory allocated */
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

exitfiles:
    /* Close the files */
    fclose(fp);
    fclose(op);

    return err;
}

void print_args( char * name )
{
    fprintf( stderr, "Usage: %s <bit depth> [<horizontal slices> <vertical slices>] <input png> <output file>\n", name );
    fprintf( stderr, "\t<bit depth> should be 16 or 32.\n" );
    fprintf( stderr, "\t<horizontal slices> should be a number two or greater signifying how many images are in this spritemap horizontally.\n" );
    fprintf( stderr, "\t<vertical slices> should be a number two or greater signifying how many images are in this spritemap vertically.\n" );
    fprintf( stderr, "\t<input png> should be any valid PNG file.\n" );
    fprintf( stderr, "\t<output file> will be written in binary for inclusion using DragonFS.\n" );
}

int main( int argc, char *argv[] )
{
    int bitdepth;

    if( argc != 4 && argc != 6 )
    {
        print_args( argv[0] );
        return -EINVAL;
    }

    /* Covert bitdepth argument */
    bitdepth = atoi( argv[1] );

    if( bitdepth == 32 )
    {
        bitdepth = BITDEPTH_32BPP;
    }
    else if( bitdepth == 16 )
    {
        bitdepth = BITDEPTH_16BPP;
    }
    else
    {
        print_args( argv[0] );
        return -EINVAL;
    }

    if( argc == 4 )
    {
        /* Translate, return result */
        return read_png( argv[2], argv[3], bitdepth, 1, 1 );
    }
    else
    {
        int hslices = atoi( argv[2] );
        int vslices = atoi( argv[3] );

        /* Translate, return result */
        return read_png( argv[4], argv[5], bitdepth, hslices, vslices );
    }
}
