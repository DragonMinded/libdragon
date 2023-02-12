#include <cstdio>
#include <cstdint>
#include <libdragon.h>
#include <memory>
#include <stdexcept>

int state = 1;

class TestClass
{
    private:
        uint32_t d;

    public:
        TestClass() {
            d = 100;
        }
        int f1()
        {
            d = d + 1;
            return d;
        }
        void exc1()
        {
            if (state)
                throw (int)d;
        }
        int exc()
        {
            try {
                exc1();
            } catch(int x) {
                return x;
            } catch(...) {
                assertf(0, "Exceptions not working");
                return -1;
            }
            return -1;
        }
        void crash(void) {
            throw std::runtime_error("Crash!");
        }
};

// Test global constructor
TestClass globalClass;

int main(void)
{
    debug_init_isviewer();
    debug_init_usblog();
    controller_init();

    auto localClass = std::make_unique<TestClass>();

    console_init();
    console_set_render_mode(RENDER_MANUAL);


    while(1)
    {
        console_clear();
        printf("Global class method: %d\n", globalClass.f1());
        printf("Local class method: %d\n", localClass->f1());
        printf("Exception data: %d\n", localClass->exc());
        printf("\nPress A to crash (test uncaught C++ exceptions)\n");
        console_render();

        controller_scan();
        struct controller_data keys = get_keys_down();
        if (keys.c[0].A)
            localClass->crash();
        
    }
}
