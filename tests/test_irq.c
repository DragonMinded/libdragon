
void test_irq_reentrancy(TestContext *ctx) {
	// Make sure that enable_interrupts() does not cause an interrupt
	// to trigger if called under interrupt.
	volatile bool cb1_called = false;
	volatile bool cb2_called = false;
	bool cb1_running = false;
	bool fail_reentrant = false;
	bool fail_order = false;

	void cb1(int ovlf) {
		cb1_called = true;
		cb1_running = true;
		disable_interrupts();

		// Wait for the other timer interrupt to trigger
		wait_ms(3);

		// This enable_interrupts should not trigger the other
		// interrupt, otherwise the test will fail.
		enable_interrupts();
		cb1_running = false;
	}

	void cb2(int ovlf) {
		cb2_called = true;
		if (!cb1_called)
			fail_order = true;
		if (cb1_running)
			fail_reentrant = true;
	}

	timer_init();
	DEFER(timer_close());

	timer_link_t *t1 = new_timer(TICKS_FROM_MS(2), TF_ONE_SHOT, cb1);
	DEFER(delete_timer(t1));
	timer_link_t *t2 = new_timer(TICKS_FROM_MS(4), TF_ONE_SHOT, cb2);
	DEFER(delete_timer(t2));

	while (!cb2_called) {}

	ASSERT(!fail_order, "invalid order of call of callbacks");
	ASSERT(!fail_reentrant, "interrupt called while another interrupt was in progress");
}
