/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - pthread.h
 * POSIX Threads API
 */

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    (defined(__unix__) || defined(__APPLE__)) && \
    (defined(__GNUC__) || defined(__clang__)) && \
    !defined(_RIN_PTHREAD_SYSCALL0) && \
    !defined(_RIN_PTHREAD_SELF_SYSCALL0) && \
    !defined(_RIN_PTHREAD_STACK_INFO_SYSCALL3) && \
    !defined(_RIN_PTHREAD_SIGMASK_SYSCALL4) && \
    !defined(_RIN_PTHREAD_NAME_SYSCALL1) && \
    !defined(_RIN_SCHED_SYSCALL0) && \
    !defined(_RIN_SCHED_SYSCALL1) && \
    !defined(_RIN_SCHED_SYSCALL2) && \
    !defined(_RIN_SCHED_SYSCALL3)
#include_next <pthread.h>
#else

#ifndef _PTHREAD_H
#define _PTHREAD_H

/* Let C++ wrappers distinguish this ABI from a hosted provider even when
 * both headers use the conventional `_PTHREAD_H` include guard. */
#define RIN_LIBC_PTHREAD_HEADER 1

#include "stddef.h"
#include "stdint.h"
#include "limits.h"
#include "errno.h"
#include "time.h"
#include "sched.h"
#include "signal.h"
#include "sys/syscall.h"
#include "linux/futex.h"
#include <rin/thread/name_abi.h>
#include <rin/thread/sched_abi.h>

#ifndef _RIN_PTHREAD_SYSCALL0
#define _RIN_PTHREAD_SYSCALL0(number) _syscall0((uintptr_t)(number))
#endif
#ifndef _RIN_PTHREAD_SELF_SYSCALL0
#define _RIN_PTHREAD_SELF_SYSCALL0(number) _RIN_PTHREAD_SYSCALL0(number)
#endif
#ifndef _RIN_PTHREAD_STACK_INFO_SYSCALL3
#define _RIN_PTHREAD_STACK_INFO_SYSCALL3(number, argument1, argument2, argument3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3))
#endif
#ifndef _RIN_PTHREAD_SIGMASK_SYSCALL4
#define _RIN_PTHREAD_SIGMASK_SYSCALL4(number, argument1, argument2, argument3, \
                                      argument4) \
    _syscall4((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3), \
              (uintptr_t)(argument4))
#endif
#ifndef _RIN_PTHREAD_NAME_SYSCALL1
#define _RIN_PTHREAD_NAME_SYSCALL1(number, argument) \
    _syscall1((uintptr_t)(number), (uintptr_t)(argument))
#endif
#ifndef _RIN_PTHREAD_SCHED_SYSCALL2
#define _RIN_PTHREAD_SCHED_SYSCALL2(number, argument1, argument2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2))
#endif

/* Pthread entry points return either a POSIX error number directly or a
 * syscall-style 0/-1 status.  Keep the raw target-width word until the
 * contract is selected; an LLP64 `unsigned long` intermediate can truncate a
 * large negative response and accidentally report success. */
static inline int _rin_pthread_error_from_result(intptr_t raw_result) {
    if (raw_result >= 0)
        return raw_result == 0 ? 0 : EIO;
    if (raw_result >= (intptr_t)-4095)
        return (int)-raw_result;
    return EIO;
}

static inline int _rin_pthread_status_from_result(intptr_t raw_result) {
    int error = _rin_pthread_error_from_result(raw_result);
    if (error == 0)
        return 0;
    errno = error;
    return -1;
}

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════*/

#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN 16384  /* Minimum stack size */
#endif

#ifndef _RIN_PTHREAD_DEFAULT_GUARDSIZE
#define _RIN_PTHREAD_DEFAULT_GUARDSIZE ((size_t)4096u)
#endif

/* ═══════════════════════════════════════════════════════════════
 * 型定義
 * ═══════════════════════════════════════════════════════════════*/

/* A thread handle may carry a kernel pointer in freestanding builds.  Keep
 * it target-word sized: `unsigned long` is only 32 bits on LLP64 hosts. */
#ifndef _RIN_PTHREAD_T_DEFINED
#define _RIN_PTHREAD_T_DEFINED
typedef uintptr_t pthread_t;
#endif
typedef unsigned int pthread_key_t;

/* `pthread_self` has no error return channel.  A malformed or failed
 * GETPID result must therefore become the reserved zero handle while errno
 * carries the diagnostic; never reinterpret a negative target-word as a
 * live pthread_t. */
static inline pthread_t _rin_pthread_self_from_result(intptr_t raw_result) {
    if (raw_result <= 0) {
        errno = raw_result < 0 ? _rin_pthread_error_from_result(raw_result) : EIO;
        return (pthread_t)0;
    }
    return (pthread_t)(uintptr_t)raw_result;
}

typedef struct {
    int detachstate;
    int scope;
    size_t stacksize;
    void* stackaddr;
    int inheritsched;
    int schedpolicy;
    int sched_priority;
    size_t guardsize;
} pthread_attr_t;

typedef struct {
    volatile int locked;
    pthread_t owner;
    int type;
    int recursion;
#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    void* kernel_mutex;
#endif
} pthread_mutex_t;

/* Recursive/error-check paths inspect ownership before taking the lock, so
 * owner publication must be atomic even while another thread is acquiring. */
static inline pthread_t __rin_pthread_mutex_owner_load(
    const pthread_mutex_t* mutex) {
    return __atomic_load_n(&mutex->owner, __ATOMIC_ACQUIRE);
}

static inline void __rin_pthread_mutex_owner_store(
    pthread_mutex_t* mutex, pthread_t owner) {
    __atomic_store_n(&mutex->owner, owner, __ATOMIC_RELEASE);
}

typedef struct {
    int type;
    int pshared;
} pthread_mutexattr_t;

typedef struct {
    volatile unsigned int generation;   /* 世代カウンタ（wakeup lost防止） */
    volatile int waiting;
    int clock_id;
    int pshared;
} pthread_cond_t;

typedef struct {
    int clock_id;
    int pshared;
} pthread_condattr_t;

typedef int pthread_once_t;

typedef struct {
    int readers;
    int writer;
    int waiting_readers;
    int waiting_writers;
    pthread_mutex_t mutex;
    pthread_cond_t can_read;
    pthread_cond_t can_write;
} pthread_rwlock_t;

typedef struct {
    int pshared;
} pthread_rwlockattr_t;

/* Shared-memory ticket word; active owners and waiters stay visible to destroy. */
typedef struct {
    volatile uint32_t state;
} pthread_spinlock_t;

#define RIN_PTHREAD_RWLOCK_WRITER_ACTIVE 0x1
#define RIN_PTHREAD_RWLOCK_PROCESS_SHARED 0x2

static inline int rin_pthread_rwlock_writer_active(
    const pthread_rwlock_t* rwlock) {
    return rwlock &&
           (rwlock->writer & RIN_PTHREAD_RWLOCK_WRITER_ACTIVE) != 0;
}

typedef struct {
    uint32_t magic;
    uint32_t count;
    volatile uint32_t waiting;
    volatile uint32_t generation;
    volatile uint32_t lock;
    volatile uint32_t lock_waiters;
    volatile uint32_t active_waiters;
    uint32_t pshared;
} pthread_barrier_t;

typedef struct {
    int pshared;
} pthread_barrierattr_t;

/* ═══════════════════════════════════════════════════════════════
 * 定数
 * ═══════════════════════════════════════════════════════════════*/

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

#define PTHREAD_MUTEX_NORMAL     0
#define PTHREAD_MUTEX_RECURSIVE  1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT    PTHREAD_MUTEX_NORMAL
#define PTHREAD_MUTEX_STALLED    0
#define PTHREAD_MUTEX_ROBUST     1
#define PTHREAD_PRIO_NONE        0
#define PTHREAD_PRIO_INHERIT     1
#define PTHREAD_PRIO_PROTECT     2

#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED  1
#define PTHREAD_BARRIER_SERIAL_THREAD (-1)

#define PTHREAD_ONCE_INIT 0

/* RinOS supports deferred cancellation at cancellation points and
 * asynchronous cancellation through its SIGCANCEL delivery path. */
#define PTHREAD_CANCEL_ENABLE        0
#define PTHREAD_CANCEL_DISABLE       1
#define PTHREAD_CANCEL_DEFERRED      0
#define PTHREAD_CANCEL_ASYNCHRONOUS  1
#define PTHREAD_CANCELED             ((void*)(intptr_t)-1)

#define PTHREAD_MUTEX_INITIALIZER { 0, 0, 0, 0 }
#define PTHREAD_COND_INITIALIZER  { 0, 0, CLOCK_REALTIME, PTHREAD_PROCESS_PRIVATE }
#define PTHREAD_RWLOCK_INITIALIZER { 0, 0, 0, 0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER }

/* ═══════════════════════════════════════════════════════════════
 * カーネル空間用API宣言 (カーネルビルドのみ)
 * ═══════════════════════════════════════════════════════════════*/

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
extern void  platform_thread_init(void);
extern void* platform_thread_create(void (*entry)(void*), void* arg);
extern void* platform_thread_create_result(void* (*entry)(void*), void* arg);
extern void* platform_thread_create_result_with_stack(
    void* (*entry)(void*), void* arg, void* stack, size_t stack_size,
    int detached);
extern void* platform_thread_create_result_with_stack_sched(
    void* (*entry)(void*), void* arg, void* stack, size_t stack_size,
    int detached, int policy, int priority);
extern void  platform_thread_join(void* thread);
extern int   platform_thread_join_result(void* thread, void** result_out);
extern int   platform_thread_detach(void* thread);
extern int   platform_thread_sched_get(void* thread, int* policy, int* priority);
extern int   platform_thread_sched_set(void* thread, int policy, int priority);
extern int   platform_thread_name_set(void* thread, const char* name);
extern int   platform_thread_name_get(void* thread, char* name, size_t length);
extern void  platform_thread_exit(void);
extern void  platform_thread_exit_with_result(void* result)
    __attribute__((noreturn));
extern void  platform_thread_yield(void);
extern uint32_t platform_thread_current_tid(void);

extern void* platform_mutex_create(void);
extern void* platform_mutex_create_pi(void);
extern void* platform_mutex_create_robust(int is_priority_inherit);
extern void  platform_mutex_lock(void* mutex);
extern int   platform_mutex_trylock(void* mutex);
extern int   platform_mutex_lock_robust(void* mutex);
extern int   platform_mutex_trylock_robust(void* mutex);
extern int   platform_mutex_consistent(void* mutex);
extern int   platform_mutex_unlock_robust(void* mutex);
extern int   platform_mutex_busy(void* mutex);
extern void  platform_mutex_unlock(void* mutex);
extern void  platform_mutex_destroy(void* mutex);
#endif

/* ═══════════════════════════════════════════════════════════════
 * スレッド属性
 * ═══════════════════════════════════════════════════════════════*/

/* RinOS threads are process-shared scheduler entities.  POSIX's
 * PTHREAD_SCOPE_PROCESS has no target ABI, so reject it instead of silently
 * accepting an attribute that would not affect pthread_create. */
#ifndef PTHREAD_SCOPE_SYSTEM
#define PTHREAD_SCOPE_SYSTEM 0
#endif
#ifndef PTHREAD_SCOPE_PROCESS
#define PTHREAD_SCOPE_PROCESS 1
#endif
#ifndef PTHREAD_INHERIT_SCHED
#define PTHREAD_INHERIT_SCHED 0
#endif
#ifndef PTHREAD_EXPLICIT_SCHED
#define PTHREAD_EXPLICIT_SCHED 1
#endif

static inline int pthread_getschedparam(pthread_t thread, int* policy,
                                        struct sched_param* param);
static inline int pthread_setschedparam(pthread_t thread, int policy,
                                        const struct sched_param* param);

static inline int pthread_attr_init(pthread_attr_t* attr) {
    if (!attr) return EINVAL;
    attr->detachstate = PTHREAD_CREATE_JOINABLE;
    attr->scope = PTHREAD_SCOPE_SYSTEM;
    attr->stacksize = 8 * 1024 * 1024;  /* 8 MiB default worker stack */
    attr->stackaddr = NULL;
    attr->inheritsched = PTHREAD_INHERIT_SCHED;
    attr->schedpolicy = SCHED_OTHER;
    attr->sched_priority = 0;
    attr->guardsize = _RIN_PTHREAD_DEFAULT_GUARDSIZE;
    return 0;
}

static inline int pthread_attr_destroy(pthread_attr_t* attr) {
    if (!attr) return EINVAL;
    attr->detachstate = PTHREAD_CREATE_JOINABLE;
    attr->scope = PTHREAD_SCOPE_SYSTEM;
    attr->stacksize = 0;
    attr->stackaddr = NULL;
    attr->inheritsched = PTHREAD_INHERIT_SCHED;
    attr->schedpolicy = SCHED_OTHER;
    attr->sched_priority = 0;
    attr->guardsize = 0;
    return 0;
}

static inline int pthread_attr_getguardsize(const pthread_attr_t* attr,
                                             size_t* guardsize) {
    if (!attr || !guardsize) return EINVAL;
    *guardsize = attr->guardsize;
    return 0;
}

static inline int pthread_attr_setguardsize(pthread_attr_t* attr,
                                             size_t guardsize) {
    if (!attr || guardsize > SIZE_MAX - (size_t)0xfffu) return EINVAL;
    attr->guardsize = guardsize;
    return 0;
}

static inline int pthread_attr_setscope(pthread_attr_t* attr, int scope) {
    if (!attr) return EINVAL;
    if (scope != PTHREAD_SCOPE_SYSTEM) return ENOTSUP;
    attr->scope = scope;
    return 0;
}

static inline int pthread_attr_getscope(const pthread_attr_t* attr, int* scope) {
    if (!attr || !scope) return EINVAL;
    *scope = attr->scope;
    return 0;
}

static inline int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize) {
    if (!attr || stacksize < PTHREAD_STACK_MIN) return EINVAL;
    attr->stacksize = stacksize;
    return 0;
}

static inline int pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stacksize) {
    if (!attr || !stacksize) return EINVAL;
    *stacksize = attr->stacksize;
    return 0;
}

static inline int pthread_attr_setdetachstate(pthread_attr_t* attr, int detachstate) {
    if (!attr) return EINVAL;
    if (detachstate != PTHREAD_CREATE_JOINABLE && detachstate != PTHREAD_CREATE_DETACHED)
        return EINVAL;
    attr->detachstate = detachstate;
    return 0;
}

static inline int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* detachstate) {
    if (!attr || !detachstate) return EINVAL;
    *detachstate = attr->detachstate;
    return 0;
}

static inline int pthread_attr_getstack(const pthread_attr_t* attr, void** stackaddr, size_t* stacksize) {
    if (!attr || !stackaddr || !stacksize) return EINVAL;
    *stackaddr = attr->stackaddr;
    *stacksize = attr->stacksize;
    return 0;
}

static inline int pthread_attr_setstack(pthread_attr_t* attr, void* stackaddr, size_t stacksize) {
    if (!attr || !stackaddr) return EINVAL;
    if (stacksize < PTHREAD_STACK_MIN) return EINVAL;
    if (stacksize > UINTPTR_MAX - (uintptr_t)stackaddr) return EINVAL;
    attr->stackaddr = stackaddr;
    attr->stacksize = stacksize;
    return 0;
}

static inline int _rin_pthread_sched_policy_supported(int policy) {
    switch (policy) {
        case SCHED_OTHER:
        case SCHED_FIFO:
        case SCHED_RR:
        case SCHED_BATCH:
        case SCHED_IDLE:
            return 1;
        case SCHED_DEADLINE:
            return 0;
        default:
            return 0;
    }
}

static inline int pthread_attr_setinheritsched(pthread_attr_t* attr,
                                                int inheritsched) {
    if (!attr || (inheritsched != PTHREAD_INHERIT_SCHED &&
                  inheritsched != PTHREAD_EXPLICIT_SCHED))
        return EINVAL;
    attr->inheritsched = inheritsched;
    return 0;
}

static inline int pthread_attr_getinheritsched(const pthread_attr_t* attr,
                                                int* inheritsched) {
    if (!attr || !inheritsched) return EINVAL;
    *inheritsched = attr->inheritsched;
    return 0;
}

static inline int pthread_attr_setschedpolicy(pthread_attr_t* attr,
                                               int policy) {
    if (!attr) return EINVAL;
    if (!_rin_pthread_sched_policy_supported(policy))
        return policy == SCHED_DEADLINE ? ENOTSUP : EINVAL;
    attr->schedpolicy = policy;
    return 0;
}

static inline int pthread_attr_getschedpolicy(const pthread_attr_t* attr,
                                               int* policy) {
    if (!attr || !policy) return EINVAL;
    *policy = attr->schedpolicy;
    return 0;
}

static inline int pthread_attr_setschedparam(pthread_attr_t* attr,
                                              const struct sched_param* param) {
    if (!attr || !param) return EINVAL;
    if (!_rin_pthread_sched_policy_supported(attr->schedpolicy))
        return attr->schedpolicy == SCHED_DEADLINE ? ENOTSUP : EINVAL;
    if (!_rin_sched_policy_priority_valid(attr->schedpolicy,
                                          param->sched_priority))
        return EINVAL;
    attr->sched_priority = param->sched_priority;
    return 0;
}

static inline int pthread_attr_getschedparam(const pthread_attr_t* attr,
                                              struct sched_param* param) {
    if (!attr || !param) return EINVAL;
    param->sched_priority = attr->sched_priority;
    return 0;
}

/* GNU extension: get attributes of running thread */
static inline int pthread_getattr_np(pthread_t thread, pthread_attr_t* attr) {
    const unsigned int known_flags =
        RIN_THREAD_STACK_INFO_FLAG_GROWS_DOWN |
        RIN_THREAD_STACK_INFO_FLAG_HAS_GUARD |
        RIN_THREAD_STACK_INFO_FLAG_DETACHED;
    if (!attr) return EINVAL;

    rin_thread_stack_info_v1 info;
    intptr_t ret = _RIN_PTHREAD_STACK_INFO_SYSCALL3(
        SYS_THREAD_STACK_INFO, (uintptr_t)thread,
        (uintptr_t)&info, (uintptr_t)sizeof(info));
    if (ret < 0) {
        return ret >= -(intptr_t)4095 ? (int)-ret : EIO;
    }
    if (ret != 0 || info.reserved != 0u ||
        (info.flags & ~known_flags) != 0u ||
        (info.flags & RIN_THREAD_STACK_INFO_FLAG_GROWS_DOWN) == 0u ||
        info.stack_base == 0u || info.stack_size == 0u ||
        info.stack_size > UINTPTR_MAX || info.stack_base > UINTPTR_MAX ||
        info.stack_size > UINTPTR_MAX - info.stack_base) {
        return EIO;
    }
    if ((info.flags & RIN_THREAD_STACK_INFO_FLAG_HAS_GUARD) != 0u) {
        if (info.guard_base == 0u || info.guard_size == 0u ||
            info.guard_size > UINTPTR_MAX || info.guard_base > UINTPTR_MAX ||
            info.guard_size > UINTPTR_MAX - info.guard_base ||
            info.guard_base + info.guard_size > info.stack_base) {
            return EIO;
        }
    } else if (info.guard_base != 0u || info.guard_size != 0u) {
        return EIO;
    }

    int policy = SCHED_OTHER;
    struct sched_param schedparam = {0};
    int sched_result = pthread_getschedparam(thread, &policy, &schedparam);
    if (sched_result != 0) return sched_result;

    attr->detachstate =
        (info.flags & RIN_THREAD_STACK_INFO_FLAG_DETACHED) != 0u
            ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE;
    attr->scope = PTHREAD_SCOPE_SYSTEM;
    attr->stackaddr = (void*)(uintptr_t)info.stack_base;
    attr->stacksize = (size_t)info.stack_size;
    attr->inheritsched = PTHREAD_EXPLICIT_SCHED;
    attr->schedpolicy = policy;
    attr->sched_priority = schedparam.sched_priority;
    attr->guardsize = (size_t)info.guard_size;
    return 0;
}

#if defined(RIN_USERSPACE) || defined(RIN_PTHREAD_RUNTIME_IMPLEMENTATION)
int pthread_setname_np(pthread_t thread, const char* name);
#else
static inline int pthread_setname_np(pthread_t thread, const char* name) {
#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    (void)thread;
    if (!name) return EINVAL;
    return platform_thread_name_set((void*)(uintptr_t)thread, name);
#else
    RinThreadNameCallV1 request;
    size_t length = 0u;
    intptr_t result;

    if (!name) return EINVAL;
    if (thread == 0u) return ESRCH;
    while (length < RIN_THREAD_NAME_CAPACITY && name[length] != '\0')
        ++length;
    if (length == RIN_THREAD_NAME_CAPACITY) return ERANGE;
    for (size_t index = 0u; index < sizeof(request); ++index)
        ((unsigned char*)&request)[index] = 0u;
    request.struct_size = (uint32_t)sizeof(request);
    request.version = RIN_THREAD_NAME_CALL_VERSION;
    request.operation = RIN_THREAD_NAME_OPERATION_SET;
    request.thread_id = (uint64_t)thread;
    for (size_t index = 0u; index < length; ++index)
        request.name[index] = name[index];
    result = _RIN_PTHREAD_NAME_SYSCALL1(SYS_THREAD_NAME_CALL, &request);
    return _rin_pthread_error_from_result(result);
#endif
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * スレッド関数
 * ═══════════════════════════════════════════════════════════════*/

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
static inline int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                                  void* (*start_routine)(void*), void* arg) {
    size_t stack_size = 8u * 1024u * 1024u;
    void* stack = NULL;
    int detached = 0;
    int policy = SCHED_OTHER;
    struct sched_param schedparam = {0};
    if (!thread || !start_routine) return EINVAL;
    if (attr) {
        if (attr->detachstate != PTHREAD_CREATE_JOINABLE &&
            attr->detachstate != PTHREAD_CREATE_DETACHED)
            return EINVAL;
        if (attr->scope == PTHREAD_SCOPE_PROCESS) return ENOTSUP;
        if (attr->scope != PTHREAD_SCOPE_SYSTEM) return EINVAL;
        if (attr->inheritsched == PTHREAD_EXPLICIT_SCHED) {
            if (!_rin_pthread_sched_policy_supported(attr->schedpolicy))
                return attr->schedpolicy == SCHED_DEADLINE ? ENOTSUP : EINVAL;
            if (!_rin_sched_policy_priority_valid(attr->schedpolicy,
                                                  attr->sched_priority))
                return EINVAL;
            policy = attr->schedpolicy;
            schedparam.sched_priority = attr->sched_priority;
        } else if (attr->inheritsched != PTHREAD_INHERIT_SCHED) {
            return EINVAL;
        } else {
            int result = pthread_getschedparam(pthread_self(), &policy,
                                               &schedparam);
            if (result != 0) return result;
        }
        stack_size = attr->stacksize;
        stack = attr->stackaddr;
        detached = attr->detachstate == PTHREAD_CREATE_DETACHED;
    } else {
        int result = pthread_getschedparam(pthread_self(), &policy,
                                           &schedparam);
        if (result != 0) return result;
    }
    if (!_rin_pthread_sched_policy_supported(policy))
        return policy == SCHED_DEADLINE ? ENOTSUP : EINVAL;
    if (!_rin_sched_policy_priority_valid(policy, schedparam.sched_priority))
        return EINVAL;
    if (stack_size < PTHREAD_STACK_MIN || stack_size > UINT32_MAX)
        return EINVAL;
    if (stack && stack_size > UINTPTR_MAX - (uintptr_t)stack)
        return EINVAL;
    void* kthread = platform_thread_create_result_with_stack_sched(
        start_routine, arg, stack, stack_size, detached, policy,
        schedparam.sched_priority);
    if (!kthread) return EAGAIN;
    *thread = (pthread_t)(uintptr_t)kthread;
    return 0;
}

static inline int pthread_join(pthread_t thread, void** retval) {
    return platform_thread_join_result((void*)(uintptr_t)thread, retval);
}

static inline int pthread_detach(pthread_t thread) {
    return platform_thread_detach((void*)(uintptr_t)thread);
}
#else
/* ユーザー空間: libc がABI実装を提供 */
extern int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                           void* (*start_routine)(void*), void* arg);
extern int pthread_join(pthread_t thread, void** retval);
extern int pthread_detach(pthread_t thread);
#endif

static inline pthread_t pthread_self(void) {
#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    return (pthread_t)platform_thread_current_tid();
#else
#if (defined(__x86_64__) || defined(_M_X64)) && \
    !defined(RIN_PTHREAD_FORCE_SYSCALL_SELF)
    /* `unsigned long` is only 32 bits in the Win64 ABI.  The TLS slot carries
     * a target-word identity, so keep the inline-asm destination the same
     * width as pthread_t on both LP64 and LLP64 hosts. */
    uintptr_t tid;
    __asm__ volatile("movq %%fs:0x10, %0" : "=r"(tid));
    return (pthread_t)tid;
#else
    return _rin_pthread_self_from_result(
        _RIN_PTHREAD_SELF_SYSCALL0(SYS_GETTID));
#endif
#endif
}

#if defined(RIN_USERSPACE) || defined(RIN_PTHREAD_RUNTIME_IMPLEMENTATION)
int pthread_getname_np(pthread_t thread, char* name, size_t len);
#else
static inline int pthread_getname_np(pthread_t thread, char* name, size_t len) {
#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    if (!name) return EINVAL;
    return platform_thread_name_get((void*)(uintptr_t)thread, name, len);
#else
    RinThreadNameCallV1 request;
    size_t terminator = RIN_THREAD_NAME_CAPACITY;
    intptr_t result;
    int result_error;

    if (!name) return EINVAL;
    if (len == 0u) return ERANGE;
    if (thread == 0u) return ESRCH;
    for (size_t index = 0u; index < sizeof(request); ++index)
        ((unsigned char*)&request)[index] = 0u;
    request.struct_size = (uint32_t)sizeof(request);
    request.version = RIN_THREAD_NAME_CALL_VERSION;
    request.operation = RIN_THREAD_NAME_OPERATION_GET;
    request.thread_id = (uint64_t)thread;
    result = _RIN_PTHREAD_NAME_SYSCALL1(SYS_THREAD_NAME_CALL, &request);
    result_error = _rin_pthread_error_from_result(result);
    if (result_error != 0)
        return result_error;
    if (request.struct_size != sizeof(request) ||
        request.version != RIN_THREAD_NAME_CALL_VERSION ||
        request.operation != RIN_THREAD_NAME_OPERATION_GET ||
        request.thread_id != (uint64_t)thread || request.reserved[0] != 0u ||
        request.reserved[1] != 0u) return EIO;
    for (size_t index = 0u; index < RIN_THREAD_NAME_CAPACITY; ++index) {
        if (request.name[index] == '\0') {
            terminator = index;
            break;
        }
    }
    if (terminator == RIN_THREAD_NAME_CAPACITY) return EIO;
    for (size_t index = terminator + 1u;
         index < RIN_THREAD_NAME_CAPACITY; ++index) {
        if (request.name[index] != '\0') return EIO;
    }
    if (len <= terminator) return ERANGE;
    for (size_t index = 0u; index <= terminator; ++index)
        name[index] = request.name[index];
    return 0;
#endif
}
#endif

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
static inline void pthread_exit(void* retval) {
    platform_thread_exit_with_result(retval);
    for (;;) {}  /* Never returns */
}
#else
extern void pthread_exit(void* retval) __attribute__((noreturn));
#endif

#ifndef __SCHED_YIELD_DEFINED
#define __SCHED_YIELD_DEFINED
static inline int sched_yield(void) {
#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    platform_thread_yield();
    return 0;
#else
    return _rin_pthread_status_from_result(
        _RIN_PTHREAD_SYSCALL0(SYS_SCHED_YIELD));
#endif
}
#endif

static inline int pthread_equal(pthread_t t1, pthread_t t2) {
    return t1 == t2;
}

/* ═══════════════════════════════════════════════════════════════
 * Mutex関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int pthread_mutexattr_init(pthread_mutexattr_t* attr) {
    if (!attr) return EINVAL;
    attr->type = PTHREAD_MUTEX_DEFAULT;
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

#define RIN_PTHREAD_MUTEXATTR_ROBUST_FLAG 0x100
#define RIN_PTHREAD_MUTEXATTR_PSHARED_MASK 0xff
#define RIN_PTHREAD_MUTEXATTR_TYPE_MASK 0xff
#define RIN_PTHREAD_MUTEXATTR_PRIO_INHERIT_FLAG 0x100
#define RIN_PTHREAD_MUTEX_ROBUST_FLAG 0x200

static inline int pthread_mutexattr_destroy(pthread_mutexattr_t* attr) {
    if (!attr) return EINVAL;
    attr->type = PTHREAD_MUTEX_DEFAULT;
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

static inline int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type) {
    if (!attr) return EINVAL;
    if (type < 0 || type > PTHREAD_MUTEX_ERRORCHECK) return EINVAL;
    attr->type = (attr->type & ~RIN_PTHREAD_MUTEXATTR_TYPE_MASK) | type;
    return 0;
}

static inline int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type) {
    if (!attr || !type) return EINVAL;
    *type = attr->type & RIN_PTHREAD_MUTEXATTR_TYPE_MASK;
    return 0;
}

static inline int pthread_mutexattr_setprotocol(pthread_mutexattr_t* attr,
                                                int protocol) {
    if (!attr) return EINVAL;
    if (protocol == PTHREAD_PRIO_NONE) {
        attr->type &= ~RIN_PTHREAD_MUTEXATTR_PRIO_INHERIT_FLAG;
        return 0;
    }
    if (protocol == PTHREAD_PRIO_INHERIT) {
        attr->type |= RIN_PTHREAD_MUTEXATTR_PRIO_INHERIT_FLAG;
        return 0;
    }
    if (protocol == PTHREAD_PRIO_PROTECT) return ENOTSUP;
    return EINVAL;
}

static inline int pthread_mutexattr_getprotocol(
    const pthread_mutexattr_t* attr, int* protocol) {
    if (!attr || !protocol) return EINVAL;
    *protocol = (attr->type & RIN_PTHREAD_MUTEXATTR_PRIO_INHERIT_FLAG) != 0
        ? PTHREAD_PRIO_INHERIT : PTHREAD_PRIO_NONE;
    return 0;
}

static inline int pthread_mutexattr_setpshared(pthread_mutexattr_t* attr, int pshared) {
    if (!attr) return EINVAL;
    if (pshared != PTHREAD_PROCESS_PRIVATE && pshared != PTHREAD_PROCESS_SHARED) return EINVAL;
#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    if (pshared == PTHREAD_PROCESS_SHARED) return ENOTSUP;
#endif
    attr->pshared = (attr->pshared & RIN_PTHREAD_MUTEXATTR_ROBUST_FLAG) |
                    pshared;
    return 0;
}

static inline int pthread_mutexattr_getpshared(const pthread_mutexattr_t* attr, int* pshared) {
    if (!attr || !pshared) return EINVAL;
    *pshared = attr->pshared & RIN_PTHREAD_MUTEXATTR_PSHARED_MASK;
    return 0;
}

static inline int pthread_mutexattr_setrobust(pthread_mutexattr_t* attr,
                                               int robust) {
    if (!attr || (robust != PTHREAD_MUTEX_STALLED &&
                  robust != PTHREAD_MUTEX_ROBUST))
        return EINVAL;
    attr->pshared = (attr->pshared & RIN_PTHREAD_MUTEXATTR_PSHARED_MASK) |
        (robust == PTHREAD_MUTEX_ROBUST ?
             RIN_PTHREAD_MUTEXATTR_ROBUST_FLAG : 0);
    return 0;
}

static inline int pthread_mutexattr_getrobust(const pthread_mutexattr_t* attr,
                                               int* robust) {
    if (!attr || !robust) return EINVAL;
    *robust = (attr->pshared & RIN_PTHREAD_MUTEXATTR_ROBUST_FLAG) != 0 ?
        PTHREAD_MUTEX_ROBUST : PTHREAD_MUTEX_STALLED;
    return 0;
}

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
static inline int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) {
    if (!mutex) return EINVAL;
    if (attr &&
        (attr->pshared & RIN_PTHREAD_MUTEXATTR_PSHARED_MASK) !=
            PTHREAD_PROCESS_PRIVATE)
        return ENOTSUP;
    if (attr &&
        ((attr->type & RIN_PTHREAD_MUTEXATTR_TYPE_MASK) >
             PTHREAD_MUTEX_ERRORCHECK ||
         (attr->type & ~(RIN_PTHREAD_MUTEXATTR_TYPE_MASK |
                         RIN_PTHREAD_MUTEXATTR_PRIO_INHERIT_FLAG)) != 0))
        return EINVAL;
    mutex->locked = 0;
    __rin_pthread_mutex_owner_store(mutex, 0);
    mutex->type = attr ? attr->type : PTHREAD_MUTEX_DEFAULT;
    if (attr &&
        (attr->pshared & RIN_PTHREAD_MUTEXATTR_ROBUST_FLAG) != 0)
        mutex->type |= RIN_PTHREAD_MUTEX_ROBUST_FLAG;
    mutex->recursion = 0;
#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    if ((mutex->type & RIN_PTHREAD_MUTEX_ROBUST_FLAG) != 0)
        mutex->kernel_mutex = platform_mutex_create_robust(
            attr &&
            (attr->type & RIN_PTHREAD_MUTEXATTR_PRIO_INHERIT_FLAG) != 0);
    else
        mutex->kernel_mutex =
            attr &&
                    (attr->type & RIN_PTHREAD_MUTEXATTR_PRIO_INHERIT_FLAG) != 0
                ? platform_mutex_create_pi() : platform_mutex_create();
    if (!mutex->kernel_mutex) return ENOMEM;
#endif
    return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;
    if (!mutex->kernel_mutex) return EINVAL;
    if (mutex->locked || platform_mutex_busy(mutex->kernel_mutex))
        return EBUSY;
#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    if (mutex->kernel_mutex) {
        platform_mutex_destroy(mutex->kernel_mutex);
        mutex->kernel_mutex = NULL;
    }
#endif
    return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;

    pthread_t self = pthread_self();
    if (self == 0) return ESRCH;
    int type = mutex->type & RIN_PTHREAD_MUTEXATTR_TYPE_MASK;

    /* 再帰ロックチェック */
    if (type == PTHREAD_MUTEX_RECURSIVE &&
        __rin_pthread_mutex_owner_load(mutex) == self) {
        if (mutex->recursion == INT_MAX) return EAGAIN;
        mutex->recursion++;
        return 0;
    }

    /* エラーチェックモードでデッドロック検出 */
    if (type == PTHREAD_MUTEX_ERRORCHECK &&
        __rin_pthread_mutex_owner_load(mutex) == self) {
        return EDEADLK;
    }

    if ((mutex->type & RIN_PTHREAD_MUTEX_ROBUST_FLAG) != 0) {
        int result;
        if (!mutex->kernel_mutex) return EINVAL;
        result = platform_mutex_lock_robust(mutex->kernel_mutex);
        if (result != 0 && result != EOWNERDEAD) return result;
        __atomic_store_n(&mutex->locked, 1, __ATOMIC_RELEASE);
        __rin_pthread_mutex_owner_store(mutex, self);
        mutex->recursion = 1;
        return result;
    }

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    if (!mutex->kernel_mutex) return EINVAL;
    platform_mutex_lock(mutex->kernel_mutex);
    __atomic_store_n(&mutex->locked, 1, __ATOMIC_RELEASE);
#endif

    __rin_pthread_mutex_owner_store(mutex, self);
    mutex->recursion = 1;
    return 0;
}

static inline int pthread_mutex_trylock(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;

    pthread_t self = pthread_self();
    if (self == 0) return ESRCH;
    int type = mutex->type & RIN_PTHREAD_MUTEXATTR_TYPE_MASK;

    if (type == PTHREAD_MUTEX_RECURSIVE &&
        __rin_pthread_mutex_owner_load(mutex) == self) {
        if (mutex->recursion == INT_MAX) return EAGAIN;
        mutex->recursion++;
        return 0;
    }

    if ((mutex->type & RIN_PTHREAD_MUTEX_ROBUST_FLAG) != 0) {
        int result;
        if (!mutex->kernel_mutex) return EINVAL;
        result = platform_mutex_trylock_robust(mutex->kernel_mutex);
        if (result != 0 && result != EOWNERDEAD) return result;
        __atomic_store_n(&mutex->locked, 1, __ATOMIC_RELEASE);
        __rin_pthread_mutex_owner_store(mutex, self);
        mutex->recursion = 1;
        return result;
    }

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    if (!mutex->kernel_mutex) return EINVAL;
    if (platform_mutex_trylock(mutex->kernel_mutex) != 0) return EBUSY;
    __atomic_store_n(&mutex->locked, 1, __ATOMIC_RELEASE);
#endif
    __rin_pthread_mutex_owner_store(mutex, self);
    mutex->recursion = 1;
    return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;

    pthread_t self = pthread_self();
    if (self == 0) return ESRCH;
    int type = mutex->type & RIN_PTHREAD_MUTEXATTR_TYPE_MASK;

    if (__rin_pthread_mutex_owner_load(mutex) != self) {
        return EPERM;
    }

    if (type == PTHREAD_MUTEX_RECURSIVE) {
        if (mutex->recursion <= 0) return EPERM;
        if (--mutex->recursion > 0) {
            return 0;
        }
    }

    if ((mutex->type & RIN_PTHREAD_MUTEX_ROBUST_FLAG) != 0) {
        int result;
        if (!mutex->kernel_mutex) return EINVAL;
        result = platform_mutex_unlock_robust(mutex->kernel_mutex);
        if (result != 0) return result;
        __rin_pthread_mutex_owner_store(mutex, 0);
        mutex->recursion = 0;
        __atomic_store_n(&mutex->locked, 0, __ATOMIC_RELEASE);
        return 0;
    }

    __rin_pthread_mutex_owner_store(mutex, 0);
    mutex->recursion = 0;

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    if (!mutex->kernel_mutex) return EINVAL;
    __atomic_store_n(&mutex->locked, 0, __ATOMIC_RELEASE);
    platform_mutex_unlock(mutex->kernel_mutex);
#endif

    return 0;
}

static inline int pthread_mutex_consistent(pthread_mutex_t* mutex) {
    pthread_t self;
    if (!mutex) return EINVAL;
    if ((mutex->type & RIN_PTHREAD_MUTEX_ROBUST_FLAG) == 0) return EINVAL;
    self = pthread_self();
    if (self == 0) return ESRCH;
    if (__rin_pthread_mutex_owner_load(mutex) != self) return EPERM;
    if (!mutex->kernel_mutex) return EINVAL;
    return platform_mutex_consistent(mutex->kernel_mutex);
}
#else
/* ユーザー空間: libc がABI実装を提供 */
extern int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr);
extern int pthread_mutex_destroy(pthread_mutex_t* mutex);
extern int pthread_mutex_lock(pthread_mutex_t* mutex);
extern int pthread_mutex_timedlock(pthread_mutex_t* mutex,
                                  const struct timespec* abstime);
extern int pthread_mutex_clocklock(pthread_mutex_t* mutex, clockid_t clock_id,
                                   const struct timespec* abstime);
extern int pthread_mutex_trylock(pthread_mutex_t* mutex);
extern int pthread_mutex_unlock(pthread_mutex_t* mutex);
extern int pthread_mutex_consistent(pthread_mutex_t* mutex);
extern int pthread_spin_init(pthread_spinlock_t* lock, int pshared);
extern int pthread_spin_destroy(pthread_spinlock_t* lock);
extern int pthread_spin_lock(pthread_spinlock_t* lock);
extern int pthread_spin_trylock(pthread_spinlock_t* lock);
extern int pthread_spin_unlock(pthread_spinlock_t* lock);
#endif

/* ═══════════════════════════════════════════════════════════════
 * 条件変数
 * ═══════════════════════════════════════════════════════════════*/

/* 条件変数属性 */
static inline int pthread_condattr_init(pthread_condattr_t* attr) {
    if (!attr) return EINVAL;
    attr->clock_id = CLOCK_REALTIME;
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

static inline int pthread_condattr_destroy(pthread_condattr_t* attr) {
    if (!attr) return EINVAL;
    return 0;
}

static inline int pthread_condattr_setclock(pthread_condattr_t* attr, int clock_id) {
    if (!attr) return EINVAL;
    if (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC) {
        return EINVAL;
    }
    attr->clock_id = clock_id;
    return 0;
}

static inline int pthread_condattr_getclock(const pthread_condattr_t* attr, int* clock_id) {
    if (!attr || !clock_id) return EINVAL;
    *clock_id = attr->clock_id;
    return 0;
}

static inline int pthread_condattr_setpshared(pthread_condattr_t* attr,
                                               int pshared) {
    if (!attr || (pshared != PTHREAD_PROCESS_PRIVATE &&
                  pshared != PTHREAD_PROCESS_SHARED))
        return EINVAL;
    attr->pshared = pshared;
    return 0;
}

static inline int pthread_condattr_getpshared(const pthread_condattr_t* attr,
                                               int* pshared) {
    if (!attr || !pshared) return EINVAL;
    *pshared = attr->pshared;
    return 0;
}

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
static inline int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr) {
    if (!cond) return EINVAL;
    if (attr && attr->pshared != PTHREAD_PROCESS_PRIVATE &&
        attr->pshared != PTHREAD_PROCESS_SHARED)
        return EINVAL;
    cond->generation = 0;
    cond->waiting = 0;
    cond->clock_id = (attr ? attr->clock_id : CLOCK_REALTIME);
    cond->pshared = (attr ? attr->pshared : PTHREAD_PROCESS_PRIVATE);
    return 0;
}

static inline int pthread_cond_destroy(pthread_cond_t* cond) {
    if (!cond) return EINVAL;
    if (cond->waiting > 0) return EBUSY;
    return 0;
}

static inline int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    if (!cond || !mutex) return EINVAL;

    /* Snapshot generation BEFORE releasing mutex.
     * If signal fires between snapshot and FUTEX_WAIT, the kernel's
     * value check (copyin32) will see generation != gen and return
     * -EAGAIN immediately -- no missed wakeup possible. */
    unsigned int gen = __atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE);
    __atomic_fetch_add(&cond->waiting, 1, __ATOMIC_SEQ_CST);
    int result = pthread_mutex_unlock(mutex);
    if (result != 0) {
        __atomic_fetch_sub(&cond->waiting, 1, __ATOMIC_SEQ_CST);
        return result;
    }

    /* Block in kernel until generation changes.
     * FUTEX_WAIT atomically checks *(&cond->generation) == gen.
     * Returns 0 on wake, -EAGAIN if value changed, -EINTR if interrupted.
     * Loop handles spurious wakeups. */
    while (__atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE) == gen) {
        long ret = syscall(
            SYS_futex, &cond->generation,
            FUTEX_WAIT | (cond->pshared == PTHREAD_PROCESS_SHARED ? 0 :
                          FUTEX_PRIVATE_FLAG),
            (int)gen, NULL, NULL, 0);
        if (ret < 0 && errno != EAGAIN && errno != EINTR) {
            result = errno != 0 ? errno : EIO;
            break;
        }
    }

    __atomic_fetch_sub(&cond->waiting, 1, __ATOMIC_SEQ_CST);
    int mutex_result = pthread_mutex_lock(mutex);
    return mutex_result != 0 ? mutex_result : result;
}

static inline int __rin_pthread_cond_timedwait_clock(
    pthread_cond_t* cond, pthread_mutex_t* mutex,
    const struct timespec* abstime, clockid_t clock_id) {
    if (!cond || !mutex || !abstime) return EINVAL;
    if (abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000L ||
        (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC))
        return EINVAL;

    unsigned int gen = __atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE);
    __atomic_fetch_add(&cond->waiting, 1, __ATOMIC_SEQ_CST);
    int result = pthread_mutex_unlock(mutex);
    if (result != 0) {
        __atomic_fetch_sub(&cond->waiting, 1, __ATOMIC_SEQ_CST);
        return result;
    }

    while (__atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE) == gen) {
        /* CLOCK_MONOTONIC values cannot be negative.  A normalized negative
         * absolute deadline is already expired; do not pass it to futex,
         * whose non-realtime timeout ABI reports EINVAL for this value. */
        if (clock_id == CLOCK_MONOTONIC && abstime->tv_sec < 0) {
            result = ETIMEDOUT;
            break;
        }
        int operation = FUTEX_WAIT_BITSET |
            (cond->pshared == PTHREAD_PROCESS_SHARED ? 0 :
             FUTEX_PRIVATE_FLAG) |
            (clock_id == CLOCK_REALTIME ? FUTEX_CLOCK_REALTIME : 0);
        long ret = syscall(SYS_futex, &cond->generation, operation, (int)gen,
                           abstime, NULL, FUTEX_BITSET_MATCH_ANY);
        if (ret < 0) {
            int err = errno;
            if (err == ETIMEDOUT) {
                if (__atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE) != gen)
                    result = 0;  /* Signal arrived just before timeout */
                else
                    result = ETIMEDOUT;
                break;
            }
            if (err == EAGAIN || err == EINTR) continue;
            result = err != 0 ? err : EIO;
            break;
        }
    }

    __atomic_fetch_sub(&cond->waiting, 1, __ATOMIC_SEQ_CST);
    int mutex_result = pthread_mutex_lock(mutex);
    return mutex_result != 0 ? mutex_result : result;
}

static inline int pthread_cond_timedwait(pthread_cond_t* cond,
                                          pthread_mutex_t* mutex,
                                          const struct timespec* abstime) {
    return __rin_pthread_cond_timedwait_clock(
        cond, mutex, abstime, cond ? cond->clock_id : CLOCK_REALTIME);
}

static inline int pthread_cond_clockwait(pthread_cond_t* cond,
                                          pthread_mutex_t* mutex,
                                          clockid_t clock_id,
                                          const struct timespec* abstime) {
    return __rin_pthread_cond_timedwait_clock(cond, mutex, abstime, clock_id);
}

static inline int pthread_cond_signal(pthread_cond_t* cond) {
    if (!cond) return EINVAL;
    __atomic_fetch_add(&cond->generation, 1, __ATOMIC_SEQ_CST);
    if (__atomic_load_n(&cond->waiting, __ATOMIC_SEQ_CST) > 0) {
        long ret = syscall(
            SYS_futex, &cond->generation,
            FUTEX_WAKE | (cond->pshared == PTHREAD_PROCESS_SHARED ? 0 :
                          FUTEX_PRIVATE_FLAG),
            1, NULL, NULL, 0);
        if (ret < 0) return errno != 0 ? errno : EIO;
    }
    return 0;
}

static inline int pthread_cond_broadcast(pthread_cond_t* cond) {
    if (!cond) return EINVAL;
    __atomic_fetch_add(&cond->generation, 1, __ATOMIC_SEQ_CST);
    if (__atomic_load_n(&cond->waiting, __ATOMIC_SEQ_CST) > 0) {
        long ret = syscall(
            SYS_futex, &cond->generation,
            FUTEX_WAKE | (cond->pshared == PTHREAD_PROCESS_SHARED ? 0 :
                          FUTEX_PRIVATE_FLAG),
            0x7fffffff, NULL, NULL, 0);
        if (ret < 0) return errno != 0 ? errno : EIO;
    }
    return 0;
}
#else
/* ユーザー空間: libc がABI実装を提供 */
extern int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);
extern int pthread_cond_destroy(pthread_cond_t* cond);
extern int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
extern int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex,
                                  const struct timespec* abstime);
extern int pthread_cond_clockwait(pthread_cond_t* cond, pthread_mutex_t* mutex,
                                 clockid_t clock_id,
                                 const struct timespec* abstime);
extern int pthread_cond_signal(pthread_cond_t* cond);
extern int pthread_cond_broadcast(pthread_cond_t* cond);
#endif

/* ═══════════════════════════════════════════════════════════════
 * 読み書きロック
 * ═══════════════════════════════════════════════════════════════*/

static inline int pthread_rwlockattr_init(pthread_rwlockattr_t* attr) {
    if (!attr) return EINVAL;
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

static inline int pthread_rwlockattr_destroy(pthread_rwlockattr_t* attr) {
    if (!attr) return EINVAL;
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

static inline int pthread_rwlockattr_setpshared(pthread_rwlockattr_t* attr,
                                                 int pshared) {
    if (!attr || (pshared != PTHREAD_PROCESS_PRIVATE &&
                  pshared != PTHREAD_PROCESS_SHARED))
        return EINVAL;
#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    if (pshared == PTHREAD_PROCESS_SHARED) return ENOTSUP;
#endif
    attr->pshared = pshared;
    return 0;
}

static inline int pthread_rwlockattr_getpshared(
    const pthread_rwlockattr_t* attr, int* pshared) {
    if (!attr || !pshared) return EINVAL;
    if (attr->pshared != PTHREAD_PROCESS_PRIVATE &&
        attr->pshared != PTHREAD_PROCESS_SHARED)
        return EINVAL;
    *pshared = attr->pshared;
    return 0;
}

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
static inline int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr) {
    int result;
    if (!rwlock) return EINVAL;
    if (attr && attr->pshared != PTHREAD_PROCESS_PRIVATE &&
        attr->pshared != PTHREAD_PROCESS_SHARED)
        return EINVAL;
    if (attr && attr->pshared == PTHREAD_PROCESS_SHARED) return ENOTSUP;
    rwlock->readers = 0;
    rwlock->writer = 0;
    rwlock->waiting_readers = 0;
    rwlock->waiting_writers = 0;
    result = pthread_mutex_init(&rwlock->mutex, NULL);
    if (result != 0) return result;
    result = pthread_cond_init(&rwlock->can_read, NULL);
    if (result != 0) {
        (void)pthread_mutex_destroy(&rwlock->mutex);
        return result;
    }
    result = pthread_cond_init(&rwlock->can_write, NULL);
    if (result != 0) {
        (void)pthread_cond_destroy(&rwlock->can_read);
        (void)pthread_mutex_destroy(&rwlock->mutex);
        return result;
    }
    return 0;
}

static inline int pthread_rwlock_destroy(pthread_rwlock_t* rwlock) {
    int result;
    if (!rwlock) return EINVAL;
    if (rwlock->readers > 0 || rin_pthread_rwlock_writer_active(rwlock) ||
        rwlock->waiting_readers > 0 || rwlock->waiting_writers > 0) return EBUSY;
    result = pthread_cond_destroy(&rwlock->can_read);
    if (result != 0) return result;
    result = pthread_cond_destroy(&rwlock->can_write);
    if (result != 0) {
        (void)pthread_cond_init(&rwlock->can_read, NULL);
        return result;
    }
    result = pthread_mutex_destroy(&rwlock->mutex);
    if (result != 0) {
        (void)pthread_cond_init(&rwlock->can_read, NULL);
        (void)pthread_cond_init(&rwlock->can_write, NULL);
        return result;
    }
    return 0;
}

static inline int rin_pthread_rwlock_notify_waiters(
    pthread_rwlock_t* rwlock) {
    if (rwlock->waiting_writers > 0) {
        if (!rin_pthread_rwlock_writer_active(rwlock) &&
            rwlock->readers == 0)
            return pthread_cond_signal(&rwlock->can_write);
    } else if (!rin_pthread_rwlock_writer_active(rwlock) &&
               rwlock->waiting_readers > 0) {
        return pthread_cond_broadcast(&rwlock->can_read);
    }
    return 0;
}

static inline void rin_pthread_rwlock_rollback_acquire(
    pthread_rwlock_t* rwlock, int writer) {
    if (pthread_mutex_lock(&rwlock->mutex) != 0) return;
    if (writer)
        rwlock->writer &= ~RIN_PTHREAD_RWLOCK_WRITER_ACTIVE;
    else if (rwlock->readers > 0)
        rwlock->readers--;
    (void)rin_pthread_rwlock_notify_waiters(rwlock);
    (void)pthread_mutex_unlock(&rwlock->mutex);
}

static inline int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock) {
    int result;
    pthread_t self;
    if (!rwlock) return EINVAL;
    result = pthread_mutex_lock(&rwlock->mutex);
    if (result != 0) return result;
    self = pthread_self();
    /* Writer-preference: block new readers when writers are waiting. */
    while (rin_pthread_rwlock_writer_active(rwlock) ||
           rwlock->waiting_writers > 0) {
        rwlock->waiting_readers++;
        result = pthread_cond_wait(&rwlock->can_read, &rwlock->mutex);
        if (result != 0 && rwlock->mutex.owner != self) return result;
        rwlock->waiting_readers--;
        if (result != 0) {
            int unlock_result = pthread_mutex_unlock(&rwlock->mutex);
            return unlock_result != 0 ? unlock_result : result;
        }
    }
    rwlock->readers++;
    result = pthread_mutex_unlock(&rwlock->mutex);
    if (result != 0) {
        rin_pthread_rwlock_rollback_acquire(rwlock, 0);
        return result;
    }
    return 0;
}

static inline int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock) {
    int result;
    pthread_t self;
    if (!rwlock) return EINVAL;
    result = pthread_mutex_lock(&rwlock->mutex);
    if (result != 0) return result;
    self = pthread_self();
    rwlock->waiting_writers++;
    while (rin_pthread_rwlock_writer_active(rwlock) || rwlock->readers > 0) {
        result = pthread_cond_wait(&rwlock->can_write, &rwlock->mutex);
        if (result != 0 && rwlock->mutex.owner != self) return result;
        if (result != 0) {
            rwlock->waiting_writers--;
            {
                int notify_result = rin_pthread_rwlock_notify_waiters(rwlock);
                int unlock_result = pthread_mutex_unlock(&rwlock->mutex);
                if (notify_result != 0) return notify_result;
                return unlock_result != 0 ? unlock_result : result;
            }
        }
    }
    rwlock->waiting_writers--;
    rwlock->writer |= RIN_PTHREAD_RWLOCK_WRITER_ACTIVE;
    result = pthread_mutex_unlock(&rwlock->mutex);
    if (result != 0) {
        rin_pthread_rwlock_rollback_acquire(rwlock, 1);
        return result;
    }
    return 0;
}

static inline int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock) {
    int result;
    if (!rwlock) return EINVAL;
    result = pthread_mutex_trylock(&rwlock->mutex);
    if (result != 0) return result;
    if (rin_pthread_rwlock_writer_active(rwlock) ||
        rwlock->waiting_writers > 0) {
        result = pthread_mutex_unlock(&rwlock->mutex);
        return result != 0 ? result : EBUSY;
    }
    rwlock->readers++;
    result = pthread_mutex_unlock(&rwlock->mutex);
    if (result != 0) {
        rin_pthread_rwlock_rollback_acquire(rwlock, 0);
        return result;
    }
    return 0;
}

static inline int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock) {
    int result;
    if (!rwlock) return EINVAL;
    result = pthread_mutex_trylock(&rwlock->mutex);
    if (result != 0) return result;
    if (rin_pthread_rwlock_writer_active(rwlock) || rwlock->readers > 0) {
        result = pthread_mutex_unlock(&rwlock->mutex);
        return result != 0 ? result : EBUSY;
    }
    rwlock->writer |= RIN_PTHREAD_RWLOCK_WRITER_ACTIVE;
    result = pthread_mutex_unlock(&rwlock->mutex);
    if (result != 0) {
        rin_pthread_rwlock_rollback_acquire(rwlock, 1);
        return result;
    }
    return 0;
}

static inline int pthread_rwlock_unlock(pthread_rwlock_t* rwlock) {
    int result;
    int unlock_result;
    if (!rwlock) return EINVAL;
    result = pthread_mutex_lock(&rwlock->mutex);
    if (result != 0) return result;
    if (rin_pthread_rwlock_writer_active(rwlock)) {
        rwlock->writer &= ~RIN_PTHREAD_RWLOCK_WRITER_ACTIVE;
    } else if (rwlock->readers > 0) {
        rwlock->readers--;
    } else {
        unlock_result = pthread_mutex_unlock(&rwlock->mutex);
        return unlock_result != 0 ? unlock_result : EPERM;
    }

    result = rin_pthread_rwlock_notify_waiters(rwlock);
    unlock_result = pthread_mutex_unlock(&rwlock->mutex);
    return result != 0 ? result : unlock_result;
}
#else
extern int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr);
extern int pthread_rwlock_destroy(pthread_rwlock_t* rwlock);
extern int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock);
extern int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock);
extern int pthread_rwlock_timedrdlock(pthread_rwlock_t* rwlock,
                                      const struct timespec* abstime);
extern int pthread_rwlock_timedwrlock(pthread_rwlock_t* rwlock,
                                      const struct timespec* abstime);
extern int pthread_rwlock_clockrdlock(pthread_rwlock_t* rwlock,
                                      clockid_t clock_id,
                                      const struct timespec* abstime);
extern int pthread_rwlock_clockwrlock(pthread_rwlock_t* rwlock,
                                      clockid_t clock_id,
                                      const struct timespec* abstime);
extern int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock);
extern int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock);
extern int pthread_rwlock_unlock(pthread_rwlock_t* rwlock);
#endif

/* ═══════════════════════════════════════════════════════════════
 * Barrier
 * ═══════════════════════════════════════════════════════════════*/

static inline int pthread_barrierattr_init(pthread_barrierattr_t* attr) {
    if (!attr) return EINVAL;
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

static inline int pthread_barrierattr_destroy(pthread_barrierattr_t* attr) {
    if (!attr) return EINVAL;
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

static inline int pthread_barrierattr_setpshared(pthread_barrierattr_t* attr,
                                                  int pshared) {
    if (!attr || (pshared != PTHREAD_PROCESS_PRIVATE &&
                  pshared != PTHREAD_PROCESS_SHARED))
        return EINVAL;
    attr->pshared = pshared;
    return 0;
}

static inline int pthread_barrierattr_getpshared(
    const pthread_barrierattr_t* attr, int* pshared) {
    if (!attr || !pshared ||
        (attr->pshared != PTHREAD_PROCESS_PRIVATE &&
         attr->pshared != PTHREAD_PROCESS_SHARED))
        return EINVAL;
    *pshared = attr->pshared;
    return 0;
}

extern int pthread_barrier_init(pthread_barrier_t* barrier,
                                const pthread_barrierattr_t* attr,
                                unsigned int count);
extern int pthread_barrier_destroy(pthread_barrier_t* barrier);
extern int pthread_barrier_wait(pthread_barrier_t* barrier);

/* ═══════════════════════════════════════════════════════════════
 * スレッドローカルストレージ (TLS)
 *
 * 実装は libs/libc/pthread_tls.c にある。
 * __thread ベースの per-thread キーバリューストア。
 * ═══════════════════════════════════════════════════════════════*/

#ifndef PTHREAD_KEYS_MAX
#define PTHREAD_KEYS_MAX              128
#endif
#ifndef PTHREAD_DESTRUCTOR_ITERATIONS
#define PTHREAD_DESTRUCTOR_ITERATIONS 4
#endif

extern int   pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
extern int   pthread_key_delete(pthread_key_t key);
extern void* pthread_getspecific(pthread_key_t key);
extern int   pthread_setspecific(pthread_key_t key, const void* value);
extern void  __pthread_run_destructors(void);

/* ═══════════════════════════════════════════════════════════════
 * Once実行
 * ═══════════════════════════════════════════════════════════════*/

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
static inline int pthread_once(pthread_once_t* once_control, void (*init_routine)(void)) {
    if (!once_control || !init_routine) return EINVAL;

    if (__sync_val_compare_and_swap(once_control, 0, 1) == 0) {
        /* We won the race - execute init_routine */
        init_routine();
        __atomic_store_n(once_control, 2, __ATOMIC_RELEASE);
        /* Wake all waiters */
        long ret = syscall(SYS_futex, once_control, FUTEX_WAKE_PRIVATE,
                           0x7fffffff, NULL, NULL, 0);
        if (ret < 0) return errno != 0 ? errno : EIO;
    } else {
        /* Another thread is running init_routine, block until done */
        while (__atomic_load_n(once_control, __ATOMIC_ACQUIRE) == 1) {
            long ret = syscall(SYS_futex, once_control, FUTEX_WAIT_PRIVATE,
                               1, NULL, NULL, 0);
            if (ret < 0 && errno != EAGAIN && errno != EINTR)
                return errno != 0 ? errno : EIO;
        }
    }
    return 0;
}
#else
extern int pthread_once(pthread_once_t* once_control, void (*init_routine)(void));
#endif

/* ═══════════════════════════════════════════════════════════════
 * シグナルマスク
 * ═══════════════════════════════════════════════════════════════*/

/* Forward declare sigset_t if not already defined */
#ifndef _SIGSET_T_DEFINED
#define _SIGSET_T_DEFINED
typedef uint64_t sigset_t;
#endif

/* SIG_BLOCK, SIG_UNBLOCK, SIG_SETMASK may be defined in signal.h */
#ifndef SIG_BLOCK
#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
#endif
#ifndef NSIG
#define NSIG 64
#endif

static inline int pthread_sigqueue(pthread_t thread, int sig,
                                   union sigval value) {
    siginfo_t information;
    unsigned char* bytes = (unsigned char*)&information;
    intptr_t result;
    intptr_t process_id;
    int saved_errno = errno;
    if (sig <= 0 || sig >= NSIG || sig == __RIN_SIGCANCEL ||
        sig == SIGKILL || sig == SIGSTOP)
        return EINVAL;
    process_id = _RIN_PTHREAD_SYSCALL0(SYS_GETPID);
    if (process_id <= 0) {
        int error = _rin_pthread_error_from_result(process_id);
        errno = saved_errno;
        return error;
    }
    for (size_t index = 0u; index < sizeof(information); ++index)
        bytes[index] = 0u;
    information.si_signo = sig;
    information.si_code = SI_QUEUE;
    information.si_value = value;
    result = _syscall4((uintptr_t)SYS_rt_tgsigqueueinfo,
                       (uintptr_t)process_id, (uintptr_t)thread,
                       (uintptr_t)sig, (uintptr_t)&information);
    {
        int error = _rin_pthread_error_from_result(result);
        errno = saved_errno;
        return error;
    }
}

/* Product-owned deferred cancellation.  The implementation keeps the
 * cancellation record in libc rather than inheriting a host pthread ABI. */
int pthread_atfork(void (*prepare)(void), void (*parent)(void),
                   void (*child)(void));
int pthread_setcancelstate(int state, int* oldstate);
int pthread_setcanceltype(int type, int* oldtype);
int pthread_cancel(pthread_t thread);
void pthread_testcancel(void);
/* Async-cancel delivery on __RIN_SIGCANCEL.  Installed once per process
 * when the first thread enables PTHREAD_CANCEL_ASYNCHRONOUS; acts only
 * for an async, enabled, pending, non-deferred owner. */
void __rin_pthread_async_cancel_handler(int sig);

typedef struct __rin_pthread_cleanup_buffer {
    struct __rin_pthread_cleanup_buffer* next;
    void (*routine)(void*);
    void* argument;
    int active;
} __rin_pthread_cleanup_buffer;

void __rin_pthread_cleanup_push_node(__rin_pthread_cleanup_buffer* buffer,
                                     void (*routine)(void*), void* argument);
void __rin_pthread_cleanup_pop_node(int execute);
void rin_pthread_cancel_run_cleanups(pthread_t thread);
int rin_pthread_cancel_pending_enabled(pthread_t thread);
int rin_pthread_cancel_register_created(pthread_t thread);
int __rin_pthread_cancel_wait_register(volatile unsigned int* word,
                                       int wake_operation);
int __rin_pthread_cancel_wait_register_noncancel(
    volatile unsigned int* word, int wake_operation);
int __rin_pthread_cancel_wait_unregister(volatile unsigned int* word);

#define __RIN_PTHREAD_CLEANUP_CAT_INNER(left, right) left##right
#define __RIN_PTHREAD_CLEANUP_CAT(left, right) \
    __RIN_PTHREAD_CLEANUP_CAT_INNER(left, right)
#define __RIN_PTHREAD_CLEANUP_PUSH(routine, argument, suffix) \
    { \
        __rin_pthread_cleanup_buffer \
            __RIN_PTHREAD_CLEANUP_CAT(__rin_cleanup_buffer_, suffix); \
        __rin_pthread_cleanup_push_node( \
            &__RIN_PTHREAD_CLEANUP_CAT(__rin_cleanup_buffer_, suffix), \
            (routine), (argument));
#if defined(__COUNTER__)
#define pthread_cleanup_push(routine, argument) \
    __RIN_PTHREAD_CLEANUP_PUSH(routine, argument, __COUNTER__)
#else
#define pthread_cleanup_push(routine, argument) \
    __RIN_PTHREAD_CLEANUP_PUSH(routine, argument, __LINE__)
#endif
#define pthread_cleanup_pop(execute) \
        __rin_pthread_cleanup_pop_node((execute)); \
    }

#if defined(RIN_USERSPACE) || defined(RIN_PTHREAD_RUNTIME_IMPLEMENTATION)
int rin_pthread_cancel_register(pthread_t thread);
void rin_pthread_cancel_set_detached(pthread_t thread);
void rin_pthread_cancel_exit(pthread_t thread, void* value);
void rin_pthread_cancel_take_exit(pthread_t thread, void** value);
#endif

/* ═══════════════════════════════════════════════════════════════
 * スケジューリング
 * ═══════════════════════════════════════════════════════════════*/

static inline int pthread_getschedparam(pthread_t thread, int* policy, struct sched_param* param) {
#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
    RinThreadSchedParametersV1 parameters = {0};
    intptr_t result;
    if (!policy || !param) return EINVAL;
    if (thread == 0) return ESRCH;
    result = _RIN_PTHREAD_SCHED_SYSCALL2(
        (uintptr_t)SYS_THREAD_SCHED_GET, (uintptr_t)thread,
        (uintptr_t)&parameters);
    if (result < 0)
        return _rin_pthread_error_from_result(result);
    if (result != 0 || parameters.struct_size != sizeof(parameters) ||
        parameters.version != RIN_THREAD_SCHED_ABI_VERSION ||
        parameters.reserved0 != 0u || parameters.reserved1 != 0u)
        return EIO;
    *policy = parameters.policy;
    param->sched_priority = parameters.priority;
    return 0;
#elif defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    if (!policy || !param) return EINVAL;
    return platform_thread_sched_get((void*)(uintptr_t)thread,
                                     policy, &param->sched_priority);
#else
    (void)thread;
    (void)policy;
    (void)param;
    return ENOSYS;
#endif
}

static inline int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param* param) {
#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
    RinThreadSchedParametersV1 parameters;
    intptr_t result;
    if (!param) return EINVAL;
    if (thread == 0) return ESRCH;
    parameters.struct_size = (uint32_t)sizeof(parameters);
    parameters.version = RIN_THREAD_SCHED_ABI_VERSION;
    parameters.reserved0 = 0u;
    parameters.policy = policy;
    parameters.priority = param->sched_priority;
    parameters.reserved1 = 0u;
    result = _RIN_PTHREAD_SCHED_SYSCALL2(
        (uintptr_t)SYS_THREAD_SCHED_SET, (uintptr_t)thread,
        (uintptr_t)&parameters);
    if (result < 0) return _rin_pthread_error_from_result(result);
    return result == 0 ? 0 : EIO;
#elif defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    if (!param) return EINVAL;
    return platform_thread_sched_set((void*)(uintptr_t)thread, policy,
                                     param->sched_priority);
#else
    (void)thread;
    (void)policy;
    (void)param;
    return ENOSYS;
#endif
}

static inline int pthread_setschedprio(pthread_t thread, int prio) {
#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
    int policy = SCHED_OTHER;
    struct sched_param param;
    int result = pthread_getschedparam(thread, &policy, &param);
    if (result != 0) return result;
    param.sched_priority = prio;
    return pthread_setschedparam(thread, policy, &param);
#elif defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    int policy = SCHED_OTHER;
    int ignored_priority = 0;
    int result = platform_thread_sched_get((void*)(uintptr_t)thread,
                                            &policy, &ignored_priority);
    if (result != 0) return result;
    return platform_thread_sched_set((void*)(uintptr_t)thread, policy, prio);
#else
    (void)thread;
    (void)prio;
    return ENOSYS;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* _PTHREAD_H */

#endif /* hosted POSIX pthread delegation */
