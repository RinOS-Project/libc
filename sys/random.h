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

static inline ssize_t getrandom(void* buf, size_t buflen, unsigned int flags)
{
    ssize_t result;
    __rin_pthread_cancel_point_if_linked();
    result = __rin_getrandom_raw(buf, buflen, flags);
    __rin_pthread_cancel_point_if_linked();
    return result;
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_RANDOM_H */

#endif /* hosted random provider selection */

#ifndef GRND_INSECURE
#    define GRND_INSECURE 0x0004
#endif
