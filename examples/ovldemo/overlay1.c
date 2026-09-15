#include <libdragon.h>
#include <math.h>

void overlay_print(int time)
{
    //Calculate time in seconds from passed in ticks
    float x = time/display_get_fps();
    printf("Overlay 1 Printer\n");
    //Do some non-constant math with log1p and cos
    //log1p is an uncommon math function
    //This verifies that dead stripping works properly
    //even if functons are only referenced from an overlay
    printf("log1p(%f)*cos(%f) = %f\n", x, x, log1p(x)*cos(x));
}