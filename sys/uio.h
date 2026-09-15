/* SPDX-License-Identifier: MIT */
/* RinOS libc - sys/uio.h */

#ifndef _SYS_UIO_H
#define _SYS_UIO_H

#include "types.h"
#include "syscall.h"
#include "../stdint.h"
#include "../limits.h"
#include "../errno.h"

#ifndef RIN_LIBC_IOVEC_DEFINED
#define RIN_LIBC_IOVEC_DEFINED 1
struct iovec {
    void*  iov_base;
    size_t iov_len;
};
#endif

#ifndef RIN_UIO_MAX_IOV
#define RIN_UIO_MAX_IOV 64u
#endif

#ifndef _RIN_UIO_SYSCALL3
#define _RIN_UIO_SYSCALL3(number, argument1, argument2, argument3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3))
#endif

static inline ssize_t __rin_uio_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        errno = result >= -4095 ? (int)-result : EIO;
        return (ssize_t)-1;
    }
    if ((uintptr_t)result > (uintptr_t)SSIZE_MAX) {
        errno = EOVERFLOW;
        return (ssize_t)-1;
    }
    return (ssize_t)result;
}

static inline ssize_t readv(int fd, const struct iovec* iov, int iovcnt) {
    intptr_t result;
    if (iovcnt < 0 || (unsigned int)iovcnt > RIN_UIO_MAX_IOV ||
        (iovcnt != 0 && !iov)) {
        errno = EINVAL;
        return (ssize_t)-1;
    }
    result = _RIN_UIO_SYSCALL3(SYS_READV, (uintptr_t)(intptr_t)fd,
                               (uintptr_t)iov, (uintptr_t)(unsigned)iovcnt);
    return __rin_uio_result(result);
}

static inline ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
    intptr_t result;
    if (iovcnt < 0 || (unsigned int)iovcnt > RIN_UIO_MAX_IOV ||
        (iovcnt != 0 && !iov)) {
        errno = EINVAL;
        return (ssize_t)-1;
    }
    result = _RIN_UIO_SYSCALL3(SYS_WRITEV, (uintptr_t)(intptr_t)fd,
                               (uintptr_t)iov, (uintptr_t)(unsigned)iovcnt);
    return __rin_uio_result(result);
}

#endif /* _SYS_UIO_H */
