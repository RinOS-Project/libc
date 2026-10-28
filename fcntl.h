/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - fcntl.h
 * ファイル制御
 */

#ifndef _FCNTL_H
#define _FCNTL_H

#include "stddef.h"
#include "stdarg.h"
#include "errno.h"
#include "limits.h"
#include "fcntl_flags.h"
#include "sys/socket.h"
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

/* ファイルディスクリプタフラグ */
#define FD_CLOEXEC  1   /* exec時にクローズ */

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
    long  l_start;  /* オフセット */
    long  l_len;    /* 長さ (0 = EOF まで) */
    int   l_pid;    /* プロセスID */
};

/* ═══════════════════════════════════════════════════════════════
 * fcntl追加コマンド
 * ═══════════════════════════════════════════════════════════════*/

#define F_DUPFD_CLOEXEC 1030 /* F_DUPFDと同様だがCLOEXECを設定 */
#define F_SETOWN        8    /* シグナル送信先プロセスを設定 */
#define F_GETOWN        9    /* シグナル送信先プロセスを取得 */
#define F_SETSIG        10   /* シグナル番号を設定 */
#define F_GETSIG        11   /* シグナル番号を取得 */
#define F_SETOWN_EX     15   /* 拡張: シグナル送信先を設定 */
#define F_GETOWN_EX     16   /* 拡張: シグナル送信先を取得 */
#define F_OFD_GETLK     36   /* オープンファイルディスクリプタロック取得 */
#define F_OFD_SETLK     37   /* オープンファイルディスクリプタロック設定 */
#define F_OFD_SETLKW    38   /* オープンファイルディスクリプタロック設定(ブロッキング) */

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

/* ═══════════════════════════════════════════════════════════════
 * AT_* フラグ (xxxat系関数用)
 * ═══════════════════════════════════════════════════════════════*/

#define AT_FDCWD            (-100) /* カレントディレクトリ */
#define AT_SYMLINK_NOFOLLOW 0x100  /* シンボリックリンクを辿らない */
#define AT_REMOVEDIR        0x200  /* ディレクトリを削除 */
#define AT_SYMLINK_FOLLOW   0x400  /* シンボリックリンクを辿る */
#define AT_NO_AUTOMOUNT     0x800  /* 自動マウントしない */
#define AT_EMPTY_PATH       0x1000 /* 空パスを許可 */
#define AT_EACCESS          0x200  /* 実効IDでアクセスチェック */

/* ═══════════════════════════════════════════════════════════════
 * ファイルディスクリプタ状態保持 (簡易実装)
 * ═══════════════════════════════════════════════════════════════*/

#define _FCNTL_MAX_FDS FD_SETSIZE
/* Descriptor state belongs to the process, not to each translation unit
 * which happens to include this header.  Definitions live in
 * fcntl_runtime.c so open/fcntl/close helpers share one table. */
extern int _fcntl_fd_flags[_FCNTL_MAX_FDS];
extern int _fcntl_fl_flags[_FCNTL_MAX_FDS];
extern int _fcntl_initialized;

static inline void _fcntl_init(void) {
    if (_fcntl_initialized) return;
    for (int i = 0; i < _FCNTL_MAX_FDS; i++) {
        _fcntl_fd_flags[i] = 0;
        _fcntl_fl_flags[i] = O_RDWR;
    }
    _fcntl_initialized = 1;
}

static inline void _fcntl_track_new_fd(int fd, int fl_flags, int fd_flags) {
    _fcntl_init();
    if (fd >= 0 && fd < _FCNTL_MAX_FDS) {
        _fcntl_fl_flags[fd] = fl_flags;
        _fcntl_fd_flags[fd] = fd_flags;
    }
}

static inline void _fcntl_track_close_fd(int fd) {
    _fcntl_init();
    if (fd >= 0 && fd < _FCNTL_MAX_FDS) {
        _fcntl_fl_flags[fd] = O_RDWR;
        _fcntl_fd_flags[fd] = 0;
    }
}

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
    if (_syscall1((uintptr_t)SYS_CLOSE,
                  (uintptr_t)(intptr_t)fd) >= 0)
        _fcntl_track_close_fd(fd);
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

static inline int _fcntl_dupfd_at_least(int fd, int minfd, int cloexec) {
    int command = cloexec ? F_DUPFD_CLOEXEC : F_DUPFD;
    int result;

    _fcntl_init();
    if (minfd < 0) {
        errno = EINVAL;
        return -1;
    }

    result = _fcntl_result_to_int(_RIN_FCNTL_SYSCALL3(
        SYS_FCNTL, (uintptr_t)fd, (uintptr_t)command, (uintptr_t)minfd));
    if (result >= 0 && result < _FCNTL_MAX_FDS &&
        fd >= 0 && fd < _FCNTL_MAX_FDS) {
        _fcntl_fl_flags[result] = _fcntl_fl_flags[fd];
        _fcntl_fd_flags[result] = cloexec ? FD_CLOEXEC : 0;
    }

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

    int fd = _fcntl_result_to_int(_syscall3(
        (uintptr_t)SYS_OPEN, (uintptr_t)pathname, (uintptr_t)flags,
        (uintptr_t)mode));

    if (fd < 0) {
        __rin_pthread_cancel_point_if_linked();
    } else {
        _fcntl_track_new_fd(fd, flags,
                            (flags & O_CLOEXEC) ? FD_CLOEXEC : 0);
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
    _fcntl_track_new_fd((int)result, flags,
                         (flags & O_CLOEXEC) ? FD_CLOEXEC : 0);
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
    _fcntl_track_new_fd((int)result, flags,
                         (flags & O_CLOEXEC) ? FD_CLOEXEC : 0);
    return (int)result;
}

static inline int creat(const char* pathname, mode_t mode) {
    return open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

static inline int fcntl(int fd, int cmd, ...) {
    _fcntl_init();

    va_list ap;
    va_start(ap, cmd);

    int result = -1;

    switch (cmd) {
        case F_DUPFD: {
            int minfd = va_arg(ap, int);
            result = _fcntl_dupfd_at_least(fd, minfd, 0);
            break;
        }

        case F_DUPFD_CLOEXEC: {
            int minfd = va_arg(ap, int);
            result = _fcntl_dupfd_at_least(fd, minfd, 1);
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
            if (result >= 0) {
                _fcntl_fd_flags[fd] = result;
            }
            break;
        }

        case F_SETFD: {
            int flags = va_arg(ap, int);
            if (fd < 0 || fd >= _FCNTL_MAX_FDS) {
                errno = EBADF;
                result = -1;
            } else if ((flags & ~FD_CLOEXEC) != 0) {
                errno = EINVAL;
                result = -1;
            } else {
                intptr_t kernel_result = _RIN_FCNTL_SYSCALL3(
                    SYS_FCNTL, (uintptr_t)fd, (uintptr_t)F_SETFD,
                    (uintptr_t)flags);
                result = _fcntl_result_to_int(kernel_result);
                if (result >= 0) {
                    _fcntl_fd_flags[fd] = flags;
                }
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
            if (result >= 0) _fcntl_fl_flags[fd] = result;
            break;

        case F_SETFL: {
            int flags = va_arg(ap, int);
            int current_flags;
            int new_flags;
            if (fd < 0 || fd >= _FCNTL_MAX_FDS) {
                errno = EBADF;
                result = -1;
                break;
            }
            /* The process may have inherited this descriptor through exec.
             * Read its access mode from the kernel instead of trusting the
             * per-process cache before passing status flags to F_SETFL. */
            current_flags = _fcntl_result_to_int(_RIN_FCNTL_SYSCALL3(
                (uintptr_t)SYS_FCNTL, (uintptr_t)fd,
                (uintptr_t)F_GETFL, 0u));
            if (current_flags < 0) {
                result = -1;
                break;
            }
            new_flags = (current_flags & O_ACCMODE) |
                (flags & ~O_ACCMODE);
            {
                intptr_t kernel_result = _RIN_FCNTL_SYSCALL3(
                    (uintptr_t)SYS_FCNTL, (uintptr_t)fd,
                    (uintptr_t)F_SETFL, (uintptr_t)new_flags);
                result = _fcntl_result_status_zero(kernel_result);
                if (result == 0) _fcntl_fl_flags[fd] = new_flags;
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
            result = _fcntl_result_status_zero(_RIN_FCNTL_SYSCALL3(
                SYS_FCNTL, (uintptr_t)fd, (uintptr_t)cmd,
                (uintptr_t)lock));
            break;
        }

        case F_SETLK:
        case F_SETLKW:
        case F_OFD_SETLK:
        case F_OFD_SETLKW:
        {
            struct flock* lock = va_arg(ap, struct flock*);
            if (lock == NULL) {
                errno = EFAULT;
                result = -1;
                break;
            }
            result = _fcntl_result_status_zero(_RIN_FCNTL_SYSCALL3(
                SYS_FCNTL, (uintptr_t)fd, (uintptr_t)cmd,
                (uintptr_t)lock));
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

static inline int posix_fadvise(int fd, off_t offset, off_t len, int advice) {
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

static inline int posix_fallocate(int fd, off_t offset, off_t len) {
    uint64_t unsigned_offset;
    uint64_t unsigned_length;
    intptr_t result;

    if (offset < 0 || len < 0) return EINVAL;
    unsigned_offset = (uint64_t)offset;
    unsigned_length = (uint64_t)len;
    if (unsigned_offset > UINT64_MAX - unsigned_length) return EOVERFLOW;
    result = _RIN_FCNTL_SYSCALL6(
        SYS_FALLOCATE, (uintptr_t)fd,
        (uintptr_t)(uint32_t)unsigned_offset,
        (uintptr_t)(uint32_t)(unsigned_offset >> 32),
        (uintptr_t)(uint32_t)unsigned_length,
        (uintptr_t)(uint32_t)(unsigned_length >> 32), 0u);
    if (result == 0) return 0;
    if (result < 0 && result >= (intptr_t)-4095) return (int)-result;
    return EIO;
}

/* xxxat系関数 */
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
    if (pathname[0] != '\0' && (flags & AT_EMPTY_PATH) != 0) {
        errno = EINVAL;
        return -1;
    }
    _fcntl_path_at_init(&call, RIN_PATH_AT_ACCESS);
    call.dirfd = dirfd;
    call.flags = (uint32_t)flags;
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
        _fcntl_path_at_init(&call, RIN_PATH_AT_CHOWN);
        call.dirfd = dirfd;
        call.flags = (uint32_t)flags;
        call.owner = (uint32_t)owner;
        call.group = (uint32_t)group;
        call.path1 = (uint64_t)(uintptr_t)pathname;
        return _fcntl_path_at_zero(&call);
    }
    if ((flags & AT_EMPTY_PATH) != 0) {
        errno = EINVAL;
        return -1;
    }
    _fcntl_path_at_init(&call, RIN_PATH_AT_CHOWN);
    call.dirfd = dirfd;
    call.flags = (uint32_t)flags;
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
    _fcntl_track_new_fd((int)result, O_CLOEXEC, FD_CLOEXEC);
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

static inline int renameat(int olddirfd, const char* oldpath, int newdirfd, const char* newpath) {
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

#endif /* _FCNTL_H */
