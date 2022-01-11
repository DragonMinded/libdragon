
void test_kernel_basic(TestContext *ctx) {
	kernel_init();
	DEFER(kernel_close());

	uint8_t thcalled[16] = {0};
	uint8_t thcalled_idx = 0;

	void func_th(void *arg)
	{
		int thid = (int)arg;

		thcalled[thcalled_idx++] = thid;
		kthread_yield();		
		thcalled[thcalled_idx++] = thid;
		kthread_yield();
		thcalled[thcalled_idx++] = thid;
	}

	// Create two threads. Pause their execution by making
	// sure they have lower priority than main thread.
	kthread_set_pri(NULL, 5);
	kthread_new("test1", 2048, 3, func_th, (void*)1);
	kthread_new("test2", 2048, 3, func_th, (void*)2);

	// Now lower the priority of the main thread. This will
	// immediately force a switch to the two threads that
	// have now higher priority.
	kthread_set_pri(NULL, 1);

	// Once we get there, the two threads have already finished
	// execution. Check that they were called in the expected order.
	uint8_t exp[] = { 1,2,1,2,1,2,0 };
	ASSERT_EQUAL_MEM(thcalled, exp, sizeof(exp), "invalid order of threads");
}

void test_kernel_priority(TestContext *ctx) {
	kernel_init();
	DEFER(kernel_close());

	uint8_t thcalled[16] = {0};
	uint8_t thcalled_idx = 0;

	void func_th1(void *arg)
	{
		thcalled[thcalled_idx++] = 1;
		kthread_yield();		
		thcalled[thcalled_idx++] = 1;
		kthread_yield();
		thcalled[thcalled_idx++] = 1;
	}

	void func_th2(void *arg)
	{
		thcalled[thcalled_idx++] = 2;
		kthread_new("test1", 2048, 5, func_th1, 0);
		thcalled[thcalled_idx++] = 2;
	}

	void func_th3(void *arg)
	{
		thcalled[thcalled_idx++] = 3;
		kthread_new("test2", 2048, 6, func_th2, 0);
		thcalled[thcalled_idx++] = 3;
		kthread_yield();
		thcalled[thcalled_idx++] = 3;
	}

	kthread_set_pri(NULL, 1);
	kthread_new("test3", 2048, 5, func_th3, 0);

	uint8_t exp[] = { 3,2,2,3,1,3,1,1,0 };
	ASSERT_EQUAL_MEM(thcalled, exp, sizeof(exp), "invalid order of threads");
}

void test_kernel_sleep(TestContext *ctx) {
	timer_init();
	DEFER(timer_close());

	kernel_init();
	DEFER(kernel_close());

	uint8_t thcalled[16] = {0};
	uint8_t thcalled_idx = 0;
	volatile uint8_t thexit = 0;

	void func_th1(void *arg)
	{
		LOG("func_th1 called\n");
		thcalled[thcalled_idx++] = 1;
		kthread_sleep(TICKS_FROM_MS(5));
		thcalled[thcalled_idx++] = 1;
		kthread_sleep(TICKS_FROM_MS(5));
		thcalled[thcalled_idx++] = 1;
		++thexit;
	}

	void func_th2(void *arg)
	{
		LOG("func_th2 called\n");
		thcalled[thcalled_idx++] = 2;
		kthread_sleep(TICKS_FROM_MS(8));
		thcalled[thcalled_idx++] = 2;
		++thexit;
	}

	kthread_set_pri(NULL, 6);
	kthread_new("test1", 2048, 4, func_th1, 0);
	kthread_new("test2", 2048, 5, func_th2, 0);

	LOG("sleeping\n");
	kthread_set_pri(NULL, 1);
	kthread_sleep(TICKS_FROM_MS(15));

	uint8_t exp[] = { 2,1,1,2,1,0 };
	ASSERT_EQUAL_MEM(thcalled, exp, sizeof(exp), "invalid order of threads");
}

// Test mbox with writer thread having priority higher than reader thread.
void test_kernel_mbox_1(TestContext *ctx) {
	timer_init();
	DEFER(timer_close());

	kernel_init();
	DEFER(kernel_close());

	uint8_t msg1=1, msg2=2, msg3=3, msg4=4, msg5=5, msg6=6, msg7=7, msg8=8;
	kmbox_t *mbox = kmbox_new(4);
	DEFER(kmbox_free(mbox));

	uint8_t msgread[16] = {0};
	uint8_t msgread_idx = 0;

	void read_thread(void* arg)
	{
		uint8_t *msg;
		LOG("read_thread() %d\n", kmbox_empty(mbox));
		while ((msg = kmbox_recv(mbox)))
		{
			LOG("mbox_recv(): %p:%d\n", msg, *msg);
			msgread[msgread_idx++] = *msg;
		}
	}

	// Bump priority of main thread
	kthread_set_pri(NULL, 2);

	// Create reader thread
	kthread_t *th1 = kthread_new("read_thread", 2048, 1, read_thread, NULL);
	DEFER(kthread_kill(th1));

	// Enqueue 4 messages
	ASSERT(kmbox_empty(mbox), "mbox not empty?");
	ASSERT(!kmbox_full(mbox), "mbox full?");
	ASSERT(kmbox_try_send(mbox, &msg1), "mbox send failure");

	ASSERT(!kmbox_empty(mbox), "mbox empty?");
	ASSERT(!kmbox_full(mbox), "mbox full?");
	ASSERT(kmbox_try_send(mbox, &msg2), "mbox send failure");

	ASSERT(!kmbox_empty(mbox), "mbox empty?");
	ASSERT(!kmbox_full(mbox), "mbox full?");
	ASSERT(kmbox_try_send(mbox, &msg3), "mbox send failure");

	ASSERT(!kmbox_empty(mbox), "mbox empty?");
	ASSERT(!kmbox_full(mbox), "mbox full?");
	ASSERT(kmbox_try_send(mbox, &msg4), "mbox send failure");

	ASSERT(!kmbox_empty(mbox), "mbox empty?");
	ASSERT(kmbox_full(mbox), "mbox not full?");

	// 5th message: not possible because it's full
	ASSERT(!kmbox_try_send(mbox, &msg5), "kmbox_try_send should not succeed with full mailbox");

	// At this point, the read thread has not read anything yet
	// because it's lower priority.
	uint8_t exp_initial[] = { 0 };
	ASSERT_EQUAL_MEM(msgread, exp_initial, sizeof(exp_initial), "invalid order of messages");

	// This will block, and at this point the reader thread will be scheduled.
	// As soon as it reads one message off the mbox, it will be switched away
	// without having time to process it.
	LOG("kmbox_send(5)\n");
	kmbox_send(mbox, &msg5);
	LOG("after kmbox_send(5)\n");

	uint8_t exp_after5[] = { 0 };
	ASSERT_EQUAL_MEM(msgread, exp_after5, sizeof(exp_after5), "invalid order of messages");

	LOG("kmbox_send(6)\n");
	kmbox_send(mbox, &msg6);
	LOG("after kmbox_send(6)\n");
	kmbox_send(mbox, &msg7);
	kmbox_send(mbox, &msg8);

	uint8_t exp_after8[] = { 1,2,3,0 };
	ASSERT_EQUAL_MEM(msgread, exp_after8, sizeof(exp_after8), "invalid order of messages");

	// Yielding the thread doesn't produce anything because the main
	// thread is higher priority
	kthread_yield();

	uint8_t exp_after_yield[] = { 1,2,3,0 };
	ASSERT_EQUAL_MEM(msgread, exp_after_yield, sizeof(exp_after_yield), "invalid order of messages");

	// Sleep to let the reader thread finish
	kthread_sleep(TICKS_FROM_MS(3));

	uint8_t exp_final[] = { 1,2,3,4,5,6,7,8,0 };
	ASSERT_EQUAL_MEM(msgread, exp_final, sizeof(exp_final), "invalid order of messages");
}


// Test mbox with writer thread having priority equal to reader thread.
void test_kernel_mbox_2(TestContext *ctx) {
	timer_init();
	DEFER(timer_close());

	kernel_init();
	DEFER(kernel_close());

	uint8_t msg1=1, msg2=2, msg3=3, msg4=4, msg5=5, msg6=6, msg7=7, msg8=8;
	kmbox_t *mbox = kmbox_new(4);
	DEFER(kmbox_free(mbox));

	uint8_t msgread[16] = {0};
	uint8_t msgread_idx = 0;

	void read_thread(void* arg)
	{
		uint8_t *msg;
		LOG("read_thread() %d\n", kmbox_empty(mbox));
		while ((msg = kmbox_recv(mbox)))
		{
			LOG("mbox_recv(): %p:%d\n", msg, *msg);
			msgread[msgread_idx++] = *msg;
		}
	}

	// Bump priority of main thread to suspend the reader thread
	kthread_set_pri(NULL, 2);

	// Create reader thread
	kthread_t *th1 = kthread_new("read_thread", 2048, 1, read_thread, NULL);
	DEFER(kthread_kill(th1));

	// Enqueue messages. The reader thread has not started yet because it
	// has lower priority for now
	kmbox_send(mbox, &msg1);
	kmbox_send(mbox, &msg2);
	kmbox_send(mbox, &msg3);
	kmbox_send(mbox, &msg4);

	// At this point, the read thread has not read anything yet because we have
	// never context switched
	uint8_t exp_initial[] = { 0 };
	ASSERT_EQUAL_MEM(msgread, exp_initial, sizeof(exp_initial), "invalid order of messages");

	// Now lower the priority of the main thread. This will cause the
	// reader thread to start, and it will then flush the whole mbox right
	// away.
	kthread_set_pri(NULL, 1);

	uint8_t exp_after_pri[] = { 1,2,3,4,0 };
	ASSERT_EQUAL_MEM(msgread, exp_after_pri, sizeof(exp_after_pri), "invalid order of messages");

	// Now writing a message will trigger a read right away
	kmbox_send(mbox, &msg5);

	uint8_t exp_after5[] = { 1,2,3,4,5,0 };
	ASSERT_EQUAL_MEM(msgread, exp_after5, sizeof(exp_after5), "invalid order of messages");

	kmbox_send(mbox, &msg6);
	kmbox_send(mbox, &msg7);
	kmbox_send(mbox, &msg8);

	uint8_t exp_after8[] = { 1,2,3,4,5,6,7,8,0 };
	ASSERT_EQUAL_MEM(msgread, exp_after8, sizeof(exp_after8), "invalid order of messages");
}


// Test mbox with writer thread having priority lower than reader thread.
void test_kernel_mbox_3(TestContext *ctx) {
	timer_init();
	DEFER(timer_close());

	kernel_init();
	DEFER(kernel_close());

	uint8_t msg1=1, msg2=2, msg3=3, msg4=4, msg5=5, msg6=6, msg7=7, msg8=8;
	kmbox_t *mbox = kmbox_new(4);
	DEFER(kmbox_free(mbox));

	uint8_t msgread[16] = {0};
	uint8_t msgread_idx = 0;

	void read_thread(void* arg)
	{
		uint8_t *msg;
		LOG("read_thread() %d\n", kmbox_empty(mbox));
		while ((msg = kmbox_recv(mbox)))
		{
			LOG("mbox_recv(): %p:%d\n", msg, *msg);
			msgread[msgread_idx++] = *msg;
		}
	}

	// Bump priority of main thread
	kthread_set_pri(NULL, 1);

	// Create reader thread
	kthread_t *th1 = kthread_new("read_thread", 2048, 2, read_thread, NULL);
	DEFER(kthread_kill(th1));

	// Enqueue messages. They will be processed right away
	// because the reader thread has higher priority.
	kmbox_send(mbox, &msg1);
	uint8_t exp_initial[] = { 1,0 };
	ASSERT_EQUAL_MEM(msgread, exp_initial, sizeof(exp_initial), "invalid order of messages");

	kmbox_send(mbox, &msg2);
	kmbox_send(mbox, &msg3);
	kmbox_send(mbox, &msg4);
	uint8_t exp_after_4[] = { 1,2,3,4,0 };
	ASSERT_EQUAL_MEM(msgread, exp_after_4, sizeof(exp_after_4), "invalid order of messages");

	// This will block, and at this point the reader thread will be scheduled.
	// As soon as it reads one message off the mbox, it will be switched away
	// without having time to process it.
	kmbox_send(mbox, &msg5);
	kmbox_send(mbox, &msg6);
	kmbox_send(mbox, &msg7);
	kmbox_send(mbox, &msg8);

	uint8_t exp_final[] = { 1,2,3,4,5,6,7,8,0 };
	ASSERT_EQUAL_MEM(msgread, exp_final, sizeof(exp_final), "invalid order of messages");
}

void test_kernel_event_1(TestContext *ctx) {
	timer_init();
	DEFER(timer_close());

	kernel_init();
	DEFER(kernel_close());

	set_SP_interrupt(true);
	DEFER(set_SP_interrupt(false));

	bool called = false;

	void wait_thread(void *arg)
	{
		kthread_wait_event(&KEVENT_IRQ_SP);
		called = true;
	}

	void rsp_thread(void* arg)
	{
		uint32_t __attribute__((aligned(8))) miniucode[] = { 0x0000000d }; // BREAK opcode
		
		data_cache_hit_writeback_invalidate(miniucode, sizeof(miniucode));
		load_ucode(miniucode, sizeof(miniucode));
		run_ucode();
	}

	kthread_set_pri(NULL, 1);
	kthread_new("wait_thread", 2048, 2, wait_thread, NULL);
	kthread_new("rsp_thread", 2048, 2, rsp_thread, NULL);

	kthread_sleep(TICKS_FROM_MS(5));
	ASSERT(called, "event not triggered");
}

void test_kernel_event_2(TestContext *ctx) {
	timer_init();
	DEFER(timer_close());

	kernel_init();
	DEFER(kernel_close());

	set_SP_interrupt(true);
	DEFER(set_SP_interrupt(false));

	uint8_t called[16] = {0};
	int called_idx = 0;

	kmbox_t *m1 = kmbox_new_stack(1);
	kmbox_t *m2 = kmbox_new_stack(1);

	void read_kthread_1(void* arg)
	{
		while (kmbox_recv(m1) == &KEVENT_IRQ_SP)
			called[called_idx++] = 1;
	}

	void read_kthread_2(void* arg)
	{
		while (kmbox_recv(m2) == &KEVENT_IRQ_SP)
			called[called_idx++] = 2;
	}

	void rsp_thread(void* arg)
	{
		uint32_t __attribute__((aligned(8))) miniucode[] = { 0x0000000d }; // BREAK opcode
		
		data_cache_hit_writeback_invalidate(miniucode, sizeof(miniucode));
		load_ucode(miniucode, sizeof(miniucode));
		run_ucode();
	}

	kmbox_attach_event(m1, &KEVENT_IRQ_SP);
	DEFER(kmbox_detach_event(m1, &KEVENT_IRQ_SP));

	kmbox_attach_event(m2, &KEVENT_IRQ_SP);
	DEFER(kmbox_detach_event(m2, &KEVENT_IRQ_SP));

	kthread_set_pri(NULL, 5);
	kthread_new("rsp_thread", 2048, 2, rsp_thread, NULL);

	kthread_t *rth1 = kthread_new("read_kthread_1", 2048, 3, read_kthread_1, NULL);
	DEFER(kthread_kill(rth1));

	kthread_t *rth2 = kthread_new("read_kthread_2", 2048, 4, read_kthread_2, NULL);
	DEFER(kthread_kill(rth2));

	// Go to sleep. Now the RSP will run, trigger the two reader threads, and
	// they will register themselves in the called array.
	kthread_sleep(TICKS_FROM_MS(5));

	// Make sure the order is correct (first the higher priority thread).
	uint8_t exp1[] = { 2, 1, 0 };
	ASSERT_EQUAL_MEM(called, exp1, sizeof(exp1), "invalid order of threads");

	// Now detach thread 2 from the event, so it will not be triggered anymore.
	kmbox_detach_event(m2, &KEVENT_IRQ_SP);

	// Run the RSP again.
	kthread_new("rsp_thread", 2048, 2, rsp_thread, NULL);
	kthread_sleep(TICKS_FROM_MS(5));

	// Only thread 1 should have been called
	uint8_t exp2[] = { 2, 1, 1, 0 };
	ASSERT_EQUAL_MEM(called, exp2, sizeof(exp2), "invalid order of threads");
}
