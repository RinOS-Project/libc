/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - unistd.h
 * POSIX標準定義
 */

#ifndef _UNISTD_H
#define _UNISTD_H

#include "stddef.h"
#include "stdint.h"
#include "stdarg.h"
#include "limits.h"
#include "internal/posix_clock.h"
#include "errno.h"
#include "stdlib.h"
#include "sys/syscall.h"
#include "fcntl.h"
#include "sys/stat.h"
#include "sys/statvfs.h"
#include "sys/ioctl.h"
#include "termios.h"
#include "time.h"
#include "sys/resource.h"
#if !defined(RINSDK_SPLIT_BUILD)
#include "internal/pthread_atfork.h"
#endif
#include "internal/pthread_cancel_point.h"
#if !defined(RIN_LIBC_NO_ACCOUNT_COMPAT)
#include "rin_account_compat.h"
#endif
#include <rin/fs/path_at_abi.h>
#include <rin/fd_range_abi.h>
#include <rin/utsname_abi.h>
#include <rin/process/process_group_abi.h>
#include <rin/process/namespace_abi.h>
#include <rin/process/image_path_abi.h>
#include <rin/process/image_range_abi.h>
#include <rin/process/exec_limits_abi.h>
#include <rin/system_info_snapshot_v1.h>

#if ARG_MAX != RIN_EXEC_ARGUMENT_ENV_BYTES_MAX
#error "ARG_MAX must match the public exec argument/environment byte limit"
#endif
#include <rin/process/memory_abi.h>

/* The kernel preserves saved UID/GID values per process. */
#ifndef _POSIX_SAVED_IDS
#define _POSIX_SAVED_IDS 1
#endif
#ifndef _POSIX_JOB_CONTROL
#define _POSIX_JOB_CONTROL 1
#endif
#ifndef _POSIX_MESSAGE_PASSING
#define _POSIX_MESSAGE_PASSING 200809L
#endif
#ifndef _POSIX_MEMLOCK
#define _POSIX_MEMLOCK 200809L
#endif
#ifndef _POSIX_MEMLOCK_RANGE
#define _POSIX_MEMLOCK_RANGE 200809L
#endif
#ifndef _POSIX_MEMORY_PROTECTION
#define _POSIX_MEMORY_PROTECTION 200809L
#endif
#ifndef _POSIX_IPV6
#define _POSIX_IPV6 200809L
#endif
#ifndef _POSIX_MAPPED_FILES
#define _POSIX_MAPPED_FILES 200809L
#endif
#ifndef _POSIX_SHARED_MEMORY_OBJECTS
#define _POSIX_SHARED_MEMORY_OBJECTS 200809L
#endif
#ifndef _POSIX_FSYNC
#define _POSIX_FSYNC 200809L
#endif
#ifndef _POSIX_SYNCHRONIZED_IO
#define _POSIX_SYNCHRONIZED_IO 200809L
#endif

/* Asynchronous I/O is implemented by the libc AIO runtime. */
#ifndef _POSIX_ASYNCHRONOUS_IO
#define _POSIX_ASYNCHRONOUS_IO 200809L
#endif
#ifndef _POSIX_CLOCK_SELECTION
#define _POSIX_CLOCK_SELECTION 200809L
#endif
#ifndef _POSIX_MONOTONIC_CLOCK
#define _POSIX_MONOTONIC_CLOCK 200809L
#endif
#ifndef _POSIX_THREADS
#define _POSIX_THREADS 200809L
#endif
/* The mutex/cond/rwlock/sem/mqueue timed APIs are implemented and host
 * accepted; advertise the TIMEOUTS option they jointly provide. */
#ifndef _POSIX_TIMEOUTS
#define _POSIX_TIMEOUTS 200809L
#endif
#ifndef _POSIX_READER_WRITER_LOCKS
#define _POSIX_READER_WRITER_LOCKS 200809L
#endif
#if defined(RIN_USERSPACE) && \
    (defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || \
     defined(_M_X64))
#ifndef _POSIX_SPIN_LOCKS
#define _POSIX_SPIN_LOCKS 200809L
#endif
#endif
#ifndef _POSIX_THREAD_SAFE_FUNCTIONS
#define _POSIX_THREAD_SAFE_FUNCTIONS 200809L
#endif
#ifndef _POSIX_THREAD_ATTR_STACKSIZE
#define _POSIX_THREAD_ATTR_STACKSIZE 200809L
#endif
#ifndef _POSIX_THREAD_ROBUST_PRIO_INHERIT
#define _POSIX_THREAD_ROBUST_PRIO_INHERIT 200809L
#endif
#if (defined(__x86_64__) || defined(_M_X64)) && \
    (!defined(RIN_FREESTANDING) || defined(RIN_USERSPACE))
#ifndef _POSIX_THREAD_PROCESS_SHARED
#define _POSIX_THREAD_PROCESS_SHARED 200809L
#endif
#endif
#ifndef _POSIX_BARRIERS
#define _POSIX_BARRIERS 200809L
#endif
#if defined(RIN_USERSPACE) && !defined(_POSIX_SPAWN)
#define _POSIX_SPAWN 200809L
#endif

/* Preserve caller-provided syscall hooks before installing defaults.  A
 * hosted unit that supplies a hook is intentionally asking for the Rin
 * frontend even when the compiler reports a hosted environment. */
#if defined(_RIN_UNISTD_SYSCALL0) || defined(_RIN_UNISTD_SYSCALL1) || \
    defined(_RIN_UNISTD_SYSCALL2) || defined(_RIN_UNISTD_SYSCALL3) || \
    defined(_RIN_UNISTD_SYSCALL5)
#define RIN_UNISTD_CUSTOM_SYSCALL_HOOK 1
#endif

#ifndef _RIN_UNISTD_SYSCALL0
#define _RIN_UNISTD_SYSCALL0(number) \
    _syscall0((uintptr_t)(number))
#endif
#ifndef _RIN_UNISTD_SYSCALL1
#define _RIN_UNISTD_SYSCALL1(number, argument1) \
    _syscall1((uintptr_t)(number), (uintptr_t)(argument1))
#endif
#ifndef _RIN_UNISTD_SYSCALL3
#define _RIN_UNISTD_SYSCALL3(number, argument1, argument2, argument3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3))
#endif
#ifndef _RIN_UNISTD_SYSCALL2
#define _RIN_UNISTD_SYSCALL2(number, argument1, argument2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2))
#endif
#ifndef _RIN_UNISTD_SYSCALL5
#define _RIN_UNISTD_SYSCALL5(number, argument1, argument2, argument3, argument4, argument5) \
    _syscall5((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3), \
              (uintptr_t)(argument4), (uintptr_t)(argument5))
#endif

#ifndef _RIN_UNISTD_SLEEP
#define _RIN_UNISTD_SLEEP(milliseconds) \
    _RIN_UNISTD_SYSCALL1(SYS_SLEEP, (uintptr_t)(milliseconds))
#endif

#ifndef _RIN_UNISTD_EXEC_SYSCALL3
#define _RIN_UNISTD_EXEC_SYSCALL3(number, argument1, argument2, argument3) \
    _RIN_UNISTD_SYSCALL3((number), (uintptr_t)(argument1), \
                         (uintptr_t)(argument2), (uintptr_t)(argument3))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* The allocator is linked by the target libc/rincrt image.  Keep these hooks
 * weak so hosted syscall fixtures can include unistd.h without pulling the
 * allocator implementation into the fixture.  They must be declared inside
 * the C-linkage block so C++ callers refer to the libc symbols. */
#if !defined(MIDL_PASS)
extern void rin_user_allocator_after_fork_child(void)
    __attribute__((weak));
extern void rin_user_allocator_before_fork(void) __attribute__((weak));
extern void rin_user_allocator_after_fork_parent(void)
    __attribute__((weak));
extern int rinfs_fork_hooks_prepare(void) __attribute__((weak));
extern int rinfs_fork_hooks_complete(int fork_succeeded)
    __attribute__((weak));
extern int rinfs_fork_prepare(void) __attribute__((weak));
extern int rinfs_fork_complete(int fork_succeeded) __attribute__((weak));
/* The error mapper is optional for compatibility with older RinRuntime. */
extern int rinfs_exec_prepare_all(void) __attribute__((weak));
extern int rinfs_exec_abort_all(void) __attribute__((weak));
extern int rinfs_exec_status_to_errno(int status) __attribute__((weak));
#endif

/* The process/exec frontend is a target-side syscall surface.  Hosted
 * consumers may include this header after a host pthread/process header,
 * whose external declarations own the same POSIX names.  Keep the Rin
 * wrappers for freestanding builds (and explicit RIN_FREESTANDING targets),
 * while letting hosted C runtimes provide their own process ABI. */
#if (defined(RIN_FREESTANDING) && RIN_FREESTANDING) || \
    !defined(__STDC_HOSTED__) || !__STDC_HOSTED__ || \
    defined(RIN_UNISTD_CUSTOM_SYSCALL_HOOK)
#define RIN_UNISTD_TARGET_PROCESS_FRONTEND 1
#endif

#ifndef __RIN_ENVIRON_DECLARED
#define __RIN_ENVIRON_DECLARED
extern char** environ;
#endif

static inline char** __rin_unistd_environment_snapshot(
    unsigned int* epoch_out) {
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING && \
    defined(RIN_USERSPACE) && RIN_USERSPACE
    return __rin_env_read_begin(epoch_out);
#else
    if (epoch_out) *epoch_out = 0u;
    return environ;
#endif
}

static inline void __rin_unistd_environment_release(unsigned int epoch) {
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING && \
    defined(RIN_USERSPACE) && RIN_USERSPACE
    __rin_env_read_end(epoch);
#else
    (void)epoch;
#endif
}

static inline int __rin_unistd_environment_finish(
    int result, unsigned int epoch) {
    __rin_unistd_environment_release(epoch);
    return result;
}

/* ═══════════════════════════════════════════════════════════════
 * 型定義
 * ═══════════════════════════════════════════════════════════════*/

#ifndef MIDL_PASS
static inline intptr_t _rin_unistd_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    return result;
}

static inline int _rin_unistd_status(intptr_t result) {
    result = _rin_unistd_result(result);
    if (result < 0) return -1;
    if (result != 0) {
        errno = (uintptr_t)result > (uintptr_t)INT_MAX ? EOVERFLOW : EIO;
        return -1;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * 定数
 * ═══════════════════════════════════════════════════════════════*/

/* シーク位置 */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* 標準ファイルディスクリプタ */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* アクセスモード */
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

/* ═══════════════════════════════════════════════════════════════
 * sysconf定数
 * ═══════════════════════════════════════════════════════════════*/

#define _SC_ARG_MAX             0   /* execの引数最大長 */
#define _SC_CHILD_MAX           1   /* プロセスあたりの子プロセス最大数 */
#define _SC_CLK_TCK             2   /* クロック刻み/秒 */
#define _SC_NGROUPS_MAX         3   /* グループID最大数 */
#define _SC_OPEN_MAX            4   /* オープンファイル最大数 */
#define _SC_STREAM_MAX          5   /* ストリーム最大数 */
#define _SC_TZNAME_MAX          6   /* タイムゾーン名最大長 */
#define _SC_JOB_CONTROL         7   /* ジョブ制御サポート */
#define _SC_SAVED_IDS           8   /* 保存ID */
#define _SC_REALTIME_SIGNALS    9   /* リアルタイムシグナル */
#define _SC_PRIORITY_SCHEDULING 10  /* 優先度スケジューリング */
#define _SC_TIMERS              11  /* タイマー */
#define _SC_ASYNCHRONOUS_IO     12  /* 非同期I/O */
#define _SC_PRIORITIZED_IO      13  /* 優先度付きI/O */
#define _SC_SYNCHRONIZED_IO     14  /* 同期I/O */
#define _SC_FSYNC               15  /* fsyncサポート */
#define _SC_MAPPED_FILES        16  /* メモリマップドファイル */
#define _SC_MEMLOCK             17  /* メモリロック */
#define _SC_MEMLOCK_RANGE       18  /* メモリロック範囲 */
#define _SC_MEMORY_PROTECTION   19  /* メモリ保護 */
#define _SC_MESSAGE_PASSING     20  /* メッセージパッシング */
#define _SC_SEMAPHORES          21  /* セマフォ */
#define _SC_SHARED_MEMORY_OBJECTS 22 /* 共有メモリ */
#define _SC_AIO_LISTIO_MAX      23  /* AIOリスト最大 */
#define _SC_AIO_MAX             24  /* AIO最大 */
#define _SC_AIO_PRIO_DELTA_MAX  25  /* AIO優先度差最大 */
#define _SC_DELAYTIMER_MAX      26  /* 遅延タイマー最大 */
#define _SC_MQ_OPEN_MAX         27  /* メッセージキュー最大 */
#define _SC_MQ_PRIO_MAX         28  /* メッセージキュー優先度最大 */
#define _SC_VERSION             29  /* POSIXバージョン */
#define _SC_PAGESIZE            30  /* ページサイズ */
#define _SC_PAGE_SIZE           _SC_PAGESIZE
#ifndef _POSIX_CHILD_MAX
#define _POSIX_CHILD_MAX        25
#endif
#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE         0
#endif
#define _SC_RTSIG_MAX           31  /* リアルタイムシグナル最大 */
#define _SC_SEM_NSEMS_MAX       32  /* セマフォ数最大 */
#define _SC_SEM_VALUE_MAX       33  /* セマフォ値最大 */
#define _SC_SIGQUEUE_MAX        34  /* シグナルキュー最大 */
#define _SC_TIMER_MAX           35  /* タイマー最大 */
#define _SC_BC_BASE_MAX         36  /* bc基数最大 */
#define _SC_BC_DIM_MAX          37  /* bc配列次元最大 */
#define _SC_BC_SCALE_MAX        38  /* bcスケール最大 */
#define _SC_BC_STRING_MAX       39  /* bc文字列最大 */
#define _SC_COLL_WEIGHTS_MAX    40  /* 照合重み最大 */
#define _SC_EXPR_NEST_MAX       41  /* 式のネスト最大 */
#define _SC_LINE_MAX            42  /* 行最大長 */
#define _SC_RE_DUP_MAX          43  /* 正規表現繰り返し最大 */
#define _SC_2_VERSION           44  /* POSIX.2バージョン */
#define _SC_2_C_DEV             45  /* C開発サポート */
#define _SC_2_FORT_DEV          46  /* Fortran開発サポート */
#define _SC_2_FORT_RUN          47  /* Fortranランタイム */
#define _SC_2_LOCALEDEF         48  /* localedefサポート */
#define _SC_2_SW_DEV            49  /* ソフトウェア開発 */
#define _SC_NPROCESSORS_CONF    83  /* 設定されたプロセッサ数 */
#define _SC_NPROCESSORS_ONLN    84  /* オンラインプロセッサ数 */
#define _SC_PHYS_PAGES          85  /* 物理ページ数 */
#define _SC_AVPHYS_PAGES        86  /* 利用可能物理ページ数 */
#define _SC_HOST_NAME_MAX       180 /* ホスト名最大長 */
#define _SC_LOGIN_NAME_MAX      71  /* ログイン名最大長 */
#define _SC_TTY_NAME_MAX        72  /* TTY名最大長 */

/* POSIX.1-2008 names added after the original RinOS assignments above.
 * Their values are private to this libc ABI; existing values stay stable. */
#define _SC_2_C_BIND                         1000
#define _SC_2_CHAR_TERM                     1001
#define _SC_2_PBS                           1002
#define _SC_2_PBS_ACCOUNTING                1003
#define _SC_2_PBS_CHECKPOINT                1004
#define _SC_2_PBS_LOCATE                    1005
#define _SC_2_PBS_MESSAGE                   1006
#define _SC_2_PBS_TRACK                     1007
#define _SC_2_UPE                           1008
#define _SC_ADVISORY_INFO                   1009
#define _SC_ATEXIT_MAX                      1010
#define _SC_BARRIERS                        1011
#define _SC_CLOCK_SELECTION                 1012
#define _SC_CPUTIME                         1013
#define _SC_GETGR_R_SIZE_MAX                1014
#define _SC_GETPW_R_SIZE_MAX                1015
#define _SC_IOV_MAX                         1016
#define _SC_IPV6                            1017
#define _SC_MONOTONIC_CLOCK                 1018
#define _SC_READER_WRITER_LOCKS             1019
#define _SC_REGEXP                          1020
#define _SC_SHELL                           1021
#define _SC_SPAWN                           1022
#define _SC_SPIN_LOCKS                      1023
#define _SC_SPORADIC_SERVER                 1024
#define _SC_SS_REPL_MAX                     1025
#define _SC_SYMLOOP_MAX                     1026
#define _SC_THREAD_ATTR_STACKADDR           1027
#define _SC_THREAD_ATTR_STACKSIZE           1028
#define _SC_THREAD_CPUTIME                  1029
#define _SC_THREAD_DESTRUCTOR_ITERATIONS    1030
#define _SC_THREAD_KEYS_MAX                 1031
#define _SC_THREAD_PRIO_INHERIT             1032
#define _SC_THREAD_PRIO_PROTECT             1033
#define _SC_THREAD_PRIORITY_SCHEDULING      1034
#define _SC_THREAD_PROCESS_SHARED           1035
#define _SC_THREAD_ROBUST_PRIO_INHERIT      1036
#define _SC_THREAD_ROBUST_PRIO_PROTECT      1037
#define _SC_THREAD_SAFE_FUNCTIONS           1038
#define _SC_THREAD_SPORADIC_SERVER          1039
#define _SC_THREAD_STACK_MIN                1040
#define _SC_THREAD_THREADS_MAX              1041
#define _SC_THREADS                         1042
#define _SC_TIMEOUTS                        1043
#define _SC_TYPED_MEMORY_OBJECTS            1044
#define _SC_V7_ILP32_OFF32                  1045
#define _SC_V7_ILP32_OFFBIG                 1046
#define _SC_V7_LP64_OFF64                   1047
#define _SC_V7_LPBIG_OFFBIG                 1048
#define _SC_XOPEN_CRYPT                     1049
#define _SC_XOPEN_ENH_I18N                  1050
#define _SC_XOPEN_REALTIME                  1051
#define _SC_XOPEN_REALTIME_THREADS          1052
#define _SC_XOPEN_SHM                       1053
#define _SC_XOPEN_STREAMS                   1054
#define _SC_XOPEN_UNIX                      1055
#define _SC_XOPEN_UUCP                      1056
#define _SC_XOPEN_VERSION                   1057
#define _SC_XOPEN_LEGACY                    1058

/* pathconf/fpathconf定数 */
#define _PC_LINK_MAX            0
#define _PC_MAX_CANON           1
#define _PC_MAX_INPUT           2
#define _PC_NAME_MAX            3
#define _PC_PATH_MAX            4
#define _PC_PIPE_BUF            5
#define _PC_CHOWN_RESTRICTED    6
#define _PC_NO_TRUNC            7
#define _PC_VDISABLE            8
#define _PC_SYNC_IO             9
#define _PC_ASYNC_IO            10
#define _PC_PRIO_IO             11
#define _PC_FILESIZEBITS        13
#define _PC_REC_INCR_XFER_SIZE  14
#define _PC_REC_MAX_XFER_SIZE   15
#define _PC_REC_MIN_XFER_SIZE   16
#define _PC_REC_XFER_ALIGN      17
#define _PC_ALLOC_SIZE_MIN      18
#define _PC_SYMLINK_MAX         19
#define _PC_2_SYMLINKS          20
#define _PC_TIMESTAMP_RESOLUTION 21

/* confstr constants */
#define _CS_PATH                0
#define _CS_V6_WIDTH_RESTRICTED_ENVS 1
#define _CS_XBS5_ILP32_OFF32_CFLAGS 1100
#define _CS_XBS5_ILP32_OFF32_LDFLAGS 1101
#define _CS_XBS5_ILP32_OFF32_LIBS 1102
#define _CS_XBS5_ILP32_OFF32_LINTFLAGS 1103
#define _CS_XBS5_ILP32_OFFBIG_CFLAGS 1104
#define _CS_XBS5_ILP32_OFFBIG_LDFLAGS 1105
#define _CS_XBS5_ILP32_OFFBIG_LIBS 1106
#define _CS_XBS5_ILP32_OFFBIG_LINTFLAGS 1107
#define _CS_XBS5_LP64_OFF64_CFLAGS 1108
#define _CS_XBS5_LP64_OFF64_LDFLAGS 1109
#define _CS_XBS5_LP64_OFF64_LIBS 1110
#define _CS_XBS5_LP64_OFF64_LINTFLAGS 1111
#define _CS_XBS5_LPBIG_OFFBIG_CFLAGS 1112
#define _CS_XBS5_LPBIG_OFFBIG_LDFLAGS 1113
#define _CS_XBS5_LPBIG_OFFBIG_LIBS 1114
#define _CS_XBS5_LPBIG_OFFBIG_LINTFLAGS 1115
#define _CS_POSIX_V6_ILP32_OFF32_CFLAGS 1116
#define _CS_POSIX_V6_ILP32_OFF32_LDFLAGS 1117
#define _CS_POSIX_V6_ILP32_OFF32_LIBS 1118
#define _CS_POSIX_V6_ILP32_OFF32_LINTFLAGS 1119
#define _CS_POSIX_V6_ILP32_OFFBIG_CFLAGS 1120
#define _CS_POSIX_V6_ILP32_OFFBIG_LDFLAGS 1121
#define _CS_POSIX_V6_ILP32_OFFBIG_LIBS 1122
#define _CS_POSIX_V6_ILP32_OFFBIG_LINTFLAGS 1123
#define _CS_POSIX_V6_LP64_OFF64_CFLAGS 1124
#define _CS_POSIX_V6_LP64_OFF64_LDFLAGS 1125
#define _CS_POSIX_V6_LP64_OFF64_LIBS 1126
#define _CS_POSIX_V6_LP64_OFF64_LINTFLAGS 1127
#define _CS_POSIX_V6_LPBIG_OFFBIG_CFLAGS 1128
#define _CS_POSIX_V6_LPBIG_OFFBIG_LDFLAGS 1129
#define _CS_POSIX_V6_LPBIG_OFFBIG_LIBS 1130
#define _CS_POSIX_V6_LPBIG_OFFBIG_LINTFLAGS 1131
#define _CS_POSIX_V6_WIDTH_RESTRICTED_ENVS \
    _CS_V6_WIDTH_RESTRICTED_ENVS
#define _CS_POSIX_V7_ILP32_OFF32_CFLAGS 1132
#define _CS_POSIX_V7_ILP32_OFF32_LDFLAGS 1133
#define _CS_POSIX_V7_ILP32_OFF32_LIBS 1134
#define _CS_POSIX_V7_ILP32_OFF32_LINTFLAGS 1135
#define _CS_POSIX_V7_ILP32_OFFBIG_CFLAGS 1136
#define _CS_POSIX_V7_ILP32_OFFBIG_LDFLAGS 1137
#define _CS_POSIX_V7_ILP32_OFFBIG_LIBS 1138
#define _CS_POSIX_V7_ILP32_OFFBIG_LINTFLAGS 1139
#define _CS_POSIX_V7_LP64_OFF64_CFLAGS 1140
#define _CS_POSIX_V7_LP64_OFF64_LDFLAGS 1141
#define _CS_POSIX_V7_LP64_OFF64_LIBS 1142
#define _CS_POSIX_V7_LP64_OFF64_LINTFLAGS 1143
#define _CS_POSIX_V7_LPBIG_OFFBIG_CFLAGS 1144
#define _CS_POSIX_V7_LPBIG_OFFBIG_LDFLAGS 1145
#define _CS_POSIX_V7_LPBIG_OFFBIG_LIBS 1146
#define _CS_POSIX_V7_LPBIG_OFFBIG_LINTFLAGS 1147
#define _CS_V7_ENV 1148
#define _CS_POSIX_V7_THREADS_CFLAGS 1149
#define _CS_POSIX_V7_THREADS_LDFLAGS 1150
#define _CS_V7_WIDTH_RESTRICTED_ENVS 2
#define _CS_POSIX_V7_WIDTH_RESTRICTED_ENVS \
    _CS_V7_WIDTH_RESTRICTED_ENVS

/* ═══════════════════════════════════════════════════════════════
 * ファイル操作
 * ═══════════════════════════════════════════════════════════════*/

/* Keep scalar I/O requests within the kernel's bounded transfer size. */
#define _RIN_UNISTD_POSITIONAL_IO_MAX 16777216u

static inline void swab(const void* src, void* dest, ssize_t n) {
    const unsigned char* s = (const unsigned char*)src;
    unsigned char* d = (unsigned char*)dest;
    ssize_t i;
    if (n <= 0) return;
    if (!s || !d) {
        errno = EFAULT;
        return;
    }
    for (i = 0; i + 1 < n; i += 2) {
        unsigned char a = s[i];
        d[i] = s[i + 1];
        d[i + 1] = a;
    }
}

static inline ssize_t read(int fd, void* buf, size_t count) {
    if (count > (size_t)SSIZE_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (count > (size_t)_RIN_UNISTD_POSITIONAL_IO_MAX)
        count = (size_t)_RIN_UNISTD_POSITIONAL_IO_MAX;
    __rin_pthread_cancel_point_if_linked();
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL3(
        SYS_READ, (uintptr_t)(intptr_t)fd, (uintptr_t)buf, (uintptr_t)count));
    __rin_pthread_cancel_point_if_linked();
    if (result < 0) return -1;
    if ((uintptr_t)result > (uintptr_t)count) {
        errno = EIO;
        return -1;
    }
    if ((uintptr_t)result > (uintptr_t)SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (ssize_t)result;
}

static inline ssize_t write(int fd, const void* buf, size_t count) {
    if (count > (size_t)SSIZE_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (count > (size_t)_RIN_UNISTD_POSITIONAL_IO_MAX)
        count = (size_t)_RIN_UNISTD_POSITIONAL_IO_MAX;
    __rin_pthread_cancel_point_if_linked();
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL3(
        SYS_WRITE, (uintptr_t)(intptr_t)fd, (uintptr_t)buf, (uintptr_t)count));
    __rin_pthread_cancel_point_if_linked();
    if (result < 0) return -1;
    if ((uintptr_t)result > (uintptr_t)count) {
        errno = EIO;
        return -1;
    }
    if ((uintptr_t)result > (uintptr_t)SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (ssize_t)result;
}

/* The wire ABI keeps a 64-bit offset as two explicit 32-bit words so the
 * same syscall contract is exact on i386 and x86_64.  Large libc requests
 * are split at the kernel's bounded-transfer limit without changing the
 * open-file description position. */
static inline ssize_t _rin_unistd_positional_io(int fd, void const* buffer,
                                                 size_t count, int64_t offset,
                                                 int is_write) {
    uint64_t base_offset;
    size_t total = 0;

    if (offset < 0 || count > (size_t)SSIZE_MAX) {
        errno = EINVAL;
        return -1;
    }
    base_offset = (uint64_t)offset;
    __rin_pthread_cancel_point_if_linked();

    do {
        size_t chunk = count - total;
        uint64_t current_offset;
        intptr_t result;

        if (chunk > _RIN_UNISTD_POSITIONAL_IO_MAX)
            chunk = _RIN_UNISTD_POSITIONAL_IO_MAX;
        if (base_offset > (uint64_t)LLONG_MAX - (uint64_t)total) {
            errno = EOVERFLOW;
            return total ? (ssize_t)total : -1;
        }
        current_offset = base_offset + (uint64_t)total;
        result = _rin_unistd_result(_RIN_UNISTD_SYSCALL5(
            is_write ? SYS_PWRITE : SYS_PREAD,
            (uintptr_t)(intptr_t)fd, (uintptr_t)buffer + total,
            (uintptr_t)chunk, (uintptr_t)(uint32_t)current_offset,
            (uintptr_t)(uint32_t)(current_offset >> 32)));
        __rin_pthread_cancel_point_if_linked();
        if (result < 0)
            return total ? (ssize_t)total : -1;
        if ((uintptr_t)result > (uintptr_t)chunk) {
            errno = EIO;
            return -1;
        }
        total += (size_t)result;
        if ((size_t)result < chunk)
            break;
    } while (total < count);

    return (ssize_t)total;
}

static inline ssize_t pread(int fd, void* buf, size_t count, off_t offset) {
    return _rin_unistd_positional_io(fd, buf, count, offset, 0);
}

static inline ssize_t pwrite(int fd, const void* buf, size_t count,
                             off_t offset) {
    return _rin_unistd_positional_io(fd, buf, count, offset, 1);
}

/* The Rin PREAD/PWRITE wire ABI already carries a 64-bit offset as two
 * 32-bit words on both native and compat32 callers. Keep the large-file
 * spellings independent of off_t so ILP32 callers do not narrow it. */
static inline ssize_t pread64(int fd, void* buf, size_t count,
                              off64_t offset) {
    return _rin_unistd_positional_io(fd, buf, count, (int64_t)offset, 0);
}

static inline ssize_t pwrite64(int fd, const void* buf, size_t count,
                               off64_t offset) {
    return _rin_unistd_positional_io(fd, buf, count, (int64_t)offset, 1);
}

static inline int close(int fd) {
    __rin_pthread_cancel_point_if_linked();
    int rc = _rin_unistd_status(_RIN_UNISTD_SYSCALL1(
        SYS_CLOSE, (uintptr_t)(intptr_t)fd));
    if (rc == 0) {
        _fcntl_track_close_fd(fd);
    }
    __rin_pthread_cancel_point_if_linked();
    return rc;
}

#ifndef CLOSE_RANGE_CLOEXEC
#define CLOSE_RANGE_CLOEXEC RIN_CLOSE_RANGE_FLAG_CLOEXEC
#endif
#ifndef CLOSE_RANGE_UNSHARE
#define CLOSE_RANGE_UNSHARE RIN_CLOSE_RANGE_FLAG_UNSHARE
#endif

static inline int close_range(unsigned int first, unsigned int last,
                              int flags) {
    int result = _rin_unistd_status(_RIN_UNISTD_SYSCALL3(
        SYS_CLOSE_RANGE, (uintptr_t)first, (uintptr_t)last,
        (uintptr_t)(unsigned int)flags));
    if (result == 0 && first < _FCNTL_MAX_FDS) {
        unsigned int end = last < _FCNTL_MAX_FDS
            ? last : _FCNTL_MAX_FDS - 1u;
        _fcntl_init();
        for (unsigned int fd = first; fd <= end; ++fd) {
            if (((unsigned int)flags & RIN_CLOSE_RANGE_FLAG_CLOEXEC) != 0u)
                _fcntl_fd_flags[fd] |= FD_CLOEXEC;
            else
                _fcntl_track_close_fd((int)fd);
        }
    }
    return result;
}

static inline off_t lseek(int fd, off_t offset, int whence) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL3(
        SYS_SEEK, (uintptr_t)(intptr_t)fd, (uintptr_t)(intptr_t)offset,
        (uintptr_t)(intptr_t)whence));
    if (result < 0) return (off_t)-1;
    if ((uintptr_t)result > (uintptr_t)LONG_MAX) {
        errno = EOVERFLOW;
        return (off_t)-1;
    }
    return (off_t)result;
}

/* The legacy SEEK syscall and off_t intentionally retain their word-sized
 * ranges.  LSEEK64 uses an explicit split offset and copies the 64-bit result
 * through a caller-owned output slot on both x86_64 and compat32. */
static inline off64_t lseek64(int fd, off64_t offset, int whence) {
    int64_t result = 0;
    int status = _rin_unistd_status(_RIN_UNISTD_SYSCALL5(
        SYS_LSEEK64, (uintptr_t)(intptr_t)fd,
        (uintptr_t)(uint32_t)(uint64_t)offset,
        (uintptr_t)(uint32_t)((uint64_t)offset >> 32),
        (uintptr_t)(intptr_t)whence, (uintptr_t)&result));
    if (status < 0) return (off64_t)-1;
    return (off64_t)result;
}

static inline int dup(int oldfd) {
    intptr_t raw = _rin_unistd_result(_RIN_UNISTD_SYSCALL1(
        SYS_DUP, (uintptr_t)(intptr_t)oldfd));
    int rc;
    if (raw < 0) return -1;
    if ((uintptr_t)raw > (uintptr_t)INT_MAX) {
        (void)_RIN_UNISTD_SYSCALL1(SYS_CLOSE, (uintptr_t)raw);
        errno = EOVERFLOW;
        return -1;
    }
    rc = (int)raw;
    _fcntl_init();
    if (rc >= 0 && rc < _FCNTL_MAX_FDS && oldfd >= 0 && oldfd < _FCNTL_MAX_FDS) {
        _fcntl_fl_flags[rc] = _fcntl_fl_flags[oldfd];
        _fcntl_fd_flags[rc] = 0;
    }
    return rc;
}

static inline int dup2(int oldfd, int newfd) {
    intptr_t raw = _rin_unistd_result(_RIN_UNISTD_SYSCALL2(
        SYS_DUP2, (uintptr_t)(intptr_t)oldfd, (uintptr_t)(intptr_t)newfd));
    int rc;
    if (raw < 0) return -1;
    if ((uintptr_t)raw > (uintptr_t)INT_MAX) {
        (void)_RIN_UNISTD_SYSCALL1(SYS_CLOSE, (uintptr_t)raw);
        errno = EOVERFLOW;
        return -1;
    }
    rc = (int)raw;
    _fcntl_init();
    if (rc >= 0 && rc < _FCNTL_MAX_FDS && oldfd >= 0 && oldfd < _FCNTL_MAX_FDS) {
        if (oldfd != newfd) {
            _fcntl_fl_flags[rc] = _fcntl_fl_flags[oldfd];
            _fcntl_fd_flags[rc] = 0;
        }
    }
    return rc;
}

static inline int pipe(int pipefd[2]) {
    int rc = _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_PIPE, (uintptr_t)pipefd, 0u));
    if (rc == 0 && pipefd) {
        _fcntl_track_new_fd(pipefd[0], O_RDONLY, 0);
        _fcntl_track_new_fd(pipefd[1], O_WRONLY, 0);
    }
    return rc;
}

static inline int pipe2(int pipefd[2], int flags) {
    int rc = _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_PIPE, (uintptr_t)pipefd, (uintptr_t)(intptr_t)flags));
    if (rc == 0 && pipefd) {
        int status_flags = flags & O_NONBLOCK;
        int fd_flags = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0;
        _fcntl_track_new_fd(pipefd[0], O_RDONLY | status_flags, fd_flags);
        _fcntl_track_new_fd(pipefd[1], O_WRONLY | status_flags, fd_flags);
    }
    return rc;
}

/* ═══════════════════════════════════════════════════════════════
 * ファイルシステム操作
 * ═══════════════════════════════════════════════════════════════*/

static inline int access(const char* pathname, int mode) {
    intptr_t result;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    if ((mode & ~7) != 0) {
        errno = EINVAL;
        return -1;
    }
    result = _RIN_UNISTD_SYSCALL2(SYS_ACCESS, (uintptr_t)pathname,
                                  (uintptr_t)(unsigned int)mode);
    return _rin_unistd_status(result);
}

static inline int chdir(const char* path) {
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL1(SYS_CHDIR, (uintptr_t)path));
}

static inline char* getcwd(char* buf, size_t size) {
    intptr_t ret;
    if (!buf || size == 0u) {
        errno = EINVAL;
        return NULL;
    }
    /* Pointer token: posixize only maps -errno, it never rejects the
     * high-half addresses a 32-bit userspace legitimately returns. */
    ret = __rin_syscall_posixize(_RIN_UNISTD_SYSCALL2(
        SYS_GETCWD, (uintptr_t)buf, (uintptr_t)size));
    if (ret == -1) return NULL;
    /* The kernel returns the target buffer address on success and zero on
     * failure.  Require the exact pointer token so a malformed positive word
     * cannot turn an unsuccessful or unrelated copy-out into success. */
    if ((uintptr_t)ret != (uintptr_t)buf) {
        errno = EIO;
        return NULL;
    }
    return buf;
}

/* readlink is declared here because realpath performs the component walk
 * before the full readlink wrapper section below. */
static inline ssize_t readlink(const char* pathname, char* buf, size_t bufsiz);

/* realpath - canonicalize a target path without relying on a host libc.
 *
 * The kernel exposes lstat/readlink/getcwd as the primitive path operations,
 * so the libc implementation performs the POSIX component walk here.  A
 * symlink is resolved relative to the directory containing the link; the
 * bounded expansion count prevents cyclic links from consuming unbounded
 * memory.  Both caller-provided PATH_MAX storage and the POSIX NULL-output
 * form are supported. */
static inline size_t _rin_realpath_length(const char* value) {
    size_t length = 0u;
    while (value[length] != '\0') ++length;
    return length;
}

static inline int _rin_realpath_copy(char* destination, size_t capacity,
                                     const char* source) {
    size_t length = _rin_realpath_length(source);
    if (length + 1u > capacity) {
        errno = ENAMETOOLONG;
        return -1;
    }
    for (size_t index = 0u; index <= length; ++index)
        destination[index] = source[index];
    return 0;
}

/* Return the authenticated launch/exec image path owned by the kernel.  This
 * is deliberately not implemented through argv[0], a raw PID, or /proc. */
static inline int rin_process_image_path_get(
    RinProcessImagePathResponseV1* response) {
    uint32_t index;
    if (response == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (_rin_unistd_status(_RIN_UNISTD_SYSCALL2(
            SYS_PROCESS_IMAGE_PATH_GET, (uintptr_t)response,
            (uintptr_t)sizeof(*response))) != 0)
        return -1;
    if (response->struct_size != sizeof(*response) ||
        response->version != RIN_PROCESS_IMAGE_PATH_ABI_VERSION ||
        response->flags != RIN_PROCESS_IMAGE_PATH_FLAG_AUTHENTICATED ||
        response->reserved0 != 0u || response->length == 0u ||
        response->length >= sizeof(response->path) ||
        response->path[response->length] != '\0' ||
        response->reserved[0] != 0u || response->reserved[1] != 0u) {
        errno = EIO;
        return -1;
    }
    for (index = 0u; index < response->length; ++index) {
        if (response->path[index] == '\0') {
            errno = EIO;
            return -1;
        }
    }
    return 0;
}

/* Return the kernel-owned current executable allocation range for address
 * ownership queries. The operation is self-only and does not enumerate VMAs. */
static inline int rin_process_image_range_get(
    RinProcessImageRangeV1* response) {
    intptr_t result;
    if (response == NULL) {
        errno = EINVAL;
        return -1;
    }
    result = _rin_unistd_result(_RIN_UNISTD_SYSCALL1(
        SYS_PROCESS_IMAGE_RANGE_GET, (uintptr_t)response));
    if (result < 0) return -1;
    if (result != 0) {
        errno = (uintptr_t)result > (uintptr_t)INT_MAX ? EOVERFLOW : EIO;
        return -1;
    }
    if (response->struct_size != sizeof(*response) ||
        response->version != RIN_PROCESS_IMAGE_RANGE_ABI_VERSION ||
        response->flags != RIN_PROCESS_IMAGE_RANGE_FLAG_AUTHENTICATED ||
        response->image_base == 0u || response->image_size == 0u ||
        response->image_size > UINT64_MAX - response->image_base ||
        response->reserved != 0u) {
        errno = EIO;
        return -1;
    }
    return 0;
}

/* Return a fixed-width, self-only process memory snapshot.  The product
 * kernel derives resident bytes from its authenticated page-accounting owner;
 * there is deliberately no PID/handle argument or host /proc fallback. */
static inline int rin_process_memory_info_get(
    RinProcessMemoryInfoV1* response) {
    intptr_t result;
    if (response == NULL) {
        errno = EINVAL;
        return -1;
    }
    result = _rin_unistd_result(_RIN_UNISTD_SYSCALL1(
        SYS_PROCESS_MEMORY_INFO_GET, (uintptr_t)response));
    if (result < 0) return -1;
    if (result != 0) {
        errno = (uintptr_t)result > (uintptr_t)INT_MAX ? EOVERFLOW : EIO;
        return -1;
    }
    if (response->struct_size != sizeof(*response) ||
        response->version != RIN_PROCESS_MEMORY_INFO_ABI_VERSION ||
        (response->flags & ~RIN_PROCESS_MEMORY_INFO_FLAG_MASK) != 0u ||
        (response->flags & RIN_PROCESS_MEMORY_INFO_FLAG_RESIDENT_VALID) == 0u ||
        response->reserved != 0u) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static inline char* realpath(const char* path, char* resolved_path) {
    enum { RIN_REALPATH_MAX_SYMLINKS = 40, RIN_REALPATH_PENDING_CAPACITY = PATH_MAX * 2 };
    char* pending = NULL;
    char* next_pending = NULL;
    char resolved[PATH_MAX];
    char component[NAME_MAX + 1u];
    char link_target[PATH_MAX];
    char candidate[PATH_MAX];
    size_t cursor;
    size_t resolved_length;
    size_t original_path_length;
    unsigned int symlink_count = 0u;
    int require_directory;

    if (!path || path[0] == '\0') {
        errno = path ? ENOENT : EFAULT;
        return NULL;
    }
    original_path_length = _rin_realpath_length(path);
    require_directory = path[original_path_length - 1u] == '/';

    pending = (char*)malloc(RIN_REALPATH_PENDING_CAPACITY);
    if (!pending) {
        errno = ENOMEM;
        return NULL;
    }

    if (path[0] == '/') {
        if (_rin_realpath_copy(pending, RIN_REALPATH_PENDING_CAPACITY, path) < 0)
            goto fail;
    } else {
        char cwd[PATH_MAX];
        size_t cwd_length;
        size_t path_length = _rin_realpath_length(path);
        if (!getcwd(cwd, sizeof(cwd))) goto fail;
        cwd_length = _rin_realpath_length(cwd);
        if (cwd_length + 1u + path_length + 1u > RIN_REALPATH_PENDING_CAPACITY) {
            errno = ENAMETOOLONG;
            goto fail;
        }
        for (size_t index = 0u; index < cwd_length; ++index)
            pending[index] = cwd[index];
        pending[cwd_length] = '/';
        for (size_t index = 0u; index <= path_length; ++index)
            pending[cwd_length + 1u + index] = path[index];
    }

    resolved[0] = '/';
    resolved[1] = '\0';
    resolved_length = 1u;
    cursor = 0u;

    for (;;) {
        size_t component_length = 0u;
        size_t component_start;
        struct stat status;

        while (pending[cursor] == '/') ++cursor;
        if (pending[cursor] == '\0') break;
        component_start = cursor;
        while (pending[cursor] != '/' && pending[cursor] != '\0') ++cursor;
        component_length = cursor - component_start;
        if (component_length > NAME_MAX) {
            errno = ENAMETOOLONG;
            goto fail;
        }
        for (size_t index = 0u; index < component_length; ++index)
            component[index] = pending[component_start + index];
        component[component_length] = '\0';

        if ((component_length == 1u && component[0] == '.') ||
            (component_length == 2u && component[0] == '.' &&
             component[1] == '.')) {
            if (resolved_length > 1u) {
                if (resolved_length + 1u + component_length + 1u >
                    sizeof(candidate)) {
                    errno = ENAMETOOLONG;
                    goto fail;
                }
                for (size_t index = 0u; index < resolved_length; ++index)
                    candidate[index] = resolved[index];
                candidate[resolved_length] = '/';
                for (size_t index = 0u; index <= component_length; ++index)
                    candidate[resolved_length + 1u + index] = component[index];
            } else {
                if (component_length + 2u > sizeof(candidate)) {
                    errno = ENAMETOOLONG;
                    goto fail;
                }
                candidate[0] = '/';
                for (size_t index = 0u; index <= component_length; ++index)
                    candidate[1u + index] = component[index];
            }
            if (lstat(candidate, &status) < 0) goto fail;
            if (!S_ISDIR(status.st_mode)) {
                errno = ENOTDIR;
                goto fail;
            }
            if (component_length == 1u) continue;
            while (resolved_length > 1u && resolved[resolved_length - 1u] != '/')
                --resolved_length;
            if (resolved_length > 1u) --resolved_length;
            resolved[resolved_length] = '\0';
            continue;
        }

        if (resolved_length > 1u) {
            if (resolved_length + 1u + component_length + 1u > sizeof(candidate)) {
                errno = ENAMETOOLONG;
                goto fail;
            }
            for (size_t index = 0u; index < resolved_length; ++index)
                candidate[index] = resolved[index];
            candidate[resolved_length] = '/';
            for (size_t index = 0u; index <= component_length; ++index)
                candidate[resolved_length + 1u + index] = component[index];
        } else {
            if (component_length + 2u > sizeof(candidate)) {
                errno = ENAMETOOLONG;
                goto fail;
            }
            candidate[0] = '/';
            for (size_t index = 0u; index <= component_length; ++index)
                candidate[1u + index] = component[index];
        }

        if (lstat(candidate, &status) < 0) goto fail;
        if (S_ISLNK(status.st_mode)) {
            ssize_t link_length;
            size_t remaining_length = _rin_realpath_length(pending + cursor);
            size_t prefix_length = (pending[component_start] == '/') ? 0u : resolved_length;
            if (++symlink_count > RIN_REALPATH_MAX_SYMLINKS) {
                errno = ELOOP;
                goto fail;
            }
            link_length = readlink(candidate, link_target, sizeof(link_target));
            if (link_length < 0) goto fail;
            if (link_length == 0) {
                errno = ENOENT;
                goto fail;
            }
            if ((size_t)link_length >= sizeof(link_target)) {
                errno = ENAMETOOLONG;
                goto fail;
            }
            link_target[link_length] = '\0';
            if (link_length > 0 && link_target[link_length - 1] == '/') {
                size_t remaining_cursor = cursor;
                while (pending[remaining_cursor] == '/') ++remaining_cursor;
                if (pending[remaining_cursor] == '\0') require_directory = 1;
            }
            next_pending = (char*)malloc(RIN_REALPATH_PENDING_CAPACITY);
            if (!next_pending) {
                errno = ENOMEM;
                goto fail;
            }
            if (link_target[0] == '/') {
                prefix_length = 0u;
            }
            if (prefix_length + (prefix_length ? 1u : 0u) +
                    (size_t)link_length + (remaining_length ? 1u : 0u) +
                    remaining_length + 1u > RIN_REALPATH_PENDING_CAPACITY) {
                errno = ENAMETOOLONG;
                goto fail;
            }
            {
                size_t output = 0u;
                if (prefix_length != 0u) {
                    for (size_t index = 0u; index < prefix_length; ++index)
                        next_pending[output++] = resolved[index];
                    next_pending[output++] = '/';
                }
                for (size_t index = 0u; index < (size_t)link_length; ++index)
                    next_pending[output++] = link_target[index];
                if (remaining_length != 0u) {
                    next_pending[output++] = '/';
                    for (size_t index = 0u; index < remaining_length; ++index)
                        next_pending[output++] = pending[cursor + index];
                }
                /* Keep the pathname generated by symlink expansion within
                 * PATH_MAX before processing later components such as '..'.
                 * A later normalization must not hide an oversized
                 * intermediate resolution result. */
                if (output >= PATH_MAX) {
                    errno = ENAMETOOLONG;
                    goto fail;
                }
                next_pending[output] = '\0';
            }
            free(pending);
            pending = next_pending;
            next_pending = NULL;
            cursor = 0u;
            continue;
        }

        resolved_length = _rin_realpath_length(candidate);
        if (_rin_realpath_copy(resolved, sizeof(resolved), candidate) < 0) goto fail;
    }

    if (require_directory) {
        struct stat final_status = { 0 };
        if (stat(resolved, &final_status) < 0) goto fail;
        if (!S_ISDIR(final_status.st_mode)) {
            errno = ENOTDIR;
            goto fail;
        }
    }

    free(pending);
    if (resolved_path) {
        if (_rin_realpath_copy(resolved_path, PATH_MAX, resolved) < 0) return NULL;
        return resolved_path;
    }
    resolved_path = (char*)malloc(resolved_length + 1u);
    if (!resolved_path) {
        errno = ENOMEM;
        return NULL;
    }
    _rin_realpath_copy(resolved_path, resolved_length + 1u, resolved);
    return resolved_path;

fail:
    free(next_pending);
    free(pending);
    return NULL;
}

/* mkdir() is defined in sys/stat.h (POSIX standard location) */
#ifndef __MKDIR_DEFINED
#define __MKDIR_DEFINED
static inline int mkdir(const char* pathname, unsigned int mode) {
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_MKDIR, (uintptr_t)pathname, (uintptr_t)mode));
}
#endif

static inline int rmdir(const char* pathname) {
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL1(SYS_RMDIR, (uintptr_t)pathname));
}

static inline int unlink(const char* pathname) {
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL1(SYS_UNLINK, (uintptr_t)pathname));
}

/* rename() は stdio.h で定義 (ANSI C) */

/* ═══════════════════════════════════════════════════════════════
 * プロセス制御
 * ═══════════════════════════════════════════════════════════════*/

#if defined(RIN_UNISTD_TARGET_PROCESS_FRONTEND)
static inline pid_t getpid(void) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL0(SYS_GETPID));
    if (result < 0) return -1;
    if (result == 0) {
        errno = EIO;
        return -1;
    }
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (pid_t)result;
}

/* Return the namespace-visible ID of the calling thread.  The kernel owns
 * the process/thread generation translation; libc only validates the raw
 * result so zero or an out-of-range positive word cannot be exposed as a
 * successful POSIX pid_t. */
static inline pid_t gettid(void) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL0(SYS_GETTID));
    if (result < 0) return (pid_t)-1;
    if (result == 0) {
        errno = EIO;
        return (pid_t)-1;
    }
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return (pid_t)-1;
    }
    return (pid_t)result;
}

static inline pid_t getppid(void) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL0(SYS_GETPPID));
    if (result < 0) return -1;
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (pid_t)result;
}

/* Linux-compatible clear_child_tid registration.  The kernel stores the
 * pointer on the calling thread, clears it, and wakes a futex waiter during
 * thread exit.  NULL disables the exit-time clear/wake operation. */
static inline pid_t set_tid_address(int* clear_child_tid) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL1(
        SYS_set_tid_address, (uintptr_t)clear_child_tid));
    if (result < 0) return (pid_t)-1;
    if (result == 0) {
        errno = EIO;
        return (pid_t)-1;
    }
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return (pid_t)-1;
    }
    return (pid_t)result;
}


/* Session/process-group operations use one fixed-width request/response
 * transaction so native, compat32, and standalone IA-32 portals share the
 * same generation and namespace checks. */
static inline int __rin_process_group_call(uint16_t operation,
                                           pid_t target_pid,
                                           pid_t process_group_id,
                                           RinProcessGroupSyscallResponseV1* response)
{
    RinProcessGroupSyscallRequestV1 request = {
        0u, 0u, 0u, 0u, 0u, 0u, 0u, {0u, 0u, 0u, 0u, 0u}
    };
    intptr_t result;
    if (response == NULL) {
        errno = EFAULT;
        return -1;
    }
    response->struct_size = 0u;
    request.struct_size = sizeof(request);
    request.version = RIN_PROCESS_GROUP_SYSCALL_ABI_VERSION;
    request.operation = operation;
    if (target_pid < 0 || process_group_id < 0) {
        errno = EINVAL;
        return -1;
    }
    request.target_pid = (uint32_t)target_pid;
    request.process_group_id = (uint32_t)process_group_id;
    result = _RIN_UNISTD_SYSCALL2(
        SYS_PROCESS_GROUP_CALL, (uintptr_t)&request,
        (uintptr_t)response);
    return _rin_unistd_status(result);
}

static inline pid_t getpgrp(void)
{
    RinProcessGroupSyscallResponseV1 response;
    if (__rin_process_group_call(RIN_PROCESS_GROUP_OP_GETPGRP, 0, 0,
                                 &response) != 0)
        return (pid_t)-1;
    if (response.process_group_id > (uint32_t)INT_MAX) {
        errno = EOVERFLOW;
        return (pid_t)-1;
    }
    return (pid_t)response.process_group_id;
}

static inline pid_t getpgid(pid_t pid)
{
    RinProcessGroupSyscallResponseV1 response;
    if (__rin_process_group_call(RIN_PROCESS_GROUP_OP_GETPGRP, pid, 0,
                                 &response) != 0)
        return (pid_t)-1;
    if (response.process_group_id > (uint32_t)INT_MAX) {
        errno = EOVERFLOW;
        return (pid_t)-1;
    }
    return (pid_t)response.process_group_id;
}

static inline int setpgid(pid_t pid, pid_t pgid)
{
    RinProcessGroupSyscallResponseV1 response;
    return __rin_process_group_call(RIN_PROCESS_GROUP_OP_SETPGID, pid, pgid,
                                    &response);
}

static inline pid_t getsid(pid_t pid)
{
    RinProcessGroupSyscallResponseV1 response;
    if (__rin_process_group_call(RIN_PROCESS_GROUP_OP_GETSID, pid, 0,
                                 &response) != 0)
        return (pid_t)-1;
    if (response.session_id > (uint32_t)INT_MAX) {
        errno = EOVERFLOW;
        return (pid_t)-1;
    }
    return (pid_t)response.session_id;
}

/* Return the foreground process group attached to this process's controlling
 * terminal.  The kernel verifies the terminal/session relationship and
 * copies the fixed-width process-group ID through the ioctl boundary. */
static inline pid_t tcgetpgrp(int fd)
{
    pid_t process_group_id = 0;
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL3(
        SYS_IOCTL, (uintptr_t)fd, (uintptr_t)TIOCGPGRP,
        (uintptr_t)&process_group_id));
    if (result < 0) return (pid_t)-1;
    if (process_group_id <= 0) {
        errno = EIO;
        return (pid_t)-1;
    }
    return process_group_id;
}

/* Set the foreground process group on this process's controlling terminal.
 * The kernel validates the session/group relationship and applies POSIX
 * background SIGTTOU behavior before changing terminal-owned state. */
static inline int tcsetpgrp(int fd, pid_t process_group_id)
{
    if (process_group_id <= 0) {
        errno = EINVAL;
        return -1;
    }
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL3(
        SYS_IOCTL, (uintptr_t)fd, (uintptr_t)TIOCSPGRP,
        (uintptr_t)&process_group_id));
}

static inline pid_t setsid(void)
{
    RinProcessGroupSyscallResponseV1 response;
    if (__rin_process_group_call(RIN_PROCESS_GROUP_OP_SETSID, 0, 0,
                                 &response) != 0)
        return (pid_t)-1;
    if (response.session_id > (uint32_t)INT_MAX) {
        errno = EOVERFLOW;
        return (pid_t)-1;
    }
    return (pid_t)response.session_id;
}

#if !defined(MIDL_PASS)
__attribute__((noreturn)) static inline void _exit(int status);
static inline int __rin_fork_complete_local(int fork_succeeded,
                                            int rinfs_prepared,
                                            int hooks_prepared) {
    int result = 0;
    if (rinfs_prepared && rinfs_fork_complete != NULL) {
        int complete_result = rinfs_fork_complete(fork_succeeded);
        if (complete_result != 0) result = complete_result;
    }
    if (hooks_prepared && rinfs_fork_hooks_complete != NULL) {
        int complete_result = rinfs_fork_hooks_complete(fork_succeeded);
        if (complete_result != 0 && result == 0) result = complete_result;
    }
    return result;
}
#endif

static inline pid_t fork(void) {
#if !defined(MIDL_PASS)
    int allocator_fork_prepared = 0;
    int fork_hooks_prepared = 0;
    int rinfs_fork_prepared = 0;
#if !defined(RINSDK_SPLIT_BUILD)
    int atfork_handlers_prepared = 0;
    __rin_pthread_atfork_snapshot atfork_snapshot = {0u};
#endif
#endif
    intptr_t result;
#if !defined(MIDL_PASS)
#if !defined(RINSDK_SPLIT_BUILD)
    if (__rin_pthread_atfork_prepare != NULL) {
        __rin_pthread_atfork_prepare(&atfork_snapshot);
        atfork_handlers_prepared = 1;
    }
#endif
    if (rinfs_fork_hooks_prepare != NULL &&
        rinfs_fork_hooks_complete != NULL) {
        if (rinfs_fork_hooks_prepare() != 0) {
#if !defined(RINSDK_SPLIT_BUILD)
            if (atfork_handlers_prepared &&
                __rin_pthread_atfork_parent != NULL)
                __rin_pthread_atfork_parent(&atfork_snapshot);
#endif
            errno = EBUSY;
            return -1;
        }
        fork_hooks_prepared = 1;
    }
    if (rinfs_fork_prepare != NULL && rinfs_fork_complete != NULL) {
        if (rinfs_fork_prepare() != 0) {
            int complete_result = __rin_fork_complete_local(
                0, rinfs_fork_prepared, fork_hooks_prepared);
#if !defined(RINSDK_SPLIT_BUILD)
            if (atfork_handlers_prepared &&
                __rin_pthread_atfork_parent != NULL)
                __rin_pthread_atfork_parent(&atfork_snapshot);
#endif
            errno = complete_result != 0 ? EIO : EBUSY;
            return -1;
        }
        rinfs_fork_prepared = 1;
    }
    if (rin_user_allocator_before_fork != NULL) {
        rin_user_allocator_before_fork();
        allocator_fork_prepared = 1;
    }
#endif
    result = _rin_unistd_result(_RIN_UNISTD_SYSCALL0(SYS_FORK));
    if (result < 0) {
#if !defined(MIDL_PASS)
        int fork_errno = errno;
        int complete_result = __rin_fork_complete_local(
            0, rinfs_fork_prepared, fork_hooks_prepared);
        if (allocator_fork_prepared &&
            rin_user_allocator_after_fork_parent != NULL)
            rin_user_allocator_after_fork_parent();
#if !defined(RINSDK_SPLIT_BUILD)
        if (atfork_handlers_prepared &&
            __rin_pthread_atfork_parent != NULL)
            __rin_pthread_atfork_parent(&atfork_snapshot);
#endif
        errno = complete_result != 0 ? EIO : fork_errno;
#endif
        return -1;
    }
#if !defined(MIDL_PASS)
#if !defined(RINSDK_SPLIT_BUILD)
    if (result == 0 && __rin_pthread_atfork_child_reset != NULL)
        __rin_pthread_atfork_child_reset();
#endif
#endif
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
#if !defined(MIDL_PASS)
        /* The kernel created a child; retain its cloned RinFS owner counts
         * even though this parent cannot represent the returned PID. */
        int complete_result = __rin_fork_complete_local(
            1, rinfs_fork_prepared, fork_hooks_prepared);
        if (allocator_fork_prepared &&
            rin_user_allocator_after_fork_parent != NULL)
            rin_user_allocator_after_fork_parent();
#if !defined(RINSDK_SPLIT_BUILD)
        if (atfork_handlers_prepared &&
            __rin_pthread_atfork_parent != NULL)
            __rin_pthread_atfork_parent(&atfork_snapshot);
#endif
        errno = complete_result != 0 ? EIO : EOVERFLOW;
#else
        errno = EOVERFLOW;
#endif
        return -1;
    }
#if !defined(MIDL_PASS)
    {
        int complete_result = __rin_fork_complete_local(
            1, rinfs_fork_prepared, fork_hooks_prepared);
        if (result == 0 && complete_result != 0) {
            errno = EIO;
            _exit(127);
        }
        if (result == 0 && rin_user_allocator_after_fork_child != NULL)
            rin_user_allocator_after_fork_child();
        else if (allocator_fork_prepared &&
                 rin_user_allocator_after_fork_parent != NULL)
            rin_user_allocator_after_fork_parent();
#if !defined(RINSDK_SPLIT_BUILD)
        if (atfork_handlers_prepared) {
            if (result == 0 && __rin_pthread_atfork_child != NULL)
                __rin_pthread_atfork_child(&atfork_snapshot);
            else if (result != 0 && __rin_pthread_atfork_parent != NULL)
                __rin_pthread_atfork_parent(&atfork_snapshot);
        }
#endif
        /* fork cannot be rolled back after the kernel publishes the child.
         * Preserve the parent's PID result and surface local completion
         * failure in errno; the child exits before reaching user code. */
        if (result != 0 && complete_result != 0) errno = EIO;
    }
#endif
    return (pid_t)result;
}

/* The kernel uses copy-on-write plus a generation-bound parent wait for
 * vfork().  Keep this wrapper free of fork/atfork hooks: the child may only
 * execute or exit before the original parent resumes. */
static inline pid_t vfork(void) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL0(SYS_VFORK));
    if (result < 0) return (pid_t)-1;
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return (pid_t)-1;
    }
    return (pid_t)result;
}

/* Return the generation-stable identity cookie for a process visible in the
 * caller's process namespace.  CoreCLR uses this as its transport/semaphore
 * disambiguation key because RinOS deliberately has no procfs start-time
 * file.  The kernel performs the namespace and generation lookup; libc only
 * validates the fixed-width public ABI and never falls back to a raw PID. */
static inline int rin_process_instance_cookie(uint32_t local_pid,
                                              uint64_t* cookie_out) {
    RinProcessNamespaceInspectRequestV1 request;
    RinProcessNamespaceInspectResponseV1 response;
    intptr_t result;
    uint32_t index;

    if (!cookie_out || local_pid == 0u) {
        errno = EINVAL;
        return -1;
    }
    request.struct_size = sizeof(request);
    request.version = RIN_PROCESS_NAMESPACE_INSPECT_ABI_VERSION;
    request.flags = 0u;
    request.local_pid = local_pid;
    request.reserved0 = 0u;
    for (index = 0u; index < 4u; ++index) request.reserved[index] = 0u;
    for (index = 0u; index < sizeof(response); ++index)
        ((unsigned char*)&response)[index] = 0u;

    result = _rin_unistd_result(_RIN_UNISTD_SYSCALL2(
        SYS_PROCESS_NAMESPACE_INSPECT, (uintptr_t)&request,
        (uintptr_t)&response));
    if (result < 0) return -1;
    if (result != 0 || response.struct_size != sizeof(response) ||
        response.version != RIN_PROCESS_NAMESPACE_INSPECT_ABI_VERSION ||
        response.local_pid != local_pid || response.target_process_id != local_pid ||
        response.target_process_instance_cookie == 0u) {
        errno = EIO;
        return -1;
    }
    *cookie_out = response.target_process_instance_cookie;
    return 0;
}

static inline int execve(const char* pathname, char* const argv[], char* const envp[]) {
    intptr_t result;
    intptr_t normalized_result;
    int exec_hooks_prepared = 0;
    int syscall_errno;
#if !defined(MIDL_PASS)
    int exec_hook_errno;
#endif
    if (!pathname || !argv) {
        errno = EINVAL;
        return -1;
    }
    if (pathname[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
#if !defined(MIDL_PASS)
    /* RinFS uses process-owned block leases. Drop them only after every tracked
     * mount has closed admission and flushed, then restore them if exec fails. */
    if (rinfs_exec_prepare_all || rinfs_exec_abort_all) {
        if (!rinfs_exec_prepare_all || !rinfs_exec_abort_all) {
            errno = EBUSY;
            return -1;
        }
        exec_hook_errno = rinfs_exec_prepare_all();
        if (exec_hook_errno != 0) {
            int mapped_errno = rinfs_exec_status_to_errno
                ? rinfs_exec_status_to_errno(exec_hook_errno) : EBUSY;
            errno = mapped_errno > 0 ? mapped_errno : EIO;
            return -1;
        }
        exec_hooks_prepared = 1;
    }
#endif
    result = _RIN_UNISTD_EXEC_SYSCALL3(
        SYS_EXEC, (uintptr_t)pathname, (uintptr_t)argv, (uintptr_t)envp);
    normalized_result = _rin_unistd_result(result);
    syscall_errno = errno;
#if !defined(MIDL_PASS)
    if (exec_hooks_prepared) {
        exec_hook_errno = rinfs_exec_abort_all();
        if (exec_hook_errno != 0) {
            int mapped_errno = rinfs_exec_status_to_errno
                ? rinfs_exec_status_to_errno(exec_hook_errno) : EIO;
            errno = mapped_errno > 0 ? mapped_errno : EIO;
            return -1;
        }
        errno = syscall_errno;
    }
#else
    (void)exec_hooks_prepared;
    (void)syscall_errno;
#endif
    if (normalized_result >= 0) errno = EIO;
    return -1;
}

static inline int fexecve(int fd, char* const argv[], char* const envp[]) {
    struct stat file_info;
    char target[4096u + 1u];
    /* Same procfs-fd resolution as fstatvfs (helper shared from
     * sys/statvfs.h included above): linked files execute through
     * their resolved path with full execve validation and hooks.
     * Unlinked fds and missing procfs surface as ENOENT; true
     * kernel fd-exec is tracked separately. */
    if (fstat(fd, &file_info) != 0) return -1;
    if (!S_ISREG(file_info.st_mode)) {
        errno = EACCES;
        return -1;
    }
    if (__rin_statvfs_readlink_fd(fd, target, sizeof(target)) < 0)
        return -1;
    return execve(target, argv, envp);
}

#ifndef _RIN_UNISTD_EXECVE
#define _RIN_UNISTD_EXECVE(pathname, arguments, environment) \
    execve((pathname), (arguments), (environment))
#endif

#if defined(RIN_UNISTD_TARGET_PROCESS_FRONTEND)
__attribute__((noreturn)) static inline void _exit(int status) {
    (void)_RIN_UNISTD_SYSCALL1(SYS_EXIT, (uintptr_t)(intptr_t)status);
    for(;;) {} /* Never returns */
}
#endif

#ifndef _RIN_WAIT_DEFINED
#define _RIN_WAIT_DEFINED
#ifndef _RIN_WAIT_SYSCALL3
#define _RIN_WAIT_SYSCALL3(number, argument1, argument2, argument3) \
    _RIN_UNISTD_SYSCALL3((number), (argument1), (argument2), (argument3))
#endif
static inline pid_t wait(int* wstatus) {
    __rin_pthread_cancel_point_if_linked();
    intptr_t result = _rin_unistd_result(_RIN_WAIT_SYSCALL3(
        SYS_WAIT, (uintptr_t)(intptr_t)-1, (uintptr_t)wstatus, 0u));
    __rin_pthread_cancel_point_if_linked();
    if (result < 0) return -1;
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (pid_t)result;
}
#endif /* _RIN_WAIT_DEFINED */

#endif /* RIN_UNISTD_TARGET_PROCESS_FRONTEND */

/* ═══════════════════════════════════════════════════════════════
 * ユーザー/グループ ID
 * ═══════════════════════════════════════════════════════════════*/

#if !defined(RIN_LIBC_NO_ACCOUNT_COMPAT)
static inline int __rin_get_credentials(__rin_credentials_v1* credentials) {
    int error = __rin_credentials_get(credentials);
    if (error != 0) {
        errno = error;
        return -1;
    }
    return 0;
}

static inline uid_t getuid(void) {
    __rin_credentials_v1 credentials;
    return __rin_get_credentials(&credentials) == 0
        ? (uid_t)credentials.uid : (uid_t)-1;
}

static inline uid_t geteuid(void) {
    __rin_credentials_v1 credentials;
    return __rin_get_credentials(&credentials) == 0
        ? (uid_t)credentials.effective_uid : (uid_t)-1;
}

static inline gid_t getgid(void) {
    __rin_credentials_v1 credentials;
    return __rin_get_credentials(&credentials) == 0
        ? (gid_t)credentials.gid : (gid_t)-1;
}

static inline gid_t getegid(void) {
    __rin_credentials_v1 credentials;
    return __rin_get_credentials(&credentials) == 0
        ? (gid_t)credentials.effective_gid : (gid_t)-1;
}

#ifndef __GETGROUPS_DEFINED
#define __GETGROUPS_DEFINED
static inline int getgroups(int size, gid_t list[]) {
    RinSupplementaryGroupsUserV2 request;
    if (size < 0) {
        errno = EINVAL;
        return -1;
    }
    __rin_account_zero(&request, sizeof(request));
    request.struct_size = sizeof(request);
    request.version = __RIN_SUPPLEMENTARY_GROUPS_USER_VERSION;
    request.operation = __RIN_SUPPLEMENTARY_GROUPS_OPERATION_GET;
    request.group_count = (uint32_t)size;
    request.groups_address = (uint64_t)(uintptr_t)list;
    return __rin_group_count_result(_RIN_CREDENTIAL_SYSCALL1(
        RIN_SYS_CREDENTIAL_GROUPS_SET, &request));
}
#endif /* __GETGROUPS_DEFINED */

static inline int __rin_set_credential(uint16_t operation, uint32_t value) {
    __rin_credential_set_v1 request;
    __rin_account_zero(&request, sizeof(request));
    request.struct_size = sizeof(request);
    request.version = __RIN_CREDENTIAL_SET_VERSION;
    request.operation = operation;
    request.value = value;
    return _rin_unistd_status(_RIN_CREDENTIAL_SYSCALL1(
        RIN_SYS_CREDENTIALS_SET, &request));
}

static inline int setuid(uid_t uid) {
#if !defined(RIN_SYS_TYPES_UNSIGNED_ID)
    if ((int64_t)uid < 0) {
        errno = EINVAL;
        return -1;
    }
#else
    (void)uid;
#endif
    return __rin_set_credential(__RIN_CREDENTIAL_SET_UID, (uint32_t)uid);
}

static inline int seteuid(uid_t euid) {
#if !defined(RIN_SYS_TYPES_UNSIGNED_ID)
    if ((int64_t)euid < 0) {
        errno = EINVAL;
        return -1;
    }
#else
    (void)euid;
#endif
    return __rin_set_credential(__RIN_CREDENTIAL_SET_EUID, (uint32_t)euid);
}

static inline int setgid(gid_t gid) {
#if !defined(RIN_SYS_TYPES_UNSIGNED_ID)
    if ((int64_t)gid < 0) {
        errno = EINVAL;
        return -1;
    }
#else
    (void)gid;
#endif
    return __rin_set_credential(__RIN_CREDENTIAL_SET_GID, (uint32_t)gid);
}

static inline int setegid(gid_t egid) {
#if !defined(RIN_SYS_TYPES_UNSIGNED_ID)
    if ((int64_t)egid < 0) {
        errno = EINVAL;
        return -1;
    }
#else
    (void)egid;
#endif
    return __rin_set_credential(__RIN_CREDENTIAL_SET_EGID, (uint32_t)egid);
}
#endif /* !RIN_LIBC_NO_ACCOUNT_COMPAT */

/* ═══════════════════════════════════════════════════════════════
 * スリープ関連
 * ═══════════════════════════════════════════════════════════════*/

static inline int __rin_unistd_monotonic_ns(uint64_t* output) {
    if (!output) return -1;
#if defined(__x86_64__)
    {
        intptr_t result = __rin_syscall_posixize(_RIN_TIME_NS64());
        if (result < 0) {
            if (result != -1) errno = EIO;
            return -1;
        }
        *output = (uint64_t)result;
    }
#else
    *output = 0u;
    {
        intptr_t result = __rin_syscall_posixize(_RIN_TIME_NS32(output));
        if (result < 0) {
            if (result != -1) errno = EIO;
            return -1;
        }
        if (result != 0) {
            errno = EIO;
            return -1;
        }
    }
#endif
    return 0;
}

static inline unsigned int sleep(unsigned int seconds) {
    uint64_t remaining_ns = (uint64_t)seconds * UINT64_C(1000000000);

    if (remaining_ns == 0u) {
        __rin_pthread_cancel_point_if_linked();
        return 0u;
    }

    while (remaining_ns != 0u) {
        uint64_t chunk_ms64;
        uint64_t chunk_ns;
        uint64_t started_ns = 0u;
        uint64_t finished_ns = 0u;
        uint64_t elapsed_ns;
        uint32_t chunk_ms;
        int measurement_errno = errno;
        int have_started =
            __rin_unistd_monotonic_ns(&started_ns) == 0;
        intptr_t result;

        errno = measurement_errno;
        __rin_pthread_cancel_point_if_linked();
        /* SYS_SLEEP accepts uint32 milliseconds on both x86 targets. */
        chunk_ms64 = remaining_ns / UINT64_C(1000000);
        if ((remaining_ns % UINT64_C(1000000)) != 0u) ++chunk_ms64;
        chunk_ms = chunk_ms64 > UINT32_MAX
            ? UINT32_MAX : (uint32_t)chunk_ms64;
        chunk_ns = (uint64_t)chunk_ms * UINT64_C(1000000);
        result = _RIN_UNISTD_SLEEP((unsigned long)chunk_ms);
        __rin_pthread_cancel_point_if_linked();
        if (result != 0) {
            int sleep_errno;
            if (result < 0) {
                if (__rin_syscall_posixize(result) == -1)
                    sleep_errno = errno;
                else
                    sleep_errno = EIO;
            } else {
                sleep_errno = EIO;
            }
            if (have_started &&
                __rin_unistd_monotonic_ns(&finished_ns) == 0 &&
                finished_ns >= started_ns) {
                elapsed_ns = finished_ns - started_ns;
                remaining_ns = elapsed_ns >= remaining_ns
                    ? 0u : remaining_ns - elapsed_ns;
            }
            errno = sleep_errno;
            return (unsigned int)(remaining_ns / UINT64_C(1000000000) +
                ((remaining_ns % UINT64_C(1000000000)) != 0u ? 1u : 0u));
        }
        remaining_ns = chunk_ns >= remaining_ns
            ? 0u : remaining_ns - chunk_ns;
    }
    return 0;
}

static inline int usleep(useconds_t usec) {
    if (usec >= 1000000U) {
        errno = EINVAL;
        return -1;
    }
    struct timespec request;
    request.tv_sec = 0;
    request.tv_nsec = (long)usec * 1000L;
    return nanosleep(&request, 0);
}

/* ═══════════════════════════════════════════════════════════════
 * その他
 * ═══════════════════════════════════════════════════════════════*/

static inline int isatty(int fd) {
    struct termios settings;
    int saved_errno = errno;
    if (tcgetattr(fd, &settings) < 0) return 0;
    errno = saved_errno;
    return 1;
}

/* XSI lockf interface over the process-owned fcntl record-lock ABI. */
#ifndef F_ULOCK
#define F_ULOCK 0
#endif
#ifndef F_LOCK
#define F_LOCK 1
#endif
#ifndef F_TLOCK
#define F_TLOCK 2
#endif
#ifndef F_TEST
#define F_TEST 3
#endif

static inline int lockf(int fd, int function, off_t size) {
    struct flock lock;
    int command;

    if (function != F_LOCK && function != F_TLOCK &&
        function != F_ULOCK && function != F_TEST) {
        errno = EINVAL;
        return -1;
    }

    lock.l_type = function == F_ULOCK ? F_UNLCK : F_WRLCK;
    lock.l_whence = SEEK_CUR;
    lock.l_start = 0;
    lock.l_len = size;
    lock.l_pid = 0;

    if (size < 0) {
        /* A signed minimum cannot be negated to form flock.l_len. */
        const off_t minimum = (off_t)(
            -(off_t)((UINT64_C(1) << (sizeof(off_t) * CHAR_BIT - 1u)) - 1u) -
            1);
        if (size == minimum) {
            errno = EOVERFLOW;
            return -1;
        }
        lock.l_start = size;
        lock.l_len = (off_t)-size;
    }

    if (function == F_TEST) {
        command = F_GETLK;
        if (fcntl(fd, command, &lock) < 0) return -1;
        if (lock.l_type != F_UNLCK) {
            errno = EACCES;
            return -1;
        }
        return 0;
    }

    command = function == F_LOCK ? F_SETLKW : F_SETLK;
    return fcntl(fd, command, &lock);
}

static inline int _rin_unistd_get_system_info_snapshot(
    RinSystemInfoSnapshotV1* information) {
    if (information == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (_rin_unistd_status(_RIN_UNISTD_SYSCALL2(
            RIN_SYS_SYSTEM_INFO_SNAPSHOT_V1, (uintptr_t)information,
            RIN_SYSTEM_INFO_SNAPSHOT_LAYOUT_RIN_V1)) < 0)
        return -1;
    if (information->struct_size != sizeof(*information) ||
        information->version != RIN_SYSTEM_INFO_SNAPSHOT_VERSION_V1 ||
        (information->valid_metrics & RIN_SYSTEM_INFO_SNAPSHOT_MEMORY_VALID) == 0u ||
        (information->valid_metrics & ~RIN_SYSTEM_INFO_SNAPSHOT_DEFINED_MASK) != 0u ||
        information->reserved != 0u || information->total_ram_bytes == 0u ||
        information->available_ram_bytes > information->total_ram_bytes ||
        (information->total_ram_bytes % RIN_SYSTEM_INFO_SNAPSHOT_PAGE_SIZE) != 0u ||
        (information->available_ram_bytes % RIN_SYSTEM_INFO_SNAPSHOT_PAGE_SIZE) != 0u) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static inline long sysconf(int name) {
    switch (name) {
        case _SC_ARG_MAX:
            return ARG_MAX;
        case _SC_CHILD_MAX:
            return CHILD_MAX;
        case _SC_PAGESIZE:
            return 4096;
        case _SC_CLK_TCK:
            return RIN_POSIX_CLK_TCK;
        case _SC_OPEN_MAX:
#if RIN_RESOURCE_LIMIT_PRODUCT_SYSCALL
        {
            struct rlimit limit;
            if (getrlimit(RLIMIT_NOFILE, &limit) < 0) return -1;
            if (limit.rlim_cur > (rlim_t)LONG_MAX) {
                errno = EOVERFLOW;
                return -1;
            }
            return (long)limit.rlim_cur;
        }
#else
            return OPEN_MAX;
#endif
        case _SC_STREAM_MAX:
            return STREAM_MAX;
        case _SC_LINE_MAX:
            return LINE_MAX;
        case _SC_HOST_NAME_MAX:
            return HOST_NAME_MAX;
        case _SC_LOGIN_NAME_MAX:
            return LOGIN_NAME_MAX;
        case _SC_NGROUPS_MAX:
            return NGROUPS_MAX;
        case _SC_TZNAME_MAX:
            return TZNAME_MAX;
        case _SC_TTY_NAME_MAX:
            /* TTY paths /dev/tty0..2 and PTY paths /dev/pts/0..31
             * fit in ttyname()'s 16-byte TLS buffer. */
            return TTY_NAME_MAX;
        case _SC_VERSION:
            return 200809L;
        case _SC_NPROCESSORS_CONF: {
            intptr_t count = _rin_unistd_result(_RIN_UNISTD_SYSCALL0(
                SYS_GETCPU_COUNT));
            if (count < 0) return -1;
            if (count == 0) {
                errno = EIO;
                return -1;
            }
            if ((uintptr_t)count > (uintptr_t)LONG_MAX) {
                errno = EOVERFLOW;
                return -1;
            }
            return (long)count;
        }
        case _SC_NPROCESSORS_ONLN: {
            intptr_t count = _rin_unistd_result(_RIN_UNISTD_SYSCALL0(
                SYS_GETCPU_ONLINE_COUNT));
            if (count < 0) return -1;
            if (count == 0) {
                errno = EIO;
                return -1;
            }
            if ((uintptr_t)count > (uintptr_t)LONG_MAX) {
                errno = EOVERFLOW;
                return -1;
            }
            return (long)count;
        }
        case _SC_PHYS_PAGES: {
            RinSystemInfoSnapshotV1 si;
            uint64_t pages;
            if (_rin_unistd_get_system_info_snapshot(&si) < 0) return -1;
            pages = si.total_ram_bytes / RIN_SYSTEM_INFO_SNAPSHOT_PAGE_SIZE;
            if (pages > (uint64_t)LONG_MAX) {
                errno = EOVERFLOW;
                return -1;
            }
            return (long)pages;
        }
        case _SC_AVPHYS_PAGES: {
            RinSystemInfoSnapshotV1 si;
            uint64_t pages;
            if (_rin_unistd_get_system_info_snapshot(&si) < 0) return -1;
            pages = si.available_ram_bytes / RIN_SYSTEM_INFO_SNAPSHOT_PAGE_SIZE;
            if (pages > (uint64_t)LONG_MAX) {
                errno = EOVERFLOW;
                return -1;
            }
            return (long)pages;
        }
        case _SC_JOB_CONTROL:
            /* Process groups, sessions, controlling TTY ownership and the
             * signal paths used for job control are kernel-owned. */
            return 1;
        case _SC_SAVED_IDS:
            return 1;
        case _SC_MESSAGE_PASSING:
            return _POSIX_MESSAGE_PASSING;
        case _SC_MQ_OPEN_MAX:
            return MQ_OPEN_MAX;
        case _SC_MQ_PRIO_MAX:
            return MQ_PRIO_MAX;
        case _SC_MEMLOCK:
            return _POSIX_MEMLOCK;
        case _SC_ASYNCHRONOUS_IO:
            return _POSIX_ASYNCHRONOUS_IO;
        case _SC_IPV6:
            return _POSIX_IPV6;
        case _SC_AIO_LISTIO_MAX:
            return AIO_LISTIO_MAX;
        case _SC_AIO_MAX:
            return AIO_MAX;
        case _SC_RTSIG_MAX:
            return RTSIG_MAX;
        case _SC_AIO_PRIO_DELTA_MAX:
            return AIO_PRIO_DELTA_MAX;
        case _SC_ATEXIT_MAX:
            return ATEXIT_MAX;
        case _SC_SEM_NSEMS_MAX:
            return SEM_NSEMS_MAX;
        case _SC_SEM_VALUE_MAX:
            return (long)SEM_VALUE_MAX;
        case _SC_DELAYTIMER_MAX:
            return DELAYTIMER_MAX;
        case _SC_TIMER_MAX:
            return TIMER_MAX;
        case _SC_RE_DUP_MAX:
            return RE_DUP_MAX;
        case _SC_SYMLOOP_MAX:
            return SYMLOOP_MAX;
        case _SC_THREAD_DESTRUCTOR_ITERATIONS:
            return PTHREAD_DESTRUCTOR_ITERATIONS;
        case _SC_THREAD_KEYS_MAX:
            return PTHREAD_KEYS_MAX;
        case _SC_THREAD_THREADS_MAX:
            return _POSIX_THREAD_THREADS_MAX;
        case _SC_THREAD_STACK_MIN:
            return PTHREAD_STACK_MIN;
        case _SC_THREAD_ATTR_STACKSIZE:
            return _POSIX_THREAD_ATTR_STACKSIZE;
#if (defined(__x86_64__) || defined(_M_X64)) && \
    (!defined(RIN_FREESTANDING) || defined(RIN_USERSPACE))
        case _SC_THREAD_PROCESS_SHARED:
            return 200809L;
#endif
        case _SC_THREAD_SAFE_FUNCTIONS:
            return _POSIX_THREAD_SAFE_FUNCTIONS;
#ifdef _POSIX_SPIN_LOCKS
        case _SC_SPIN_LOCKS:
            return _POSIX_SPIN_LOCKS;
#endif
        case _SC_THREADS:
        case _SC_READER_WRITER_LOCKS:
        case _SC_BARRIERS:
            return 200809L;
#if defined(RIN_USERSPACE)
        case _SC_SPAWN:
            return 200809L;
#endif
        case _SC_IOV_MAX:
            return IOV_MAX;
#if !defined(RIN_LIBC_NO_ACCOUNT_COMPAT)
        case _SC_GETGR_R_SIZE_MAX: {
            const size_t string_bytes =
                sizeof(((__rin_account_info_v1*)0)->name) + sizeof("x");
            const size_t pointer_size = sizeof(char*);
            const size_t members_offset =
                ((string_bytes + pointer_size - 1u) / pointer_size) *
                pointer_size;
            return (long)(members_offset + 2u * pointer_size);
        }
        case _SC_GETPW_R_SIZE_MAX:
            return (long)(sizeof(((__rin_account_info_v1*)0)->name) +
                          sizeof("x") +
                          sizeof(((__rin_account_info_v1*)0)->display_name) +
                          sizeof(((__rin_account_info_v1*)0)->home_dir) +
                          sizeof(((__rin_account_info_v1*)0)->shell));
#endif
        case _SC_PRIORITIZED_IO:
        case _SC_PRIORITY_SCHEDULING:
        case _SC_REALTIME_SIGNALS:
        case _SC_REGEXP:
        case _SC_SEMAPHORES:
        case _SC_TIMERS:
        case _SC_BC_BASE_MAX:
        case _SC_BC_DIM_MAX:
        case _SC_BC_SCALE_MAX:
        case _SC_BC_STRING_MAX:
        case _SC_COLL_WEIGHTS_MAX:
        case _SC_EXPR_NEST_MAX:
        case _SC_2_VERSION:
        case _SC_2_C_BIND:
        case _SC_2_C_DEV:
        case _SC_2_CHAR_TERM:
        case _SC_2_FORT_DEV:
        case _SC_2_FORT_RUN:
        case _SC_2_LOCALEDEF:
        case _SC_2_PBS:
        case _SC_2_PBS_ACCOUNTING:
        case _SC_2_PBS_CHECKPOINT:
        case _SC_2_PBS_LOCATE:
        case _SC_2_PBS_MESSAGE:
        case _SC_2_PBS_TRACK:
        case _SC_2_SW_DEV:
        case _SC_2_UPE:
        case _SC_ADVISORY_INFO:
        case _SC_CPUTIME:
#if defined(RIN_LIBC_NO_ACCOUNT_COMPAT)
        case _SC_GETGR_R_SIZE_MAX:
        case _SC_GETPW_R_SIZE_MAX:
#endif
        case _SC_SHELL:
#ifndef _POSIX_SPIN_LOCKS
        case _SC_SPIN_LOCKS:
#endif
        case _SC_SPORADIC_SERVER:
        case _SC_SS_REPL_MAX:
#if !defined(RIN_USERSPACE)
        case _SC_SPAWN:
#endif
        case _SC_THREAD_ATTR_STACKADDR:
        case _SC_THREAD_CPUTIME:
        case _SC_THREAD_PRIO_INHERIT:
        case _SC_THREAD_PRIO_PROTECT:
        case _SC_THREAD_PRIORITY_SCHEDULING:
#if (!defined(__x86_64__) && !defined(_M_X64)) || \
    (defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE))
        case _SC_THREAD_PROCESS_SHARED:
#endif
        case _SC_THREAD_ROBUST_PRIO_PROTECT:
        case _SC_THREAD_SPORADIC_SERVER:
        case _SC_TYPED_MEMORY_OBJECTS:
        case _SC_V7_ILP32_OFF32:
        case _SC_V7_ILP32_OFFBIG:
        case _SC_V7_LP64_OFF64:
        case _SC_V7_LPBIG_OFFBIG:
        case _SC_XOPEN_CRYPT:
        case _SC_XOPEN_ENH_I18N:
        case _SC_XOPEN_REALTIME:
        case _SC_XOPEN_REALTIME_THREADS:
        case _SC_XOPEN_SHM:
        case _SC_XOPEN_STREAMS:
        case _SC_XOPEN_UNIX:
        case _SC_XOPEN_UUCP:
        case _SC_XOPEN_VERSION:
        case _SC_XOPEN_LEGACY:
            /* POSIX reserves errno changes for invalid names.  A known but
             * unsupported option returns -1 without changing errno. */
            return -1;
        case _SC_THREAD_ROBUST_PRIO_INHERIT:
            return 200809L;
        case _SC_TIMEOUTS:
            return 200809L;
        case _SC_CLOCK_SELECTION:
        case _SC_MONOTONIC_CLOCK:
        case _SC_SYNCHRONIZED_IO:
        case _SC_FSYNC:
        case _SC_MAPPED_FILES:
        case _SC_SHARED_MEMORY_OBJECTS:
        case _SC_MEMLOCK_RANGE:
        case _SC_MEMORY_PROTECTION:
            return 200809L;
        case _SC_SIGQUEUE_MAX: {
            struct rlimit signal_limit;
            const rlim_t queue_capacity = (rlim_t)64u;
            if (getrlimit(RLIMIT_SIGPENDING, &signal_limit) < 0)
                return -1;
            if (signal_limit.rlim_cur > queue_capacity)
                return (long)queue_capacity;
            return (long)signal_limit.rlim_cur;
        }
        default:
            errno = EINVAL;
            return -1;
    }
}

static inline int getpagesize(void) {
    return 4096;
}

static inline int _rin_pathconf_name_is_known(int name) {
    switch (name) {
        case _PC_LINK_MAX:
        case _PC_MAX_CANON:
        case _PC_MAX_INPUT:
        case _PC_NAME_MAX:
        case _PC_PATH_MAX:
        case _PC_PIPE_BUF:
        case _PC_CHOWN_RESTRICTED:
        case _PC_NO_TRUNC:
        case _PC_VDISABLE:
        case _PC_SYNC_IO:
        case _PC_ASYNC_IO:
        case _PC_PRIO_IO:
        case _PC_FILESIZEBITS:
        case _PC_REC_INCR_XFER_SIZE:
        case _PC_REC_MAX_XFER_SIZE:
        case _PC_REC_MIN_XFER_SIZE:
        case _PC_REC_XFER_ALIGN:
        case _PC_ALLOC_SIZE_MIN:
        case _PC_SYMLINK_MAX:
        case _PC_2_SYMLINKS:
        case _PC_TIMESTAMP_RESOLUTION:
            return 1;
        default:
            return 0;
    }
}

/* A character device is a terminal only when its terminal ioctl contract is
 * present.  Keep the probe's errno and temporary descriptor private. */
static inline int _rin_pathconf_path_is_terminal(const char* path) {
    int saved_errno = errno;
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    int result;
    if (fd < 0) {
        errno = saved_errno;
        return -1;
    }
    result = isatty(fd);
    (void)close(fd);
    errno = saved_errno;
    return result;
}

static inline int _rin_pathconf_fd_is_terminal(int fd) {
    int saved_errno = errno;
    int result = isatty(fd);
    errno = saved_errno;
    return result;
}

static inline long pathconf(const char* path, int name) {
    struct stat status;
    int saved_errno;
    int terminal;
    if (!path) {
        errno = EINVAL;
        return -1;
    }
    if (!_rin_pathconf_name_is_known(name)) {
        errno = EINVAL;
        return -1;
    }
    if (name == _PC_NAME_MAX) {
        struct statvfs filesystem;
        if (statvfs(path, &filesystem) < 0) return -1;
        return (long)filesystem.f_namemax;
    }
    if (stat(path, &status) < 0) return -1;
    switch (name) {
        case _PC_PATH_MAX:
            return PATH_MAX;
        case _PC_PIPE_BUF:
            if (S_ISFIFO(status.st_mode)) return PIPE_BUF;
            return -1;
        case _PC_CHOWN_RESTRICTED:
            return 1;
        case _PC_NO_TRUNC:
            return 1;
        case _PC_MAX_CANON:
        case _PC_MAX_INPUT:
        case _PC_VDISABLE:
            if (!S_ISCHR(status.st_mode)) return -1;
            saved_errno = errno;
            terminal = _rin_pathconf_path_is_terminal(path);
            if (terminal < 0) return -1;
            errno = saved_errno;
            if (!terminal) return -1;
            if (name == _PC_VDISABLE) return _POSIX_VDISABLE;
            /* The terminal ioctl ABI does not publish canonical/input queue
             * limits.  Report an indeterminate limit instead of guessing. */
            return -1;
        case _PC_LINK_MAX:
            /* VFS stat does not expose the backing filesystem's link limit;
             * report it as indeterminate instead of assuming every mount is
             * a writable RinFS volume. */
            return -1;
        case _PC_SYNC_IO:
        case _PC_ASYNC_IO:
        case _PC_PRIO_IO:
        case _PC_FILESIZEBITS:
        case _PC_REC_INCR_XFER_SIZE:
        case _PC_REC_MAX_XFER_SIZE:
        case _PC_REC_MIN_XFER_SIZE:
        case _PC_REC_XFER_ALIGN:
        case _PC_ALLOC_SIZE_MIN:
        case _PC_SYMLINK_MAX:
        case _PC_2_SYMLINKS:
        case _PC_TIMESTAMP_RESOLUTION:
            return -1;
        default:
            errno = EINVAL;
            return -1;
    }
}

static inline long fpathconf(int fd, int name) {
    struct stat status;
    int saved_errno;
    int terminal;
    if (!_rin_pathconf_name_is_known(name)) {
        errno = EINVAL;
        return -1;
    }
    if (name == _PC_NAME_MAX) {
        intptr_t maximum = __rin_syscall_posixize(_syscall1(
            (uintptr_t)RIN_SYS_FILESYSTEM_NAME_MAX_GET_FD, (uintptr_t)fd));
        return maximum < 0 ? -1 : (long)maximum;
    }
    if (fstat(fd, &status) < 0) return -1;
    switch (name) {
        case _PC_PATH_MAX:
            return PATH_MAX;
        case _PC_PIPE_BUF:
            if (S_ISFIFO(status.st_mode)) return PIPE_BUF;
            return -1;
        case _PC_CHOWN_RESTRICTED:
        case _PC_NO_TRUNC:
            return 1;
        case _PC_MAX_CANON:
        case _PC_MAX_INPUT:
        case _PC_VDISABLE:
            if (!S_ISCHR(status.st_mode)) return -1;
            saved_errno = errno;
            terminal = _rin_pathconf_fd_is_terminal(fd);
            errno = saved_errno;
            if (!terminal) return -1;
            if (name == _PC_VDISABLE) return _POSIX_VDISABLE;
            return -1;
        case _PC_LINK_MAX:
            return -1;
        case _PC_SYNC_IO:
        case _PC_ASYNC_IO:
        case _PC_PRIO_IO:
        case _PC_FILESIZEBITS:
        case _PC_REC_INCR_XFER_SIZE:
        case _PC_REC_MAX_XFER_SIZE:
        case _PC_REC_MIN_XFER_SIZE:
        case _PC_REC_XFER_ALIGN:
        case _PC_ALLOC_SIZE_MIN:
        case _PC_SYMLINK_MAX:
        case _PC_2_SYMLINKS:
        case _PC_TIMESTAMP_RESOLUTION:
            return -1;
        default:
            errno = EINVAL;
            return -1;
    }
}

static inline const char* __rin_confstr_model_value(
        int name, int* configured) {
    static const char width_restricted_v6[] =
        "POSIX_V6_ILP32_OFF32\nPOSIX_V6_ILP32_OFFBIG\n"
        "POSIX_V6_LP64_OFF64\nPOSIX_V6_LPBIG_OFFBIG";
    static const char width_restricted_v7[] =
        "POSIX_V7_ILP32_OFF32\nPOSIX_V7_ILP32_OFFBIG\n"
        "POSIX_V7_LP64_OFF64\nPOSIX_V7_LPBIG_OFFBIG";
    static const char i686[] = "--target=i686-unknown-rinos";
    static const char i686_offbig[] =
        "--target=i686-unknown-rinos -D_FILE_OFFSET_BITS=64";
    static const char x86_64[] = "--target=x86_64-unknown-rinos";
    static const char libraries[] = "--dep rincrt.rll";
    if (!configured) return NULL;
    *configured = 1;
    switch (name) {
    case _CS_XBS5_ILP32_OFF32_CFLAGS:
    case _CS_POSIX_V6_ILP32_OFF32_CFLAGS:
    case _CS_POSIX_V7_ILP32_OFF32_CFLAGS:
        return i686;
    case _CS_XBS5_ILP32_OFF32_LDFLAGS:
    case _CS_POSIX_V6_ILP32_OFF32_LDFLAGS:
    case _CS_POSIX_V7_ILP32_OFF32_LDFLAGS:
        return i686;
    case _CS_XBS5_ILP32_OFF32_LIBS:
    case _CS_POSIX_V6_ILP32_OFF32_LIBS:
    case _CS_POSIX_V7_ILP32_OFF32_LIBS:
        return libraries;
    case _CS_XBS5_ILP32_OFFBIG_CFLAGS:
    case _CS_POSIX_V6_ILP32_OFFBIG_CFLAGS:
    case _CS_POSIX_V7_ILP32_OFFBIG_CFLAGS:
        return i686_offbig;
    case _CS_XBS5_ILP32_OFFBIG_LDFLAGS:
    case _CS_POSIX_V6_ILP32_OFFBIG_LDFLAGS:
    case _CS_POSIX_V7_ILP32_OFFBIG_LDFLAGS:
        return i686;
    case _CS_XBS5_ILP32_OFFBIG_LIBS:
    case _CS_POSIX_V6_ILP32_OFFBIG_LIBS:
    case _CS_POSIX_V7_ILP32_OFFBIG_LIBS:
        return libraries;
    case _CS_XBS5_LP64_OFF64_CFLAGS:
    case _CS_POSIX_V6_LP64_OFF64_CFLAGS:
    case _CS_POSIX_V7_LP64_OFF64_CFLAGS:
    case _CS_XBS5_LPBIG_OFFBIG_CFLAGS:
    case _CS_POSIX_V6_LPBIG_OFFBIG_CFLAGS:
    case _CS_POSIX_V7_LPBIG_OFFBIG_CFLAGS:
        return x86_64;
    case _CS_XBS5_LP64_OFF64_LDFLAGS:
    case _CS_POSIX_V6_LP64_OFF64_LDFLAGS:
    case _CS_POSIX_V7_LP64_OFF64_LDFLAGS:
    case _CS_XBS5_LPBIG_OFFBIG_LDFLAGS:
    case _CS_POSIX_V6_LPBIG_OFFBIG_LDFLAGS:
    case _CS_POSIX_V7_LPBIG_OFFBIG_LDFLAGS:
        return x86_64;
    case _CS_XBS5_LP64_OFF64_LIBS:
    case _CS_POSIX_V6_LP64_OFF64_LIBS:
    case _CS_POSIX_V7_LP64_OFF64_LIBS:
    case _CS_XBS5_LPBIG_OFFBIG_LIBS:
    case _CS_POSIX_V6_LPBIG_OFFBIG_LIBS:
    case _CS_POSIX_V7_LPBIG_OFFBIG_LIBS:
        return libraries;
    case _CS_XBS5_ILP32_OFF32_LINTFLAGS:
    case _CS_XBS5_ILP32_OFFBIG_LINTFLAGS:
    case _CS_XBS5_LP64_OFF64_LINTFLAGS:
    case _CS_XBS5_LPBIG_OFFBIG_LINTFLAGS:
    case _CS_POSIX_V6_ILP32_OFF32_LINTFLAGS:
    case _CS_POSIX_V6_ILP32_OFFBIG_LINTFLAGS:
    case _CS_POSIX_V6_LP64_OFF64_LINTFLAGS:
    case _CS_POSIX_V6_LPBIG_OFFBIG_LINTFLAGS:
    case _CS_POSIX_V7_ILP32_OFF32_LINTFLAGS:
    case _CS_POSIX_V7_ILP32_OFFBIG_LINTFLAGS:
    case _CS_POSIX_V7_LP64_OFF64_LINTFLAGS:
    case _CS_POSIX_V7_LPBIG_OFFBIG_LINTFLAGS:
    case _CS_V7_ENV:
        *configured = 0;
        return NULL;
    case _CS_V6_WIDTH_RESTRICTED_ENVS:
        return width_restricted_v6;
    case _CS_V7_WIDTH_RESTRICTED_ENVS:
        return width_restricted_v7;
    case _CS_POSIX_V7_THREADS_CFLAGS:
    case _CS_POSIX_V7_THREADS_LDFLAGS:
        return "";
    default:
        *configured = -1;
        return NULL;
    }
}

static inline size_t confstr(int name, char* buf, size_t len) {
    static const char path[] = "/bin:/sys/bin:/sys/apps";
    const char* value = NULL;
    size_t required = 0u;
    int configured = 1;
    if (name == _CS_PATH) {
        value = path;
        required = sizeof(path);
    } else {
        value = __rin_confstr_model_value(name, &configured);
        if (configured < 0) {
            errno = EINVAL;
            return 0u;
        }
        if (!configured) return 0u;
        if (!value) {
            errno = EINVAL;
            return 0u;
        }
        while (value[required] != '\0') ++required;
        ++required;
    }
    if (len != 0u) {
        size_t copy_count;
        if (!buf) {
            errno = EFAULT;
            return 0u;
        }
        copy_count = required < len ? required : len;
        for (size_t index = 0u; index + 1u < copy_count; ++index)
            buf[index] = value[index];
        buf[copy_count - 1u] = '\0';
    }
    return required;
}

/* gethostname - ホスト名取得 */
static inline int gethostname(char* name, size_t len) {
    RinUtsnameAbiV2 identity;
    size_t size = 0;
    if (!name || len == 0) {
        errno = EINVAL;
        return -1;
    }
    if (_rin_unistd_status(_RIN_UNISTD_SYSCALL1(
            SYS_UNAME_V2, (uintptr_t)&identity)) < 0)
        return -1;
    while (size < RIN_UTSNAME_ABI_V2_FIELD_SIZE && identity.nodename[size])
        ++size;
    if (size == RIN_UTSNAME_ABI_V2_FIELD_SIZE) {
        errno = EIO;
        return -1;
    }
    size_t copy_length = size < len ? size : len - 1u;
    for (size_t index = 0; index < copy_length; ++index)
        name[index] = identity.nodename[index];
    name[copy_length] = '\0';
    return 0;
}

static inline int sethostname(const char* name, size_t len) {
    if (len > (size_t)(RIN_UTSNAME_ABI_V2_FIELD_SIZE - 1u) ||
        (len != 0u && name == NULL)) {
        errno = len != 0u && name == NULL ? EFAULT : EINVAL;
        return -1;
    }
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_SETHOSTNAME, (uintptr_t)name, (uintptr_t)len));
}

/* setdomainname - update the authenticated kernel UTS domain identity. */
static inline int setdomainname(const char* name, size_t len) {
    if (len > (size_t)(RIN_UTSNAME_ABI_V2_FIELD_SIZE - 1u) ||
        (len != 0u && name == NULL)) {
        errno = len != 0u && name == NULL ? EFAULT : EINVAL;
        return -1;
    }
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_SETDOMAINNAME, (uintptr_t)name, (uintptr_t)len));
}

/* getdomainname - ドメイン名取得 */
static inline int getdomainname(char* name, size_t len) {
    RinUtsnameAbiV2 identity;
    size_t size = 0;
    if (!name || len == 0) {
        errno = EINVAL;
        return -1;
    }
    if (_rin_unistd_status(_RIN_UNISTD_SYSCALL1(
            SYS_UNAME_V2, (uintptr_t)&identity)) < 0)
        return -1;
    while (size < RIN_UTSNAME_ABI_V2_FIELD_SIZE && identity.domainname[size])
        ++size;
    if (size == RIN_UTSNAME_ABI_V2_FIELD_SIZE) {
        errno = EIO;
        return -1;
    }
    if (size + 1u > len) {
        errno = EINVAL;
        return -1;
    }
    for (size_t index = 0; index <= size; ++index)
        name[index] = identity.domainname[index];
    return 0;
}

/* ttyname - TTY名取得
 *
 * The kernel's stdio owner exposes an authenticated identity for descriptors
 * 0..2. PTY slave names are accepted only when the namespace path resolves to
 * the same character-device number as the descriptor. */
static inline int _rin_ttyname_resolve(int fd, char* output, size_t capacity) {
    struct stat status = {0};
    struct stat pty_status = {0};
    char pty_path[32];
    const char* name;
    size_t length;
    size_t index;

    if (fd < 0) return EBADF;
    if (fstat(fd, &status) < 0) return errno ? errno : EIO;
    if (!S_ISCHR(status.st_mode)) return ENOTTY;
    if (!isatty(fd)) return ENOTTY;
    if (fd <= 2 && status.st_rdev == (dev_t)fd) {
        name = (fd == 0) ? "/dev/tty0" :
               (fd == 1) ? "/dev/tty1" : "/dev/tty2";
    } else if ((uint64_t)status.st_rdev >= UINT64_C(0x8800)) {
        static const char prefix[] = "/dev/pts/";
        char digits[20];
        uint64_t number = (uint64_t)status.st_rdev - UINT64_C(0x8800);
        size_t prefix_length = sizeof(prefix) - 1u;
        size_t digit_count = 0u;
        do {
            if (digit_count == sizeof(digits)) return ENOTTY;
            digits[digit_count++] = (char)('0' + number % 10u);
            number /= 10u;
        } while (number != 0u);
        for (index = 0u; index < prefix_length; ++index)
            pty_path[index] = prefix[index];
        while (digit_count != 0u)
            pty_path[prefix_length++] = digits[--digit_count];
        if (prefix_length >= sizeof(pty_path)) return ENOTTY;
        pty_path[prefix_length] = '\0';
        if (stat(pty_path, &pty_status) < 0 ||
            !S_ISCHR(pty_status.st_mode) ||
            pty_status.st_rdev != status.st_rdev)
            return ENOTTY;
        name = pty_path;
    } else {
        return ENOTTY;
    }
    length = 0;
    while (name[length] != '\0') ++length;
    if (!output || capacity <= length) return ERANGE;
    for (index = 0; index <= length; ++index) output[index] = name[index];
    return 0;
}

static inline char* ttyname(int fd) {
#if defined(__cplusplus)
    static thread_local char buffer[16];
#else
    static _Thread_local char buffer[16];
#endif
    int saved_errno = errno;
    int error = _rin_ttyname_resolve(fd, buffer, sizeof(buffer));
    if (error != 0) {
        errno = error;
        return NULL;
    }
    errno = saved_errno;
    return buffer;
}

static inline int ttyname_r(int fd, char* buf, size_t buflen) {
    int saved_errno = errno;
    int error;
    if (!buf) return EINVAL;
    if (buflen == 0) return ERANGE;
    error = _rin_ttyname_resolve(fd, buf, buflen);
    errno = saved_errno;
    return error;
}

static inline int getlogin_r(char* name, size_t namesize) {
    int saved_errno = errno;
    intptr_t result;
    int error;
    if (!name) return EINVAL;
    if (namesize == 0u) return ERANGE;
    result = __rin_syscall_posixize(_RIN_UNISTD_SYSCALL2(
        RIN_SYS_GETLOGIN_R, (uintptr_t)name, (uintptr_t)namesize));
    if (result < 0) {
        error = errno;
        errno = saved_errno;
        return error != 0 ? error : EIO;
    }
    errno = saved_errno;
    return result == 0 ? 0 : EIO;
}

static inline char* getlogin(void) {
#if defined(__cplusplus)
    static thread_local char buffer[LOGIN_NAME_MAX];
#else
    static _Thread_local char buffer[LOGIN_NAME_MAX];
#endif
    int saved_errno = errno;
    int error;
    buffer[0] = '\0';
    error = getlogin_r(buffer, sizeof(buffer));
    if (error != 0) {
        errno = error;
        return NULL;
    }
    errno = saved_errno;
    return buffer;
}

/* alarm - アラーム設定 */
static inline unsigned int alarm(unsigned int seconds) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL1(
        SYS_ALARM, (uintptr_t)seconds));
    if (result < 0) return 0;
    if ((uintptr_t)result > (uintptr_t)UINT_MAX) {
        errno = EOVERFLOW;
        return 0;
    }
    return (unsigned int)result;
}

/* pause - シグナルを待つ
 *
 * The syscall blocks the current generation-bound thread until a catchable
 * signal is queued.  A successful return is malformed (pause has no success
 * value), so it is converted to EIO instead of being exposed as success. */
static inline int pause(void) {
    intptr_t result;
    __rin_pthread_cancel_point_if_linked();
    result = _rin_unistd_result(_RIN_UNISTD_SYSCALL0(SYS_PAUSE));
    __rin_pthread_cancel_point_if_linked();
    if (result >= 0) errno = EIO;
    return -1;
}

/* link/symlink - リンク作成 */
static inline int link(const char* oldpath, const char* newpath) {
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_LINK, (uintptr_t)oldpath, (uintptr_t)newpath));
}

static inline int symlink(const char* target, const char* linkpath) {
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_SYMLINK, (uintptr_t)target, (uintptr_t)linkpath));
}

static inline ssize_t readlink(const char* pathname, char* buf, size_t bufsiz) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL3(
        SYS_READLINK, (uintptr_t)pathname, (uintptr_t)buf, (uintptr_t)bufsiz));
    if (result < 0) return -1;
    if ((uintptr_t)result > (uintptr_t)bufsiz) {
        errno = EIO;
        return -1;
    }
    if ((uintptr_t)result > (uintptr_t)SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (ssize_t)result;
}

/* truncate/ftruncate - ファイルサイズ変更 */
static inline int truncate64(const char* path, off64_t length);
static inline int ftruncate64(int fd, off64_t length);

static inline int truncate(const char* path, off_t length) {
#if defined(__x86_64__) || defined(_M_X64)
    return truncate64(path, (off64_t)length);
#else
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_TRUNCATE, (uintptr_t)path, (uintptr_t)(intptr_t)length));
#endif
}

static inline int ftruncate(int fd, off_t length) {
#if defined(__x86_64__) || defined(_M_X64)
    return ftruncate64(fd, (off64_t)length);
#else
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_FTRUNCATE, (uintptr_t)(intptr_t)fd,
        (uintptr_t)(intptr_t)length));
#endif
}

static inline int truncate64(const char* path, off64_t length) {
    uint64_t bits;
    if (length < 0) {
        errno = EINVAL;
        return -1;
    }
    bits = (uint64_t)length;
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL3(
        SYS_TRUNCATE64, (uintptr_t)path, (uintptr_t)(uint32_t)bits,
        (uintptr_t)(uint32_t)(bits >> 32)));
}

static inline int ftruncate64(int fd, off64_t length) {
    uint64_t bits;
    if (length < 0) {
        errno = EINVAL;
        return -1;
    }
    bits = (uint64_t)length;
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL3(
        SYS_FTRUNCATE64, (uintptr_t)(intptr_t)fd,
        (uintptr_t)(uint32_t)bits, (uintptr_t)(uint32_t)(bits >> 32)));
}

/* chown/fchown/lchown - 所有者変更 */
static inline int chown(const char* pathname, uid_t owner, gid_t group) {
    intptr_t result;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    result = _RIN_UNISTD_SYSCALL3(SYS_CHOWN, (uintptr_t)pathname,
                                  (uint32_t)owner, (uint32_t)group);
    return _rin_unistd_status(result);
}

static inline int fchown(int fd, uid_t owner, gid_t group) {
    intptr_t result;
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    result = _RIN_UNISTD_SYSCALL3(SYS_FCHOWN, (uintptr_t)(intptr_t)fd,
                                  (uint32_t)owner, (uint32_t)group);
    return _rin_unistd_status(result);
}

static inline int lchown(const char* pathname, uid_t owner, gid_t group) {
    intptr_t result;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    result = _RIN_UNISTD_SYSCALL3(SYS_LCHOWN, (uintptr_t)pathname,
                                  (uint32_t)owner, (uint32_t)group);
    return _rin_unistd_status(result);
}

/* sync/fsync/fdatasync - 同期 */
static inline void sync(void) {
    (void)_RIN_UNISTD_SYSCALL0(SYS_SYNC);
}

static inline int fsync(int fd) {
    __rin_pthread_cancel_point_if_linked();
    int result = _rin_unistd_status(_RIN_UNISTD_SYSCALL1(
        SYS_FSYNC, (uintptr_t)(intptr_t)fd));
    __rin_pthread_cancel_point_if_linked();
    return result;
}

static inline int syncfs(int fd) {
    __rin_pthread_cancel_point_if_linked();
    int result = _rin_unistd_status(_RIN_UNISTD_SYSCALL1(
        SYS_SYNCFS, (uintptr_t)(intptr_t)fd));
    __rin_pthread_cancel_point_if_linked();
    return result;
}

static inline int fdatasync(int fd) {
    return fsync(fd);
}

/* dup3 - フラグ付きdup2 */
static inline int dup3(int oldfd, int newfd, int flags) {
    intptr_t result;
    if (oldfd == newfd) {
        errno = EINVAL;
        return -1;
    }
    if ((flags & ~O_CLOEXEC) != 0) {
        errno = EINVAL;
        return -1;
    }
    result = _rin_unistd_result(_RIN_UNISTD_SYSCALL3(
        SYS_DUP3, (uintptr_t)(intptr_t)oldfd,
        (uintptr_t)(intptr_t)newfd, (uintptr_t)flags));
    if (result < 0) return -1;
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
        (void)_RIN_UNISTD_SYSCALL1(SYS_CLOSE, (uintptr_t)result);
        errno = EOVERFLOW;
        return -1;
    }
    return (int)result;
}

/* exec frontend.  These bounds match the kernel's pointer-free snapshot. */
#if defined(RIN_UNISTD_TARGET_PROCESS_FRONTEND)
#define __RIN_EXEC_ARGUMENT_COUNT_MAX RIN_EXEC_ARGUMENT_COUNT_MAX
#define __RIN_EXEC_PATH_MAX RIN_EXEC_ARGUMENT_PATH_MAX
#define __RIN_EXEC_ENVIRONMENT_COUNT_MAX RIN_EXEC_ENVIRONMENT_COUNT_MAX
#define __RIN_EXEC_ENVIRONMENT_STRING_MAX RIN_EXEC_ARGUMENT_STRING_MAX

static inline int __rin_exec_collect_arguments(
    const char* first, va_list* arguments,
    char* output[__RIN_EXEC_ARGUMENT_COUNT_MAX + 1u]) {
    const char* current = first;
    size_t count = 0u;
    while (current) {
        if (count == __RIN_EXEC_ARGUMENT_COUNT_MAX) {
            errno = E2BIG;
            return -1;
        }
        output[count++] = (char*)current;
        current = va_arg(*arguments, const char*);
    }
    output[count] = NULL;
    return 0;
}

static inline int __rin_exec_text_length(
    const char* text, size_t limit, size_t* length) {
    size_t index = 0u;
    if (!text || !length) return 0;
    while (index < limit && text[index] != '\0') ++index;
    if (index == limit) return 0;
    *length = index;
    return 1;
}

static inline const char* __rin_exec_search_path(
    char** environment, int* error) {
    size_t entry_index;
    if (error) *error = 0;
    if (!environment) return NULL;
    for (entry_index = 0u;
         entry_index <= __RIN_EXEC_ENVIRONMENT_COUNT_MAX;
         ++entry_index) {
        const char* entry = environment[entry_index];
        size_t length;
        if (!entry) return NULL;
        if (entry_index == __RIN_EXEC_ENVIRONMENT_COUNT_MAX) {
            if (error) *error = E2BIG;
            return NULL;
        }
        if (!__rin_exec_text_length(
                entry, __RIN_EXEC_ENVIRONMENT_STRING_MAX, &length)) {
            if (error) *error = E2BIG;
            return NULL;
        }
        if (length >= 5u && entry[0] == 'P' && entry[1] == 'A' &&
            entry[2] == 'T' && entry[3] == 'H' && entry[4] == '=') {
            return entry + 5u;
        }
    }
    if (error) *error = E2BIG;
    return NULL;
}

static inline int __rin_exec_shell_fallback(
    const char* path, char* const argv[], char** environment) {
    char* shell_arguments[__RIN_EXEC_ARGUMENT_COUNT_MAX + 1u];
    size_t input_count = 0u;
    size_t output_count = 2u;
    if (!argv) {
        errno = EFAULT;
        return -1;
    }
    while (input_count < __RIN_EXEC_ARGUMENT_COUNT_MAX && argv[input_count])
        ++input_count;
    if (input_count == __RIN_EXEC_ARGUMENT_COUNT_MAX || input_count > 63u) {
        errno = E2BIG;
        return -1;
    }
    shell_arguments[0] = (char*)"sh";
    shell_arguments[1] = (char*)path;
    for (size_t index = 1u; index < input_count; ++index)
        shell_arguments[output_count++] = argv[index];
    shell_arguments[output_count] = NULL;
    return _RIN_UNISTD_EXECVE("/bin/sh", shell_arguments, environment);
}

static inline int __rin_execvp(const char* file, char* const argv[]) {
    static const char default_path[] = "/bin:/sys/bin:/sys/apps";
    const char* search_path;
    const char* segment;
    size_t file_length;
    int path_error = 0;
    int saved_error = ENOENT;
    unsigned int environment_epoch = 0u;
    char candidate[__RIN_EXEC_PATH_MAX];
    char** environment;

    if (!file || !argv) {
        errno = EINVAL;
        return -1;
    }
    if (file[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    if (!__rin_exec_text_length(file, __RIN_EXEC_PATH_MAX, &file_length)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    environment = __rin_unistd_environment_snapshot(&environment_epoch);
    for (size_t index = 0u; index < file_length; ++index) {
        if (file[index] == '/') {
            int result = _RIN_UNISTD_EXECVE(file, argv, environment);
            if (result < 0 && errno == ENOEXEC)
                result = __rin_exec_shell_fallback(file, argv, environment);
            return __rin_unistd_environment_finish(
                result, environment_epoch);
        }
    }

    search_path = __rin_exec_search_path(environment, &path_error);
    if (path_error != 0) {
        errno = path_error;
        return __rin_unistd_environment_finish(-1, environment_epoch);
    }
    if (!search_path) search_path = default_path;
    if (!__rin_exec_text_length(
            search_path, __RIN_EXEC_ENVIRONMENT_STRING_MAX, &file_length)) {
        errno = E2BIG;
        return __rin_unistd_environment_finish(-1, environment_epoch);
    }
    segment = search_path;
    for (;;) {
        const char* end = segment;
        size_t directory_length;
        size_t name_length;
        size_t required;
        size_t offset = 0u;
        int result;
        while (*end && *end != ':') ++end;
        directory_length = (size_t)(end - segment);
        if (!__rin_exec_text_length(file, __RIN_EXEC_PATH_MAX, &name_length)) {
            errno = ENAMETOOLONG;
            return __rin_unistd_environment_finish(-1, environment_epoch);
        }
        required = directory_length + (directory_length ? 1u : 0u) +
                   name_length + 1u;
        if (required <= sizeof(candidate)) {
            for (size_t index = 0u; index < directory_length; ++index)
                candidate[offset++] = segment[index];
            if (directory_length) candidate[offset++] = '/';
            for (size_t index = 0u; index <= name_length; ++index)
                candidate[offset++] = file[index];
            result = _RIN_UNISTD_EXECVE(candidate, argv, environment);
            if (result < 0 && errno == ENOEXEC) {
                result = __rin_exec_shell_fallback(
                    candidate, argv, environment);
                return __rin_unistd_environment_finish(
                    result, environment_epoch);
            }
            if (result < 0 && (errno == ENOENT || errno == ENOTDIR)) {
                /* Continue searching. */
            } else if (result < 0 && errno == EACCES) {
                saved_error = EACCES;
            } else {
                return __rin_unistd_environment_finish(
                    result, environment_epoch);
            }
        } else if (saved_error == ENOENT) {
            saved_error = ENAMETOOLONG;
        }
        if (*end == '\0') break;
        segment = end + 1;
    }
    errno = saved_error;
    return __rin_unistd_environment_finish(-1, environment_epoch);
}

static inline int execl(const char* pathname, const char* arg, ...) {
    char* arguments[__RIN_EXEC_ARGUMENT_COUNT_MAX + 1u];
    char** environment;
    unsigned int environment_epoch = 0u;
    va_list list;
    int result;
    if (!pathname || !arg) {
        errno = EINVAL;
        return -1;
    }
    va_start(list, arg);
    result = __rin_exec_collect_arguments(arg, &list, arguments);
    va_end(list);
    if (result != 0) return -1;
    environment = __rin_unistd_environment_snapshot(&environment_epoch);
    result = _RIN_UNISTD_EXECVE(pathname, arguments, environment);
    return __rin_unistd_environment_finish(result, environment_epoch);
}

static inline int execlp(const char* file, const char* arg, ...) {
    char* arguments[__RIN_EXEC_ARGUMENT_COUNT_MAX + 1u];
    va_list list;
    int result;
    if (!file || !arg) {
        errno = EINVAL;
        return -1;
    }
    va_start(list, arg);
    result = __rin_exec_collect_arguments(arg, &list, arguments);
    va_end(list);
    if (result != 0) return -1;
    return __rin_execvp(file, arguments);
}

static inline int execle(const char* pathname, const char* arg, ...) {
    char* arguments[__RIN_EXEC_ARGUMENT_COUNT_MAX + 1u];
    char* const* environment;
    va_list list;
    int result;
    if (!pathname || !arg) {
        errno = EINVAL;
        return -1;
    }
    va_start(list, arg);
    result = __rin_exec_collect_arguments(arg, &list, arguments);
    if (result == 0)
        environment = va_arg(list, char* const*);
    else
        environment = NULL;
    va_end(list);
    if (result != 0) return -1;
    return _RIN_UNISTD_EXECVE(pathname, arguments, environment);
}

static inline int execv(const char* pathname, char* const argv[]) {
    unsigned int environment_epoch = 0u;
    char** environment = __rin_unistd_environment_snapshot(&environment_epoch);
    int result = _RIN_UNISTD_EXECVE(pathname, argv, environment);
    return __rin_unistd_environment_finish(result, environment_epoch);
}

static inline int execvp(const char* file, char* const argv[]) {
    return __rin_execvp(file, argv);
}
#endif /* RIN_UNISTD_TARGET_PROCESS_FRONTEND */

/* POSIX declares getopt() and its state in <unistd.h>.  The implementation
 * is shared with the optional getopt_long() API in getopt.h. */
#include "getopt.h"

/* sbrk - ヒープポインタ操作
 * NOTE: noinline to prevent compiler optimization issues with syscalls */
__attribute__((unused, noinline)) static void* sbrk(long increment) {
    intptr_t result;

    /* The heap break is process-owned kernel state.  Use the relative owner
     * rather than caching a translation-unit-local value and issuing a racy
     * read/modify/write pair through SYS_BRK. */
    /* Pointer token, see getcwd: posixize maps -errno without
     * rejecting the high-half break a 32-bit process may own. */
    result = __rin_syscall_posixize(_RIN_UNISTD_SYSCALL1(
        SYS_SBRK, (uintptr_t)(intptr_t)increment));
    if (result == -1) return (void*)-1;
    return (void*)(uintptr_t)result;
}

#endif /* !MIDL_PASS */

#if defined(__RIN_FILE_OFFSET_BITS64) && !defined(MIDL_PASS)
/* These are header-only libc entry points, so redirect their public spellings
 * to the explicit 64-bit implementations at the call site.  Keeping the
 * redirect after the definitions avoids rewriting their own declarations. */
#define pread pread64
#define pwrite pwrite64
#define lseek lseek64
#define truncate truncate64
#define ftruncate ftruncate64
#endif

#include "sys/splice.h"

#ifdef __cplusplus
}
#endif

#undef RIN_UNISTD_TARGET_PROCESS_FRONTEND

#endif /* _UNISTD_H */
