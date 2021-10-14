extern unsigned int __global_constructor_test_value;

void test_constructors(TestContext *ctx) {
	ASSERT(__global_constructor_test_value == 0xC70125, "Global constructors did not get executed!");
}