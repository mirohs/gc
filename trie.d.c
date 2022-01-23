#include <time.h>
#define NODEBUG
#include "util.h"
#include "trie.h"

#define is_value(t) (((t) & 1) == 0)
#define is_node(t) (((t) & 1) == 1)
#define is_empty(t) ((t) == 0)
#define bit_count 4
#define slot_count (1 << (bit_count))
#define bit_mask ((slot_count) - 1)

typedef struct Node Node
struct Node
    uint64_t slots[slot_count]

int allocated_nodes = 0
Node* new_node(void)
    allocated_nodes++
    return xcalloc(1, sizeof(Node))

*bool trie_is_empty(uint64_t t)
    return is_empty(t)

*void trie_insert(uint64_t* t, uint64_t x, int level)
    require_not_null(t)
    require("not null", x != 0)
    require("is value", is_value(x))
    require("not negative", level >= 0)
    loop:
    uint64_t y = *t
    PLf("t = %p, y = %llx, x = %llx, level = %d", t, y, x, level)
    if is_empty(y) do
        PL
        // empty trie, set x
        *t = x
    else if x == y do
        PL
        // value already in trie, do nothing
        return
    else if is_node(y) do
        PL
        // tree is a node (LSB set)
        Node* node = (Node*)(y & ~1)
        int i = (x >> (bit_count * level)) & bit_mask
        PLf("tree is node %p, insert in slot %d", node, i)
        // trie_insert(node->slots + i, x, level + 1)
        t = node->slots + i; level += 1
        goto loop // avoid recursion
    else
        PL
        assert("valid y", !is_empty(y) && x != y && is_value(y))
        // slot contains value y that needs to be moved down
        // x and y are identical for the least sigificant nibbles from 0 to level (exclusive)
        while (true)
            Node* node = new_node()
            *t = (uint64_t)node | 1 // set marker bit (LSB set)
            int i = (x >> (bit_count * level)) & bit_mask
            int j = (y >> (bit_count * level)) & bit_mask
            if i != j do
                PLf("set at level = %d, i = %d, j = %d", level, i, j)
                node->slots[i] = x
                node->slots[j] = y
                break
            else
                t = node->slots + i
                level++

*bool trie_contains(uint64_t t, uint64_t x, int level)
    require("not null", x != 0)
    require("is value", is_value(x))
    require("not negative", level >= 0)
    loop:
    PLf("t = %llx, x = %llx, level = %d", t, x, level)
    if is_empty(t) do
        PL
        return false
    else if x == t do
        PL
        return true
    else if is_node(t) do
        PL
        // t is a node (LSB set)
        Node* node = (Node*)(t & ~1) // clear marker bit (LSB)
        int i = (x >> (bit_count * level)) & bit_mask
        PLf("tree is node %p, search in slot %d", node, i)
        // return trie_contains(node->slots[i], x, level + 1)
        t = node->slots[i]; level += 1
        goto loop // avoid recursion
    PL
    // slot contains another value, x not in tree
    require("is another value", !is_empty(t) && is_value(t) && x != t)
    return false

*void trie_remove(uint64_t* t, uint64_t x, int level)
    require_not_null(t)
    require("not null", x != 0)
    require("is value", is_value(x))
    require("not negative", level >= 0)
    uint64_t y = *t
    PLf("t = %p, y = %llx, x = %llx, level = %d", t, y, x, level)
    if is_empty(y) do
        PL
        // slot empty, value is not in trie, do nothing
        return
    else if x == y do
        PL
        // found, remove
        *t = 0
    else if is_node(y) do
        PL
        // tree is a node (LSB set)
        Node* node = (Node*)(y & ~1) // clear LSB
        int i = (x >> (bit_count * level)) & bit_mask
        PLf("tree is node %p, remove in slot %d", node, i)
        uint64_t* slots = node->slots
        trie_remove(slots + i, x, level + 1)
        // if now zero slots are used or one slot is used for a value, then delete the node
        int j = 0, n = 0
        for int i = 0; i < slot_count; i++ do
            if slots[i] != 0 do
                j = i
                n++
                if n > 1 do return
        if n == 0 do
            *t = 0
            free(node)
        else if n == 1 && is_value(slots[j]) do
            *t = slots[j]
            free(node)
    else
        PL
        // slot contains another value, x not in tree, do nothing
        assert("is another value", !is_empty(y) && is_value(y) && x != y)
        return

void trie_size(uint64_t t, int level, int* count, int* max_level, double* mean_level)
    require("not negative", level >= 0)
    require_not_null(count)
    require_not_null(max_level)
    require_not_null(mean_level)
    if t != 0 do
        if level > *max_level do *max_level = level
        if is_value(t) do
            // t is a value (LSB clear)
            *count += 1
            *mean_level += level
        else
            // t is a node (LSB set)
            Node* node = (Node*)(t & ~1) // clear marker bit (LSB)
            for int i = 0; i < slot_count; i++ do
                trie_size(node->slots[i], level + 1, count, max_level, mean_level)

*void trie_print(uint64_t t, int level, int index)
    if t != 0 do
        if is_value(t) do
            // t is a value (LSB clear)
            printf("%d:%d: %llx\n", level, index, t)
        else
            // t is a node (LSB set)
            Node* node = (Node*)(t & ~1) // clear marker bit (LSB)
            for int i = 0; i < slot_count; i++ do
                trie_print(node->slots[i], level + 1, i)

*typedef bool (*VisitFn)(uint64_t x);

*void trie_visit(uint64_t* t, VisitFn f)
    require_not_null(t)
    require_not_null(f)
    uint64_t x = *t
    if x != 0 do
        if is_value(x) do
            // x is a value (LSB clear)
            bool keep = f(x)
            if !keep do *t = 0
        else
            assert("valid node", is_node(x) && !is_empty(x))
            // x is a node (LSB set)
            Node* node = (Node*)(x & ~1) // clear marker bit (LSB)
            uint64_t* slots = node->slots
            int j = 0, n = 0
            for int i = 0; i < slot_count; i++ do
                if slots[i] != 0 do
                    trie_visit(slots + i, f)
                    if slots[i] != 0 do
                        j = i
                        n++
            // if now zero slots are used or one slot is used for a value, then delete the node
            if n == 0 do
                *t = 0
                free(node)
            else if n == 1 && is_value(slots[j]) do
                *t = slots[j]
                free(node)

/*
*void trie_visit(uint64_t* t, VisitFn f)
    uint64_t x = *t
    if x != 0 do
        if is_value(x) do
            f(t)
        else
            assert("is node", is_node(x))
            Node* node = (Node*)(x & ~1) // clear marker bit (LSB)
            while node != NULL do
                for int i = 0; i < slot_count; i++ do
                    uint64_t* s = node->slots + i
                    x = *t
                    if x != 0 do
                        if is_value(x) do
                            f(s)
                        else
                            assert("is node", is_node(x))
                            Node* node2 = (Node*)(x & ~1) // clear marker bit (LSB)
                            node2->next = node->next
                            node->next = node2
                node = node->next
*/

/*
*#define trie_visit(t, doit) {\
    if (t != 0) {\
        if (is_value(t)) {\
            doit(t);\
        } else {\
            Node* node = (Node*)(t & ~1);\
            while (node != NULL) {\
                for (int i = 0; i < slot_count; i++) {\
                    uint64_t s = node->slots[i];\
                    if (s != 0) {\
                        if (is_value(s)) {\
                            doit(t);\
                        } else {\
                            Node* node2 = (Node*)(s & ~1);\
                            node2->next = node->next;\
                            node->next = node2;\
                        }\
                    }\
                }\
                node = node->next;\
            }\
        }\
    }\
}
*/
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
    PLi(trie_contains(t, 0x1234, 0))
    PLi(trie_contains(t, 0x1244, 0))
    trie_insert(&t, 0x1244, 0)
    trie_print(t, 0, 0)
    PLi(trie_contains(t, 0x1234, 0))
    PLi(trie_contains(t, 0x1244, 0))

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
    PLi(trie_contains(t, 0x1234, 0))
    PLi(trie_contains(t, 0x1244, 0))
    PLi(trie_contains(t, 0x2, 0))
    PLi(trie_contains(t, 0x4, 0))
    PLi(trie_contains(t, 0x6, 0))
    PLi(trie_contains(t, 0x8, 0))

    int count = 0, max_level = 0
    double mean_level = 0
    trie_size(t, 0, &count, &max_level, &mean_level)
    printf("count = %d, max_level = %d, mean_level = %.3f\n", 
            count, max_level, mean_level / count)

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
    allocated_nodes = 0

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

    int count = 0, max_level = 0
    double mean_level = 0
    trie_size(t, 0, &count, &max_level, &mean_level)
    printf("count = %d, max_level = %d, mean_level = %.3f\n", 
            count, max_level, mean_level / count)
    printf("%d allocated nodes, %.2f Nodes/Pointer, %.2f Pointers/Node, %.2f memory overhead\n", 
            allocated_nodes,
            (double)allocated_nodes / N, (double)N / allocated_nodes,
            (double)allocated_nodes * sizeof(Node) / (8 * N))

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

    int count = 0, max_level = 0
    double mean_level = 0
    trie_size(t, 0, &count, &max_level, &mean_level)
    printf("count = %d, max_level = %d, mean_level = %.3f\n", 
            count, max_level, mean_level / count)

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

    count = 0; max_level = 0
    mean_level = 0
    trie_size(t, 0, &count, &max_level, &mean_level)
    printf("count = %d, max_level = %d, mean_level = %.3f\n", 
            count, max_level, mean_level / count)

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

    int count = 0, max_level = 0
    double mean_level = 0
    trie_size(t, 0, &count, &max_level, &mean_level)
    printf("count = %d, max_level = %d, mean_level = %.3f\n", 
            count, max_level, mean_level / count)
    // trie_print(t, 0, 0)

int xmain(void)
    test()
    return 0
