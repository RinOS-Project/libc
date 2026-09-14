/*
 * RinOS libc - sys/syscall.h
 * システムコール番号定義 (rin.h / platform_bare64.cと同期)
 */

#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#if defined(__cplusplus) && !defined(RIN_FREESTANDING) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <errno.h>
#else
#include "../errno.h"
#endif
#include "../stdint.h"
#include <rin/syscall_legacy.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Syscall numbers come from the generated Rin ABI schema.  POSIX-style names
 * below are RinPort source aliases only; they are not a Linux kernel ABI. */

/* thread stack info flags */
#define RIN_THREAD_STACK_INFO_FLAG_GROWS_DOWN 0x00000001U
#define RIN_THREAD_STACK_INFO_FLAG_HAS_GUARD  0x00000002U
#define RIN_THREAD_STACK_INFO_FLAG_DETACHED   0x00000004U

#ifndef RIN_THREAD_STACK_INFO_V1_DEFINED
#define RIN_THREAD_STACK_INFO_V1_DEFINED 1
typedef struct rin_thread_stack_info_v1 {
    unsigned long long stack_base;  /* usable lowest address (guard excluded) */
    unsigned long long stack_size;  /* usable bytes (guard excluded) */
    unsigned long long guard_base;  /* guard lowest address (optional) */
    unsigned long long guard_size;  /* guard bytes (optional) */
    unsigned int flags;             /* RIN_THREAD_STACK_INFO_FLAG_* */
    unsigned int reserved;
} rin_thread_stack_info_v1;
#endif

/*
 * Linux-hosted futex regression support.
 *
 * The normal lowercase aliases below deliberately name the RinOS syscall
 * ABI.  A Linux process cannot execute those numbers directly: in
 * particular, RinOS SYS_FUTEX is not the x86-64 Linux futex syscall number.
 * Keep this opt-in test adapter narrow so hosted C++ synchronization tests
 * can exercise a real Linux futex without changing the product ABI.
 */
#if defined(RIN_HOST_LINUX_FUTEX_TEST)
#if !defined(__linux__)
#error "RIN_HOST_LINUX_FUTEX_TEST requires a Linux host"
#elif defined(__x86_64__)
#define RIN_HOST_LINUX_SYS_FUTEX       202u
#define RIN_HOST_LINUX_SYS_SCHED_YIELD 24u
#elif defined(__i386__)
#define RIN_HOST_LINUX_SYS_FUTEX       240u
#define RIN_HOST_LINUX_SYS_SCHED_YIELD 158u
#else
#error "RIN_HOST_LINUX_FUTEX_TEST supports only x86 Linux targets"
#endif
#endif

/* ═══════════════════════════════════════════════════════════════
 * Linux互換小文字エイリアス (Abseil等で使用)
 * ═══════════════════════════════════════════════════════════════*/

/* Use linux/unistd.h numbers for Linux-compatible syscall names */
#define SYS_write           SYS_WRITE
#define SYS_read            SYS_READ
#define SYS_open            SYS_OPEN
#define SYS_close           SYS_CLOSE
#define SYS_mmap            SYS_MMAP
#define SYS_munmap          SYS_MUNMAP
#define SYS_mprotect        SYS_MPROTECT
#define SYS_madvise         SYS_MADVISE
#define SYS_mincore         SYS_MINCORE
#define SYS_mlockall        SYS_MLOCKALL
#define SYS_munlockall      SYS_MUNLOCKALL
#define SYS_mremap          SYS_MREMAP
#define SYS_brk             SYS_BRK
#define SYS_exit            SYS_EXIT
#define SYS_fork            SYS_FORK
#define SYS_execve          SYS_EXEC
#define SYS_wait4           SYS_WAIT4
#define SYS_getrusage       SYS_GETRUSAGE
#define SYS_getpriority     SYS_GETPRIORITY
#define SYS_setpriority     SYS_SETPRIORITY
#define SYS_kill            SYS_KILL
#define SYS_getpid          SYS_GETPID
#define SYS_umask           SYS_UMASK
#define SYS_sethostname     SYS_SETHOSTNAME
#define SYS_setdomainname   SYS_SETDOMAINNAME
#define SYS_dup3            SYS_DUP3

/* Signal-related RinPort syscalls. */
#define SYS_rt_sigprocmask  SYS_RT_SIGPROCMASK
#define SYS_rt_sigaction    SYS_RT_SIGACTION
#define SYS_rt_sigreturn    SYS_RT_SIGRETURN
#define SYS_rt_sigpending   SYS_RT_SIGPENDING
#define SYS_sigaltstack     SYS_SIGALTSTACK
#define SYS_rt_sigsuspend   SYS_RT_SIGSUSPEND
#define SYS_ppoll           SYS_PPOLL

/* Thread-related syscalls */
#define SYS_gettid          SYS_GETTID
#define SYS_set_tid_address 351
#if defined(RIN_HOST_LINUX_FUTEX_TEST)
#define SYS_sched_yield     RIN_HOST_LINUX_SYS_SCHED_YIELD
#define SYS_futex           RIN_HOST_LINUX_SYS_FUTEX
#else
#define SYS_sched_yield     SYS_SCHED_YIELD
#define SYS_futex           SYS_FUTEX
#endif
#define SYS_clone           SYS_CLONE
#define SYS_getcpu          SYS_GETCPU

/* ═══════════════════════════════════════════════════════════════
 * Linux __NR_* 互換マクロ
 * RinOS番号にマッピング — カーネルハンドラと一致させる
 * ═══════════════════════════════════════════════════════════════*/

/* Map Linux __NR_* names to RinOS SYS_* numbers so ported runtime
 * syscall(__NR_xxx, ...) calls reach the correct kernel handler. */
#define __NR_exit           SYS_EXIT        /* 0 */
#define __NR_read           SYS_READ        /* 22 */
#define __NR_write          SYS_WRITE       /* 23 */
#define __NR_open           SYS_OPEN        /* 20 */
#define __NR_close          SYS_CLOSE       /* 21 */
#define __NR_brk            SYS_BRK         /* 53 */
#define __NR_mmap2          SYS_MMAP        /* 50 */
#define __NR_mmap           SYS_MMAP        /* 50 */
#define __NR_munmap         SYS_MUNMAP      /* 51 */
#define __NR_mprotect       SYS_MPROTECT    /* 52 */
#define __NR_madvise        SYS_MADVISE     /* 58 */
#define __NR_mincore        SYS_MINCORE     /* 59 */
#define __NR_mlockall       SYS_MLOCKALL    /* 66 */
#define __NR_munlockall     SYS_MUNLOCKALL  /* 67 */
#define __NR_mremap         SYS_MREMAP      /* 68 */
#define __NR_rt_sigaction   174
#define __NR_rt_sigprocmask 175
#define __NR_rt_sigpending  176
#define __NR_rt_sigreturn   173
#define __NR_sigaltstack    SYS_sigaltstack
#define __NR_rt_sigsuspend  SYS_rt_sigsuspend
#define __NR_ppoll          SYS_ppoll
#define __NR_umask          SYS_UMASK
#define __NR_sethostname    SYS_SETHOSTNAME
#define __NR_setdomainname  SYS_SETDOMAINNAME
#define __NR_dup3          SYS_DUP3
#define __NR_getrusage      SYS_GETRUSAGE
#define __NR_getpriority    SYS_GETPRIORITY
#define __NR_setpriority    SYS_SETPRIORITY
#define __NR_wait4          SYS_WAIT4
#define __NR_gettid         SYS_GETTID
#if defined(RIN_HOST_LINUX_FUTEX_TEST)
#define __NR_futex          RIN_HOST_LINUX_SYS_FUTEX
#define __NR_sched_yield    RIN_HOST_LINUX_SYS_SCHED_YIELD
#else
#define __NR_futex          240
#define __NR_sched_yield    SYS_SCHED_YIELD
#endif
#define __NR_clone          SYS_CLONE
#define __NR_getcpu         SYS_GETCPU
#define __NR_tkill          SYS_TKILL       /* 500 (RinOS独自, Linux 200はGUIと衝突) */
#define __NR_tgkill         SYS_TGKILL      /* 501 */
#define __NR_exit_group     SYS_EXIT        /* 0 — RinOSはexit_groupなし */
#define __NR_clock_gettime  SYS_TIME        /* 60 — 簡易マッピング */
#define __NR_getrandom      350             /* bounded kernel CSPRNG */
#define __NR_set_tid_address 351            /* clear_child_tid handler */

/* ═══════════════════════════════════════════════════════════════
 * システムコール呼び出しマクロ
 * x86-64: syscall命令 (rax=num, rdi,rsi,rdx,r10,r8,r9=args)
 * x86: int $0x80 (eax=num, ebx,ecx,edx,esi,edi,ebp=args)
 * ═══════════════════════════════════════════════════════════════*/

#if defined(__x86_64__) || defined(_M_X64)
/* ─────────────────────────────────────────────────────────────
 * x86-64 syscall implementations
 * ─────────────────────────────────────────────────────────────*/

static inline intptr_t _syscall0(uintptr_t num) {
    intptr_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline intptr_t _syscall1(uintptr_t num, uintptr_t a1) {
    intptr_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline intptr_t _syscall2(uintptr_t num, uintptr_t a1, uintptr_t a2) {
    intptr_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline intptr_t _syscall3(uintptr_t num, uintptr_t a1,
                             uintptr_t a2, uintptr_t a3) {
    intptr_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline intptr_t _syscall4(uintptr_t num, uintptr_t a1,
                             uintptr_t a2, uintptr_t a3,
                             uintptr_t a4) {
    intptr_t ret;
    register uintptr_t r10 __asm__("r10") = a4;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline intptr_t _syscall5(uintptr_t num, uintptr_t a1,
                             uintptr_t a2, uintptr_t a3,
                             uintptr_t a4, uintptr_t a5) {
    intptr_t ret;
    register uintptr_t r10 __asm__("r10") = a4;
    register uintptr_t r8 __asm__("r8") = a5;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline intptr_t _syscall6(uintptr_t num, uintptr_t a1,
                             uintptr_t a2, uintptr_t a3,
                             uintptr_t a4, uintptr_t a5,
                             uintptr_t a6) {
    intptr_t ret;
    register uintptr_t r10 __asm__("r10") = a4;
    register uintptr_t r8 __asm__("r8") = a5;
    register uintptr_t r9 __asm__("r9") = a6;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

#else
/* ─────────────────────────────────────────────────────────────
 * x86 (32-bit) syscall implementations using int $0x80
 * ─────────────────────────────────────────────────────────────*/

static inline intptr_t _syscall0(uintptr_t num) {
    intptr_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num)
        : "memory"
    );
    return ret;
}

static inline intptr_t _syscall1(uintptr_t num, uintptr_t a1) {
    intptr_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1)
        : "memory"
    );
    return ret;
}

static inline intptr_t _syscall2(uintptr_t num, uintptr_t a1, uintptr_t a2) {
    intptr_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2)
        : "memory"
    );
    return ret;
}

static inline intptr_t _syscall3(uintptr_t num, uintptr_t a1,
                             uintptr_t a2, uintptr_t a3) {
    intptr_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2), "d"(a3)
        : "memory"
    );
    return ret;
}

static inline intptr_t _syscall4(uintptr_t num, uintptr_t a1,
                             uintptr_t a2, uintptr_t a3,
                             uintptr_t a4) {
    intptr_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2), "d"(a3), "S"(a4)
        : "memory"
    );
    return ret;
}

static inline intptr_t _syscall5(uintptr_t num, uintptr_t a1,
                             uintptr_t a2, uintptr_t a3,
                             uintptr_t a4, uintptr_t a5) {
    intptr_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2), "d"(a3), "S"(a4), "D"(a5)
        : "memory"
    );
    return ret;
}

static inline intptr_t _syscall6(uintptr_t num, uintptr_t a1,
                             uintptr_t a2, uintptr_t a3,
                             uintptr_t a4, uintptr_t a5,
                             uintptr_t a6) {
    intptr_t ret;
    __asm__ volatile (
        "movl %7, %%eax\n\t"
        "pushl %%ebp\n\t"
        "movl %%eax, %%ebp\n\t"
        "movl %1, %%eax\n\t"
        "int $0x80\n\t"
        "popl %%ebp"
        : "=&a"(ret)
        : "g"(num), "b"(a1), "c"(a2), "d"(a3), "S"(a4), "D"(a5), "m"(a6)
        : "memory"
    );
    return ret;
}

#endif /* __x86_64__ */

/* ═══════════════════════════════════════════════════════════════
 * syscallマクロ (引数個数ディスパッチ)
 * 実引数数に応じて _syscall0.._syscall6 を選択する。システムコール
 * レジスタはCのlongではなくtarget pointer幅のwordを運ぶため、引数は
 * uintptr_t、戻り値はerrorを表せるintptr_tに正規化する（LLP64 hostで
 * pointerを縮小しない）。
 * ═══════════════════════════════════════════════════════════════*/

#define __RIN_SYSCALL_1(n) \
    _syscall0((uintptr_t)(n))
#define __RIN_SYSCALL_2(n, a1) \
    _syscall1((uintptr_t)(n), (uintptr_t)(a1))
#define __RIN_SYSCALL_3(n, a1, a2) \
    _syscall2((uintptr_t)(n), (uintptr_t)(a1), (uintptr_t)(a2))
#define __RIN_SYSCALL_4(n, a1, a2, a3) \
    _syscall3((uintptr_t)(n), (uintptr_t)(a1), (uintptr_t)(a2), \
              (uintptr_t)(a3))
#define __RIN_SYSCALL_5(n, a1, a2, a3, a4) \
    _syscall4((uintptr_t)(n), (uintptr_t)(a1), (uintptr_t)(a2), \
              (uintptr_t)(a3), (uintptr_t)(a4))
#define __RIN_SYSCALL_6(n, a1, a2, a3, a4, a5) \
    _syscall5((uintptr_t)(n), (uintptr_t)(a1), (uintptr_t)(a2), \
              (uintptr_t)(a3), (uintptr_t)(a4), (uintptr_t)(a5))
#define __RIN_SYSCALL_7(n, a1, a2, a3, a4, a5, a6) \
    _syscall6((uintptr_t)(n), (uintptr_t)(a1), (uintptr_t)(a2), \
              (uintptr_t)(a3), (uintptr_t)(a4), (uintptr_t)(a5), \
              (uintptr_t)(a6))

#define __RIN_SYSCALL_DISPATCH(_1, _2, _3, _4, _5, _6, _7, NAME, ...) NAME
#define __RIN_SYSCALL_IMPL(...) \
    __RIN_SYSCALL_DISPATCH(__VA_ARGS__, __RIN_SYSCALL_7, __RIN_SYSCALL_6, \
                           __RIN_SYSCALL_5, __RIN_SYSCALL_4, __RIN_SYSCALL_3, \
                           __RIN_SYSCALL_2, __RIN_SYSCALL_1)

static inline intptr_t __rin_syscall_posixize(intptr_t ret) {
    if (ret < 0 && ret >= -4095) {
        errno = (int)(-ret);
        return -1;
    }
    return ret;
}

#define syscall(...) __rin_syscall_posixize(__RIN_SYSCALL_IMPL(__VA_ARGS__)(__VA_ARGS__))

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SYSCALL_H */
