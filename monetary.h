/* SPDX-License-Identifier: MIT */
/* POSIX monetary formatting interfaces. */

#ifndef _RIN_MONETARY_H
#define _RIN_MONETARY_H

#include "features.h"
#include <stddef.h>
#include <sys/types.h>
#include <locale.h>

#if defined(__RINOS__) || defined(__USE_XOPEN_EXTENDED) || \
    defined(__USE_GNU) || defined(__DARWIN_C_SOURCE) || \
    (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L) || \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 500) || \
    (defined(__XSI_VISIBLE) && __XSI_VISIBLE)

#ifdef __cplusplus
extern "C" {
#define RIN_MONETARY_RESTRICT
#else
#define RIN_MONETARY_RESTRICT restrict
#endif

ssize_t strfmon(char* RIN_MONETARY_RESTRICT s, size_t maxsize,
                const char* RIN_MONETARY_RESTRICT format, ...);

#if defined(__RINOS__) || defined(__USE_XOPEN2K8) || \
    defined(__USE_XOPEN2K8XSI) || defined(__USE_GNU) || \
    defined(__DARWIN_C_SOURCE) || \
    (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L) || \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 700) || \
    (defined(__XSI_VISIBLE) && __XSI_VISIBLE >= 700)
ssize_t strfmon_l(char* RIN_MONETARY_RESTRICT s, size_t maxsize,
                  locale_t locale,
                  const char* RIN_MONETARY_RESTRICT format, ...);
#endif

#ifdef __cplusplus
#undef RIN_MONETARY_RESTRICT
}
#else
#undef RIN_MONETARY_RESTRICT
#endif

#endif /* POSIX monetary interfaces */

#endif
