// Writes the 32bit value to COP0 register 9 (count)
#define WRITE_TICKS(val) do { asm volatile("mtc0 %0,$9"::"r"(val)); } while(0)

// Writes the 32bit value to COP0 register 11 (compare)
#define WRITE_COMPARE(val) do { asm volatile("mtc0 %0,$11" ::"r"(val)); } while(0)

#define ARM_INTERRUPT(compare) do { \
	asm volatile(".align 4"); \
	WRITE_TICKS(0x0); \
	WRITE_COMPARE(compare); \
} while(0)

#define TEST(name, tests) do { \
	const int NUM_CASES = sizeof(tests) / sizeof(tests[0]); \
	for (int i=0; i < NUM_CASES; i++) { \
		mock_tick = tests[i][1]; \
		WRITE_TICKS(tests[i][0]); \
		/* act */ \
		name(tests[i][2]); \
		READ_TICKS(ticks_0); \
		ASSERT(state != Timeout, "Case: %u Test timed out. " #name " didn't finish on time (Ticks: %#10lX)", state, ticks_0); \
		ASSERT(state == Test, "Case: %u " #name " finished too early (Ticks: %#10lX)", state, ticks_0); \
		ASSERT(state <= Timeout, "Case: %u Unexpected state for " #name " (Ticks: %#10lX)", state, ticks_0); \
		/* re-sync to frame */ \
		while (state < Timeout); \
		/* reset state */ \
		state = Start; \
	} \
} while(0)

static volatile uint32_t mock_tick = 0;
static volatile enum state_t{ Start = 0, Test = 1, Timeout = 2 } state = -1;

static void frame_callback() {
	state++;

	switch(state) {
		case Test:
			WRITE_TICKS(mock_tick);
		break;
		default:
		break;
	}
}

// Array of test cases in the form {initial, mock, wait}
// We have three phases incremented on every frame (VI used as an interval)

// Start: set the count register to the initial value and enter the wait loop
// Test: VI interrupt updates the count register to the mock value
// Timeout: If the wait loop fails to exit upon setting the mock value, the VI

// interrupt will move the state into this phase, which is asserted in the test.
// So we can test them each of them in three frames, without waiting the full
// delay. Tested wait should be an order of magnitude longer than the VI interval
// to prevent the loop from exiting before we are able to update the count reg.
// If wait calculation is unable to wrap properly, the loop will run for a maximum
// of 90 secs and endup in Timeout state, failing the tes.
// Execution times are negligible with this setup.
// 0x2CB4178 is 1 second

static uint32_t test_cases[][3] = {
	// No overflow
	{0x2CB4178, 0x59682F0, 0x2CB4178},
	// Wrap towards the end
	{0xFD34BE87, 0x2CB4178, 0x59682F0},
	// wait a long time - 1 second in the range to 1 second before the end
	{0x2CB4178, 0xFD34BE87, 0xFA697D0F},
	 // already fulfilled way past the start tick
	{0x2CB4178, 0xFD34BE87, 0x59682F0},
	// TODO: also test unfulfilled cases to make sure we don't exit the loop early.
};

static uint32_t test_cases_ms[][3] = {
	// No overflow
	{0x2CB4178, 0x59682F0, 1000},
	// Wrap towards the end
	{0xFD34BE87, 0x2CB4178, 2000},
	// wait a long time - 1 second in the range to 1 second before the end
	{0x2CB4178, 0xFD34BE87, 88000},
	 // already fulfilled way past the start tick
	{0x2CB4178, 0xFD34BE87, 2000},
	// TODO: also test unfulfilled cases to make sure we don't exit the loop early.
};

void test_ticks(TestContext *ctx) {
	uint32_t ticks_0;
	uint32_t ticks_1;

	uint32_t continue_ticks;
	READ_TICKS(continue_ticks);

	disable_interrupts();

	// Put the instructions on the same cacheline
	asm volatile(".align 4");
	WRITE_TICKS(0x0);
	READ_TICKS(ticks_0);
	WRITE_TICKS(0xFFFFFFFF);
	READ_TICKS(ticks_1);

	enable_interrupts();

	ASSERT(ticks_0 == 0x0 && ticks_1 == 0xFFFFFFFF, "not reading correct register. Received %#010lX and %#010lX", ticks_0, ticks_1);

	disable_interrupts();

	// Put the instructions on the same cacheline
	asm volatile(".align 4");
	WRITE_TICKS(0x0);
	ticks_0 = get_ticks();
	WRITE_TICKS(0xFFFFFFFF);
	ticks_1 = get_ticks();

	enable_interrupts();

	ASSERT(ticks_0 == 0x0 && ticks_1 == 0xFFFFFFFF, "not reading correct register or function not inlined. Received %#010lX and %#010lX", ticks_0, ticks_1);

	disable_interrupts();

	// Put the instructions on the same cacheline
	asm volatile(".align 4");
	WRITE_TICKS(0x0);
	ticks_0 = get_ticks_ms();
	WRITE_TICKS(0x7FFFFFFF);
	ticks_1 = get_ticks_ms();

	// Prepare for next test
	register_VI_handler(frame_callback);
	enable_interrupts();

	ASSERT(ticks_0 == 0 && ticks_1 == 45812, "not reading correct register or function not inlined. Received %lu and %lu", ticks_0, ticks_1);

	// Sync to nearest video frame to use as an interval
	while (state < Start);

	TEST(WAIT_TICKS, test_cases);
	TEST(wait_ticks, test_cases);
	TEST(wait_ms, test_cases_ms);

	// Cleanup
	unregister_VI_handler(frame_callback);
	WRITE_TICKS(continue_ticks);
}

#undef WRITE_TICKS
#undef WRITE_COMPARE
#undef ARM_INTERRUPT
#undef TEST