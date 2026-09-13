/*
 * RinOS libc - signal.h
 * シグナル処理
 */

#ifndef _SIGNAL_H
#define _SIGNAL_H

#include "stddef.h"
#include "sys/syscall.h"
#include "errno.h"

#ifndef _RIN_SIGNAL_SYSCALL0
#define _RIN_SIGNAL_SYSCALL0(number) _syscall0((uintptr_t)(number))
#endif
#ifndef _RIN_SIGNAL_SYSCALL2
#define _RIN_SIGNAL_SYSCALL2(number, argument1, argument2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2))
#endif
#ifndef _RIN_SIGNAL_SYSCALL4
#define _RIN_SIGNAL_SYSCALL4(number, argument1, argument2, argument3, argument4) \
    _syscall4((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3), \
              (uintptr_t)(argument4))
#endif

static inline int __rin_signal_status_result(intptr_t result) {
    if (result < 0) {
        uintptr_t magnitude = (uintptr_t)0 - (uintptr_t)result;
        errno = magnitude != 0u && magnitude <= 4095u ?
            (int)magnitude : EIO;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * 型定義
 * ═══════════════════════════════════════════════════════════════*/

typedef int sig_atomic_t;
typedef void (*sighandler_t)(int);

#ifndef _SIGSET_T_DEFINED
#define _SIGSET_T_DEFINED
typedef unsigned long sigset_t;
#endif

/* ═══════════════════════════════════════════════════════════════
 * シグナル番号
 * ═══════════════════════════════════════════════════════════════*/

#define SIGHUP      1   /* 端末ハングアップ */
#define SIGINT      2   /* 割り込み (Ctrl+C) */
#define SIGQUIT     3   /* 終了 (Ctrl+\) */
#define SIGILL      4   /* 不正命令 */
#define SIGTRAP     5   /* トレース/ブレークポイント */
#define SIGABRT     6   /* 異常終了 */
#define SIGBUS      7   /* バスエラー */
#define SIGFPE      8   /* 浮動小数点例外 */
#define SIGKILL     9   /* 強制終了 */
#define SIGUSR1     10  /* ユーザー定義1 */
#define SIGSEGV     11  /* セグメンテーション違反 */
#define SIGUSR2     12  /* ユーザー定義2 */
#define SIGPIPE     13  /* パイプ破損 */
#define SIGALRM     14  /* アラーム */
#define SIGTERM     15  /* 終了要求 */
#define SIGSTKFLT   16  /* スタックフォルト */
#define SIGCHLD     17  /* 子プロセス終了 */
#define SIGCONT     18  /* 続行 */
#define SIGSTOP     19  /* 停止 */
#define SIGTSTP     20  /* 端末からの停止 */
#define SIGTTIN     21  /* バックグラウンド読み取り */
#define SIGTTOU     22  /* バックグラウンド書き込み */
#define SIGURG      23  /* ソケット緊急 */
#define SIGXCPU     24  /* CPU時間超過 */
#define SIGXFSZ     25  /* ファイルサイズ超過 */
#define SIGVTALRM   26  /* 仮想アラーム */
#define SIGPROF     27  /* プロファイルアラーム */
#define SIGWINCH    28  /* ウィンドウサイズ変更 */
#define SIGIO       29  /* I/O可能 */
#define SIGPOLL     SIGIO
#define SIGPWR      30  /* 電源異常 */
#define SIGSYS      31  /* 不正システムコール */

#define NSIG        32  /* シグナル数 */

/* ═══════════════════════════════════════════════════════════════
 * シグナルハンドラ定数
 * ═══════════════════════════════════════════════════════════════*/

#define SIG_DFL     ((sighandler_t)0)   /* デフォルト動作 */
#define SIG_IGN     ((sighandler_t)1)   /* 無視 */
#define SIG_ERR     ((sighandler_t)-1)  /* エラー */

/* sigprocmask how argument */
#define SIG_BLOCK   0   /* Block signals */
#define SIG_UNBLOCK 1   /* Unblock signals */
#define SIG_SETMASK 2   /* Set signal mask */

/* ═══════════════════════════════════════════════════════════════
 * siginfo_t - Linux ABI 互換シグナル情報構造体 (128 bytes)
 *
 * レイアウト:
 *   +0:  si_signo  (int, 4)
 *   +4:  si_errno  (int, 4)
 *   +8:  si_code   (int, 4)
 *   +12: [pad 4]
 *   +16: union _sifields {
 *          _sigfault { void* _addr; ... }   ← si_addr at +16
 *          _kill     { pid_t _pid; uid_t _uid; }
 *          _sigchld  { ... }
 *          _timer    { ... }
 *          _sigpoll  { ... }
 *          _rt       { pid, uid, sigval }
 *        }
 *   Total: 128 bytes (SI_MAX_SIZE)
 *
 * カーネル KernelSigInfo (sched.h) と同一オフセット。
 * ═══════════════════════════════════════════════════════════════*/

#define SI_MAX_SIZE 128

#ifndef _SIGVAL_DEFINED
#define _SIGVAL_DEFINED
union sigval {
    int   sival_int;
    void* sival_ptr;
};
#endif

#ifndef _SIGEVENT_DEFINED
#define _SIGEVENT_DEFINED
struct sigevent {
    int sigev_notify;
    int sigev_signo;
    union sigval sigev_value;
    /* POSIX SIGEV_THREAD payload.  Keep these members here as well as in
     * time.h so a translation unit that includes signal.h first still sees
     * the complete public type used by timer_create(). */
    void (*sigev_notify_function)(union sigval);
    void* sigev_notify_attributes;
};
#endif

union __rin_siginfo_fields {
    int _pad[(SI_MAX_SIZE - 16) / (int)sizeof(int)];

    struct {
        void* _addr;
        short _addr_lsb;
    } _sigfault;
    struct {
        int _pid;
        int _uid;
    } _kill;
    struct {
        int _pid;
        int _uid;
        int _status;
        int __pad_chld;
        long _utime;
        long _stime;
    } _sigchld;
    struct {
        int _tid;
        int _overrun;
        union sigval _sigval;
    } _timer;
    struct {
        int _pid;
        int _uid;
        union sigval _sigval;
    } _rt;
    struct {
        long _band;
        int _fd;
    } _sigpoll;
};

struct __rin_siginfo_payload {
    int si_signo;
    int si_errno;
    int si_code;
    int __si_pad0;
    union __rin_siginfo_fields _sifields;
};

typedef union siginfo {
    struct __rin_siginfo_payload __fields;
    int _si_pad[SI_MAX_SIZE / (int)sizeof(int)]; /* Ensure 128 byte total size */
} siginfo_t;

/* Linux ABI 互換アクセサマクロ */
#define si_signo     __fields.si_signo
#define si_errno     __fields.si_errno
#define si_code      __fields.si_code
#define si_pid       __fields._sifields._kill._pid
#define si_uid       __fields._sifields._kill._uid
#define si_addr      __fields._sifields._sigfault._addr
#define si_addr_lsb  __fields._sifields._sigfault._addr_lsb
#define si_status    __fields._sifields._sigchld._status
#define si_utime     __fields._sifields._sigchld._utime
#define si_stime     __fields._sifields._sigchld._stime
#define si_value     __fields._sifields._rt._sigval
#define si_band      __fields._sifields._sigpoll._band
#define si_fd        __fields._sifields._sigpoll._fd
#define si_timerid   __fields._sifields._timer._tid
#define si_overrun   __fields._sifields._timer._overrun

/* ═══════════════════════════════════════════════════════════════
 * sigaction構造体
 * ═══════════════════════════════════════════════════════════════*/

/* Include ucontext for full definition (used with SA_SIGINFO) */
#include "ucontext.h"

union __rin_sigaction_handler {
    sighandler_t sa_handler;
    void (*sa_sigaction)(int, siginfo_t*, void*);
};

struct sigaction {
    union __rin_sigaction_handler __handler;
    sigset_t     sa_mask;       /* ブロックするシグナル */
    int          sa_flags;      /* フラグ */
    void        (*sa_restorer)(void); /* Restore handler (unused) */
};

#define sa_handler   __handler.sa_handler
#define sa_sigaction __handler.sa_sigaction

/* sa_flags */
#define SA_NOCLDSTOP    0x00000001  /* 子停止時にSIGCHLDを送らない */
#define SA_NOCLDWAIT    0x00000002  /* 子をゾンビにしない */
#define SA_SIGINFO      0x00000004  /* 詳細情報付きハンドラ */
#define SA_ONSTACK      0x08000000  /* 代替シグナルスタックを使用 */
#define SA_RESTART      0x10000000  /* システムコールを再開 */
#define SA_NODEFER      0x40000000  /* ハンドラ実行中もブロックしない */
#define SA_RESETHAND    0x80000000  /* 1回だけ */

/* ═══════════════════════════════════════════════════════════════
 * Signal Codes (si_code values)
 * ═══════════════════════════════════════════════════════════════*/

/* si_code values for SIGILL */
#define ILL_ILLOPC      1   /* Illegal opcode */
#define ILL_ILLOPN      2   /* Illegal operand */
#define ILL_ILLADR      3   /* Illegal addressing mode */
#define ILL_ILLTRP      4   /* Illegal trap */
#define ILL_PRVOPC      5   /* Privileged opcode */
#define ILL_PRVREG      6   /* Privileged register */
#define ILL_COPROC      7   /* Coprocessor error */
#define ILL_BADSTK      8   /* Internal stack error */

/* si_code values for SIGFPE */
#define FPE_INTDIV      1   /* Integer divide by zero */
#define FPE_INTOVF      2   /* Integer overflow */
#define FPE_FLTDIV      3   /* Floating point divide by zero */
#define FPE_FLTOVF      4   /* Floating point overflow */
#define FPE_FLTUND      5   /* Floating point underflow */
#define FPE_FLTRES      6   /* Floating point inexact result */
#define FPE_FLTINV      7   /* Invalid floating point operation */
#define FPE_FLTSUB      8   /* Subscript out of range */

/* si_code values for SIGSEGV */
#define SEGV_MAPERR     1   /* Address not mapped to object */
#define SEGV_ACCERR     2   /* Invalid permissions for mapped object */
#define SEGV_BNDERR     3   /* Bounds checking failure */
#define SEGV_PKUERR     4   /* Protection key checking failure */

/* si_code values for SIGBUS */
#define BUS_ADRALN      1   /* Invalid address alignment */
#define BUS_ADRERR      2   /* Non-existent physical address */
#define BUS_OBJERR      3   /* Object specific hardware error */
#define BUS_MCEERR_AR   4   /* Hardware memory error consumed on a machine check; action required */
#define BUS_MCEERR_AO   5   /* Hardware memory error detected in process but not consumed; action optional */

/* si_code values for SIGTRAP */
#define TRAP_BRKPT      1   /* Process breakpoint */
#define TRAP_TRACE      2   /* Process trace trap */
#define TRAP_BRANCH     3   /* Process taken branch trap */
#define TRAP_HWBKPT     4   /* Hardware breakpoint/watchpoint */

/* si_code values for SIGCHLD */
#define CLD_EXITED      1   /* Child has exited */
#define CLD_KILLED      2   /* Child was killed */
#define CLD_DUMPED      3   /* Child terminated abnormally */
#define CLD_TRAPPED     4   /* Traced child has trapped */
#define CLD_STOPPED     5   /* Child has stopped */
#define CLD_CONTINUED   6   /* Stopped child has continued */

/* si_code values for SIGPOLL */
#define POLL_IN         1   /* Data input available */
#define POLL_OUT        2   /* Output buffers available */
#define POLL_MSG        3   /* Input message available */
#define POLL_ERR        4   /* I/O error */
#define POLL_PRI        5   /* High priority input available */
#define POLL_HUP        6   /* Device disconnected */

/* General si_code values */
#define SI_USER         0       /* Sent by kill, sigsend, raise */
#define SI_KERNEL       0x80    /* Sent by the kernel */
#define SI_QUEUE        -1      /* Sent by sigqueue */
#define SI_TIMER        -2      /* Sent by timer expiration */
#define SI_MESGQ        -3      /* Sent by real time mesq state change */
#define SI_ASYNCIO      -4      /* Sent by AIO completion */
#define SI_SIGIO        -5      /* Sent by queued SIGIO */
#define SI_TKILL        -6      /* Sent by tkill system call */

/* ═══════════════════════════════════════════════════════════════
 * Alternate Signal Stack
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _STACK_T_DEFINED
#define _STACK_T_DEFINED
typedef struct {
    void*  ss_sp;       /* Stack base or pointer */
    int    ss_flags;    /* Flags */
    size_t ss_size;     /* Number of bytes in stack */
} stack_t;
#endif

/* Flags for ss_flags */
#define SS_ONSTACK  1   /* Currently executing on this stack */
#define SS_DISABLE  2   /* Disable this alternate stack */

/* Minimum and default stack sizes */
#define MINSIGSTKSZ 2048
#define SIGSTKSZ    8192

static inline int sigaltstack(const stack_t* ss, stack_t* old_ss) {
    /* The kernel owns the per-thread stack state and performs the atomic
     * copy-in/copy-out validation.  Passing NULL for either pointer follows
     * POSIX: NULL ss queries only, NULL old_ss suppresses the snapshot. */
    return __rin_signal_status_result(
        _RIN_SIGNAL_SYSCALL2(SYS_sigaltstack, ss, old_ss));
}

/* ═══════════════════════════════════════════════════════════════
 * シグナルセット操作
 * ═══════════════════════════════════════════════════════════════*/

static inline int sigemptyset(sigset_t* set) {
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    *set = 0;
    return 0;
}

static inline int sigfillset(sigset_t* set) {
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    *set = ~0UL;
    return 0;
}

static inline int sigaddset(sigset_t* set, int signum) {
    if (!set || signum < 1 || signum >= NSIG) {
        errno = EINVAL;
        return -1;
    }
    *set |= (1UL << signum);
    return 0;
}

static inline int sigdelset(sigset_t* set, int signum) {
    if (!set || signum < 1 || signum >= NSIG) {
        errno = EINVAL;
        return -1;
    }
    *set &= ~(1UL << signum);
    return 0;
}

static inline int sigismember(const sigset_t* set, int signum) {
    if (!set || signum < 1 || signum >= NSIG) {
        errno = EINVAL;
        return -1;
    }
    return (*set & (1UL << signum)) ? 1 : 0;
}

/* ═══════════════════════════════════════════════════════════════
 * シグナル復帰トランポリン (sa_restorer)
 *
 * シグナルハンドラからの RET 先。SYS_rt_sigreturn を呼び出して
 * カーネルに保存コンテキストを復元させる。
 * ═══════════════════════════════════════════════════════════════*/

#if defined(__x86_64__) || defined(_M_X64)
__attribute__((used, naked))
static void __restore_rt(void) {
    __asm__ volatile (
        "mov $173, %%rax\n"    /* SYS_rt_sigreturn = 173 */
        "syscall\n"
        "ud2\n"                /* Should never reach here */
        ::: "memory"
    );
}
#elif defined(__i386__) || defined(_M_IX86)
__attribute__((used, naked))
static void __restore_rt(void) {
    __asm__ volatile (
        "mov $173, %%eax\n"    /* SYS_rt_sigreturn = 173 */
        "int $0x80\n"
        "ud2\n"                /* Should never reach here */
        ::: "memory"
    );
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * シグナル関数
 * ═══════════════════════════════════════════════════════════════*/

static inline sighandler_t signal(int signum, sighandler_t handler) {
    intptr_t result;
    struct sigaction oldact;
    if (signum < 1 || signum >= NSIG ||
        signum == SIGKILL || signum == SIGSTOP) {
        errno = EINVAL;
        return SIG_ERR;
    }

    /* The disposition is process-wide kernel state.  A header-local cache
     * gives each translation unit a different "previous" handler and races
     * with sigaction() calls from other threads.  Ask the kernel for the old
     * action in the same transaction instead. */
    for (size_t index = 0u; index < sizeof(oldact); ++index)
        ((unsigned char*)&oldact)[index] = 0u;

    /* sigaction経由でカーネルに登録 */
    struct sigaction act;
    act.sa_handler = handler;
    act.sa_mask = 0;
    act.sa_flags = 0x04000000; /* SA_RESTORER */
    act.sa_restorer = __restore_rt;

    result = _RIN_SIGNAL_SYSCALL4(
        SYS_rt_sigaction, signum, &act, &oldact, sizeof(sigset_t));
    if (__rin_signal_status_result(result) != 0) return SIG_ERR;
    return oldact.sa_handler;
}

static inline int sigaction(int signum, const struct sigaction* act,
                            struct sigaction* oldact) {
    if (signum < 1 || signum >= NSIG ||
        signum == SIGKILL || signum == SIGSTOP) {
        errno = EINVAL;
        return -1;
    }

    /* sa_restorerが未設定の場合、自動的に__restore_rtを設定 */
    if (act) {
        struct sigaction mod_act = *act;
        if (mod_act.sa_restorer == 0) {
            mod_act.sa_restorer = __restore_rt;
            mod_act.sa_flags |= 0x04000000; /* SA_RESTORER */
        }
        intptr_t result = _RIN_SIGNAL_SYSCALL4(
            SYS_rt_sigaction, signum, &mod_act, oldact,
            sizeof(sigset_t));
        return __rin_signal_status_result(result);
    } else {
        return __rin_signal_status_result(_RIN_SIGNAL_SYSCALL4(
            SYS_rt_sigaction, signum, 0, oldact, sizeof(sigset_t)));
    }
}

static inline int kill(int pid, int sig) {
    if (sig < 0 || sig >= NSIG) {
        errno = EINVAL;
        return -1;
    }
    return __rin_signal_status_result(
        _RIN_SIGNAL_SYSCALL2(SYS_KILL, pid, sig));
}

static inline int raise(int sig) {
    intptr_t tid;
    intptr_t result;
    if (sig < 0 || sig >= NSIG) {
        errno = EINVAL;
        return -1;
    }
    tid = _RIN_SIGNAL_SYSCALL0(SYS_GETTID);
    if (tid < 0) return __rin_signal_status_result(tid);
    result = _RIN_SIGNAL_SYSCALL2(SYS_TKILL, tid, sig);
    return __rin_signal_status_result(result);
}

static inline int sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
    if (set && how != SIG_BLOCK && how != SIG_UNBLOCK && how != SIG_SETMASK) {
        errno = EINVAL;
        return -1;
    }
    return __rin_signal_status_result(_RIN_SIGNAL_SYSCALL4(
        SYS_rt_sigprocmask, how, set, oldset, sizeof(sigset_t)));
}

static inline int sigpending(sigset_t* set) {
    sigset_t snapshot = 0;
    intptr_t result;
    if (!set) {
        errno = EFAULT;
        return -1;
    }
    result = _RIN_SIGNAL_SYSCALL2(
        SYS_rt_sigpending, &snapshot, sizeof(snapshot));
    if (__rin_signal_status_result(result) != 0) return -1;
    *set = snapshot;
    return 0;
}

static inline int sigsuspend(const sigset_t* mask) {
    intptr_t result;
    if (!mask) {
        errno = EFAULT;
        return -1;
    }
    result = _RIN_SIGNAL_SYSCALL2(SYS_rt_sigsuspend, mask,
                                   sizeof(sigset_t));
    if (result == 0) {
        /* A successful return would violate sigsuspend's EINTR contract. */
        errno = EIO;
        return -1;
    }
    return __rin_signal_status_result(result);
}

/* pthread_kill: スレッドにシグナル送信 */
static inline int pthread_kill_sig(uintptr_t tid, int sig) {
    intptr_t result;
    if (sig < 0 || sig >= NSIG) return EINVAL;
    result = _RIN_SIGNAL_SYSCALL2(SYS_TKILL, tid, sig);
    if (result < 0 && result >= -4095) return (int)-result;
    return result == 0 ? 0 : EIO;
}

#ifdef __cplusplus
}
#endif

#endif /* _SIGNAL_H */
