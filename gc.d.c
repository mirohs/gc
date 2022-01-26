/*
@author: Michael Rohs
@date: January 19, 2022
*/

#define NO_DEBUG
// #define NO_ASSERT
// #define NO_REQUIRE
// #define NO_ENSURE

#include <setjmp.h>
#include "util.h"
#include "trie.h"
#include "gc.h"

/*
Macros for trie functions (see trie.h). The four least significant bits of
pointers to dynamically allocated memory are zero. A value in the trie must
have the LSB clear. Thus the pointer value can be shifted right by three
bits. The LSB of the result is still zero.
*/
#define tr_insert(t, x) trie_insert(t, (uint64_t)(x) >> 3, 0)
#define tr_contains(t, x) trie_contains(t, (uint64_t)(x) >> 3, 0)
#define tr_remove(t, x) trie_remove(t, (uint64_t)(x) >> 3, 0)

// Gets the offset of a struct member.
*#define offsetof(type, member) ((int)__builtin_offsetof(type, member))

/*
The user sees the "object" member within the allocation structure. This macro
computes the address of the allocation object given the address of the user
object.
*/
#define allocation_address(o) ((Allocation*)((char*)(o) - offsetof(Allocation, object)))

*typedef struct GCType GCType
typedef struct Allocation Allocation

*void gc_collect(void)

/*
GCType describes an object in terms of its size and in terms of the offsets of
pointers to dynamically allocated (and managed) memory that it contains.
*/
struct GCType
    int size // byte size of an object of this type
    int pointer_count // number of pointers that an object of this type contains
    int pointers[] // byte offsets of pointers

/*
Allocation represents the header of a single memory block that is allocated for
management by this garbage collector. If the user object is an array, then count
> 0, otherwise count == 0. The type attribute is the type of the user object or
NULL if the user object does not contain any pointers to dynamically allocated
memory (to be managed by the garbage collector). The user object starts at
offset "object".
*/
struct Allocation
    bool marked // set by mark (if this allocation is reachable) and cleared by sweep
    int count // number of array elements or 0
    GCType* type // type of the user object or NULL
    char object[] // <-- user object starts here

// The trie of all allocations.
uint64_t allocations = 0

// The trie of root allocations.
uint64_t roots = 0

/*
The bottom of the call stack is set in the initialization (or main) function.
Needed for scanning the stack.
*/
uint64_t* bottom_of_stack = NULL

*void gc_set_bottom_of_stack(void* bos)
    require_not_null(bos)
    require("aligned pointer", ((uint64_t)bos & 7) == 0)
    bottom_of_stack = bos

// Allocates the given number of bytes.
*void* gc_alloc(int size)
    require("not negative", size >= 0)
    Allocation* a = calloc(1, sizeof(Allocation) + size)
    if a == NULL do
        // if could not get memory, collect and try again
        gc_collect()
        // use xcalloc here to stop if fails again
        a = xcalloc(1, sizeof(Allocation) + size)
    // a->marked = false
    // a->count = 0
    // a->type = NULL
    assert("allocation is aligned", a != NULL && ((uint64_t)a & 0xf) == 0)
    tr_insert(&allocations, a)
    PLf("a = %p, o = %p, type = %p", a, a->object, a->type)
    ensure("inserted", tr_contains(allocations, a))
    return a->object

// Allocates an object of the given type.
*void* gc_alloc_object(GCType* type)
    require_not_null(type)
    Allocation* a = calloc(1, sizeof(Allocation) + type->size)
    // gc_collect() // stress test collection
    if a == NULL do
        // if could not get memory, collect and try again
        gc_collect()
        // use xcalloc here to stop if fails again
        a = xcalloc(1, sizeof(Allocation) + type->size)
    // a->marked = false
    // a->count = 0
    a->type = type
    assert("allocation is aligned", a != NULL && ((uint64_t)a & 0xf) == 0)
    tr_insert(&allocations, a)
    PLf("a = %p, o = %p, type = %p", a, a->object, a->type)
    ensure("inserted", tr_contains(allocations, a))
    return a->object

// Allocates an array of count objects of the given type.
*void* gc_alloc_array(GCType* type, int count)
    require_not_null(type)
    require("positive", count > 0)
    Allocation* a = calloc(1, sizeof(Allocation) + count * type->size)
    if a == NULL do
        // if could not get memory, collect and try again
        gc_collect()
        // use xcalloc here to stop if fails again
        a = xcalloc(1, sizeof(Allocation) + count * type->size)
    // a->marked = false
    a->count = count
    a->type = type
    assert("allocation is aligned", a != NULL && ((uint64_t)a & 0xf) == 0)
    tr_insert(&allocations, a)
    PLf("a = %p, o = %p, type = %p", a, a->object, a->type)
    ensure("inserted", tr_contains(allocations, a))
    return a->object

*bool gc_is_empty(void)
    return trie_is_empty(allocations)

// Checks if the set of roots contains o.
*bool gc_contains_root(void* o)
    require_not_null(o)
    Allocation* a = allocation_address(o)
    return a != NULL && ((uint64_t)a & 0xf) == 0 && tr_contains(roots, a)

// Adds an object as a root object.
*void gc_add_root(void* o)
    require_not_null(o)
    Allocation* a = allocation_address(o)
    assert("allocation is aligned", a != NULL && ((uint64_t)a & 0xf) == 0)
    tr_insert(&roots, a)
    ensure("is a root", tr_contains(roots, a))

// Removes an object from the set of root objects.
*void gc_remove_root(void* o)
    require_not_null(o)
    Allocation* a = allocation_address(o)
    assert("allocation is aligned", a != NULL && ((uint64_t)a & 0xf) == 0)
    tr_remove(&roots, a)
    ensure("is not a root", !tr_contains(roots, a))

// Prints the current allocations.
bool f_print(uint64_t x)
    Allocation* a = (Allocation*)(x << 3)
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
collector. GCType itself is dynamically allocated, but not garbage collected. The
size should respect the requirements of alignment, if used with arrays. The
pointer table is initialized to zeros.
*/
*GCType* gc_new_type(int size, int pointer_count)
    require("not negative", size >= 0)
    require("not negative", pointer_count >= 0)
    GCType* t = xcalloc(1, sizeof(GCType) + pointer_count * sizeof(int))
    t->size = size
    t->pointer_count = pointer_count
    return t

// Sets the offset of i-th the pointer to managed memory.
*void gc_set_offset(GCType* type, int index, int offset)
    require_not_null(type)
    require("valid index", 0 <= index && index < type->pointer_count)
    require("valid offset", 0 <= offset && offset + sizeof(void*) <= type->size)
    type->pointers[index] = offset

/*
Sweeps the marked allocations and either clears the mark or deletes the
allocation.
*/
bool f_sweep(uint64_t x)
    Allocation* a = (Allocation*)(x << 3)
    if a->marked do
        a->marked = false
        return true // keep
    else
        PLf("free a = %p, o = %p", a, a->object)
        // printf("free a = %p, o = %p\n", a, a->object)
        free(a)
        return false // remove
void sweep(void)
    trie_visit(&allocations, f_sweep)

// Marks all allocations reachable from a, including a itself.
void mark(Allocation* a)
    assert_not_null(a)
    PLf("marking o = %p, a = %p, count = %d, marked = %d", a->object, a, a->count, a->marked)
    if a->marked do return
    a->marked = true
    GCType* t = a->type
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
            if pj != NULL do
                mark(allocation_address(pj))
        element += element_size

// Marks all root objects and all objects that are reachable from them.
bool f_mark_roots(uint64_t x)
    Allocation* r = (Allocation*)(x << 3)
    mark(r)
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
    assert("aligned pointer", ((uint64_t)top_of_stack & 7) == 0)
    assert_not_null(bottom_of_stack)
    PLf("bottom_of_stack = %p", bottom_of_stack)
    PLf("top_of_stack    = %p", top_of_stack)
    assert("stack grows down", top_of_stack < bottom_of_stack)
    for uint64_t* p = top_of_stack; p < bottom_of_stack; p++ do
        // is the value on the stack at address p a valid allocation?
        // if so, the stack contains the user part, need to subtract
        // offset of object in Allocation
        if *p != 0 do
            Allocation* a = allocation_address(*p)
            if a != NULL && ((uint64_t)a & 0xf) == 0 do 
                bool is_allocation = tr_contains(allocations, a)
                PLf("p = %p, a = %p, is alloc = %d", p, a, is_allocation)
                if is_allocation do
                    mark(a)

/*
Called when it is necessary to collect garbage. The stack is automatically
searched for pointers to garbage-collected memory. Moreover, objects that have
explicitly been added as root objects are also scanned. This function may also
be called manually by clients.
*/
*void gc_collect(void)
    mark_stack()
    mark_roots()
    sweep()

void test_alignment(void)
    // test address alignment on the stack
    assert("aligned pointer", ((uint64_t)bottom_of_stack & 7) == 0)
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

int xmain(int argc, char* argv[])
    gc_set_bottom_of_stack(&argv)
    test_alignment()
    return 0
