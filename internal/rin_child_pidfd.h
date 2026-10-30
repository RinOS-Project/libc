/* SPDX-License-Identifier: MIT */
/* PID再利用後も対象を取り違えない子プロセス管理用の内部関数。 */

#ifndef RIN_INTERNAL_CHILD_PIDFD_H
#define RIN_INTERNAL_CHILD_PIDFD_H

#include "../sys/pidfd.h"
#include "../sys/wait.h"
#include "../unistd.h"

static inline pid_t rin_child_fork_pidfd(int* pidfd_out) {
    if (pidfd_out == NULL) {
        errno = EINVAL;
        return (pid_t)-1;
    }
    return __rin_fork_with_pidfd(pidfd_out);
}

static inline int rin_child_pidfd_wait_status(int pidfd, int* status_out) {
    siginfo_t information = {0};
    int status;
    if (pidfd < 0 || status_out == NULL) {
        errno = EINVAL;
        return -1;
    }
    for (;;) {
        if (waitid(P_PIDFD, (id_t)pidfd, &information, WEXITED) == 0)
            break;
        if (errno != EINTR) return -1;
    }
    if (information.si_pid == 0) {
        errno = EIO;
        return -1;
    }
    if (information.si_code == CLD_EXITED) {
        status = (information.si_status & 0xff) << 8;
    } else if (information.si_code == CLD_DUMPED) {
        status = (information.si_status & 0x7f) | 0x80;
    } else if (information.si_code == CLD_KILLED) {
        status = information.si_status & 0x7f;
    } else {
        errno = EIO;
        return -1;
    }
    *status_out = status;
    return 0;
}

static inline int rin_child_pidfd_terminate_and_wait(int* pidfd_inout) {
    int pidfd;
    int saved_error = 0;
    int status = 0;
    if (pidfd_inout == NULL || *pidfd_inout < 0) return 0;
    pidfd = *pidfd_inout;
    if (pidfd_send_signal(pidfd, SIGKILL, NULL, 0u) != 0 && errno != ESRCH)
        saved_error = errno != 0 ? errno : EIO;
    if (rin_child_pidfd_wait_status(pidfd, &status) != 0 &&
        errno != ECHILD && saved_error == 0)
        saved_error = errno != 0 ? errno : EIO;
    (void)close(pidfd);
    *pidfd_inout = -1;
    if (saved_error != 0) {
        errno = saved_error;
        return -1;
    }
    return 0;
}

#endif /* RIN_INTERNAL_CHILD_PIDFD_H */
