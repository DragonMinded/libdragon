extern unsigned int __global_cpp_constructor_test_value;

unsigned int __global_constructor_test_value;

__attribute__((constructor)) void __global_constructor_test()
{
    __global_constructor_test_value = 0xC0C70125;
}

void test_constructors(TestContext *ctx) {
	ASSERT(__global_constructor_test_value == 0xC0C70125, "Global constructors did not get executed!");
	ASSERT(__global_cpp_constructor_test_value == 0xD0C70125, "Global C++ constructors did not get executed!");
}