/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - sys/mman.h
 * メモリマッピング
 */

#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H

#include "types.h"
#include "syscall.h"
#include <stdarg.h>

#ifndef _RIN_MMAN_SYSCALL0
#define _RIN_MMAN_SYSCALL0(number) \
    _syscall0((uintptr_t)(number))
#endif

#ifndef _RIN_MMAN_SYSCALL1
#define _RIN_MMAN_SYSCALL1(number, arg1) \
    _syscall1((uintptr_t)(number), (uintptr_t)(arg1))
#endif

#ifndef _RIN_MMAN_SYSCALL2
#define _RIN_MMAN_SYSCALL2(number, arg1, arg2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(arg1), (uintptr_t)(arg2))
#endif

#ifndef _RIN_MMAN_SYSCALL3
#define _RIN_MMAN_SYSCALL3(number, arg1, arg2, arg3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(arg1), (uintptr_t)(arg2), \
              (uintptr_t)(arg3))
#endif

#ifndef _RIN_MMAN_SYSCALL5
#define _RIN_MMAN_SYSCALL5(number, arg1, arg2, arg3, arg4, arg5) \
    _syscall5((uintptr_t)(number), (uintptr_t)(arg1), (uintptr_t)(arg2), \
              (uintptr_t)(arg3), (uintptr_t)(arg4), (uintptr_t)(arg5))
#endif

#ifndef _RIN_MMAN_SYSCALL6
#define _RIN_MMAN_SYSCALL6(number, arg1, arg2, arg3, arg4, arg5, arg6) \
    _syscall6((uintptr_t)(number), (uintptr_t)(arg1), (uintptr_t)(arg2), \
              (uintptr_t)(arg3), (uintptr_t)(arg4), (uintptr_t)(arg5), \
              (uintptr_t)(arg6))
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline int __rin_mman_zero_result(intptr_t result)
{
    if (result == 0) return 0;
    if (result > 0) {
        errno = EIO;
        return -1;
    }
    errno = result >= -4095 ? (int)(-result) : EIO;
    return -1;
}

/* ═══════════════════════════════════════════════════════════════
 * mmap プロテクションフラグ
 * ═══════════════════════════════════════════════════════════════*/

#define PROT_NONE      0x00       /* アクセス不可 */
#define PROT_READ      0x01       /* 読み取り可能 */
#define PROT_WRITE     0x02       /* 書き込み可能 */
#define PROT_EXEC      0x04       /* 実行可能 */
#define PROT_GROWSDOWN 0x01000000 /* スタック用（下方向に拡張） */
#define PROT_GROWSUP   0x02000000 /* 上方向に拡張 */

/* ═══════════════════════════════════════════════════════════════
 * mmap フラグ
 * ═══════════════════════════════════════════════════════════════*/

#define MAP_SHARED      0x01   /* 他プロセスと共有 */
#define MAP_PRIVATE     0x02   /* プライベートコピーオンライト */
#define MAP_FIXED       0x10   /* 指定アドレスに配置 */
#define MAP_ANONYMOUS   0x20   /* ファイルなし (メモリのみ) */
#define MAP_ANON        MAP_ANONYMOUS
#define MAP_GROWSDOWN   0x100  /* スタック用 (下方向に拡張) */
#define MAP_DENYWRITE   0x800  /* 書き込み禁止 */
#define MAP_LOCKED      0x2000 /* ページをロック */
#define MAP_NORESERVE   0x4000 /* スワップ予約なし */
#define MAP_POPULATE    0x8000 /* 事前にページをフォルト */
#define MAP_NONBLOCK    0x10000/* ページフォルトをブロックしない */
#define MAP_STACK       0x20000/* スタック用ヒント */
#define MAP_HUGETLB     0x40000/* ヒュージページを使用 */

/* mmap失敗時の戻り値 */
#define MAP_FAILED ((void*)-1)

static inline void* __rin_mman_pointer_result(intptr_t result)
{
    if (result >= 0) return (void*)(uintptr_t)result;
    errno = result >= -4095 ? (int)(-result) : EIO;
    return MAP_FAILED;
}

/* ═══════════════════════════════════════════════════════════════
 * msync フラグ
 * ═══════════════════════════════════════════════════════════════*/

#define MS_ASYNC      1   /* 非同期書き込み */
#define MS_SYNC       4   /* 同期書き込み */
#define MS_INVALIDATE 2   /* キャッシュ無効化 */

/* ═══════════════════════════════════════════════════════════════
 * madvise アドバイス
 * ═══════════════════════════════════════════════════════════════*/

#define MADV_NORMAL      0   /* デフォルト */
#define MADV_RANDOM      1   /* ランダムアクセス */
#define MADV_SEQUENTIAL  2   /* シーケンシャルアクセス */
#define MADV_WILLNEED    3   /* すぐに必要 */
#define MADV_DONTNEED    4   /* しばらく不要 */
#define MADV_FREE        8   /* ページ解放可能 */
#define MADV_REMOVE      9   /* 削除 */
#define MADV_DONTFORK    10  /* fork時にコピーしない */
#define MADV_DOFORK      11  /* fork時にコピーする */

/* ═══════════════════════════════════════════════════════════════
 * mlock フラグ
 * ═══════════════════════════════════════════════════════════════*/

#define MCL_CURRENT 1   /* 現在のページをロック */
#define MCL_FUTURE  2   /* 将来のページもロック */
#define MCL_ONFAULT 4   /* フォルト時にロック */

/* ═══════════════════════════════════════════════════════════════
 * mremap フラグ
 * ═══════════════════════════════════════════════════════════════*/

#define MREMAP_MAYMOVE   1  /* 移動可能 */
#define MREMAP_FIXED     2  /* 固定アドレス */
#define MREMAP_DONTUNMAP 4  /* 元のマッピングを保持 */

/* ═══════════════════════════════════════════════════════════════
 * メモリマッピング関数
 * ═══════════════════════════════════════════════════════════════*/

static inline void* mmap(void* addr, size_t length, int prot, int flags,
                         int fd, off_t offset) {
    intptr_t ret;
    if (fd == -1 || (flags & MAP_ANONYMOUS) != 0) {
        ret = _RIN_MMAN_SYSCALL6(SYS_MMAP,
                                 (uintptr_t)addr,
                                 (uintptr_t)length,
                                 (uintptr_t)(intptr_t)prot,
                                 (uintptr_t)(intptr_t)flags,
                                 (uintptr_t)(intptr_t)fd,
                                 (uintptr_t)(intptr_t)offset);
    } else {
        ret = _RIN_MMAN_SYSCALL6(SYS_MMAP_FD,
                                 (uintptr_t)addr,
                                 (uintptr_t)length,
                                 (uintptr_t)(intptr_t)prot,
                                 (uintptr_t)(intptr_t)flags,
                                 (uintptr_t)(intptr_t)fd,
                                 (uintptr_t)(intptr_t)offset);
    }
    return __rin_mman_pointer_result(ret);
}

static inline int munmap(void* addr, size_t length) {
    intptr_t ret = _RIN_MMAN_SYSCALL2(
        SYS_MUNMAP, (uintptr_t)addr, (uintptr_t)length);
    return __rin_mman_zero_result(ret);
}

static inline int mprotect(void* addr, size_t length, int prot) {
    intptr_t ret = _RIN_MMAN_SYSCALL3(
        SYS_MPROTECT, (uintptr_t)addr, (uintptr_t)length,
        (uintptr_t)(intptr_t)prot);
    return __rin_mman_zero_result(ret);
}

/* ═══════════════════════════════════════════════════════════════
 * 追加メモリ管理関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int msync(void* addr, size_t length, int flags) {
    intptr_t ret = _RIN_MMAN_SYSCALL3(
        SYS_MSYNC, (uintptr_t)addr, (uintptr_t)length,
        (uintptr_t)(intptr_t)flags);
    return __rin_mman_zero_result(ret);
}

static inline int madvise(void* addr, size_t length, int advice) {
    intptr_t ret = _RIN_MMAN_SYSCALL3(
        SYS_MADVISE, (uintptr_t)addr, (uintptr_t)length,
        (uintptr_t)(intptr_t)advice);
    return __rin_mman_zero_result(ret);
}

static inline int mlock(const void* addr, size_t length) {
    intptr_t ret = _RIN_MMAN_SYSCALL2(
        SYS_MLOCK, (uintptr_t)addr, (uintptr_t)length);
    return __rin_mman_zero_result(ret);
}

static inline int munlock(const void* addr, size_t length) {
    intptr_t ret = _RIN_MMAN_SYSCALL2(
        SYS_MUNLOCK, (uintptr_t)addr, (uintptr_t)length);
    return __rin_mman_zero_result(ret);
}

static inline int mlockall(int flags) {
    intptr_t ret = _RIN_MMAN_SYSCALL1(
        SYS_MLOCKALL, (uintptr_t)(intptr_t)flags);
    return __rin_mman_zero_result(ret);
}

static inline int munlockall(void) {
    intptr_t ret = _RIN_MMAN_SYSCALL0(SYS_MUNLOCKALL);
    return __rin_mman_zero_result(ret);
}

static inline int mincore(void* addr, size_t length, unsigned char* vec) {
    intptr_t ret = _RIN_MMAN_SYSCALL3(
        SYS_MINCORE, (uintptr_t)addr, (uintptr_t)length, (uintptr_t)vec);
    return __rin_mman_zero_result(ret);
}

static inline void* mremap(void* old_addr, size_t old_size, size_t new_size,
                           int flags, ...) {
    uintptr_t new_addr = 0u;
    intptr_t ret;
    if ((flags & MREMAP_FIXED) != 0) {
        va_list arguments;
        va_start(arguments, flags);
        new_addr = (uintptr_t)va_arg(arguments, void*);
        va_end(arguments);
    }
    ret = _RIN_MMAN_SYSCALL5(
        SYS_MREMAP, (uintptr_t)old_addr, (uintptr_t)old_size,
        (uintptr_t)new_size, (uintptr_t)(intptr_t)flags, new_addr);
    return __rin_mman_pointer_result(ret);
}

/* shm_open/shm_unlink は別途実装 */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_MMAN_H */
