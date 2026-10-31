/* SPDX-License-Identifier: MIT */
/* RinOSのeventfd公開API。 */
#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
#include_next <sys/eventfd.h>
#else
#ifndef _SYS_EVENTFD_H
#define _SYS_EVENTFD_H

#include "../fcntl.h"

#define SYS_eventfd2 SYS_EVENTFD2
#define __NR_eventfd2 SYS_EVENTFD2
#define EFD_SEMAPHORE 0x0001
#define EFD_NONBLOCK  O_NONBLOCK
#define EFD_CLOEXEC   O_CLOEXEC

#ifdef __cplusplus
extern "C" {
#endif

int eventfd(unsigned int initial_value, int flags);
int eventfd2(unsigned int initial_value, int flags);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_EVENTFD_H */
#endif /* hosted Linux delegates to native provider */
