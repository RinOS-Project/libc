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
    size_t length = 0u;
    if (!name || name[0] != '/' || name[1] == '\0') {
        errno = EINVAL;
        return -1;
    }
    while (length <= NAME_MAX + 1u && name[length] != '\0') ++length;
    if (length > NAME_MAX + 1u) {
        errno = ENAMETOOLONG;
        return -1;
    }
    if ((length == 2u && name[1] == '.') ||
        (length == 3u && name[1] == '.' && name[2] == '.')) {
        errno = EINVAL;
        return -1;
    }
    for (size_t index = 1u; index < length; ++index) {
        if (name[index] == '/') {
            errno = EINVAL;
            return -1;
        }
    }
    size_t prefix_length = sizeof(prefix) - 1u;
    for (size_t index = 0u; index < prefix_length; ++index)
        path[index] = prefix[index];
    for (size_t index = 1u; index < length; ++index)
        path[prefix_length + index - 1u] = name[index];
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
    return openat(AT_FDCWD, path, oflag, mode);
}

int shm_unlink(const char* name) {
    char path[sizeof("/dev/shm/") + NAME_MAX];
    if (rin_shm_name_path(name, path) != 0) return -1;
    return unlinkat(AT_FDCWD, path, 0);
}

static unsigned int rin_memfd_sequence;

static void rin_memfd_format_name(
    unsigned int process_id, unsigned int sequence,
    char name[sizeof("/.memfd-") + 17u]) {
    static const char prefix[] = "/.memfd-";
    static const char hex[] = "0123456789abcdef";
    size_t prefix_length = sizeof(prefix) - 1u;
    for (size_t index = 0u; index < prefix_length; ++index)
        name[index] = prefix[index];
    for (size_t index = 0u; index < 8u; ++index) {
        unsigned int shift = (unsigned int)((7u - index) * 4u);
        name[prefix_length + index] = hex[(process_id >> shift) & 0x0fu];
        name[prefix_length + 9u + index] = hex[(sequence >> shift) & 0x0fu];
    }
    name[prefix_length + 8u] = '-';
    name[prefix_length + 17u] = '\0';
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
    for (unsigned int attempt = 0u; attempt < 32u; ++attempt) {
        char shm_name[sizeof("/.memfd-") + 17u];
        unsigned int process_id = (unsigned int)getpid();
        unsigned int sequence = __atomic_add_fetch(
            &rin_memfd_sequence, 1u, __ATOMIC_RELAXED);
        int open_flags = O_CREAT | O_EXCL | O_RDWR;
        int fd;
        rin_memfd_format_name(process_id, sequence, shm_name);
        if ((flags & MFD_CLOEXEC) != 0u) open_flags |= O_CLOEXEC;
        fd = shm_open(shm_name, open_flags, 0600);
        if (fd < 0) {
            if (errno == EEXIST) continue;
            return -1;
        }
        if (shm_unlink(shm_name) == 0) return fd;
        {
            int saved_errno = errno;
            (void)close(fd);
            errno = saved_errno;
        }
        return -1;
    }
    errno = EAGAIN;
    return -1;
}
