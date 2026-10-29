/* SPDX-License-Identifier: MIT */
/* RinOS libc public extended-attribute API. */

#ifndef _SYS_XATTR_H
#define _SYS_XATTR_H

#include "../fcntl.h"

#ifndef XATTR_CREATE
#define XATTR_CREATE RIN_PATH_AT_XATTR_CREATE
#endif
#ifndef XATTR_REPLACE
#define XATTR_REPLACE RIN_PATH_AT_XATTR_REPLACE
#endif
#ifndef XATTR_NAME_MAX
#define XATTR_NAME_MAX 255u
#endif
/* RinOS currently stores at most this many bytes in one attribute value. */
#ifndef XATTR_SIZE_MAX
#define XATTR_SIZE_MAX RIN_PATH_AT_XATTR_VALUE_MAX
#endif
#ifndef XATTR_LIST_MAX
#define XATTR_LIST_MAX RIN_PATH_AT_XATTR_LIST_MAX
#endif

#ifndef MIDL_PASS
#ifdef __cplusplus
extern "C" {
#endif

static inline ssize_t _rin_xattr_path_get(const char* path, const char* name,
                                          void* value, size_t size,
                                          int nofollow) {
    RinPathAtCallV1 call;
    _fcntl_path_at_init(&call, RIN_PATH_AT_GETXATTR);
    call.flags = nofollow ? RIN_PATH_AT_SYMLINK_NOFOLLOW : 0u;
    call.path1 = (uint64_t)(uintptr_t)path;
    call.path2 = (uint64_t)(uintptr_t)name;
    call.buffer = (uint64_t)(uintptr_t)value;
    call.buffer_size = (uint64_t)size;
    return (ssize_t)_fcntl_path_at_value(&call);
}

static inline ssize_t _rin_xattr_path_list(const char* path, char* list,
                                           size_t size, int nofollow) {
    RinPathAtCallV1 call;
    _fcntl_path_at_init(&call, RIN_PATH_AT_LISTXATTR);
    call.flags = nofollow ? RIN_PATH_AT_SYMLINK_NOFOLLOW : 0u;
    call.path1 = (uint64_t)(uintptr_t)path;
    call.buffer = (uint64_t)(uintptr_t)list;
    call.buffer_size = (uint64_t)size;
    return (ssize_t)_fcntl_path_at_value(&call);
}

static inline int _rin_xattr_path_set(const char* path, const char* name,
                                      const void* value, size_t size,
                                      int flags, int nofollow) {
    RinPathAtCallV1 call;
    _fcntl_path_at_init(&call, RIN_PATH_AT_SETXATTR);
    call.flags = nofollow ? RIN_PATH_AT_SYMLINK_NOFOLLOW : 0u;
    call.mode = (uint32_t)flags;
    call.path1 = (uint64_t)(uintptr_t)path;
    call.path2 = (uint64_t)(uintptr_t)name;
    call.buffer = (uint64_t)(uintptr_t)value;
    call.buffer_size = (uint64_t)size;
    return _fcntl_path_at_zero(&call);
}

static inline int _rin_xattr_path_remove(const char* path, const char* name,
                                         int nofollow) {
    RinPathAtCallV1 call;
    _fcntl_path_at_init(&call, RIN_PATH_AT_REMOVEXATTR);
    call.flags = nofollow ? RIN_PATH_AT_SYMLINK_NOFOLLOW : 0u;
    call.path1 = (uint64_t)(uintptr_t)path;
    call.path2 = (uint64_t)(uintptr_t)name;
    return _fcntl_path_at_zero(&call);
}

static inline ssize_t getxattr(const char* path, const char* name,
                               void* value, size_t size) {
    return _rin_xattr_path_get(path, name, value, size, 0);
}
static inline ssize_t lgetxattr(const char* path, const char* name,
                                void* value, size_t size) {
    return _rin_xattr_path_get(path, name, value, size, 1);
}
static inline ssize_t fgetxattr(int fd, const char* name,
                                void* value, size_t size) {
    RinPathAtCallV1 call;
    _fcntl_path_at_init(&call, RIN_PATH_AT_FGETXATTR);
    call.flags = RIN_PATH_AT_EMPTY_PATH;
    call.dirfd = fd;
    call.path1 = (uint64_t)(uintptr_t)"";
    call.path2 = (uint64_t)(uintptr_t)name;
    call.buffer = (uint64_t)(uintptr_t)value;
    call.buffer_size = (uint64_t)size;
    return (ssize_t)_fcntl_path_at_value(&call);
}

static inline ssize_t listxattr(const char* path, char* list, size_t size) {
    return _rin_xattr_path_list(path, list, size, 0);
}
static inline ssize_t llistxattr(const char* path, char* list, size_t size) {
    return _rin_xattr_path_list(path, list, size, 1);
}
static inline ssize_t flistxattr(int fd, char* list, size_t size) {
    RinPathAtCallV1 call;
    _fcntl_path_at_init(&call, RIN_PATH_AT_FLISTXATTR);
    call.flags = RIN_PATH_AT_EMPTY_PATH;
    call.dirfd = fd;
    call.path1 = (uint64_t)(uintptr_t)"";
    call.path2 = (uint64_t)(uintptr_t)"";
    call.buffer = (uint64_t)(uintptr_t)list;
    call.buffer_size = (uint64_t)size;
    return (ssize_t)_fcntl_path_at_value(&call);
}

static inline int setxattr(const char* path, const char* name,
                           const void* value, size_t size, int flags) {
    return _rin_xattr_path_set(path, name, value, size, flags, 0);
}
static inline int lsetxattr(const char* path, const char* name,
                            const void* value, size_t size, int flags) {
    return _rin_xattr_path_set(path, name, value, size, flags, 1);
}
static inline int fsetxattr(int fd, const char* name, const void* value,
                            size_t size, int flags) {
    RinPathAtCallV1 call;
    _fcntl_path_at_init(&call, RIN_PATH_AT_FSETXATTR);
    call.flags = RIN_PATH_AT_EMPTY_PATH | (uint32_t)flags;
    call.dirfd = fd;
    call.path1 = (uint64_t)(uintptr_t)"";
    call.path2 = (uint64_t)(uintptr_t)name;
    call.buffer = (uint64_t)(uintptr_t)value;
    call.buffer_size = (uint64_t)size;
    return _fcntl_path_at_zero(&call);
}

static inline int removexattr(const char* path, const char* name) {
    return _rin_xattr_path_remove(path, name, 0);
}
static inline int lremovexattr(const char* path, const char* name) {
    return _rin_xattr_path_remove(path, name, 1);
}
static inline int fremovexattr(int fd, const char* name) {
    RinPathAtCallV1 call;
    _fcntl_path_at_init(&call, RIN_PATH_AT_FREMOVEXATTR);
    call.flags = RIN_PATH_AT_EMPTY_PATH;
    call.dirfd = fd;
    call.path1 = (uint64_t)(uintptr_t)"";
    call.path2 = (uint64_t)(uintptr_t)name;
    return _fcntl_path_at_zero(&call);
}

#ifdef __cplusplus
}
#endif
#endif /* MIDL_PASS */
#endif /* _SYS_XATTR_H */
