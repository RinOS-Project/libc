/*
 * RinOS libc - sys/resource.h
 * リソース制限
 */

#ifndef _SYS_RESOURCE_H
#define _SYS_RESOURCE_H

#include "types.h"
#include "syscall.h"
#include "../time.h"
#include "../errno.h"
#include "../limits.h"
#if defined(RIN_FREESTANDING) || defined(RIN_USERSPACE) || \
    defined(RIN_RESOURCE_LIMIT_PRODUCT_SYSCALL)
#include "../../../../src/shared/rin_resource_limit_abi.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * リソースタイプ
 * ═══════════════════════════════════════════════════════════════*/

#define RLIMIT_CPU      0   /* CPU時間 (秒) */
#define RLIMIT_FSIZE    1   /* ファイルサイズ (バイト) */
#define RLIMIT_DATA     2   /* データセグメントサイズ */
#define RLIMIT_STACK    3   /* スタックサイズ */
#define RLIMIT_CORE     4   /* コアファイルサイズ */
#define RLIMIT_RSS      5   /* 常駐セットサイズ */
#define RLIMIT_NPROC    6   /* プロセス数 */
#define RLIMIT_NOFILE   7   /* オープンファイル数 */
#define RLIMIT_MEMLOCK  8   /* ロック可能メモリ */
#define RLIMIT_AS       9   /* アドレス空間サイズ */
#define RLIMIT_LOCKS    10  /* ファイルロック数 */
#define RLIMIT_SIGPENDING 11 /* 保留シグナル数 */
#define RLIMIT_MSGQUEUE 12  /* メッセージキューサイズ */
#define RLIMIT_NICE     13  /* nice値 */
#define RLIMIT_RTPRIO   14  /* リアルタイム優先度 */
#define RLIMIT_RTTIME   15  /* リアルタイムタイムアウト */
#define RLIMIT_NLIMITS  16

#define RLIM_NLIMITS    RLIMIT_NLIMITS

/* 無制限 */
#define RLIM_INFINITY   (~0UL)
#define RLIM_SAVED_MAX  RLIM_INFINITY
#define RLIM_SAVED_CUR  RLIM_INFINITY

/* The kernel's bounded mlock owner currently exposes a read-only default
 * through getrlimit.  Keep this value in the public libc contract so callers
 * can size an MCL_CURRENT request without guessing; setrlimit remains
 * unsupported until a per-process resource-limit owner is wired in. */
#define RIN_RLIMIT_MEMLOCK_DEFAULT ((rlim_t)(64UL * 1024UL * 1024UL))

/* A freestanding libc can use the kernel's fixed-width resource-limit ABI
 * without requiring a product-specific weak hook.  Hosted builds retain the
 * historical fixed-value/ENOSYS boundary unless they opt in explicitly. */
#if !defined(RIN_RESOURCE_LIMIT_PRODUCT_SYSCALL)
#if defined(RIN_FREESTANDING) || defined(RIN_USERSPACE)
#define RIN_RESOURCE_LIMIT_PRODUCT_SYSCALL 1
#else
#define RIN_RESOURCE_LIMIT_PRODUCT_SYSCALL 0
#endif
#endif

#if !defined(RIN_PROCESS_PRIORITY_PRODUCT_SYSCALL)
#if defined(RIN_FREESTANDING) || defined(RIN_USERSPACE)
#define RIN_PROCESS_PRIORITY_PRODUCT_SYSCALL 1
#else
#define RIN_PROCESS_PRIORITY_PRODUCT_SYSCALL 0
#endif
#endif

#if RIN_RESOURCE_LIMIT_PRODUCT_SYSCALL
#ifndef _RIN_RESOURCE_LIMIT_SYSCALL2
#define _RIN_RESOURCE_LIMIT_SYSCALL2(number, argument1, argument2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2))
#endif
#endif

#if RIN_PROCESS_PRIORITY_PRODUCT_SYSCALL
/* Keep the priority syscall boundary overrideable by linked contract tests
 * while the product path uses the normal target-width syscall words. */
#ifndef _RIN_PRIORITY_SYSCALL2
#define _RIN_PRIORITY_SYSCALL2(number, argument1, argument2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2))
#endif
#ifndef _RIN_PRIORITY_SYSCALL3
#define _RIN_PRIORITY_SYSCALL3(number, argument1, argument2, argument3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3))
#endif
#endif

/* ═══════════════════════════════════════════════════════════════
 * rlimit構造体
 * ═══════════════════════════════════════════════════════════════*/

typedef unsigned long rlim_t;

struct rlimit {
    rlim_t rlim_cur;  /* ソフトリミット */
    rlim_t rlim_max;  /* ハードリミット */
};

struct rlimit64 {
    unsigned long long rlim_cur;
    unsigned long long rlim_max;
};
/* A freestanding product may bind these hooks to the kernel's
 * generation-bound resource-limit owner.  They use POSIX return values
 * (0/-1) and must set errno on failure.  Weak lookup keeps hosted libc
 * builds independent of the kernel while avoiding a synthetic success when
 * a product hook is present but rejects a request. */
#if defined(__GNUC__) || defined(__clang__)
extern int rin_resource_limit_get(int resource, struct rlimit* limit)
    __attribute__((weak));
extern int rin_resource_limit_set(int resource, const struct rlimit* limit)
    __attribute__((weak));
extern int rin_resource_limit_prlimit(pid_t pid, int resource,
                                      const struct rlimit* new_limit,
                                      struct rlimit* old_limit)
    __attribute__((weak));
#endif

static inline int __rin_resource_limit_get_hook(int resource,
                                                 struct rlimit* limit)
{
#if defined(__GNUC__) || defined(__clang__)
    if (rin_resource_limit_get != NULL)
        return rin_resource_limit_get(resource, limit) == 0 ? 0 : -1;
#else
    (void)resource;
    (void)limit;
#endif
    return 1; /* no product owner is linked */
}

static inline int __rin_resource_limit_set_hook(
    int resource, const struct rlimit* limit)
{
#if defined(__GNUC__) || defined(__clang__)
    if (rin_resource_limit_set != NULL)
        return rin_resource_limit_set(resource, limit) == 0 ? 0 : -1;
#else
    (void)resource;
    (void)limit;
#endif
    return 1;
}

#if RIN_RESOURCE_LIMIT_PRODUCT_SYSCALL
/* Return 0 for the three resources owned by the kernel ABI and 1 for a class
 * that must remain on the established libc fallback boundary. */
static inline int __rin_resource_limit_wire_resource(
    int resource, uint32_t* wire_resource)
{
    if (!wire_resource) return -1;
    if (resource == RLIMIT_NOFILE || resource == RLIMIT_MEMLOCK ||
        resource == RLIMIT_AS) {
        *wire_resource = (uint32_t)resource;
        return 0;
    }
    return 1;
}

static inline int __rin_resource_limit_wire_call(
    uint16_t operation, uint32_t flags, uint32_t target_process_id,
    uint64_t target_instance_cookie, int resource,
    const struct rlimit* new_limit, struct rlimit* old_limit)
{
    RinResourceLimitCallRequestV1 request = {0};
    RinResourceLimitCallResponseV1 response = {0};
    uint32_t wire_resource;
    intptr_t raw_result;
    int resource_result = __rin_resource_limit_wire_resource(
        resource, &wire_resource);
    if (resource_result != 0) {
        if (resource_result < 0) errno = EFAULT;
        return resource_result;
    }
    if ((flags & RIN_RESOURCE_LIMIT_CALL_FLAG_SET) != 0u && !new_limit) {
        errno = EFAULT;
        return -1;
    }
    request.struct_size = RIN_RESOURCE_LIMIT_CALL_SIZE;
    request.version = RIN_RESOURCE_LIMIT_ABI_VERSION;
    request.operation = operation;
    request.flags = flags;
    request.resource = wire_resource;
    request.target_process_id = target_process_id;
    request.target_instance_cookie = target_instance_cookie;
    if (new_limit) {
        request.new_current = resource == RLIMIT_AS &&
                new_limit->rlim_cur == (rlim_t)RLIM_INFINITY
            ? UINT64_MAX : (uint64_t)new_limit->rlim_cur;
        request.new_maximum = resource == RLIMIT_AS &&
                new_limit->rlim_max == (rlim_t)RLIM_INFINITY
            ? UINT64_MAX : (uint64_t)new_limit->rlim_max;
    }
    raw_result = _RIN_RESOURCE_LIMIT_SYSCALL2(
        SYS_RESOURCE_LIMIT_CALL, &request, &response);
    raw_result = __rin_syscall_posixize(raw_result);
    if (raw_result == -1) return -1;
    if (raw_result != 0 || response.struct_size != RIN_RESOURCE_LIMIT_CALL_SIZE ||
        response.version != RIN_RESOURCE_LIMIT_ABI_VERSION ||
        response.operation != operation || response.status != 0 ||
        response.resource != wire_resource ||
        response.target_process_id != target_process_id ||
        response.target_instance_cookie != target_instance_cookie ||
        response.reserved0 != 0u || response.reserved[0] != 0u ||
        response.reserved[1] != 0u || response.current > response.maximum ||
        (response.current > (uint64_t)(rlim_t)~(rlim_t)0 &&
         !(resource == RLIMIT_AS && response.current == UINT64_MAX)) ||
        (response.maximum > (uint64_t)(rlim_t)~(rlim_t)0 &&
         !(resource == RLIMIT_AS && response.maximum == UINT64_MAX)) ||
        (operation == RIN_RESOURCE_LIMIT_CALL_SET &&
         (response.current != request.new_current ||
          response.maximum != request.new_maximum))) {
        errno = EIO;
        return -1;
    }
    if (old_limit) {
        old_limit->rlim_cur = (rlim_t)response.current;
        old_limit->rlim_max = (rlim_t)response.maximum;
    }
    return 0;
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * rusage構造体 (sys/wait.hでも定義)
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _RUSAGE_DEFINED
#define _RUSAGE_DEFINED

struct rusage {
    struct timeval ru_utime;  /* ユーザーCPU時間 */
    struct timeval ru_stime;  /* システムCPU時間 */
    long ru_maxrss;           /* 最大RSS (キロバイト) */
    long ru_ixrss;            /* 共有メモリサイズ */
    long ru_idrss;            /* 非共有データサイズ */
    long ru_isrss;            /* 非共有スタックサイズ */
    long ru_minflt;           /* ソフトページフォルト */
    long ru_majflt;           /* ハードページフォルト */
    long ru_nswap;            /* スワップ */
    long ru_inblock;          /* ブロック入力 */
    long ru_oublock;          /* ブロック出力 */
    long ru_msgsnd;           /* 送信メッセージ */
    long ru_msgrcv;           /* 受信メッセージ */
    long ru_nsignals;         /* 受信シグナル */
    long ru_nvcsw;            /* 自発的コンテキストスイッチ */
    long ru_nivcsw;           /* 非自発的コンテキストスイッチ */
};

#define RUSAGE_SELF     0
#define RUSAGE_CHILDREN (-1)
#define RUSAGE_THREAD   1

#endif /* _RUSAGE_DEFINED */

/* ═══════════════════════════════════════════════════════════════
 * 優先度定数
 * ═══════════════════════════════════════════════════════════════*/

#define PRIO_MIN     (-20)
#define PRIO_MAX     20

#define PRIO_PROCESS 0
#define PRIO_PGRP    1
#define PRIO_USER    2

/* ═══════════════════════════════════════════════════════════════
 * リソース制限関数
 * ═══════════════════════════════════════════════════════════════*/

/* getrlimit - リソース制限を取得
 *
 * The current process FD table has one published, enforced capacity.  Do not
 * manufacture values for resource classes whose kernel owners do not exist. */
static inline int getrlimit(int resource, struct rlimit* rlim) {
    int hook_result;
    if (resource < 0 || resource >= RLIMIT_NLIMITS) {
        errno = EINVAL;
        return -1;
    }
    if (!rlim) {
        errno = EFAULT;
        return -1;
    }
    hook_result = __rin_resource_limit_get_hook(resource, rlim);
    if (hook_result <= 0) return hook_result;
#if RIN_RESOURCE_LIMIT_PRODUCT_SYSCALL
    hook_result = __rin_resource_limit_wire_call(
        RIN_RESOURCE_LIMIT_CALL_GET, 0u, 0u, 0u, resource, NULL, rlim);
    if (hook_result <= 0) return hook_result;
#endif
    if (resource == RLIMIT_MEMLOCK) {
        rlim->rlim_cur = RIN_RLIMIT_MEMLOCK_DEFAULT;
        rlim->rlim_max = RIN_RLIMIT_MEMLOCK_DEFAULT;
        return 0;
    }
    if (resource != RLIMIT_NOFILE) {
        errno = ENOSYS;
        return -1;
    }
    rlim->rlim_cur = (rlim_t)OPEN_MAX;
    rlim->rlim_max = (rlim_t)OPEN_MAX;
    return 0;
}

/* setrlimit - リソース制限を設定 */
static inline int setrlimit(int resource, const struct rlimit* rlim) {
    int hook_result;
    if (resource < 0 || resource >= RLIMIT_NLIMITS) {
        errno = EINVAL;
        return -1;
    }
    if (!rlim) {
        errno = EFAULT;
        return -1;
    }
    if (rlim->rlim_cur > rlim->rlim_max) {
        errno = EINVAL;
        return -1;
    }
    hook_result = __rin_resource_limit_set_hook(resource, rlim);
    if (hook_result <= 0) return hook_result;
#if RIN_RESOURCE_LIMIT_PRODUCT_SYSCALL
    hook_result = __rin_resource_limit_wire_call(
        RIN_RESOURCE_LIMIT_CALL_SET, RIN_RESOURCE_LIMIT_CALL_FLAG_SET,
        0u, 0u, resource, rlim, NULL);
    if (hook_result <= 0) return hook_result;
#endif
    /* The process FD table has a fixed, enforced capacity.  Accept an exact
     * no-op publication of that limit so compatibility callers can use the
     * usual getrlimit/adjust/setrlimit sequence without claiming that a
     * different bound was installed. */
    if (resource == RLIMIT_NOFILE &&
        rlim->rlim_cur == (rlim_t)OPEN_MAX &&
        rlim->rlim_max == (rlim_t)OPEN_MAX) {
        return 0;
    }
    errno = ENOSYS;
    return -1;
}

/* prlimit - プロセスのリソース制限を取得/設定 */
static inline int prlimit(pid_t pid, int resource,
                          const struct rlimit* new_limit,
                          struct rlimit* old_limit) {
    if (pid < 0) {
        errno = EINVAL;
        return -1;
    }
#if defined(__GNUC__) || defined(__clang__)
    if (rin_resource_limit_prlimit != NULL)
        return rin_resource_limit_prlimit(pid, resource, new_limit, old_limit);
#endif
#if RIN_RESOURCE_LIMIT_PRODUCT_SYSCALL
    if (pid == 0 && (new_limit != NULL || old_limit != NULL)) {
        if (new_limit && new_limit->rlim_cur > new_limit->rlim_max) {
            errno = EINVAL;
            return -1;
        }
        return __rin_resource_limit_wire_call(
            RIN_RESOURCE_LIMIT_CALL_PRLIMIT,
            new_limit ? RIN_RESOURCE_LIMIT_CALL_FLAG_SET : 0u,
            0u, 0u, resource, new_limit, old_limit);
    }
#endif
    if (pid != 0 || new_limit) {
        errno = ENOSYS;
        return -1;
    }
    return getrlimit(resource, old_limit);
}

/* ═══════════════════════════════════════════════════════════════
 * リソース使用量関数
 * ═══════════════════════════════════════════════════════════════*/

/* getrusage - リソース使用量を取得 */
#ifndef _RIN_GETRUSAGE_SYSCALL2
#define _RIN_GETRUSAGE_SYSCALL2(number, argument1, argument2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2))
#endif

/* getrusage exposes an int status but receives a target-width syscall word.
 * Keep the errno published by POSIXization, reject unknown positive status,
 * and never truncate a result that cannot be represented by int. */
static inline int __rin_getrusage_status_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    if (result > (intptr_t)INT_MAX || result < (intptr_t)INT_MIN) {
        errno = EOVERFLOW;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static inline int getrusage(int who, struct rusage* usage) {
    if (who != RUSAGE_SELF && who != RUSAGE_CHILDREN &&
        who != RUSAGE_THREAD) {
        errno = EINVAL;
        return -1;
    }
    if (!usage) {
        errno = EFAULT;
        return -1;
    }
    return __rin_getrusage_status_result(_RIN_GETRUSAGE_SYSCALL2(
        SYS_GETRUSAGE, (uintptr_t)(intptr_t)who, (uintptr_t)usage));
}

/* ═══════════════════════════════════════════════════════════════
 * 優先度関数
 * ═══════════════════════════════════════════════════════════════*/

/* getpriority - プロセス優先度を取得 */
static inline int getpriority(int which, id_t who) {
#if RIN_PROCESS_PRIORITY_PRODUCT_SYSCALL
    intptr_t raw;
    if ((which != PRIO_PROCESS && which != PRIO_PGRP &&
         which != PRIO_USER) || (int)who < 0) {
        errno = EINVAL;
        return -1;
    }
    raw = __rin_syscall_posixize(_RIN_PRIORITY_SYSCALL2(
        (uintptr_t)SYS_GETPRIORITY, (uintptr_t)(intptr_t)which,
        (uintptr_t)(intptr_t)(int)who));
    if (raw == -1) return -1;
    if (raw < 0 || raw > 40) {
        errno = EIO;
        return -1;
    }
    return 20 - (int)raw;
#else
    (void)which;
    (void)who;
    errno = ENOSYS;
    return -1;
#endif
}

/* setpriority - プロセス優先度を設定 */
static inline int setpriority(int which, id_t who, int prio) {
#if RIN_PROCESS_PRIORITY_PRODUCT_SYSCALL
    intptr_t raw;
    if ((which != PRIO_PROCESS && which != PRIO_PGRP &&
         which != PRIO_USER) || (int)who < 0 ||
        prio < PRIO_MIN || prio > PRIO_MAX) {
        errno = EINVAL;
        return -1;
    }
    raw = __rin_syscall_posixize(_RIN_PRIORITY_SYSCALL3(
        (uintptr_t)SYS_SETPRIORITY, (uintptr_t)(intptr_t)which,
        (uintptr_t)(intptr_t)(int)who, (uintptr_t)(intptr_t)prio));
    if (raw == -1) return -1;
    if (raw != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
#else
    (void)which;
    (void)who;
    (void)prio;
    errno = ENOSYS;
    return -1;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_RESOURCE_H */
