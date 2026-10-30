/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - sys/sendfile.h
 * Bounded kernel-owned file-to-socket transfer.
 */

#ifndef _SYS_SENDFILE_H
#define _SYS_SENDFILE_H

#include "../unistd.h"
#include "types.h"
#include <rin/contract_abi.h>

#ifndef MIDL_PASS
static inline ssize_t _rin_sendfile_transfer_v2(
    int source_fd, int socket_fd, uint64_t offset_address,
    uint32_t offset_width_bits, size_t count) {
    size_t remaining = count;
    size_t transferred = 0u;
    const size_t max_transfer = (size_t)(16u * 1024u * 1024u);
    int issue_zero_length = count == 0u;

    if (remaining > (size_t)SSIZE_MAX) remaining = (size_t)SSIZE_MAX;
    do {
        RinFileToSocketRequestV2 request = {0};
        size_t chunk = remaining > max_transfer ? max_transfer : remaining;
        intptr_t result;

        request.struct_size = (uint32_t)sizeof(request);
        request.version = 2u;
        request.source_fd = source_fd;
        request.socket_fd = socket_fd;
        request.offset_width_bits = offset_width_bits;
        request.offset_address = offset_address;
        request.length = (uint64_t)chunk;
        result = _rin_unistd_result(_RIN_UNISTD_SYSCALL1(
            SYS_FILE_TO_SOCKET, (uintptr_t)&request));
        if (result < 0)
            return transferred != 0u ? (ssize_t)transferred : (ssize_t)-1;
        if ((uintptr_t)result > (uintptr_t)chunk) {
            errno = EIO;
            return transferred != 0u ? (ssize_t)transferred : (ssize_t)-1;
        }
        transferred += (size_t)result;
        if ((size_t)result < chunk || result == 0) break;
        if (issue_zero_length) break;
        remaining -= (size_t)result;
    } while (remaining != 0u);
    return (ssize_t)transferred;
}

/* Linux互換sendfile。offset指定時はその値だけを進め、NULL時は共有FD位置を進める。 */
static inline ssize_t sendfile(int out_fd, int in_fd, off_t* offset,
                               size_t count) {
    const uint32_t width = offset != NULL
        ? (uint32_t)(sizeof(*offset) * CHAR_BIT) : 0u;
    if (offset != NULL && width != 32u && width != 64u) {
        errno = EINVAL;
        return (ssize_t)-1;
    }
    return _rin_sendfile_transfer_v2(
        in_fd, out_fd, (uint64_t)(uintptr_t)offset, width, count);
}

static inline ssize_t sendfile64(int out_fd, int in_fd, off64_t* offset,
                                 size_t count) {
    return _rin_sendfile_transfer_v2(
        in_fd, out_fd, (uint64_t)(uintptr_t)offset,
        offset != NULL ? 64u : 0u, count);
}

static inline ssize_t rin_file_to_socket(int source_fd, int socket_fd,
                                         uint64_t offset, uint64_t length,
                                         uint32_t flags) {
    RinFileToSocketRequestV1 request = {0};
    intptr_t result;

    request.struct_size = (uint32_t)sizeof(request);
    request.version = 1u;
    request.source_fd = source_fd;
    request.socket_fd = socket_fd;
    request.flags = flags;
    request.offset = offset;
    request.length = length;
    result = _rin_unistd_result(_RIN_UNISTD_SYSCALL1(
        SYS_FILE_TO_SOCKET, (uintptr_t)&request));
    if (result < 0) return (ssize_t)-1;
    if ((uintptr_t)result > (uintptr_t)SSIZE_MAX) {
        errno = EOVERFLOW;
        return (ssize_t)-1;
    }
    return (ssize_t)result;
}
#endif

#endif
