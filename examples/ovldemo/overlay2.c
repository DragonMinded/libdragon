#include <libdragon.h>
#include <math.h>

void overlay_print(int time)
{
    //Calculate time in seconds
    float x = time/display_get_fps();
    printf("Overlay 2 Printer\n");
    //Do some non-constant math to test importing functions
    printf("sin(%f)=%f\n", x, sin(x));
}