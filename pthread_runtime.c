/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - pthread runtime implementation
 * libcがpthread ABIの所有者となる実装。
 */

#define RIN_PTHREAD_RUNTIME_IMPLEMENTATION 1
#include "pthread.h"
#include "internal/pthread_cond_interrupt.h"
#include "rin_pthread_futex_policy.h"
#include "rin_thread_detach_policy.h"
#include "rin_thread_stack_policy.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"

#define CLONE_THREAD    0x00010000UL
#define CLONE_VM        0x00000100UL
#define CLONE_FS        0x00000200UL
#define CLONE_FILES     0x00000400UL
#define CLONE_SIGHAND   0x00000800UL

#define THREAD_STACK_DEFAULT (8UL * 1024UL * 1024UL)
#define THREAD_STACK_GUARD   0x1000UL
#define THREAD_PAGE_SIZE     0x1000UL

/* Keep pthread_mutex_t's public size stable.  The otherwise small `type`
 * field carries the process-shared bit alongside the POSIX mutex kind. */
#define RIN_PTHREAD_MUTEX_PSHARED_FLAG 0x100
#define RIN_PTHREAD_MUTEX_ROBUST_FLAG  0x200
#define RIN_PTHREAD_MUTEX_NOT_RECOVERABLE_FLAG 0x400
#define RIN_PTHREAD_MUTEX_PRIO_INHERIT_FLAG 0x800
#define RIN_PTHREAD_MUTEX_TYPE_MASK    0xff
#define RIN_PTHREAD_ROBUST_OWNER_MASK UINT32_C(0x3fffffff)
#define RIN_PTHREAD_ROBUST_OWNER_DIED UINT32_C(0x40000000)
#define RIN_PTHREAD_ROBUST_WAITERS UINT32_C(0x80000000)

/* The Windows host-test toolchain emulates __thread through a
 * pthread_once call, which would re-enter RinOS's own pthread_once in
 * TUs that link this runtime (and pollute futex-call accounting with its
 * one-time init wake).  A plain cell is safe on that host-only path:
 * host-test waits are single-threaded and scripted, so no second thread
 * can bump the epoch mid-wait.  Production (ELF) toolchains use real
 * TLS. */
#if defined(_WIN32)
static uint64_t g_rin_pthread_cond_signal_interrupt_epoch;
#else
static __thread uint64_t g_rin_pthread_cond_signal_interrupt_epoch;
#endif

#ifndef _RIN_PTHREAD_RUNTIME_FUTEX
#define _RIN_PTHREAD_RUNTIME_FUTEX(address, operation, value, timeout) \
    syscall(SYS_futex, address, operation, value, timeout, NULL, 0)
#endif

#ifndef _RIN_PTHREAD_RUNTIME_FUTEX_BITSET
#define _RIN_PTHREAD_RUNTIME_FUTEX_BITSET(address, operation, value, timeout, \
                                         bitset) \
    syscall(SYS_futex, address, operation, value, timeout, NULL, bitset)
#endif

static int rin_pthread_mutexattr_is_shared(const pthread_mutexattr_t* attr) {
    return attr &&
        (attr->pshared & RIN_PTHREAD_MUTEXATTR_PSHARED_MASK) ==
            PTHREAD_PROCESS_SHARED;
}

static int rin_pthread_mutexattr_is_robust(const pthread_mutexattr_t* attr) {
    return attr &&
        (attr->pshared & RIN_PTHREAD_MUTEXATTR_ROBUST_FLAG) != 0;
}

static int rin_pthread_mutex_is_shared(const pthread_mutex_t* mutex) {
    return mutex && (mutex->type & RIN_PTHREAD_MUTEX_PSHARED_FLAG) != 0;
}

static int rin_pthread_mutex_type(const pthread_mutex_t* mutex) {
    return mutex ? (mutex->type & RIN_PTHREAD_MUTEX_TYPE_MASK) :
                   PTHREAD_MUTEX_DEFAULT;
}

static int rin_pthread_mutex_is_robust(const pthread_mutex_t* mutex) {
    return mutex && (mutex->type & RIN_PTHREAD_MUTEX_ROBUST_FLAG) != 0;
}

static int rin_pthread_mutex_is_prio_inherit(const pthread_mutex_t* mutex) {
    return mutex && (mutex->type & RIN_PTHREAD_MUTEX_PRIO_INHERIT_FLAG) != 0;
}

static pthread_t rin_pthread_mutex_owner_token(const pthread_mutex_t* mutex,
                                               pthread_t self) {
    uintptr_t pid;
    if (!rin_pthread_mutex_is_shared(mutex)) return self;
    pid = (uintptr_t)(uint32_t)getpid();
    if (pid == 0u) return self;
#if UINTPTR_MAX > UINT32_MAX
    return (pthread_t)((pid << 32) | ((uintptr_t)self & UINT32_MAX));
#else
    /* The compat ABI has a 32-bit owner word.  Mix both identities so equal
     * TIDs in different processes do not look recursively owned. */
    return (pthread_t)((uint32_t)self ^
                       ((uint32_t)pid * UINT32_C(0x9e3779b9)));
#endif
}

static int rin_pthread_mutex_futex_operation(const pthread_mutex_t* mutex,
                                              int operation) {
    return operation | (rin_pthread_mutex_is_shared(mutex) ? 0 :
                        FUTEX_PRIVATE_FLAG);
}

static int rin_pthread_cond_futex_operation(const pthread_cond_t* cond,
                                             int operation) {
    return operation |
        (cond && cond->pshared == PTHREAD_PROCESS_SHARED ? 0 :
         FUTEX_PRIVATE_FLAG);
}

static int rin_pthread_mutex_robust_owner_id(pthread_mutex_t* mutex,
                                             pthread_t owner,
                                             uint32_t* owner_id_out) {
    long result;
    if (!mutex || !owner_id_out) return EINVAL;
    result = syscall(SYS_futex, &mutex->locked,
                     FUTEX_RIN_ROBUST_REGISTER |
                         (rin_pthread_mutex_is_prio_inherit(mutex)
                              ? FUTEX_RIN_PI_FLAG : 0) |
                         (rin_pthread_mutex_is_prio_inherit(mutex) &&
                          rin_pthread_mutex_is_robust(mutex)
                              ? FUTEX_RIN_PI_ROBUST_FLAG : 0) |
                         (rin_pthread_mutex_is_shared(mutex) ? 0 :
                          FUTEX_PRIVATE_FLAG),
                     (uintptr_t)&mutex->owner, (void*)(uintptr_t)owner,
                     NULL, 0);
    if (result < 0) {
        if (result == -1 && errno > 0 && errno <= 4095) return errno;
        if (result < -1 && -result <= 4095) return (int)-result;
        return EIO;
    }
    if (result == 0 || (uint64_t)result > RIN_PTHREAD_ROBUST_OWNER_MASK)
        return EIO;
    *owner_id_out = (uint32_t)result;
    return 0;
}

static void rin_pthread_mutex_robust_owner_release(pthread_mutex_t* mutex,
                                                    pthread_t owner) {
    if (!mutex) return;
    (void)syscall(SYS_futex, &mutex->locked,
                  FUTEX_RIN_ROBUST_UNREGISTER |
                      (rin_pthread_mutex_is_prio_inherit(mutex)
                           ? FUTEX_RIN_PI_FLAG : 0) |
                      (rin_pthread_mutex_is_prio_inherit(mutex) &&
                       rin_pthread_mutex_is_robust(mutex)
                           ? FUTEX_RIN_PI_ROBUST_FLAG : 0) |
                      (rin_pthread_mutex_is_shared(mutex) ? 0 :
                       FUTEX_PRIVATE_FLAG),
                  (uintptr_t)&mutex->owner, (void*)(uintptr_t)owner,
                  NULL, 0);
}

static int rin_pthread_mutex_robust_lock(pthread_mutex_t* mutex,
                                        pthread_t owner, int type,
                                        int try_only,
                                        const struct timespec* abstime,
                                        clockid_t clock_id) {
    uint32_t owner_id;
    int timed_out = 0;
    int register_error = rin_pthread_mutex_robust_owner_id(
        mutex, owner, &owner_id);
    if (register_error != 0) return register_error;
    for (;;) {
        uint32_t state = __atomic_load_n((uint32_t*)&mutex->locked,
                                         __ATOMIC_ACQUIRE);
        uint32_t current_owner = state & RIN_PTHREAD_ROBUST_OWNER_MASK;
        if ((__atomic_load_n(&mutex->type, __ATOMIC_ACQUIRE) &
             RIN_PTHREAD_MUTEX_NOT_RECOVERABLE_FLAG) != 0) {
            rin_pthread_mutex_robust_owner_release(mutex, owner);
            return ENOTRECOVERABLE;
        }
        if (current_owner == owner_id) {
            if (type == PTHREAD_MUTEX_RECURSIVE) {
                if (mutex->recursion == __INT_MAX__) return EAGAIN;
                mutex->recursion++;
                return 0;
            }
            if (type == PTHREAD_MUTEX_ERRORCHECK) return EDEADLK;
            if (try_only || timed_out)
                return timed_out ? ETIMEDOUT : EBUSY;
        }
        if (current_owner == 0u) {
            uint32_t desired = (state & (RIN_PTHREAD_ROBUST_OWNER_DIED |
                                         RIN_PTHREAD_ROBUST_WAITERS)) |
                               owner_id;
            if (__atomic_compare_exchange_n((uint32_t*)&mutex->locked,
                                            &state, desired, 0,
                                            __ATOMIC_ACQUIRE,
                                            __ATOMIC_RELAXED)) {
                __rin_pthread_mutex_owner_store(mutex, owner);
                mutex->recursion = 1;
                if (rin_pthread_mutex_is_prio_inherit(mutex)) {
                    uint32_t refreshed_owner_id = 0u;
                    (void)rin_pthread_mutex_robust_owner_id(
                        mutex, owner, &refreshed_owner_id);
                }
                return (desired & RIN_PTHREAD_ROBUST_OWNER_DIED) != 0u
                    ? EOWNERDEAD : 0;
            }
            continue;
        }
        if (try_only || timed_out) {
            rin_pthread_mutex_robust_owner_release(mutex, owner);
            return timed_out ? ETIMEDOUT : EBUSY;
        }
        if ((state & RIN_PTHREAD_ROBUST_WAITERS) == 0u) {
            uint32_t expected = state;
            uint32_t desired = state | RIN_PTHREAD_ROBUST_WAITERS;
            if (!__atomic_compare_exchange_n((uint32_t*)&mutex->locked,
                                             &expected, desired, 0,
                                             __ATOMIC_ACQ_REL,
                                             __ATOMIC_ACQUIRE))
                continue;
            state = desired;
        }
        {
            long wait_result;
            /* A negative absolute clock deadline is already expired. Retry
             * the atomic acquisition check once before returning timeout,
             * and never pass an invalid absolute time to the futex ABI. */
            if (abstime && abstime->tv_sec < 0) {
                timed_out = 1;
                continue;
            }
            if (abstime) {
                wait_result = _RIN_PTHREAD_RUNTIME_FUTEX_BITSET(
                    &mutex->locked,
                    rin_pthread_mutex_futex_operation(
                        mutex, FUTEX_WAIT_BITSET |
                            (clock_id == CLOCK_REALTIME
                                 ? FUTEX_CLOCK_REALTIME : 0)),
                    (int)state, abstime, FUTEX_BITSET_MATCH_ANY);
            } else {
                wait_result = _RIN_PTHREAD_RUNTIME_FUTEX(
                    &mutex->locked,
                    rin_pthread_mutex_futex_operation(mutex, FUTEX_WAIT),
                    (int)state, NULL);
            }
            int wait_error = rin_pthread_futex_wait_error(wait_result,
                                                           errno);
            if (wait_error == ETIMEDOUT && abstime) {
                /* An unlock can race with the deadline; retry acquisition
                 * before deciding that the timed operation has failed. */
                timed_out = 1;
                continue;
            }
            if (wait_error == EINTR) continue;
            if (wait_error != 0) {
                rin_pthread_mutex_robust_owner_release(mutex, owner);
                return wait_error;
            }
        }
    }
}

#ifndef _RIN_PTHREAD_RUNTIME_CLONE
#define _RIN_PTHREAD_RUNTIME_CLONE(flags, start, argument, stack, size, guard) \
    _syscall6((uintptr_t)SYS_CLONE, (uintptr_t)(flags), (uintptr_t)(start), \
              (uintptr_t)(argument), (uintptr_t)(stack), (uintptr_t)(size), \
              (uintptr_t)(guard))
#endif

#ifndef _RIN_PTHREAD_RUNTIME_DETACH
#define _RIN_PTHREAD_RUNTIME_DETACH(thread) \
    _syscall1((uintptr_t)SYS_THREAD_DETACH, (uintptr_t)(thread))
#endif

#ifndef _RIN_PTHREAD_RUNTIME_MMAP
#define _RIN_PTHREAD_RUNTIME_MMAP(address, length, protection, flags) \
    _syscall4((uintptr_t)SYS_MMAP, (uintptr_t)(address), (uintptr_t)(length), \
              (uintptr_t)(protection), (uintptr_t)(flags))
#endif

#ifndef _RIN_PTHREAD_RUNTIME_MPROTECT
#define _RIN_PTHREAD_RUNTIME_MPROTECT(address, length, protection) \
    _syscall3((uintptr_t)SYS_MPROTECT, (uintptr_t)(address), (uintptr_t)(length), \
              (uintptr_t)(protection))
#endif

#ifndef _RIN_PTHREAD_RUNTIME_MUNMAP
#define _RIN_PTHREAD_RUNTIME_MUNMAP(address, length) \
    _syscall2((uintptr_t)SYS_MUNMAP, (uintptr_t)(address), (uintptr_t)(length))
#endif

#ifndef _RIN_PTHREAD_RUNTIME_JOIN
#define _RIN_PTHREAD_RUNTIME_JOIN(thread) \
    _syscall1((uintptr_t)SYS_THREAD_JOIN, (uintptr_t)(thread))
#endif

#ifndef _RIN_PTHREAD_RUNTIME_SELF
#define _RIN_PTHREAD_RUNTIME_SELF() pthread_self()
#endif

#if defined(__x86_64__) || defined(_M_X64)
/* Keep the bootstrap argument record below the initial callee frame,
 * the SysV red-zone, and a small bootstrap spill area. */
#define THREAD_BOOTSTRAP_RESERVED_64 0x100UL
#endif

#define PROT_NONE      0x00
#define PROT_READ      0x01
#define PROT_WRITE     0x02

#define MAP_PRIVATE    0x02
#define MAP_ANONYMOUS  0x20
#define MAP_STACK      0x20000

typedef struct {
    pthread_t tid;
    void* alloc_base;
    uintptr_t alloc_size;
    int alloc_kind; /* 1=malloc, 2=mmap */
    int detached;
} rin_thread_stack_rec_t;

enum {
    RIN_THREAD_ALLOC_NONE = 0,
    RIN_THREAD_ALLOC_MALLOC = 1,
    RIN_THREAD_ALLOC_MMAP = 2
};

#define RIN_THREAD_STACK_REC_MAX 256
static rin_thread_stack_rec_t g_thread_stack_recs[RIN_THREAD_STACK_REC_MAX];
static volatile unsigned int g_thread_stack_lock = 0;

/* pthread_tls.c で定義 */
extern void __pthread_run_destructors(void);
/* rin_atexit_registry.inc で定義 */
extern void __rin_cxa_thread_finalize(void);
/* The stdio owner is optional in small runtimes. When linked, release any
 * FILE locks left by the exiting thread after its TLS destructors run. */
extern void rin_stdio_thread_exit_cleanup(void) __attribute__((weak));
void rin_log(const char* msg);

typedef struct {
    void* (*start_routine)(void*);
    void* arg;
    volatile int start_release;
    int start_error;
} rin_thread_start_record_t;

static uintptr_t rin_align_up_word(uintptr_t value, uintptr_t align) {
    return (value + align - (uintptr_t)1) & ~(align - (uintptr_t)1);
}

static int rin_pthread_syscall_error(intptr_t result) {
    if (result >= 0 || result < -(intptr_t)4095) return EIO;
    return (int)-result;
}

#if defined(__x86_64__) || defined(_M_X64)
static uintptr_t rin_align_down_word(uintptr_t value, uintptr_t align) {
    return value & ~(align - (uintptr_t)1);
}
#endif

static void rin_thread_stack_lock_acquire(void) {
    while (__sync_lock_test_and_set(&g_thread_stack_lock, 1U)) {
        (void)_syscall0((uintptr_t)SYS_SCHED_YIELD);
    }
}

static void rin_thread_stack_lock_release(void) {
    __sync_lock_release(&g_thread_stack_lock);
}

static rin_thread_stack_rec_t* rin_thread_stack_find_locked(pthread_t tid) {
    for (unsigned int i = 0; i < RIN_THREAD_STACK_REC_MAX; i++) {
        if (g_thread_stack_recs[i].tid == tid) {
            return &g_thread_stack_recs[i];
        }
    }
    return NULL;
}

static int rin_thread_name_copy(char destination[16], const char* source) {
    unsigned int index;

    if (!source) return EINVAL;
    for (index = 0; index < 16u; ++index) {
        char ch = source[index];
        if (ch == '\0') {
            destination[index] = '\0';
            return 0;
        }
        destination[index] = ch;
    }
    return ERANGE;
}

static void rin_thread_stack_register(pthread_t tid, void* alloc_base,
                                      uintptr_t alloc_size, int alloc_kind,
                                      int detached) {
    if (tid == 0 || !alloc_base || alloc_size == 0 || alloc_kind == RIN_THREAD_ALLOC_NONE) {
        return;
    }

    rin_thread_stack_lock_acquire();
    rin_thread_stack_rec_t* rec = rin_thread_stack_find_locked(tid);
    if (!rec) {
        for (unsigned int i = 0; i < RIN_THREAD_STACK_REC_MAX; i++) {
            if (g_thread_stack_recs[i].tid == 0) {
                rec = &g_thread_stack_recs[i];
                break;
            }
        }
    }
    if (rec) {
        rec->tid = tid;
        rec->alloc_base = alloc_base;
        rec->alloc_size = alloc_size;
        rec->alloc_kind = alloc_kind;
        rec->detached = detached;
    }
    rin_thread_stack_lock_release();
}

static int rin_thread_stack_take(pthread_t tid, void** alloc_base,
                                 uintptr_t* alloc_size, int* alloc_kind,
                                 int* detached) {
    int found = 0;
    if (alloc_base) *alloc_base = NULL;
    if (alloc_size) *alloc_size = 0;
    if (alloc_kind) *alloc_kind = RIN_THREAD_ALLOC_NONE;
    if (detached) *detached = 0;

    if (tid == 0) return -1;

    rin_thread_stack_lock_acquire();
    rin_thread_stack_rec_t* rec = rin_thread_stack_find_locked(tid);
    if (rec) {
        if (alloc_base) *alloc_base = rec->alloc_base;
        if (alloc_size) *alloc_size = rec->alloc_size;
        if (alloc_kind) *alloc_kind = rec->alloc_kind;
        if (detached) *detached = rec->detached;
        rec->tid = 0;
        rec->alloc_base = NULL;
        rec->alloc_size = 0;
        rec->alloc_kind = RIN_THREAD_ALLOC_NONE;
        rec->detached = 0;
        found = 1;
    }
    rin_thread_stack_lock_release();

    return found ? 0 : -1;
}

static int rin_pthread_setname_np_impl(pthread_t thread, const char* name) {
    char candidate[16] = {0};
    RinThreadNameCallV1 request;
    intptr_t result;
    int validation;

    if (thread == 0) return ESRCH;
    validation = rin_thread_name_copy(candidate, name);
    if (validation != 0) return validation;
    for (size_t index = 0u; index < sizeof(request); ++index)
        ((unsigned char*)&request)[index] = 0u;
    request.struct_size = (uint32_t)sizeof(request);
    request.version = RIN_THREAD_NAME_CALL_VERSION;
    request.operation = RIN_THREAD_NAME_OPERATION_SET;
    request.thread_id = (uint64_t)thread;
    for (size_t index = 0u; index < sizeof(candidate); ++index)
        request.name[index] = candidate[index];
    result = _RIN_PTHREAD_NAME_SYSCALL1(SYS_THREAD_NAME_CALL, &request);
    return _rin_pthread_error_from_result(result);
}

int pthread_setname_np(pthread_t thread, const char* name) {
    int saved_errno = errno;
    int result = rin_pthread_setname_np_impl(thread, name);
    errno = saved_errno;
    return result;
}

static int rin_pthread_getname_np_impl(pthread_t thread, char* name,
                                       size_t length) {
    RinThreadNameCallV1 request;
    intptr_t result;
    size_t terminator = RIN_THREAD_NAME_CAPACITY;

    if (!name) return EINVAL;
    if (length == 0u) return ERANGE;
    if (thread == 0) return ESRCH;
    for (size_t index = 0u; index < sizeof(request); ++index)
        ((unsigned char*)&request)[index] = 0u;
    request.struct_size = (uint32_t)sizeof(request);
    request.version = RIN_THREAD_NAME_CALL_VERSION;
    request.operation = RIN_THREAD_NAME_OPERATION_GET;
    request.thread_id = (uint64_t)thread;
    result = _RIN_PTHREAD_NAME_SYSCALL1(SYS_THREAD_NAME_CALL, &request);
    {
        int error = _rin_pthread_error_from_result(result);
        if (error != 0) return error;
    }
    if (request.struct_size != sizeof(request) ||
        request.version != RIN_THREAD_NAME_CALL_VERSION ||
        request.operation != RIN_THREAD_NAME_OPERATION_GET ||
        request.thread_id != (uint64_t)thread || request.reserved[0] != 0u ||
        request.reserved[1] != 0u)
        return EIO;
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
    if (length <= terminator) return ERANGE;
    for (size_t index = 0u; index <= terminator; ++index)
        name[index] = request.name[index];
    return 0;
}

int pthread_getname_np(pthread_t thread, char* name, size_t length) {
    int saved_errno = errno;
    int result = rin_pthread_getname_np_impl(thread, name, length);
    errno = saved_errno;
    return result;
}

void __rin_pthread_run_exit_destructors(void) {
    /* Itanium C++ ABI thread_local objects precede POSIX TSD cleanup. */
    __rin_cxa_thread_finalize();
    __pthread_run_destructors();
    if (rin_stdio_thread_exit_cleanup)
        rin_stdio_thread_exit_cleanup();
}

/* スレッド終了ラッパー: デストラクタ反復 → SYS_THREAD_EXIT */
void pthread_exit(void* retval) {
    pthread_t self = _RIN_PTHREAD_RUNTIME_SELF();
    rin_pthread_cancel_run_cleanups(self);
    rin_pthread_cancel_exit(self, retval);
    __rin_pthread_run_exit_destructors();
#if defined(__x86_64__) || defined(_M_X64)
    __asm__ volatile(
        "xor %%rdi, %%rdi\n"
        "mov %0, %%rax\n"
        "syscall\n"
        "ud2\n"
        :: "i"(SYS_THREAD_EXIT)
        : "rdi", "rax", "rcx", "r11", "memory"
    );
#else
    __asm__ volatile(
        "xor %%ebx, %%ebx\n"
        "mov %0, %%eax\n"
        "int $0x80\n"
        "ud2\n"
        :: "i"(SYS_THREAD_EXIT)
        : "eax", "ebx", "memory"
    );
#endif
    __builtin_unreachable();
}

static void __thread_exit_wrapper(void* retval);

/* start_routine が return した時に __thread_exit_wrapper を呼ぶ */
static void __attribute__((naked, used)) __thread_exit_trampoline(void) {
    __asm__ volatile(
        "call __thread_exit_wrapper\n"
        "ud2\n"
    );
}

static void* __attribute__((used)) __pthread_thread_bootstrap(void* raw_arg) {
    rin_thread_start_record_t* start = (rin_thread_start_record_t*)raw_arg;
    void* (*start_routine)(void*) = NULL;
    void* start_arg = NULL;
    void* result = NULL;

    (void)rin_pthread_cancel_register(_RIN_PTHREAD_RUNTIME_SELF());

    if (start) {
        while (__atomic_load_n(&start->start_release, __ATOMIC_ACQUIRE) == 0) {
            (void)_RIN_PTHREAD_RUNTIME_FUTEX(
                &start->start_release, FUTEX_WAIT_PRIVATE, 0, NULL);
        }
        if (__atomic_load_n(&start->start_error, __ATOMIC_ACQUIRE) != 0) {
            __thread_exit_wrapper(NULL);
            __builtin_unreachable();
        }
        start_routine = start->start_routine;
        start_arg = start->arg;
    }

    if (start_routine) {
        result = start_routine(start_arg);
    }

    __thread_exit_wrapper(result);
    __builtin_unreachable();
}

static void __attribute__((used)) __thread_exit_wrapper(void* retval) {
    pthread_exit(retval);
}

static int rin_pthread_detach_created(pthread_t thread) {
    intptr_t detach_result = _RIN_PTHREAD_RUNTIME_DETACH(thread);
    enum rin_thread_detach_outcome detach_outcome =
        rin_thread_detach_classify(detach_result);
    if (detach_outcome == RIN_THREAD_DETACH_INVALID)
        return rin_pthread_syscall_error(detach_result);

    /* Detached stack VMAs become kernel-owned.  Drop userspace bookkeeping
     * before allowing the child to leave its start gate. */
    rin_pthread_cancel_set_detached(thread);
    (void)rin_thread_stack_take(thread, NULL, NULL, NULL, NULL);
    return 0;
}

static void rin_pthread_start_release(rin_thread_start_record_t* record,
                                      int start_error) {
    __atomic_store_n(&record->start_error, start_error, __ATOMIC_RELAXED);
    __atomic_store_n(&record->start_release, 1, __ATOMIC_RELEASE);
    (void)_RIN_PTHREAD_RUNTIME_FUTEX(
        &record->start_release, FUTEX_WAKE_PRIVATE, 1, NULL);
}

static int rin_pthread_create_impl(pthread_t* thread,
                                   const pthread_attr_t* attr,
                                   void* (*start_routine)(void*), void* arg) {
    void* stack_usable_base = NULL;
    uintptr_t stack_usable_size = 0;
    uintptr_t stack_guard_size = 0;
    void* alloc_base = NULL;
    uintptr_t alloc_size = 0;
    int alloc_kind = RIN_THREAD_ALLOC_NONE;
    int caller_stack = attr && attr->stackaddr != NULL;
    int schedule_policy = SCHED_OTHER;
    struct sched_param schedule_param = {0};
    uintptr_t requested_stack = THREAD_STACK_DEFAULT;
    uintptr_t requested_guard = THREAD_STACK_GUARD;

    if (!thread || !start_routine) return EINVAL;

    if (attr) {
        if (attr->detachstate != PTHREAD_CREATE_JOINABLE &&
            attr->detachstate != PTHREAD_CREATE_DETACHED)
            return EINVAL;
        if (attr->scope != PTHREAD_SCOPE_SYSTEM) return ENOTSUP;
        if (attr->guardsize >
            SIZE_MAX - (size_t)(THREAD_PAGE_SIZE - 1UL))
            return EINVAL;
        requested_guard = (uintptr_t)attr->guardsize;
        if (attr->inheritsched == PTHREAD_EXPLICIT_SCHED) {
            if (!_rin_pthread_sched_policy_supported(attr->schedpolicy))
                return attr->schedpolicy == SCHED_DEADLINE ? ENOTSUP : EINVAL;
            if (!_rin_sched_policy_priority_valid(attr->schedpolicy,
                                                  attr->sched_priority))
                return EINVAL;
            schedule_policy = attr->schedpolicy;
            schedule_param.sched_priority = attr->sched_priority;
        } else if (attr->inheritsched == PTHREAD_INHERIT_SCHED) {
            int schedule_result = pthread_getschedparam(
                _RIN_PTHREAD_RUNTIME_SELF(), &schedule_policy,
                &schedule_param);
            if (schedule_result != 0) return schedule_result;
            if (!_rin_pthread_sched_policy_supported(schedule_policy))
                return schedule_policy == SCHED_DEADLINE ? ENOTSUP : EINVAL;
            if (!_rin_sched_policy_priority_valid(
                    schedule_policy, schedule_param.sched_priority))
                return EINVAL;
        } else {
            return EINVAL;
        }
    } else {
        int schedule_result = pthread_getschedparam(
            _RIN_PTHREAD_RUNTIME_SELF(), &schedule_policy, &schedule_param);
        if (schedule_result != 0) return schedule_result;
        if (!_rin_pthread_sched_policy_supported(schedule_policy))
            return schedule_policy == SCHED_DEADLINE ? ENOTSUP : EINVAL;
        if (!_rin_sched_policy_priority_valid(
                schedule_policy, schedule_param.sched_priority))
            return EINVAL;
    }

    if (caller_stack) {
        /* A caller-provided stack is a bounded object: never enlarge its
         * declared extent to the runtime's preferred allocation minimum. */
        if (attr->stacksize < PTHREAD_STACK_MIN) return EINVAL;
        requested_stack = (uintptr_t)attr->stacksize;
        if (requested_stack >
            UINTPTR_MAX - (uintptr_t)attr->stackaddr)
            return EINVAL;
    } else if (attr && attr->stacksize > 0) {
        if (attr->stacksize < PTHREAD_STACK_MIN) return EINVAL;
        requested_stack = (uintptr_t)attr->stacksize;
    }
    if (!caller_stack) {
        if (requested_stack < PTHREAD_STACK_MIN ||
            requested_stack > (UINTPTR_MAX - (THREAD_PAGE_SIZE - 1UL)) ||
            requested_guard > (UINTPTR_MAX - (THREAD_PAGE_SIZE - 1UL))) {
            return EINVAL;
        }
        requested_stack = rin_align_up_word(requested_stack, THREAD_PAGE_SIZE);
        stack_guard_size = rin_align_up_word(requested_guard,
                                              THREAD_PAGE_SIZE);
    } else {
        /* A caller-provided stack is already a bounded mapping owned by the
         * caller; the pthread runtime must not reserve or protect its prefix. */
        stack_guard_size = 0u;
    }

    if (caller_stack) {
        stack_usable_base = attr->stackaddr;
        stack_usable_size = requested_stack;
#if defined(__x86_64__) || defined(_M_X64)
        {
            uintptr_t ignored_top = 0;
            if (!rin_thread_stack_top_checked((uintptr_t)stack_usable_base,
                                              (size_t)stack_usable_size,
                                              &ignored_top)) {
                return EINVAL;
            }
        }
#endif
    }

    if (!stack_usable_base) {
        /* Threads must use a dedicated stack VM region with guard pages.
         * Physical backing still comes from the kernel page allocator, but
         * the logical owner remains the thread stack, never malloc/free. */
        uintptr_t total_map;
        if (requested_stack > (UINTPTR_MAX - stack_guard_size)) {
            return EINVAL;
        }
        total_map = requested_stack + stack_guard_size;
        intptr_t map_result = _RIN_PTHREAD_RUNTIME_MMAP(
            0, total_map, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK);
        if (map_result >= 0 && map_result != 0) {
            void* map_base = (void*)(uintptr_t)map_result;
            intptr_t protection_result = _RIN_PTHREAD_RUNTIME_MPROTECT(
                (uintptr_t)((unsigned char*)map_base + stack_guard_size),
                requested_stack, PROT_READ | PROT_WRITE);
            if (protection_result >= 0) {
                alloc_base = map_base;
                alloc_size = total_map;
                alloc_kind = RIN_THREAD_ALLOC_MMAP;
                stack_usable_base = (void*)((unsigned char*)map_base +
                                            stack_guard_size);
                stack_usable_size = requested_stack;

#if defined(__x86_64__) || defined(_M_X64)
                {
                    uintptr_t ignored_top = 0;
                    if (!rin_thread_stack_top_checked((uintptr_t)stack_usable_base,
                                                      (size_t)stack_usable_size,
                                                      &ignored_top)) {
                        (void)_RIN_PTHREAD_RUNTIME_MUNMAP((uintptr_t)map_base,
                                                          total_map);
                        alloc_base = NULL;
                        alloc_size = 0;
                        alloc_kind = RIN_THREAD_ALLOC_NONE;
                        stack_usable_base = NULL;
                        stack_usable_size = 0;
                        stack_guard_size = 0;
                    }
                }
#endif
            } else {
                static const char digits[] = "0123456789ABCDEF";
                char message[] =
                    "[PTHREAD] stack mprotect failed result=0x0000000000000000\n";
                uint64_t raw_result = (uint64_t)protection_result;
                for (uint32_t index = 0u; index < 16u; ++index) {
                    uint32_t shift = 60u - index * 4u;
                    message[41u + index] = digits[
                        (raw_result >> shift) & UINT64_C(0x0f)];
                }
                rin_log(message);
                (void)_RIN_PTHREAD_RUNTIME_MUNMAP((uintptr_t)map_base, total_map);
            }
        } else {
            rin_log("[PTHREAD] stack mmap failed\n");
        }
    }

    if (!stack_usable_base || stack_usable_size < 32) {
        if (alloc_kind == RIN_THREAD_ALLOC_MMAP) {
            (void)_RIN_PTHREAD_RUNTIME_MUNMAP((uintptr_t)alloc_base, alloc_size);
        } else if (alloc_kind == RIN_THREAD_ALLOC_MALLOC) {
            free(alloc_base);
        }
        return EAGAIN;
    }

    /* Private anonymous mmap stacks are zero-filled by the VMM on demand:
     * untouched pages map the pinned zero page and writes use zeroed COW
     * pages.  Do not fault in the full stack before the new thread runs. */
    if (alloc_kind != RIN_THREAD_ALLOC_MMAP &&
        !(attr && attr->stackaddr != NULL)) {
        memset(stack_usable_base, 0, (size_t)stack_usable_size);
    }

    {
        uintptr_t stack_top = 0;
        uintptr_t record_size = rin_align_up_word(
            (uintptr_t)sizeof(rin_thread_start_record_t), (uintptr_t)sizeof(uintptr_t));
        uintptr_t record_addr = 0;
        rin_thread_start_record_t* record = NULL;
#if defined(__x86_64__) || defined(_M_X64)
        uintptr_t record_limit = 0;
        {
            uintptr_t checked_top = 0;
            if (!rin_thread_stack_top_checked((uintptr_t)stack_usable_base,
                                              (size_t)stack_usable_size,
                                              &checked_top)) {
                if (alloc_kind == RIN_THREAD_ALLOC_MMAP) {
                    (void)_RIN_PTHREAD_RUNTIME_MUNMAP((uintptr_t)alloc_base,
                                                      alloc_size);
                } else if (alloc_kind == RIN_THREAD_ALLOC_MALLOC) {
                    free(alloc_base);
                }
                return EINVAL;
            }
            stack_top = checked_top;
        }
        record_limit = stack_top - sizeof(uintptr_t);
        if (record_limit <= THREAD_BOOTSTRAP_RESERVED_64 + record_size) {
            if (alloc_kind == RIN_THREAD_ALLOC_MMAP) {
                (void)_RIN_PTHREAD_RUNTIME_MUNMAP((uintptr_t)alloc_base, alloc_size);
            } else if (alloc_kind == RIN_THREAD_ALLOC_MALLOC) {
                free(alloc_base);
            }
            return EINVAL;
        }
        record_addr = rin_align_down_word(record_limit - THREAD_BOOTSTRAP_RESERVED_64 - record_size,
                                          16UL);
        if (record_addr < (uintptr_t)stack_usable_base ||
            record_addr + record_size > record_limit - THREAD_BOOTSTRAP_RESERVED_64) {
            if (alloc_kind == RIN_THREAD_ALLOC_MMAP) {
                (void)_RIN_PTHREAD_RUNTIME_MUNMAP((uintptr_t)alloc_base, alloc_size);
            } else if (alloc_kind == RIN_THREAD_ALLOC_MALLOC) {
                free(alloc_base);
            }
            return EINVAL;
        }
#else
        {
            uintptr_t shadow_bytes = sizeof(uintptr_t) * 2UL;
            stack_top = (((uintptr_t)stack_usable_base + stack_usable_size) &
                         ~(uintptr_t)0xFULL);
            if (stack_top < (uintptr_t)stack_usable_base + shadow_bytes + record_size) {
                if (alloc_kind == RIN_THREAD_ALLOC_MMAP) {
                    (void)_RIN_PTHREAD_RUNTIME_MUNMAP((uintptr_t)alloc_base,
                                                      alloc_size);
                } else if (alloc_kind == RIN_THREAD_ALLOC_MALLOC) {
                    free(alloc_base);
                }
                return EINVAL;
            }
            record_addr = stack_top - shadow_bytes - record_size;
        }
#endif
        record = (rin_thread_start_record_t*)(uintptr_t)record_addr;
        record->start_routine = start_routine;
        record->arg = arg;
        record->start_release = 0;
        record->start_error = 0;
        *(uintptr_t*)(uintptr_t)(stack_top - sizeof(uintptr_t)) =
            (uintptr_t)__thread_exit_trampoline;

        uintptr_t flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD;
        intptr_t ret = _RIN_PTHREAD_RUNTIME_CLONE(
            flags,
            (uintptr_t)__pthread_thread_bootstrap,
            record_addr,
            (uintptr_t)stack_usable_base,
            stack_usable_size,
            stack_guard_size);
        if (ret <= 0) {
            int err = rin_pthread_syscall_error(ret);
            rin_log("[PTHREAD] clone failed\n");
            if (alloc_kind == RIN_THREAD_ALLOC_MMAP) {
                (void)_RIN_PTHREAD_RUNTIME_MUNMAP((uintptr_t)alloc_base, alloc_size);
            } else if (alloc_kind == RIN_THREAD_ALLOC_MALLOC) {
                free(alloc_base);
            }
            return err;
        }

        pthread_t created_thread = (pthread_t)ret;
        int create_error =
            rin_pthread_cancel_register_created(created_thread);
        if (alloc_kind != RIN_THREAD_ALLOC_NONE) {
            rin_thread_stack_register(created_thread, alloc_base, alloc_size,
                                      alloc_kind, 0);
        }
        if (create_error == 0) {
            create_error = pthread_setschedparam(created_thread,
                                                  schedule_policy,
                                                  &schedule_param);
        }
        if (create_error == 0 && attr &&
            attr->detachstate == PTHREAD_CREATE_DETACHED) {
            create_error = rin_pthread_detach_created(created_thread);
        }
        if (create_error != 0) {
            /* Do not run application code after a failed scheduler setup or
             * detach operation.  Detach internally when possible; otherwise
             * release the child into its error exit and reap it here. */
            if (rin_pthread_detach_created(created_thread) == 0) {
                rin_pthread_start_release(record, create_error);
            } else {
                rin_pthread_start_release(record, create_error);
                (void)pthread_join(created_thread, NULL);
            }
            return create_error;
        }

        *thread = created_thread;
        rin_pthread_start_release(record, 0);
        return 0;
    }
}

int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg) {
    int saved_errno = errno;
    int result = rin_pthread_create_impl(thread, attr, start_routine, arg);
    errno = saved_errno;
    return result;
}

static int rin_pthread_join_impl(pthread_t thread, void** retval) {
    int detached = 0;
    if (retval) *retval = NULL;
    if (thread == 0u) return ESRCH;
    pthread_testcancel();

    rin_thread_stack_lock_acquire();
    rin_thread_stack_rec_t* rec = rin_thread_stack_find_locked(thread);
    if (rec) detached = rec->detached;
    rin_thread_stack_lock_release();

    if (detached) {
        return EINVAL;
    }

    /* SYS_THREAD_JOIN has no timeout form.  Poll liveness in short bounded
     * sleeps so deferred cancellation remains observable while a join waits;
     * the final join syscall still owns the single reap operation. */
    for (;;) {
        int probe = pthread_kill(thread, 0);
        if (probe == ESRCH) break;
        if (probe != 0) return probe;
        {
            struct timespec pause = {0, 10000000L};
            (void)nanosleep(&pause, NULL);
        }
        pthread_testcancel();
    }

    {
        intptr_t ret = _RIN_PTHREAD_RUNTIME_JOIN(thread);
        if (ret != 0) {
            if (rin_pthread_syscall_error(ret) == EINTR) {
                pthread_testcancel();
                return EINTR;
            }
            return rin_pthread_syscall_error(ret);
        }
    }

    rin_pthread_cancel_take_exit(thread, retval);

    {
        void* alloc_base = NULL;
        uintptr_t alloc_size = 0;
        int alloc_kind = RIN_THREAD_ALLOC_NONE;
        int ignored_detached = 0;
        if (rin_thread_stack_take(thread, &alloc_base, &alloc_size, &alloc_kind, &ignored_detached) == 0) {
            if (alloc_kind == RIN_THREAD_ALLOC_MMAP) {
                (void)_RIN_PTHREAD_RUNTIME_MUNMAP((uintptr_t)alloc_base, alloc_size);
            } else if (alloc_kind == RIN_THREAD_ALLOC_MALLOC) {
                free(alloc_base);
            }
        }
    }

    return 0;
}

int pthread_join(pthread_t thread, void** retval) {
    int saved_errno = errno;
    int result = rin_pthread_join_impl(thread, retval);
    errno = saved_errno;
    return result;
}

static int rin_pthread_detach_impl(pthread_t thread) {
    int found = 0;
    int already_detached = 0;

    if (thread == 0) return EINVAL;

    rin_thread_stack_lock_acquire();
    rin_thread_stack_rec_t* rec = rin_thread_stack_find_locked(thread);
    if (rec) {
        found = 1;
        if (rec->detached) {
            already_detached = 1;
        }
    }
    rin_thread_stack_lock_release();

    if (found && already_detached) {
        return EINVAL;
    }

    {
        intptr_t ret = _RIN_PTHREAD_RUNTIME_DETACH(thread);
        enum rin_thread_detach_outcome outcome =
            rin_thread_detach_classify(ret);
        if (outcome == RIN_THREAD_DETACH_INVALID)
            return rin_pthread_syscall_error(ret);

        /* The kernel owns and reclaims the MAP_STACK VMA after detach.  Do
         * not munmap it here: an active detached thread is still executing
         * on that VMA, and an already reaped thread has already had it
         * reclaimed by the kernel.  Forget only the user bookkeeping record
         * so repeated short-lived detached workers cannot exhaust the table.
         */
        rin_pthread_cancel_set_detached(thread);
        (void)rin_thread_stack_take(thread, NULL, NULL, NULL, NULL);
        return 0;
    }
}

int pthread_detach(pthread_t thread) {
    int saved_errno = errno;
    int result = rin_pthread_detach_impl(thread);
    errno = saved_errno;
    return result;
}

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) {
    if (!mutex) return EINVAL;
    if (attr) {
        int type = attr->type & RIN_PTHREAD_MUTEX_TYPE_MASK;
        if (type != PTHREAD_MUTEX_NORMAL &&
            type != PTHREAD_MUTEX_RECURSIVE &&
            type != PTHREAD_MUTEX_ERRORCHECK) return EINVAL;
        if ((attr->type & ~(RIN_PTHREAD_MUTEX_TYPE_MASK |
                            RIN_PTHREAD_MUTEXATTR_PRIO_INHERIT_FLAG)) != 0)
            return EINVAL;
        int pshared = attr->pshared & RIN_PTHREAD_MUTEXATTR_PSHARED_MASK;
        if (pshared != PTHREAD_PROCESS_PRIVATE &&
            pshared != PTHREAD_PROCESS_SHARED) return EINVAL;
    }
    mutex->locked = 0;
    __rin_pthread_mutex_owner_store(mutex, 0);
    mutex->type = (attr
                       ? (attr->type & RIN_PTHREAD_MUTEX_TYPE_MASK)
                       : PTHREAD_MUTEX_DEFAULT) |
        (rin_pthread_mutexattr_is_shared(attr) ?
             RIN_PTHREAD_MUTEX_PSHARED_FLAG : 0);
    if (rin_pthread_mutexattr_is_robust(attr))
        mutex->type |= RIN_PTHREAD_MUTEX_ROBUST_FLAG;
    if (attr &&
        (attr->type & RIN_PTHREAD_MUTEXATTR_PRIO_INHERIT_FLAG) != 0)
        mutex->type |= RIN_PTHREAD_MUTEX_PRIO_INHERIT_FLAG;
    mutex->recursion = 0;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;
    if (rin_pthread_mutex_is_robust(mutex) ||
        rin_pthread_mutex_is_prio_inherit(mutex)) {
        uint32_t state = __atomic_load_n((uint32_t*)&mutex->locked,
                                         __ATOMIC_ACQUIRE);
        if ((state & (RIN_PTHREAD_ROBUST_OWNER_MASK |
                      RIN_PTHREAD_ROBUST_OWNER_DIED)) != 0u ||
            (state & RIN_PTHREAD_ROBUST_WAITERS) != 0u)
            return EBUSY;
    } else if (mutex->locked) {
        return EBUSY;
    }
    return 0;
}

static int rin_pthread_mutex_lock_impl(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;

    pthread_t self = _RIN_PTHREAD_RUNTIME_SELF();
    pthread_t owner = rin_pthread_mutex_owner_token(mutex, self);
    int type = rin_pthread_mutex_type(mutex);

    if (rin_pthread_mutex_is_robust(mutex) ||
        rin_pthread_mutex_is_prio_inherit(mutex)) {
        return rin_pthread_mutex_robust_lock(mutex, owner, type, 0, NULL,
                                             CLOCK_REALTIME);
    }

    if (type == PTHREAD_MUTEX_RECURSIVE &&
        __rin_pthread_mutex_owner_load(mutex) == owner) {
        if (mutex->recursion == __INT_MAX__) return EAGAIN;
        mutex->recursion++;
        return 0;
    }

    if (type == PTHREAD_MUTEX_ERRORCHECK &&
        __rin_pthread_mutex_owner_load(mutex) == owner) {
        return EDEADLK;
    }

    {
        int c = 0;
        if (!__atomic_compare_exchange_n(&mutex->locked, &c, 1, 0,
                                          __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            if (c != 2) {
                c = __atomic_exchange_n(&mutex->locked, 2, __ATOMIC_ACQUIRE);
            }
            while (c != 0) {
                long wait_result = _RIN_PTHREAD_RUNTIME_FUTEX(
                    &mutex->locked,
                    rin_pthread_mutex_futex_operation(mutex, FUTEX_WAIT),
                    2, NULL);
                int wait_error = rin_pthread_futex_wait_error(wait_result,
                                                               errno);
                if (wait_error == EINTR) {
                    c = __atomic_exchange_n(&mutex->locked, 2,
                                            __ATOMIC_ACQUIRE);
                    continue;
                }
                if (wait_error != 0) return wait_error;
                c = __atomic_exchange_n(&mutex->locked, 2, __ATOMIC_ACQUIRE);
            }
        }
    }

    __rin_pthread_mutex_owner_store(mutex, owner);
    mutex->recursion = 1;
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t* mutex) {
    int saved_errno = errno;
    int result = rin_pthread_mutex_lock_impl(mutex);
    errno = saved_errno;
    return result;
}

static int rin_pthread_mutex_timedlock_impl(
    pthread_mutex_t* mutex, const struct timespec* abstime,
    clockid_t clock_id) {
    if (!mutex || !abstime) return EINVAL;
    if (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC)
        return EINVAL;
    if (abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000L)
        return EINVAL;

    pthread_t self = _RIN_PTHREAD_RUNTIME_SELF();
    pthread_t owner = rin_pthread_mutex_owner_token(mutex, self);
    int type = rin_pthread_mutex_type(mutex);

    if (rin_pthread_mutex_is_robust(mutex) ||
        rin_pthread_mutex_is_prio_inherit(mutex)) {
        return rin_pthread_mutex_robust_lock(mutex, owner, type, 0, abstime,
                                             clock_id);
    }

    if (type == PTHREAD_MUTEX_RECURSIVE &&
        __rin_pthread_mutex_owner_load(mutex) == owner) {
        if (mutex->recursion == __INT_MAX__) return EAGAIN;
        mutex->recursion++;
        return 0;
    }
    if (type == PTHREAD_MUTEX_ERRORCHECK &&
        __rin_pthread_mutex_owner_load(mutex) == owner)
        return EDEADLK;

    {
        int c = 0;
        if (!__atomic_compare_exchange_n(&mutex->locked, &c, 1, 0,
                                         __ATOMIC_ACQUIRE,
                                         __ATOMIC_RELAXED)) {
            if (c != 2)
                c = __atomic_exchange_n(&mutex->locked, 2, __ATOMIC_ACQUIRE);
            while (c != 0) {
                if (abstime->tv_sec < 0) {
                    int unlocked = 0;
                    if (__atomic_compare_exchange_n(
                            &mutex->locked, &unlocked, 2, 0,
                            __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
                        break;
                    return ETIMEDOUT;
                }
                long wait_result = _RIN_PTHREAD_RUNTIME_FUTEX_BITSET(
                    &mutex->locked,
                    rin_pthread_mutex_futex_operation(
                        mutex, FUTEX_WAIT_BITSET |
                            (clock_id == CLOCK_REALTIME
                                 ? FUTEX_CLOCK_REALTIME : 0)),
                    2, abstime, FUTEX_BITSET_MATCH_ANY);
                int wait_error = rin_pthread_futex_wait_error(wait_result,
                                                               errno);
                if (wait_error == EINTR) {
                    c = __atomic_exchange_n(&mutex->locked, 2,
                                            __ATOMIC_ACQUIRE);
                    continue;
                }
                if (wait_error == ETIMEDOUT) {
                    /* Resolve a concurrent unlock before returning timeout. */
                    c = __atomic_exchange_n(&mutex->locked, 2,
                                            __ATOMIC_ACQUIRE);
                    if (c != 0) return ETIMEDOUT;
                    break;
                }
                if (wait_error != 0) return wait_error;
                c = __atomic_exchange_n(&mutex->locked, 2, __ATOMIC_ACQUIRE);
            }
        }
    }

    __rin_pthread_mutex_owner_store(mutex, owner);
    mutex->recursion = 1;
    return 0;
}

int pthread_mutex_timedlock(pthread_mutex_t* mutex,
                            const struct timespec* abstime) {
    int saved_errno = errno;
    int result = rin_pthread_mutex_timedlock_impl(
        mutex, abstime, CLOCK_REALTIME);
    errno = saved_errno;
    return result;
}

int pthread_mutex_clocklock(pthread_mutex_t* mutex, clockid_t clock_id,
                            const struct timespec* abstime) {
    int saved_errno = errno;
    int result = rin_pthread_mutex_timedlock_impl(
        mutex, abstime, clock_id);
    errno = saved_errno;
    return result;
}

static int rin_pthread_mutex_trylock_impl(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;

    pthread_t self = _RIN_PTHREAD_RUNTIME_SELF();
    pthread_t owner = rin_pthread_mutex_owner_token(mutex, self);
    int type = rin_pthread_mutex_type(mutex);

    if (rin_pthread_mutex_is_robust(mutex) ||
        rin_pthread_mutex_is_prio_inherit(mutex)) {
        return rin_pthread_mutex_robust_lock(mutex, owner, type, 1, NULL,
                                             CLOCK_REALTIME);
    }

    if (type == PTHREAD_MUTEX_RECURSIVE &&
        __rin_pthread_mutex_owner_load(mutex) == owner) {
        if (mutex->recursion == __INT_MAX__) return EAGAIN;
        mutex->recursion++;
        return 0;
    }

    {
        int expected = 0;
        if (!__atomic_compare_exchange_n(&mutex->locked, &expected, 1, 0,
                                         __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            return EBUSY;
        }
    }

    __rin_pthread_mutex_owner_store(mutex, owner);
    mutex->recursion = 1;
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t* mutex) {
    int saved_errno = errno;
    int result = rin_pthread_mutex_trylock_impl(mutex);
    errno = saved_errno;
    return result;
}

static int rin_pthread_mutex_unlock_impl(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;

    pthread_t self = _RIN_PTHREAD_RUNTIME_SELF();
    pthread_t owner = rin_pthread_mutex_owner_token(mutex, self);
    int type = rin_pthread_mutex_type(mutex);

    if (rin_pthread_mutex_is_robust(mutex) ||
        rin_pthread_mutex_is_prio_inherit(mutex)) {
        uint32_t state;
        uint32_t previous;
        uint32_t owner_id;
        int wake_error = 0;
        if (__rin_pthread_mutex_owner_load(mutex) != owner)
            return EPERM;
        wake_error = rin_pthread_mutex_robust_owner_id(mutex, owner,
                                                       &owner_id);
        if (wake_error != 0) return wake_error;
        state = __atomic_load_n((uint32_t*)&mutex->locked, __ATOMIC_ACQUIRE);
        if ((state & RIN_PTHREAD_ROBUST_OWNER_MASK) != owner_id) {
            rin_pthread_mutex_robust_owner_release(mutex, owner);
            return EPERM;
        }
        if (type == PTHREAD_MUTEX_RECURSIVE) {
            if (mutex->recursion <= 0) return EPERM;
            if (mutex->recursion > 1) {
                mutex->recursion--;
                return 0;
            }
        }
        if (rin_pthread_mutex_is_robust(mutex) &&
            (state & RIN_PTHREAD_ROBUST_OWNER_DIED) != 0u)
            __atomic_fetch_or(&mutex->type,
                              RIN_PTHREAD_MUTEX_NOT_RECOVERABLE_FLAG,
                              __ATOMIC_RELEASE);
        mutex->recursion = 0;
        for (;;) {
            uint32_t unlocked_state;
            if ((state & RIN_PTHREAD_ROBUST_OWNER_MASK) != owner_id) {
                rin_pthread_mutex_robust_owner_release(mutex, owner);
                return EPERM;
            }
            /* Keep the waiter bit in the unlocked word when waking one
             * waiter.  The next owner then carries the bit forward and
             * wakes another waiter on its unlock.  Clearing it here can
             * strand every waiter except the one just woken. */
            unlocked_state = state & RIN_PTHREAD_ROBUST_WAITERS;
            previous = state;
            if (__atomic_compare_exchange_n((uint32_t*)&mutex->locked,
                                            &state, unlocked_state, 0,
                                            __ATOMIC_ACQ_REL,
                                            __ATOMIC_ACQUIRE))
                break;
        }
        {
            pthread_t expected_owner = owner;
            (void)__atomic_compare_exchange_n(&mutex->owner, &expected_owner,
                                              (pthread_t)0, 0,
                                              __ATOMIC_ACQ_REL,
                                              __ATOMIC_ACQUIRE);
        }
        if ((previous & RIN_PTHREAD_ROBUST_WAITERS) != 0u) {
            long wake_result = _RIN_PTHREAD_RUNTIME_FUTEX(
                &mutex->locked,
                rin_pthread_mutex_futex_operation(
                    mutex, FUTEX_WAKE),
                0x7fffffff,
                NULL);
            wake_error = rin_pthread_futex_wake_error(wake_result, errno);
        }
        rin_pthread_mutex_robust_owner_release(mutex, owner);
        return wake_error;
    }

    if ((type == PTHREAD_MUTEX_ERRORCHECK ||
         type == PTHREAD_MUTEX_RECURSIVE) &&
        __rin_pthread_mutex_owner_load(mutex) != owner) {
        return EPERM;
    }

    if (type == PTHREAD_MUTEX_RECURSIVE) {
        if (mutex->recursion <= 0) return EPERM;
        if (mutex->recursion > 1) {
            mutex->recursion--;
            return 0;
        }
    }

    __rin_pthread_mutex_owner_store(mutex, 0);
    mutex->recursion = 0;

    if (__atomic_exchange_n(&mutex->locked, 0, __ATOMIC_RELEASE) == 2) {
        long wake_result = _RIN_PTHREAD_RUNTIME_FUTEX(
            &mutex->locked,
            rin_pthread_mutex_futex_operation(mutex, FUTEX_WAKE), 1, NULL);
        int wake_error = rin_pthread_futex_wake_error(wake_result, errno);
        if (wake_error != 0) return wake_error;
    }

    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    int saved_errno = errno;
    int result = rin_pthread_mutex_unlock_impl(mutex);
    errno = saved_errno;
    return result;
}

static int rin_pthread_mutex_consistent_impl(pthread_mutex_t* mutex) {
    pthread_t self;
    pthread_t owner;
    uint32_t state;
    uint32_t owner_id;
    int result;
    if (!mutex) return EINVAL;
    if (!rin_pthread_mutex_is_robust(mutex))
        return EINVAL;
    self = _RIN_PTHREAD_RUNTIME_SELF();
    owner = rin_pthread_mutex_owner_token(mutex, self);
    if (__rin_pthread_mutex_owner_load(mutex) != owner)
        return EPERM;
    result = rin_pthread_mutex_robust_owner_id(mutex, owner, &owner_id);
    if (result != 0) return result;
    state = __atomic_load_n((uint32_t*)&mutex->locked, __ATOMIC_ACQUIRE);
    if ((state & RIN_PTHREAD_ROBUST_OWNER_MASK) != owner_id) {
        rin_pthread_mutex_robust_owner_release(mutex, owner);
        return EPERM;
    }
    if ((state & RIN_PTHREAD_ROBUST_OWNER_DIED) == 0u) return EINVAL;
    __atomic_fetch_and((uint32_t*)&mutex->locked,
                       ~RIN_PTHREAD_ROBUST_OWNER_DIED,
                       __ATOMIC_RELEASE);
    return 0;
}

int pthread_mutex_consistent(pthread_mutex_t* mutex) {
    int saved_errno = errno;
    int result = rin_pthread_mutex_consistent_impl(mutex);
    errno = saved_errno;
    return result;
}

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr) {
    if (!cond) return EINVAL;
    if (attr && attr->pshared != PTHREAD_PROCESS_PRIVATE &&
        attr->pshared != PTHREAD_PROCESS_SHARED)
        return EINVAL;
    cond->generation = 0;
    cond->waiting = 0;
    cond->clock_id = attr ? attr->clock_id : CLOCK_REALTIME;
    cond->pshared = attr ? attr->pshared : PTHREAD_PROCESS_PRIVATE;
    return 0;
}

int pthread_cond_destroy(pthread_cond_t* cond) {
    if (!cond) return EINVAL;
    if (cond->waiting > 0) return EBUSY;
    return 0;
}

static int rin_pthread_cond_wait_impl(pthread_cond_t* cond,
                                      pthread_mutex_t* mutex) {
    int result = 0;
    int mutex_result;

    if (!cond || !mutex) return EINVAL;
    pthread_testcancel();

    unsigned int gen = __atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE);
    __atomic_fetch_add(&cond->waiting, 1, __ATOMIC_SEQ_CST);
    mutex_result = pthread_mutex_unlock(mutex);
    if (mutex_result != 0) {
        __atomic_fetch_sub(&cond->waiting, 1, __ATOMIC_SEQ_CST);
        return mutex_result;
    }

    while (__atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE) == gen) {
        long wait_result = _RIN_PTHREAD_RUNTIME_FUTEX(
            &cond->generation,
            rin_pthread_cond_futex_operation(cond, FUTEX_WAIT),
            (int)gen, NULL);
        int signal_interrupted = wait_result == -1 && errno == EINTR;
        int wait_error = rin_pthread_futex_wait_error(wait_result, errno);
        if (rin_pthread_cancel_pending_enabled(_RIN_PTHREAD_RUNTIME_SELF())) {
            result = EINTR;
            break;
        }
        if (signal_interrupted) {
            ++g_rin_pthread_cond_signal_interrupt_epoch;
            result = EINTR;
            break;
        }
        if (wait_error != 0) {
            result = wait_error;
            break;
        }
    }

    __atomic_fetch_sub(&cond->waiting, 1, __ATOMIC_SEQ_CST);
    mutex_result = pthread_mutex_lock(mutex);
    if (mutex_result != 0) return mutex_result;
    if (result == EINTR) pthread_testcancel();
    return result == EINTR ? 0 : result;
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    int saved_errno = errno;
    int result = rin_pthread_cond_wait_impl(cond, mutex);
    errno = saved_errno;
    return result;
}

static int rin_pthread_cond_timedwait_impl(
    pthread_cond_t* cond, pthread_mutex_t* mutex,
    const struct timespec* abstime) {
    int mutex_result;

    if (!cond || !mutex || !abstime) return EINVAL;
    pthread_testcancel();
    if (abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000L)
        return EINVAL;
    if (cond->clock_id != CLOCK_REALTIME &&
        cond->clock_id != CLOCK_MONOTONIC)
        return EINVAL;

    unsigned int gen = __atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE);
    __atomic_fetch_add(&cond->waiting, 1, __ATOMIC_SEQ_CST);
    mutex_result = pthread_mutex_unlock(mutex);
    if (mutex_result != 0) {
        __atomic_fetch_sub(&cond->waiting, 1, __ATOMIC_SEQ_CST);
        return mutex_result;
    }

    int result = 0;
    while (__atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE) == gen) {
#if !defined(RIN_TIME_ABI64)
        if (abstime->tv_sec < 0) {
            result = ETIMEDOUT;
            break;
        }
#endif
        int operation = FUTEX_WAIT_BITSET |
            (cond->clock_id == CLOCK_REALTIME ? FUTEX_CLOCK_REALTIME : 0);
        struct timespec relative_timeout;
        const struct timespec* timeout = abstime;
        int use_relative_timeout = 0;
#if defined(RIN_TIME_ABI64)
        if (cond->clock_id == CLOCK_REALTIME) {
            int deadline_error = _rin_time_realtime_deadline_relative(
                abstime, &relative_timeout, &use_relative_timeout);
            if (deadline_error == ETIMEDOUT) {
                result = ETIMEDOUT;
                break;
            }
            if (deadline_error != 0) {
                result = deadline_error;
                break;
            }
            if (use_relative_timeout) {
                operation = FUTEX_WAIT;
                timeout = &relative_timeout;
            }
        }
#endif
        long ret = use_relative_timeout
            ? _RIN_PTHREAD_RUNTIME_FUTEX(
                  &cond->generation,
                  rin_pthread_cond_futex_operation(cond, operation),
                  (int)gen, timeout)
            : _RIN_PTHREAD_RUNTIME_FUTEX_BITSET(
                  &cond->generation,
                  rin_pthread_cond_futex_operation(cond, operation),
                  (int)gen, timeout, FUTEX_BITSET_MATCH_ANY);
        int signal_interrupted = ret == -1 && errno == EINTR;
        int wait_error = rin_pthread_futex_wait_error(ret, errno);
        if (wait_error == ETIMEDOUT) {
            if (__atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE) != gen)
                break;
            if (rin_pthread_cancel_pending_enabled(
                    _RIN_PTHREAD_RUNTIME_SELF())) {
                result = EINTR;
                break;
            }
            result = ETIMEDOUT;
            break;
        }
        if (rin_pthread_cancel_pending_enabled(_RIN_PTHREAD_RUNTIME_SELF())) {
            result = EINTR;
            break;
        }
        if (signal_interrupted) {
            ++g_rin_pthread_cond_signal_interrupt_epoch;
            result = EINTR;
            break;
        }
        if (wait_error != 0) {
            result = wait_error;
            break;
        }
    }

    __atomic_fetch_sub(&cond->waiting, 1, __ATOMIC_SEQ_CST);
    mutex_result = pthread_mutex_lock(mutex);
    if (mutex_result != 0) return mutex_result;
    if (result == EINTR) pthread_testcancel();
    return result == EINTR ? 0 : result;
}

int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex,
                           const struct timespec* abstime) {
    int saved_errno = errno;
    int result = rin_pthread_cond_timedwait_impl(cond, mutex, abstime);
    errno = saved_errno;
    return result;
}

int __rin_pthread_cond_wait_interruptible(pthread_cond_t* cond,
                                          pthread_mutex_t* mutex) {
    uint64_t before = g_rin_pthread_cond_signal_interrupt_epoch;
    int result = pthread_cond_wait(cond, mutex);
    if (result != 0) return result;
    return g_rin_pthread_cond_signal_interrupt_epoch != before ? EINTR : 0;
}

int __rin_pthread_cond_timedwait_interruptible(
    pthread_cond_t* cond, pthread_mutex_t* mutex,
    const struct timespec* abstime) {
    uint64_t before = g_rin_pthread_cond_signal_interrupt_epoch;
    int result = pthread_cond_timedwait(cond, mutex, abstime);
    if (result != 0) return result;
    return g_rin_pthread_cond_signal_interrupt_epoch != before ? EINTR : 0;
}

static int rin_pthread_cond_signal_impl(pthread_cond_t* cond) {
    if (!cond) return EINVAL;
    __atomic_fetch_add(&cond->generation, 1, __ATOMIC_SEQ_CST);
    if (__atomic_load_n(&cond->waiting, __ATOMIC_SEQ_CST) > 0) {
        long wake_result = _RIN_PTHREAD_RUNTIME_FUTEX(
            &cond->generation,
            rin_pthread_cond_futex_operation(cond, FUTEX_WAKE), 1, NULL);
        int wake_error = rin_pthread_futex_wake_error(wake_result, errno);
        if (wake_error != 0) return wake_error;
    }
    return 0;
}

int pthread_cond_signal(pthread_cond_t* cond) {
    int saved_errno = errno;
    int result = rin_pthread_cond_signal_impl(cond);
    errno = saved_errno;
    return result;
}

static int rin_pthread_cond_broadcast_impl(pthread_cond_t* cond) {
    if (!cond) return EINVAL;
    __atomic_fetch_add(&cond->generation, 1, __ATOMIC_SEQ_CST);
    if (__atomic_load_n(&cond->waiting, __ATOMIC_SEQ_CST) > 0) {
        long wake_result = _RIN_PTHREAD_RUNTIME_FUTEX(
            &cond->generation,
            rin_pthread_cond_futex_operation(cond, FUTEX_WAKE),
            0x7fffffff, NULL);
        int wake_error = rin_pthread_futex_wake_error(wake_result, errno);
        if (wake_error != 0) return wake_error;
    }
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
    int saved_errno = errno;
    int result = rin_pthread_cond_broadcast_impl(cond);
    errno = saved_errno;
    return result;
}

static void rin_pthread_once_cancel_reset(void* argument) {
    pthread_once_t* once_control = (pthread_once_t*)argument;
    if (once_control == NULL) return;
    __atomic_store_n(once_control, 0, __ATOMIC_RELEASE);
    (void)_RIN_PTHREAD_RUNTIME_FUTEX(
        once_control, FUTEX_WAKE_PRIVATE, 0x7fffffff, NULL);
}

static int rin_pthread_once_impl(pthread_once_t* once_control,
                                 void (*init_routine)(void)) {
    if (!once_control || !init_routine) return EINVAL;

retry_initialization:
    if (__sync_val_compare_and_swap(once_control, 0, 1) == 0) {
        pthread_cleanup_push(rin_pthread_once_cancel_reset, once_control);
        init_routine();
        pthread_cleanup_pop(0);
        __atomic_store_n(once_control, 2, __ATOMIC_RELEASE);
        {
            int saved_errno = errno;
            long wake_result = _RIN_PTHREAD_RUNTIME_FUTEX(
                once_control, FUTEX_WAKE_PRIVATE, 0x7fffffff, NULL);
            int wake_error = rin_pthread_futex_wake_error(wake_result, errno);
            errno = saved_errno;
            if (wake_error != 0) return wake_error;
        }
    } else {
        while (__atomic_load_n(once_control, __ATOMIC_ACQUIRE) == 1) {
            int saved_errno = errno;
            long wait_result = _RIN_PTHREAD_RUNTIME_FUTEX(
                once_control, FUTEX_WAIT_PRIVATE, 1, NULL);
            int wait_error = rin_pthread_futex_wait_error(wait_result, errno);
            errno = saved_errno;
            if (wait_error != 0) return wait_error;
        }
        /* Cancellation of the initializer resets the control word to zero.
         * A waiter that observes that reset must compete to run the routine;
         * treating zero as completed would let every waiter return early. */
        if (__atomic_load_n(once_control, __ATOMIC_ACQUIRE) == 0)
            goto retry_initialization;
    }
    return 0;
}

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void)) {
    int saved_errno = errno;
    int result = rin_pthread_once_impl(once_control, init_routine);
    errno = saved_errno;
    return result;
}

int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr) {
    int result;
    int pshared = attr ? attr->pshared : PTHREAD_PROCESS_PRIVATE;
    pthread_mutexattr_t mutex_attr;
    pthread_condattr_t cond_attr;
    if (!rwlock) return EINVAL;
    if (pshared != PTHREAD_PROCESS_PRIVATE &&
        pshared != PTHREAD_PROCESS_SHARED)
        return EINVAL;
    result = pthread_mutexattr_init(&mutex_attr);
    if (result != 0) return result;
    result = pthread_condattr_init(&cond_attr);
    if (result != 0) {
        (void)pthread_mutexattr_destroy(&mutex_attr);
        return result;
    }
    result = pthread_mutexattr_setpshared(&mutex_attr, pshared);
    if (result == 0)
        result = pthread_condattr_setpshared(&cond_attr, pshared);
    if (result != 0) {
        (void)pthread_condattr_destroy(&cond_attr);
        (void)pthread_mutexattr_destroy(&mutex_attr);
        return result;
    }
    rwlock->readers = 0;
    rwlock->writer = pshared == PTHREAD_PROCESS_SHARED
        ? RIN_PTHREAD_RWLOCK_PROCESS_SHARED : 0;
    rwlock->waiting_readers = 0;
    rwlock->waiting_writers = 0;
    result = pthread_mutex_init(&rwlock->mutex, &mutex_attr);
    if (result == 0) {
        result = pthread_cond_init(&rwlock->can_read, &cond_attr);
        if (result != 0) (void)pthread_mutex_destroy(&rwlock->mutex);
    }
    if (result == 0) {
        result = pthread_cond_init(&rwlock->can_write, &cond_attr);
        if (result != 0) {
            (void)pthread_cond_destroy(&rwlock->can_read);
            (void)pthread_mutex_destroy(&rwlock->mutex);
        }
    }
    (void)pthread_condattr_destroy(&cond_attr);
    (void)pthread_mutexattr_destroy(&mutex_attr);
    return result;
}

int pthread_rwlock_destroy(pthread_rwlock_t* rwlock) {
    if (!rwlock) return EINVAL;
    if (rwlock->readers > 0 || rin_pthread_rwlock_writer_active(rwlock) ||
        rwlock->waiting_readers > 0 || rwlock->waiting_writers > 0) {
        return EBUSY;
    }
    {
        int result = pthread_cond_destroy(&rwlock->can_read);
        if (result != 0) return result;
        result = pthread_cond_destroy(&rwlock->can_write);
        if (result != 0) return result;
        return pthread_mutex_destroy(&rwlock->mutex);
    }
}

static int rin_pthread_mutex_owned_by_current_thread(
    const pthread_mutex_t* mutex) {
    return mutex && __atomic_load_n(&mutex->locked, __ATOMIC_ACQUIRE) != 0 &&
           __rin_pthread_mutex_owner_load(mutex) ==
               _RIN_PTHREAD_RUNTIME_SELF();
}

static int rin_pthread_rwlock_notify_waiters_locked(
    pthread_rwlock_t* rwlock) {
    if (rwlock->waiting_writers > 0) {
        if (!rin_pthread_rwlock_writer_active(rwlock) &&
            rwlock->readers == 0)
            return pthread_cond_signal(&rwlock->can_write);
    } else if (rwlock->waiting_readers > 0) {
        return pthread_cond_broadcast(&rwlock->can_read);
    }
    return 0;
}

typedef struct {
    pthread_rwlock_t* rwlock;
    int writer;
} RinPthreadRwlockWaitCleanup;

/* pthread_cond_wait() reacquires rwlock->mutex before cancellation cleanup. */
static void rin_pthread_rwlock_wait_cancel_cleanup(void* argument) {
    RinPthreadRwlockWaitCleanup* cleanup =
        (RinPthreadRwlockWaitCleanup*)argument;
    pthread_rwlock_t* rwlock;
    if (!cleanup || !cleanup->rwlock) return;
    rwlock = cleanup->rwlock;
    if (cleanup->writer)
        --rwlock->waiting_writers;
    else
        --rwlock->waiting_readers;
    (void)rin_pthread_rwlock_notify_waiters_locked(rwlock);
    (void)pthread_mutex_unlock(&rwlock->mutex);
}

static void rin_pthread_rwlock_rollback_acquire(pthread_rwlock_t* rwlock,
                                                int writer) {
    if (pthread_mutex_lock(&rwlock->mutex) != 0) return;
    if (writer) {
        rwlock->writer &= ~RIN_PTHREAD_RWLOCK_WRITER_ACTIVE;
    } else if (rwlock->readers > 0) {
        rwlock->readers--;
    }
    (void)pthread_mutex_unlock(&rwlock->mutex);
}

int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock) {
    int result;
    RinPthreadRwlockWaitCleanup cleanup;
    if (!rwlock) return EINVAL;
    cleanup.rwlock = rwlock;
    cleanup.writer = 0;
    result = pthread_mutex_lock(&rwlock->mutex);
    if (result != 0) return result;
    while (rin_pthread_rwlock_writer_active(rwlock) ||
           rwlock->waiting_writers > 0) {
        rwlock->waiting_readers++;
        pthread_cleanup_push(rin_pthread_rwlock_wait_cancel_cleanup,
                             &cleanup);
        result = pthread_cond_wait(&rwlock->can_read, &rwlock->mutex);
        pthread_cleanup_pop(0);
        __atomic_fetch_sub(&rwlock->waiting_readers, 1, __ATOMIC_SEQ_CST);
        if (result != 0) {
            if (rin_pthread_mutex_owned_by_current_thread(&rwlock->mutex)) {
                int unlock_result = pthread_mutex_unlock(&rwlock->mutex);
                if (unlock_result != 0) return unlock_result;
            }
            return result;
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

int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock) {
    int result;
    RinPthreadRwlockWaitCleanup cleanup;
    if (!rwlock) return EINVAL;
    cleanup.rwlock = rwlock;
    cleanup.writer = 1;
    result = pthread_mutex_lock(&rwlock->mutex);
    if (result != 0) return result;
    rwlock->waiting_writers++;
    while (rin_pthread_rwlock_writer_active(rwlock) ||
           rwlock->readers > 0) {
        pthread_cleanup_push(rin_pthread_rwlock_wait_cancel_cleanup,
                             &cleanup);
        result = pthread_cond_wait(&rwlock->can_write, &rwlock->mutex);
        pthread_cleanup_pop(0);
        if (result != 0) {
            __atomic_fetch_sub(&rwlock->waiting_writers, 1,
                               __ATOMIC_SEQ_CST);
            if (rin_pthread_mutex_owned_by_current_thread(&rwlock->mutex)) {
                (void)rin_pthread_rwlock_notify_waiters_locked(rwlock);
                int unlock_result = pthread_mutex_unlock(&rwlock->mutex);
                if (unlock_result != 0) return unlock_result;
            }
            return result;
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

static int rin_pthread_rwlock_timedlock_impl(
    pthread_rwlock_t* rwlock, const struct timespec* abstime, int writer) {
    int result;
    int acquired = 0;
    int waiter_registered = 0;
    RinPthreadRwlockWaitCleanup cleanup;
    if (!rwlock || !abstime || abstime->tv_nsec < 0 ||
        abstime->tv_nsec >= 1000000000L)
        return EINVAL;

    result = pthread_mutex_timedlock(&rwlock->mutex, abstime);
    if (result != 0) return result;
    cleanup.rwlock = rwlock;
    cleanup.writer = writer;

    if (writer) {
        rwlock->waiting_writers++;
        waiter_registered = 1;
        while (rin_pthread_rwlock_writer_active(rwlock) ||
               rwlock->readers > 0) {
            pthread_cleanup_push(rin_pthread_rwlock_wait_cancel_cleanup,
                                 &cleanup);
            result = pthread_cond_timedwait(&rwlock->can_write,
                                            &rwlock->mutex, abstime);
            pthread_cleanup_pop(0);
            if (result != 0) {
                --rwlock->waiting_writers;
                waiter_registered = 0;
                if (rin_pthread_mutex_owned_by_current_thread(
                        &rwlock->mutex)) {
                    if (result == ETIMEDOUT &&
                        !rin_pthread_rwlock_writer_active(rwlock) &&
                        rwlock->readers == 0 &&
                        rwlock->waiting_writers == 0) {
                        rwlock->writer |=
                            RIN_PTHREAD_RWLOCK_WRITER_ACTIVE;
                        acquired = 1;
                        result = 0;
                        break;
                    }
                    {
                        int notify_result =
                            rin_pthread_rwlock_notify_waiters_locked(rwlock);
                        int unlock_result =
                            pthread_mutex_unlock(&rwlock->mutex);
                        if (notify_result != 0) return notify_result;
                        if (unlock_result != 0) return unlock_result;
                    }
                }
                return result;
            }
        }
        if (waiter_registered) --rwlock->waiting_writers;
        if (!acquired) {
            if (!rin_pthread_rwlock_writer_active(rwlock) &&
                rwlock->readers == 0) {
                rwlock->writer |= RIN_PTHREAD_RWLOCK_WRITER_ACTIVE;
            } else {
                (void)rin_pthread_rwlock_notify_waiters_locked(rwlock);
                (void)pthread_mutex_unlock(&rwlock->mutex);
                return EBUSY;
            }
        }
    } else {
        while (rin_pthread_rwlock_writer_active(rwlock) ||
               rwlock->waiting_writers > 0) {
            rwlock->waiting_readers++;
            pthread_cleanup_push(rin_pthread_rwlock_wait_cancel_cleanup,
                                 &cleanup);
            result = pthread_cond_timedwait(&rwlock->can_read,
                                            &rwlock->mutex, abstime);
            pthread_cleanup_pop(0);
            __atomic_fetch_sub(&rwlock->waiting_readers, 1,
                               __ATOMIC_SEQ_CST);
            if (result != 0) {
                if (result == ETIMEDOUT &&
                    !rin_pthread_rwlock_writer_active(rwlock) &&
                    rwlock->waiting_writers == 0) {
                    rwlock->readers++;
                    acquired = 1;
                    result = 0;
                    break;
                }
                if (rin_pthread_mutex_owned_by_current_thread(
                        &rwlock->mutex)) {
                    int unlock_result =
                        pthread_mutex_unlock(&rwlock->mutex);
                    if (unlock_result != 0) return unlock_result;
                }
                return result;
            }
        }
        if (!acquired) {
            if (!rin_pthread_rwlock_writer_active(rwlock) &&
                rwlock->waiting_writers == 0) {
                rwlock->readers++;
            } else {
                (void)pthread_mutex_unlock(&rwlock->mutex);
                return EBUSY;
            }
        }
    }

    result = pthread_mutex_unlock(&rwlock->mutex);
    if (result != 0) {
        rin_pthread_rwlock_rollback_acquire(rwlock, writer);
        return result;
    }
    return 0;
}

int pthread_rwlock_timedrdlock(pthread_rwlock_t* rwlock,
                               const struct timespec* abstime) {
    int saved_errno = errno;
    int result = rin_pthread_rwlock_timedlock_impl(rwlock, abstime, 0);
    errno = saved_errno;
    return result;
}

int pthread_rwlock_timedwrlock(pthread_rwlock_t* rwlock,
                               const struct timespec* abstime) {
    int saved_errno = errno;
    int result = rin_pthread_rwlock_timedlock_impl(rwlock, abstime, 1);
    errno = saved_errno;
    return result;
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock) {
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

int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock) {
    int result;
    if (!rwlock) return EINVAL;
    result = pthread_mutex_trylock(&rwlock->mutex);
    if (result != 0) return result;
    if (rin_pthread_rwlock_writer_active(rwlock) ||
        rwlock->readers > 0) {
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

int pthread_rwlock_unlock(pthread_rwlock_t* rwlock) {
    int notify_result;
    int unlock_result;
    if (!rwlock) return EINVAL;
    unlock_result = pthread_mutex_lock(&rwlock->mutex);
    if (unlock_result != 0) return unlock_result;
    if (rin_pthread_rwlock_writer_active(rwlock)) {
        rwlock->writer &= ~RIN_PTHREAD_RWLOCK_WRITER_ACTIVE;
    } else if (rwlock->readers > 0) {
        rwlock->readers--;
    } else {
        unlock_result = pthread_mutex_unlock(&rwlock->mutex);
        return unlock_result != 0 ? unlock_result : EPERM;
    }

    notify_result = rin_pthread_rwlock_notify_waiters_locked(rwlock);
    unlock_result = pthread_mutex_unlock(&rwlock->mutex);
    return notify_result != 0 ? notify_result : unlock_result;
}

/* 初期化印の後ろに12 bitのnext/serving ticketを持ち、待機数を上限化する。 */
#define RIN_PTHREAD_SPIN_SIGNATURE UINT32_C(0xa5000000)
#define RIN_PTHREAD_SPIN_SIGNATURE_MASK UINT32_C(0xff000000)
#define RIN_PTHREAD_SPIN_NEXT_SHIFT 12u
#define RIN_PTHREAD_SPIN_TICKET_MASK UINT32_C(0x00000fff)
#define RIN_PTHREAD_SPIN_NEXT_MASK UINT32_C(0x00fff000)
#define RIN_PTHREAD_SPIN_YIELD_INTERVAL 4096u

static uint32_t rin_pthread_spin_next(uint32_t state)
{
    return (state >> RIN_PTHREAD_SPIN_NEXT_SHIFT) &
        RIN_PTHREAD_SPIN_TICKET_MASK;
}

static uint32_t rin_pthread_spin_serving(uint32_t state)
{
    return state & RIN_PTHREAD_SPIN_TICKET_MASK;
}

static uint32_t rin_pthread_spin_outstanding(uint32_t state)
{
    return (rin_pthread_spin_next(state) -
            rin_pthread_spin_serving(state)) &
        RIN_PTHREAD_SPIN_TICKET_MASK;
}

static int rin_pthread_spin_state_valid(uint32_t state)
{
    return (state & RIN_PTHREAD_SPIN_SIGNATURE_MASK) ==
        RIN_PTHREAD_SPIN_SIGNATURE;
}

static void rin_pthread_spin_relax(void)
{
#if defined(__i386__) || defined(__x86_64__)
    __asm__ volatile("pause" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
}

static int rin_pthread_spin_init_impl(pthread_spinlock_t* lock, int pshared)
{
    if (lock == NULL || (pshared != PTHREAD_PROCESS_PRIVATE &&
                         pshared != PTHREAD_PROCESS_SHARED))
        return EINVAL;
    /* Process-shared locks use the same lock-free word in shared mappings. */
    __atomic_store_n(&lock->state, RIN_PTHREAD_SPIN_SIGNATURE,
                     __ATOMIC_RELEASE);
    return 0;
}

int pthread_spin_init(pthread_spinlock_t* lock, int pshared)
{
    int saved_errno = errno;
    int result = rin_pthread_spin_init_impl(lock, pshared);
    errno = saved_errno;
    return result;
}

static int rin_pthread_spin_destroy_impl(pthread_spinlock_t* lock)
{
    uint32_t state;
    if (lock == NULL) return EINVAL;
    state = __atomic_load_n(&lock->state, __ATOMIC_ACQUIRE);
    for (;;) {
        if (!rin_pthread_spin_state_valid(state)) return EINVAL;
        if (rin_pthread_spin_outstanding(state) != 0u) return EBUSY;
        if (__atomic_compare_exchange_n(&lock->state, &state, 0u, 0,
                                        __ATOMIC_ACQ_REL,
                                        __ATOMIC_ACQUIRE))
            return 0;
    }
}

int pthread_spin_destroy(pthread_spinlock_t* lock)
{
    int saved_errno = errno;
    int result = rin_pthread_spin_destroy_impl(lock);
    errno = saved_errno;
    return result;
}

static int rin_pthread_spin_trylock_impl(pthread_spinlock_t* lock)
{
    uint32_t state;
    if (lock == NULL) return EINVAL;
    state = __atomic_load_n(&lock->state, __ATOMIC_ACQUIRE);
    for (;;) {
        uint32_t next;
        if (!rin_pthread_spin_state_valid(state)) return EINVAL;
        if (rin_pthread_spin_outstanding(state) != 0u) return EBUSY;
        next = (rin_pthread_spin_next(state) + 1u) &
            RIN_PTHREAD_SPIN_TICKET_MASK;
        if (__atomic_compare_exchange_n(
                &lock->state, &state,
                (state & ~RIN_PTHREAD_SPIN_NEXT_MASK) |
                    (next << RIN_PTHREAD_SPIN_NEXT_SHIFT),
                0, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE))
            return 0;
    }
}

int pthread_spin_trylock(pthread_spinlock_t* lock)
{
    int saved_errno = errno;
    int result = rin_pthread_spin_trylock_impl(lock);
    errno = saved_errno;
    return result;
}

static int rin_pthread_spin_lock_impl(pthread_spinlock_t* lock)
{
    uint32_t state;
    uint32_t ticket;
    uint32_t spins = 0u;
    if (lock == NULL) return EINVAL;
    state = __atomic_load_n(&lock->state, __ATOMIC_ACQUIRE);
    for (;;) {
        uint32_t next;
        uint32_t outstanding;
        if (!rin_pthread_spin_state_valid(state)) return EINVAL;
        outstanding = rin_pthread_spin_outstanding(state);
        if (outstanding == RIN_PTHREAD_SPIN_TICKET_MASK) return EAGAIN;
        ticket = rin_pthread_spin_next(state);
        next = (ticket + 1u) & RIN_PTHREAD_SPIN_TICKET_MASK;
        if (__atomic_compare_exchange_n(
                &lock->state, &state,
                (state & ~RIN_PTHREAD_SPIN_NEXT_MASK) |
                    (next << RIN_PTHREAD_SPIN_NEXT_SHIFT),
                0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
            break;
    }

    for (;;) {
        if (!rin_pthread_spin_state_valid(state)) return EINVAL;
        if (rin_pthread_spin_serving(state) == ticket) return 0;
        if (++spins >= RIN_PTHREAD_SPIN_YIELD_INTERVAL) {
            (void)sched_yield();
            spins = 0u;
        } else {
            rin_pthread_spin_relax();
        }
        state = __atomic_load_n(&lock->state, __ATOMIC_ACQUIRE);
    }
}

int pthread_spin_lock(pthread_spinlock_t* lock)
{
    int saved_errno = errno;
    int result = rin_pthread_spin_lock_impl(lock);
    errno = saved_errno;
    return result;
}

static int rin_pthread_spin_unlock_impl(pthread_spinlock_t* lock)
{
    uint32_t state;
    if (lock == NULL) return EINVAL;
    state = __atomic_load_n(&lock->state, __ATOMIC_ACQUIRE);
    for (;;) {
        uint32_t serving;
        if (!rin_pthread_spin_state_valid(state) ||
            rin_pthread_spin_outstanding(state) == 0u)
            return EINVAL;
        serving = (rin_pthread_spin_serving(state) + 1u) &
            RIN_PTHREAD_SPIN_TICKET_MASK;
        if (__atomic_compare_exchange_n(
                &lock->state, &state,
                (state & ~RIN_PTHREAD_SPIN_TICKET_MASK) | serving, 0,
                __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
            return 0;
    }
}

int pthread_spin_unlock(pthread_spinlock_t* lock)
{
    int saved_errno = errno;
    int result = rin_pthread_spin_unlock_impl(lock);
    errno = saved_errno;
    return result;
}
