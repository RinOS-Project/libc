/* SPDX-License-Identifier: MIT */
#define RIN_FCNTL_ADVISE_RUNTIME_IMPLEMENTATION 1
#define RIN_MMAN_POSIX_ADVISE_RUNTIME_IMPLEMENTATION 1
#include "fcntl.h"
#include "sys/mman.h"
#include "sys/syscall.h"
#include "sys/xattr.h"
#include <string.h>
#include "unistd.h"
#include <rin/ipc.h>

int _fcntl_fd_flags[_FCNTL_MAX_FDS];
int _fcntl_fl_flags[_FCNTL_MAX_FDS];
int _fcntl_initialized = 0;

int posix_fadvise(int fd, off_t offset, off_t len, int advice) {
    return _rin_posix_fadvise_result(fd, offset, len, advice);
}

int posix_fallocate(int fd, off_t offset, off_t len) {
    return _rin_fallocate_result(fd, 0, offset, len);
}

int posix_madvise(void* addr, size_t length, int advice) {
    int saved_errno = errno;
    int error;
    /* POSIX advice may influence performance but must not change memory
     * contents. Linux MADV_DONTNEED is destructive, so treat the POSIX
     * DONTNEED hint as NORMAL while retaining kernel range validation. */
    if (advice == POSIX_MADV_DONTNEED)
        advice = MADV_NORMAL;
    if (madvise(addr, length, advice) == 0) {
        errno = saved_errno;
        return 0;
    }
    error = errno;
    errno = saved_errno;
    return error;
}

static int rin_shm_name_snapshot(const char* name, char* destination,
                                 size_t capacity) {
    RinSdkArgsV1 request = {0};
    intptr_t result;
    if (!name || !destination || capacity == 0u || capacity > UINT32_MAX) {
        errno = name && destination ? EINVAL : EFAULT;
        return -1;
    }
    request.struct_size = sizeof(request);
    request.version = RIN_SDK_STRUCT_VERSION_1;
    request.value[0] = (uint64_t)(uintptr_t)name;
    request.value[1] = (uint64_t)capacity;
    request.value[2] = 1u;
    result = __RIN_SYSCALL_7(
        SYS_SDK_CALL, (uintptr_t)&request, (uintptr_t)sizeof(request),
        RIN_SDK_LIBRARY_IPC, 28u, (uintptr_t)destination,
        (uintptr_t)capacity);
    if (result == RIN_SUCCESS) return 0;
    if (result == RIN_ERROR_INVALID_ADDRESS) errno = EFAULT;
    else if (result == RIN_ERROR_LIMIT_EXCEEDED) errno = ENAMETOOLONG;
    else if (result == RIN_ERROR_INVALID_ARGUMENT) errno = EINVAL;
    else if (result == RIN_ERROR_NOT_SUPPORTED) errno = ENOSYS;
    else errno = EIO;
    return -1;
}

static int rin_shm_name_path(const char* name,
                             char path[sizeof("/dev/shm/") + NAME_MAX]) {
    static const char prefix[] = "/dev/shm/";
    char name_copy[NAME_MAX + 2u];
    size_t length = 0u;
    if (!name) {
        errno = EFAULT;
        return -1;
    }
    if (rin_shm_name_snapshot(name, name_copy, sizeof(name_copy)) != 0)
        return -1;
    while (length < sizeof(name_copy)) {
        if (name_copy[length] == '\0') break;
        ++length;
    }
    if (name_copy[0] != '/' || name_copy[1] == '\0') {
        errno = EINVAL;
        return -1;
    }
    if (length > NAME_MAX + 1u) {
        errno = ENAMETOOLONG;
        return -1;
    }
    if ((length == 2u && name_copy[1] == '.') ||
        (length == 3u && name_copy[1] == '.' && name_copy[2] == '.')) {
        errno = EINVAL;
        return -1;
    }
    for (size_t index = 1u; index < length; ++index) {
        if (name_copy[index] == '/') {
            errno = EINVAL;
            return -1;
        }
    }
    size_t prefix_length = sizeof(prefix) - 1u;
    for (size_t index = 0u; index < prefix_length; ++index)
        path[index] = prefix[index];
    for (size_t index = 1u; index < length; ++index)
        path[prefix_length + index - 1u] = name_copy[index];
    path[prefix_length + length - 1u] = '\0';
    return 0;
}

int shm_open(const char* name, int oflag, mode_t mode) {
    char path[sizeof("/dev/shm/") + NAME_MAX];
    int access_mode = oflag & O_ACCMODE;
    const int allowed_flags = O_ACCMODE | O_CREAT | O_EXCL | O_CLOEXEC;
    if ((oflag & ~allowed_flags) != 0 ||
        (access_mode != O_RDONLY && access_mode != O_RDWR) ||
        ((oflag & O_EXCL) != 0 && (oflag & O_CREAT) == 0)) {
        errno = EINVAL;
        return -1;
    }
    if (rin_shm_name_path(name, path) != 0) return -1;
    return openat(AT_FDCWD, path, oflag | O_CLOEXEC, mode);
}

int shm_unlink(const char* name) {
    char path[sizeof("/dev/shm/") + NAME_MAX];
    if (rin_shm_name_path(name, path) != 0) return -1;
    return unlinkat(AT_FDCWD, path, 0);
}

int memfd_create(const char* name, unsigned int flags) {
    static const char debug_label_xattr[] = "user.rinos.memfd.name";
    static const char debug_label_prefix[] = "memfd:";
    char name_snapshot[250];
    char debug_label[sizeof(debug_label_prefix) + sizeof(name_snapshot)];
    size_t name_length = 0u;
    int fd;

    if (name == 0) {
        errno = EFAULT;
        return -1;
    }
    if ((flags & ~MFD_CLOEXEC) != 0u) {
        errno = EINVAL;
        return -1;
    }
    /* Snapshot through the checked-copy IPC operation.  memfd_create must
     * not dereference the caller's name in libc: an invalid pointer is an
     * EFAULT result, and concurrent unmap cannot change the label halfway
     * through file creation. */
    if (rin_shm_name_snapshot(name, name_snapshot,
                              sizeof(name_snapshot)) != 0) {
        if (errno == ENAMETOOLONG) errno = EINVAL;
        return -1;
    }
    while (name_length < sizeof(name_snapshot) &&
           name_snapshot[name_length] != '\0')
        ++name_length;
    if (name_length == sizeof(name_snapshot) || name_length > 249u) {
        errno = EINVAL;
        return -1;
    }

    /* O_EXCL makes the anonymous inode permanently un-linkable, matching
     * memfd lifetime semantics even though the current backing is tmpfs. */
    fd = openat(AT_FDCWD, "/dev/shm", O_TMPFILE | O_EXCL | O_RDWR |
                    ((flags & MFD_CLOEXEC) != 0u ? O_CLOEXEC : 0),
                0600);
    if (fd < 0) return -1;

    memcpy(debug_label, debug_label_prefix, sizeof(debug_label_prefix) - 1u);
    memcpy(debug_label + sizeof(debug_label_prefix) - 1u,
           name_snapshot, name_length);
    debug_label[sizeof(debug_label_prefix) - 1u + name_length] = '\0';
    if (fsetxattr(fd, debug_label_xattr, debug_label,
                  sizeof(debug_label_prefix) - 1u + name_length + 1u,
                  XATTR_CREATE) != 0) {
        int saved_errno = errno;
        (void)close(fd);
        errno = saved_errno;
        return -1;
    }
    return fd;
}
