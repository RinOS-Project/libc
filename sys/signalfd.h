/* SPDX-License-Identifier: MIT */
/* RinOS POSIX signalfd interface. */
#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
#include_next <sys/signalfd.h>
#else
#ifndef _SYS_SIGNALFD_H
#define _SYS_SIGNALFD_H

#include "../stddef.h"
#include "../fcntl.h"
#include "../signal.h"
#include "../stdint.h"

#define SYS_signalfd SYS_SIGNALFD
#define SYS_signalfd4 SYS_SIGNALFD4
#define __NR_signalfd SYS_SIGNALFD
#define __NR_signalfd4 SYS_SIGNALFD4

#define SFD_NONBLOCK O_NONBLOCK
#define SFD_CLOEXEC O_CLOEXEC
#if defined(__RIN_FEATURES_GNU) || defined(__RIN_FEATURES_DEFAULT)
#define SFD_CLOFORK O_CLOFORK
#endif

/* Fixed 128-byte read record. Layout matches Linux struct
 * signalfd_siginfo and the kernel RinSignalfdSiginfo. */
struct signalfd_siginfo {
    uint32_t ssi_signo;    /* +0 */
    int32_t ssi_errno;     /* +4 */
    int32_t ssi_code;      /* +8 */
    uint32_t ssi_pid;      /* +12 */
    uint32_t ssi_uid;      /* +16 */
    int32_t ssi_fd;        /* +20 */
    uint32_t ssi_tid;      /* +24 */
    uint32_t ssi_band;     /* +28 */
    uint32_t ssi_overrun;  /* +32 */
    uint32_t ssi_trapno;   /* +36 */
    int32_t ssi_status;    /* +40 */
    int32_t ssi_int;       /* +44 */
    uint64_t ssi_ptr;      /* +48 */
    uint64_t ssi_utime;    /* +56 */
    uint64_t ssi_stime;    /* +64 */
    uint64_t ssi_addr;     /* +72 */
    uint8_t __pad[48];     /* +80 = 128 */
};

#if defined(__cplusplus)
static_assert(sizeof(struct signalfd_siginfo) == 128u,
              "signalfd_siginfo ABI size drift");
static_assert(offsetof(struct signalfd_siginfo, ssi_ptr) == 48u &&
                  offsetof(struct signalfd_siginfo, ssi_addr) == 72u &&
                  offsetof(struct signalfd_siginfo, __pad) == 80u,
              "signalfd_siginfo ABI offset drift");
#else
_Static_assert(sizeof(struct signalfd_siginfo) == 128u,
               "signalfd_siginfo ABI size drift");
_Static_assert(offsetof(struct signalfd_siginfo, ssi_ptr) == 48u &&
                   offsetof(struct signalfd_siginfo, ssi_addr) == 72u &&
                   offsetof(struct signalfd_siginfo, __pad) == 80u,
               "signalfd_siginfo ABI offset drift");
#endif

#ifdef __cplusplus
extern "C" {
#endif

int signalfd(int fd, const sigset_t* mask, int flags);
int signalfd4(int fd, const sigset_t* mask, int sizemask, int flags);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SIGNALFD_H */
#endif /* hosted Linux delegates to native provider */
