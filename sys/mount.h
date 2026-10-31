/* SPDX-License-Identifier: MIT */
#ifndef RINOS_SYS_MOUNT_H
#define RINOS_SYS_MOUNT_H

#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <rin/contract_abi.h>
#include <rin/fs/fd_mount_abi.h>
#include <linux/mount.h>
#include <sys/syscall.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline long __rin_filesystem_fd_mount_call(
    RinFilesystemFdMountRequestV1* request) {
    RinFilesystemFdMountResponseV1 response = {0};
    long status;
    request->struct_size = sizeof(*request);
    request->version = RIN_FILESYSTEM_FD_MOUNT_ABI_VERSION;
    status = syscall(RIN_SYS_FILESYSTEM_FD_MOUNT_CALL, request, &response);
    if (status < 0) return status;
    if (response.result < 0) {
        if (response.result < -4095) errno = EIO;
        else errno = (int)-response.result;
        return -1;
    }
    return response.result;
}

static inline int open_tree(int dirfd, const char* path, unsigned int flags) {
    RinFilesystemFdMountRequestV1 request = {0};
    request.operation = RIN_FILESYSTEM_FD_MOUNT_OP_OPEN_TREE;
    request.fd = -1;
    request.dirfd = dirfd;
    request.from_dirfd = -1;
    request.to_dirfd = -1;
    request.args[0] = (uint64_t)(uintptr_t)path;
    request.args[1] = flags;
    return (int)__rin_filesystem_fd_mount_call(&request);
}

static inline int move_mount(int from_dirfd, const char* from_path,
                             int to_dirfd, const char* to_path,
                             unsigned int flags) {
    RinFilesystemFdMountRequestV1 request = {0};
    request.operation = RIN_FILESYSTEM_FD_MOUNT_OP_MOVE_MOUNT;
    request.fd = -1;
    request.dirfd = -1;
    request.from_dirfd = from_dirfd;
    request.to_dirfd = to_dirfd;
    request.args[0] = (uint64_t)(uintptr_t)from_path;
    request.args[1] = (uint64_t)(uintptr_t)to_path;
    request.args[2] = flags;
    return (int)__rin_filesystem_fd_mount_call(&request);
}

static inline int mount_setattr(int dirfd, const char* path,
                                unsigned int flags,
                                const struct mount_attr* attributes,
                                size_t size) {
    RinFilesystemFdMountRequestV1 request = {0};
    request.operation = RIN_FILESYSTEM_FD_MOUNT_OP_MOUNT_SETATTR;
    request.fd = -1;
    request.dirfd = dirfd;
    request.from_dirfd = -1;
    request.to_dirfd = -1;
    request.args[0] = (uint64_t)(uintptr_t)path;
    request.args[1] = flags;
    request.args[2] = (uint64_t)(uintptr_t)attributes;
    request.args[3] = size;
    return (int)__rin_filesystem_fd_mount_call(&request);
}

static inline int fsopen(const char* filesystem_name, unsigned int flags) {
    RinFilesystemFdMountRequestV1 request = {0};
    request.operation = RIN_FILESYSTEM_FD_MOUNT_OP_FSOPEN;
    request.fd = -1;
    request.dirfd = -1;
    request.from_dirfd = -1;
    request.to_dirfd = -1;
    request.args[0] = (uint64_t)(uintptr_t)filesystem_name;
    request.args[1] = flags;
    return (int)__rin_filesystem_fd_mount_call(&request);
}

static inline int fsconfig(int fsfd, unsigned int command, const char* key,
                           const void* value, int auxiliary_fd) {
    RinFilesystemFdMountRequestV1 request = {0};
    request.operation = RIN_FILESYSTEM_FD_MOUNT_OP_FSCONFIG;
    request.fd = fsfd;
    request.dirfd = -1;
    request.from_dirfd = -1;
    request.to_dirfd = -1;
    request.args[0] = command;
    request.args[1] = (uint64_t)(uintptr_t)key;
    request.args[2] = (uint64_t)(uintptr_t)value;
    request.args[3] = (uint64_t)(int64_t)auxiliary_fd;
    return (int)__rin_filesystem_fd_mount_call(&request);
}

static inline int fsmount(int fsfd, unsigned int flags,
                          unsigned int mount_flags) {
    RinFilesystemFdMountRequestV1 request = {0};
    request.operation = RIN_FILESYSTEM_FD_MOUNT_OP_FSMOUNT;
    request.fd = fsfd;
    request.dirfd = -1;
    request.from_dirfd = -1;
    request.to_dirfd = -1;
    request.args[0] = flags;
    request.args[1] = mount_flags;
    return (int)__rin_filesystem_fd_mount_call(&request);
}

static inline int fspick(int dirfd, const char* path, unsigned int flags) {
    RinFilesystemFdMountRequestV1 request = {0};
    request.operation = RIN_FILESYSTEM_FD_MOUNT_OP_FSPICK;
    request.fd = -1;
    request.dirfd = dirfd;
    request.from_dirfd = -1;
    request.to_dirfd = -1;
    request.args[0] = (uint64_t)(uintptr_t)path;
    request.args[1] = flags;
    return (int)__rin_filesystem_fd_mount_call(&request);
}

static inline int rin_mount(const char* device, const char* mount_point,
                            uint32_t filesystem_type, const char* options) {
    return (int)syscall(RIN_SYS_FILESYSTEM_MOUNT, device, mount_point,
                        filesystem_type, options);
}

static inline int rin_unmount(const char* mount_point) {
    return (int)syscall(RIN_SYS_FILESYSTEM_UNMOUNT, mount_point);
}

#ifdef __cplusplus
}
#endif

#endif /* RINOS_SYS_MOUNT_H */
