/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - sys/sendfile.h
 * Bounded kernel-owned file-to-socket transfer.
 */

#ifndef _SYS_SENDFILE_H
#define _SYS_SENDFILE_H

#include "../unistd.h"
#include <rin/contract_abi.h>

#ifndef MIDL_PASS
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
