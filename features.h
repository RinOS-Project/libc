/*
 * RinOS features.h
 * Feature test macros for libc compatibility
 */

#ifndef RIN_LIBC_FEATURES_H
#define RIN_LIBC_FEATURES_H

/* hosted POSIX buildではhost CRTのfeature-test設定をそのまま使う。 */
#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    (defined(__unix__) || defined(__APPLE__)) && \
    (defined(__GNUC__) || defined(__clang__))
#include_next <features.h>
#else

#ifndef _FEATURES_H
#define _FEATURES_H

/* RinOS freestanding environment */
#define __RINOS__ 1

/* POSIX version compatibility (minimal) */
#define _POSIX_VERSION 200809L
#if defined(RIN_USERSPACE) && !defined(_POSIX_MESSAGE_PASSING)
#define _POSIX_MESSAGE_PASSING 200809L
#endif

/* Keep the explicit GNU request for the individual extension owners that
 * implement a GNU ABI, while leaving the broader GNU namespace disabled. */
#if defined(_GNU_SOURCE)
#define _RIN_USE_GNU_QSORT_R 1
#endif
#undef _GNU_SOURCE
#undef __USE_GNU

#endif /* _FEATURES_H */

#endif /* hosted POSIX feature-test delegation */
#endif /* RIN_LIBC_FEATURES_H */
