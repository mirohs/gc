/*
@author: Michael Rohs
@date: January 19, 2022
*/

#define NO_DEBUG
// #define NO_ASSERT
// #define NO_REQUIRE
// #define NO_ENSURE

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

// int allocated_nodes = 0

*void gc_collect(void)

Node* new_node(void)
    // allocated_nodes++
    // return xcalloc(1, sizeof(Node))
    Node* node = calloc(1, sizeof(Node))
    if node == NULL do
        // if could not get memory, collect and try again
        gc_collect()
        // use xcalloc here to stop if fails again
        node = xcalloc(1, sizeof(Node))
    return node

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
    if x == 0 do return false
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
    assert("is another value", !is_empty(t) && is_value(t) && x != t)
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

*typedef bool (*TrieVisitFn)(uint64_t x)

*void trie_visit(uint64_t* t, TrieVisitFn f)
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
