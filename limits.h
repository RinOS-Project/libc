/* SPDX-License-Identifier: MIT */
#ifndef _LIMITS_H
#define _LIMITS_H

/* Char limits */
#ifndef CHAR_BIT
#define CHAR_BIT    8
#endif
#ifndef SCHAR_MIN
#define SCHAR_MIN   (-128)
#endif
#ifndef SCHAR_MAX
#define SCHAR_MAX   127
#endif
#ifndef UCHAR_MAX
#define UCHAR_MAX   255
#endif
#ifndef CHAR_MIN
#ifdef __CHAR_UNSIGNED__
#define CHAR_MIN    0
#else
#define CHAR_MIN    SCHAR_MIN
#endif
#endif
#ifndef CHAR_MAX
#ifdef __CHAR_UNSIGNED__
#define CHAR_MAX    UCHAR_MAX
#else
#define CHAR_MAX    SCHAR_MAX
#endif
#endif

/* Short limits */
#ifndef SHRT_MIN
#define SHRT_MIN    (-32768)
#endif
#ifndef SHRT_MAX
#define SHRT_MAX    32767
#endif
#ifndef USHRT_MAX
#define USHRT_MAX   65535
#endif

/* Int limits */
#ifndef INT_MIN
#define INT_MIN     (-2147483647 - 1)
#endif
#ifndef INT_MAX
#define INT_MAX     2147483647
#endif
#ifndef UINT_MAX
#define UINT_MAX    4294967295U
#endif

/* long follows the compiler ABI: ILP32/LLP64 use 32 bits, LP64 uses 64. */
#ifndef LONG_MIN
#define LONG_MIN    (-__LONG_MAX__ - 1L)
#endif
#ifndef LONG_MAX
#define LONG_MAX    __LONG_MAX__
#endif
#ifndef ULONG_MAX
#define ULONG_MAX   (2UL * __LONG_MAX__ + 1UL)
#endif

/* Long long limits */
#ifndef LLONG_MIN
#define LLONG_MIN   (-9223372036854775807LL - 1LL)
#endif
#ifndef LLONG_MAX
#define LLONG_MAX   9223372036854775807LL
#endif
#ifndef ULLONG_MAX
#define ULLONG_MAX  18446744073709551615ULL
#endif

/* size_t is pointer-width on every supported ABI, including LLP64 hosts. */
#ifndef SIZE_MAX
#define SIZE_MAX    __SIZE_MAX__
#endif
#ifndef SSIZE_MAX
#define SSIZE_MAX   LONG_MAX
#endif

/* Other */
#if defined(RIN_FREESTANDING) || !defined(__STDC_HOSTED__) || !__STDC_HOSTED__
#ifndef MB_LEN_MAX
#define MB_LEN_MAX  4
#endif
#endif

#ifndef PATH_MAX
#define PATH_MAX    1024
#endif

#ifndef NAME_MAX
#define NAME_MAX    255
#endif
#ifndef LINE_MAX
#define LINE_MAX    2048
#endif

#ifndef _POSIX_ARG_MAX
#define _POSIX_ARG_MAX 4096
#endif
#ifndef _POSIX_ATEXIT_MAX
#define _POSIX_ATEXIT_MAX 32
#endif
#ifndef _POSIX_CHILD_MAX
#define _POSIX_CHILD_MAX 25
#endif
#ifndef _POSIX_LINK_MAX
#define _POSIX_LINK_MAX 8
#endif
#ifndef _POSIX_NAME_MAX
#define _POSIX_NAME_MAX 14
#endif
#ifndef _POSIX_NGROUPS_MAX
#define _POSIX_NGROUPS_MAX 8
#endif
#ifndef _POSIX_OPEN_MAX
#define _POSIX_OPEN_MAX 20
#endif
#ifndef _POSIX_PATH_MAX
#define _POSIX_PATH_MAX 256
#endif
#ifndef _POSIX_SSIZE_MAX
#define _POSIX_SSIZE_MAX 32767
#endif
#ifndef _XOPEN_IOV_MAX
#define _XOPEN_IOV_MAX 16
#endif
#ifndef IOV_MAX
#define IOV_MAX 64
#endif
#ifndef _POSIX_SYMLINK_MAX
#define _POSIX_SYMLINK_MAX 255
#endif
#ifndef _POSIX2_LINE_MAX
#define _POSIX2_LINE_MAX 2048
#endif

/* Public limits backed by current kernel/libc owners. */
#ifndef ARG_MAX
#define ARG_MAX     (256 * 1024)
#endif
#ifndef CHILD_MAX
#define CHILD_MAX   25
#endif
#ifndef NGROUPS_MAX
#define NGROUPS_MAX 8
#endif
#ifndef OPEN_MAX
#define OPEN_MAX    1024
#endif
#ifndef TZNAME_MAX
#define TZNAME_MAX  31
#endif
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif
#ifndef _POSIX_HOST_NAME_MAX
#define _POSIX_HOST_NAME_MAX 255
#endif
#ifndef _POSIX_SEM_VALUE_MAX
#define _POSIX_SEM_VALUE_MAX 32767
#endif
#ifndef _POSIX_SEM_NSEMS_MAX
#define _POSIX_SEM_NSEMS_MAX 256
#endif
#ifndef _POSIX_AIO_MAX
#define _POSIX_AIO_MAX 1
#endif
#ifndef _POSIX_AIO_LISTIO_MAX
#define _POSIX_AIO_LISTIO_MAX 2
#endif
#ifndef _POSIX_MQ_OPEN_MAX
#define _POSIX_MQ_OPEN_MAX 8
#endif
#ifndef _POSIX_MQ_PRIO_MAX
#define _POSIX_MQ_PRIO_MAX 32
#endif
#ifndef _POSIX_LOGIN_NAME_MAX
#define _POSIX_LOGIN_NAME_MAX 9
#endif
#ifndef _POSIX_STREAM_MAX
#define _POSIX_STREAM_MAX 8
#endif
#ifndef _POSIX_TTY_NAME_MAX
#define _POSIX_TTY_NAME_MAX 9
#endif
#ifndef _POSIX_TZNAME_MAX
#define _POSIX_TZNAME_MAX 6
#endif
#ifndef _POSIX_DELAYTIMER_MAX
#define _POSIX_DELAYTIMER_MAX 32
#endif
#ifndef _POSIX_TIMER_MAX
#define _POSIX_TIMER_MAX 32
#endif
#ifndef _POSIX_PIPE_BUF
#define _POSIX_PIPE_BUF 512
#endif
#ifndef _POSIX_RE_DUP_MAX
#define _POSIX_RE_DUP_MAX 255
#endif
#ifndef _POSIX2_RE_DUP_MAX
#define _POSIX2_RE_DUP_MAX 255
#endif
#ifndef RE_DUP_MAX
#define RE_DUP_MAX 4096
#endif
#ifndef _POSIX_SYMLOOP_MAX
#define _POSIX_SYMLOOP_MAX 8
#endif
#ifndef SYMLOOP_MAX
#define SYMLOOP_MAX 16
#endif
#ifndef _POSIX_THREAD_DESTRUCTOR_ITERATIONS
#define _POSIX_THREAD_DESTRUCTOR_ITERATIONS 4
#endif
#ifndef PTHREAD_DESTRUCTOR_ITERATIONS
#define PTHREAD_DESTRUCTOR_ITERATIONS 4
#endif
#ifndef _POSIX_THREAD_KEYS_MAX
#define _POSIX_THREAD_KEYS_MAX 128
#endif
#ifndef PTHREAD_KEYS_MAX
#define PTHREAD_KEYS_MAX 128
#endif
#ifndef _POSIX_THREAD_THREADS_MAX
#define _POSIX_THREAD_THREADS_MAX 64
#endif
#ifndef SEM_VALUE_MAX
#define SEM_VALUE_MAX 0x7fffffffU
#endif
#ifndef SEM_NSEMS_MAX
#define SEM_NSEMS_MAX 256
#endif
#ifndef AIO_MAX
#define AIO_MAX 64
#endif
#ifndef ATEXIT_MAX
#define ATEXIT_MAX 256
#endif
#ifndef AIO_LISTIO_MAX
#define AIO_LISTIO_MAX 64
#endif
#ifndef AIO_PRIO_DELTA_MAX
#define AIO_PRIO_DELTA_MAX 0
#endif
#ifndef MQ_OPEN_MAX
#define MQ_OPEN_MAX 64
#endif
#ifndef MQ_PRIO_MAX
#define MQ_PRIO_MAX 32
#endif
#ifndef STREAM_MAX
#define STREAM_MAX 16
#endif
#ifndef TTY_NAME_MAX
#define TTY_NAME_MAX 16
#endif
#ifndef DELAYTIMER_MAX
#define DELAYTIMER_MAX 0x7fffffff
#endif
#ifndef TIMER_MAX
#define TIMER_MAX 32
#endif
#ifndef PIPE_BUF
#define PIPE_BUF 4096
#endif
#ifndef LOGIN_NAME_MAX
#define LOGIN_NAME_MAX 32
#endif

#endif /* _LIMITS_H */
