#include <float.h>

// Avoid converting the division into a multiplication, as that would break
// the test causing a "not implemented" exception instead of an underflow.
__attribute__((optimize("no-reciprocal-math"), noinline))
void test_cop1_denormalized_float(TestContext *ctx) {
    uint32_t fcr31 = C1_FCR31();
    DEFER(C1_WRITE_FCR31(fcr31));

    /* Turn off undeflow exception (if enabled) */
    C1_WRITE_FCR31(fcr31 & ~C1_ENABLE_UNDERFLOW);

    /* Create a volatile float, so gcc does not optimize it out */
    volatile float x = 1.0f;

    /* Perform an operation that should generate a denormalized number */
    x /= FLT_MAX;

    /* Verify that the float was "flushed" to zero and that a 
       "not implemented" exception was not raised */
    ASSERT(x == 0.0f, "Denormalized float was not flushed to zero");
}

void test_cop1_interrupts(TestContext *ctx) {
   // Test that we can use FPUs in the context of an interrupt handler.
   // This is useful because in general interrupt handlers save FPU registers
   // only "on demand" when needed.
   timer_init();
   DEFER(timer_close());

   volatile float float_value = 1234.0f;
   void cb1(int ovlf) {
      disable_interrupts();
      float_value *= 2;
      enable_interrupts();
   }

   void cb2(int ovlf) {
      float_value *= 2;
   }

   timer_link_t *tt1 = new_timer(TICKS_FROM_MS(2), TF_ONE_SHOT, cb1);
   DEFER(delete_timer(tt1));
   timer_link_t *tt2 = new_timer(TICKS_FROM_MS(2), TF_ONE_SHOT, cb2);
   DEFER(delete_timer(tt2));

   wait_ms(3);

   ASSERT_EQUAL_SIGNED((int)float_value, 4936, "invalid floating point value");
}
