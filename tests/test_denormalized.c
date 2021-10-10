#include <float.h>

void __initalize_cop1();

void test_denormalized(TestContext *ctx) {
    /* Create a volatile float, so gcc does not optimize it out */
    volatile float x = 1.0f;
    x /= FLT_MAX;

    ASSERT(x == 0.0f, "Denormalized float was not flushed to zero");
}
