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
#define PATH_MAX    4096
#endif

#ifndef NAME_MAX
#define NAME_MAX    255
#endif

/* Public limits backed by current kernel/libc owners. */
#ifndef ARG_MAX
#define ARG_MAX     (256 * 1024)
#endif
#ifndef CHILD_MAX
#define CHILD_MAX   16
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
#define _POSIX_HOST_NAME_MAX HOST_NAME_MAX
#endif
#ifndef LOGIN_NAME_MAX
#define LOGIN_NAME_MAX 32
#endif

#endif /* _LIMITS_H */
