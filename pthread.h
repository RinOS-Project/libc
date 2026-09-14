/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - pthread.h
 * POSIX Threads API
 */

#ifndef _PTHREAD_H
#define _PTHREAD_H

/* Let C++ wrappers distinguish this ABI from a hosted provider even when
 * both headers use the conventional `_PTHREAD_H` include guard. */
#define RIN_LIBC_PTHREAD_HEADER 1

#include "stddef.h"
#include "stdint.h"
#include "errno.h"
#include "time.h"
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

/* ═══════════════════════════════════════════════════════════════
 * 型定義
 * ═══════════════════════════════════════════════════════════════*/

/* A thread handle may carry a kernel pointer in freestanding builds.  Keep
 * it target-word sized: `unsigned long` is only 32 bits on LLP64 hosts. */
typedef uintptr_t pthread_t;
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

typedef struct {
    int type;
    int pshared;
} pthread_mutexattr_t;

typedef struct {
    volatile unsigned int generation;   /* 世代カウンタ（wakeup lost防止） */
    volatile int waiting;
    int clock_id;
} pthread_cond_t;

typedef struct {
    int clock_id;
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
    int dummy;
} pthread_rwlockattr_t;

/* ═══════════════════════════════════════════════════════════════
 * 定数
 * ═══════════════════════════════════════════════════════════════*/

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

#define PTHREAD_MUTEX_NORMAL     0
#define PTHREAD_MUTEX_RECURSIVE  1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT    PTHREAD_MUTEX_NORMAL

#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED  1

#define PTHREAD_ONCE_INIT 0

/* RinOS implements deferred POSIX cancellation.  Asynchronous cancellation
 * would require an unwind-safe activation ABI that is not part of the
 * product contract; callers requesting it receive ENOTSUP. */
#define PTHREAD_CANCEL_ENABLE        0
#define PTHREAD_CANCEL_DISABLE       1
#define PTHREAD_CANCEL_DEFERRED      0
#define PTHREAD_CANCEL_ASYNCHRONOUS  1
#define PTHREAD_CANCELED             ((void*)(intptr_t)-1)

#define PTHREAD_MUTEX_INITIALIZER { 0, 0, 0, 0 }
#define PTHREAD_COND_INITIALIZER  { 0, 0, CLOCK_REALTIME }
#define PTHREAD_RWLOCK_INITIALIZER { 0, 0, 0, 0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER }

/* ═══════════════════════════════════════════════════════════════
 * カーネル空間用API宣言 (カーネルビルドのみ)
 * ═══════════════════════════════════════════════════════════════*/

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
extern void  platform_thread_init(void);
extern void* platform_thread_create(void (*entry)(void*), void* arg);
extern void  platform_thread_join(void* thread);
extern int   platform_thread_detach(void* thread);
extern int   platform_thread_sched_get(void* thread, int* policy, int* priority);
extern int   platform_thread_sched_set(void* thread, int policy, int priority);
extern int   platform_thread_name_set(void* thread, const char* name);
extern int   platform_thread_name_get(void* thread, char* name, size_t length);
extern void  platform_thread_exit(void);
extern void  platform_thread_yield(void);
extern uint32_t platform_thread_current_tid(void);

extern void* platform_mutex_create(void);
extern void  platform_mutex_lock(void* mutex);
extern int   platform_mutex_trylock(void* mutex);
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

static inline int pthread_attr_init(pthread_attr_t* attr) {
    if (!attr) return EINVAL;
    attr->detachstate = PTHREAD_CREATE_JOINABLE;
    attr->scope = PTHREAD_SCOPE_SYSTEM;
    attr->stacksize = 8 * 1024 * 1024;  /* 8 MiB default worker stack */
    attr->stackaddr = NULL;
    return 0;
}

static inline int pthread_attr_destroy(pthread_attr_t* attr) {
    if (!attr) return EINVAL;
    attr->detachstate = PTHREAD_CREATE_JOINABLE;
    attr->scope = PTHREAD_SCOPE_SYSTEM;
    attr->stacksize = 0;
    attr->stackaddr = NULL;
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
    if (!attr || stacksize < 4096) return EINVAL;
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
    attr->stackaddr = stackaddr;
    attr->stacksize = stacksize;
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

    attr->detachstate =
        (info.flags & RIN_THREAD_STACK_INFO_FLAG_DETACHED) != 0u
            ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE;
    attr->scope = PTHREAD_SCOPE_SYSTEM;
    attr->stackaddr = (void*)(uintptr_t)info.stack_base;
    attr->stacksize = (size_t)info.stack_size;
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
    (void)attr;
    if (!thread || !start_routine) return EINVAL;
    void* kthread = platform_thread_create((void (*)(void*))start_routine, arg);
    if (!kthread) return EAGAIN;
    *thread = (pthread_t)(uintptr_t)kthread;
    return 0;
}

static inline int pthread_join(pthread_t thread, void** retval) {
    if (retval) *retval = NULL;
    platform_thread_join((void*)(uintptr_t)thread);
    return 0;
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
        _RIN_PTHREAD_SELF_SYSCALL0(SYS_GETPID));
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
    if (len < RIN_THREAD_NAME_CAPACITY) return ERANGE;
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
    for (size_t index = 0u; index < RIN_THREAD_NAME_CAPACITY; ++index)
        name[index] = request.name[index];
    return 0;
#endif
}
#endif

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
static inline void pthread_exit(void* retval) {
    (void)retval;
    platform_thread_exit();
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

static inline int pthread_mutexattr_destroy(pthread_mutexattr_t* attr) {
    if (!attr) return EINVAL;
    attr->type = PTHREAD_MUTEX_DEFAULT;
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

static inline int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type) {
    if (!attr) return EINVAL;
    if (type < 0 || type > PTHREAD_MUTEX_ERRORCHECK) return EINVAL;
    attr->type = type;
    return 0;
}

static inline int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type) {
    if (!attr || !type) return EINVAL;
    *type = attr->type;
    return 0;
}

static inline int pthread_mutexattr_setpshared(pthread_mutexattr_t* attr, int pshared) {
    if (!attr) return EINVAL;
    if (pshared != PTHREAD_PROCESS_PRIVATE && pshared != PTHREAD_PROCESS_SHARED) return EINVAL;
    attr->pshared = pshared;
    return 0;
}

static inline int pthread_mutexattr_getpshared(const pthread_mutexattr_t* attr, int* pshared) {
    if (!attr || !pshared) return EINVAL;
    *pshared = attr->pshared;
    return 0;
}

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
static inline int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) {
    if (!mutex) return EINVAL;
    mutex->locked = 0;
    mutex->owner = 0;
    mutex->type = attr ? attr->type : PTHREAD_MUTEX_DEFAULT;
    mutex->recursion = 0;
#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    mutex->kernel_mutex = platform_mutex_create();
#endif
    return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;
    if (mutex->locked) return EBUSY;
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

    /* 再帰ロックチェック */
    if (mutex->type == PTHREAD_MUTEX_RECURSIVE && mutex->owner == self) {
        mutex->recursion++;
        return 0;
    }

    /* エラーチェックモードでデッドロック検出 */
    if (mutex->type == PTHREAD_MUTEX_ERRORCHECK && mutex->owner == self) {
        return EDEADLK;
    }

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    if (mutex->kernel_mutex) {
        platform_mutex_lock(mutex->kernel_mutex);
    }
#endif

    mutex->owner = self;
    mutex->recursion = 1;
    return 0;
}

static inline int pthread_mutex_trylock(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;

    pthread_t self = pthread_self();

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE && mutex->owner == self) {
        mutex->recursion++;
        return 0;
    }

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    if (mutex->kernel_mutex) {
        if (platform_mutex_trylock(mutex->kernel_mutex) != 0) {
            return EBUSY;
        }
    }
#endif
    mutex->owner = self;
    mutex->recursion = 1;
    return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;

    pthread_t self = pthread_self();

    if (mutex->type == PTHREAD_MUTEX_ERRORCHECK && mutex->owner != self) {
        return EPERM;
    }

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE) {
        if (--mutex->recursion > 0) {
            return 0;
        }
    }

    mutex->owner = 0;
    mutex->recursion = 0;

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    mutex->locked = 0;
    if (mutex->kernel_mutex) {
        platform_mutex_unlock(mutex->kernel_mutex);
    }
#endif

    return 0;
}
#else
/* ユーザー空間: libc がABI実装を提供 */
extern int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr);
extern int pthread_mutex_destroy(pthread_mutex_t* mutex);
extern int pthread_mutex_lock(pthread_mutex_t* mutex);
extern int pthread_mutex_trylock(pthread_mutex_t* mutex);
extern int pthread_mutex_unlock(pthread_mutex_t* mutex);
#endif

/* ═══════════════════════════════════════════════════════════════
 * 条件変数
 * ═══════════════════════════════════════════════════════════════*/

/* 条件変数属性 */
static inline int pthread_condattr_init(pthread_condattr_t* attr) {
    if (!attr) return EINVAL;
    attr->clock_id = CLOCK_REALTIME;
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

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
static inline int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr) {
    if (!cond) return EINVAL;
    cond->generation = 0;
    cond->waiting = 0;
    cond->clock_id = (attr ? attr->clock_id : CLOCK_REALTIME);
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
    pthread_mutex_unlock(mutex);

    /* Block in kernel until generation changes.
     * FUTEX_WAIT atomically checks *(&cond->generation) == gen.
     * Returns 0 on wake, -EAGAIN if value changed, -EINTR if interrupted.
     * Loop handles spurious wakeups. */
    while (__atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE) == gen) {
        syscall(SYS_futex, &cond->generation, FUTEX_WAIT,
                (int)gen, NULL, NULL, 0);
    }

    __atomic_fetch_sub(&cond->waiting, 1, __ATOMIC_SEQ_CST);
    pthread_mutex_lock(mutex);
    return 0;
}

static inline int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex,
                                          const struct timespec* abstime) {
    if (!cond || !mutex || !abstime) return EINVAL;

    unsigned int gen = __atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE);
    __atomic_fetch_add(&cond->waiting, 1, __ATOMIC_SEQ_CST);
    pthread_mutex_unlock(mutex);

    int result = 0;
    while (__atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE) == gen) {
        /* Compute relative timeout from absolute deadline */
        struct timespec now;
        clock_gettime(cond->clock_id, &now);

        long long timeout_ns = (long long)(abstime->tv_sec - now.tv_sec) * 1000000000LL
                              + (long long)(abstime->tv_nsec - now.tv_nsec);
        if (timeout_ns <= 0) {
            result = ETIMEDOUT;
            break;
        }

        struct timespec ts;
        ts.tv_sec = (time_t)(timeout_ns / 1000000000LL);
        ts.tv_nsec = (long)(timeout_ns % 1000000000LL);

        long ret = syscall(SYS_futex, &cond->generation, FUTEX_WAIT,
                           (int)gen, &ts, NULL, 0);
        if (ret < 0) {
            int err = errno;
            if (err == ETIMEDOUT) {
                if (__atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE) != gen)
                    result = 0;  /* Signal arrived just before timeout */
                else
                    result = ETIMEDOUT;
                break;
            }
            /* EAGAIN/EINTR: re-check generation in loop */
        }
    }

    __atomic_fetch_sub(&cond->waiting, 1, __ATOMIC_SEQ_CST);
    pthread_mutex_lock(mutex);
    return result;
}

static inline int pthread_cond_signal(pthread_cond_t* cond) {
    if (!cond) return EINVAL;
    __atomic_fetch_add(&cond->generation, 1, __ATOMIC_SEQ_CST);
    if (__atomic_load_n(&cond->waiting, __ATOMIC_SEQ_CST) > 0)
        syscall(SYS_futex, &cond->generation, FUTEX_WAKE, 1, NULL, NULL, 0);
    return 0;
}

static inline int pthread_cond_broadcast(pthread_cond_t* cond) {
    if (!cond) return EINVAL;
    __atomic_fetch_add(&cond->generation, 1, __ATOMIC_SEQ_CST);
    if (__atomic_load_n(&cond->waiting, __ATOMIC_SEQ_CST) > 0)
        syscall(SYS_futex, &cond->generation, FUTEX_WAKE,
                0x7fffffff, NULL, NULL, 0);
    return 0;
}
#else
/* ユーザー空間: libc がABI実装を提供 */
extern int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);
extern int pthread_cond_destroy(pthread_cond_t* cond);
extern int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
extern int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex,
                                  const struct timespec* abstime);
extern int pthread_cond_signal(pthread_cond_t* cond);
extern int pthread_cond_broadcast(pthread_cond_t* cond);
#endif

/* ═══════════════════════════════════════════════════════════════
 * 読み書きロック
 * ═══════════════════════════════════════════════════════════════*/

#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
static inline int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr) {
    (void)attr;
    if (!rwlock) return EINVAL;
    rwlock->readers = 0;
    rwlock->writer = 0;
    rwlock->waiting_readers = 0;
    rwlock->waiting_writers = 0;
    pthread_mutex_init(&rwlock->mutex, NULL);
    pthread_cond_init(&rwlock->can_read, NULL);
    pthread_cond_init(&rwlock->can_write, NULL);
    return 0;
}

static inline int pthread_rwlock_destroy(pthread_rwlock_t* rwlock) {
    if (!rwlock) return EINVAL;
    if (rwlock->readers > 0 || rwlock->writer ||
        rwlock->waiting_readers > 0 || rwlock->waiting_writers > 0) return EBUSY;
    pthread_cond_destroy(&rwlock->can_read);
    pthread_cond_destroy(&rwlock->can_write);
    pthread_mutex_destroy(&rwlock->mutex);
    return 0;
}

static inline int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock) {
    if (!rwlock) return EINVAL;
    pthread_mutex_lock(&rwlock->mutex);
    /* Writer-preference: block new readers when writers are waiting. */
    while (rwlock->writer || rwlock->waiting_writers > 0) {
        rwlock->waiting_readers++;
        pthread_cond_wait(&rwlock->can_read, &rwlock->mutex);
        rwlock->waiting_readers--;
    }
    rwlock->readers++;
    pthread_mutex_unlock(&rwlock->mutex);
    return 0;
}

static inline int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock) {
    if (!rwlock) return EINVAL;
    pthread_mutex_lock(&rwlock->mutex);
    rwlock->waiting_writers++;
    while (rwlock->writer || rwlock->readers > 0) {
        pthread_cond_wait(&rwlock->can_write, &rwlock->mutex);
    }
    rwlock->waiting_writers--;
    rwlock->writer = 1;
    pthread_mutex_unlock(&rwlock->mutex);
    return 0;
}

static inline int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock) {
    if (!rwlock) return EINVAL;
    if (pthread_mutex_trylock(&rwlock->mutex) != 0) return EBUSY;
    if (rwlock->writer || rwlock->waiting_writers > 0) {
        pthread_mutex_unlock(&rwlock->mutex);
        return EBUSY;
    }
    rwlock->readers++;
    pthread_mutex_unlock(&rwlock->mutex);
    return 0;
}

static inline int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock) {
    if (!rwlock) return EINVAL;
    if (pthread_mutex_trylock(&rwlock->mutex) != 0) return EBUSY;
    if (rwlock->writer || rwlock->readers > 0) {
        pthread_mutex_unlock(&rwlock->mutex);
        return EBUSY;
    }
    rwlock->writer = 1;
    pthread_mutex_unlock(&rwlock->mutex);
    return 0;
}

static inline int pthread_rwlock_unlock(pthread_rwlock_t* rwlock) {
    if (!rwlock) return EINVAL;
    pthread_mutex_lock(&rwlock->mutex);
    if (rwlock->writer) {
        rwlock->writer = 0;
    } else if (rwlock->readers > 0) {
        rwlock->readers--;
    } else {
        pthread_mutex_unlock(&rwlock->mutex);
        return EPERM;
    }

    if (rwlock->waiting_writers > 0) {
        if (rwlock->writer == 0 && rwlock->readers == 0) {
            pthread_cond_signal(&rwlock->can_write);
        }
    } else if (rwlock->waiting_readers > 0) {
        pthread_cond_broadcast(&rwlock->can_read);
    }

    pthread_mutex_unlock(&rwlock->mutex);
    return 0;
}
#else
extern int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr);
extern int pthread_rwlock_destroy(pthread_rwlock_t* rwlock);
extern int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock);
extern int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock);
extern int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock);
extern int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock);
extern int pthread_rwlock_unlock(pthread_rwlock_t* rwlock);
#endif

/* ═══════════════════════════════════════════════════════════════
 * スレッドローカルストレージ (TLS)
 *
 * 実装は libs/libc/pthread_tls.c にある。
 * __thread ベースの per-thread キーバリューストア。
 * ═══════════════════════════════════════════════════════════════*/

#define PTHREAD_KEYS_MAX              128
#define PTHREAD_DESTRUCTOR_ITERATIONS 4

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
        syscall(SYS_futex, once_control, FUTEX_WAKE, 0x7fffffff, NULL, NULL, 0);
    } else {
        /* Another thread is running init_routine, block until done */
        while (__atomic_load_n(once_control, __ATOMIC_ACQUIRE) == 1) {
            syscall(SYS_futex, once_control, FUTEX_WAIT, 1, NULL, NULL, 0);
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
typedef unsigned long sigset_t;
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

static inline int pthread_sigmask(int how, const sigset_t* set, sigset_t* oldset) {
    sigset_t snapshot = 0;
    const sigset_t* set_argument = set;
    intptr_t result;
    int saved_errno = errno;
    int error = 0;
    if (set && how != SIG_BLOCK && how != SIG_UNBLOCK && how != SIG_SETMASK)
        return EINVAL;
    if (set) {
        snapshot = *set;
        set_argument = &snapshot;
    }
    result = _RIN_PTHREAD_SIGMASK_SYSCALL4(
        SYS_rt_sigprocmask, how, set_argument, oldset, sizeof(sigset_t));
    error = _rin_pthread_error_from_result(result);
    errno = saved_errno;
    return error;
}

/* pthread_kill - send signal to a thread */
static inline int pthread_kill(pthread_t thread, int sig) {
    /* SYS_TKILL(500): thread = kernel scheduler TID; the kernel enforces
     * process-namespace visibility and generation checks. */
    intptr_t result;
    if (sig < 0 || sig >= NSIG) return EINVAL;
    result = _syscall2((uintptr_t)SYS_TKILL, (uintptr_t)thread,
                       (uintptr_t)sig);
    return _rin_pthread_error_from_result(result);
}

/* Product-owned deferred cancellation.  The implementation keeps the
 * cancellation record in libc rather than inheriting a host pthread ABI. */
int pthread_setcancelstate(int state, int* oldstate);
int pthread_setcanceltype(int type, int* oldtype);
int pthread_cancel(pthread_t thread);
void pthread_testcancel(void);

#if defined(RIN_USERSPACE) || defined(RIN_PTHREAD_RUNTIME_IMPLEMENTATION)
int rin_pthread_cancel_register(pthread_t thread);
void rin_pthread_cancel_set_detached(pthread_t thread);
void rin_pthread_cancel_exit(pthread_t thread, void* value);
void rin_pthread_cancel_take_exit(pthread_t thread, void** value);
#endif

/* ═══════════════════════════════════════════════════════════════
 * スケジューリング
 * ═══════════════════════════════════════════════════════════════*/

#include "sched.h"

static inline int pthread_getschedparam(pthread_t thread, int* policy, struct sched_param* param) {
#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
    RinThreadSchedParametersV1 parameters = {0};
    intptr_t result;
    if (!policy || !param) return EINVAL;
    if (thread == 0) return ESRCH;
    result = _syscall2((uintptr_t)SYS_THREAD_SCHED_GET,
                       (uintptr_t)thread, (uintptr_t)&parameters);
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
    result = _syscall2((uintptr_t)SYS_THREAD_SCHED_SET,
                       (uintptr_t)thread, (uintptr_t)&parameters);
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
