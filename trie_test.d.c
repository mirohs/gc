/*
@author: Michael Rohs
@date: January 19, 2021
*/

// #define NO_DEBUG
// #define NO_ASSERT
// #define NO_REQUIRE
// #define NO_ENSURE

#include <time.h>
#include "util.h"
#include "trie.h"

bool f_visit_keep(uint64_t x)
    printf("%llx\n", x)
    return true

bool f_visit_remove(uint64_t x)
    printf("%llx\n", x)
    return false

void test0(void)
    uint64_t t = 0
    trie_insert(&t, 0x1234, 0)
    trie_print(t, 0, 0)
    trie_insert(&t, 0x1234, 0)
    trie_print(t, 0, 0)
    test_equal_i(trie_contains(t, 0x1234, 0), true)
    test_equal_i(trie_contains(t, 0x1244, 0), false)
    trie_insert(&t, 0x1244, 0)
    trie_print(t, 0, 0)
    test_equal_i(trie_contains(t, 0x1234, 0), true)
    test_equal_i(trie_contains(t, 0x1244, 0), true)

    trie_insert(&t, 0x2, 0)
    trie_print(t, 0, 0)
    trie_insert(&t, 0x6, 0)
    trie_print(t, 0, 0)
    trie_insert(&t, 0x4, 0)
    trie_print(t, 0, 0)
    trie_insert(&t, 0x44, 0)
    trie_insert(&t, 0x66, 0)
    trie_insert(&t, 0x88, 0)
    trie_insert(&t, 0x98, 0)
    printf("\n")
    trie_print(t, 0, 0)
    printf("\n")
    trie_print(t, 0, 0)
    printf("\n")
    test_equal_i(trie_contains(t, 0x1234, 0), true)
    test_equal_i(trie_contains(t, 0x1244, 0), true)
    test_equal_i(trie_contains(t, 0x2, 0), true)
    test_equal_i(trie_contains(t, 0x4, 0), true)
    test_equal_i(trie_contains(t, 0x6, 0), true)
    test_equal_i(trie_contains(t, 0x8, 0), false)
    test_equal_i(trie_contains(t, 0x88, 0), true)

    /*
    int count = 0, max_level = 0
    double mean_level = 0
    trie_size(t, 0, &count, &max_level, &mean_level)
    printf("count = %d, max_level = %d, mean_level = %.3f\n", 
            count, max_level, mean_level / count)
    */

    trie_visit(&t, f_visit_keep)
    assert("is empty", !trie_is_empty(t))

    trie_visit(&t, f_visit_remove)
    assert("is empty", trie_is_empty(t))

#define N 100000

void test1(void)
    // printf("%ld\n", time(NULL))
    srand (time(NULL));
    uint64_t t = 0
    // printf("%d\n", RAND_MAX)
    // allocated_nodes = 0

    for int i = 0; i < N; i++ do
        //uint64_t x = (rand() + 1) << 1
        uint64_t x = (i + 1) << 1
        trie_insert(&t, x, 0)

    #if 1
    for int i = 0; i < N; i++ do
        uint64_t x = (i + 1) << 1
        assert("", trie_contains(t, x, 0))

    for int i = N; i < 10 * N; i++ do
        uint64_t x = (i + 1) << 1
        assert("", !trie_contains(t, x, 0))
    #endif

    /*
    int count = 0, max_level = 0
    double mean_level = 0
    trie_size(t, 0, &count, &max_level, &mean_level)
    printf("count = %d, max_level = %d, mean_level = %.3f\n", 
            count, max_level, mean_level / count)
    printf("%d allocated nodes, %.2f Nodes/Pointer, %.2f Pointers/Node, %.2f memory overhead\n", 
            allocated_nodes,
            (double)allocated_nodes / N, (double)N / allocated_nodes,
            (double)allocated_nodes * sizeof(Node) / (8 * N))
    */

void test2(void)
    uint64_t t = 0

    for int i = 0; i < N; i++ do
        uint64_t x = (i + 1) << 1
        trie_insert(&t, x, 0)

    for int i = 0; i < N; i++ do
        uint64_t x = (i + 1) << 1
        assert("", trie_contains(t, x, 0))

    for int i = N; i < 2 * N; i++ do
        uint64_t x = (i + 1) << 1
        assert("", !trie_contains(t, x, 0))

    for int i = 0; i < N/2; i++ do
        uint64_t x = (i + 1) << 1
        trie_remove(&t, x, 0)
        trie_remove(&t, x, 0)

    for int i = 0; i < N/2; i++ do
        uint64_t x = (i + 1) << 1
        assert("", !trie_contains(t, x, 0))

    for int i = N/2; i < N; i++ do
        uint64_t x = (i + 1) << 1
        assert("", trie_contains(t, x, 0))

    /*
    int count = 0, max_level = 0
    double mean_level = 0
    trie_size(t, 0, &count, &max_level, &mean_level)
    printf("count = %d, max_level = %d, mean_level = %.3f\n", 
            count, max_level, mean_level / count)
    */

    for int i = 0; i < N; i++ do
        uint64_t x = (i + 1) << 1
        trie_remove(&t, x, 0)
    assert("trie empty", t == 0)

    for int i = 0; i < N; i++ do
        uint64_t x = (i + 1) << 1
        trie_insert(&t, x, 0)

    for int i = 0; i < N; i++ do
        uint64_t x = (i + 1) << 1
        assert("", trie_contains(t, x, 0))

    for int i = N; i < 10 * N; i++ do
        uint64_t x = (i + 1) << 1
        assert("", !trie_contains(t, x, 0))

    for int i = 0; i < N/2; i++ do
        uint64_t x = (i + 1) << 1
        trie_remove(&t, x, 0)
        trie_remove(&t, x, 0)

    for int i = 0; i < N/2; i++ do
        uint64_t x = (i + 1) << 1
        assert("", !trie_contains(t, x, 0))

    for int i = N/2; i < N; i++ do
        uint64_t x = (i + 1) << 1
        assert("", trie_contains(t, x, 0))

    /*
    count = 0; max_level = 0
    mean_level = 0
    trie_size(t, 0, &count, &max_level, &mean_level)
    printf("count = %d, max_level = %d, mean_level = %.3f\n", 
            count, max_level, mean_level / count)
    */

void test(void)
    uint64_t t = 0
    char* buffer[N]
    int trailing_zeros = 64

    for int i = 0; i < N; i++ do
        buffer[i] = xmalloc(i+1)
        // printf("%llx\n", (uint64_t)buffer[i] >> 2)
        // count trailing zeros
        // https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
        int tz = __builtin_ctz((uint64_t)buffer[i])
        if tz < trailing_zeros do trailing_zeros = tz
        trie_insert(&t, (uint64_t)buffer[i] >> 2, 0)
    printf("trailing_zeros = %d\n", trailing_zeros)

    for int i = 0; i < N; i++ do
        assert("", trie_contains(t, (uint64_t)buffer[i] >> 2, 0))

    for int i = 0; i < N/2; i++ do
        uint64_t x = (uint64_t)buffer[i] >> 2
        trie_remove(&t, x, 0)
        trie_remove(&t, x, 0)

    for int i = 0; i < N/2; i++ do
        uint64_t x = (uint64_t)buffer[i] >> 2
        assert("", !trie_contains(t, x, 0))

    for int i = N/2; i < N; i++ do
        uint64_t x = (uint64_t)buffer[i] >> 2
        assert("", trie_contains(t, x, 0))

    /*
    int count = 0, max_level = 0
    double mean_level = 0
    trie_size(t, 0, &count, &max_level, &mean_level)
    printf("count = %d, max_level = %d, mean_level = %.3f\n", 
            count, max_level, mean_level / count)
    */
    // trie_print(t, 0, 0)

int main(void)
    test0()
    test1()
    test()
    return 0

