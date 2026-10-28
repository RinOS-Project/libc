/* SPDX-License-Identifier: MIT */
/* Shared time_t width selection for <time.h> and <sys/types.h>. */
#ifndef RIN_LIBC_INTERNAL_TIME_TYPE_H
#define RIN_LIBC_INTERNAL_TIME_TYPE_H

#include "../stdint.h"
#include "../limits.h"

/* New IA-32 RinOS userspace uses the time64 source ABI. Existing binaries
 * keep the layout they were compiled with and continue using legacy syscalls. */
#if (defined(__i386__) || defined(__x86_64__) || defined(_M_X64)) && \
    !defined(RIN_TIME_BITS)
#define RIN_TIME_BITS 64
#endif

#if (defined(RIN_TIME_BITS) && RIN_TIME_BITS == 64) || \
    (!defined(RIN_TIME_BITS) && \
     (defined(__x86_64__) || defined(_M_X64)))
#define RIN_TIME_ABI64 1
#endif

#if defined(RIN_TIME_BITS) && RIN_TIME_BITS == 64
typedef int64_t rin_time_t;
#define RIN_TIME_T_MAX INT64_MAX
#define RIN_TIME_T_MIN INT64_MIN
#elif defined(RIN_TIME_BITS) && RIN_TIME_BITS == 32
typedef int32_t rin_time_t;
#define RIN_TIME_T_MAX INT32_MAX
#define RIN_TIME_T_MIN INT32_MIN
#elif !defined(RIN_TIME_BITS)
typedef long rin_time_t;
#define RIN_TIME_T_MAX LONG_MAX
#define RIN_TIME_T_MIN LONG_MIN
#else
#error "RIN_TIME_BITS must be 32 or 64"
#endif

#endif /* RIN_LIBC_INTERNAL_TIME_TYPE_H */
