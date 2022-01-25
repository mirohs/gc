/*
@author: Michael Rohs
@date: January 19, 2021
*/

#define NO_DEBUG
// #define NO_ASSERT
// #define NO_REQUIRE
// #define NO_ENSURE

#include <setjmp.h>
#include <time.h>
#include "util.h"
#include "trie.h"
#include "gc.h"

// A string is an object without pointers to managed memory, so its type is NULL.
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

// A needs a type, bacause it contains one pointer to managed memory (t).
GCType* make_a_type(void)
    GCType* type = gc_new_type(sizeof(A), 1)
    gc_set_offset(type, 0, offsetof(A, t))
    return type

// Singleton type object for objects of type A.
GCType* a_type

// Creates and initializes a new instance of type A.
A* new_a(int i, char* s, char* t)
    require_not_null(s)
    require_not_null(t)
    A* a = gc_alloc_object(a_type)
    a->i = i
    a->s = s
    a->t = new_str(t)
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
GCType* b_type

// Creates and initializes a new instance of type B.
B* new_b(int j, A* a)
    require_not_null(a)
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

void test0(void)
    a_type = make_a_type()
    printf("a_type = %p\n", a_type)
    b_type = make_b_type()
    printf("b_type = %p\n", b_type)

    A* a1 = new_a(5, "hello", "world")
    print_a(a1)
    B* b = new_b(3, a1)
    print_b(b)

    A* a2 = new_a(7, "abc", "def")
    gc_add_root(b)
    gc_add_root(a2)
    gc_remove_root(b)
    gc_remove_root(a2)
    // a1 = NULL;
    a2 = NULL; b = NULL

    B* bs = gc_alloc_array(b_type, 3)
    for int i = 0; i < 3; i++ do
        B* bi = bs + i
        bi->j = i
        bi->a = a1
        print_b(bi)
    for int i = 0; i < 3; i++ do
        test_equal_i(bs[i].j, i)
        test_equal_i(bs[i].a->i, 5)
    bs = NULL; a1 = NULL

    free(a_type)
    free(b_type)

int tree_sum(void)
    Node* t = node(1, node(2, leaf(3), leaf(4)), node(5, leaf(6), leaf(7)))
    // gc_add_root(t)
    int n = sum_tree(t)
    // gc_remove_root(t)
    return n

void test1(void)
    node_type = make_node_type()
    printf("node_type = %p\n", node_type)
    int n = tree_sum()
    printf("n = %d\n", n)
    test_equal_i(n, 1+2+3+4+5+6+7)
    gc_collect()
    // print_allocations()
    free(node_type)

void test2(void)
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
    free(node_type)

Node* fill_tree(int i)
    if i <= 0 do
        return NULL
    else
        return node(i, fill_tree(i - 1), fill_tree(i - 2))

void test3(void)
    node_type = make_node_type()
    printf("node_type = %p\n", node_type)
    Node* t = NULL
    /*
    for int i = 0; i < 100000; i++ do
        t = node(i, t, NULL)
        // t = NULL
    */
    clock_t time = clock();
    t = fill_tree(30)
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
    free(node_type)

int main(int argc, char* argv[])
    gc_set_bottom_of_stack(&argv)
    test0()
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
    return 0

