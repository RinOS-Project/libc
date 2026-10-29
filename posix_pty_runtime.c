/* SPDX-License-Identifier: MIT */

#include "stdlib.h"

#include "errno.h"
#include "fcntl.h"
#include "sys/ioctl.h"
#include "sys/stat.h"
#include "unistd.h"

#include <stdint.h>

#define RIN_PTY_MASTER_RDEV UINT32_C(0x0502)
#define RIN_PTY_SLAVE_RDEV_BASE UINT32_C(0x8800)
#define RIN_PTY_PATH_CAPACITY 32u

static int rin_pty_error(void)
{
    return errno != 0 ? errno : EIO;
}

static int rin_pty_format_slave_path(int32_t number, char* output,
                                     size_t capacity)
{
    static const char prefix[] = "/dev/pts/";
    char digits[10];
    size_t digit_count = 0u;
    size_t index;
    uint32_t value;

    if (number < 0 || output == NULL) return EINVAL;
    value = (uint32_t)number;
    do {
        digits[digit_count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u && digit_count < sizeof(digits));
    if (value != 0u) return EOVERFLOW;
    if (sizeof(prefix) + digit_count > capacity) return ERANGE;

    for (index = 0u; index < sizeof(prefix) - 1u; ++index)
        output[index] = prefix[index];
    for (index = 0u; index < digit_count; ++index)
        output[sizeof(prefix) - 1u + index] =
            digits[digit_count - index - 1u];
    output[sizeof(prefix) - 1u + digit_count] = '\0';
    return 0;
}

/* The PTY owner creates the slave with its owner's uid/gid and mode 0600.
 * Resolve it in the caller's namespace and compare the inode generation so
 * a stale number or a different namespace cannot name another PTY. */
static int rin_pty_master_snapshot(int fd, char* path_out,
                                   size_t path_capacity)
{
    struct stat master_status;
    struct stat slave_status;
    int32_t number = -1;
    int error;
    int ioctl_result;

    if (path_out == NULL) return EINVAL;
    ioctl_result = ioctl(fd, TIOCGPTN, &number);
    if (ioctl_result < 0) return rin_pty_error();
    if (ioctl_result != 0) return EIO;
    if (number < 0) return EIO;
    if (fstat(fd, &master_status) != 0) return rin_pty_error();
    if (!S_ISCHR(master_status.st_mode) ||
        master_status.st_rdev != (dev_t)RIN_PTY_MASTER_RDEV)
        return ENOTTY;

    error = rin_pty_format_slave_path(number, path_out, path_capacity);
    if (error != 0) return error;
    if (stat(path_out, &slave_status) != 0) return rin_pty_error();
    if (!S_ISCHR(slave_status.st_mode) ||
        slave_status.st_rdev !=
            (dev_t)(RIN_PTY_SLAVE_RDEV_BASE + (uint32_t)number) ||
        slave_status.st_ino == 0u ||
        slave_status.st_ino != master_status.st_ino ||
        slave_status.st_uid != master_status.st_uid ||
        slave_status.st_gid != master_status.st_gid ||
        (slave_status.st_mode & 07777u) != 0600u)
        return EIO;

    return 0;
}

int posix_openpt(int oflag)
{
    const int allowed_flags = O_ACCMODE | O_NOCTTY | O_CLOEXEC | O_NONBLOCK;
    if ((oflag & O_ACCMODE) != O_RDWR ||
        (oflag & ~allowed_flags) != 0) {
        errno = EINVAL;
        return -1;
    }
    return open("/dev/ptmx", oflag);
}

int grantpt(int fd)
{
    char path[RIN_PTY_PATH_CAPACITY];
    int error = rin_pty_master_snapshot(fd, path, sizeof(path));
    if (error != 0) {
        errno = error;
        return -1;
    }
    return 0;
}

int unlockpt(int fd)
{
    int unlocked = 0;
    int result = ioctl(fd, TIOCSPTLCK, &unlocked);
    if (result < 0) return -1;
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

int ptsname_r(int fd, char* buffer, size_t buffer_size)
{
    char path[RIN_PTY_PATH_CAPACITY];
    size_t length = 0u;
    int saved_errno = errno;
    int error;

    if (buffer != NULL && buffer_size != 0u) buffer[0] = '\0';
    if (buffer == NULL || buffer_size == 0u) {
        error = EINVAL;
        goto done;
    }
    error = rin_pty_master_snapshot(fd, path, sizeof(path));
    if (error != 0) goto done;
    while (path[length] != '\0') ++length;
    if (length >= buffer_size) {
        error = ERANGE;
        goto done;
    }
    for (length = 0u; path[length] != '\0'; ++length)
        buffer[length] = path[length];
    buffer[length] = '\0';
    error = 0;

done:
    errno = saved_errno;
    return error;
}

char* ptsname(int fd)
{
    static char path[RIN_PTY_PATH_CAPACITY];
    int saved_errno = errno;
    int error = ptsname_r(fd, path, sizeof(path));
    if (error != 0) {
        errno = error;
        return NULL;
    }
    errno = saved_errno;
    return path;
}
