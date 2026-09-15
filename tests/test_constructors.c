extern unsigned int __global_cpp_constructor_test_value;

unsigned int __global_constructor_test_value;
unsigned int __global_constructor_test_value_old;
unsigned int __global_constructor_test_prio;

__attribute__((constructor(123))) void __global_constructor_test_prio1(void)
{
    __global_constructor_test_prio = 0xC0C70123;
}

__attribute__((constructor(125))) void __global_constructor_test_prio2(void)
{
    __global_constructor_test_value_old = __global_constructor_test_value;
    __global_constructor_test_prio = 0xE0C70125;
}

__attribute__((constructor)) void __global_constructor_test()
{
    __global_constructor_test_value = 0xC0C70125;
}

void test_constructors(TestContext *ctx) {
	ASSERT(__global_constructor_test_value == 0xC0C70125, "Global constructors did not get executed!");
	ASSERT(__global_cpp_constructor_test_value == 0xD0C70125, "Global C++ constructors did not get executed!");
    ASSERT(__global_constructor_test_prio == 0xE0C70125 && __global_constructor_test_value_old == 0, "Global constructors with priority don't work correctly!");
}