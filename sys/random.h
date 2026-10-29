/*
 * RinOS libc - sys/random.h
 * getrandom/getentropy compatibility helpers
 */

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

static inline ssize_t getrandom(void* buf, size_t buflen, unsigned int flags)
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
        ssize_t received = getrandom(bytes + offset, length - offset, 0);
        if (received <= 0) {
            if (received == 0) errno = EIO;
            return -1;
        }
        offset += (size_t)received;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_RANDOM_H */
