/* SPDX-License-Identifier: MIT */
#define RIN_STDLIB_ALIGNED_ALLOC_RUNTIME_IMPLEMENTATION 1
#include "stdlib.h"
#include "stdint.h"

void* memalign(size_t alignment, size_t size) {
    return _rin_aligned_alloc_internal(alignment, size, 0);
}

void free_sized(void* ptr, size_t size) {
    (void)size;
    free(ptr);
}

void free_aligned_sized(void* ptr, size_t alignment, size_t size) {
    (void)alignment;
    (void)size;
    free(ptr);
}

void* reallocarray(void* ptr, size_t nmemb, size_t size) {
    if (size != 0u && nmemb > SIZE_MAX / size) {
        errno = ENOMEM;
        return NULL;
    }
    return realloc(ptr, nmemb * size);
}
