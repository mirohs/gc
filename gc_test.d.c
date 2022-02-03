/*
@author: Michael Rohs
@date: January 19, 2022
*/

// #define NO_DEBUG
// #define NO_ASSERT
// #define NO_REQUIRE
// #define NO_ENSURE

#include <time.h>
#include <errno.h>
#include <sys/resource.h>
#include "util.h"
#include "gc.h"

/*
Allocate a C string. A string is an object without pointers to managed memory.
Thus it does not need a GCType and uses gc_alloc.
*/
char* new_str(char* s)
    require_not_null(s)
    char* t = gc_alloc(strlen(s) + 1)
    strcpy(t, s)
    return t

/*
Example type that contains one pointer to managed memory (t). The other pointer
(s) is not relevant, because it is supposed to point to memory that is not
managed by the garbage collector.
*/
typedef struct A A
struct A
    int i
    char* s // not managed
    char* t // managed

// A needs a GCType, bacause it contains one pointer to managed memory (t).
GCType* make_a_type(void)
    GCType* type = gc_new_type(sizeof(A), 1)
    gc_set_offset(type, 0, offsetof(A, t))
    PLf("%d, %d, %d", offsetof(A, i), offsetof(A, s), offsetof(A, t))
    return type

// Singleton type object for objects of type A.
GCType* a_type = NULL

// Creates and initializes a new instance of type A.
A* new_a(int i, char* s, char* t)
    require_not_null(s)
    require_not_null(t)
    require_not_null(a_type)
    A* a = gc_alloc_object(a_type)
    a->i = i
    a->s = s
    a->t = new_str(t) // create a managed copy
    return a

// Prints an A object.
void print_a(A* a)
    require_not_null(a)
    printf("A(%d, %s, %s)\n", a->i, a->s, a->t)

// Example type that contains one pointer to managed memory (a).
typedef struct B B
struct B
    int j
    A* a // managed

// B needs a type, bacause it contains one pointer to managed memory (a).
GCType* make_b_type(void)
    GCType* type = gc_new_type(sizeof(B), 1)
    gc_set_offset(type, 0, offsetof(B, a))
    return type

// Singleton type object for objects of type B.
GCType* b_type = NULL

// Creates and initializes a new instance of type B.
B* new_b(int j, A* a)
    require_not_null(a)
    require_not_null(b_type)
    B* b = gc_alloc_object(b_type)
    b->j = j
    b->a = a
    return b

// Prints a B object.
void print_b(B* b)
    require_not_null(b)
    printf("B(%d, %d, %s, %s)\n", b->j, b->a->i, b->a->s, b->a->t)

typedef struct Node Node
struct Node
    int i
    Node* left // managed
    Node* right // managed

GCType* make_node_type(void)
    GCType* type = gc_new_type(sizeof(Node), 2)
    gc_set_offset(type, 0, offsetof(Node, left))
    gc_set_offset(type, 1, offsetof(Node, right))
    return type

GCType* node_type

Node* node(int i, Node* left, Node* right)
    Node* node = gc_alloc_object(node_type)
    // collect() // stress test collection
    node->i = i
    //PLi(i)
    node->left = left
    node->right = right
    return node

Node* leaf(int i)
    return node(i, NULL, NULL)

void print_tree(Node* t)
    if t != NULL do
        printf("o = %p, i = %d\n", t, t->i)
        print_tree(t->left)
        print_tree(t->right)

int sum_tree(Node* t)
    if t == NULL do return 0
    return sum_tree(t->left) + t->i + sum_tree(t->right)

//void test0(void)
void __attribute__((noinline)) test0(void)
    PLf("frame address = %p", __builtin_frame_address(0))
    uint64_t* p = __builtin_frame_address(0)
    PLf("value at frame address = %llx", *p)

    if a_type == NULL do
        a_type = make_a_type()
        printf("a_type = %p\n", a_type)
    if b_type == NULL do
        b_type = make_b_type()
        printf("b_type = %p\n", b_type)

    A* a1 = new_a(5, "hello", "world")
    print_a(a1)
    // a1 = NULL
    PLf("&a1 = %p", &a1)
    // a1 = NULL
    //gc_add_root(a1)

    B* b = new_b(3, a1)
    print_b(b)

    A* a2 = new_a(7, "abc", "def")
    print_a(a2)
    /*
    gc_add_root(b)
    gc_add_root(a2)
    gc_remove_root(b)
    gc_remove_root(a2)
    */
    // a1 = NULL;
    // a2 = NULL; b = NULL

    B* bs = gc_alloc_array(b_type, 3)
    for int i = 0; i < 3; i++ do
        B* bi = bs + i
        bi->j = i
        bi->a = a1
        print_b(bi)
    for int i = 0; i < 3; i++ do
        test_equal_i(bs[i].j, i)
        test_equal_i(bs[i].a->i, 5)
    ///bs = NULL; a1 = NULL

    free(a_type); a_type = NULL
    free(b_type); b_type = NULL

int tree_count(Node* t)
    if t == NULL do return 0
    return tree_count(t->left) + 1 + tree_count(t->right)

int tree_sum(void)
    Node* t = node(1,
                   node(2,
                        leaf(3),
                        leaf(4)),
                   node(5,
                        leaf(6),
                        leaf(7)))
    // gc_add_root(t)
    int n = sum_tree(t)
    // gc_remove_root(t)
    test_equal_i(tree_count(t), 7)
    gc_collect()
    test_equal_i(tree_count(t), 7)
    t->right->left = NULL
    gc_collect()
    test_equal_i(tree_count(t), 6)
    return n

void __attribute__((noinline)) test1(void)
    if node_type == NULL do
        node_type = make_node_type()
        printf("node_type = %p\n", node_type)
    int n = tree_sum()
    printf("n = %d\n", n)
    test_equal_i(n, 1+2+3+4+5+6+7)
    // gc_collect()
    // print_allocations()
    free(node_type); node_type = NULL

void __attribute__((noinline)) test2(void)
    if node_type == NULL do
        node_type = make_node_type()
        printf("node_type = %p\n", node_type)
    Node* t = NULL
    for int i = 0; i < 10; i++ do
        t = node(i, t, NULL)
    print_tree(t)
    // gc_add_root(t)
    t->left->left->left = t // handle cycles
    // print_allocations()
    // mark_stack(); print_allocations()
    // mark_roots(); print_allocations()
    // sweep(); print_allocations()
    t->left->left = NULL
    print_tree(t)
    t = NULL
    // mark_stack(); print_allocations()
    // mark_roots(); print_allocations()
    // sweep(); print_allocations()
    free(node_type); node_type = NULL

Node* fill_tree(int i)
    if i <= 0 do
        return NULL
    else
        return node(i, fill_tree(i - 1), fill_tree(i - 2))

void __attribute__((noinline)) test3(void)
    if node_type == NULL do
        node_type = make_node_type()
        printf("node_type = %p\n", node_type)
    Node* t = NULL
    clock_t time = clock();
    //
    for int i = 0; i < 10000000; i++ do
        t = node(i, t, NULL)
        // t = NULL
        if (i % 123) == 990 do
            t = NULL
            gc_collect()
    //
    //t = fill_tree(24)
    time = clock() - time;
    printf("time: %g ms\n", time * 1000.0 / CLOCKS_PER_SEC);
    /*
    int count = 0; int max_level = 0; double mean_level = 0
    trie_size(allocations, 0, &count, &max_level, &mean_level)
    printf("count = %d, max_level = %d, mean_level = %.3f, trie_nodes = %d, %.2f\n",
            count, max_level, mean_level / count, allocated_nodes,
            (double)count * sizeof(Node) / (allocated_nodes * 16 * 8))
    */
    // print_tree(t)
    t = NULL
    // gc_add_root(t)
    // print_allocations()
    time = clock();
    gc_collect()
    time = clock() - time;
    printf("time: %g ms\n", time * 1000.0 / CLOCKS_PER_SEC);
    // gc_collect();
    // print_allocations()
    free(node_type); node_type = NULL

typedef struct Node3 Node3
struct Node3
    int x
    Node* a// managed
    Node* b // managed
    Node3* c // managed

GCType* make_node3_type(void)
    GCType* type = gc_new_type(sizeof(Node3), 3)
    gc_set_offset(type, 0, offsetof(Node3, a))
    gc_set_offset(type, 1, offsetof(Node3, b))
    gc_set_offset(type, 2, offsetof(Node3, c))
    return type

GCType* node3_type = NULL

Node3* node3(int x, Node* a, Node* b, Node3* c)
    require_not_null(node3_type)
    Node3* node = gc_alloc_object(node3_type)
    node->x = x
    node->a = a
    node->b = b
    node->c = c
    return node

int tree3_count(Node3* t)
    if t == NULL do return 0
    return 1 + tree_count(t->a) + tree_count(t->b) + tree3_count(t->c)

int freed_count = 0
int test_freed_count = 0

void __attribute__((noinline)) test4(void)
    if node_type == NULL do
        node_type = make_node_type()
        printf("node_type = %p\n", node_type)
    if node3_type == NULL do
        node3_type = make_node3_type()
        printf("node3_type = %p\n", node3_type)

    Node3* t = node3(1,
                   node(2,
                        leaf(3),
                        leaf(4)),
                   node(5,
                        leaf(6),
                        leaf(7)),
                   node3(8,
                        leaf(9),
                        leaf(10),
                        node3(11, NULL, NULL, NULL)))

    test_equal_i(tree3_count(t), 11)
    gc_collect()
    printf("freed %d objects\n", freed_count)
    test_freed_count = freed_count
    test_equal_i(tree3_count(t), 11)
    t->c = NULL
    gc_collect()
    printf("freed %d objects\n", freed_count)
    test_freed_count += freed_count
    test_equal_i(tree3_count(t), 7)
    t->b->left = NULL
    gc_collect()
    printf("freed %d objects\n", freed_count)
    test_freed_count += freed_count
    test_equal_i(tree3_count(t), 6)

int main(void)
    PLf("frame address = %p", __builtin_frame_address(0))
    gc_set_bottom_of_stack(__builtin_frame_address(0))

    #if 0
    // Limit address space (in bytes)
    struct rlimit limit
    int err = getrlimit(RLIMIT_AS, &limit)
    printf("%d, %d, %llu, %llu\n", err, errno, limit.rlim_cur, limit.rlim_max)
    assert("no error", err == 0)

    limit.rlim_cur = 34950000000
    limit.rlim_max = RLIM_INFINITY
    err = setrlimit(RLIMIT_AS, &limit)
    printf("%d, %d, %llu, %llu\n", err, errno, limit.rlim_cur, limit.rlim_max)
    assert("no error", err == 0)

    err = getrlimit(RLIMIT_AS, &limit)
    printf("%d, %d, %llu, %llu\n", err, errno, limit.rlim_cur, limit.rlim_max)
    assert("no error", err == 0)
    #endif

    #if 1
    // Limit address space (in bytes)
    struct rlimit limit
    int err = getrlimit(RLIMIT_STACK, &limit) // 8388608
    printf("%d, %d, %llu, %llu\n", err, errno, limit.rlim_cur, limit.rlim_max)
    assert("no error", err == 0)

    limit.rlim_cur = 10000
    err = setrlimit(RLIMIT_STACK, &limit)
    printf("%d, %d, %llu, %llu\n", err, errno, limit.rlim_cur, limit.rlim_max)
    assert("no error", err == 0)

    err = getrlimit(RLIMIT_STACK, &limit)
    printf("%d, %d, %llu, %llu\n", err, errno, limit.rlim_cur, limit.rlim_max)
    assert("no error", err == 0)
    #endif

    test0()
    PLs("main")
    gc_collect()
    test_equal_i(gc_is_empty(), true)
    test1()
    gc_collect()
    test_equal_i(gc_is_empty(), true)
    test2()
    gc_collect()
    test_equal_i(gc_is_empty(), true)
    test3()
    gc_collect()
    test_equal_i(gc_is_empty(), true)
    test4()
    gc_collect()
    printf("freed %d objects\n", test_freed_count)
    test_freed_count += freed_count
    // test_equal_i(test_freed_count, 11)
    test_equal_i(gc_is_empty(), true)

    return 0
