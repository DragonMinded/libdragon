#include <float.h>

void test_cop1_denormalized_float(TestContext *ctx) {
    /* Create a volatile float, so gcc does not optimize it out */
    volatile float x = 1.0f;

    /* Perform an operation that should generate a denormalized number */
    x /= FLT_MAX;

    /* Verify that the float was "flushed" to zero and that a 
       "not implemented" exception was not raised */
    ASSERT(x == 0.0f, "Denormalized float was not flushed to zero");
}
