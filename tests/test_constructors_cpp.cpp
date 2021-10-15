
extern "C" {
    unsigned int __global_cpp_constructor_test_value;
}

class TestClass
{
    public:
        TestClass() {
            __global_cpp_constructor_test_value = 0xD0C70125;
        }
};

TestClass globalClass;

