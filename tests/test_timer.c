
void test_timer_oneshot(TestContext *ctx) {
	timer_init();
	DEFER(timer_close());

	uint32_t tick0 = timer_ticks();

	volatile int cb1_called = 0;
	void cb1(int ovlf) {
		cb1_called++;
	}

	timer_link_t *tt1 = new_timer(TICKS_FROM_MS(2), TF_ONE_SHOT, cb1);
	DEFER(delete_timer(tt1));

	wait_ms(2);
	ASSERT_EQUAL_SIGNED(cb1_called, 1, "timer 1 not called");
	wait_ms(3);
	ASSERT_EQUAL_SIGNED(cb1_called, 1, "timer 1 called again?");
	stop_timer(tt1);
	wait_ms(3);
	ASSERT_EQUAL_SIGNED(cb1_called, 1, "timer 1 called again?");

	// Restart the timer. This time, wait with interrupts disabled, so that
	// we check the the timer triggers as soon as we re-enable them.
	start_timer(tt1, TICKS_FROM_MS(3), TF_ONE_SHOT, cb1);
	wait_ms(2);
	ASSERT_EQUAL_SIGNED(cb1_called, 1, "timer 1 called again?");
	disable_interrupts();
	wait_ms(3);
	enable_interrupts();
	ASSERT_EQUAL_SIGNED(cb1_called, 2, "timer 1 not called");

	// Check that timer_ticks return approximate correct value across all timer executions
	tick0 = timer_ticks() - tick0;
	ASSERT_EQUAL_SIGNED(
		TIMER_MICROS(tick0) / 1000,
		2+3+3+2+3,
		"invalid timer_ticks");
}

void test_timer_continuous(TestContext *ctx) {
	timer_init();
	DEFER(timer_close());

	volatile int cb2_called = 0;
	void cb2(int ovlf) {
		cb2_called++;
	}

	timer_link_t *t2 = new_timer(TICKS_FROM_MS(2), TF_CONTINUOUS, cb2);
	DEFER(delete_timer(t2));

	uint64_t tick0 = timer_ticks();

	wait_ms(7);
	ASSERT_EQUAL_SIGNED(cb2_called, 3, "timer 2 not called");
	stop_timer(t2);

	wait_ms(3);
	ASSERT_EQUAL_SIGNED(cb2_called, 3, "timer 2 called again?");

	// try switching from continuous to one shot
	start_timer(t2, TICKS_FROM_MS(2), TF_ONE_SHOT, cb2);
	wait_ms(5);
	ASSERT_EQUAL_SIGNED(cb2_called, 4, "timer 2 not called");

	// Check that timer_ticks return approximate correct value across all timer executions
	tick0 = timer_ticks() - tick0;
	ASSERT_EQUAL_SIGNED(
		TIMER_MICROS(tick0) / 1000, 
		7+3+5,
		"invalid timer_ticks");
}

void test_timer_mixed(TestContext *ctx) {
	timer_init();
	DEFER(timer_close());

	volatile uint8_t called_list[256] = {0};
	volatile int called_idx = 0;

	void cb1(int ovlf) { called_list[called_idx++] = 1; }
	void cb2(int ovlf) { called_list[called_idx++] = 2; }
	void cb3(int ovlf) { called_list[called_idx++] = 3; }

	timer_link_t *t2 = new_timer(TICKS_FROM_MS(2), TF_CONTINUOUS, cb2);
	DEFER(delete_timer(t2));

	timer_link_t *t3 = new_timer(TICKS_FROM_MS(7), TF_CONTINUOUS, cb3);
	DEFER(delete_timer(t3));

	timer_link_t *t1 = new_timer(TICKS_FROM_MS(11), TF_ONE_SHOT, cb1);
	DEFER(delete_timer(t1));

	uint64_t tick0 = timer_ticks();

	wait_ms(12);
	stop_timer(t2);

	wait_ms(20);
	stop_timer(t3);

	uint8_t expected[] = { 2, 2, 2, 3, 2, 2, 1, 2, 3, 3, 3, 0 };
	ASSERT_EQUAL_MEM((uint8_t*)called_list, expected, sizeof(expected), "invalid order of timer callbacks");

	// Check that timer_ticks return approximate correct value across all timer executions
	tick0 = timer_ticks() - tick0;
	ASSERT_EQUAL_SIGNED(
		TIMER_MICROS(tick0) / 1000, 
		12+20,
		"invalid timer_ticks");
}

void test_timer_slow_callback(TestContext *ctx) {
	timer_init();
	DEFER(timer_close());

	// Check that if a callback is too slow, it doesn't prevent other
	// timers from running.
	volatile int called_slow = 0;
	volatile int called_fast = 0;
	void slow(int ovlf) { 
		wait_ms(10);
		called_slow++;
	}

	timer_link_t *t4;
	void fast(int ovlf) {
		called_fast++;
	}

	timer_link_t *t1 = new_timer(TICKS_FROM_MS(4), TF_ONE_SHOT, slow);
	DEFER(delete_timer(t1));
	timer_link_t *t2 = new_timer(TICKS_FROM_MS(2), TF_ONE_SHOT, slow);
	DEFER(delete_timer(t2));
	timer_link_t *t3 = new_timer(TICKS_FROM_MS(5), TF_ONE_SHOT, slow);
	DEFER(delete_timer(t3));
	t4 = new_timer(TICKS_FROM_MS(2), TF_CONTINUOUS, fast);
	DEFER(delete_timer(t4));

	uint64_t tick0 = timer_ticks();
	wait_ms(10);
	uint64_t tick1 = timer_ticks();

	ASSERT_EQUAL_SIGNED(called_slow, 3, "slow timers not called");

	// The total execution time is 30ms (3 slow timers) + 2ms (time before
	// the first slow timer fires). The fast timer is run every 2ms in this
	// interval.
	ASSERT_EQUAL_SIGNED(called_fast, (30+2)/2, "fast timers not called");

	ASSERT_EQUAL_SIGNED(
		TIMER_MICROS(tick1-tick0) / 1000, 
		30+2,
		"invalid timer_ticks");
}

// Change the hardware count register.
static uint32_t write_count(uint32_t x) {
	uint32_t old = TICKS_READ();
	C0_WRITE_COUNT(x);
	return old;
}

void test_timer_ticks(TestContext *ctx) {
	timer_init();
	DEFER(timer_close());

	// We want to fuzz different conditions around the overflow, that is
	// when the hardware counter goes to zero. That's where all problems lie
	// in the implementation. Try first to be exhaustive. Also, check with
	// both enabled and disabled interrupts because the codepaths are different.
	for (int j=0; j<2; j++) {
		for (int i=0; i<512; i++) {
			uint32_t start = (uint32_t)0 - i;
			
			uint32_t old = write_count(start);
			if (j==1) disable_interrupts();
			uint64_t t0 = timer_ticks();
			uint64_t t1 = timer_ticks();
			uint64_t t2 = timer_ticks();
			uint64_t t3 = timer_ticks();
			uint64_t t4 = timer_ticks();
			uint64_t t5 = timer_ticks();
			if (j==1) enable_interrupts();

			write_count(old); // restore counter to not mess up with global time accounting

			ASSERT(t0 < t1, "invalid timer_ticks [start=%lx]: %llx < %llx", start, t0, t1);
			ASSERT(t1 < t2, "invalid timer_ticks [start=%lx]: %llx < %llx", start, t1, t2);
			ASSERT(t2 < t3, "invalid timer_ticks [start=%lx]: %llx < %llx", start, t2, t3);
			ASSERT(t3 < t4, "invalid timer_ticks [start=%lx]: %llx < %llx", start, t3, t4);
			ASSERT(t4 < t5, "invalid timer_ticks [start=%lx,%d]: %llx < %llx < %llx < %llx < %llx < %llx", start, j, t0, t1, t2, t3, t4, t5);
			ASSERT(t5-t0 < 1000, "invalid timer_ticks [start=%lx]: %llx - %llx", start, t0, t5);
		}
	}

	// Now do the same testing as above, with full fuzzying. In the fuzzying,
	// introduce also one shot timers expiring near or at overflow, to further
	// stress any kind of condition
	bool cbcalled = false;
	void tcb(int ovfl) { cbcalled = true; }

	timer_link_t *tt1 = new_timer(0, TF_ONE_SHOT, tcb);
	stop_timer(tt1);
	DEFER(delete_timer(tt1));

	for (int i=0; i<4096; i++) {
		uint32_t start = (uint32_t)0 - RANDN(128);
		bool with_irq = RANDN(2) != 0;
		bool use_timer = RANDN(2) != 0;

		cbcalled = false;
		if (use_timer) start_timer(tt1, start-TICKS_READ()+RANDN(64), TF_ONE_SHOT, tcb);

		uint32_t old = write_count(start);
		if (!with_irq) disable_interrupts();
		uint64_t t0 = timer_ticks();
		uint64_t t1 = timer_ticks();
		uint64_t t2 = timer_ticks();
		uint64_t t3 = timer_ticks();
		uint64_t t4 = timer_ticks();
		while (TICKS_BEFORE(TICKS_READ(), 128)) {}  // wait until tick 128 to make sure the timer triggers (if any)
		uint64_t t5 = timer_ticks();
		if (!with_irq) enable_interrupts();
		write_count(old); // restore counter to not mess up with global time accounting

		if (use_timer) stop_timer(tt1);

		// Check that all ticks are monotonically increasing, that there are
		// no large jumps (eg: high part incremented twice), and that the
		// timer callback was called (if it was meant to).
		ASSERT(t0 < t1 && t1 < t2 && t2 < t3 && t3 < t4 && t4 < t5 && (t5-t0)<1000 && cbcalled == use_timer, 
			"invalid timer_ticks %d:[start=%lx,irq=%d,tt1=%d:%lx/%d]\n%llx < %llx < %llx < %llx < %llx < %llx",
			i, start, with_irq, use_timer, tt1->left, cbcalled, t0, t1, t2, t3, t4, t5);
	}
}

void test_timer_disabled_restart(TestContext *ctx) {
	timer_init();
	DEFER(timer_close());

	uint32_t tick0 = timer_ticks();

	volatile int cb1_called = 0;
	void cb1(int ovlf) {
		cb1_called++;
	}

	timer_link_t *tt1 = new_timer(TICKS_FROM_MS(2), TF_ONE_SHOT | TF_DISABLED, cb1);
	DEFER(delete_timer(tt1));

	wait_ms(2);
	ASSERT_EQUAL_SIGNED(cb1_called, 0, "timer 1 called?");
	wait_ms(3);
	ASSERT_EQUAL_SIGNED(cb1_called, 0, "timer 1 called again?");
	wait_ms(3);
	ASSERT_EQUAL_SIGNED(cb1_called, 0, "timer 1 called again?");
	
	// Restart the timer. This time is should trigger
	restart_timer(tt1);
	wait_ms(2);
	ASSERT_EQUAL_SIGNED(cb1_called, 1, "timer 1 not called");
	wait_ms(3);
	ASSERT_EQUAL_SIGNED(cb1_called, 1, "timer 1 called again?");
	wait_ms(3);
	ASSERT_EQUAL_SIGNED(cb1_called, 1, "timer 1 called again?");

	// Check that timer_ticks return approximate correct value across all timer executions
	tick0 = timer_ticks() - tick0;
	ASSERT_EQUAL_SIGNED(
		TIMER_MICROS(tick0) / 1000,
		2+3+3+2+3+3,
		"invalid timer_ticks");
}

void test_timer_disabled_start(TestContext *ctx) {
	timer_init();
	DEFER(timer_close());

	uint32_t tick0 = timer_ticks();

	volatile int cb1_called = 0;
	void cb1(int ovlf) {
		cb1_called++;
	}

	timer_link_t *tt1 = new_timer(TICKS_FROM_MS(2), TF_ONE_SHOT | TF_DISABLED, cb1);
	DEFER(delete_timer(tt1));

	wait_ms(2);
	ASSERT_EQUAL_SIGNED(cb1_called, 0, "timer 1 called?");
	wait_ms(3);
	ASSERT_EQUAL_SIGNED(cb1_called, 0, "timer 1 called again?");
	wait_ms(3);
	ASSERT_EQUAL_SIGNED(cb1_called, 0, "timer 1 called again?");
	
	// Restart the timer. This time is should trigger
	start_timer(tt1, TICKS_FROM_MS(2), TF_ONE_SHOT, cb1);
	wait_ms(2);
	ASSERT_EQUAL_SIGNED(cb1_called, 1, "timer 1 not called");
	wait_ms(3);
	ASSERT_EQUAL_SIGNED(cb1_called, 1, "timer 1 called again?");
	wait_ms(3);
	ASSERT_EQUAL_SIGNED(cb1_called, 1, "timer 1 called again?");

	// Check that timer_ticks return approximate correct value across all timer executions
	tick0 = timer_ticks() - tick0;
	ASSERT_EQUAL_SIGNED(
		TIMER_MICROS(tick0) / 1000,
		2+3+3+2+3+3,
		"invalid timer_ticks");
}
