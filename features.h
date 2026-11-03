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

/* Preserve the selected standard and extension namespaces after _GNU_SOURCE
 * is normalized into the per-owner switches below. */
#if (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 202405L) || \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 800)
#define __RIN_FEATURES_POSIX_2024 1
#endif
#if defined(_GNU_SOURCE)
#define __RIN_FEATURES_GNU 1
#endif
#if defined(_DEFAULT_SOURCE) || defined(_BSD_SOURCE) || \
    (!defined(__STRICT_ANSI__) && !defined(_POSIX_C_SOURCE) && \
     !defined(_XOPEN_SOURCE))
#define __RIN_FEATURES_DEFAULT 1
#endif

/* Keep the explicit GNU request for the individual extension owners that
 * implement a GNU ABI, while leaving the broader GNU namespace disabled. */
#if defined(_GNU_SOURCE)
#define _RIN_USE_GNU_ARGP 1
#define _RIN_USE_GNU_FOPENCOOKIE 1
#define _RIN_USE_GNU_EVENTFD 1
#define _RIN_USE_GNU_CPU_SET 1
#define _RIN_USE_GNU_SYSINFO 1
#define _RIN_USE_GNU_QSORT_R 1
#define _RIN_USE_GNU_DIRENT 1
#define _RIN_USE_GNU_SEARCH 1
#define _RIN_USE_GNU_STRVERSCMP 1
#define _RIN_USE_GNU_PTHREAD 1
#define _RIN_USE_GNU_GETTEXT 1
#define _RIN_USE_GNU_GETCPU 1
#define _RIN_USE_GNU_REALLOCARRAY 1
#define _RIN_USE_GNU_SECURE_GETENV 1
#define _RIN_USE_GNU_PTSNAME_R 1
#define _RIN_USE_GNU_MKOSTEMP 1
#define _RIN_USE_GNU_MKOSTEMPS 1
#define _RIN_USE_GNU_PIPE2 1
#define _RIN_USE_GNU_DUP3 1
#define _RIN_USE_BSD_GETENTROPY 1
#define _RIN_USE_GNU_FNMATCH 1
#define _RIN_USE_BSD_FNMATCH 1
#define _RIN_USE_BSD_SIGNAL 1
#define _RIN_USE_XSI_NICE 1
#define _RIN_USE_XSI_KILLPG 1
#define _RIN_USE_BSD_RANDOM 1
#endif
#if defined(_DEFAULT_SOURCE) || defined(_BSD_SOURCE)
#define _RIN_USE_BSD_RANDOM 1
#define _RIN_USE_BSD_GETTEXT 1
#define _RIN_USE_BSD_GETENTROPY 1
#define _RIN_USE_BSD_FNMATCH 1
#define _RIN_USE_BSD_SIGNAL 1
#define _RIN_USE_XSI_KILLPG 1
#endif
#if defined(_SVID_SOURCE)
#define _RIN_USE_BSD_SIGNAL 1
#endif
#if !defined(__STRICT_ANSI__) && !defined(_POSIX_C_SOURCE) && \
    !defined(_XOPEN_SOURCE)
#define _RIN_USE_XSI_KILLPG 1
#define _RIN_USE_BSD_GETTEXT 1
#define _RIN_USE_BSD_FNMATCH 1
#define _RIN_USE_BSD_SIGNAL 1
#endif
#if defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 500
#define _RIN_USE_XSI_KILLPG 1
#endif
#undef _GNU_SOURCE
#undef __USE_GNU

#endif /* _FEATURES_H */

#endif /* hosted POSIX feature-test delegation */

/* The hosted branch delegates macro policy to the host CRT, then exports the
 * same private selectors used by RinOS headers. */
#if !defined(__RINOS__)
#if (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 202405L) || \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 800)
#define __RIN_FEATURES_POSIX_2024 1
#endif
#if defined(_GNU_SOURCE) || defined(__USE_GNU)
#define __RIN_FEATURES_GNU 1
#endif
#if defined(_DEFAULT_SOURCE) || defined(_BSD_SOURCE) || \
    defined(__USE_MISC) || \
    (!defined(__STRICT_ANSI__) && !defined(_POSIX_C_SOURCE) && \
     !defined(_XOPEN_SOURCE))
#define __RIN_FEATURES_DEFAULT 1
#endif
#endif

#endif /* RIN_LIBC_FEATURES_H */
