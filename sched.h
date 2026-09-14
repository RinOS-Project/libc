/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - sched.h
 * プロセススケジューリング (カーネルsched.c連携)
 */

#ifndef _SCHED_H
#define _SCHED_H

#include "stddef.h"
#include "sys/types.h"
#include "time.h"  /* for struct timespec */
#include "errno.h"
#include "limits.h"
#include <rin/thread/sched_abi.h>

/* Caller-provided syscall hooks are the explicit opt-in for the Rin POSIX
 * scheduler surface in hosted tests.  Otherwise a hosted pthread provider
 * owns the overlapping sched_* declarations. */
#if defined(_RIN_SCHED_SYSCALL0) || defined(_RIN_SCHED_SYSCALL1) || \
    defined(_RIN_SCHED_SYSCALL2) || defined(_RIN_SCHED_SYSCALL3)
#define RIN_SCHED_CUSTOM_SYSCALL_HOOK 1
#endif

#if (defined(RIN_FREESTANDING) && RIN_FREESTANDING) || \
    !defined(__STDC_HOSTED__) || !__STDC_HOSTED__ || \
    defined(RIN_SCHED_CUSTOM_SYSCALL_HOOK) || !defined(WIN_PTHREADS_H)
#define RIN_SCHED_TARGET_FUNCTIONS 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * スケジューリングポリシー (POSIX)
 * ═══════════════════════════════════════════════════════════════*/

#define SCHED_OTHER     0   /* 通常のタイムシェアリング */
#define SCHED_FIFO      1   /* 先入れ先出しリアルタイム */
#define SCHED_RR        2   /* ラウンドロビンリアルタイム */
#define SCHED_BATCH     3   /* バッチ処理 */
#define SCHED_IDLE      5   /* アイドル優先度 */
#define SCHED_DEADLINE  6   /* デッドラインスケジューリング */

/* ═══════════════════════════════════════════════════════════════
 * RinOS内部優先度 (カーネルSchedPriority対応)
 * ═══════════════════════════════════════════════════════════════*/

#define _SCHED_PRIO_IDLE     0
#define _SCHED_PRIO_LOW      1
#define _SCHED_PRIO_NORMAL   2
#define _SCHED_PRIO_HIGH     3
#define _SCHED_PRIO_REALTIME 4

/* ═══════════════════════════════════════════════════════════════
 * スケジューリングパラメータ
 * ═══════════════════════════════════════════════════════════════*/

#if defined(RIN_SCHED_TARGET_FUNCTIONS)
struct sched_param {
    int sched_priority;
};
#endif

/* ═══════════════════════════════════════════════════════════════
 * CPU集合 (cpuset)
 * ═══════════════════════════════════════════════════════════════*/

#define CPU_SETSIZE 1024

typedef struct {
    unsigned long __bits[CPU_SETSIZE / (8 * sizeof(unsigned long))];
} cpu_set_t;

/* Keep the fixed cpuset ABI, but never let a caller-controlled CPU number
 * index outside the 1024-bit object.  The old macros evaluated `cpu` more
 * than once and turned negative/oversized input into an out-of-bounds write.
 * Casting once to uint64_t makes negative signed values large and therefore
 * rejects them without signed arithmetic.  NULL sets are ignored/read as
 * empty, matching the library's failure-safe boundary instead of trapping. */
static inline int __rin_sched_cpu_valid(uint64_t cpu) {
    return cpu < (uint64_t)CPU_SETSIZE;
}

static inline void __rin_sched_cpu_set(uint64_t cpu, cpu_set_t* cpuset) {
    const uint64_t word_bits = (uint64_t)(8u * sizeof(unsigned long));
    if (!cpuset || !__rin_sched_cpu_valid(cpu)) return;
    cpuset->__bits[cpu / word_bits] |=
        (1UL << (unsigned int)(cpu % word_bits));
}

static inline void __rin_sched_cpu_clear(uint64_t cpu, cpu_set_t* cpuset) {
    const uint64_t word_bits = (uint64_t)(8u * sizeof(unsigned long));
    if (!cpuset || !__rin_sched_cpu_valid(cpu)) return;
    cpuset->__bits[cpu / word_bits] &=
        ~(1UL << (unsigned int)(cpu % word_bits));
}

static inline int __rin_sched_cpu_is_set(uint64_t cpu,
                                         const cpu_set_t* cpuset) {
    const uint64_t word_bits = (uint64_t)(8u * sizeof(unsigned long));
    if (!cpuset || !__rin_sched_cpu_valid(cpu)) return 0;
    return (cpuset->__bits[cpu / word_bits] &
            (1UL << (unsigned int)(cpu % word_bits))) != 0UL;
}

static inline void __rin_sched_cpu_zero(cpu_set_t* cpuset) {
    if (!cpuset) return;
    for (unsigned int i = 0u; i < sizeof(cpuset->__bits) /
                                 sizeof(cpuset->__bits[0]); ++i)
        cpuset->__bits[i] = 0UL;
}

static inline int __rin_sched_cpu_count(const cpu_set_t* cpuset) {
    int count = 0;
    if (!cpuset) return 0;
    for (unsigned int i = 0u; i < sizeof(cpuset->__bits) /
                                 sizeof(cpuset->__bits[0]); ++i) {
        unsigned long bits = cpuset->__bits[i];
        while (bits != 0UL) {
            ++count;
            bits &= bits - 1UL;
        }
    }
    return count;
}

#define CPU_SET(cpu, cpusetp) \
    __rin_sched_cpu_set((uint64_t)(cpu), (cpusetp))
#define CPU_CLR(cpu, cpusetp) \
    __rin_sched_cpu_clear((uint64_t)(cpu), (cpusetp))
#define CPU_ISSET(cpu, cpusetp) \
    __rin_sched_cpu_is_set((uint64_t)(cpu), (cpusetp))
#define CPU_ZERO(cpusetp) __rin_sched_cpu_zero((cpusetp))
#define CPU_COUNT(cpusetp) __rin_sched_cpu_count((cpusetp))

/* ═══════════════════════════════════════════════════════════════
 * カーネルスケジューラAPI (syscall経由)
 * ═══════════════════════════════════════════════════════════════*/

#include "sys/syscall.h"

#ifndef _RIN_SCHED_SYSCALL0
#define _RIN_SCHED_SYSCALL0(number) \
    _syscall0((uintptr_t)(number))
#endif

#ifndef _RIN_SCHED_SYSCALL1
#define _RIN_SCHED_SYSCALL1(number, argument1) \
    _syscall1((uintptr_t)(number), (uintptr_t)(argument1))
#endif

#ifndef _RIN_SCHED_SYSCALL3
#define _RIN_SCHED_SYSCALL3(number, argument1, argument2, argument3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3))
#endif

#ifndef _RIN_SCHED_SYSCALL2
#define _RIN_SCHED_SYSCALL2(number, argument1, argument2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2))
#endif

static inline intptr_t _rin_sched_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    return result;
}

static inline int _rin_sched_status(intptr_t result) {
    result = _rin_sched_result(result);
    if (result < 0) return -1;
    if (result != 0) {
        errno = (uintptr_t)result > (uintptr_t)INT_MAX ? EOVERFLOW : EIO;
        return -1;
    }
    return 0;
}

/* SYS_SCHED_YIELD = 132, SYS_SLEEP = 62, SYS_GETPID = 4 */
static inline void rin_sched_yield(void) {
    (void)_RIN_SCHED_SYSCALL0(SYS_SCHED_YIELD);
}

static inline void rin_sched_sleep(unsigned int ticks) {
    (void)_RIN_SCHED_SYSCALL1(SYS_SLEEP, (uintptr_t)ticks);
}

static inline int rin_sched_task_current_id(void) {
    intptr_t result = _rin_sched_result(_RIN_SCHED_SYSCALL0(SYS_GETPID));
    if (result < 0) return -1;
    if (result > (intptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)result;
}

/* カーネルタスク情報取得 (優先度) - SchedTask*はvoid*として扱う */
void* rin_sched_task_get(int task_id);
void* rin_sched_task_current(void);
int rin_sched_task_get_priority(void* task);
void rin_sched_task_set_priority(void* task, int priority);

/* システム情報取得 */
unsigned int rin_sched_get_ticks(void);

/* ═══════════════════════════════════════════════════════════════
 * 内部ヘルパー: POSIXポリシー <-> RinOS優先度変換
 * ═══════════════════════════════════════════════════════════════*/

static inline int _policy_to_rinos_priority(int policy) {
    switch (policy) {
        case SCHED_FIFO:
        case SCHED_RR:
        case SCHED_DEADLINE:
            return _SCHED_PRIO_REALTIME;
        case SCHED_BATCH:
            return _SCHED_PRIO_LOW;
        case SCHED_IDLE:
            return _SCHED_PRIO_IDLE;
        case SCHED_OTHER:
        default:
            return _SCHED_PRIO_NORMAL;
    }
}

static inline int _rinos_priority_to_policy(int priority) {
    switch (priority) {
        case _SCHED_PRIO_REALTIME:
            return SCHED_RR;
        case _SCHED_PRIO_HIGH:
            return SCHED_RR;
        case _SCHED_PRIO_LOW:
            return SCHED_BATCH;
        case _SCHED_PRIO_IDLE:
            return SCHED_IDLE;
        case _SCHED_PRIO_NORMAL:
        default:
            return SCHED_OTHER;
    }
}

/* ═══════════════════════════════════════════════════════════════
 * スケジューリング関数 (カーネル連携)
 * ═══════════════════════════════════════════════════════════════*/

#if defined(RIN_SCHED_TARGET_FUNCTIONS)
extern uint32_t platform_thread_current_tid(void);
extern int platform_thread_sched_rr_interval(void* thread,
                                             uint32_t* milliseconds);
/* CPUを明け渡す (カーネルsched_yield呼び出し) */
#ifndef __SCHED_YIELD_DEFINED
#define __SCHED_YIELD_DEFINED
static inline int sched_yield(void) {
    return _rin_sched_status(_RIN_SCHED_SYSCALL0(SYS_SCHED_YIELD));
}
#endif

/* スケジューリングポリシーを取得 */
static inline int sched_getscheduler(pid_t pid) {
#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
    RinThreadSchedParametersV1 parameters = {
        (uint32_t)sizeof(parameters), RIN_THREAD_SCHED_ABI_VERSION, 0u,
        SCHED_OTHER, 0, 0u};
    intptr_t result;
    uint32_t thread_id;
    if (pid < 0) { errno = ESRCH; return -1; }
    if ((uint64_t)pid > UINT32_MAX) { errno = EOVERFLOW; return -1; }
    if (pid == 0) {
        result = __rin_syscall_posixize(
            _RIN_SCHED_SYSCALL0(SYS_GETTID));
        if (result < 0) return -1;
        if (result == 0 || (uintptr_t)result > UINT32_MAX) {
            errno = result == 0 ? ESRCH : EOVERFLOW;
            return -1;
        }
        thread_id = (uint32_t)result;
    } else {
        thread_id = (uint32_t)pid;
    }
    result = _rin_sched_result(_RIN_SCHED_SYSCALL2(
        SYS_THREAD_SCHED_GET, thread_id, &parameters));
    if (result < 0) return -1;
    if (result != 0 || parameters.struct_size != sizeof(parameters) ||
        parameters.version != RIN_THREAD_SCHED_ABI_VERSION ||
        parameters.reserved0 != 0u || parameters.reserved1 != 0u) {
        errno = EIO;
        return -1;
    }
    return parameters.policy;
#elif defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    void* task = rin_sched_task_get((int)pid);
    if (!task) {
        /* 現在のタスクを取得 */
        task = rin_sched_task_current();
    }
    if (task) {
        int priority = rin_sched_task_get_priority(task);
        return _rinos_priority_to_policy(priority);
    }
    return -1;
#else
    (void)pid;
    errno = ENOSYS;
    return -1;
#endif
}

/* スケジューリングポリシーを設定 */
static inline int sched_setscheduler(pid_t pid, int policy, const struct sched_param* param) {
#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
    RinThreadSchedParametersV1 parameters;
    intptr_t result;
    uint32_t thread_id;
    if (!param || pid < 0) { errno = EINVAL; return -1; }
    if ((uint64_t)pid > UINT32_MAX) { errno = EOVERFLOW; return -1; }
    thread_id = pid == 0 ? 0u : (uint32_t)pid;
    if (thread_id == 0u) {
        result = _rin_sched_result(_RIN_SCHED_SYSCALL0(SYS_GETTID));
        if (result < 0 || result == 0 || (uintptr_t)result > UINT32_MAX) {
            if (result >= 0) errno = result == 0 ? ESRCH : EOVERFLOW;
            return -1;
        }
        thread_id = (uint32_t)result;
    }
    parameters.struct_size = (uint32_t)sizeof(parameters);
    parameters.version = RIN_THREAD_SCHED_ABI_VERSION;
    parameters.reserved0 = 0u;
    parameters.policy = policy;
    parameters.priority = param->sched_priority;
    parameters.reserved1 = 0u;
    result = _rin_sched_result(_RIN_SCHED_SYSCALL2(
        SYS_THREAD_SCHED_SET, thread_id, &parameters));
    if (result < 0) return -1;
    if (result != 0) { errno = EIO; return -1; }
    return 0;
#elif defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    void* task = rin_sched_task_get((int)pid);
    if (!task && pid == 0) {
        task = rin_sched_task_current();
    }
    if (task) {
        int priority = _policy_to_rinos_priority(policy);
        /* パラメータから優先度を調整 */
        if (param && param->sched_priority > 0) {
            if (param->sched_priority >= 50) {
                priority = _SCHED_PRIO_REALTIME;
            } else if (param->sched_priority >= 25) {
                priority = _SCHED_PRIO_HIGH;
            }
        }
        rin_sched_task_set_priority(task, priority);
        return 0;
    }
    return -1;
#else
    (void)pid;
    (void)policy;
    (void)param;
    errno = ENOSYS;
    return -1;
#endif
}

/* スケジューリングパラメータを取得 */
static inline int sched_getparam(pid_t pid, struct sched_param* param) {
    if (!param) {
        errno = EINVAL;
        return -1;
    }

#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
    RinThreadSchedParametersV1 parameters = {
        (uint32_t)sizeof(parameters), RIN_THREAD_SCHED_ABI_VERSION, 0u,
        SCHED_OTHER, 0, 0u};
    intptr_t result;
    uint32_t thread_id = pid == 0 ? 0u : (uint32_t)pid;
    if (pid < 0) { errno = ESRCH; return -1; }
    if ((uint64_t)pid > UINT32_MAX) { errno = EOVERFLOW; return -1; }
    if (thread_id == 0u) {
        result = _rin_sched_result(_RIN_SCHED_SYSCALL0(SYS_GETTID));
        if (result < 0 || result == 0 || (uintptr_t)result > UINT32_MAX) {
            if (result >= 0) errno = result == 0 ? ESRCH : EOVERFLOW;
            return -1;
        }
        thread_id = (uint32_t)result;
    }
    result = _rin_sched_result(_RIN_SCHED_SYSCALL2(
        SYS_THREAD_SCHED_GET, thread_id, &parameters));
    if (result < 0) return -1;
    if (result != 0 || parameters.struct_size != sizeof(parameters) ||
        parameters.version != RIN_THREAD_SCHED_ABI_VERSION ||
        parameters.reserved0 != 0u || parameters.reserved1 != 0u) {
        errno = EIO;
        return -1;
    }
    param->sched_priority = parameters.priority;
    return 0;
#elif defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    void* task = rin_sched_task_get((int)pid);
    if (!task && pid == 0) {
        task = rin_sched_task_current();
    }
    if (task) {
        int priority = rin_sched_task_get_priority(task);
        /* RinOS優先度(0-4)をPOSIX優先度(0-99)にマッピング */
        param->sched_priority = priority * 25;
        return 0;
    }
    return -1;
#else
    (void)pid;
    errno = ENOSYS;
    return -1;
#endif
}

/* スケジューリングパラメータを設定 */
static inline int sched_setparam(pid_t pid, const struct sched_param* param) {
    if (!param) {
        errno = EINVAL;
        return -1;
    }

#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
    int policy = sched_getscheduler(pid);
    if (policy < 0) return -1;
    return sched_setscheduler(pid, policy, param);
#elif defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    void* task = rin_sched_task_get((int)pid);
    if (!task && pid == 0) {
        task = rin_sched_task_current();
    }
    if (task) {
        /* POSIX優先度(0-99)をRinOS優先度(0-4)にマッピング */
        int priority = param->sched_priority / 25;
        if (priority > _SCHED_PRIO_REALTIME) priority = _SCHED_PRIO_REALTIME;
        rin_sched_task_set_priority(task, priority);
        return 0;
    }
    return -1;
#else
    (void)pid;
    errno = ENOSYS;
    return -1;
#endif
}

/* 最大優先度を取得 */
static inline int sched_get_priority_max(int policy) {
    switch (policy) {
        case SCHED_FIFO:
        case SCHED_RR:
        case SCHED_DEADLINE:
            return 99;
        case SCHED_OTHER:
        case SCHED_BATCH:
        case SCHED_IDLE:
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}

/* 最小優先度を取得 */
static inline int sched_get_priority_min(int policy) {
    switch (policy) {
        case SCHED_FIFO:
        case SCHED_RR:
        case SCHED_DEADLINE:
            return 1;
        case SCHED_OTHER:
        case SCHED_BATCH:
        case SCHED_IDLE:
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}

/* タイムスライスを取得 */
static inline int sched_rr_get_interval(pid_t pid, struct timespec* interval) {
    if (!interval) {
        errno = EINVAL;
        return -1;
    }
#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
    uint32_t milliseconds = 0u;
    intptr_t result;
    uint32_t thread_id = pid == 0 ? 0u : (uint32_t)pid;
    if (pid < 0) { errno = ESRCH; return -1; }
    if ((uint64_t)pid > UINT32_MAX) { errno = EOVERFLOW; return -1; }
    if (thread_id == 0u) {
        result = _rin_sched_result(_RIN_SCHED_SYSCALL0(SYS_GETTID));
        if (result < 0 || result == 0 || (uintptr_t)result > UINT32_MAX) {
            if (result >= 0) errno = result == 0 ? ESRCH : EOVERFLOW;
            return -1;
        }
        thread_id = (uint32_t)result;
    }
    result = _rin_sched_result(_RIN_SCHED_SYSCALL2(
        SYS_THREAD_SCHED_RR_INTERVAL, thread_id, &milliseconds));
    if (result < 0) return -1;
    if (result != 0 || milliseconds == 0u) { errno = EIO; return -1; }
    interval->tv_sec = (time_t)(milliseconds / 1000u);
    interval->tv_nsec = (long)(milliseconds % 1000u) * 1000000L;
    return 0;
#elif defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    uint32_t milliseconds = 0u;
    int result;
    result = platform_thread_sched_rr_interval(
        (void*)(uintptr_t)(pid == 0 ? platform_thread_current_tid() : pid),
        &milliseconds);
    if (result != 0) {
        errno = result;
        return -1;
    }
    interval->tv_sec = (time_t)(milliseconds / 1000u);
    interval->tv_nsec = (long)(milliseconds % 1000u) * 1000000L;
    return 0;
#else
    (void)pid;
    errno = ENOSYS;
    return -1;
#endif
}

/* CPU affinity を取得 */
static inline int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t* mask) {
    intptr_t result;
    if (!mask || cpusetsize != sizeof(cpu_set_t)) {
        errno = EINVAL;
        return -1;
    }
    result = _rin_sched_result(_RIN_SCHED_SYSCALL3(
        SYS_SCHED_GETAFFINITY, (uintptr_t)pid, cpusetsize, (uintptr_t)mask));
    return result < 0 ? -1 : 0;
}

/* CPU affinity を設定 */
static inline int sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t* mask) {
    intptr_t result;
    if (!mask || cpusetsize != sizeof(cpu_set_t)) {
        errno = EINVAL;
        return -1;
    }
    result = _rin_sched_result(_RIN_SCHED_SYSCALL3(
        SYS_SCHED_SETAFFINITY, (uintptr_t)pid, cpusetsize, (uintptr_t)mask));
    return result < 0 ? -1 : 0;
}

/* 現在実行中のCPU番号を取得 */
static inline int sched_getcpu(void) {
    intptr_t cpu = _rin_sched_result(_RIN_SCHED_SYSCALL0(SYS_GETCPU));
    if (cpu < 0) return -1;
    if (cpu > (intptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)cpu;
}
#endif /* RIN_SCHED_TARGET_FUNCTIONS */

#ifdef __cplusplus
}
#endif

#undef RIN_SCHED_TARGET_FUNCTIONS
#undef RIN_SCHED_CUSTOM_SYSCALL_HOOK

#endif /* _SCHED_H */
