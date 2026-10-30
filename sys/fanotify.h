/* SPDX-License-Identifier: MIT */
/* RinOS fanotify permission-event API. */
#ifndef _SYS_FANOTIFY_H
#define _SYS_FANOTIFY_H

#include "syscall.h"
#include "../errno.h"
#include "../stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FAN_ACCESS             UINT64_C(0x00000001)
#define FAN_MODIFY             UINT64_C(0x00000002)
#define FAN_ATTRIB             UINT64_C(0x00000004)
#define FAN_CLOSE_WRITE        UINT64_C(0x00000008)
#define FAN_CLOSE_NOWRITE      UINT64_C(0x00000010)
#define FAN_OPEN               UINT64_C(0x00000020)
#define FAN_MOVED_FROM         UINT64_C(0x00000040)
#define FAN_MOVED_TO           UINT64_C(0x00000080)
#define FAN_CREATE             UINT64_C(0x00000100)
#define FAN_DELETE             UINT64_C(0x00000200)
#define FAN_DELETE_SELF        UINT64_C(0x00000400)
#define FAN_MOVE_SELF          UINT64_C(0x00000800)
#define FAN_OPEN_EXEC          UINT64_C(0x00001000)
#define FAN_Q_OVERFLOW         UINT64_C(0x00004000)
#define FAN_OPEN_PERM          UINT64_C(0x00010000)
#define FAN_ACCESS_PERM        UINT64_C(0x00020000)
#define FAN_OPEN_EXEC_PERM     UINT64_C(0x00040000)

#define FAN_CLOEXEC            0x00000001u
#define FAN_NONBLOCK           0x00000002u
#define FAN_CLASS_NOTIF        0x00000000u
#define FAN_CLASS_CONTENT      0x00000004u
#define FAN_CLASS_PRE_CONTENT  0x00000008u
#define FAN_REPORT_FID         0x00000200u

#define FAN_MARK_ADD           0x00000001u
#define FAN_MARK_REMOVE        0x00000002u
#define FAN_MARK_DONT_FOLLOW   0x00000004u
#define FAN_MARK_MOUNT         0x00000010u
#define FAN_MARK_FLUSH         0x00000080u
#define FAN_MARK_FILESYSTEM    0x00000100u

#define FAN_ALLOW              1u
#define FAN_DENY               2u
#define FAN_NOFD               (-1)
#define FANOTIFY_METADATA_VERSION 3u
#define FAN_EVENT_INFO_TYPE_FID 1u

#define RIN_FANOTIFY_HANDLE_RINFS UINT32_C(0x52534631)

struct fanotify_event_metadata {
    uint32_t event_len;
    uint8_t vers;
    uint8_t reserved;
    uint16_t metadata_len;
    uint64_t mask;
    int32_t fd;  /* permission-event response token in RinOS */
    int32_t pid;
};

struct fanotify_event_info_header {
    uint8_t info_type;
    uint8_t pad;
    uint16_t len;
};

struct fanotify_event_info_fid {
    struct fanotify_event_info_header hdr;
    uint32_t fsid[2];
    uint32_t handle_bytes;
    int32_t handle_type;
    uint8_t handle[12]; /* uint64_t inode followed by uint32_t generation */
};

struct fanotify_response {
    int32_t fd; /* fanotify_event_metadata.fdの応答token */
    uint32_t response;
};

static inline int __rin_fanotify_result(intptr_t result)
{
    result = __rin_syscall_posixize(result);
    if (result < 0) return -1;
    if (result > (intptr_t)INT32_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)result;
}

static inline int fanotify_init(unsigned int flags,
                                unsigned int event_f_flags)
{
    if ((flags & ~(FAN_CLOEXEC | FAN_NONBLOCK | FAN_CLASS_CONTENT |
                   FAN_CLASS_PRE_CONTENT | FAN_REPORT_FID)) != 0u ||
        (flags & FAN_REPORT_FID) == 0u ||
        (flags & (FAN_CLASS_CONTENT | FAN_CLASS_PRE_CONTENT)) ==
            (FAN_CLASS_CONTENT | FAN_CLASS_PRE_CONTENT)) {
        errno = EINVAL;
        return -1;
    }
    return __rin_fanotify_result(_syscall2(
        (uintptr_t)SYS_FANOTIFY_INIT, (uintptr_t)flags,
        (uintptr_t)event_f_flags));
}

static inline int fanotify_mark(int fd, unsigned int flags, uint64_t mask,
                                int dirfd, const char* pathname)
{
    intptr_t result;
    if (fd < 0 ||
        ((flags & FAN_MARK_FLUSH) == 0u && pathname == (const char*)0) ||
        ((flags & FAN_MARK_FLUSH) != 0u && mask != 0u)) {
        errno = EINVAL;
        return -1;
    }
    result = _syscall6(
        (uintptr_t)SYS_FANOTIFY_MARK, (uintptr_t)(intptr_t)fd,
        (uintptr_t)flags, (uintptr_t)(uint32_t)mask,
        (uintptr_t)(uint32_t)(mask >> 32u), (uintptr_t)(intptr_t)dirfd,
        (uintptr_t)pathname);
    return __rin_fanotify_result(result);
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_FANOTIFY_H */
