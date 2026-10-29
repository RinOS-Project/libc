/* SPDX-License-Identifier: MIT */
/* RinOS libc splice-family wrappers. */
#ifndef _SYS_SPLICE_H
#define _SYS_SPLICE_H

#include "../unistd.h"
#include "../sys/uio.h"
#include "../limits.h"
#include <rin/splice_abi.h>

#ifndef SPLICE_F_MOVE
#define SPLICE_F_MOVE     RIN_SPLICE_F_MOVE
#endif
#ifndef SPLICE_F_NONBLOCK
#define SPLICE_F_NONBLOCK RIN_SPLICE_F_NONBLOCK
#endif
#ifndef SPLICE_F_MORE
#define SPLICE_F_MORE     RIN_SPLICE_F_MORE
#endif
#ifndef SPLICE_F_GIFT
#define SPLICE_F_GIFT     RIN_SPLICE_F_GIFT
#endif

#ifndef MIDL_PASS
static inline ssize_t _rin_splice_call(RinSpliceCallV1* request)
{
    intptr_t result;
    request->struct_size = (uint32_t)sizeof(*request);
    if (request->version == 0u)
        request->version = RIN_SPLICE_CALL_ABI_VERSION_V1;
    result = _rin_unistd_result(_RIN_UNISTD_SYSCALL1(
        RIN_SYS_SPLICE_CALL_V1, (uintptr_t)request));
    if (result < 0) return (ssize_t)-1;
    if ((uintptr_t)result > (uintptr_t)SSIZE_MAX) {
        errno = EOVERFLOW;
        return (ssize_t)-1;
    }
    return (ssize_t)result;
}

static inline ssize_t splice(int fd_in, off_t* off_in, int fd_out,
                             off_t* off_out, size_t length,
                             unsigned int flags)
{
    RinSpliceCallV1 request = {0};
    request.operation = RIN_SPLICE_OP_SPLICE;
    request.version = RIN_SPLICE_CALL_ABI_VERSION_V2;
    request.reserved[0] = (uint64_t)(sizeof(off_t) * 8u);
    request.fd_in = fd_in;
    request.fd_out = fd_out;
    request.flags = flags;
    request.length = (uint64_t)length;
    request.offset_in_pointer = (uint64_t)(uintptr_t)off_in;
    request.offset_out_pointer = (uint64_t)(uintptr_t)off_out;
    return _rin_splice_call(&request);
}

static inline ssize_t tee(int fd_in, int fd_out, size_t length,
                          unsigned int flags)
{
    RinSpliceCallV1 request = {0};
    request.operation = RIN_SPLICE_OP_TEE;
    request.fd_in = fd_in;
    request.fd_out = fd_out;
    request.flags = flags;
    request.length = (uint64_t)length;
    return _rin_splice_call(&request);
}

static inline ssize_t vmsplice(int fd, const struct iovec* iov,
                               size_t iov_count, unsigned int flags)
{
    RinSpliceCallV1 request = {0};
    if (iov_count > UINT32_MAX) {
        errno = EINVAL;
        return (ssize_t)-1;
    }
    request.operation = RIN_SPLICE_OP_VMSPLICE;
    request.fd_in = fd;
    request.fd_out = -1;
    request.flags = flags;
    request.iov_count = (uint32_t)iov_count;
    request.iov_pointer = (uint64_t)(uintptr_t)iov;
    return _rin_splice_call(&request);
}

static inline ssize_t copy_file_range(int fd_in, off_t* off_in,
                                      int fd_out, off_t* off_out,
                                      size_t length, unsigned int flags)
{
    RinSpliceCallV1 request = {0};
    request.operation = RIN_SPLICE_OP_COPY_FILE_RANGE;
    request.version = RIN_SPLICE_CALL_ABI_VERSION_V2;
    request.reserved[0] = (uint64_t)(sizeof(off_t) * 8u);
    request.fd_in = fd_in;
    request.fd_out = fd_out;
    request.flags = flags;
    request.length = (uint64_t)length;
    request.offset_in_pointer = (uint64_t)(uintptr_t)off_in;
    request.offset_out_pointer = (uint64_t)(uintptr_t)off_out;
    return _rin_splice_call(&request);
}
#endif /* !MIDL_PASS */

#endif /* _SYS_SPLICE_H */
