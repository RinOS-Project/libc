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

#include "internal/posix_capabilities.h"

#ifndef _FEATURES_H
#define _FEATURES_H

/* RinOS freestanding environment */
#define __RINOS__ 1

/* Keep the explicit GNU request for the individual extension owners that
 * implement a GNU ABI, while leaving the broader GNU namespace disabled. */
#if defined(_GNU_SOURCE)
#define _RIN_USE_GNU_EVENTFD 1
#define _RIN_USE_GNU_CPU_SET 1
#define _RIN_USE_GNU_SYSINFO 1
#define _RIN_USE_GNU_QSORT_R 1
#define _RIN_USE_GNU_DIRENT 1
#define _RIN_USE_GNU_SEARCH 1
#define _RIN_USE_GNU_STRVERSCMP 1
#define _RIN_USE_XSI_NICE 1
#define _RIN_USE_BSD_RANDOM 1
#endif
#if defined(_DEFAULT_SOURCE) || defined(_BSD_SOURCE)
#define _RIN_USE_BSD_RANDOM 1
#endif
#undef _GNU_SOURCE
#undef __USE_GNU

#endif /* _FEATURES_H */

#endif /* hosted POSIX feature-test delegation */
#endif /* RIN_LIBC_FEATURES_H */
