/* SPDX-License-Identifier: MIT */
/* POSIX search, tree, and queue interfaces. */

#ifndef _RIN_SEARCH_H
#define _RIN_SEARCH_H

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
