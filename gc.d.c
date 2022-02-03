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

// Checks whether a is not NULL and 16-byte-aligned.
#define is_alloc_aligned(a) ((a) != NULL && ((uint64_t)(a) & 0xf) == 0)

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
>= 1, otherwise count == 1. The type attribute is the type of the user object or
NULL if the user object does not contain any pointers to dynamically allocated
memory (to be managed by the garbage collector). The user object starts at
offset "object".
*/
struct Allocation
    bool marked // set by mark (if this allocation is reachable) and cleared by sweep
    int count // number of array elements (>= 1) or 1
    int i, j // loop state, used in mark to avoid recursion
    Allocation* parent // predecessor in implicit stack, used in mark to avoid recursion
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
    // gc_collect() // stress test collection
    if a == NULL do
        // if could not get memory, collect and try again
        gc_collect()
        // use xcalloc here to stop if fails again
        a = xcalloc(1, sizeof(Allocation) + size)
    // a->marked = false
    a->count = 1
    // a->type = NULL
    assert("is aligned", is_alloc_aligned(a))
    tr_insert(&allocations, a)
    PLf("a = %p, o = %p, type = %p", a, a->object, a->type)
    ensure("inserted", tr_contains(allocations, a))
    return a->object

// Allocates an object of the given type.
*void* gc_alloc_object(GCType* type)
    require_not_null(type)
    Allocation* a = calloc(1, sizeof(Allocation) + type->size)
    // stress test collection
    // static int counter = 0
    // counter++; if (counter % 11) == 1 do gc_collect()
    if a == NULL do
        // if could not get memory, collect and try again
        gc_collect()
        // use xcalloc here to stop if fails again
        a = xcalloc(1, sizeof(Allocation) + type->size)
    // a->marked = false
    a->count = 1
    a->type = type
    assert("is aligned", is_alloc_aligned(a))
    tr_insert(&allocations, a)
    PLf("a = %p, o = %p, type = %p", a, a->object, a->type)
    ensure("inserted", tr_contains(allocations, a))
    return a->object

// Allocates an array of count objects of the given type.
*void* gc_alloc_array(GCType* type, int count)
    require_not_null(type)
    require("positive", count > 0)
    Allocation* a = calloc(1, sizeof(Allocation) + count * type->size)
    // gc_collect() // stress test collection
    if a == NULL do
        // if could not get memory, collect and try again
        gc_collect()
        // use xcalloc here to stop if fails again
        a = xcalloc(1, sizeof(Allocation) + count * type->size)
    // a->marked = false
    a->count = count
    a->type = type
    assert("is aligned", is_alloc_aligned(a))
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
    return is_alloc_aligned(a) && tr_contains(roots, a)

// Adds an object as a root object.
*void gc_add_root(void* o)
    require_not_null(o)
    Allocation* a = allocation_address(o)
    assert("is aligned", is_alloc_aligned(a))
    assert("is allocation", tr_contains(allocations, a))
    tr_insert(&roots, a)
    ensure("is a root", tr_contains(roots, a))

// Removes an object from the set of root objects.
*void gc_remove_root(void* o)
    require_not_null(o)
    Allocation* a = allocation_address(o)
    assert("is aligned", is_alloc_aligned(a))
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
    PLf("frame address = %p", __builtin_frame_address(0))
    require_not_null(a)
    require("is allocation", is_alloc_aligned(a) && tr_contains(allocations, a))
    PLf("marking o = %p, a = %p, count = %d, marked = %d", a->object, a, a->count, a->marked)
    if a->marked do return
    a->marked = true
    GCType* t = a->type
    if t == NULL do return
    a->parent = NULL
    a->i = 0; a->j = 0
    while a != NULL do
        int i = a->i, j = a->j
        while i < a->count do // for all elements
            while j < t->pointer_count do // for each pointer in i-th element
                PLf("i = %d, j = %d", i, j)
                int offset = t->pointers[j]
                char* pj = *(char**)(a->object + i * t->size + offset)
                if pj != NULL do
                    Allocation* aj = allocation_address(pj)
                    assert("is allocation", is_alloc_aligned(aj) && tr_contains(allocations, aj))
                    PLf("pj = %p, a = %p, count = %d, marked = %d\n", pj, aj, aj->count, aj->marked)
                    // mark(aj) <-- avoid recursion, capture loop state and process aj
                    if !aj->marked do
                        aj->marked = true
                        if aj->type != NULL do
                            a->i = i; a->j = j + 1; aj->parent = a
                            a = aj; t = a->type
                            i = -1; break
                j++
            j = 0; i++
        a = a->parent
        if a != NULL do t = a->type

// Marks all root objects and all objects that are reachable from them.
bool f_mark_roots(uint64_t x)
    PLf("%llx", x << 3)
    Allocation* r = (Allocation*)(x << 3)
    mark(r)
    return true // keep
void mark_roots(void)
    trie_visit(&roots, f_mark_roots)

/*
Marks registers and returns its own frame address. mark_registers has its own
stack frame (noinline). It is called by mark_stack and thus its frame address is
below (in memory) the frame of mark_stack. The frame address of mark_registers
serves as the top_of_stack (lowest address) for scanning the stack.
*/
uint64_t* __attribute__((noinline)) mark_registers(void)
    // https://gcc.gnu.org/onlinedocs/gcc/Return-Address.html
    uint64_t* top_of_stack = __builtin_frame_address(0)
    PLf("frame address = %p", __builtin_frame_address(0))

    /* As a result of optimization (-fomit-frame-pointer) the frame pointer
    register (rbp) may be used as a regular register. In this case the frame
    pointer register may contain a pointer to a managed object and thus has to
    be scanned. Unfortunately, setjmp mangles rbp for security reasons. Thus it
    is scanned explicitly. */
    uint64_t rbp = 0
    __asm__ ("movq %%rbp, %0" : "=r"(rbp))
    PLf("rbp = %llx", rbp)
    if rbp != 0 do
        Allocation* a = allocation_address(rbp)
        if is_alloc_aligned(a) && tr_contains(allocations, a) do
            PLf("found allocation: rbp = %llx, a = %p", rbp, a)
            mark(a)

    /* https://en.wikipedia.org/wiki/Setjmp.h
    /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/setjmp.h
    Registers may contain pointers to managed objects. On x86-64/macOS setjmp
    saves callee-saved registers (rbp, rsp, rbx, r12, r13, r14, r15). It mangles
    rbp and rsp for security reasons. Caller-saved registers have already been
    saved at the call-site, which is a stack frame below this stack frame in
    client code (outside this gc). The stack frames of client code need to be
    scanned completely, up to the bottom of the stack. */
    jmp_buf buf // 148 bytes for x86-64/macOS
    assert("aligned buf", ((uint64_t)&buf & 7) == 0)
    memset(&buf, 0, sizeof(jmp_buf))
    setjmp(buf) // save the contents of callee-saved registers
    uint64_t* p = (uint64_t*)buf
    uint64_t* q = p + sizeof(jmp_buf) / sizeof(uint64_t)
    for ; p < q; p++ do
        if *p != 0 do
            Allocation* a = allocation_address(*p)
            if is_alloc_aligned(a) && tr_contains(allocations, a) do
                PLf("found allocation: p = %p, a = %p", p, a)
                mark(a)
    ensure("aligned pointer", top_of_stack != NULL && ((uint64_t)top_of_stack & 7) == 0)
    return top_of_stack

// Scan the stack for pointers to allocations.
void mark_stack(void)
    PLf("frame address = %p", __builtin_frame_address(0))
    uint64_t* top_of_stack = mark_registers()
    assert_not_null(bottom_of_stack)
    PLf("bottom_of_stack = %p", bottom_of_stack)
    PLf("top_of_stack    = %p %ld", top_of_stack, bottom_of_stack - top_of_stack)
    assert("stack grows down", top_of_stack < bottom_of_stack)
    for uint64_t* p = top_of_stack; p < bottom_of_stack; p++ do
        // PLf("p = %p", p)
        // is the value on the stack at address p a valid allocation?
        // if so, the stack contains the user part, need to subtract
        // offset of object in Allocation
        if *p != 0 do
            Allocation* a = allocation_address(*p)
            if is_alloc_aligned(a) && tr_contains(allocations, a) do 
                PLf("found allocation: p = %p, a = %p", p, a)
                mark(a)

/*
Called when it is necessary to collect garbage. The stack is automatically
searched for pointers to garbage-collected memory. Moreover, objects that have
explicitly been added as root objects are also scanned. This function may also
be called manually by clients.
*/
*void gc_collect(void)
    PLf("frame address = %p", __builtin_frame_address(0))
    mark_stack()
    mark_roots()
    // PL; print_allocations()
    sweep()
    // PL; print_allocations()

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

int xmain(void)
    gc_set_bottom_of_stack(__builtin_frame_address(0))
    test_alignment()
    return 0
