#include <errno.h>
#include <sys/stat.h>

_Thread_local int tls_var = 5;
_Thread_local int tls_var_bss;

void test_kernel_basic(TestContext *ctx) {
	kernel_init();
	DEFER(kernel_close());

	uint8_t thcalled[16] = {0};
	uint8_t thcalled_idx = 0;

	int func_th(void *arg)
	{
		kthread_detach(NULL);
		int thid = (int)arg;

		thcalled[thcalled_idx++] = thid;
		kthread_yield();		
		thcalled[thcalled_idx++] = thid;
		kthread_yield();
		thcalled[thcalled_idx++] = thid;
		return 0;
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

	int func_th1(void *arg)
	{
		kthread_detach(NULL);
		thcalled[thcalled_idx++] = 1;
		kthread_yield();		
		thcalled[thcalled_idx++] = 1;
		kthread_yield();
		thcalled[thcalled_idx++] = 1;
		return 0;
	}

	int func_th2(void *arg)
	{
		kthread_detach(NULL);
		thcalled[thcalled_idx++] = 2;
		kthread_new("test1", 2048, 5, func_th1, 0);
		thcalled[thcalled_idx++] = 2;
		return 0;
	}

	int func_th3(void *arg)
	{
		kthread_detach(NULL);
		thcalled[thcalled_idx++] = 3;
		kthread_new("test2", 2048, 6, func_th2, 0);
		thcalled[thcalled_idx++] = 3;
		kthread_yield();
		thcalled[thcalled_idx++] = 3;
		return 0;
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

	int func_th1(void *arg)
	{
		kthread_detach(NULL);
		LOG("func_th1 called\n");
		thcalled[thcalled_idx++] = 1;
		kthread_sleep(TICKS_FROM_MS(5));
		thcalled[thcalled_idx++] = 1;
		kthread_sleep(TICKS_FROM_MS(5));
		thcalled[thcalled_idx++] = 1;
		++thexit;
		return 0;
	}

	int func_th2(void *arg)
	{
		kthread_detach(NULL);
		LOG("func_th2 called\n");
		thcalled[thcalled_idx++] = 2;
		kthread_sleep(TICKS_FROM_MS(8));
		thcalled[thcalled_idx++] = 2;
		++thexit;
		return 0;
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

void test_kernel_mutex_1(TestContext *ctx) {
	timer_init();
	DEFER(timer_close());

	kernel_init();
	DEFER(kernel_close());
	
	kmutex_t mtx;
	kmutex_init(&mtx, KMUTEX_RECURSIVE);
	DEFER(kmutex_destroy(&mtx));
	
	uint8_t thcalled[16] = {0};
	int thcalled_idx = 0;

	int func_th(void* arg)
	{
		LOG("func_th1 called\n");

		kmutex_lock(&mtx);
		thcalled[thcalled_idx++] = (int)arg;
		kmutex_unlock(&mtx);
		return 0;
	}

	kmutex_lock(&mtx);

	kthread_set_pri(NULL, 1);
	kthread_t *th1 = kthread_new("test1", 2048, 4, func_th, (void*)1);
	kthread_t *th2 = kthread_new("test2", 2048, 5, func_th, (void*)2);
	kthread_t *th3 = kthread_new("test3", 2048, 7, func_th, (void*)3);
	kthread_t *th4 = kthread_new("test4", 2048, 6, func_th, (void*)4);

	kmutex_unlock(&mtx);
	
	kthread_join(th1);
	kthread_join(th2);
	kthread_join(th3);
	kthread_join(th4);

	uint8_t exp[] = { 3,4,2,1 };
	ASSERT_EQUAL_MEM(thcalled, exp, sizeof(exp), "invalid order of threads");
}

// Check that errno is a thread local variable
void test_kernel_libc1(TestContext *ctx) {
	kernel_init();
	DEFER(kernel_close());

	kcond_t stepper;
	kcond_init(&stepper);

	int func_mkdir(void *arg)
	{
		kcond_wait(&stepper, NULL);
		mkdir(arg, 0777);
		kcond_wait(&stepper, NULL);
		return errno;
	}

	kthread_t *th1 = kthread_new("test1", 4096, 4, func_mkdir, "abc");
	kthread_t *th2 = kthread_new("test2", 4096, 4, func_mkdir, "rom:/abc");

	kcond_broadcast(&stepper);
	kcond_broadcast(&stepper);
	int errno1 = kthread_join(th1);
	int errno2 = kthread_join(th2);

	ASSERT_EQUAL_SIGNED(errno1, EINVAL, "invalid error code for mkdir1");
	ASSERT_EQUAL_SIGNED(errno2, ENOSYS, "invalid error code for mkdir2");
}

// Check that errno is a thread local variable
void test_kernel_libc2(TestContext *ctx) {
	kernel_init();
	DEFER(kernel_close());

	kcond_t stepper;
	kcond_init(&stepper);

	int func_strtok(void *arg)
	{
		kcond_wait(&stepper, NULL);
		int n=1;
		strtok(arg, ",");
		kthread_yield();
		while (strtok(NULL, ",") != NULL) {
			kthread_yield();
			++n;
		}
		return n;
	}

	char str1[] = "a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z";
	char str2[] = "A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z";

	kthread_t* th1 = kthread_new("test3", 4096, 4, func_strtok, str1);
	kthread_t* th2 = kthread_new("test4", 4096, 4, func_strtok, str2);

	kcond_broadcast(&stepper);
	int n1 = kthread_join(th1);
	int n2 = kthread_join(th2);

	ASSERT_EQUAL_SIGNED(n1, 26, "invalid number of tokens for strtok1");
	ASSERT_EQUAL_SIGNED(n2, 26, "invalid number of tokens for strtok2");
}

void test_kernel_thread_local(TestContext *ctx) {

	kernel_init();
	DEFER(kernel_close());

	// Test that thread-local storage works also in the main thread
	ASSERT_EQUAL_SIGNED(tls_var, 5, "tls_var not set");
	ASSERT_EQUAL_SIGNED(tls_var_bss, 0, "tls_var_bss not set");
	tls_var = 0x1234;
	tls_var_bss = 0x5678;
	ASSERT_EQUAL_SIGNED(tls_var, 0x1234, "tls_var not set");
	ASSERT_EQUAL_SIGNED(tls_var_bss, 0x5678, "tls_var not set");

	kcond_t stepper;
	kcond_init(&stepper);

	uint8_t thval[16] = {0};
	int thval_idx = 0;

	int func_th(void *arg)
	{
		thval[thval_idx++] = tls_var;
		tls_var = (int)arg;
		kcond_wait(&stepper, NULL);
		thval[thval_idx++] = tls_var;
		tls_var = (int)arg;
		kcond_wait(&stepper, NULL);
		thval[thval_idx++] = tls_var;
		return 0;
	}

	kthread_t *th1 = kthread_new("test1", 2048, 3, func_th, (void*)10);
	kthread_t *th2 = kthread_new("test2", 2048, 2, func_th, (void*)20);
	kthread_t *th3 = kthread_new("test2", 2048, 1, func_th, (void*)30);

	kcond_broadcast(&stepper);
	kcond_broadcast(&stepper);
	kthread_join(th1);
	kthread_join(th2);
	kthread_join(th3);

	uint8_t expected[] = { 10, 20, 30, 11, 21, 31, 12, 22, 32 };
	ASSERT_EQUAL_MEM(thval, expected, sizeof(expected), "invalid order of threads");
}
