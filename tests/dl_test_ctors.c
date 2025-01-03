//Value used to test if global constructors have run
unsigned int dl_ctor_test_value;

//Global constructor to set test value
__attribute__((constructor)) void __dl_ctor_test()
{
    dl_ctor_test_value = 0x456789AB;
}
