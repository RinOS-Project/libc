/* SPDX-License-Identifier: MIT */
/* C11 UTF-16 and UTF-32 multibyte conversion interfaces. */

#ifndef _RIN_UCHAR_H
#define _RIN_UCHAR_H

#include "stddef.h"
#include "stdint.h"
#include "wchar.h"

#ifndef _MSVCRT_COMPAT

#ifndef __cplusplus
typedef uint_least16_t char16_t;
typedef uint_least32_t char32_t;
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

#ifdef __cplusplus
}
#endif

#endif /* !_MSVCRT_COMPAT */

#endif /* _RIN_UCHAR_H */
