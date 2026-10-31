/* SPDX-License-Identifier: MIT */
/* C11 UTF-16 and UTF-32 multibyte conversion interfaces. */

#ifndef _RIN_UCHAR_H
#define _RIN_UCHAR_H
#define __STDC_VERSION_UCHAR_H__ 202311L


#include "stddef.h"
#include "stdint.h"
#include "wchar.h"

#ifndef _MSVCRT_COMPAT

#ifndef __cplusplus
typedef uint_least16_t char16_t;
typedef uint_least32_t char32_t;
#endif
#if (!defined(__cplusplus) && \
     (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)) || \
    (defined(__cplusplus) && __cplusplus < 202002L)
/* C23 and C++20 provide char8_t as a keyword; older modes use the byte alias
 * so the C23 conversion declarations below name the same type everywhere. */
#if defined(__cplusplus) && \
    ((defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 10) || \
     (defined(__clang__) && __clang_major__ >= 10))
#define __RIN_UCHAR_PRAGMA_CXX20_COMPAT 1
#endif
#ifdef __RIN_UCHAR_PRAGMA_CXX20_COMPAT
/* Suppress the diagnostic regarding char8_t being a keyword in C++20
 * (same approach as glibc's <uchar.h>). */
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wc++20-compat"
#endif
typedef unsigned char char8_t;
#ifdef __RIN_UCHAR_PRAGMA_CXX20_COMPAT
# pragma GCC diagnostic pop
#endif
#undef __RIN_UCHAR_PRAGMA_CXX20_COMPAT
#endif

#ifdef __cplusplus
extern "C" {
#endif

size_t mbrtoc16(char16_t* output, const char* input, size_t length,
                mbstate_t* state);
size_t c16rtomb(char* output, char16_t input, mbstate_t* state);
size_t mbrtoc32(char32_t* output, const char* input, size_t length,
                mbstate_t* state);
size_t c32rtomb(char* output, char32_t input, mbstate_t* state);
size_t mbrtoc8(char8_t* output, const char* input, size_t length,
               mbstate_t* state);
size_t c8rtomb(char* output, char8_t input, mbstate_t* state);

#ifdef __cplusplus
}
#endif

#endif /* !_MSVCRT_COMPAT */

#endif /* _RIN_UCHAR_H */
