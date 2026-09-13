/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - pthread runtime implementation
 * libcがpthread ABIの所有者となる実装。
 */

#define RIN_PTHREAD_RUNTIME_IMPLEMENTATION 1
#include "pthread.h"
#include "rin_pthread_futex_policy.h"
#include "rin_thread_detach_policy.h"
#include "rin_thread_stack_policy.h"
#include "rin_thread_timeout_policy.h"
#include "stdlib.h"
#include "string.h"

#define CLONE_THREAD    0x00010000UL
#define CLONE_VM        0x00000100UL
#define CLONE_FS        0x00000200UL
#define CLONE_FILES     0x00000400UL
#define CLONE_SIGHAND   0x00000800UL

#define THREAD_STACK_DEFAULT (8UL * 1024UL * 1024UL)
#define THREAD_STACK_MIN     (2UL * 1024UL * 1024UL)
#define THREAD_STACK_GUARD   0x1000UL
#define THREAD_PAGE_SIZE     0x1000UL

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

#ifndef _RIN_PTHREAD_RUNTIME_FUTEX
#define _RIN_PTHREAD_RUNTIME_FUTEX(address, operation, value, timeout) \
    syscall(SYS_futex, address, operation, value, timeout, NULL, 0)
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

typedef struct {
    pthread_t tid;
    char name[16];
} rin_thread_name_rec_t;

enum {
    RIN_THREAD_ALLOC_NONE = 0,
    RIN_THREAD_ALLOC_MALLOC = 1,
    RIN_THREAD_ALLOC_MMAP = 2
};

#define RIN_THREAD_STACK_REC_MAX 256
static rin_thread_stack_rec_t g_thread_stack_recs[RIN_THREAD_STACK_REC_MAX];
static volatile unsigned int g_thread_stack_lock = 0;
#define RIN_THREAD_NAME_REC_MAX (RIN_THREAD_STACK_REC_MAX + 1u)
static rin_thread_name_rec_t g_thread_name_recs[RIN_THREAD_NAME_REC_MAX];

/* pthread_tls.c で定義 */
extern void __pthread_run_destructors(void);
/* rin_atexit_registry.inc で定義 */
extern void __rin_cxa_thread_finalize(void);
void rin_log(const char* msg);

typedef struct {
    void* (*start_routine)(void*);
    void* arg;
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

static rin_thread_name_rec_t* rin_thread_name_find_locked(pthread_t tid) {
    for (unsigned int i = 0; i < RIN_THREAD_NAME_REC_MAX; i++) {
        if (g_thread_name_recs[i].tid == tid) {
            return &g_thread_name_recs[i];
        }
    }
    return NULL;
}

static rin_thread_name_rec_t* rin_thread_name_find_free_locked(void) {
    for (unsigned int i = 0; i < RIN_THREAD_NAME_REC_MAX; i++) {
        if (g_thread_name_recs[i].tid == 0) {
            return &g_thread_name_recs[i];
        }
    }
    return NULL;
}

static void rin_thread_name_clear_locked(pthread_t tid) {
    rin_thread_name_rec_t* record = rin_thread_name_find_locked(tid);
    if (record) {
        for (unsigned int i = 0; i < sizeof(record->name); i++) {
            record->name[i] = '\0';
        }
        record->tid = 0;
    }
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

static size_t rin_thread_name_length(const char name[16]) {
    size_t length;

    for (length = 0; length < 16u; ++length) {
        if (name[length] == '\0') return length;
    }
    return 16u;
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
        rin_thread_name_clear_locked(tid);
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

int pthread_setname_np(pthread_t thread, const char* name) {
    char candidate[16];
    rin_thread_name_rec_t* record;
    pthread_t current_thread = (pthread_t)_RIN_PTHREAD_RUNTIME_SELF();
    int result;

    if (thread == 0) return ESRCH;
    result = rin_thread_name_copy(candidate, name);
    if (result != 0) return result;

    rin_thread_stack_lock_acquire();
    if (thread != current_thread && !rin_thread_stack_find_locked(thread)) {
        rin_thread_stack_lock_release();
        return ESRCH;
    }
    record = rin_thread_name_find_locked(thread);
    if (!record) {
        record = rin_thread_name_find_free_locked();
        if (!record) {
            rin_thread_stack_lock_release();
            return EAGAIN;
        }
        record->tid = thread;
    }
    for (unsigned int index = 0; index < sizeof(record->name); ++index) {
        record->name[index] = candidate[index];
        if (candidate[index] == '\0') break;
    }
    rin_thread_stack_lock_release();
    return 0;
}

int pthread_getname_np(pthread_t thread, char* name, size_t length) {
    rin_thread_name_rec_t* record;
    pthread_t current_thread = (pthread_t)_RIN_PTHREAD_RUNTIME_SELF();
    size_t required_length = 1u;

    if (!name) return EINVAL;
    if (length == 0u) return ERANGE;
    if (thread == 0) return ESRCH;

    rin_thread_stack_lock_acquire();
    record = rin_thread_name_find_locked(thread);
    if (!record && thread != current_thread && !rin_thread_stack_find_locked(thread)) {
        rin_thread_stack_lock_release();
        return ESRCH;
    }
    if (record) {
        size_t name_length = rin_thread_name_length(record->name);
        if (name_length == sizeof(record->name)) {
            rin_thread_stack_lock_release();
            return EIO;
        }
        required_length += name_length;
    }
    if (length < required_length) {
        rin_thread_stack_lock_release();
        return ERANGE;
    }
    if (record) {
        for (size_t index = 0; index < required_length; ++index) {
            name[index] = record->name[index];
        }
    } else {
        name[0] = '\0';
    }
    rin_thread_stack_lock_release();
    return 0;
}

void __rin_pthread_run_exit_destructors(void) {
    /* Itanium C++ ABI thread_local objects precede POSIX TSD cleanup. */
    __rin_cxa_thread_finalize();
    __pthread_run_destructors();
}

/* スレッド終了ラッパー: デストラクタ反復 → SYS_THREAD_EXIT */
void pthread_exit(void* retval) {
    rin_pthread_cancel_exit(_RIN_PTHREAD_RUNTIME_SELF(), retval);
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

static void __thread_exit_wrapper(void);

/* start_routine が return した時に __thread_exit_wrapper を呼ぶ */
static void __attribute__((naked, used)) __thread_exit_trampoline(void) {
    __asm__ volatile(
        "call __thread_exit_wrapper\n"
        "ud2\n"
    );
}

static void* __attribute__((used)) __pthread_thread_bootstrap(void* raw_arg) {
    rin_thread_start_record_t record;
    rin_thread_start_record_t* start = (rin_thread_start_record_t*)raw_arg;
    void* result = NULL;

    memset(&record, 0, sizeof(record));
    if (start) {
        record = *start;
    }

    (void)rin_pthread_cancel_register(_RIN_PTHREAD_RUNTIME_SELF());

    if (record.start_routine) {
        result = record.start_routine(record.arg);
    }

    (void)result;
    __thread_exit_wrapper();
    __builtin_unreachable();
}

static void __attribute__((used)) __thread_exit_wrapper(void) {
    pthread_exit(NULL);
}

int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg) {
    void* stack_usable_base = NULL;
    uintptr_t stack_usable_size = 0;
    uintptr_t stack_guard_size = 0;
    void* alloc_base = NULL;
    uintptr_t alloc_size = 0;
    int alloc_kind = RIN_THREAD_ALLOC_NONE;
    uintptr_t requested_stack = THREAD_STACK_DEFAULT;

    if (!thread || !start_routine) return EINVAL;

    if (attr && attr->stacksize > 0) {
        requested_stack = (uintptr_t)attr->stacksize;
    }
    if (requested_stack < THREAD_STACK_MIN) {
        requested_stack = THREAD_STACK_MIN;
    }
    if (requested_stack > (UINTPTR_MAX - (THREAD_PAGE_SIZE - 1UL))) {
        return EINVAL;
    }
    requested_stack = rin_align_up_word(requested_stack, THREAD_PAGE_SIZE);

    if (attr && attr->stackaddr != NULL) {
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
        if (requested_stack > (UINTPTR_MAX - THREAD_STACK_GUARD)) {
            return EINVAL;
        }
        total_map = requested_stack + THREAD_STACK_GUARD;
        intptr_t map_result = _RIN_PTHREAD_RUNTIME_MMAP(
            0, total_map, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK);
        if (map_result >= 0 && map_result != 0) {
            void* map_base = (void*)(uintptr_t)map_result;
            if (_RIN_PTHREAD_RUNTIME_MPROTECT(
                    (uintptr_t)((unsigned char*)map_base + THREAD_STACK_GUARD),
                    requested_stack, PROT_READ | PROT_WRITE) >= 0) {
                alloc_base = map_base;
                alloc_size = total_map;
                alloc_kind = RIN_THREAD_ALLOC_MMAP;
                stack_usable_base = (void*)((unsigned char*)map_base + THREAD_STACK_GUARD);
                stack_usable_size = requested_stack;
                stack_guard_size = THREAD_STACK_GUARD;

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
                rin_log("[PTHREAD] stack mprotect failed\n");
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

    if (!(attr && attr->stackaddr != NULL)) {
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

        *thread = (pthread_t)ret;
        (void)rin_pthread_cancel_register(*thread);
        if (alloc_kind != RIN_THREAD_ALLOC_NONE) {
            rin_thread_stack_register(*thread, alloc_base, alloc_size,
                                      alloc_kind, 0);
        }
        if (attr && attr->detachstate == PTHREAD_CREATE_DETACHED) {
            intptr_t detach_result = _RIN_PTHREAD_RUNTIME_DETACH(*thread);
            enum rin_thread_detach_outcome detach_outcome =
                rin_thread_detach_classify(detach_result);
            if (detach_outcome == RIN_THREAD_DETACH_INVALID) {
                return rin_pthread_syscall_error(detach_result);
            }

            /* MAP_STACK is a kernel-owned VMA.  Detach transfers its
             * lifetime to the kernel, including the already-terminated
             * (REAPED) case.  The user-side record is bookkeeping only; if
             * it is retained until thread exit, every detached worker leaks a
             * record and eventually prevents further pthread_create calls. */
            rin_pthread_cancel_set_detached(*thread);
            (void)detach_outcome;
            (void)rin_thread_stack_take(*thread, NULL, NULL, NULL, NULL);
        }
        return 0;
    }
}

int pthread_join(pthread_t thread, void** retval) {
    int detached = 0;
    if (retval) *retval = NULL;

    rin_thread_stack_lock_acquire();
    rin_thread_stack_rec_t* rec = rin_thread_stack_find_locked(thread);
    if (rec) detached = rec->detached;
    rin_thread_stack_lock_release();

    if (detached) {
        return EINVAL;
    }

    {
        intptr_t ret = _RIN_PTHREAD_RUNTIME_JOIN(thread);
        if (ret != 0) {
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

int pthread_detach(pthread_t thread) {
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

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) {
    if (!mutex) return EINVAL;
    if (attr) {
        if (attr->type != PTHREAD_MUTEX_NORMAL &&
            attr->type != PTHREAD_MUTEX_RECURSIVE &&
            attr->type != PTHREAD_MUTEX_ERRORCHECK) return EINVAL;
        if (attr->pshared == PTHREAD_PROCESS_SHARED) return ENOSYS;
        if (attr->pshared != PTHREAD_PROCESS_PRIVATE) return EINVAL;
    }
    mutex->locked = 0;
    mutex->owner = 0;
    mutex->type = attr ? attr->type : PTHREAD_MUTEX_DEFAULT;
    mutex->recursion = 0;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;
    if (mutex->locked) return EBUSY;
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;

    pthread_t self = _RIN_PTHREAD_RUNTIME_SELF();

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE && mutex->owner == self) {
        if (mutex->recursion == __INT_MAX__) return EAGAIN;
        mutex->recursion++;
        return 0;
    }

    if (mutex->type == PTHREAD_MUTEX_ERRORCHECK && mutex->owner == self) {
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
                    &mutex->locked, FUTEX_WAIT, 2, NULL);
                int wait_error = rin_pthread_futex_wait_error(wait_result,
                                                               errno);
                if (wait_error != 0) return wait_error;
                c = __atomic_exchange_n(&mutex->locked, 2, __ATOMIC_ACQUIRE);
            }
        }
    }

    mutex->owner = self;
    mutex->recursion = 1;
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;

    pthread_t self = _RIN_PTHREAD_RUNTIME_SELF();

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE && mutex->owner == self) {
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

    mutex->owner = self;
    mutex->recursion = 1;
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;

    pthread_t self = _RIN_PTHREAD_RUNTIME_SELF();

    if ((mutex->type == PTHREAD_MUTEX_ERRORCHECK ||
         mutex->type == PTHREAD_MUTEX_RECURSIVE) && mutex->owner != self) {
        return EPERM;
    }

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE) {
        if (mutex->recursion <= 0) return EPERM;
        if (mutex->recursion > 1) {
            mutex->recursion--;
            return 0;
        }
    }

    mutex->owner = 0;
    mutex->recursion = 0;

    if (__atomic_exchange_n(&mutex->locked, 0, __ATOMIC_RELEASE) == 2) {
        long wake_result = _RIN_PTHREAD_RUNTIME_FUTEX(
            &mutex->locked, FUTEX_WAKE, 1, NULL);
        int wake_error = rin_pthread_futex_wake_error(wake_result, errno);
        if (wake_error != 0) return wake_error;
    }

    return 0;
}

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr) {
    if (!cond) return EINVAL;
    cond->generation = 0;
    cond->waiting = 0;
    cond->clock_id = attr ? attr->clock_id : CLOCK_REALTIME;
    return 0;
}

int pthread_cond_destroy(pthread_cond_t* cond) {
    if (!cond) return EINVAL;
    if (cond->waiting > 0) return EBUSY;
    return 0;
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    int result = 0;
    int mutex_result;

    if (!cond || !mutex) return EINVAL;

    unsigned int gen = __atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE);
    __atomic_fetch_add(&cond->waiting, 1, __ATOMIC_SEQ_CST);
    mutex_result = pthread_mutex_unlock(mutex);
    if (mutex_result != 0) {
        __atomic_fetch_sub(&cond->waiting, 1, __ATOMIC_SEQ_CST);
        return mutex_result;
    }

    while (__atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE) == gen) {
        long wait_result = _RIN_PTHREAD_RUNTIME_FUTEX(
            &cond->generation, FUTEX_WAIT, (int)gen, NULL);
        int wait_error = rin_pthread_futex_wait_error(wait_result, errno);
        if (wait_error != 0) {
            result = wait_error;
            break;
        }
    }

    __atomic_fetch_sub(&cond->waiting, 1, __ATOMIC_SEQ_CST);
    mutex_result = pthread_mutex_lock(mutex);
    return mutex_result != 0 ? mutex_result : result;
}

int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex,
                           const struct timespec* abstime) {
    int mutex_result;

    if (!cond || !mutex || !abstime) return EINVAL;
    if (abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000L)
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
        struct timespec now;
        struct rin_thread_relative_timeout relative;
        enum rin_thread_timeout_outcome timeout_outcome;

        if (clock_gettime(cond->clock_id, &now) != 0) {
            result = errno ? errno : EIO;
            break;
        }
        timeout_outcome = rin_thread_relative_timeout(
            (long)abstime->tv_sec, abstime->tv_nsec,
            (long)now.tv_sec, now.tv_nsec, __LONG_MAX__, &relative);
        if (timeout_outcome == RIN_THREAD_TIMEOUT_INVALID) {
            result = EIO;
            break;
        }
        if (timeout_outcome == RIN_THREAD_TIMEOUT_EXPIRED) {
            result = ETIMEDOUT;
            break;
        }

        struct timespec ts;
        ts.tv_sec = (time_t)relative.seconds;
        ts.tv_nsec = relative.nanoseconds;

        long ret = _RIN_PTHREAD_RUNTIME_FUTEX(
            &cond->generation, FUTEX_WAIT, (int)gen, &ts);
        if (ret == -1 && errno == ETIMEDOUT) {
            if (__atomic_load_n(&cond->generation, __ATOMIC_ACQUIRE) != gen)
                result = 0;
            else
                result = ETIMEDOUT;
            break;
        }
        {
            int wait_error = rin_pthread_futex_wait_error(ret, errno);
            if (wait_error != 0) {
                result = wait_error;
                break;
            }
        }
    }

    __atomic_fetch_sub(&cond->waiting, 1, __ATOMIC_SEQ_CST);
    mutex_result = pthread_mutex_lock(mutex);
    return mutex_result != 0 ? mutex_result : result;
}

int pthread_cond_signal(pthread_cond_t* cond) {
    if (!cond) return EINVAL;
    __atomic_fetch_add(&cond->generation, 1, __ATOMIC_SEQ_CST);
    if (__atomic_load_n(&cond->waiting, __ATOMIC_SEQ_CST) > 0) {
        long wake_result = _RIN_PTHREAD_RUNTIME_FUTEX(
            &cond->generation, FUTEX_WAKE, 1, NULL);
        int wake_error = rin_pthread_futex_wake_error(wake_result, errno);
        if (wake_error != 0) return wake_error;
    }
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
    if (!cond) return EINVAL;
    __atomic_fetch_add(&cond->generation, 1, __ATOMIC_SEQ_CST);
    if (__atomic_load_n(&cond->waiting, __ATOMIC_SEQ_CST) > 0) {
        long wake_result = _RIN_PTHREAD_RUNTIME_FUTEX(
            &cond->generation, FUTEX_WAKE, 0x7fffffff, NULL);
        int wake_error = rin_pthread_futex_wake_error(wake_result, errno);
        if (wake_error != 0) return wake_error;
    }
    return 0;
}

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void)) {
    if (!once_control || !init_routine) return EINVAL;

    if (__sync_val_compare_and_swap(once_control, 0, 1) == 0) {
        init_routine();
        __atomic_store_n(once_control, 2, __ATOMIC_RELEASE);
        {
            long wake_result = _RIN_PTHREAD_RUNTIME_FUTEX(
                once_control, FUTEX_WAKE, 0x7fffffff, NULL);
            int wake_error = rin_pthread_futex_wake_error(wake_result, errno);
            if (wake_error != 0) return wake_error;
        }
    } else {
        while (__atomic_load_n(once_control, __ATOMIC_ACQUIRE) == 1) {
            long wait_result = _RIN_PTHREAD_RUNTIME_FUTEX(
                once_control, FUTEX_WAIT, 1, NULL);
            int wait_error = rin_pthread_futex_wait_error(wait_result, errno);
            if (wait_error != 0) return wait_error;
        }
    }
    return 0;
}

int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr) {
    int result;
    (void)attr;
    if (!rwlock) return EINVAL;
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

int pthread_rwlock_destroy(pthread_rwlock_t* rwlock) {
    if (!rwlock) return EINVAL;
    if (rwlock->readers > 0 || rwlock->writer ||
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
           mutex->owner == _RIN_PTHREAD_RUNTIME_SELF();
}

static void rin_pthread_rwlock_rollback_acquire(pthread_rwlock_t* rwlock,
                                                int writer) {
    if (pthread_mutex_lock(&rwlock->mutex) != 0) return;
    if (writer) {
        rwlock->writer = 0;
    } else if (rwlock->readers > 0) {
        rwlock->readers--;
    }
    (void)pthread_mutex_unlock(&rwlock->mutex);
}

int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock) {
    int result;
    if (!rwlock) return EINVAL;
    result = pthread_mutex_lock(&rwlock->mutex);
    if (result != 0) return result;
    while (rwlock->writer || rwlock->waiting_writers > 0) {
        rwlock->waiting_readers++;
        result = pthread_cond_wait(&rwlock->can_read, &rwlock->mutex);
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
    if (!rwlock) return EINVAL;
    result = pthread_mutex_lock(&rwlock->mutex);
    if (result != 0) return result;
    rwlock->waiting_writers++;
    while (rwlock->writer || rwlock->readers > 0) {
        result = pthread_cond_wait(&rwlock->can_write, &rwlock->mutex);
        if (result != 0) {
            __atomic_fetch_sub(&rwlock->waiting_writers, 1,
                               __ATOMIC_SEQ_CST);
            if (rin_pthread_mutex_owned_by_current_thread(&rwlock->mutex)) {
                int unlock_result = pthread_mutex_unlock(&rwlock->mutex);
                if (unlock_result != 0) return unlock_result;
            }
            return result;
        }
    }
    rwlock->waiting_writers--;
    rwlock->writer = 1;
    result = pthread_mutex_unlock(&rwlock->mutex);
    if (result != 0) {
        rin_pthread_rwlock_rollback_acquire(rwlock, 1);
        return result;
    }
    return 0;
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock) {
    int result;
    if (!rwlock) return EINVAL;
    result = pthread_mutex_trylock(&rwlock->mutex);
    if (result != 0) return result;
    if (rwlock->writer || rwlock->waiting_writers > 0) {
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
    if (rwlock->writer || rwlock->readers > 0) {
        result = pthread_mutex_unlock(&rwlock->mutex);
        return result != 0 ? result : EBUSY;
    }
    rwlock->writer = 1;
    result = pthread_mutex_unlock(&rwlock->mutex);
    if (result != 0) {
        rin_pthread_rwlock_rollback_acquire(rwlock, 1);
        return result;
    }
    return 0;
}

int pthread_rwlock_unlock(pthread_rwlock_t* rwlock) {
    int notify_result = 0;
    int unlock_result;
    if (!rwlock) return EINVAL;
    unlock_result = pthread_mutex_lock(&rwlock->mutex);
    if (unlock_result != 0) return unlock_result;
    if (rwlock->writer) {
        rwlock->writer = 0;
    } else if (rwlock->readers > 0) {
        rwlock->readers--;
    } else {
        unlock_result = pthread_mutex_unlock(&rwlock->mutex);
        return unlock_result != 0 ? unlock_result : EPERM;
    }

    if (rwlock->waiting_writers > 0) {
        if (rwlock->writer == 0 && rwlock->readers == 0) {
            notify_result = pthread_cond_signal(&rwlock->can_write);
        }
    } else if (rwlock->waiting_readers > 0) {
        notify_result = pthread_cond_broadcast(&rwlock->can_read);
    }

    unlock_result = pthread_mutex_unlock(&rwlock->mutex);
    return notify_result != 0 ? notify_result : unlock_result;
}
