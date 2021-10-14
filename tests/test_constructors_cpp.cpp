
extern "C" {
    unsigned int __global_constructor_test_value;
}

class TestClass
{
    public:
        TestClass() {
            __global_constructor_test_value = 0xC70125;
        }
};

TestClass globalClass;

