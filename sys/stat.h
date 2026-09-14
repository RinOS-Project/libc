/*
 * RinOS libc - sys/stat.h
 * ファイル状態
 */

#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include "../stddef.h"
#include "../stdint.h"
#include "../errno.h"
#include "../limits.h"
#include "../fcntl.h"
#include "types.h"
#include "syscall.h"
#include <rin/fs/path_at_abi.h>

#ifndef _RIN_STAT_SYSCALL2
#define _RIN_STAT_SYSCALL2(number, argument1, argument2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2))
#endif
#ifndef _RIN_STAT_SYSCALL4
#define _RIN_STAT_SYSCALL4(number, argument1, argument2, argument3, argument4) \
    _syscall4((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3), \
              (uintptr_t)(argument4))
#endif
#ifndef _RIN_STAT_SYSCALL1
#define _RIN_STAT_SYSCALL1(number, argument1) \
    _syscall1((uintptr_t)(number), (uintptr_t)(argument1))
#endif
#ifndef _RIN_STAT_PATH_AT_CALL
#define _RIN_STAT_PATH_AT_CALL(call) \
    _syscall1((uintptr_t)SYS_PATH_AT, (uintptr_t)(call))
#endif

static inline void __rin_stat_path_at_init(RinPathAtCallV1* call,
                                           uint16_t operation) {
    unsigned int index;
    unsigned char* bytes = (unsigned char*)call;
    for (index = 0u; index < sizeof(*call); ++index) bytes[index] = 0u;
    call->struct_size = (uint32_t)sizeof(*call);
    call->version = RIN_PATH_AT_CALL_VERSION;
    call->operation = operation;
    call->dirfd = AT_FDCWD;
    call->secondary_dirfd = AT_FDCWD;
}

static inline int __rin_stat_path_at_result(intptr_t result) {
    if (result < 0) {
        errno = result >= -4095 ? (int)-result : EIO;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

/* All stat-family syscalls return a kernel word, while the POSIX surface
 * exposes int.  Do not narrow a positive status or a wide success word: a
 * non-zero status is an I/O failure and a value outside int is an ABI fault.
 * Negative errno words have already been POSIXized by the caller and must
 * retain the errno it published. */
static inline int __rin_stat_int_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    if (result > (intptr_t)INT_MAX || result < (intptr_t)INT_MIN) {
        errno = EOVERFLOW;
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
 * ファイルモード定数
 * ═══════════════════════════════════════════════════════════════*/

/* ファイルタイプ */
#define S_IFMT   0170000  /* ファイルタイプマスク */
#define S_IFSOCK 0140000  /* ソケット */
#define S_IFLNK  0120000  /* シンボリックリンク */
#define S_IFREG  0100000  /* 通常ファイル */
#define S_IFBLK  0060000  /* ブロックデバイス */
#define S_IFDIR  0040000  /* ディレクトリ */
#define S_IFCHR  0020000  /* キャラクタデバイス */
#define S_IFIFO  0010000  /* FIFO */

/* ファイルタイプテストマクロ */
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

/* パーミッション */
#define S_ISUID  04000  /* Set UID bit */
#define S_ISGID  02000  /* Set GID bit */
#define S_ISVTX  01000  /* Sticky bit */

#define S_IRWXU  00700  /* Owner: rwx */
#define S_IRUSR  00400  /* Owner: read */
#define S_IWUSR  00200  /* Owner: write */
#define S_IXUSR  00100  /* Owner: execute */

#define S_IRWXG  00070  /* Group: rwx */
#define S_IRGRP  00040  /* Group: read */
#define S_IWGRP  00020  /* Group: write */
#define S_IXGRP  00010  /* Group: execute */

#define S_IRWXO  00007  /* Others: rwx */
#define S_IROTH  00004  /* Others: read */
#define S_IWOTH  00002  /* Others: write */
#define S_IXOTH  00001  /* Others: execute */

/* ═══════════════════════════════════════════════════════════════
 * stat構造体
 * ═══════════════════════════════════════════════════════════════*/

struct stat {
    dev_t     st_dev;     /* デバイスID */
    ino_t     st_ino;     /* inode番号 */
    mode_t    st_mode;    /* ファイルモード */
    nlink_t   st_nlink;   /* ハードリンク数 */
    uid_t     st_uid;     /* 所有者UID */
    gid_t     st_gid;     /* 所有者GID */
    dev_t     st_rdev;    /* デバイス番号 (特殊ファイル) */
    off_t     st_size;    /* サイズ (バイト) */
    blksize_t st_blksize; /* I/Oブロックサイズ */
    blkcnt_t  st_blocks;  /* 割り当てブロック数 */
    time_t    st_atime;   /* 最終アクセス時刻 */
    time_t    st_mtime;   /* 最終修正時刻 */
    time_t    st_ctime;   /* 最終状態変更時刻 */
};

/* ═══════════════════════════════════════════════════════════════
 * 関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int stat(const char* pathname, struct stat* statbuf) {
    if (!pathname || !statbuf) {
        errno = EFAULT;
        return -1;
    }
    return __rin_stat_int_result(_RIN_STAT_SYSCALL2(
        SYS_STAT, pathname, statbuf));
}

static inline int fstat(int fd, struct stat* statbuf) {
    if (!statbuf) {
        errno = EFAULT;
        return -1;
    }
    return __rin_stat_int_result(_RIN_STAT_SYSCALL2(
        SYS_FSTAT, fd, statbuf));
}

static inline int lstat(const char* pathname, struct stat* statbuf) {
    if (!pathname || !statbuf) {
        errno = EFAULT;
        return -1;
    }
    return __rin_stat_int_result(_RIN_STAT_SYSCALL2(SYS_LSTAT, pathname, statbuf));
}

static inline int fstatat(int dirfd, const char* pathname, struct stat* statbuf, int flags) {
    RinPathAtCallV1 call;
    if (!pathname || !statbuf) {
        errno = EFAULT;
        return -1;
    }

    if ((flags & ~(AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW |
                   AT_NO_AUTOMOUNT)) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (pathname[0] == '\0') {
        if ((flags & AT_EMPTY_PATH) == 0) {
            errno = ENOENT;
            return -1;
        }
        /* AT_EMPTY_PATH resolves the already-open description.  NOFOLLOW is
         * meaningful during pathname traversal, but cannot change the object
         * selected by an ordinary descriptor, so the descriptor owner handles
         * both flag combinations through the same fstat syscall. */
        return fstat(dirfd, statbuf);
    }
    if ((flags & AT_EMPTY_PATH) != 0) {
        errno = EINVAL;
        return -1;
    }
    __rin_stat_path_at_init(&call, RIN_PATH_AT_STAT);
    call.flags = (uint32_t)flags;
    call.dirfd = dirfd;
    call.path1 = (uint64_t)(uintptr_t)pathname;
    call.buffer = (uint64_t)(uintptr_t)statbuf;
    call.buffer_size = sizeof(*statbuf);
    return __rin_stat_path_at_result(_RIN_STAT_PATH_AT_CALL(&call));
}

/* Capability-only fstatat for WASI preopens. Unlike fstatat(), this cannot
 * fall back to the process CWD or traverse outside the retained dirfd. */
static inline int rin_fstatat_beneath(int dirfd, const char* pathname,
                                      struct stat* statbuf, int flags) {
    intptr_t result;

    if (!pathname || !statbuf) {
        errno = EFAULT;
        return -1;
    }
    if (pathname[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    if ((flags & ~AT_SYMLINK_NOFOLLOW) != 0) {
        errno = EINVAL;
        return -1;
    }

    result = _RIN_STAT_SYSCALL4(
        SYS_FSTATAT_BENEATH, (uintptr_t)dirfd, (uintptr_t)pathname,
        (uintptr_t)statbuf, (uintptr_t)flags);
    return __rin_stat_int_result(result);
}

#ifndef __MKDIR_DEFINED
#define __MKDIR_DEFINED
static inline int mkdir(const char* pathname, mode_t mode) {
    return __rin_stat_int_result(_RIN_STAT_SYSCALL2(
        SYS_MKDIR, pathname, mode));
}
#endif

static inline int chmod(const char* pathname, mode_t mode) {
    intptr_t result;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    result = _RIN_STAT_SYSCALL2(SYS_CHMOD, pathname, mode);
    if (result < 0) {
        errno = result >= -4095 ? (int)-result : EIO;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static inline int fchmod(int fd, mode_t mode) {
    intptr_t result;
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    if (((unsigned long)mode & ~(unsigned long)0777u) != 0u) {
        errno = EINVAL;
        return -1;
    }
    result = _RIN_STAT_SYSCALL2(SYS_FCHMOD, fd, mode);
    if (result < 0) {
        errno = result >= -4095 ? (int)-result : EIO;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static inline int fchmodat(int dirfd, const char* pathname, mode_t mode,
                           int flags) {
    const int known_flags = AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW;
    RinPathAtCallV1 call;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    if ((flags & ~known_flags) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (pathname[0] == '\0') {
        if ((flags & AT_EMPTY_PATH) != 0) {
            intptr_t result;
            if (((unsigned long)mode & ~(unsigned long)0777u) != 0u) {
                errno = EINVAL;
                return -1;
            }
            /* Keep the fchmodat descriptor contract: AT_FDCWD and other
             * target-width descriptor values are passed to the kernel as-is.
             * Calling fchmod() here would reject AT_FDCWD before the syscall
             * and would make AT_EMPTY_PATH|AT_SYMLINK_NOFOLLOW inconsistent
             * with fstatat's direct SYS_FSTAT path. */
            result = _RIN_STAT_SYSCALL2(SYS_FCHMOD, dirfd, mode);
            if (result < 0) {
                errno = result >= -4095 ? (int)-result : EIO;
                return -1;
            }
            if (result != 0) {
                errno = EIO;
                return -1;
            }
            return 0;
        }
        errno = ENOENT;
        return -1;
    }
    if ((flags & AT_EMPTY_PATH) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (((unsigned long)mode & ~(unsigned long)0777u) != 0u) {
        errno = EINVAL;
        return -1;
    }
    __rin_stat_path_at_init(&call, RIN_PATH_AT_CHMOD);
    call.flags = (uint32_t)flags;
    call.dirfd = dirfd;
    call.mode = (uint32_t)mode;
    call.path1 = (uint64_t)(uintptr_t)pathname;
    return __rin_stat_path_at_result(_RIN_STAT_PATH_AT_CALL(&call));
}

static inline mode_t umask(mode_t mask) {
    intptr_t result = _RIN_STAT_SYSCALL1(SYS_UMASK, mask & 0777u);
    if (result < 0) {
        errno = result >= -4095 ? (int)-result : EIO;
        return (mode_t)-1;
    }
    if ((uintptr_t)result > (uintptr_t)0777u) {
        errno = EIO;
        return (mode_t)-1;
    }
    return (mode_t)result;
}

#ifdef __cplusplus
}
#endif

/* POSIX declares futimens() through <sys/stat.h>. */
#include "../utime.h"

#endif /* _SYS_STAT_H */
