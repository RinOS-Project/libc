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
#include <rin/contract_abi.h>

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

#ifndef _RIN_STATVFS_SYSCALL1
#define _RIN_STATVFS_SYSCALL1(number, request) \
    _syscall1((uintptr_t)(number), (uintptr_t)(request))
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
    RinStatVfsRequestV1 request;
    intptr_t result;
    if (!path || !buf) {
        errno = EFAULT;
        return -1;
    }
    request.struct_size = sizeof(request);
    request.version = RIN_STATVFS_ABI_VERSION;
    request.word_size = sizeof(unsigned long);
    request.path = (uint64_t)(uintptr_t)path;
    request.buffer = (uint64_t)(uintptr_t)buf;
    request.reserved = 0u;
    result = _RIN_STATVFS_SYSCALL1(RIN_SYS_STATVFS_GET, &request);
    return __rin_statvfs_status_result(result);
}


static inline int fstatvfs(int fd, struct statvfs* buf) {
    RinStatVfsFdRequestV1 request;
    intptr_t result;
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    if (!buf) {
        errno = EFAULT;
        return -1;
    }
    request.struct_size = sizeof(request);
    request.version = RIN_STATVFS_ABI_VERSION;
    request.word_size = sizeof(unsigned long);
    request.fd = fd;
    request.reserved0 = 0u;
    request.buffer = (uint64_t)(uintptr_t)buf;
    result = _RIN_STATVFS_SYSCALL1(RIN_SYS_STATVFS_GET_FD, &request);
    return __rin_statvfs_status_result(result);
}
#ifdef __cplusplus
}
#endif

#endif /* _SYS_STATVFS_H */

#endif /* hosted Linux delegates to its native filesystem usage provider */
