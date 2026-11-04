/*
 * RinOS libc - sys/random.h
 * getrandom/getentropy compatibility helpers
 */

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    !defined(_RIN_RANDOM_SYSCALL3) && \
    (defined(__unix__) || defined(__APPLE__)) && \
    (defined(__GNUC__) || defined(__clang__))
#if defined(__linux__) && defined(__has_include_next) && \
    __has_include_next(<sys/random.h>)
#include_next <sys/random.h>
#else
#define RIN_SYS_RANDOM_HOSTED_UNAVAILABLE 1
#endif
#else

#ifndef _SYS_RANDOM_H
#define _SYS_RANDOM_H

#include "../errno.h"
#include "../stddef.h"
#include "../limits.h"
#include "../unistd.h"
#include "syscall.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GRND_NONBLOCK
#    define GRND_NONBLOCK 0x0001
#endif

#ifndef GRND_RANDOM
#    define GRND_RANDOM 0x0002
#endif

#ifndef _RIN_RANDOM_SYSCALL3
#define _RIN_RANDOM_SYSCALL3(number, buffer, length, flags) \
    _syscall3((uintptr_t)(number), (uintptr_t)(buffer), \
              (uintptr_t)(length), (uintptr_t)(flags))
#endif

/* Keep the raw operation separate: getentropy() is not a cancellation point,
 * while getrandom() is. */
static inline ssize_t __rin_getrandom_raw(
    void* buf, size_t buflen, unsigned int flags)
{
    intptr_t ret = _RIN_RANDOM_SYSCALL3(__NR_getrandom, buf, buflen, flags);
    if (ret < 0) {
        errno = ret >= -(intptr_t)4095 ? (int)-ret : EIO;
        return -1;
    }
    if ((uintptr_t)ret > (uintptr_t)buflen) {
        errno = EIO;
        return -1;
    }
    if (ret > (intptr_t)SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (ssize_t)ret;
}

static inline ssize_t getrandom(void* buf, size_t buflen, unsigned int flags)
{
    ssize_t result;
    __rin_pthread_cancel_point_if_linked();
    result = __rin_getrandom_raw(buf, buflen, flags);
    __rin_pthread_cancel_point_if_linked();
    return result;
}

#if defined(_RIN_USE_BSD_GETENTROPY) || defined(_DEFAULT_SOURCE) || \
    defined(_BSD_SOURCE) || defined(__USE_MISC) || \
    (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 202405L) || \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 800) || \
    defined(__USE_XOPEN2K24) || defined(__USE_XOPEN2K24XSI) || \
    (!defined(__STRICT_ANSI__) && !defined(_POSIX_C_SOURCE) && \
     !defined(_XOPEN_SOURCE))
static inline int getentropy(void* buffer, size_t length)
{
    unsigned char* bytes = (unsigned char*)buffer;
    size_t offset = 0;

    if (length > 256) {
        errno = EIO;
        return -1;
    }
    if (!buffer && length != 0) {
        errno = EFAULT;
        return -1;
    }
    while (offset < length) {
        ssize_t received = __rin_getrandom_raw(
            bytes + offset, length - offset, 0);
        if (received <= 0) {
            if (received < 0 && errno == EINTR) continue;
            if (received == 0) errno = EIO;
            return -1;
        }
        offset += (size_t)received;
    }
    return 0;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SYS_RANDOM_H */

#endif /* hosted random provider selection */

#ifndef GRND_INSECURE
#    define GRND_INSECURE 0x0004
#endif
