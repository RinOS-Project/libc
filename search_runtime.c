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
    char* owned_key;
    uint64_t hash;
    int occupied;
} RinHSearchSlot;

typedef struct RinHSearchTable {
    size_t capacity;
    size_t filled;
    RinHSearchSlot* slots;
    volatile uint32_t lock;
} RinHSearchTable;

typedef struct RinSearchTreeNode {
    void* key;
    struct RinSearchTreeNode* left;
    struct RinSearchTreeNode* right;
    struct RinSearchTreeNode* parent;
    int height;
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

static char* rin_search_key_copy(const char* key)
{
    size_t length = strlen(key);
    char* copy;
    if (length == SIZE_MAX) return NULL;
    copy = (char*)malloc(length + 1u);
    if (copy != NULL) memcpy(copy, key, length + 1u);
    return copy;
}

static RinHSearchTable* rin_hsearch_table_create(size_t nel)
{
    size_t required;
    size_t capacity = 8u;
    RinHSearchTable* table;
    if (nel == 0u || nel > SIZE_MAX / 2u) {
        errno = nel == 0u ? EINVAL : ENOMEM;
        return NULL;
    }
    required = nel * 2u;
    if (required < 8u) required = 8u;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2u) {
            errno = ENOMEM;
            return NULL;
        }
        capacity *= 2u;
    }
    if (capacity > UINT_MAX || capacity > SIZE_MAX / sizeof(RinHSearchSlot)) {
        errno = ENOMEM;
        return NULL;
    }
    table = (RinHSearchTable*)calloc(1u, sizeof(*table));
    if (table == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    table->slots = (RinHSearchSlot*)calloc(capacity, sizeof(*table->slots));
    if (table->slots == NULL) {
        free(table);
        errno = ENOMEM;
        return NULL;
    }
    table->capacity = capacity;
    return table;
}

static void rin_hsearch_table_destroy(RinHSearchTable* table)
{
    size_t index;
    if (table == NULL) return;
    for (index = 0u; index < table->capacity; ++index) {
        if (table->slots[index].occupied)
            free(table->slots[index].owned_key);
    }
    free(table->slots);
    free(table);
}

static void rin_hsearch_table_lock(RinHSearchTable* table)
{
    while (__atomic_exchange_n(&table->lock, 1u, __ATOMIC_ACQUIRE) != 0u) {
    }
}

static void rin_hsearch_table_unlock(RinHSearchTable* table)
{
    __atomic_store_n(&table->lock, 0u, __ATOMIC_RELEASE);
}

int hcreate(size_t nel)
{
    RinHSearchTable* table;
    rin_hsearch_lock();
    if (g_hsearch_table != NULL) {
        rin_hsearch_unlock();
        errno = ENOMEM;
        return 0;
    }
    rin_hsearch_unlock();
    table = rin_hsearch_table_create(nel);
    if (table == NULL) return 0;
    rin_hsearch_lock();
    if (g_hsearch_table != NULL) {
        rin_hsearch_unlock();
        rin_hsearch_table_destroy(table);
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
    rin_hsearch_table_destroy(table);
}

int hcreate_r(size_t nel, struct hsearch_data* public_table)
{
    RinHSearchTable* table;
    if (public_table == NULL || public_table->table != NULL ||
        public_table->size != 0u || public_table->filled != 0u) {
        errno = EINVAL;
        return 0;
    }
    table = rin_hsearch_table_create(nel);
    if (table == NULL) return 0;
    public_table->table = (struct _ENTRY*)table;
    public_table->size = (unsigned int)table->capacity;
    public_table->filled = 0u;
    return 1;
}

void hdestroy_r(struct hsearch_data* public_table)
{
    RinHSearchTable* table;
    if (public_table == NULL || public_table->table == NULL) return;
    table = (RinHSearchTable*)public_table->table;
    public_table->table = NULL;
    public_table->size = 0u;
    public_table->filled = 0u;
    rin_hsearch_table_destroy(table);
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
            candidate->owned_key = rin_search_key_copy(item.key);
            if (candidate->owned_key == NULL) {
                errno = ENOMEM;
                rin_hsearch_unlock();
                return NULL;
            }
            candidate->entry.key = candidate->owned_key;
            candidate->entry.data = item.data;
            candidate->hash = hash;
            candidate->occupied = 1;
            ++table->filled;
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

int hsearch_r(ENTRY item, ACTION action, ENTRY** result,
              struct hsearch_data* public_table)
{
    RinHSearchTable* table;
    uint64_t hash;
    size_t slot;
    size_t probe;
    char* copied_key = NULL;
    if (result != NULL) *result = NULL;
    if (item.key == NULL || result == NULL || public_table == NULL ||
        (action != FIND && action != ENTER) || public_table->table == NULL) {
        errno = EINVAL;
        return 0;
    }
    table = (RinHSearchTable*)public_table->table;
    if (public_table->size != table->capacity ||
        table->capacity == 0u ||
        (table->capacity & (table->capacity - 1u)) != 0u) {
        errno = EINVAL;
        return 0;
    }
    hash = rin_hsearch_hash(item.key);
    for (;;) {
        RinHSearchSlot* empty = NULL;
        rin_hsearch_table_lock(table);
        if (table->filled > table->capacity ||
            public_table->filled != table->filled) {
            rin_hsearch_table_unlock(table);
            free(copied_key);
            errno = EINVAL;
            return 0;
        }
        slot = (size_t)hash & (table->capacity - 1u);
        for (probe = 0u; probe < table->capacity; ++probe) {
            RinHSearchSlot* candidate = &table->slots[slot];
            if (!candidate->occupied) {
                empty = candidate;
                break;
            }
            if (candidate->hash == hash &&
                strcmp(candidate->entry.key, item.key) == 0) {
                *result = &candidate->entry;
                rin_hsearch_table_unlock(table);
                free(copied_key);
                return 1;
            }
            slot = (slot + 1u) & (table->capacity - 1u);
        }
        if (empty == NULL || action == FIND) {
            rin_hsearch_table_unlock(table);
            free(copied_key);
            errno = empty == NULL ? ENOMEM : ESRCH;
            return 0;
        }
        if (copied_key == NULL) {
            rin_hsearch_table_unlock(table);
            copied_key = rin_search_key_copy(item.key);
            if (copied_key == NULL) {
                errno = ENOMEM;
                return 0;
            }
            continue;
        }
        empty->owned_key = copied_key;
        empty->entry.key = copied_key;
        empty->entry.data = item.data;
        empty->hash = hash;
        empty->occupied = 1;
        ++table->filled;
        ++public_table->filled;
        rin_hsearch_table_unlock(table);
        *result = &empty->entry;
        return 1;
    }
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

static int rin_tsearch_height(const RinSearchTreeNode* node)
{
    return node != NULL ? node->height : 0;
}

static void rin_tsearch_update_height(RinSearchTreeNode* node)
{
    const int left_height = rin_tsearch_height(node->left);
    const int right_height = rin_tsearch_height(node->right);
    node->height = (left_height > right_height ? left_height : right_height) + 1;
}

static RinSearchTreeNode* rin_tsearch_rotate_left(
    void** rootp, RinSearchTreeNode* pivot)
{
    RinSearchTreeNode* replacement = pivot->right;
    RinSearchTreeNode* parent = pivot->parent;
    pivot->right = replacement->left;
    if (pivot->right != NULL) pivot->right->parent = pivot;
    replacement->left = pivot;
    replacement->parent = parent;
    pivot->parent = replacement;
    if (parent == NULL) {
        rin_tsearch_set_root(rootp, replacement);
    } else if (parent->left == pivot) {
        parent->left = replacement;
    } else {
        parent->right = replacement;
    }
    rin_tsearch_update_height(pivot);
    rin_tsearch_update_height(replacement);
    return replacement;
}

static RinSearchTreeNode* rin_tsearch_rotate_right(
    void** rootp, RinSearchTreeNode* pivot)
{
    RinSearchTreeNode* replacement = pivot->left;
    RinSearchTreeNode* parent = pivot->parent;
    pivot->left = replacement->right;
    if (pivot->left != NULL) pivot->left->parent = pivot;
    replacement->right = pivot;
    replacement->parent = parent;
    pivot->parent = replacement;
    if (parent == NULL) {
        rin_tsearch_set_root(rootp, replacement);
    } else if (parent->left == pivot) {
        parent->left = replacement;
    } else {
        parent->right = replacement;
    }
    rin_tsearch_update_height(pivot);
    rin_tsearch_update_height(replacement);
    return replacement;
}

static void rin_tsearch_rebalance(void** rootp, RinSearchTreeNode* node)
{
    while (node != NULL) {
        int balance;
        RinSearchTreeNode* subtree_root;
        rin_tsearch_update_height(node);
        balance = rin_tsearch_height(node->left) -
                  rin_tsearch_height(node->right);
        if (balance > 1) {
            if (rin_tsearch_height(node->left->left) <
                rin_tsearch_height(node->left->right))
                (void)rin_tsearch_rotate_left(rootp, node->left);
            subtree_root = rin_tsearch_rotate_right(rootp, node);
        } else if (balance < -1) {
            if (rin_tsearch_height(node->right->right) <
                rin_tsearch_height(node->right->left))
                (void)rin_tsearch_rotate_right(rootp, node->right);
            subtree_root = rin_tsearch_rotate_left(rootp, node);
        } else {
            subtree_root = node;
        }
        node = subtree_root->parent;
    }
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
    node->height = 1;
    *link = node;
    if (parent == NULL) rin_tsearch_set_root(rootp, node);
    else rin_tsearch_rebalance(rootp, parent);
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
    RinSearchTreeNode* rebalance_start;
    if (rootp == NULL || compar == NULL) {
        errno = EINVAL;
        return NULL;
    }
    node = rin_tsearch_find(key, rin_tsearch_root(rootp), compar);
    if (node == NULL) return NULL;
    parent = node->parent;
    if (node->left == NULL) {
        rin_tsearch_transplant(rootp, node, node->right);
        rebalance_start = parent;
    } else if (node->right == NULL) {
        rin_tsearch_transplant(rootp, node, node->left);
        rebalance_start = parent;
    } else {
        RinSearchTreeNode* successor = rin_tsearch_minimum(node->right);
        if (successor->parent != node) {
            rebalance_start = successor->parent;
            rin_tsearch_transplant(rootp, successor, successor->right);
            successor->right = node->right;
            successor->right->parent = successor;
        } else {
            rebalance_start = successor;
        }
        rin_tsearch_transplant(rootp, node, successor);
        successor->left = node->left;
        successor->left->parent = successor;
        rin_tsearch_update_height(successor);
    }
    free(node);
    rin_tsearch_rebalance(rootp, rebalance_start);
    return parent != NULL ? (void*)parent : (void*)rootp;
}

typedef void (*RinTreeWalkCallback)(const void*, VISIT, int, void*);

static void rin_twalk(const void* root, RinTreeWalkCallback action,
                      void* context)
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
                action(node, leaf, level, context);
            } else {
                action(node, preorder, level, context);
                if (node->left != NULL) {
                    next = node->left;
                    next_level = level < INT_MAX ? level + 1 : INT_MAX;
                } else {
                    action(node, postorder, level, context);
                    if (node->right != NULL) {
                        next = node->right;
                        next_level = level < INT_MAX ? level + 1 : INT_MAX;
                    } else {
                        action(node, endorder, level, context);
                    }
                }
            }
        } else if (previous == node->left) {
            action(node, postorder, level, context);
            if (node->right != NULL) {
                next = node->right;
                next_level = level < INT_MAX ? level + 1 : INT_MAX;
            } else {
                action(node, endorder, level, context);
            }
        } else {
            action(node, endorder, level, context);
        }
        previous = node;
        node = next;
        level = next_level;
    }
}

typedef struct RinTreeWalkLegacyContext {
    void (*action)(const void*, VISIT, int);
} RinTreeWalkLegacyContext;

typedef struct RinTreeWalkReentrantContext {
    void (*action)(const void*, VISIT, void*);
    void* context;
} RinTreeWalkReentrantContext;

static void rin_twalk_legacy_action(const void* node, VISIT visit, int level,
                                    void* context)
{
    RinTreeWalkLegacyContext* legacy = (RinTreeWalkLegacyContext*)context;
    legacy->action(node, visit, level);
}

static void rin_twalk_reentrant_bridge(const void* node, VISIT visit,
                                       int level, void* context)
{
    RinTreeWalkReentrantContext* reentrant =
        (RinTreeWalkReentrantContext*)context;
    (void)level;
    reentrant->action(node, visit, reentrant->context);
}

void twalk(const void* root, void (*action)(const void*, VISIT, int))
{
    RinTreeWalkLegacyContext context;
    if (action == NULL) return;
    context.action = action;
    rin_twalk(root, rin_twalk_legacy_action, &context);
}

void twalk_r(const void* root,
             void (*action)(const void*, VISIT, void*), void* context)
{
    RinTreeWalkReentrantContext callback_context;
    if (action == NULL) return;
    callback_context.action = action;
    callback_context.context = context;
    rin_twalk(root, rin_twalk_reentrant_bridge, &callback_context);
}

void tdestroy(void* root, void (*free_node)(void*))
{
    RinSearchTreeNode* node = (RinSearchTreeNode*)root;
    while (node != NULL) {
        if (node->left != NULL) {
            node = node->left;
        } else if (node->right != NULL) {
            node = node->right;
        } else {
            RinSearchTreeNode* parent = node->parent;
            if (parent != NULL) {
                if (parent->left == node) parent->left = NULL;
                else parent->right = NULL;
            }
            if (free_node != NULL) free_node(node->key);
            free(node);
            node = parent;
        }
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
    memmove((unsigned char*)base + *nelp * width, key, width);
    ++*nelp;
    return (unsigned char*)base + (*nelp - 1u) * width;
}
