#include <stdio.h>
#include <stdint.h>

/**
  * To use this file, pipe a sprite of the old format into stdin, and
  * redirect stdout to a second file of your chosing.  The sprite header
  * will be converted.  This tool has no error checking and assumes a valid
  * sprite header.  It is provided merely for convenience.
  */

int main( int argc, char *argv[] )
{
    uint8_t zero = 0;
    uint8_t val;

    /* Read in old width */
    fread( &val, sizeof( val ), 1, stdin );

    /* Write empty value and then new */
    fwrite( &zero, sizeof( zero ), 1, stdout );
    fwrite( &val, sizeof( val ), 1, stdout );
    
    /* Read in old height */
    fread( &val, sizeof( val ), 1, stdin );

    /* Write empty value and then new */
    fwrite( &zero, sizeof( zero ), 1, stdout );
    fwrite( &val, sizeof( val ), 1, stdout );

    /* Straight copy of bitdepth and format */
    fread( &val, sizeof( val ), 1, stdin );
    fwrite( &val, sizeof( val ), 1, stdout );

    fread( &val, sizeof( val ), 1, stdin );
    fwrite( &val, sizeof( val ), 1, stdout );

    /* Assuming horizontal and vertical stride of 1 */
    val = 1;
    fwrite( &val, sizeof( val ), 1, stdout );
    fwrite( &val, sizeof( val ), 1, stdout );

    /* Now just byte copy until end of stream */
    while( !feof( stdin ) )
    {
        fread( &val, sizeof( val ), 1, stdin );
        
        if( !feof( stdin ) )
        {
            /* Only copy out if the last read didn't make an eof */
            fwrite( &val, sizeof( val ), 1, stdout );
        }
    }

    return 0;
}
