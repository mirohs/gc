# gc

## A simple mark-and-sweep garbage collector.

This is a simple mark-and-sweep [garbage
collector](https://en.wikipedia.org/wiki/Garbage_collection_(computer_science))
that is backed by a [trie](https://en.wikipedia.org/wiki/Trie). The runtime
stack is automatically scanned for pointers to managed memory. Moreover,
additional root objects may be added, e.g. for objects that are stored in static
or file-level variables. The garbage collector is provided with information
about the structure of objects to make the scanning of the object graph
reasonably efficient. To this end a type descriptor tells the garbage collector
at which offsets within structures to find pointers to managed memory.

The garbage collector is implemented in debraced C. Debraced C is C with
optional braces, significant indentation, and automatic generation of header
files. The programmer writes code with optional braces in a `.d.c` (debraced C)
file. Public items are marked with a `*` at the beginning of the line. Public
items are exported from the translation unit and included in the module's header
file. The compilation process is automated by `make`.

The tool [embrace](https://github.com/mirohs/embrace) reintroduces braces into
the debraced C file and the tool [headify](https://github.com/mirohs/headify)
automatically generates header files. Both tools have to be available to compile
this project.

## Building

```sh
git clone https://github.com/mirohs/gc.git
git clone https://github.com/mirohs/embrace.git
git clone https://github.com/mirohs/headify.git
cd embrace
make
cd ../headify
make
cd ../gc
make
```

## Running the Tests

```sh
./gc
```

## API

The garbage collector has the following API.

```c
void* gc_alloc(int size);
void* gc_alloc_object(int type);
void* gc_alloc_array(int type, int count);

void gc_set_bottom_of_stack(void* bos);

int gc_new_type(int size, int pointer_count);
void gc_set_offset(int type, int index, int offset);
#define offsetof(type, member) ((int)__builtin_offsetof(type, member))

void gc_add_root(void* o);
void gc_remove_root(void* o);
bool gc_contains_root(void* o);

bool gc_is_empty(void);
void gc_collect(void);
```

## Example Usage

The following example illustrates how the garbage collector is used in a client
program.

```c
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
int make_a_type(void)
    int type = gc_new_type(sizeof(A), 1)
    gc_set_offset(type, 0, offsetof(A, t))
    return type

// Singleton type object for objects of type A.
int a_type = 0

// Creates and initializes a new instance of type A.
A* new_a(int i, char* s, char* t)
    require_not_null(s)
    require_not_null(t)
    A* a = gc_alloc_object(a_type)
    a->i = i
    a->s = s
    a->t = new_str(t) // create a managed copy
    return a

// Prints an A object.
void print_a(A* a)
    require_not_null(a)
    printf("A(%d, %s, %s)\n", a->i, a->s, a->t)

void test0(void)
    if a_type == 0 do
        a_type = make_a_type()
        printf("a_type = %d\n", a_type)

    A* a1 = new_a(5, "hello", "world")
    print_a(a1)

    A* a2 = new_a(7, "abc", "def")
    print_a(a2)

    a2 = NULL // no more references to a2, may be garbage collected
    gc_collect() // may explicitly collect, typically not needed
    test_equal_i(gc_is_empty(), false)

int main(void)
    gc_set_bottom_of_stack(__builtin_frame_address(0))
    test0()
    gc_collect()
    test_equal_i(gc_is_empty(), true)
    return 0
```
