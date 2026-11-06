/*
 * RinOS libc - sys/stat.h
 * ファイル状態
 */

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    (defined(__linux__) || defined(__MINGW32__) || \
     defined(__MINGW64__)) && \
    (defined(__GNUC__) || defined(__clang__)) && \
    !defined(_RIN_STAT_SYSCALL1) && !defined(_RIN_STAT_SYSCALL2) && \
    !defined(_RIN_STAT_SYSCALL4) && !defined(_RIN_STAT_PATH_AT_CALL)
#include_next <sys/stat.h>
#elif defined(_WIN32) && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    !defined(RIN_HOSTED_WINDOWS_ENABLE_RIN_SYSCALL) && \
    !defined(_RIN_STAT_SYSCALL1) && !defined(_RIN_STAT_SYSCALL2) && \
    !defined(_RIN_STAT_SYSCALL4) && !defined(_RIN_STAT_PATH_AT_CALL)
#error "RinOS sys/stat.h is target-only on hosted Windows; use the host CRT or provide a stat syscall adapter"
#else

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

#if defined(__i386__) && defined(RIN_TIME_ABI64) && \
    !defined(__RIN_FILE_OFFSET_BITS64) && \
    !defined(__RIN_SYS_TYPES_HOST_OWNER) && !defined(MIDL_PASS)
#define __RIN_STAT_TIME64_ABI 1
#endif

#ifndef MIDL_PASS
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
#endif /* !MIDL_PASS */

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

#if defined(__RIN_STAT_TIME64_ABI)
#if defined(__cplusplus)
static_assert(sizeof(struct stat) == 64u, "RinOS IA-32 time64 stat size");
static_assert(offsetof(struct stat, st_ino) == 4u,
              "RinOS IA-32 time64 stat inode offset");
static_assert(offsetof(struct stat, st_size) == 28u,
              "RinOS IA-32 time64 stat size offset");
static_assert(offsetof(struct stat, st_blocks) == 36u,
              "RinOS IA-32 time64 stat blocks offset");
static_assert(offsetof(struct stat, st_atime) == 40u,
              "RinOS IA-32 time64 stat timestamp offset");
static_assert(offsetof(struct stat, st_mtime) == 48u,
              "RinOS IA-32 time64 stat timestamp offset");
static_assert(offsetof(struct stat, st_ctime) == 56u,
              "RinOS IA-32 time64 stat timestamp offset");
#else
_Static_assert(sizeof(struct stat) == 64u, "RinOS IA-32 time64 stat size");
_Static_assert(offsetof(struct stat, st_ino) == 4u,
               "RinOS IA-32 time64 stat inode offset");
_Static_assert(offsetof(struct stat, st_size) == 28u,
               "RinOS IA-32 time64 stat size offset");
_Static_assert(offsetof(struct stat, st_blocks) == 36u,
               "RinOS IA-32 time64 stat blocks offset");
_Static_assert(offsetof(struct stat, st_atime) == 40u,
               "RinOS IA-32 time64 stat timestamp offset");
_Static_assert(offsetof(struct stat, st_mtime) == 48u,
               "RinOS IA-32 time64 stat timestamp offset");
_Static_assert(offsetof(struct stat, st_ctime) == 56u,
               "RinOS IA-32 time64 stat timestamp offset");
#endif
#endif

#if !defined(__RIN_SYS_TYPES_HOST_OWNER)
/* Stable compat32 large-file record. The old 52-byte stat ABI intentionally
 * stays unchanged; stat64 and _FILE_OFFSET_BITS=64 use this 88-byte layout. */
struct stat64 {
    uint32_t st_dev;
    uint32_t __reserved_dev;
    uint64_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t st_rdev;
    uint32_t __reserved_rdev;
    int64_t  st_size;
    int64_t  st_blksize;
    int64_t  st_blocks;
    int64_t  st_atime;
    int64_t  st_mtime;
    int64_t  st_ctime;
};
#if !defined(MIDL_PASS) && defined(__cplusplus)
static_assert(sizeof(struct stat64) == 88u, "RinOS stat64 ABI size");
#elif !defined(MIDL_PASS)
_Static_assert(sizeof(struct stat64) == 88u, "RinOS stat64 ABI size");
#endif

#ifndef STATX_TYPE
#define STATX_TYPE RIN_STATX_TYPE
#endif
#ifndef STATX_MODE
#define STATX_MODE RIN_STATX_MODE
#endif
#ifndef STATX_NLINK
#define STATX_NLINK RIN_STATX_NLINK
#endif
#ifndef STATX_UID
#define STATX_UID RIN_STATX_UID
#endif
#ifndef STATX_GID
#define STATX_GID RIN_STATX_GID
#endif
#ifndef STATX_ATIME
#define STATX_ATIME RIN_STATX_ATIME
#endif
#ifndef STATX_MTIME
#define STATX_MTIME RIN_STATX_MTIME
#endif
#ifndef STATX_CTIME
#define STATX_CTIME RIN_STATX_CTIME
#endif
#ifndef STATX_INO
#define STATX_INO RIN_STATX_INO
#endif
#ifndef STATX_SIZE
#define STATX_SIZE RIN_STATX_SIZE
#endif
#ifndef STATX_BLOCKS
#define STATX_BLOCKS RIN_STATX_BLOCKS
#endif
#ifndef STATX_BASIC_STATS
#define STATX_BASIC_STATS RIN_STATX_BASIC_STATS
#endif
#ifndef STATX_BTIME
#define STATX_BTIME RIN_STATX_BTIME
#endif
#ifndef STATX_MNT_ID
#define STATX_MNT_ID RIN_STATX_MNT_ID
#endif
#ifndef STATX_DIOALIGN
#define STATX_DIOALIGN RIN_STATX_DIOALIGN
#endif
#ifndef STATX_MNT_ID_UNIQUE
#define STATX_MNT_ID_UNIQUE RIN_STATX_MNT_ID_UNIQUE
#endif
#ifndef STATX_SUBVOL
#define STATX_SUBVOL RIN_STATX_SUBVOL
#endif
#ifndef STATX_WRITE_ATOMIC
#define STATX_WRITE_ATOMIC RIN_STATX_WRITE_ATOMIC
#endif
#ifndef STATX_DIO_READ_ALIGN
#define STATX_DIO_READ_ALIGN RIN_STATX_DIO_READ_ALIGN
#endif
#ifndef STATX__RESERVED
#define STATX__RESERVED RIN_STATX_RESERVED
#endif
#ifndef STATX_ALL
#define STATX_ALL RIN_STATX_ALL
#endif

struct statx_timestamp {
    int64_t tv_sec;
    uint32_t tv_nsec;
    int32_t __reserved;
};

/* Version 1 is a fixed-width 256-byte Linux statx-compatible record. The
 * kernel leaves unsupported optional fields zero and clears their mask bits. */
struct statx {
    uint32_t stx_mask;
    uint32_t stx_blksize;
    uint64_t stx_attributes;
    uint32_t stx_nlink;
    uint32_t stx_uid;
    uint32_t stx_gid;
    uint16_t stx_mode;
    uint16_t __spare0;
    uint64_t stx_ino;
    uint64_t stx_size;
    uint64_t stx_blocks;
    uint64_t stx_attributes_mask;
    struct statx_timestamp stx_atime;
    struct statx_timestamp stx_btime;
    struct statx_timestamp stx_ctime;
    struct statx_timestamp stx_mtime;
    uint32_t stx_rdev_major;
    uint32_t stx_rdev_minor;
    uint32_t stx_dev_major;
    uint32_t stx_dev_minor;
    uint64_t stx_mnt_id;
    uint32_t stx_dio_mem_align;
    uint32_t stx_dio_offset_align;
    uint64_t stx_subvol;
    uint32_t stx_atomic_write_unit_min;
    uint32_t stx_atomic_write_unit_max;
    uint32_t stx_atomic_write_segments_max;
    uint32_t stx_dio_read_offset_align;
    uint32_t stx_atomic_write_unit_max_opt;
    uint32_t __spare1;
    uint64_t __spare3[8];
};
#if !defined(MIDL_PASS) && defined(__cplusplus)
static_assert(sizeof(struct statx_timestamp) == 16u,
              "RinOS statx timestamp ABI size");
static_assert(sizeof(struct statx) == RIN_STATX_V1_SIZE,
              "RinOS statx v1 ABI size");
static_assert(offsetof(struct statx, stx_ino) == 32u,
              "RinOS statx inode offset");
static_assert(offsetof(struct statx, stx_atime) == 64u,
              "RinOS statx timestamps offset");
static_assert(offsetof(struct statx, stx_mnt_id) == 144u,
              "RinOS statx mount offset");
#elif !defined(MIDL_PASS)
_Static_assert(sizeof(struct statx_timestamp) == 16u,
               "RinOS statx timestamp ABI size");
_Static_assert(sizeof(struct statx) == RIN_STATX_V1_SIZE,
               "RinOS statx v1 ABI size");
_Static_assert(offsetof(struct statx, stx_ino) == 32u,
               "RinOS statx inode offset");
_Static_assert(offsetof(struct statx, stx_atime) == 64u,
               "RinOS statx timestamps offset");
_Static_assert(offsetof(struct statx, stx_mnt_id) == 144u,
               "RinOS statx mount offset");
#endif

#if defined(__RIN_STAT_TIME64_ABI)
static inline int __rin_stat64_to_time64(const struct stat64* source,
                                         struct stat* output) {
    if (!source || !output) {
        errno = EFAULT;
        return -1;
    }
    if (source->st_ino > ULONG_MAX ||
        source->st_size < (int64_t)LONG_MIN ||
        source->st_size > (int64_t)LONG_MAX ||
        source->st_blksize < (int64_t)LONG_MIN ||
        source->st_blksize > (int64_t)LONG_MAX ||
        source->st_blocks < (int64_t)LONG_MIN ||
        source->st_blocks > (int64_t)LONG_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    output->st_dev = (dev_t)source->st_dev;
    output->st_ino = (ino_t)source->st_ino;
    output->st_mode = (mode_t)source->st_mode;
    output->st_nlink = (nlink_t)source->st_nlink;
    output->st_uid = (uid_t)source->st_uid;
    output->st_gid = (gid_t)source->st_gid;
    output->st_rdev = (dev_t)source->st_rdev;
    output->st_size = (off_t)source->st_size;
    output->st_blksize = (blksize_t)source->st_blksize;
    output->st_blocks = (blkcnt_t)source->st_blocks;
    output->st_atime = (time_t)source->st_atime;
    output->st_mtime = (time_t)source->st_mtime;
    output->st_ctime = (time_t)source->st_ctime;
    return 0;
}

static inline int __rin_fstat_time64(int fd, struct stat* statbuf) {
    struct stat64 wide;
    int result;
    if (!statbuf) {
        errno = EFAULT;
        return -1;
    }
    result = __rin_stat_int_result(_RIN_STAT_SYSCALL2(
        SYS_FSTAT64, fd, &wide));
    return result < 0 ? -1 : __rin_stat64_to_time64(&wide, statbuf);
}

static inline int __rin_stat_time64_pathat(int dirfd, const char* pathname,
                                           struct stat* statbuf, int flags) {
    RinPathAtCallV1 call;
    if (!statbuf) {
        errno = EFAULT;
        return -1;
    }
    if ((flags & ~(AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW |
                   AT_NO_AUTOMOUNT)) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (!pathname) {
        if ((flags & AT_EMPTY_PATH) == 0) {
            errno = EFAULT;
            return -1;
        }
        pathname = "";
    }
    if ((flags & AT_EMPTY_PATH) != 0) {
        unsigned char first_byte;
        if (__rin_user_snapshot_byte(pathname, &first_byte) != 0)
            return -1;
        if (first_byte == '\0') {
            if (dirfd != AT_FDCWD)
                return __rin_fstat_time64(dirfd, statbuf);
            pathname = ".";
        }
        flags &= ~AT_EMPTY_PATH;
    }
    __rin_stat_path_at_init(&call, RIN_PATH_AT_STAT);
    call.flags = (uint32_t)(flags & ~AT_EMPTY_PATH);
    call.dirfd = dirfd;
    call.path1 = (uint64_t)(uintptr_t)pathname;
    call.buffer = (uint64_t)(uintptr_t)statbuf;
    call.buffer_size = sizeof(*statbuf);
    return __rin_stat_path_at_result(_RIN_STAT_PATH_AT_CALL(&call));
}
#endif /* __RIN_STAT_TIME64_ABI */
#endif

/* ═══════════════════════════════════════════════════════════════
 * 関数
 * ═══════════════════════════════════════════════════════════════*/

#ifndef MIDL_PASS
static inline int stat(const char* pathname, struct stat* statbuf) {
#if defined(__RIN_STAT_TIME64_ABI)
    return __rin_stat_time64_pathat(AT_FDCWD, pathname, statbuf, 0);
#else
    if (!pathname || !statbuf) {
        errno = EFAULT;
        return -1;
    }
    return __rin_stat_int_result(_RIN_STAT_SYSCALL2(
        SYS_STAT, pathname, statbuf));
#endif
}

static inline int fstat(int fd, struct stat* statbuf) {
#if defined(__RIN_STAT_TIME64_ABI)
    return __rin_fstat_time64(fd, statbuf);
#else
    if (!statbuf) {
        errno = EFAULT;
        return -1;
    }
    return __rin_stat_int_result(_RIN_STAT_SYSCALL2(
        SYS_FSTAT, fd, statbuf));
#endif
}

/* Return the mount/object generation pinned by an open VFS descriptor.
 * Directory descriptors are accepted by the path-at owner as well as files. */
static inline int rin_fgetidentity(
    int fd, RinPathAtFileIdentityV1* identity_out) {
    RinPathAtCallV1 call;
    intptr_t result;
    if (!identity_out) {
        errno = EFAULT;
        return -1;
    }
    __rin_stat_path_at_init(&call, RIN_PATH_AT_FGETIDENTITY);
    call.flags = AT_EMPTY_PATH;
    call.dirfd = fd;
    call.buffer = (uint64_t)(uintptr_t)identity_out;
    call.buffer_size = (uint64_t)sizeof(*identity_out);
    result = _RIN_STAT_PATH_AT_CALL(&call);
    if (result < 0) {
        errno = result >= -4095 ? (int)-result : EIO;
        return -1;
    }
    if ((uint64_t)result != sizeof(*identity_out)) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static inline int lstat(const char* pathname, struct stat* statbuf) {
#if defined(__RIN_STAT_TIME64_ABI)
    return __rin_stat_time64_pathat(AT_FDCWD, pathname, statbuf,
                                    AT_SYMLINK_NOFOLLOW);
#else
    if (!pathname || !statbuf) {
        errno = EFAULT;
        return -1;
    }
    return __rin_stat_int_result(_RIN_STAT_SYSCALL2(SYS_LSTAT, pathname, statbuf));
#endif
}

/* Device and FIFO inode creation shares the stable descriptor-relative path
 * ABI so path resolution and mutation authorization remain kernel-owned. */
static inline int mknodat(int dirfd, const char* pathname, mode_t mode,
                          dev_t dev) {
    RinPathAtCallV1 call;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    __rin_stat_path_at_init(&call, RIN_PATH_AT_MKNOD);
    call.dirfd = dirfd;
    call.path1 = (uint64_t)(uintptr_t)pathname;
    call.mode = (uint32_t)mode;
    call.reserved0 = (uint32_t)dev;
    return __rin_stat_int_result(_RIN_STAT_PATH_AT_CALL(&call));
}

static inline int mknod(const char* pathname, mode_t mode, dev_t dev) {
    return mknodat(AT_FDCWD, pathname, mode, dev);
}

static inline int mkfifoat(int dirfd, const char* pathname, mode_t mode) {
    return mknodat(dirfd, pathname,
                   (mode_t)(S_IFIFO | (mode & 07777)), 0);
}

static inline int mkfifo(const char* pathname, mode_t mode) {
    return mkfifoat(AT_FDCWD, pathname, mode);
}

static inline int fstatat(int dirfd, const char* pathname, struct stat* statbuf, int flags) {
#if defined(__RIN_STAT_TIME64_ABI)
    return __rin_stat_time64_pathat(dirfd, pathname, statbuf, flags);
#else
    RinPathAtCallV1 call;
    if (!statbuf) {
        errno = EFAULT;
        return -1;
    }

    if ((flags & ~(AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW |
                   AT_NO_AUTOMOUNT)) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (!pathname) {
        if ((flags & AT_EMPTY_PATH) == 0) {
            errno = EFAULT;
            return -1;
        }
        pathname = "";
    }
    if ((flags & AT_EMPTY_PATH) != 0) {
        unsigned char first_byte;
        if (__rin_user_snapshot_byte(pathname, &first_byte) != 0)
            return -1;
        if (first_byte == '\0') {
            if (dirfd != AT_FDCWD) {
                /* AT_EMPTY_PATH resolves the already-open description.
                 * NOFOLLOW cannot change the object selected by a normal FD. */
                return fstat(dirfd, statbuf);
            }
            /* Resolve the current working directory as ".". */
            pathname = ".";
        }
    }
    __rin_stat_path_at_init(&call, RIN_PATH_AT_STAT);
    /* AT_EMPTY_PATH only changes lookup when pathname is empty. Keep an
     * ordinary nonempty lookup valid and avoid passing the no-op bit to the
     * kernel path-at contract, which reserves it for empty-path operations. */
    call.flags = (uint32_t)(flags & ~AT_EMPTY_PATH);
    call.dirfd = dirfd;
    call.path1 = (uint64_t)(uintptr_t)pathname;
    call.buffer = (uint64_t)(uintptr_t)statbuf;
    call.buffer_size = sizeof(*statbuf);
    return __rin_stat_path_at_result(_RIN_STAT_PATH_AT_CALL(&call));
#endif
}

#if !defined(__RIN_SYS_TYPES_HOST_OWNER)
/* Query fixed-width metadata without changing either legacy stat layout.
 * statx also covers an open descriptor through AT_EMPTY_PATH. */
static inline int statx(int dirfd, const char* pathname, int flags,
                        unsigned int mask, struct statx* statbuf) {
    RinPathAtCallV1 call;
    if (!statbuf) {
        errno = EFAULT;
        return -1;
    }
    if ((flags & ~(AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW |
                   AT_NO_AUTOMOUNT | AT_STATX_SYNC_TYPE)) != 0 ||
        (flags & AT_STATX_SYNC_TYPE) == AT_STATX_SYNC_TYPE) {
        errno = EINVAL;
        return -1;
    }
    if (!pathname) {
        if ((flags & AT_EMPTY_PATH) == 0) {
            errno = EFAULT;
            return -1;
        }
        pathname = "";
    }
    __rin_stat_path_at_init(&call, RIN_PATH_AT_STATX);
    call.flags = (uint32_t)flags;
    call.dirfd = dirfd;
    call.mode = (uint32_t)mask;
    call.path1 = (uint64_t)(uintptr_t)pathname;
    call.buffer = (uint64_t)(uintptr_t)statbuf;
    call.buffer_size = RIN_STATX_V1_SIZE;
    return __rin_stat_path_at_result(_RIN_STAT_PATH_AT_CALL(&call));
}

static inline int fstatx(int fd, unsigned int mask, struct statx* statbuf) {
    return statx(fd, "", AT_EMPTY_PATH, mask, statbuf);
}
#endif /* !__RIN_SYS_TYPES_HOST_OWNER */

#if !defined(__RIN_SYS_TYPES_HOST_OWNER)
/* Explicit 64-bit metadata calls share the versioned path-at buffer contract.
 * fstat64 has a dedicated syscall so old two-argument fstat callers never
 * depend on unspecified values in unused syscall registers. */
static inline int fstat64(int fd, struct stat64* statbuf) {
    if (!statbuf) {
        errno = EFAULT;
        return -1;
    }
    return __rin_stat_int_result(_RIN_STAT_SYSCALL2(
        SYS_FSTAT64, fd, statbuf));
}

static inline int fstatat64(int dirfd, const char* pathname,
                            struct stat64* statbuf, int flags) {
    RinPathAtCallV1 call;

    if (!statbuf) {
        errno = EFAULT;
        return -1;
    }
    if ((flags & ~(AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW |
                   AT_NO_AUTOMOUNT)) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (!pathname) {
        if ((flags & AT_EMPTY_PATH) == 0) {
            errno = EFAULT;
            return -1;
        }
        pathname = "";
    }
    if ((flags & AT_EMPTY_PATH) != 0) {
        unsigned char first_byte;
        if (__rin_user_snapshot_byte(pathname, &first_byte) != 0)
            return -1;
        if (first_byte == '\0') {
            if (dirfd != AT_FDCWD) return fstat64(dirfd, statbuf);
            pathname = ".";
        }
        flags &= ~AT_EMPTY_PATH;
    }
    __rin_stat_path_at_init(&call, RIN_PATH_AT_STAT);
    call.flags = (uint32_t)(flags & ~AT_EMPTY_PATH);
    call.dirfd = dirfd;
    call.path1 = (uint64_t)(uintptr_t)pathname;
    call.buffer = (uint64_t)(uintptr_t)statbuf;
    call.buffer_size = sizeof(*statbuf);
    return __rin_stat_path_at_result(_RIN_STAT_PATH_AT_CALL(&call));
}

static inline int stat64(const char* pathname, struct stat64* statbuf) {
    return fstatat64(AT_FDCWD, pathname, statbuf, 0);
}

static inline int lstat64(const char* pathname, struct stat64* statbuf) {
    return fstatat64(AT_FDCWD, pathname, statbuf, AT_SYMLINK_NOFOLLOW);
}
#endif /* !__RIN_SYS_TYPES_HOST_OWNER */

/* Capability-only fstatat for WASI preopens. Unlike fstatat(), this cannot
 * fall back to the process CWD or traverse outside the retained dirfd. */
static inline int rin_fstatat_beneath(int dirfd, const char* pathname,
#if defined(__RIN_FILE_OFFSET_BITS64) && \
    !defined(__RIN_SYS_TYPES_HOST_OWNER)
                                      struct stat64* statbuf,
#else
                                      struct stat* statbuf,
#endif
                                      int flags) {
    intptr_t result;
    RinPathAtCallV1 call;

    if (!pathname || !statbuf) {
        errno = EFAULT;
        return -1;
    }
    if ((flags & ~AT_SYMLINK_NOFOLLOW) != 0) {
        errno = EINVAL;
        return -1;
    }
    __rin_stat_path_at_init(&call, RIN_PATH_AT_STAT_BENEATH);
    call.flags = (uint32_t)flags;
    call.dirfd = dirfd;
    call.path1 = (uint64_t)(uintptr_t)pathname;
    call.buffer = (uint64_t)(uintptr_t)statbuf;
    call.buffer_size = sizeof(*statbuf);
    result = _RIN_STAT_PATH_AT_CALL(&call);
    return __rin_stat_path_at_result(result);
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
    if (((unsigned long)mode & ~(unsigned long)01777u) != 0u) {
        errno = EINVAL;
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
    if (((unsigned long)mode & ~(unsigned long)01777u) != 0u) {
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
    if ((flags & AT_EMPTY_PATH) != 0) {
        unsigned char first_byte;
        if (__rin_user_snapshot_byte(pathname, &first_byte) != 0)
            return -1;
        if (first_byte == '\0') {
            if (dirfd == AT_FDCWD) {
                pathname = ".";
                flags &= ~AT_EMPTY_PATH;
            } else {
                if (((unsigned long)mode & ~(unsigned long)01777u) != 0u) {
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
        } else {
            flags &= ~AT_EMPTY_PATH;
        }
    }
    if ((flags & AT_EMPTY_PATH) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (((unsigned long)mode & ~(unsigned long)01777u) != 0u) {
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
#endif /* !MIDL_PASS */

#ifdef __cplusplus
}
#endif

/* POSIX declares futimens() through <sys/stat.h>. */
#include "../utime.h"

#if defined(__RIN_FILE_OFFSET_BITS64) && \
    !defined(__RIN_SYS_TYPES_HOST_OWNER) && !defined(MIDL_PASS)
/* POSIX source-level redirection also changes `struct stat` to the 64-bit ABI
 * tag for translation units that define _FILE_OFFSET_BITS=64. */
#define stat stat64
#define fstat fstat64
#define lstat lstat64
#define fstatat fstatat64
#endif

#endif /* _SYS_STAT_H */
#endif /* hosted native filesystem CRT */
