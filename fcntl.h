/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - fcntl.h
 * ファイル制御
 */

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    defined(__linux__) && (defined(__GNUC__) || defined(__clang__)) && \
    !defined(_RIN_FCNTL_SYSCALL2) && \
    !defined(_RIN_FCNTL_SYSCALL3) && \
    !defined(_RIN_FCNTL_SYSCALL4) && \
    !defined(_RIN_FCNTL_SYSCALL6) && \
    !defined(_RIN_FCNTL_PATH_AT_CALL)
#include_next <fcntl.h>
#else

#ifndef RINOS_LIBC_FCNTL_H
#define RINOS_LIBC_FCNTL_H

/* Publish the descriptor-relative path flags before any dependent header can
 * recurse through stat.h, so path-at users see the same ABI constants as
 * direct fcntl consumers. */
#ifndef AT_FDCWD
#define AT_FDCWD            (-100) /* カレントディレクトリ */
#endif
#ifndef AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_NOFOLLOW 0x100  /* シンボリックリンクを辿らない */
#endif
#ifndef AT_REMOVEDIR
#define AT_REMOVEDIR        0x200  /* ディレクトリを削除 */
#endif
#ifndef AT_SYMLINK_FOLLOW
#define AT_SYMLINK_FOLLOW   0x400  /* シンボリックリンクを辿る */
#endif
#ifndef AT_NO_AUTOMOUNT
#define AT_NO_AUTOMOUNT     0x800  /* 自動マウントしない */
#endif
#ifndef AT_EMPTY_PATH
#define AT_EMPTY_PATH       0x1000 /* 空パスを許可 */
#endif
#ifndef AT_EACCESS
#define AT_EACCESS          0x200  /* 実効IDでアクセスチェック */
#endif

#include "stddef.h"
#include "stdarg.h"
#include "errno.h"
#include "limits.h"
#include "fcntl_flags.h"
#include "sys/syscall.h"
#include "sys/types.h"
#include "internal/pthread_cancel_point.h"
#include <rin/fs/path_at_abi.h>
#include <rin/fs/fcntl_lock_abi.h>

#ifndef MIDL_PASS
#ifndef _RIN_FCNTL_SYSCALL3
#define _RIN_FCNTL_SYSCALL3(number, arg1, arg2, arg3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(arg1), (uintptr_t)(arg2), \
              (uintptr_t)(arg3))
#endif
#ifndef _RIN_FCNTL_SYSCALL4
#define _RIN_FCNTL_SYSCALL4(number, arg1, arg2, arg3, arg4) \
    _syscall4((uintptr_t)(number), (uintptr_t)(arg1), (uintptr_t)(arg2), \
              (uintptr_t)(arg3), (uintptr_t)(arg4))
#endif
#ifndef _RIN_FCNTL_SYSCALL2
#define _RIN_FCNTL_SYSCALL2(number, arg1, arg2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(arg1), (uintptr_t)(arg2))
#endif
#ifndef _RIN_FCNTL_SYSCALL6
#define _RIN_FCNTL_SYSCALL6(number, arg1, arg2, arg3, arg4, arg5, arg6) \
    _syscall6((uintptr_t)(number), (uintptr_t)(arg1), (uintptr_t)(arg2), \
              (uintptr_t)(arg3), (uintptr_t)(arg4), (uintptr_t)(arg5), \
              (uintptr_t)(arg6))
#endif
#ifndef _RIN_FCNTL_PATH_AT_CALL
#define _RIN_FCNTL_PATH_AT_CALL(call) \
    _syscall1((uintptr_t)SYS_PATH_AT, (uintptr_t)(call))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * fcntlコマンド
 * ═══════════════════════════════════════════════════════════════*/

#define F_DUPFD     0   /* ファイルディスクリプタを複製 */
#define F_GETFD     1   /* ファイルディスクリプタフラグを取得 */
#define F_SETFD     2   /* ファイルディスクリプタフラグを設定 */
#define F_GETFL     3   /* ファイル状態フラグを取得 */
#define F_SETFL     4   /* ファイル状態フラグを設定 */
#define F_GETLK     5   /* ロックを取得 */
#define F_SETLK     6   /* ロックを設定 */
#define F_SETLKW    7   /* ロックを設定 (ブロッキング) */
#define F_SETPIPE_SZ 1031 /* Change pipe capacity */
#define F_GETPIPE_SZ 1032 /* Query pipe capacity */

/* ファイルディスクリプタフラグ */
#define FD_CLOEXEC  1   /* exec時にクローズ */
#define FD_CLOFORK  2   /* fork時に子へ継承しない */

/* ロックタイプ */
#define F_RDLCK     0   /* 読み取りロック */
#define F_WRLCK     1   /* 書き込みロック */
#define F_UNLCK     2   /* ロック解除 */

/* ═══════════════════════════════════════════════════════════════
 * ロック構造体
 * ═══════════════════════════════════════════════════════════════*/

struct flock {
    short l_type;   /* ロックタイプ: F_RDLCK, F_WRLCK, F_UNLCK */
    short l_whence; /* 開始位置: SEEK_SET, SEEK_CUR, SEEK_END */
    off_t l_start;  /* オフセット */
    off_t l_len;    /* 長さ (0 = EOF まで) */
    int   l_pid;    /* プロセスID */
};

#define F_GETLK64   12  /* 64ビット範囲のロック照会 */
#define F_SETLK64   13  /* 64ビット範囲のロック設定 */
#define F_SETLKW64  14  /* 64ビット範囲のロック設定 (ブロッキング) */
#define F_OFD_GETLK64   1024 /* 64ビット範囲のOFDロック照会 */
#define F_OFD_SETLK64   1025 /* 64ビット範囲のOFDロック設定 */
#define F_OFD_SETLKW64  1026 /* 64ビット範囲のOFDロック設定 (ブロッキング) */

/* ═══════════════════════════════════════════════════════════════
 * fcntl追加コマンド
 * ═══════════════════════════════════════════════════════════════*/

#define F_DUPFD_CLOEXEC 1030 /* F_DUPFDと同様だがCLOEXECを設定 */
#define F_DUPFD_CLOFORK 0x52494E01 /* RinOS private CLOFORK extension */
#define F_SETOWN        8    /* シグナル送信先プロセスを設定 */
#define F_GETOWN        9    /* シグナル送信先プロセスを取得 */
#define F_SETSIG        10   /* シグナル番号を設定 */
#define F_GETSIG        11   /* シグナル番号を取得 */
#define F_SETOWN_EX     15   /* 拡張: シグナル送信先を設定 */
#define F_GETOWN_EX     16   /* 拡張: シグナル送信先を取得 */
#define F_OFD_GETLK     36   /* オープンファイルディスクリプタロック取得 */
#define F_OFD_SETLK     37   /* オープンファイルディスクリプタロック設定 */
#define F_OFD_SETLKW    38   /* オープンファイルディスクリプタロック設定(ブロッキング) */
/* RinOS BSD flock locks use a separate OFD-owned conflict namespace. */
#define F_RIN_FLOCK_SETLK  0x52464c01
#define F_RIN_FLOCK_SETLKW 0x52464c02

/* ═══════════════════════════════════════════════════════════════
 * flock構造体 (64ビット版)
 * ═══════════════════════════════════════════════════════════════*/

struct flock64 {
    short  l_type;   /* ロックタイプ */
    short  l_whence; /* 開始位置 */
    off64_t l_start; /* オフセット */
    off64_t l_len;   /* 長さ */
    pid_t  l_pid;    /* プロセスID */
};

/* ═══════════════════════════════════════════════════════════════
 * f_owner_ex構造体
 * ═══════════════════════════════════════════════════════════════*/

struct f_owner_ex {
    int type;   /* F_OWNER_TID, F_OWNER_PID, F_OWNER_PGRP */
    pid_t pid;  /* プロセス/スレッドID */
};

#define F_OWNER_TID  0
#define F_OWNER_PID  1
#define F_OWNER_PGRP 2

/* ═══════════════════════════════════════════════════════════════
 * posix_fadvise アドバイス
 * ═══════════════════════════════════════════════════════════════*/

#define POSIX_FADV_NORMAL     0
#define POSIX_FADV_RANDOM     1
#define POSIX_FADV_SEQUENTIAL 2
#define POSIX_FADV_WILLNEED   3
#define POSIX_FADV_DONTNEED   4
#define POSIX_FADV_NOREUSE    5

/* Linux fallocate(2) mode flags. Modes that the selected filesystem cannot
 * perform are reported by the kernel as EOPNOTSUPP. */
#ifndef FALLOC_FL_KEEP_SIZE
#define FALLOC_FL_KEEP_SIZE       0x01
#endif
#ifndef FALLOC_FL_PUNCH_HOLE
#define FALLOC_FL_PUNCH_HOLE      0x02
#endif
#ifndef FALLOC_FL_NO_HIDE_STALE
#define FALLOC_FL_NO_HIDE_STALE   0x04
#endif
#ifndef FALLOC_FL_COLLAPSE_RANGE
#define FALLOC_FL_COLLAPSE_RANGE  0x08
#endif
#ifndef FALLOC_FL_ZERO_RANGE
#define FALLOC_FL_ZERO_RANGE      0x10
#endif
#ifndef FALLOC_FL_INSERT_RANGE
#define FALLOC_FL_INSERT_RANGE    0x20
#endif
#ifndef FALLOC_FL_UNSHARE_RANGE
#define FALLOC_FL_UNSHARE_RANGE   0x40
#endif

/* ═══════════════════════════════════════════════════════════════
 * AT_* フラグ (xxxat系関数用)
 * ═══════════════════════════════════════════════════════════════*/

/* ═══════════════════════════════════════════════════════════════
 * ファイルディスクリプタ状態保持 (簡易実装)
 * ═══════════════════════════════════════════════════════════════*/

#define _FCNTL_MAX_FDS FD_SETSIZE
/* The legacy descriptor-cache data symbols remain exported by the SDK v1
 * runtime for binary compatibility. New headers intentionally expose no
 * cache API: all descriptor state is obtained from the kernel owner. */

/* Syscall results travel in the target-width intptr_t carrier.  Never route
 * a successful descriptor result through host `long`/`int` before checking its
 * range: LLP64 hosts make `long` only 32 bits while a valid Rin descriptor is
 * still target-word sized.  Unknown negative values are not POSIX errno
 * encodings and must fail closed instead of becoming a wrapped descriptor. */
static inline int _fcntl_result_to_int(intptr_t raw_result) {
    if (raw_result < 0) {
        if (raw_result >= (intptr_t)-4095) {
            errno = (int)-raw_result;
        } else {
            errno = EIO;
        }
        return -1;
    }
    if ((uintptr_t)raw_result > (uintptr_t)INT32_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)raw_result;
}

static inline void _fcntl_cancel_close_fd(void* argument) {
    const int fd = argument ? *(const int*)argument : -1;
    if (fd < 0) return;
    (void)_syscall1((uintptr_t)SYS_CLOSE, (uintptr_t)(intptr_t)fd);
}

static inline int _fcntl_result_status_zero(intptr_t raw_result) {
    if (raw_result < 0) {
        if (raw_result >= (intptr_t)-4095) {
            errno = (int)-raw_result;
        } else {
            errno = EIO;
        }
        return -1;
    }
    if (raw_result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static inline int _fcntl_lock64_wire(
    int fd, int command, RinNativeFlockAbi* wire) {
    return _fcntl_result_status_zero(_RIN_FCNTL_SYSCALL3(
        SYS_FCNTL_LOCK64, (uintptr_t)fd, (uintptr_t)command,
        (uintptr_t)wire));
}

static inline int _fcntl_lock64_get_command(int command) {
    return command == F_GETLK || command == F_OFD_GETLK ||
           command == F_GETLK64 || command == F_OFD_GETLK64;
}

static inline int _fcntl_lock64_flock(int fd, int command,
                                      struct flock* lock) {
    RinNativeFlockAbi wire = {0};
    int kernel_command = command;
    int result;
    if (!lock) {
        errno = EFAULT;
        return -1;
    }
    if (command == F_GETLK64) kernel_command = F_GETLK;
    else if (command == F_SETLK64) kernel_command = F_SETLK;
    else if (command == F_SETLKW64) kernel_command = F_SETLKW;
    else if (command == F_OFD_GETLK64) kernel_command = F_OFD_GETLK;
    else if (command == F_OFD_SETLK64) kernel_command = F_OFD_SETLK;
    else if (command == F_OFD_SETLKW64) kernel_command = F_OFD_SETLKW;
    wire.type = (int16_t)lock->l_type;
    wire.whence = (int16_t)lock->l_whence;
    wire.start = (int64_t)lock->l_start;
    wire.length = (int64_t)lock->l_len;
    wire.pid = (int32_t)lock->l_pid;
    result = _fcntl_lock64_wire(fd, kernel_command, &wire);
    if (result == 0 && _fcntl_lock64_get_command(command)) {
        if (sizeof(off_t) < sizeof(int64_t) &&
            (wire.start < (int64_t)INT_MIN ||
             wire.start > (int64_t)INT_MAX ||
             wire.length < (int64_t)INT_MIN ||
             wire.length > (int64_t)INT_MAX)) {
            errno = EOVERFLOW;
            return -1;
        }
        lock->l_type = wire.type;
        lock->l_whence = wire.whence;
        lock->l_start = (off_t)wire.start;
        lock->l_len = (off_t)wire.length;
        lock->l_pid = wire.pid;
    }
    return result;
}

static inline int _fcntl_lock64_flock64(int fd, int command,
                                        struct flock64* lock) {
    RinNativeFlockAbi wire = {0};
    int kernel_command = command;
    int result;
    if (!lock) {
        errno = EFAULT;
        return -1;
    }
    if (command == F_GETLK64) kernel_command = F_GETLK;
    else if (command == F_SETLK64) kernel_command = F_SETLK;
    else if (command == F_SETLKW64) kernel_command = F_SETLKW;
    else if (command == F_OFD_GETLK64) kernel_command = F_OFD_GETLK;
    else if (command == F_OFD_SETLK64) kernel_command = F_OFD_SETLK;
    else if (command == F_OFD_SETLKW64) kernel_command = F_OFD_SETLKW;
    wire.type = (int16_t)lock->l_type;
    wire.whence = (int16_t)lock->l_whence;
    wire.start = (int64_t)lock->l_start;
    wire.length = (int64_t)lock->l_len;
    wire.pid = (int32_t)lock->l_pid;
    result = _fcntl_lock64_wire(fd, kernel_command, &wire);
    if (result == 0 && _fcntl_lock64_get_command(command)) {
        lock->l_type = wire.type;
        lock->l_whence = wire.whence;
        lock->l_start = (off64_t)wire.start;
        lock->l_len = (off64_t)wire.length;
        lock->l_pid = wire.pid;
    }
    return result;
}

static inline int _fcntl_dupfd_at_least(int fd, int minfd, int command) {
    int result;

    if (minfd < 0) {
        errno = EINVAL;
        return -1;
    }

    result = _fcntl_result_to_int(_RIN_FCNTL_SYSCALL3(
        SYS_FCNTL, (uintptr_t)fd, (uintptr_t)command, (uintptr_t)minfd));
    return result;
}

/* ═══════════════════════════════════════════════════════════════
 * 関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int open(const char* pathname, int flags, ...) {
    unsigned int mode = 0644;  /* デフォルト */
    __rin_pthread_cancel_point_if_linked();

    /* O_CREAT/O_TMPFILE creation modes come from the variadic argument. */
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, unsigned int);
        va_end(ap);
    }

    int fd = _fcntl_result_to_int(_RIN_FCNTL_SYSCALL3(
        (uintptr_t)SYS_OPEN, (uintptr_t)pathname, (uintptr_t)flags,
        (uintptr_t)mode));

    if (fd < 0) {
        __rin_pthread_cancel_point_if_linked();
    } else {
        __rin_pthread_cancel_point_cleanup_if_linked(_fcntl_cancel_close_fd,
                                                      &fd);
    }

    return fd;
}

static inline void _fcntl_path_at_init(RinPathAtCallV1* call,
                                       unsigned int operation) {
    size_t index;
    for (index = 0u; index < sizeof(*call); ++index)
        ((unsigned char*)call)[index] = 0u;
    call->struct_size = (uint32_t)sizeof(*call);
    call->version = RIN_PATH_AT_CALL_VERSION;
    call->operation = (uint16_t)operation;
    call->dirfd = AT_FDCWD;
    call->secondary_dirfd = AT_FDCWD;
}

static inline intptr_t _fcntl_path_at_value(RinPathAtCallV1* call) {
    intptr_t result = _RIN_FCNTL_PATH_AT_CALL(call);
    if (result < 0) {
        errno = result >= -4095L ? (int)-result : EIO;
        return -1;
    }
    return result;
}

static inline int _fcntl_path_at_zero(RinPathAtCallV1* call) {
    intptr_t result = _fcntl_path_at_value(call);
    if (result < 0) return -1;
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

/* All descriptor-relative pathname operations use a single fixed-width ABI.
 * This keeps path joining, directory-FD validation, and absolute-path
 * handling inside the kernel rather than widening a capability in libc. */
static inline int openat(int dirfd, const char* pathname, int flags, ...) {
    RinPathAtCallV1 call;
    unsigned int mode = 0644u;
    intptr_t result;

    __rin_pthread_cancel_point_if_linked();
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    if (pathname[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, unsigned int);
        va_end(ap);
    }
    _fcntl_path_at_init(&call, RIN_PATH_AT_OPEN);
    call.dirfd = dirfd;
    call.open_flags = (uint32_t)flags;
    call.mode = mode;
    call.path1 = (uint64_t)(uintptr_t)pathname;
    result = _fcntl_path_at_value(&call);
    if (result < 0 || (uintptr_t)result > (uintptr_t)INT32_MAX) {
        if (result >= 0) errno = EOVERFLOW;
        __rin_pthread_cancel_point_if_linked();
        return -1;
    }
    {
        int fd = (int)result;
        __rin_pthread_cancel_point_cleanup_if_linked(_fcntl_cancel_close_fd,
                                                      &fd);
    }
    return (int)result;
}

/* These are intentionally separate from POSIX openat()/mkdirat(). They are
 * used by WASI preopens, where the kernel must resolve the path underneath
 * the supplied directory descriptor rather than accepting normal `*at`
 * pathname semantics. */
static inline int rin_openat_beneath(int dirfd, const char* pathname, int flags,
                                     mode_t mode) {
    intptr_t result;

    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    if (pathname[0] == '\0') {
        errno = ENOENT;
        return -1;
    }

    result = _RIN_FCNTL_SYSCALL4(
        SYS_OPENAT_BENEATH, (uintptr_t)dirfd, (uintptr_t)pathname,
        (uintptr_t)flags, (uintptr_t)mode);
    result = _fcntl_result_to_int(result);
    if (result < 0) return -1;
    return (int)result;
}

static inline int creat(const char* pathname, mode_t mode) {
    return open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

static inline int fcntl(int fd, int cmd, ...) {
    va_list ap;
    va_start(ap, cmd);

    int result = -1;

    switch (cmd) {
        case F_DUPFD: {
            int minfd = va_arg(ap, int);
            result = _fcntl_dupfd_at_least(fd, minfd, F_DUPFD);
            break;
        }

        case F_DUPFD_CLOEXEC: {
            int minfd = va_arg(ap, int);
            result = _fcntl_dupfd_at_least(fd, minfd, F_DUPFD_CLOEXEC);
            break;
        }

        case F_DUPFD_CLOFORK: {
            int minfd = va_arg(ap, int);
            result = _fcntl_dupfd_at_least(fd, minfd, F_DUPFD_CLOFORK);
            break;
        }

        case F_GETFD: {
            intptr_t kernel_result;
            if (fd < 0 || fd >= _FCNTL_MAX_FDS) {
                errno = EBADF;
                result = -1;
                break;
            }
            kernel_result = _RIN_FCNTL_SYSCALL3(
                SYS_FCNTL, (uintptr_t)fd, (uintptr_t)F_GETFD, 0u);
            result = _fcntl_result_to_int(kernel_result);
            break;
        }

        case F_SETFD: {
            int flags = va_arg(ap, int);
            if (fd < 0 || fd >= _FCNTL_MAX_FDS) {
                errno = EBADF;
                result = -1;
            } else if ((flags & ~(FD_CLOEXEC | FD_CLOFORK)) != 0) {
                errno = EINVAL;
                result = -1;
            } else {
                intptr_t kernel_result = _RIN_FCNTL_SYSCALL3(
                    SYS_FCNTL, (uintptr_t)fd, (uintptr_t)F_SETFD,
                    (uintptr_t)flags);
                result = _fcntl_result_to_int(kernel_result);
            }
            break;
        }

        case F_GETFL:
            if (fd < 0 || fd >= _FCNTL_MAX_FDS) {
                errno = EBADF;
                result = -1;
                break;
            }
            result = _fcntl_result_to_int(_RIN_FCNTL_SYSCALL3(
                (uintptr_t)SYS_FCNTL, (uintptr_t)fd,
                (uintptr_t)F_GETFL, 0u));
            break;

        case F_GETPIPE_SZ:
            if (fd < 0 || fd >= _FCNTL_MAX_FDS) {
                errno = EBADF;
                result = -1;
                break;
            }
            result = _fcntl_result_to_int(_RIN_FCNTL_SYSCALL3(
                (uintptr_t)SYS_FCNTL, (uintptr_t)fd,
                (uintptr_t)F_GETPIPE_SZ, 0u));
            break;

        case F_SETPIPE_SZ: {
            int requested = va_arg(ap, int);
            if (fd < 0 || fd >= _FCNTL_MAX_FDS) {
                errno = EBADF;
                result = -1;
                break;
            }
            result = _fcntl_result_to_int(_RIN_FCNTL_SYSCALL3(
                (uintptr_t)SYS_FCNTL, (uintptr_t)fd,
                (uintptr_t)F_SETPIPE_SZ, (uintptr_t)requested));
            break;
        }

        case F_SETFL: {
            int flags = va_arg(ap, int);
            if (fd < 0 || fd >= _FCNTL_MAX_FDS) {
                errno = EBADF;
                result = -1;
                break;
            }
            {
                intptr_t kernel_result = _RIN_FCNTL_SYSCALL3(
                    (uintptr_t)SYS_FCNTL, (uintptr_t)fd,
                    (uintptr_t)F_SETFL, (uintptr_t)flags);
                result = _fcntl_result_status_zero(kernel_result);
            }
            break;
        }

        case F_GETLK:
        case F_OFD_GETLK:
        {
            struct flock* lock = va_arg(ap, struct flock*);
            if (lock == NULL) {
                errno = EFAULT;
                result = -1;
                break;
            }
#if defined(__RIN_FILE_OFFSET_BITS64) || defined(__x86_64__)
            result = _fcntl_lock64_flock(fd, cmd, lock);
#else
            result = _fcntl_result_status_zero(_RIN_FCNTL_SYSCALL3(
                SYS_FCNTL, (uintptr_t)fd, (uintptr_t)cmd,
                (uintptr_t)lock));
#endif
            break;
        }

        case F_GETLK64:
        case F_OFD_GETLK64: {
            struct flock64* lock = va_arg(ap, struct flock64*);
            result = _fcntl_lock64_flock64(fd, cmd, lock);
            break;
        }

        case F_SETLK:
        case F_SETLKW:
        case F_OFD_SETLK:
        case F_OFD_SETLKW:
        case F_RIN_FLOCK_SETLK:
        case F_RIN_FLOCK_SETLKW:
        {
            struct flock* lock = va_arg(ap, struct flock*);
            if (lock == NULL) {
                errno = EFAULT;
                result = -1;
                break;
            }
#if defined(__RIN_FILE_OFFSET_BITS64) || defined(__x86_64__)
            result = _fcntl_lock64_flock(fd, cmd, lock);
#else
            result = _fcntl_result_status_zero(_RIN_FCNTL_SYSCALL3(
                SYS_FCNTL, (uintptr_t)fd, (uintptr_t)cmd,
                (uintptr_t)lock));
#endif
            break;
        }

        case F_SETLK64:
        case F_SETLKW64:
        case F_OFD_SETLK64:
        case F_OFD_SETLKW64: {
            struct flock64* lock = va_arg(ap, struct flock64*);
            result = _fcntl_lock64_flock64(fd, cmd, lock);
            break;
        }

        case F_GETOWN:
            errno = ENOSYS;
            result = -1;
            break;

        case F_SETOWN:
            errno = ENOSYS;
            result = -1;
            break;

        case F_GETSIG:
            errno = ENOSYS;
            result = -1;
            break;

        case F_SETSIG:
            errno = ENOSYS;
            result = -1;
            break;

        default:
            errno = EINVAL;
            result = -1;
            break;
    }

    va_end(ap);
    return result;
}

/* ═══════════════════════════════════════════════════════════════
 * 追加関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int _rin_posix_fadvise_result(
    int fd, off_t offset, off_t len, int advice) {
    uint64_t unsigned_offset;
    uint64_t unsigned_length;
    intptr_t result;

    if (offset < 0 || len < 0 || advice < POSIX_FADV_NORMAL ||
        advice > POSIX_FADV_NOREUSE)
        return EINVAL;
    unsigned_offset = (uint64_t)offset;
    unsigned_length = (uint64_t)len;
    if (unsigned_offset > UINT64_MAX - unsigned_length)
        return EOVERFLOW;

    result = _RIN_FCNTL_SYSCALL6(
        SYS_FADVISE, (uintptr_t)fd,
        (uintptr_t)(uint32_t)unsigned_offset,
        (uintptr_t)(uint32_t)(unsigned_offset >> 32),
        (uintptr_t)(uint32_t)unsigned_length,
        (uintptr_t)(uint32_t)(unsigned_length >> 32),
        (uintptr_t)(uint32_t)advice);
    if (result == 0) return 0;
    if (result < 0 && result >= (intptr_t)-4095)
        return (int)-result;
    return EIO;
}

static inline int _rin_fallocate_result(
    int fd, int mode, off_t offset, off_t len) {
    uint64_t unsigned_offset;
    uint64_t unsigned_length;
    intptr_t result;

    if (offset < 0 || len <= 0) return EINVAL;
    unsigned_offset = (uint64_t)offset;
    unsigned_length = (uint64_t)len;
    if (unsigned_offset > (uint64_t)INT64_MAX - unsigned_length)
        return EFBIG;
    result = _RIN_FCNTL_SYSCALL6(
        SYS_FALLOCATE, (uintptr_t)fd,
        (uintptr_t)(uint32_t)unsigned_offset,
        (uintptr_t)(uint32_t)(unsigned_offset >> 32),
        (uintptr_t)(uint32_t)unsigned_length,
        (uintptr_t)(uint32_t)(unsigned_length >> 32), (uintptr_t)mode);
    if (result == 0) return 0;
    if (result < 0 && result >= (intptr_t)-4095) return (int)-result;
    return EIO;
}

static inline int fallocate(int fd, int mode, off_t offset, off_t len) {
    int error = _rin_fallocate_result(fd, mode, offset, len);
    if (error == 0) return 0;
    errno = error;
    return -1;
}

#if (defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)) || \
    defined(RIN_FCNTL_ADVISE_RUNTIME_IMPLEMENTATION)
int posix_fadvise(int fd, off_t offset, off_t len, int advice);
int posix_fallocate(int fd, off_t offset, off_t len);
#else
static inline int posix_fadvise(int fd, off_t offset, off_t len, int advice) {
    return _rin_posix_fadvise_result(fd, offset, len, advice);
}

static inline int posix_fallocate(int fd, off_t offset, off_t len) {
    return _rin_fallocate_result(fd, 0, offset, len);
}
#endif

/* xxxat系関数 */
static inline int faccessat2(int dirfd, const char* pathname, int mode,
                             int flags) {
    const int allowed_flags = AT_EACCESS | AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    if ((mode & ~7) != 0 || (flags & ~allowed_flags) != 0) {
        errno = EINVAL;
        return -1;
    }
    /* Keep pathname access inside the kernel's checked-copy and namespace
     * lookup path.  In particular, do not dereference pathname here to test
     * for an empty string. */
    return _fcntl_result_status_zero(_RIN_FCNTL_SYSCALL4(
        (uintptr_t)SYS_FACCESSAT2, (uintptr_t)(intptr_t)dirfd,
        (uintptr_t)pathname, (uintptr_t)mode, (uintptr_t)flags));
}

static inline int faccessat(int dirfd, const char* pathname, int mode, int flags) {
    RinPathAtCallV1 call;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    if ((mode & ~7) != 0) {
        errno = EINVAL;
        return -1;
    }
    if ((flags & ~(AT_EACCESS | AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (pathname[0] == '\0' && (flags & AT_EMPTY_PATH) == 0) {
        errno = ENOENT;
        return -1;
    }
    if (pathname[0] == '\0' && dirfd == AT_FDCWD) {
        /* AT_FDCWD is a path-resolution base, not an open descriptor. */
        pathname = ".";
        flags &= ~AT_EMPTY_PATH;
    }
    _fcntl_path_at_init(&call, RIN_PATH_AT_ACCESS);
    call.dirfd = dirfd;
    call.flags = (uint32_t)(pathname[0] == '\0'
        ? flags : (flags & ~AT_EMPTY_PATH));
    call.mode = (uint32_t)mode;
    call.path1 = (uint64_t)(uintptr_t)pathname;
    return _fcntl_path_at_zero(&call);
}

static inline int fchownat(int dirfd, const char* pathname, uid_t owner, gid_t group, int flags) {
    RinPathAtCallV1 call;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    if ((flags & ~(AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (pathname[0] == '\0') {
        if ((flags & AT_EMPTY_PATH) == 0) {
            errno = ENOENT;
            return -1;
        }
        if (dirfd != AT_FDCWD) {
            _fcntl_path_at_init(&call, RIN_PATH_AT_CHOWN);
            call.dirfd = dirfd;
            call.flags = (uint32_t)flags;
            call.owner = (uint32_t)owner;
            call.group = (uint32_t)group;
            call.path1 = (uint64_t)(uintptr_t)pathname;
            return _fcntl_path_at_zero(&call);
        }
        pathname = ".";
        flags &= ~AT_EMPTY_PATH;
    }
    _fcntl_path_at_init(&call, RIN_PATH_AT_CHOWN);
    call.dirfd = dirfd;
    call.flags = (uint32_t)(flags & ~AT_EMPTY_PATH);
    call.owner = (uint32_t)owner;
    call.group = (uint32_t)group;
    call.path1 = (uint64_t)(uintptr_t)pathname;
    return _fcntl_path_at_zero(&call);
}

static inline int linkat(int olddirfd, const char* oldpath, int newdirfd, const char* newpath, int flags) {
    const int known_flags = AT_SYMLINK_FOLLOW | AT_EMPTY_PATH;
    RinPathAtCallV1 call;

    if (!oldpath || !newpath) {
        errno = EFAULT;
        return -1;
    }
    if ((flags & ~known_flags) != 0) {
        errno = EINVAL;
        return -1;
    }
    if ((flags & AT_EMPTY_PATH) != 0 && oldpath[0] != '\0') {
        errno = EINVAL;
        return -1;
    }
    if (newpath[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    _fcntl_path_at_init(&call, RIN_PATH_AT_LINK);
    call.dirfd = olddirfd;
    call.secondary_dirfd = newdirfd;
    call.flags = (uint32_t)flags;
    call.path1 = (uint64_t)(uintptr_t)oldpath;
    call.path2 = (uint64_t)(uintptr_t)newpath;
    return _fcntl_path_at_zero(&call);
}

static inline int mkdirat(int dirfd, const char* pathname, mode_t mode) {
    RinPathAtCallV1 call;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    if (pathname[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    _fcntl_path_at_init(&call, RIN_PATH_AT_MKDIR);
    call.dirfd = dirfd;
    call.mode = (uint32_t)mode;
    call.path1 = (uint64_t)(uintptr_t)pathname;
    return _fcntl_path_at_zero(&call);
}

/* Create one directory below a VFS directory descriptor and return a
 * descriptor pinned to the inode created by this call. This closes the
 * mkdirat()+openat() name-replacement window for storage owners. */
static inline int rin_mkdirat_open_dir(int dirfd, const char* pathname,
                                       mode_t mode) {
    RinPathAtCallV1 call;
    intptr_t result;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    if (pathname[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    _fcntl_path_at_init(&call, RIN_PATH_AT_MKDIR_OPEN);
    call.dirfd = dirfd;
    call.open_flags = RIN_PATH_AT_O_CLOEXEC;
    call.mode = (uint32_t)mode;
    call.path1 = (uint64_t)(uintptr_t)pathname;
    result = _fcntl_path_at_value(&call);
    if (result < 0 || (uintptr_t)result > (uintptr_t)INT32_MAX) {
        if (result >= 0) errno = EOVERFLOW;
        return -1;
    }
    return (int)result;
}

/* Remove a directory name only when it still names the inode held by
 * expected_dirfd. The target descriptor stays open through the atomic check
 * and removal. */
static inline int rin_rmdirat_if_same(int dirfd, const char* pathname,
                                      int expected_dirfd) {
    RinPathAtCallV1 call;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    if (pathname[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    _fcntl_path_at_init(&call, RIN_PATH_AT_RMDIR_MATCH);
    call.dirfd = dirfd;
    call.secondary_dirfd = expected_dirfd;
    call.path1 = (uint64_t)(uintptr_t)pathname;
    return _fcntl_path_at_zero(&call);
}

static inline int rin_mkdirat_beneath(int dirfd, const char* pathname,
                                      mode_t mode) {
    intptr_t result;

    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    if (pathname[0] == '\0') {
        errno = ENOENT;
        return -1;
    }

    result = _RIN_FCNTL_SYSCALL3(
        SYS_MKDIRAT_BENEATH, (uintptr_t)dirfd, (uintptr_t)pathname,
        (uintptr_t)mode);
    return _fcntl_result_status_zero(result);
}

static inline int rin_renameat_inline(int olddirfd, const char* oldpath,
                                      int newdirfd, const char* newpath) {
    RinPathAtCallV1 call;
    if (!oldpath || !newpath) {
        errno = EFAULT;
        return -1;
    }
    if (oldpath[0] == '\0' || newpath[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    _fcntl_path_at_init(&call, RIN_PATH_AT_RENAME);
    call.dirfd = olddirfd;
    call.secondary_dirfd = newdirfd;
    call.path1 = (uint64_t)(uintptr_t)oldpath;
    call.path2 = (uint64_t)(uintptr_t)newpath;
    return _fcntl_path_at_zero(&call);
}

/* Descriptor-relative atomic publication. The kernel refuses an existing
 * destination instead of replacing it between a caller-side preflight and
 * the rename. */
static inline int rin_renameat_noreplace(int olddirfd, const char* oldpath,
                                         int newdirfd, const char* newpath) {
    RinPathAtCallV1 call;
    if (!oldpath || !newpath) {
        errno = EFAULT;
        return -1;
    }
    if (oldpath[0] == '\0' || newpath[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    _fcntl_path_at_init(&call, RIN_PATH_AT_RENAME);
    call.flags = RIN_PATH_AT_RENAME_NOREPLACE;
    call.dirfd = olddirfd;
    call.secondary_dirfd = newdirfd;
    call.path1 = (uint64_t)(uintptr_t)oldpath;
    call.path2 = (uint64_t)(uintptr_t)newpath;
    return _fcntl_path_at_zero(&call);
}

/* RinOS extension: atomically fail with EEXIST when the destination exists. */
static inline int rin_rename_noreplace(const char* oldpath,
                                       const char* newpath) {
    if (!oldpath || !newpath) {
        errno = EFAULT;
        return -1;
    }
    if (oldpath[0] == '\0' || newpath[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    return rin_renameat_noreplace(AT_FDCWD, oldpath, AT_FDCWD, newpath);
}

static inline int symlinkat(const char* target, int newdirfd, const char* linkpath) {
    RinPathAtCallV1 call;
    if (!target || !linkpath) {
        errno = EFAULT;
        return -1;
    }
    if (linkpath[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    _fcntl_path_at_init(&call, RIN_PATH_AT_SYMLINK);
    call.dirfd = newdirfd;
    call.path1 = (uint64_t)(uintptr_t)target;
    call.path2 = (uint64_t)(uintptr_t)linkpath;
    return _fcntl_path_at_zero(&call);
}

static inline int unlinkat(int dirfd, const char* pathname, int flags) {
    RinPathAtCallV1 call;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    if ((flags & ~AT_REMOVEDIR) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (pathname[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    _fcntl_path_at_init(&call, RIN_PATH_AT_UNLINK);
    call.dirfd = dirfd;
    call.flags = (uint32_t)flags;
    call.path1 = (uint64_t)(uintptr_t)pathname;
    return _fcntl_path_at_zero(&call);
}

static inline ssize_t readlinkat(int dirfd, const char* pathname, char* buf, size_t bufsiz) {
    RinPathAtCallV1 call;
    intptr_t result;
    if (!pathname || !buf) {
        errno = EFAULT;
        return -1;
    }
    _fcntl_path_at_init(&call, RIN_PATH_AT_READLINK);
    call.dirfd = dirfd;
    call.path1 = (uint64_t)(uintptr_t)pathname;
    call.buffer = (uint64_t)(uintptr_t)buf;
    call.buffer_size = (uint64_t)bufsiz;
    result = _fcntl_path_at_value(&call);
    if (result < 0 || (uint64_t)result > (uint64_t)bufsiz) {
        if (result >= 0) errno = EIO;
        return -1;
    }
    if ((uint64_t)result > (uint64_t)SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (ssize_t)result;
}

#ifdef __cplusplus
}
#endif
#endif /* !MIDL_PASS */

#endif /* RINOS_LIBC_FCNTL_H */

#endif /* hosted Linux delegates to its native file-control provider */
