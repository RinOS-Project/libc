/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - malloc.c
 * ユーザー空間 arena allocator
 *
 * 小サイズ確保は rin_malloc の size-class arena で再利用し、arena の補充と
 * 大サイズ確保は匿名 mmap backendへ委譲する。
 *
 * 方針: malloc(0) は NULL を返す (POSIX/ISO 許容)
 */

#include "stddef.h"
#include "stdint.h"
#include "errno.h"
#include "rin_aligned_alloc_meta.h"

/* rin_runtime.c で定義 (userspace arena frontend) */
extern void* rin_malloc(size_t size);
extern void  rin_free(void* ptr);
extern void* rin_realloc(void* ptr, size_t size);
extern size_t rin_malloc_usable_size(void* ptr);

void* malloc(size_t size) {
    if (size == 0) return 0;
    void* ptr = rin_malloc(size);
    if (!ptr) errno = ENOMEM;
    return ptr;
}

void free(void* ptr) {
    void* raw = ptr;
    if (rin_load_aligned_allocation_raw_pointer(ptr, &raw)) ptr = raw;
    rin_free(ptr);  /* rin_free は NULL チェック済み */
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

void* calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) return 0;
    if (nmemb > ((size_t)-1) / size) {
        errno = ENOMEM;
        return 0;  /* オーバーフロー防止 */
    }
    size_t total = nmemb * size;
    void* ptr = rin_malloc(total);
    if (!ptr) {
        errno = ENOMEM;
        return 0;
    }
    char* p = (char*)ptr;
    for (size_t i = 0; i < total; i++) p[i] = 0;
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    void* raw = ptr;
    size_t alignment;
    size_t old_size;
    size_t meta_bytes;
    size_t total;
    size_t copy_size;
    uintptr_t raw_address;
    uintptr_t aligned_address;
    uintptr_t address;
    void* replacement_raw;
    unsigned char* destination;
    const unsigned char* source;
    if (ptr && rin_load_aligned_allocation_metadata(
                   ptr, &raw, &alignment, &old_size)) {
        meta_bytes = rin_aligned_allocation_meta_size();
        if (size == 0) {
            rin_free(raw);
            return 0;
        }
        if (size > SIZE_MAX - meta_bytes ||
            alignment - 1u > SIZE_MAX - meta_bytes - size) {
            errno = ENOMEM;
            return 0;
        }
        total = size + meta_bytes + alignment - 1u;
        replacement_raw = rin_malloc(total);
        if (!replacement_raw) {
            errno = ENOMEM;
            return 0;
        }
        raw_address = (uintptr_t)replacement_raw;
        if (raw_address > UINTPTR_MAX - meta_bytes) {
            rin_free(replacement_raw);
            errno = ENOMEM;
            return 0;
        }
        address = raw_address + meta_bytes;
        if (alignment - 1u > UINTPTR_MAX - address) {
            rin_free(replacement_raw);
            errno = ENOMEM;
            return 0;
        }
        aligned_address = (address + alignment - 1u) &
                          ~((uintptr_t)alignment - 1u);
        rin_store_aligned_allocation_metadata_ex(
            (void*)aligned_address, replacement_raw, alignment, size);
        copy_size = old_size < size ? old_size : size;
        destination = (unsigned char*)aligned_address;
        source = (const unsigned char*)ptr;
        for (size_t index = 0; index < copy_size; ++index)
            destination[index] = source[index];
        rin_free(raw);
        return (void*)aligned_address;
    }
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return 0; }
    void* replacement = rin_realloc(ptr, size);
    if (!replacement) errno = ENOMEM;
    return replacement;
}

void* reallocarray(void* ptr, size_t nmemb, size_t size) {
    if (size != 0 && nmemb > SIZE_MAX / size) {
        errno = ENOMEM;
        return 0;
    }
    return realloc(ptr, nmemb * size);
}

size_t malloc_usable_size(void* ptr) {
    void* raw = ptr;
    size_t capacity;
    if (ptr && rin_load_aligned_allocation_raw_pointer(ptr, &raw)) {
        uintptr_t offset = (uintptr_t)ptr - (uintptr_t)raw;
        capacity = rin_malloc_usable_size(raw);
        return capacity > offset ? capacity - (size_t)offset : 0u;
    }
    return rin_malloc_usable_size(ptr);
}

/* ═══════════════════════════════════════════════════════════════
 * atexit - プログラム終了時に呼び出す関数を登録
 * ═══════════════════════════════════════════════════════════════*/

extern int __rin_atexit_register(void (*func)(void));

int atexit(void (*func)(void)) {
    return __rin_atexit_register(func);
}
