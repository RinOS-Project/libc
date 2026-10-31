/* SPDX-License-Identifier: MIT */
/* RinOS libc - sys/uio.h */

#ifndef _SYS_UIO_H
#define _SYS_UIO_H

#include "types.h"
#include "syscall.h"
#include "../stdint.h"
#include "../limits.h"
#include "../errno.h"
#include "../internal/pthread_cancel_point.h"
#include <sys/types.h>
#include <rin/process/vm_iov_abi.h>

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
#ifndef _RIN_UIO_SYSCALL5
#define _RIN_UIO_SYSCALL5(number, argument1, argument2, argument3, argument4, argument5) \
    _syscall5((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3), \
              (uintptr_t)(argument4), (uintptr_t)(argument5))
#endif
#ifndef _RIN_UIO_SYSCALL6
#define _RIN_UIO_SYSCALL6(number, argument1, argument2, argument3, argument4, argument5, argument6) \
    _syscall6((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3), \
              (uintptr_t)(argument4), (uintptr_t)(argument5), \
              (uintptr_t)(argument6))
#endif

#ifndef RWF_DSYNC
#define RWF_DSYNC 0x00000002
#endif
#ifndef RWF_HIPRI
#define RWF_HIPRI 0x00000001
#endif
#ifndef RWF_SYNC
#define RWF_SYNC 0x00000004
#endif
#ifndef RWF_NOWAIT
#define RWF_NOWAIT 0x00000008
#endif
#ifndef RWF_APPEND
#define RWF_APPEND 0x00000010
#endif

static inline ssize_t __rin_uio_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        if (result != -1) errno = EIO;
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
    __rin_pthread_cancel_point_if_linked();
    if (iovcnt < 0 || (unsigned int)iovcnt > RIN_UIO_MAX_IOV ||
        (iovcnt != 0 && !iov)) {
        errno = EINVAL;
        return (ssize_t)-1;
    }
    result = _RIN_UIO_SYSCALL3(SYS_READV, (uintptr_t)(intptr_t)fd,
                               (uintptr_t)iov, (uintptr_t)(unsigned)iovcnt);
    __rin_pthread_cancel_point_if_linked();
    return __rin_uio_result(result);
}

static inline ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
    intptr_t result;
    __rin_pthread_cancel_point_if_linked();
    if (iovcnt < 0 || (unsigned int)iovcnt > RIN_UIO_MAX_IOV ||
        (iovcnt != 0 && !iov)) {
        errno = EINVAL;
        return (ssize_t)-1;
    }
    result = _RIN_UIO_SYSCALL3(SYS_WRITEV, (uintptr_t)(intptr_t)fd,
                               (uintptr_t)iov, (uintptr_t)(unsigned)iovcnt);
    __rin_pthread_cancel_point_if_linked();
    return __rin_uio_result(result);
}

static inline ssize_t __rin_process_vm_iov(
    pid_t pid, const struct iovec* local_iov, int local_iov_count,
    const struct iovec* remote_iov, int remote_iov_count,
    unsigned int direction, unsigned int flags) {
    RinProcessVmIovRequestV1 request = {0};
    intptr_t result;
    if (pid <= 0 || local_iov_count < 0 || remote_iov_count < 0 ||
        (unsigned int)local_iov_count > RIN_PROCESS_VM_IOV_MAX_COUNT ||
        (unsigned int)remote_iov_count > RIN_PROCESS_VM_IOV_MAX_COUNT ||
        (local_iov_count != 0 && local_iov == 0) ||
        (remote_iov_count != 0 && remote_iov == 0) || flags != 0u ||
        (direction != RIN_PROCESS_VM_IOV_READ_REMOTE &&
         direction != RIN_PROCESS_VM_IOV_WRITE_REMOTE)) {
        errno = EINVAL;
        return (ssize_t)-1;
    }
    request.struct_size = sizeof(request);
    request.version = RIN_PROCESS_VM_IOV_ABI_VERSION;
    request.direction = (uint16_t)direction;
    request.flags = flags;
    request.target_pid = pid;
    request.local_iov_count = (uint32_t)local_iov_count;
    request.remote_iov_count = (uint32_t)remote_iov_count;
    request.iovec_width = (uint32_t)sizeof(uintptr_t);
    request.local_iov_address = (uint64_t)(uintptr_t)local_iov;
    request.remote_iov_address = (uint64_t)(uintptr_t)remote_iov;
    result = _syscall1(SYS_PROCESS_VM_IOV, (uintptr_t)&request);
    return __rin_uio_result(result);
}

static inline ssize_t process_vm_readv(pid_t pid,
                                       const struct iovec* local_iov,
                                       unsigned long local_iov_count,
                                       const struct iovec* remote_iov,
                                       unsigned long remote_iov_count,
                                       unsigned long flags) {
    if (local_iov_count > (unsigned long)INT_MAX ||
        remote_iov_count > (unsigned long)INT_MAX ||
        flags > (unsigned long)UINT_MAX) {
        errno = EINVAL;
        return (ssize_t)-1;
    }
    return __rin_process_vm_iov(
        pid, local_iov, (int)local_iov_count, remote_iov,
        (int)remote_iov_count, RIN_PROCESS_VM_IOV_READ_REMOTE,
        (unsigned int)flags);
}

static inline ssize_t process_vm_writev(pid_t pid,
                                        const struct iovec* local_iov,
                                        unsigned long local_iov_count,
                                        const struct iovec* remote_iov,
                                        unsigned long remote_iov_count,
                                        unsigned long flags) {
    if (local_iov_count > (unsigned long)INT_MAX ||
        remote_iov_count > (unsigned long)INT_MAX ||
        flags > (unsigned long)UINT_MAX) {
        errno = EINVAL;
        return (ssize_t)-1;
    }
    return __rin_process_vm_iov(
        pid, local_iov, (int)local_iov_count, remote_iov,
        (int)remote_iov_count, RIN_PROCESS_VM_IOV_WRITE_REMOTE,
        (unsigned int)flags);
}

static inline ssize_t __rin_uio_positional(int fd, const struct iovec* iov,
                                           int iovcnt, int64_t offset,
                                           int writing) {
    intptr_t result;
    __rin_pthread_cancel_point_if_linked();
    if (offset < 0 || iovcnt < 0 ||
        (unsigned int)iovcnt > RIN_UIO_MAX_IOV ||
        (iovcnt != 0 && !iov)) {
        errno = EINVAL;
        return (ssize_t)-1;
    }
    result = _RIN_UIO_SYSCALL5(
        writing ? SYS_PWRITEV : SYS_PREADV,
        (uintptr_t)(intptr_t)fd, (uintptr_t)iov, (uintptr_t)(unsigned)iovcnt,
        (uintptr_t)(uint32_t)(uint64_t)offset,
        (uintptr_t)(uint32_t)((uint64_t)offset >> 32));
    __rin_pthread_cancel_point_if_linked();
    return __rin_uio_result(result);
}

static inline ssize_t preadv(int fd, const struct iovec* iov, int iovcnt,
                             off_t offset) {
    return __rin_uio_positional(fd, iov, iovcnt, (int64_t)offset, 0);
}

static inline ssize_t pwritev(int fd, const struct iovec* iov, int iovcnt,
                              off_t offset) {
    return __rin_uio_positional(fd, iov, iovcnt, (int64_t)offset, 1);
}

static inline ssize_t preadv64(int fd, const struct iovec* iov, int iovcnt,
                               off64_t offset) {
    return __rin_uio_positional(fd, iov, iovcnt, (int64_t)offset, 0);
}

static inline ssize_t pwritev64(int fd, const struct iovec* iov, int iovcnt,
                                off64_t offset) {
    return __rin_uio_positional(fd, iov, iovcnt, (int64_t)offset, 1);
}

static inline ssize_t __rin_uio_positional2(int fd, const struct iovec* iov,
                                            int iovcnt, int64_t offset,
                                            int flags, int writing) {
    intptr_t result;
    __rin_pthread_cancel_point_if_linked();
    if (iovcnt < 0 || (unsigned int)iovcnt > RIN_UIO_MAX_IOV ||
        (iovcnt != 0 && !iov)) {
        errno = EINVAL;
        return (ssize_t)-1;
    }
    result = _RIN_UIO_SYSCALL6(
        writing ? SYS_PWRITEV2 : SYS_PREADV2,
        (uintptr_t)(intptr_t)fd, (uintptr_t)iov, (uintptr_t)(unsigned)iovcnt,
        (uintptr_t)(uint32_t)(uint64_t)offset,
        (uintptr_t)(uint32_t)((uint64_t)offset >> 32),
        (uintptr_t)(unsigned int)flags);
    __rin_pthread_cancel_point_if_linked();
    return __rin_uio_result(result);
}

static inline ssize_t preadv2(int fd, const struct iovec* iov, int iovcnt,
                              off_t offset, int flags) {
    return __rin_uio_positional2(fd, iov, iovcnt, (int64_t)offset, flags, 0);
}

static inline ssize_t pwritev2(int fd, const struct iovec* iov, int iovcnt,
                               off_t offset, int flags) {
    return __rin_uio_positional2(fd, iov, iovcnt, (int64_t)offset, flags, 1);
}

static inline ssize_t preadv64v2(int fd, const struct iovec* iov, int iovcnt,
                                 off64_t offset, int flags) {
    return __rin_uio_positional2(fd, iov, iovcnt, (int64_t)offset, flags, 0);
}

static inline ssize_t pwritev64v2(int fd, const struct iovec* iov, int iovcnt,
                                  off64_t offset, int flags) {
    return __rin_uio_positional2(fd, iov, iovcnt, (int64_t)offset, flags, 1);
}

#endif /* _SYS_UIO_H */
