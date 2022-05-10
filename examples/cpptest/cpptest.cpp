#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

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
};

// Test global constructor
TestClass globalClass;

int main(void)
{
    TestClass* localClass = new TestClass();

    console_init();
    console_set_render_mode(RENDER_MANUAL);

    while(1)
    {
        console_clear();
        printf("Global class method: %d\n", globalClass.f1());
        printf("Local class method: %d\n", localClass->f1());
        console_render();
    }

    delete localClass;
}
