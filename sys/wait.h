/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - sys/wait.h
 * プロセス終了情報
 */

#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include "types.h"
#include "syscall.h"
#include "../time.h"
#include "../errno.h"
#include "../limits.h"
#include "../signal.h"

#ifndef _RIN_WAIT_SYSCALL3
#define _RIN_WAIT_SYSCALL3(number, argument1, argument2, argument3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3))
#endif
#ifndef _RIN_WAIT_SYSCALL4
#define _RIN_WAIT_SYSCALL4(number, argument1, argument2, argument3, argument4) \
    _syscall4((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3), \
              (uintptr_t)(argument4))
#endif
#ifndef _RIN_WAITID_SYSCALL4
#define _RIN_WAITID_SYSCALL4(number, argument1, argument2, argument3, argument4) \
    _syscall4((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3), \
              (uintptr_t)(argument4))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * waitpid オプション
 * ═══════════════════════════════════════════════════════════════*/

#define WNOHANG    1   /* ブロックしない */
#define WUNTRACED  2   /* 停止した子も報告 */
#define WSTOPPED   WUNTRACED
#define WEXITED     4   /* 終了した子を報告 */
#define WCONTINUED 8   /* 再開した子も報告 */
#define WNOWAIT    0x01000000 /* 終了状態を消費しない */

/* Private selector carried through the existing SYS_WAIT ABI.  waitid()
 * needs to request only stopped/continued events without accidentally
 * consuming an exited child; waitpid() never accepts or emits this bit. */
#define __RIN_WAIT_OPTION_EXIT 0x40000000
#define __RIN_WAIT_OPTION_NO_EXIT 0x20000000
#define __RIN_WAIT_OPTION_NOWAIT 0x10000000
#define __RIN_WAIT_OPTION_GROUP 0x08000000

/* wait-family calls return a target-width word carrying either a child PID
 * or a negative errno.  Keep that word intact until the public pid_t
 * boundary so unknown negatives cannot masquerade as a PID and a wide
 * positive result cannot be truncated on LLP64. */
static inline pid_t __rin_wait_pid_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        if (result != -1) errno = EIO;
        return (pid_t)-1;
    }
    if (result > (intptr_t)INT_MAX || result < (intptr_t)INT_MIN) {
        errno = EOVERFLOW;
        return (pid_t)-1;
    }
    return (pid_t)result;
}

/* waitid's private syscall returns a positive visible PID for an event, but
 * POSIX waitid returns zero after the siginfo copy. */
static inline int __rin_waitid_event_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    if (result > (intptr_t)INT_MAX || result < (intptr_t)INT_MIN) {
        errno = EOVERFLOW;
        return -1;
    }
    return 0;
}

/* waitid() child selectors. Process-group and pidfd selectors need kernel
 * process-group/pidfd ownership and are therefore not emulated here. */
#ifndef _IDTYPE_T_DEFINED
#define _IDTYPE_T_DEFINED
typedef int idtype_t;
#endif
#define P_ALL  0
#define P_PID  1
#define P_PGID 2

/* ═══════════════════════════════════════════════════════════════
 * 終了ステータスマクロ
 *
 * ステータス形式:
 *   bits 0-7:  終了シグナル (0 = 正常終了)
 *   bits 8-15: 終了コード (正常終了時)
 * ═══════════════════════════════════════════════════════════════*/

/* 正常終了したか */
#define WIFEXITED(status)    (((status) & 0x7F) == 0)

/* 終了コードを取得 */
#define WEXITSTATUS(status)  (((status) >> 8) & 0xFF)

/* シグナルで終了したか */
#define WIFSIGNALED(status)  (((status) & 0x7F) != 0 && ((status) & 0x7F) != 0x7F)

/* 終了シグナルを取得 */
#define WTERMSIG(status)     ((status) & 0x7F)

/* 停止したか */
#define WIFSTOPPED(status)   (((status) & 0xFF) == 0x7F)

/* 停止シグナルを取得 */
#define WSTOPSIG(status)     (((status) >> 8) & 0xFF)

/* 再開したか */
#define WIFCONTINUED(status) ((status) == 0xFFFF)

/* コアダンプが生成されたか */
#define WCOREDUMP(status)    ((status) & 0x80)

/* ═══════════════════════════════════════════════════════════════
 * wait関数
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _RIN_WAIT_DEFINED
#define _RIN_WAIT_DEFINED
static inline pid_t wait(int* wstatus) {
    return __rin_wait_pid_result(
        _RIN_WAIT_SYSCALL3(SYS_WAIT, (uintptr_t)(intptr_t)-1,
                           (uintptr_t)wstatus, 0u));
}
#endif /* _RIN_WAIT_DEFINED */

static inline pid_t waitpid(pid_t pid, int* wstatus, int options) {
    /* The high bits below are private selectors used only by waitid's
     * syscall adapter.  They must never leak through the public waitpid
     * contract: accepting them would let a caller request non-consuming or
     * stopped-only behavior that POSIX waitpid does not define.  Keep this
     * mask deliberately limited to the three public option bits. */
    const int known_options = WNOHANG | WUNTRACED | WCONTINUED;
    if ((options & ~known_options) != 0) {
        errno = EINVAL;
        return (pid_t)-1;
    }
    return __rin_wait_pid_result(
        _RIN_WAIT_SYSCALL3(SYS_WAIT, (uintptr_t)(intptr_t)pid,
                           (uintptr_t)wstatus,
                           (uintptr_t)(unsigned int)options));
}

static inline void __rin_waitid_clear_info(siginfo_t* information) {
    unsigned long index;
    for (index = 0UL; index < sizeof(*information); ++index)
        ((unsigned char*)information)[index] = 0u;
}

static inline int waitid(idtype_t idtype, id_t id, siginfo_t* infop,
                         int options) {
    const int known_options = WNOHANG | WSTOPPED | WEXITED | WCONTINUED |
                              WNOWAIT;
    int wait_flags = 0;
    intptr_t result;

    if (!infop) {
        errno = EFAULT;
        return -1;
    }
    if ((options & ~known_options) != 0) {
        errno = EINVAL;
        return -1;
    }
    if ((options & (WSTOPPED | WEXITED | WCONTINUED)) == 0) {
        errno = EINVAL;
        return -1;
    }
    switch (idtype) {
        case P_ALL:
            break;
        case P_PID:
            if (id == 0u || id > (id_t)0x7fffffffU) {
                errno = EINVAL;
                return -1;
            }
            break;
        case P_PGID:
            if (id > (id_t)0x7fffffffU) {
                errno = EINVAL;
                return -1;
            }
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    if ((options & WSTOPPED) != 0) wait_flags |= WUNTRACED;
    if ((options & WCONTINUED) != 0) wait_flags |= WCONTINUED;
    if ((options & WEXITED) != 0) wait_flags |= __RIN_WAIT_OPTION_EXIT;
    else if ((options & (WSTOPPED | WCONTINUED)) != 0)
        wait_flags |= __RIN_WAIT_OPTION_NO_EXIT;
    if ((options & WNOWAIT) != 0) wait_flags |= __RIN_WAIT_OPTION_NOWAIT;
    if ((options & WNOHANG) != 0) wait_flags |= WNOHANG;

    result = _RIN_WAITID_SYSCALL4(
        SYS_WAITID, (uintptr_t)(int32_t)idtype, (uintptr_t)(uint32_t)id,
        (uintptr_t)infop, (uintptr_t)(unsigned int)wait_flags);
    if (__rin_waitid_event_result(result) != 0) return -1;
    if (result == 0) {
        __rin_waitid_clear_info(infop);
        return 0;
    }
    /* The private kernel boundary returns the visible child PID to
     * distinguish an event from WNOHANG's zero result. POSIX waitid itself
     * returns zero after the siginfo copy has committed. */
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * wait3/wait4 (BSD互換)
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _RUSAGE_DEFINED
#define _RUSAGE_DEFINED
struct rusage {
    struct timeval ru_utime;  /* ユーザーCPU時間 */
    struct timeval ru_stime;  /* システムCPU時間 */
    long ru_maxrss;           /* 最大RSS */
    long ru_ixrss;
    long ru_idrss;
    long ru_isrss;
    long ru_minflt;           /* ソフトページフォルト */
    long ru_majflt;           /* ハードページフォルト */
    long ru_nswap;
    long ru_inblock;          /* ブロック入力 */
    long ru_oublock;          /* ブロック出力 */
    long ru_msgsnd;
    long ru_msgrcv;
    long ru_nsignals;         /* 受信シグナル数 */
    long ru_nvcsw;            /* 自発的コンテキストスイッチ */
    long ru_nivcsw;           /* 非自発的コンテキストスイッチ */
};
#endif

/* rusageの定数 */
#define RUSAGE_SELF     0
#define RUSAGE_CHILDREN (-1)
#define RUSAGE_THREAD   1

static inline int __rin_wait_bsd_options_valid(int options) {
    return (options & ~(WNOHANG | WUNTRACED | WCONTINUED)) == 0;
}

static inline pid_t wait3(int* wstatus, int options, struct rusage* rusage) {
    if (!__rin_wait_bsd_options_valid(options)) {
        errno = EINVAL;
        return (pid_t)-1;
    }
    if (rusage) {
        return __rin_wait_pid_result(
            _RIN_WAIT_SYSCALL4(SYS_WAIT4, (uintptr_t)(intptr_t)-1,
                               (uintptr_t)wstatus, (uintptr_t)(unsigned int)options,
                               (uintptr_t)rusage));
    }
    return waitpid(-1, wstatus, options);
}

static inline pid_t wait4(pid_t pid, int* wstatus, int options, struct rusage* rusage) {
    if (!__rin_wait_bsd_options_valid(options)) {
        errno = EINVAL;
        return (pid_t)-1;
    }
    if (rusage) {
        return __rin_wait_pid_result(
            _RIN_WAIT_SYSCALL4(SYS_WAIT4, (uintptr_t)(intptr_t)pid,
                               (uintptr_t)wstatus, (uintptr_t)(unsigned int)options,
                               (uintptr_t)rusage));
    }
    return waitpid(pid, wstatus, options);
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_WAIT_H */
