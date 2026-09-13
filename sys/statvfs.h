/*
 * RinOS libc - sys/statvfs.h
 * Minimal filesystem statistics compatibility.
 */

#ifndef _SYS_STATVFS_H
#define _SYS_STATVFS_H

#include "../errno.h"
#include "../limits.h"
#include "types.h"
#include "syscall.h"

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

#ifndef _RIN_STATVFS_SYSCALL2
#define _RIN_STATVFS_SYSCALL2(number, path, output) \
    _syscall2((uintptr_t)(number), (uintptr_t)(path), (uintptr_t)(output))
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
    buf->f_flag =
        (usage.flags & __RIN_FILESYSTEM_USAGE_FLAG_READ_ONLY) != 0u ? 1u : 0u;
    buf->f_namemax = usage.name_max;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_STATVFS_H */
