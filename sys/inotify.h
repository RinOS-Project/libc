/* SPDX-License-Identifier: MIT */
/* RinOS product-backed inotify-shaped filesystem watch ABI. */

#ifndef _SYS_INOTIFY_H
#define _SYS_INOTIFY_H

#include "syscall.h"
#include "../errno.h"
#include "../stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Keep the wire values consumed by System.IO.FileSystem.Watcher.  These are
 * event-format constants, not Linux syscall numbers. */
#define IN_ACCESS       0x00000001u
#define IN_MODIFY       0x00000002u
#define IN_ATTRIB       0x00000004u
#define IN_MOVED_FROM   0x00000040u
#define IN_MOVED_TO     0x00000080u
#define IN_CREATE       0x00000100u
#define IN_DELETE       0x00000200u
#define IN_DELETE_SELF  0x00000400u
#define IN_MOVE_SELF    0x00000800u
#define IN_Q_OVERFLOW   0x00004000u
#define IN_IGNORED      0x00008000u
#define IN_ONLYDIR      0x01000000u
#define IN_DONT_FOLLOW  0x02000000u
#define IN_EXCL_UNLINK  0x04000000u
#define IN_MASK_ADD     0x20000000u
#define IN_ISDIR        0x40000000u

/* inotify_init1 flags are intentionally limited to the flags represented by
 * the product descriptor owner.  The RinOS queue is created close-on-exec;
 * the managed FileSystemWatcher uses its blocking read side while kernel
 * event publication remains nonblocking. */
#define IN_CLOEXEC      0x00080000
#define IN_NONBLOCK     0x00000800

static inline int __rin_inotify_result(intptr_t result)
{
    result = __rin_syscall_posixize(result);
    if (result < 0) return -1;
    if (result > (intptr_t)INT32_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)result;
}

static inline int inotify_init1(int flags)
{
    if ((flags & ~(IN_CLOEXEC | IN_NONBLOCK)) != 0) {
        errno = EINVAL;
        return -1;
    }
    return __rin_inotify_result(_syscall1(
        (uintptr_t)SYS_FILESYSTEM_WATCH_INIT, (uintptr_t)(uint32_t)flags));
}

static inline int inotify_init(void)
{
    return inotify_init1(0);
}

static inline int inotify_add_watch(int fd, const char* pathname,
                                    uint32_t mask)
{
    intptr_t result;
    if (fd < 0 || pathname == (const char*)0 || mask == 0u) {
        errno = EINVAL;
        return -1;
    }
    result = _syscall3((uintptr_t)SYS_FILESYSTEM_WATCH_ADD,
                       (uintptr_t)(intptr_t)fd, (uintptr_t)pathname,
                       (uintptr_t)mask);
    return __rin_inotify_result(result);
}

static inline int inotify_rm_watch(int fd, int wd)
{
    intptr_t result;
    if (fd < 0 || wd <= 0) {
        errno = EINVAL;
        return -1;
    }
    result = _syscall2((uintptr_t)SYS_FILESYSTEM_WATCH_REMOVE,
                       (uintptr_t)(intptr_t)fd,
                       (uintptr_t)(intptr_t)wd);
    return __rin_inotify_result(result);
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_INOTIFY_H */
