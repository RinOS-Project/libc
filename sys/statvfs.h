/*
 * RinOS libc - sys/statvfs.h
 * Minimal filesystem statistics compatibility.
 */

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    defined(__linux__) && (defined(__GNUC__) || defined(__clang__)) && \
    !defined(_RIN_STATVFS_SYSCALL2)
#include_next <sys/statvfs.h>
#else

#ifndef _SYS_STATVFS_H
#define _SYS_STATVFS_H

#include "../errno.h"
#include "../limits.h"
#include "types.h"
#include "syscall.h"
#include "stat.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long fsblkcnt_t;
typedef unsigned long fsfilcnt_t;

struct statvfs {
    unsigned long  f_bsize;
    unsigned long  f_frsize;
    fsblkcnt_t     f_blocks;
    fsblkcnt_t     f_bfree;
    fsblkcnt_t     f_bavail;
    fsfilcnt_t     f_files;
    fsfilcnt_t     f_ffree;
    fsfilcnt_t     f_favail;
    unsigned long  f_fsid;
    unsigned long  f_flag;
    unsigned long  f_namemax;
};

/* Wire layout mirrors the schema-owned RinFilesystemUsageV1 without making a
 * POSIX compatibility header expose the complete RinNative contract header. */
typedef struct __rin_statvfs_usage_v1 {
    uint32_t struct_size;
    uint16_t version;
    uint16_t filesystem_type;
    uint64_t flags;
    uint32_t block_size;
    uint32_t name_max;
    uint64_t total_blocks;
    uint64_t free_blocks;
    uint64_t available_blocks;
    uint64_t total_inodes;
    uint64_t free_inodes;
    uint8_t volume_name[24];
    uint64_t reserved;
} __rin_statvfs_usage_v1;

#if defined(__cplusplus)
static_assert(sizeof(__rin_statvfs_usage_v1) == 96,
              "statvfs filesystem usage wire layout changed");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(__rin_statvfs_usage_v1) == 96,
               "statvfs filesystem usage wire layout changed");
#endif

#define __RIN_FILESYSTEM_USAGE_VERSION 1u
#define __RIN_FILESYSTEM_USAGE_FLAG_BLOCKS_VALID 1u
#define __RIN_FILESYSTEM_USAGE_FLAG_READ_ONLY 4u
#define __RIN_FILESYSTEM_USAGE_FLAG_NOEXEC 64u
#define __RIN_FILESYSTEM_USAGE_FLAG_NOSUID 128u
#define __RIN_FILESYSTEM_USAGE_FLAG_NODEV 256u

#ifndef ST_RDONLY
#define ST_RDONLY 1u
#endif
#ifndef ST_NOSUID
#define ST_NOSUID 2u
#endif
#ifndef ST_NODEV
#define ST_NODEV 4u
#endif
#ifndef ST_NOEXEC
#define ST_NOEXEC 8u
#endif

#ifndef _RIN_STATVFS_SYSCALL2
#define _RIN_STATVFS_SYSCALL2(number, path, output) \
    _syscall2((uintptr_t)(number), (uintptr_t)(path), (uintptr_t)(output))
#endif

#ifndef _RIN_STATVFS_SYSCALL3
#define _RIN_STATVFS_SYSCALL3(number, a1, a2, a3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(a1), (uintptr_t)(a2), \
              (uintptr_t)(a3))
#endif

/* The filesystem-usage syscall is status-only on the wire, but the public
 * POSIX wrapper returns int.  Keep the target-width result intact until the
 * status boundary: preserve POSIXized errno, reject unknown negative words,
 * and do not turn a non-zero or unrepresentable word into success. */
static inline int __rin_statvfs_status_result(intptr_t result) {
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

static inline int statvfs(const char* path, struct statvfs* buf) {
    __rin_statvfs_usage_v1 usage;
    intptr_t result;
    size_t index;
    for (index = 0u; index < sizeof(usage); ++index)
        ((unsigned char*)&usage)[index] = 0u;
    if (!path || !buf) {
        errno = EFAULT;
        return -1;
    }
    usage.struct_size = sizeof(usage);
    usage.version = __RIN_FILESYSTEM_USAGE_VERSION;
    result = _RIN_STATVFS_SYSCALL2(
        RIN_SYS_FILESYSTEM_USAGE_GET, path, &usage);
    if (__rin_statvfs_status_result(result) != 0) return -1;
    if ((usage.flags & __RIN_FILESYSTEM_USAGE_FLAG_BLOCKS_VALID) == 0u ||
        usage.block_size == 0u || usage.free_blocks > usage.total_blocks ||
        usage.available_blocks > usage.free_blocks) {
        errno = EIO;
        return -1;
    }
    if (usage.total_blocks > (uint64_t)(fsblkcnt_t)-1 ||
        usage.free_blocks > (uint64_t)(fsblkcnt_t)-1 ||
        usage.available_blocks > (uint64_t)(fsblkcnt_t)-1 ||
        usage.total_inodes > (uint64_t)(fsfilcnt_t)-1 ||
        usage.free_inodes > (uint64_t)(fsfilcnt_t)-1) {
        errno = EOVERFLOW;
        return -1;
    }

    buf->f_bsize = usage.block_size;
    buf->f_frsize = usage.block_size;
    buf->f_blocks = (fsblkcnt_t)usage.total_blocks;
    buf->f_bfree = (fsblkcnt_t)usage.free_blocks;
    buf->f_bavail = (fsblkcnt_t)usage.available_blocks;
    buf->f_files = (fsfilcnt_t)usage.total_inodes;
    buf->f_ffree = (fsfilcnt_t)usage.free_inodes;
    buf->f_favail = (fsfilcnt_t)usage.free_inodes;
    buf->f_fsid = usage.filesystem_type;
    buf->f_flag = 0u;
    if ((usage.flags & __RIN_FILESYSTEM_USAGE_FLAG_READ_ONLY) != 0u)
        buf->f_flag |= ST_RDONLY;
    if ((usage.flags & __RIN_FILESYSTEM_USAGE_FLAG_NOSUID) != 0u)
        buf->f_flag |= ST_NOSUID;
    if ((usage.flags & __RIN_FILESYSTEM_USAGE_FLAG_NODEV) != 0u)
        buf->f_flag |= ST_NODEV;
    if ((usage.flags & __RIN_FILESYSTEM_USAGE_FLAG_NOEXEC) != 0u)
        buf->f_flag |= ST_NOEXEC;
    buf->f_namemax = usage.name_max;
    return 0;
}


/* Decimal writer for /proc/self/fd/<fd> (sys headers avoid stdio here). */
static inline void __rin_statvfs_write_uint(char* out, size_t* length,
                                            unsigned int value) {
    char digits[10];
    size_t count = 0u;
    size_t index;
    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    for (index = 0u; index < count; ++index)
        out[(*length)++] = digits[count - index - 1u];
}

/* readlink(/proc/self/fd/<fd>) via direct syscall: unistd.h owns the
 * public readlink, but including it here would cycle (unistd.h includes
 * this header).  Result is NUL-terminated; unlinked fds and missing
 * procfs surface as ENOENT from the kernel. */
static inline ssize_t __rin_statvfs_readlink_fd(int fd, char* buf,
                                                size_t size) {
    static const char prefix[] = "/proc/self/fd/";
    char path[14u + 10u + 1u];
    size_t length = 0u;
    size_t index;
    intptr_t result;
    if (!buf || size < 2u) {
        errno = EINVAL;
        return -1;
    }
    for (index = 0u; prefix[index] != '\0'; ++index)
        path[length++] = prefix[index];
    __rin_statvfs_write_uint(path, &length, (unsigned int)fd);
    path[length] = '\0';
    result = __rin_syscall_posixize(_RIN_STATVFS_SYSCALL3(
        SYS_READLINK, path, buf, size - 1u));
    if (result < 0) return -1;
    if ((uintptr_t)result >= (uintptr_t)(size - 1u)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    if ((uintptr_t)result > (uintptr_t)SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    buf[result] = '\0';
    return (ssize_t)result;
}

static inline int fstatvfs(int fd, struct statvfs* buf) {
    struct stat file_info;
    char target[4096u + 1u];
    if (!buf) {
        errno = EFAULT;
        return -1;
    }
    /* fstat supplies POSIX EBADF (procfs readlink reports ENOENT for
     * dead fds) and gates non-file fds to honest EINVAL: pipes and
     * sockets have no path to re-resolve.  True kernel fd-usage
     * (unlinked-safe, fstype-complete) is tracked separately. */
    if (fstat(fd, &file_info) != 0) return -1;
    if (!S_ISREG(file_info.st_mode) && !S_ISDIR(file_info.st_mode)) {
        errno = EINVAL;
        return -1;
    }
    if (__rin_statvfs_readlink_fd(fd, target, sizeof(target)) < 0)
        return -1;
    return statvfs(target, buf);
}
#ifdef __cplusplus
}
#endif

#endif /* _SYS_STATVFS_H */

#endif /* hosted Linux delegates to its native filesystem usage provider */
