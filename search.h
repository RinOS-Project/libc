/* SPDX-License-Identifier: MIT */
/* POSIX search, tree, and queue interfaces. */

#ifndef _RIN_SEARCH_H
#define _RIN_SEARCH_H

#include "features.h"
#include <stddef.h>

#if defined(__RINOS__) || defined(__USE_XOPEN_EXTENDED) || \
    defined(__USE_GNU) || defined(__DARWIN_C_SOURCE) || \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 500) || \
    (defined(__XSI_VISIBLE) && __XSI_VISIBLE)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct entry {
    char* key;
    void* data;
} ENTRY;

typedef enum {
    FIND = 0,
    ENTER = 1
} ACTION;

typedef enum {
    preorder = 0,
    postorder = 1,
    endorder = 2,
    leaf = 3
} VISIT;

int hcreate(size_t nel);
void hdestroy(void);
ENTRY* hsearch(ENTRY item, ACTION action);

void* tsearch(const void* key, void** rootp,
              int (*compar)(const void*, const void*));
void* tfind(const void* key, void* const* rootp,
            int (*compar)(const void*, const void*));
void* tdelete(const void* key, void** rootp,
              int (*compar)(const void*, const void*));
void twalk(const void* root,
           void (*action)(const void*, VISIT, int));

#if defined(__USE_GNU) || defined(_RIN_USE_GNU_SEARCH)
/* GNU-compatible caller-owned hash table.  Initialize with {0}; distinct
 * instances do not share the process-global hsearch() table. */
struct _ENTRY;
struct hsearch_data {
    struct _ENTRY* table;
    unsigned int size;
    unsigned int filled;
};

int hcreate_r(size_t nel, struct hsearch_data* table);
void hdestroy_r(struct hsearch_data* table);
int hsearch_r(ENTRY item, ACTION action, ENTRY** result,
              struct hsearch_data* table);
void tdestroy(void* root, void (*free_node)(void*));
void twalk_r(const void* root,
             void (*action)(const void*, VISIT, void*), void* context);
#endif

void insque(void* element, void* pred);
void remque(void* element);

void* lfind(const void* key, const void* base, size_t* nelp,
            size_t width, int (*compar)(const void*, const void*));
void* lsearch(const void* key, void* base, size_t* nelp,
              size_t width, int (*compar)(const void*, const void*));

#ifdef __cplusplus
}
#endif

#endif /* XSI search interfaces */

#endif
