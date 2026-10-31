/* SPDX-License-Identifier: MIT */
/* GNU qsort_r runtime with a per-call comparator context. */

#define _GNU_SOURCE 1
#define _RIN_USE_GNU_QSORT_R 1
#include "stdlib.h"

typedef int (*rin_qsort_r_comparator)(const void*, const void*, void*);

static void rin_qsort_r_sift_down(unsigned char* bytes, size_t root,
                                  size_t end, size_t size,
                                  rin_qsort_r_comparator compare,
                                  void* context)
{
    if (end == 0u) return;
    while (root <= (end - 1u) / 2u) {
        const size_t child = root * 2u + 1u;
        size_t selected = root;
        if (compare(bytes + selected * size, bytes + child * size,
                    context) < 0)
            selected = child;
        if (child < end &&
            compare(bytes + selected * size, bytes + (child + 1u) * size,
                    context) < 0)
            selected = child + 1u;
        if (selected == root) return;
        _rin_qsort_swap_bytes(bytes + root * size,
                              bytes + selected * size, size);
        root = selected;
    }
}

void qsort_r(void* base, size_t nmemb, size_t size,
             int (*compar)(const void*, const void*, void*), void* context)
{
    unsigned char* bytes = (unsigned char*)base;
    size_t start;
    size_t end;
    if (bytes == NULL || compar == NULL || size == 0u || nmemb < 2u ||
        nmemb > SIZE_MAX / size)
        return;

    start = (nmemb - 2u) / 2u + 1u;
    while (start != 0u) {
        --start;
        rin_qsort_r_sift_down(bytes, start, nmemb - 1u, size,
                              compar, context);
    }
    end = nmemb - 1u;
    while (end != 0u) {
        _rin_qsort_swap_bytes(bytes, bytes + end * size, size);
        --end;
        rin_qsort_r_sift_down(bytes, 0u, end, size, compar, context);
    }
}
