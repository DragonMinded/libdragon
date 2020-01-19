#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

static resolution_t res = RESOLUTION_320x240;
static bitdepth_t bit = DEPTH_32_BPP;

class TestClass
{
    private:
        int d;

    public:
        TestClass() {
            d = 2;
        }
        int f1()
        {
            return d;
        }
};

// Test global constructor
TestClass o1;

int main(void)
{
    int* a = new int(2);

    init_interrupts();

    console_init();

    console_set_render_mode(RENDER_MANUAL);

    display_init( res, bit, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );

    while(1)
    {
        console_clear();
        printf("Libdragon v%d.%d.%d \n", libdragon_version.major, libdragon_version.minor, libdragon_version.revision);
        printf("Test: %d\n", o1.f1());
        console_render();
    }

    delete a;
}
