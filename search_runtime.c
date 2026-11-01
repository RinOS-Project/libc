/* SPDX-License-Identifier: MIT */
/* POSIX hash, binary-tree, linear-search, and intrusive-queue runtime. */

#include "search.h"

#include "errno.h"
#include "limits.h"
#include "stdint.h"
#include "stdlib.h"
#include "string.h"

typedef struct RinHSearchSlot {
    ENTRY entry;
    uint64_t hash;
    int occupied;
} RinHSearchSlot;

typedef struct RinHSearchTable {
    size_t capacity;
    RinHSearchSlot* slots;
} RinHSearchTable;

typedef struct RinSearchTreeNode {
    void* key;
    struct RinSearchTreeNode* left;
    struct RinSearchTreeNode* right;
    struct RinSearchTreeNode* parent;
} RinSearchTreeNode;

static RinHSearchTable* g_hsearch_table;
static volatile uint32_t g_hsearch_lock;

static void rin_hsearch_lock(void)
{
    while (__atomic_exchange_n(&g_hsearch_lock, 1u, __ATOMIC_ACQUIRE) != 0u) {
    }
}

static void rin_hsearch_unlock(void)
{
    __atomic_store_n(&g_hsearch_lock, 0u, __ATOMIC_RELEASE);
}

static uint64_t rin_hsearch_hash(const char* key)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    while (*key != '\0') {
        hash ^= (uint8_t)*key++;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

int hcreate(size_t nel)
{
    size_t required;
    size_t capacity = 8u;
    RinHSearchTable* table;
    RinHSearchSlot* slots;
    rin_hsearch_lock();
    if (g_hsearch_table != NULL) {
        rin_hsearch_unlock();
        errno = ENOMEM;
        return 0;
    }
    rin_hsearch_unlock();
    if (nel > SIZE_MAX / 2u) {
        errno = ENOMEM;
        return 0;
    }
    required = nel * 2u;
    if (required < 8u) required = 8u;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2u) {
            errno = ENOMEM;
            return 0;
        }
        capacity *= 2u;
    }
    if (capacity > SIZE_MAX / sizeof(*slots)) {
        errno = ENOMEM;
        return 0;
    }
    slots = (RinHSearchSlot*)calloc(capacity, sizeof(*slots));
    if (slots == NULL) {
        errno = ENOMEM;
        return 0;
    }
    table = (RinHSearchTable*)malloc(sizeof(*table));
    if (table == NULL) {
        free(slots);
        errno = ENOMEM;
        return 0;
    }
    table->capacity = capacity;
    table->slots = slots;
    rin_hsearch_lock();
    if (g_hsearch_table != NULL) {
        rin_hsearch_unlock();
        free(slots);
        free(table);
        errno = ENOMEM;
        return 0;
    }
    g_hsearch_table = table;
    rin_hsearch_unlock();
    return 1;
}

void hdestroy(void)
{
    RinHSearchTable* table;
    rin_hsearch_lock();
    table = g_hsearch_table;
    g_hsearch_table = NULL;
    rin_hsearch_unlock();
    if (table == NULL) return;
    free(table->slots);
    free(table);
}

ENTRY* hsearch(ENTRY item, ACTION action)
{
    RinHSearchTable* table;
    uint64_t hash;
    size_t slot;
    size_t probe;
    if (item.key == NULL || (action != FIND && action != ENTER)) {
        errno = EINVAL;
        return NULL;
    }
    rin_hsearch_lock();
    table = g_hsearch_table;
    if (table == NULL) {
        errno = EINVAL;
        rin_hsearch_unlock();
        return NULL;
    }
    hash = rin_hsearch_hash(item.key);
    slot = (size_t)hash & (table->capacity - 1u);
    for (probe = 0u; probe < table->capacity; ++probe) {
        RinHSearchSlot* candidate = &table->slots[slot];
        if (!candidate->occupied) {
            if (action == FIND) {
                rin_hsearch_unlock();
                return NULL;
            }
            candidate->entry = item;
            candidate->hash = hash;
            candidate->occupied = 1;
            rin_hsearch_unlock();
            return &candidate->entry;
        }
        if (candidate->hash == hash &&
            strcmp(candidate->entry.key, item.key) == 0) {
            rin_hsearch_unlock();
            return &candidate->entry;
        }
        slot = (slot + 1u) & (table->capacity - 1u);
    }
    errno = ENOMEM;
    rin_hsearch_unlock();
    return NULL;
}

static RinSearchTreeNode* rin_tsearch_find(
    const void* key, RinSearchTreeNode* node,
    int (*compar)(const void*, const void*))
{
    while (node != NULL) {
        const int order = compar(key, node->key);
        if (order == 0) return node;
        node = order < 0 ? node->left : node->right;
    }
    return NULL;
}

static RinSearchTreeNode* rin_tsearch_root(void* const* rootp)
{
    void* value = NULL;
    memcpy(&value, rootp, sizeof(value));
    return (RinSearchTreeNode*)value;
}

static void rin_tsearch_set_root(void** rootp, RinSearchTreeNode* root)
{
    void* value = root;
    memcpy(rootp, &value, sizeof(value));
}

void* tsearch(const void* key, void** rootp,
              int (*compar)(const void*, const void*))
{
    RinSearchTreeNode* root;
    RinSearchTreeNode* parent = NULL;
    RinSearchTreeNode** link;
    RinSearchTreeNode* node;
    if (rootp == NULL || compar == NULL) {
        errno = EINVAL;
        return NULL;
    }
    root = rin_tsearch_root(rootp);
    link = &root;
    while (*link != NULL) {
        const int order = compar(key, (*link)->key);
        if (order == 0) return *link;
        parent = *link;
        link = order < 0 ? &(*link)->left : &(*link)->right;
    }
    node = (RinSearchTreeNode*)malloc(sizeof(*node));
    if (node == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    node->key = (void*)key;
    node->left = NULL;
    node->right = NULL;
    node->parent = parent;
    *link = node;
    if (parent == NULL) rin_tsearch_set_root(rootp, node);
    return node;
}

void* tfind(const void* key, void* const* rootp,
            int (*compar)(const void*, const void*))
{
    RinSearchTreeNode* node;
    if (rootp == NULL || compar == NULL) {
        errno = EINVAL;
        return NULL;
    }
    node = rin_tsearch_find(key, rin_tsearch_root(rootp), compar);
    return node;
}

static void rin_tsearch_transplant(void** rootp, RinSearchTreeNode* old_node,
                                  RinSearchTreeNode* new_node)
{
    if (old_node->parent == NULL) {
        rin_tsearch_set_root(rootp, new_node);
    } else if (old_node == old_node->parent->left) {
        old_node->parent->left = new_node;
    } else {
        old_node->parent->right = new_node;
    }
    if (new_node != NULL) new_node->parent = old_node->parent;
}

static RinSearchTreeNode* rin_tsearch_minimum(RinSearchTreeNode* node)
{
    while (node->left != NULL) node = node->left;
    return node;
}

void* tdelete(const void* key, void** rootp,
              int (*compar)(const void*, const void*))
{
    RinSearchTreeNode* node;
    RinSearchTreeNode* parent;
    if (rootp == NULL || compar == NULL) {
        errno = EINVAL;
        return NULL;
    }
    node = rin_tsearch_find(key, rin_tsearch_root(rootp), compar);
    if (node == NULL) return NULL;
    parent = node->parent;
    if (node->left == NULL) {
        rin_tsearch_transplant(rootp, node, node->right);
    } else if (node->right == NULL) {
        rin_tsearch_transplant(rootp, node, node->left);
    } else {
        RinSearchTreeNode* successor = rin_tsearch_minimum(node->right);
        if (successor->parent != node) {
            rin_tsearch_transplant(rootp, successor, successor->right);
            successor->right = node->right;
            successor->right->parent = successor;
        }
        rin_tsearch_transplant(rootp, node, successor);
        successor->left = node->left;
        successor->left->parent = successor;
    }
    free(node);
    return parent != NULL ? (void*)parent : (void*)rootp;
}

void twalk(const void* root, void (*action)(const void*, VISIT, int))
{
    RinSearchTreeNode* node = (RinSearchTreeNode*)root;
    RinSearchTreeNode* previous = NULL;
    int level = 0;
    if (node == NULL || action == NULL) return;
    while (node != NULL) {
        RinSearchTreeNode* next = node->parent;
        int next_level = level > 0 ? level - 1 : 0;
        if (previous == node->parent) {
            if (node->left == NULL && node->right == NULL) {
                action(node, leaf, level);
            } else {
                action(node, preorder, level);
                if (node->left != NULL) {
                    next = node->left;
                    next_level = level < INT_MAX ? level + 1 : INT_MAX;
                } else {
                    action(node, postorder, level);
                    if (node->right != NULL) {
                        next = node->right;
                        next_level = level < INT_MAX ? level + 1 : INT_MAX;
                    } else {
                        action(node, endorder, level);
                    }
                }
            }
        } else if (previous == node->left) {
            action(node, postorder, level);
            if (node->right != NULL) {
                next = node->right;
                next_level = level < INT_MAX ? level + 1 : INT_MAX;
            } else {
                action(node, endorder, level);
            }
        } else {
            action(node, endorder, level);
        }
        previous = node;
        node = next;
        level = next_level;
    }
}

static void* rin_queue_read_link(const void* element, size_t index)
{
    void* link = NULL;
    memcpy(&link, (const unsigned char*)element + index * sizeof(link),
           sizeof(link));
    return link;
}

static void rin_queue_write_link(void* element, size_t index, void* link)
{
    memcpy((unsigned char*)element + index * sizeof(link), &link,
           sizeof(link));
}

void insque(void* element, void* pred)
{
    void* successor;
    if (element == NULL) return;
    if (pred == NULL) {
        rin_queue_write_link(element, 0u, NULL);
        rin_queue_write_link(element, 1u, NULL);
        return;
    }
    successor = rin_queue_read_link(pred, 0u);
    rin_queue_write_link(element, 0u, successor);
    rin_queue_write_link(element, 1u, pred);
    rin_queue_write_link(pred, 0u, element);
    if (successor != NULL)
        rin_queue_write_link(successor, 1u, element);
}

void remque(void* element)
{
    void* predecessor;
    void* successor;
    if (element == NULL) return;
    predecessor = rin_queue_read_link(element, 1u);
    successor = rin_queue_read_link(element, 0u);
    if (predecessor != NULL)
        rin_queue_write_link(predecessor, 0u, successor);
    if (successor != NULL)
        rin_queue_write_link(successor, 1u, predecessor);
}

static void* rin_lsearch_find(const void* key, const void* base,
                              size_t count, size_t width,
                              int (*compar)(const void*, const void*))
{
    const unsigned char* bytes = (const unsigned char*)base;
    size_t index;
    for (index = 0u; index < count; ++index) {
        const void* entry = bytes + index * width;
        if (compar(key, entry) == 0) return (void*)entry;
    }
    return NULL;
}

void* lfind(const void* key, const void* base, size_t* nelp,
            size_t width, int (*compar)(const void*, const void*))
{
    if (nelp == NULL || compar == NULL || width == 0u || key == NULL ||
        (*nelp != 0u && base == NULL)) {
        errno = EINVAL;
        return NULL;
    }
    if (*nelp > SIZE_MAX / width) {
        errno = EOVERFLOW;
        return NULL;
    }
    return rin_lsearch_find(key, base, *nelp, width, compar);
}

void* lsearch(const void* key, void* base, size_t* nelp,
              size_t width, int (*compar)(const void*, const void*))
{
    void* found;
    if (nelp == NULL || compar == NULL || width == 0u || key == NULL ||
        base == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (*nelp >= SIZE_MAX / width) {
        errno = EOVERFLOW;
        return NULL;
    }
    found = rin_lsearch_find(key, base, *nelp, width, compar);
    if (found != NULL) return found;
    memcpy((unsigned char*)base + *nelp * width, key, width);
    ++*nelp;
    return (unsigned char*)base + (*nelp - 1u) * width;
}
