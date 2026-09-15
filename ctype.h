/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - ctype.h
 * Character classification via LibUnicode
 */

#ifndef _CTYPE_H
#define _CTYPE_H

/* MSVCRT translation units own the Windows LC_* and setlocale ABI. */
#if !defined(RINCXX_CCTYPE_H) && !defined(_MSVCRT_COMPAT)
#include "locale.h"
#endif
#ifndef MIDL_PASS
#include "../libunicode/rin_unicode.h"
#endif

#ifndef MIDL_PASS
#ifdef __cplusplus
extern "C" {
#endif

#ifndef RINCXX_CCTYPE_H
#if defined(RIN_USERSPACE)
static inline int isalnum(int c) { return rin_locale_current_ctype_test(c, RIN_LOCALE_CTYPE_ALNUM); }
static inline int isalpha(int c) { return rin_locale_current_ctype_test(c, RIN_LOCALE_CTYPE_ALPHA); }
static inline int isdigit(int c) { return rin_locale_current_ctype_test(c, RIN_LOCALE_CTYPE_DIGIT); }
static inline int isxdigit(int c) { return rin_locale_current_ctype_test(c, RIN_LOCALE_CTYPE_XDIGIT); }
static inline int islower(int c) { return rin_locale_current_ctype_test(c, RIN_LOCALE_CTYPE_LOWER); }
static inline int isupper(int c) { return rin_locale_current_ctype_test(c, RIN_LOCALE_CTYPE_UPPER); }
static inline int isspace(int c) { return rin_locale_current_ctype_test(c, RIN_LOCALE_CTYPE_SPACE); }
static inline int isblank(int c) { return rin_locale_current_ctype_test(c, RIN_LOCALE_CTYPE_BLANK); }
static inline int iscntrl(int c) { return rin_locale_current_ctype_test(c, RIN_LOCALE_CTYPE_CNTRL); }
static inline int isprint(int c) { return rin_locale_current_ctype_test(c, RIN_LOCALE_CTYPE_PRINT); }
static inline int isgraph(int c) { return rin_locale_current_ctype_test(c, RIN_LOCALE_CTYPE_GRAPH); }
static inline int ispunct(int c) { return rin_locale_current_ctype_test(c, RIN_LOCALE_CTYPE_PUNCT); }
static inline int tolower(int c) { return rin_locale_current_ctype_map(c, RIN_LOCALE_CTYPE_TOLOWER); }
static inline int toupper(int c) { return rin_locale_current_ctype_map(c, RIN_LOCALE_CTYPE_TOUPPER); }
#else
static inline int isalnum(int c) { return rin_isalnum(c); }
static inline int isalpha(int c) { return rin_isalpha(c); }
static inline int isdigit(int c) { return rin_isdigit(c); }
static inline int isxdigit(int c) { return rin_isxdigit(c); }
static inline int islower(int c) { return rin_islower(c); }
static inline int isupper(int c) { return rin_isupper(c); }
static inline int isspace(int c) { return rin_isspace(c); }
static inline int isblank(int c) { return rin_isblank(c); }
static inline int iscntrl(int c) { return rin_iscntrl(c); }
static inline int isprint(int c) { return rin_isprint(c); }
static inline int isgraph(int c) { return rin_isgraph(c); }
static inline int ispunct(int c) { return rin_ispunct(c); }
static inline int tolower(int c) { return rin_tolower(c); }
static inline int toupper(int c) { return rin_toupper(c); }
#endif
static inline int isascii(int c) { return (unsigned int)c <= 127u; }
static inline int toascii(int c) { return c & 0x7F; }
#endif

#if !defined(RINCXX_CCTYPE_H) && !defined(_MSVCRT_COMPAT)
static inline int isalnum_l(int c, locale_t l) { return rin_locale_ctype_test(l, c, RIN_LOCALE_CTYPE_ALNUM); }
static inline int isalpha_l(int c, locale_t l) { return rin_locale_ctype_test(l, c, RIN_LOCALE_CTYPE_ALPHA); }
static inline int isdigit_l(int c, locale_t l) { return rin_locale_ctype_test(l, c, RIN_LOCALE_CTYPE_DIGIT); }
static inline int isxdigit_l(int c, locale_t l) { return rin_locale_ctype_test(l, c, RIN_LOCALE_CTYPE_XDIGIT); }
static inline int islower_l(int c, locale_t l) { return rin_locale_ctype_test(l, c, RIN_LOCALE_CTYPE_LOWER); }
static inline int isupper_l(int c, locale_t l) { return rin_locale_ctype_test(l, c, RIN_LOCALE_CTYPE_UPPER); }
static inline int isspace_l(int c, locale_t l) { return rin_locale_ctype_test(l, c, RIN_LOCALE_CTYPE_SPACE); }
static inline int isblank_l(int c, locale_t l) { return rin_locale_ctype_test(l, c, RIN_LOCALE_CTYPE_BLANK); }
static inline int iscntrl_l(int c, locale_t l) { return rin_locale_ctype_test(l, c, RIN_LOCALE_CTYPE_CNTRL); }
static inline int isprint_l(int c, locale_t l) { return rin_locale_ctype_test(l, c, RIN_LOCALE_CTYPE_PRINT); }
static inline int isgraph_l(int c, locale_t l) { return rin_locale_ctype_test(l, c, RIN_LOCALE_CTYPE_GRAPH); }
static inline int ispunct_l(int c, locale_t l) { return rin_locale_ctype_test(l, c, RIN_LOCALE_CTYPE_PUNCT); }
static inline int tolower_l(int c, locale_t l) { return rin_locale_ctype_map(l, c, RIN_LOCALE_CTYPE_TOLOWER); }
static inline int toupper_l(int c, locale_t l) { return rin_locale_ctype_map(l, c, RIN_LOCALE_CTYPE_TOUPPER); }
#endif

#ifdef __cplusplus
}
#endif
#endif /* !MIDL_PASS */

#endif
