static volatile uint32_t test_ticks_mock = 0;
static volatile enum test_ticks_state_t { Start = 0, Test = 1, Timeout = 2 } state = -1;

static void frame_callback() {
	state++;

	state = state > Timeout ? Timeout : state;

	switch(state) {
		case Test:
			C0_WRITE_COUNT(test_ticks_mock);
		break;
		default:
		break;
	}
}

// Array of test cases in the form {initial, mock, wait}
// We have three phases incremented on every frame (VI used as an interval)

// Start: set the count register to the initial value and enter the wait loop
// Test: VI interrupt updates the count register to the mock value - 5ms
// Timeout: If the wait loop fails to exit 5ms after setting the mock, the VI
// interrupt will move the state into this phase, which is asserted in the test.
// So we can test each of them in three frames, without waiting the full delay.
// If the wait does end early, state will be left on "Start" or current tick will
// be closer to the original mock value instead of being 5ms after it.
// Tested wait should be an order of magnitude longer than the VI interval
// to prevent the loop from exiting before we are able to update the count reg.
// If wait calculation is incorrect, the loop will run for a maximum of ~91 secs
// and end up in "Timeout" state, or exit early on "Start" state failing the
// test. Execution times are negligible with this setup.
// 0x2CB4178 is 1 second

typedef uint32_t test_ticks_cases_t[][3];

static test_ticks_cases_t test_ticks_cases = {
	// No overflow
	{0x2CB4178, 0x59682F0, 0x2CB4178},
	// Wrap towards the end
	{0xFD34BE87, 0x2CB4178, 0x59682F0},
	// Wait a long time - 1 second into the range => 1 second before the end
	{0x2CB4178, 0xFD34BE87, 0xFA697D0F},
	// Long wait, wrapping around - 1 second before the end => 2 second before the end
	{0xFD34BE87, 0xFA697D0F, 0xFD34BE87},
};

static test_ticks_cases_t test_ticks_ms_cases = {
	// No overflow
	{0x2CB4178, 0x59682F0, 1000},
	// Wrap towards the end
	{0xFD34BE87, 0x2CB4178, 2000},
	// Wait a long time - 1 second into the range => 1 second before the end
	{0x2CB4178, 0xFD34BE87, 89626},
	// Long wait, wrapping around - 1 second before the end => 2 second before the end
	{0xFD34BE87, 0xFA697D0F, 90626},
};

static void test_ticks_func(TestContext *ctx, void to_test(unsigned long), char name[], test_ticks_cases_t tests, int num_cases) {
	for (int i=0; i < num_cases; i++) {
		/* move ticks a little before the target to make sure it is actually doing some waiting */
		test_ticks_mock = tests[i][1] - TICKS_FROM_MS(5);
		C0_WRITE_COUNT(tests[i][0]);
		/* act */
		(*to_test)(tests[i][2]);
		uint32_t ticks_0 = TICKS_READ();
		ASSERT(state <= Timeout, "Case: %u Unexpected state for %s (Ticks: %#10lX)\n", i,name, ticks_0);
		ASSERT(state != Timeout, "Case: %u Test timed out. %s didn't finish on time (Ticks: %#10lX)\n", i, name, ticks_0);
		ASSERT(state == Test && ticks_0 >= tests[i][1], "Case: %u %s finished too early (Ticks: %#10lX)\n", i, name, ticks_0);
		
		/* re-sync to frame */
		while (state < Timeout);
		/* reset state */
		state = Start;
	}
}

void test_ticks(TestContext *ctx) {
	uint32_t ticks_0;
	uint32_t ticks_1;

	uint32_t continue_ticks = TICKS_READ();
	DEFER(C0_WRITE_COUNT(continue_ticks));

	disable_interrupts();

	// Put the instructions on the same cacheline
	for (int i = 0; i < 2; i++) {
		C0_WRITE_COUNT(0x0);
		ticks_0 = TICKS_READ();
		C0_WRITE_COUNT(0xFFFFFFFF);
		ticks_1 = TICKS_READ();
	}

	enable_interrupts();

	ASSERT(ticks_0 == 0x0 && ticks_1 == 0xFFFFFFFF, "not reading correct register or it was not inlined. Received %#010lX and %#010lX", ticks_0, ticks_1);

	disable_interrupts();

	// Make sure they are all in I-cache o/w ticks may increment irrespective of actually run instructions
	for (int i = 0; i < 2; i++) {
		C0_WRITE_COUNT(0x0);
		ticks_0 = get_ticks();
		C0_WRITE_COUNT(0xFFFFFFFF);
		ticks_1 = get_ticks();
	}

	enable_interrupts();

	ASSERT(ticks_0 == 0x0 && ticks_1 == 0xFFFFFFFF, "not reading correct register or function not inlined. Received %#010lX and %#010lX", ticks_0, ticks_1);

	disable_interrupts();

	// Make sure they are all in I-cache o/w ticks may increment irrespective of actually run instructions
	for (int i = 0; i < 2; i++) {
		C0_WRITE_COUNT(0x0);
		ticks_0 = get_ticks_ms();
		C0_WRITE_COUNT(0x7FFFFFFF);
		ticks_1 = get_ticks_ms();
	}

	// Prepare for next test
	register_VI_handler(frame_callback);
	DEFER(unregister_VI_handler(frame_callback));
	enable_interrupts();

	ASSERT(ticks_0 == 0 && ticks_1 == (!sys_bbplayer() ? 45812 : 30542), "not reading correct register or function not inlined. Received %lu and %lu", ticks_0, ticks_1);

	// Sync to nearest video frame to use as an interval
	while (state < Start);

	test_ticks_func(ctx, wait_ticks, "wait_ticks", test_ticks_cases, sizeof(test_ticks_cases) / sizeof(test_ticks_cases[0]));

	// The wait_ms test contains hardcoded tick values that refer to the conversion
	// between milliseconds and ticks, as computed on a N64. It won't work on
	// iQue, so just skip this part.
	if (!sys_bbplayer())
		test_ticks_func(ctx, wait_ms, "wait_ms", test_ticks_ms_cases, sizeof(test_ticks_ms_cases) / sizeof(test_ticks_ms_cases[0]));
}