/* SPDX-License-Identifier: MIT */
/* RinOS POSIX timerfd interface. */
#ifndef _SYS_TIMERFD_H
#define _SYS_TIMERFD_H

#include "../fcntl.h"
#include "../time.h"

#define SYS_timerfd_create SYS_TIMERFD_CREATE
#define SYS_timerfd_settime SYS_TIMERFD_SETTIME
#define SYS_timerfd_gettime SYS_TIMERFD_GETTIME
#define __NR_timerfd_create SYS_TIMERFD_CREATE
#define __NR_timerfd_settime SYS_TIMERFD_SETTIME
#define __NR_timerfd_gettime SYS_TIMERFD_GETTIME

#define TFD_TIMER_ABSTIME     0x0001
#define TFD_TIMER_CANCEL_ON_SET 0x0002
#define TFD_NONBLOCK O_NONBLOCK
#define TFD_CLOEXEC  O_CLOEXEC

#ifdef __cplusplus
extern "C" {
#endif

int timerfd_create(clockid_t clock_id, int flags);
int timerfd_settime(int fd, int flags, const struct itimerspec* value,
                    struct itimerspec* old_value);
int timerfd_gettime(int fd, struct itimerspec* value);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_TIMERFD_H */
