#include "../src/hashtable_internal.h"

void test_hashtable_cap4_edge_case(TestContext *ctx)
{
    hashtable_t ht;
    hashtable_init(&ht, 4, NULL);

    hashtable_insert(&ht, 1, (void*)123);
    hashtable_insert(&ht, 2, (void*)234);
    hashtable_insert(&ht, 3, (void*)345);

    hashtable_insert(&ht, 4, (void*)456);
    hashtable_remove(&ht, 4);
    hashtable_insert(&ht, 4, (void*)456);

    ASSERT_EQUAL_UNSIGNED((uint32_t)VirtualCachedAddr(456), (uint32_t)hashtable_lookup(&ht, 4), "Key 4 does not match");
    ASSERT_EQUAL_UNSIGNED((uint32_t)VirtualCachedAddr(345), (uint32_t)hashtable_lookup(&ht, 3), "Key 3 does not match");
    ASSERT_EQUAL_UNSIGNED((uint32_t)VirtualCachedAddr(234), (uint32_t)hashtable_lookup(&ht, 2), "Key 2 does not match");
    ASSERT_EQUAL_UNSIGNED((uint32_t)VirtualCachedAddr(123), (uint32_t)hashtable_lookup(&ht, 1), "Key 1 does not match");
}