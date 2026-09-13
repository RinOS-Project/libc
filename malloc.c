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

/* rin_runtime.c で定義 (userspace arena frontend) */
extern void* rin_malloc(size_t size);
extern void  rin_free(void* ptr);
extern void* rin_realloc(void* ptr, size_t size);
extern size_t rin_malloc_usable_size(void* ptr);

void* malloc(size_t size) {
    if (size == 0) return 0;
    return rin_malloc(size);
}

void free(void* ptr) {
    rin_free(ptr);  /* rin_free は NULL チェック済み */
}

void* calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) return 0;
    if (nmemb > ((size_t)-1) / size) return 0;  /* オーバーフロー防止 */
    size_t total = nmemb * size;
    void* ptr = rin_malloc(total);
    if (ptr) {
        char* p = (char*)ptr;
        for (size_t i = 0; i < total; i++) p[i] = 0;
    }
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return 0; }
    return rin_realloc(ptr, size);
}

size_t malloc_usable_size(void* ptr) {
    return rin_malloc_usable_size(ptr);
}

/* ═══════════════════════════════════════════════════════════════
 * atexit - プログラム終了時に呼び出す関数を登録
 * ═══════════════════════════════════════════════════════════════*/

extern int __rin_atexit_register(void (*func)(void));

int atexit(void (*func)(void)) {
    return __rin_atexit_register(func);
}
