/*
@author: Michael Rohs
@date: January 19, 2021
*/

#include <setjmp.h>
#include "util.h"
#include "trie.h"
#undef NODEBUG

/*
Macros for trie functions (see trie.h). The three least significant bits of
pointers are zero. A value in the trie must have the LSB clear. Thus can shift
right by two bits. The LSB of the result is still zero.
*/
#define tr_insert(t, x) trie_insert(t, (uint64_t)(x) >> 2, 0)
#define tr_contains(t, x) trie_contains(t, (uint64_t)(x) >> 2, 0)
#define tr_remove(t, x) trie_remove(t, (uint64_t)(x) >> 2, 0)

// Gets the offset of a struct member.
#define offsetof(type, member) ((int)__builtin_offsetof(type, member))

/*
The user sees the "object" member within the allocation structure. This macro
computes the address of the allocation object given the address of the user
object.
*/
#define allocation_address(o) ((Allocation*)((char*)(o) - offsetof(Allocation, object)))

typedef struct Type Type
typedef struct Allocation Allocation

void collect(void)

/*
Type describes an object in terms of its size and in terms of the offsets of
pointers to dynamically allocated (and managed) memory that it contains.
*/
struct Type
    int size // byte size of an object of this type
    int pointer_count // number of pointers that on object of this type contains
    int pointers[] // byte offsets of pointers

/*
Allocation represents the header of a single memory block allocated for
management by this garbage collector. If the user object is an array, then count
> 0, otherwise count == 0. The type attribute is the type of the user object or
NULL if the user object does not contain any pointers to dynamically allocated
memory (to be managed by the garbage collector). The user object starts at
offset "object".
*/
struct Allocation
    bool marked // set by mark (if this allocation is reachable) and cleared by sweep
    int count // number of array elements or 0
    Type* type // type of the user object or NULL
    char object[] // <-- user object starts here

// The trie of all allocations.
uint64_t allocations = 0

// The trie of root allocations.
uint64_t roots = 0

/*
The bottom of the call stack is set in the main function. Needed for scanning
the stack.
*/
uint64_t* bottom_of_stack

// Allocates the given number of bytes.
void* gc_alloc(int size)
    require("not negative", size >= 0)
    Allocation* a = calloc(1, sizeof(Allocation) + size)
    if a == NULL do
        // if could not get memory, collect and try again
        collect()
        // use xcalloc here to stop if fails again
        a = xcalloc(1, sizeof(Allocation) + size)
    // a->marked = false
    // a->count = 0
    // a->type = NULL
    tr_insert(&allocations, a)
    assert("inserted", tr_contains(allocations, a))
    PLf("a = %p, o = %p, type = %p", a, a->object, a->type)
    return a->object

// Allocates an object of the given type.
void* gc_alloc_object(Type* type)
    require_not_null(type)
    Allocation* a = calloc(1, sizeof(Allocation) + type->size)
    // collect() // stress test collection
    if a == NULL do
        // if could not get memory, collect and try again
        collect()
        // use xcalloc here to stop if fails again
        a = xcalloc(1, sizeof(Allocation) + type->size)
    // a->marked = false
    // a->count = 0
    a->type = type
    tr_insert(&allocations, a)
    assert("inserted", tr_contains(allocations, a))
    PLf("a = %p, o = %p, type = %p", a, a->object, a->type)
    return a->object

// Allocates an array of count objects of the given type.
void* gc_alloc_array(Type* type, int count)
    require_not_null(type)
    require("positive", count > 0)
    Allocation* a = calloc(1, sizeof(Allocation) + count * type->size)
    if a == NULL do
        // if could not get memory, collect and try again
        collect()
        // use xcalloc here to stop if fails again
        a = xcalloc(1, sizeof(Allocation) + count * type->size)
    // a->marked = false
    a->count = count
    a->type = type
    tr_insert(&allocations, a)
    assert("inserted", tr_contains(allocations, a))
    PLf("a = %p, o = %p, type = %p", a, a->object, a->type)
    return a->object

// Checks if the set of roots contains o.
bool contains_root(void* o)
    require_not_null(o)
    Allocation* a = allocation_address(o)
    return tr_contains(roots, a)

// Adds an object as a root object.
void add_root(void* o)
    require_not_null(o)
    Allocation* a = allocation_address(o)
    tr_insert(&roots, a)
    ensure("is a root", tr_contains(roots, a))

// Removes an object from the set of root objects.
void remove_root(void* o)
    require_not_null(o)
    Allocation* a = allocation_address(o)
    tr_remove(&roots, a)
    ensure("is not a root", !tr_contains(roots, a))

// Prints the current allocations.
bool f_print(uint64_t x)
    Allocation* a = (Allocation*)(x << 2)
    printf("\ta = %p, o = %p, count = %d, marked = %d\n", a, a->object, a->count, a->marked)
    return true
void print_allocations(void)
    printf("print_allocations:\n")
    if trie_is_empty(allocations) do
        printf("\tno allocations\n")
    else
        trie_visit(&allocations, f_print)

/*
Allocates a new type with the given size of the user object and the given number
of pointers to dynamically allocated content that is managed by the garbage
collector. Type itself is dynamically allocated, but not garbage collected. The
size should respect the requirements of alignment, if used with arrays. The
pointer table is initialized to zeros.
*/
Type* new_type(int size, int pointer_count)
    require("not negative", size >= 0)
    require("not negative", pointer_count >= 0)
    Type* t = xcalloc(1, sizeof(Type) + pointer_count * sizeof(int))
    t->size = size
    t->pointer_count = pointer_count
    return t

// Sets the offset of i-th the pointer to managed memory.
void type_set_offset(Type* type, int index, int offset)
    require_not_null(type)
    require("valid index", 0 <= index && index < type->pointer_count)
    require("valid offset", 0 <= offset && offset + sizeof(void*) <= type->size)
    type->pointers[index] = offset

/*
Sweeps the marked allocations and either clears the mark or deletes the
allocation.
*/
bool f_sweep(uint64_t x)
    Allocation* a = (Allocation*)(x << 2)
    if a->marked do
        a->marked = false
        return true // keep
    else
        PLf("free %p", a)
        free(a)
        return false // remove
void sweep(void)
    trie_visit(&allocations, f_sweep)

// Marks all objects reachable from o, including o itself.
void mark(void* o) // todo: should call with allocation as argument?
    assert_not_null(o)
    Allocation* a = allocation_address(o)
    PLf("marking o = %p, a = %p, count = %d, marked = %d", o, a, a->count, a->marked)
    if a->marked do return
    a->marked = true
    Type* t = a->type
    if t == NULL do return
    char* element = a->object
    int n = a->count
    if n == 0 do n = 1
    int m = t->pointer_count
    int element_size = t->size
    int* pointers = t->pointers
    for int i = 0; i < n; i++ do // for all elements
        for int j = 0; j < m; j++ do // for each pointer in i-th element
            int p = pointers[j]
            char* pj = *(char**)(element + p)
            if pj != NULL do mark(pj)
        element += element_size

// Marks all root objects and all objects that are reachable from them.
bool f_mark_roots(uint64_t x)
    Allocation* r = (Allocation*)(x << 2)
    mark(r->object)
    return true // keep
void mark_roots(void)
    trie_visit(&roots, f_mark_roots)

// Scan the stack for pointers to allocations.
void mark_stack(void)
    // https://gcc.gnu.org/onlinedocs/gcc/Return-Address.html
    // https://en.wikipedia.org/wiki/Setjmp.h
    jmp_buf buf // 148 bytes (apple-darwin21.2.0, clang 13.0.0)
    memset(&buf, 0, sizeof(jmp_buf))
    setjmp(buf) // save the contents of the registers
    uint64_t* top_of_stack = (uint64_t*)&buf
    assert("aligned pointer", (*top_of_stack & 7) == 0)
    PLf("bottom_of_stack = %p", bottom_of_stack)
    PLf("top_of_stack    = %p", top_of_stack)
    assert("stack grows down", top_of_stack < bottom_of_stack)
    for uint64_t* p = top_of_stack; p < bottom_of_stack; p++ do
        // is the value on the stack at address p a valid allocation?
        // if so, the stack contains the user part, need to subtract
        // offset of object in Allocation
        Allocation* a = allocation_address(*p)
        uint64_t x = (uint64_t)a
        if x != 0 && x != 0xfffffffffffffff0 && (x & 7) == 0 do 
            bool is_allocation = tr_contains(allocations, a)
            PLf("p = %p, a = %p, is alloc = %d", p, a, is_allocation)
            if is_allocation do
                mark(a->object)

/*
Called when it is necessary to collect garbage. The stack is automatically
searched for pointers to garbage-collected memory. Moreover, objects that have
explicitly been added as root objects are also scanned.
*/
void collect(void)
    mark_stack()
    mark_roots()
    sweep()

/////////////////////////////////////////////////
// Test code below.

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
Type* make_a_type(void)
    Type* type = new_type(sizeof(A), 1)
    type_set_offset(type, 0, offsetof(A, t))
    return type

// Singleton type object for objects of type A.
Type* a_type

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
Type* make_b_type(void)
    Type* type = new_type(sizeof(B), 1)
    type_set_offset(type, 0, offsetof(B, a))
    return type

// Singleton type object for objects of type B.
Type* b_type

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

Type* make_node_type(void)
    Type* type = new_type(sizeof(Node), 2)
    type_set_offset(type, 0, offsetof(Node, left))
    type_set_offset(type, 1, offsetof(Node, right))
    return type

Type* node_type

Node* node(int i, Node* left, Node* right)
    Node* node = gc_alloc_object(node_type)
    // collect() // stress test collection
    node->i = i
    PLi(i)
    node->left = left
    node->right = right
    return node

Node* leaf(int i)
    return node(i, NULL, NULL)

void print_tree(Node* t)
    if t != NULL do
        printf("o = %p, a = %p, i = %d\n", t, allocation_address(t), t->i)
        print_tree(t->left)
        print_tree(t->right)

int sum_tree(Node* t)
    if t == NULL do return 0
    return sum_tree(t->left) + t->i + sum_tree(t->right)

// Test function.
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
    print_allocations()
    //mark(a1->t)
    // mark(a1)
    // mark(b); print_allocations()
    // sweep(); print_allocations()
    add_root(b)
    //add_root(a2)
    // remove_root(a2)
    // collect()
    mark_roots(); print_allocations()
    sweep(); print_allocations()

    B* bs = gc_alloc_array(b_type, 3)
    Allocation* al = allocation_address(bs)
    printf("%p, %p\n", al, allocation_address(a1))
    PLi(al->count)
    PLi(al->type->pointer_count)
    for int i = 0; i < 3; i++ do
        B* bi = bs + i
        bi->j = i
        bi->a = a1
        print_b(bi)
    print_allocations()
    mark(bs)
    print_allocations()

    free(a_type)
    free(b_type)

int tree_sum(void)
    Node* t = node(1, node(2, leaf(3), leaf(4)), node(5, leaf(6), leaf(7)))
    // add_root(t)
    int n = sum_tree(t)
    // remove_root(t)
    return n

void test(void)
    node_type = make_node_type()
    printf("node_type = %p\n", node_type)
    int n = tree_sum()
    printf("n = %d\n", n)
    assert("correct sum", n == 1+2+3+4+5+6+7)
    collect()
    print_allocations()
    free(node_type)

void test2(void)
    node_type = make_node_type()
    printf("node_type = %p\n", node_type)
    //Node* t = NULL
    Node* t = node(1,
                   node(2, leaf(3), leaf(4)),
                   node(5, leaf(6), leaf(7)))
    print_tree(t)
    add_root(t)
    //t = NULL
    //t->left = NULL
    //t->left->left = NULL
    t->left->left->left = t // handle cycles
    //t->right->right->right = t
    //add_root(t->left)
    //add_root(t->right)
    //add_root(t->right->left)
    print_allocations()
    mark_roots(); print_allocations()
    sweep(); print_allocations()
    Node* s = leaf(123)
    t = leaf(124)
    t = NULL
    print_allocations()
    printf("&s = %p\n", &s)
    //mark_stack();
    print_allocations()
    s = NULL
    collect()
    print_allocations()
    free(node_type)

void test_alignment(void)
    // test address alignment on the stack
    assert("aligned pointer", (*bottom_of_stack & 7) == 0)
    assert("valid pointer size", sizeof(uint64_t) == sizeof(void*))

    char* s = "x"
    char c = 1
    char d = 2
    char* t = "t"
    assert("", ((uint64_t)&s & 0x7) == 0)
    assert("", ((uint64_t)&t & 0x7) == 0)
    assert("", (uint64_t)&s - (uint64_t)&t == 0x10)
    PLf("&s = %p, &c = %p, &d = %p, &t = %p, %llu", 
        &s, &c, &d, &t, (uint64_t)&c - (uint64_t)&t)

    int i = 1
    char e = 2
    int j = 3
    assert("", ((uint64_t)&i & 0x3) == 0)
    assert("", ((uint64_t)&j & 0x3) == 0)
    assert("", (uint64_t)&i - (uint64_t)&j == 8)
    PLf("&i = %p, &e = %p, &j = %p, %llu", 
        &i, &e, &j, (uint64_t)&i - (uint64_t)&j)

    short f = 1
    char g = 2
    short h = 3
    assert("", ((uint64_t)&f & 0x1) == 0)
    assert("", ((uint64_t)&h & 0x1) == 0)
    assert("", (uint64_t)&f - (uint64_t)&h == 4)
    PLf("&f = %p, &g = %p, &h = %p, %llu", 
        &f, &g, &h, (uint64_t)&f - (uint64_t)&h)

    double k = 1
    char l = 2
    double m = 3
    assert("", ((uint64_t)&k & 0x7) == 0)
    assert("", ((uint64_t)&m & 0x7) == 0)
    assert("", (uint64_t)&k - (uint64_t)&m == 0x10)
    PLf("&k = %p, &l = %p, &m = %p, %llu", 
        &k, &l, &m, (uint64_t)&k - (uint64_t)&m)

int main(int argc, char* argv[])
    bottom_of_stack = (uint64_t*)&argv
    assert("aligned pointer", (*bottom_of_stack & 7) == 0)
    test()
    test_alignment()
    return 0
