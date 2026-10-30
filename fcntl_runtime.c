/* SPDX-License-Identifier: MIT */
#include "fcntl.h"
#include "sys/mman.h"
#include "unistd.h"

int _fcntl_fd_flags[_FCNTL_MAX_FDS];
int _fcntl_fl_flags[_FCNTL_MAX_FDS];
int _fcntl_initialized = 0;

static int rin_shm_name_path(const char* name,
                             char path[sizeof("/dev/shm/") + NAME_MAX]) {
    static const char prefix[] = "/dev/shm/";
    char name_copy[NAME_MAX + 2u];
    size_t length = 0u;
    if (!name || name[0] != '/' || name[1] == '\0') {
        errno = EINVAL;
        return -1;
    }
    /* Single bounded pass over caller memory: every check and the
     * path build below observe the stable copy. */
    while (length <= NAME_MAX + 1u) {
        name_copy[length] = name[length];
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
    if (name == 0) {
        errno = EFAULT;
        return -1;
    }
    if ((flags & ~MFD_CLOEXEC) != 0u) {
        errno = EINVAL;
        return -1;
    }
    (void)name;
    return openat(AT_FDCWD, "/dev/shm", O_TMPFILE | O_RDWR |
                      ((flags & MFD_CLOEXEC) != 0u ? O_CLOEXEC : 0),
                  0600);
}
