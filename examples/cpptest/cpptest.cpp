#include <cstdio>
#include <cstdint>
#include <libdragon.h>
#include <memory>

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
    auto localClass = std::make_unique<TestClass>();

    console_init();
    console_set_render_mode(RENDER_MANUAL);

    while(1)
    {
        console_clear();
        printf("Global class method: %d\n", globalClass.f1());
        printf("Local class method: %d\n", localClass->f1());
        console_render();
    }

}
