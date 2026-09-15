/*
 * RinOS libc - wctype.h
 * Wide classification via LibUnicode
 */

#ifndef _WCTYPE_H
#define _WCTYPE_H

#include "stddef.h"
#include "stdint.h"
#include "../libunicode/rin_unicode.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_WINT_T)
#if defined(__MINGW32__) && \
    defined(__SIZEOF_WCHAR_T__) && __SIZEOF_WCHAR_T__ == 2
/* MinGW's short-wchar builtin wide-classification ABI uses 16-bit wint_t,
 * including hosted fixtures compiled with the freestanding RinOS headers. */
typedef unsigned short wint_t;
#else
typedef unsigned int wint_t;
#endif
#define _WINT_T
#endif
#if !defined(_WCTYPE_T_DEFINED)
typedef unsigned long wctype_t;
#define _WCTYPE_T_DEFINED
#endif
typedef unsigned long wctrans_t;

#ifndef WEOF
#define WEOF ((wint_t)-1)
#endif

#ifndef MIDL_PASS
static inline int iswalnum(wint_t wc) { return rin_unicode_isalnum((uint32_t)wc); }
static inline int iswalpha(wint_t wc) { return rin_unicode_isalpha((uint32_t)wc); }
static inline int iswblank(wint_t wc) { return rin_unicode_isblank((uint32_t)wc); }
static inline int iswcntrl(wint_t wc) { return rin_unicode_iscntrl((uint32_t)wc); }
static inline int iswdigit(wint_t wc) { return rin_unicode_isdigit((uint32_t)wc); }
static inline int iswgraph(wint_t wc) { return rin_unicode_isgraph((uint32_t)wc); }
static inline int iswlower(wint_t wc) { return rin_unicode_islower((uint32_t)wc); }
static inline int iswprint(wint_t wc) { return rin_unicode_isprint((uint32_t)wc); }
static inline int iswpunct(wint_t wc) { return rin_unicode_ispunct((uint32_t)wc); }
static inline int iswspace(wint_t wc) { return rin_unicode_isspace((uint32_t)wc); }
static inline int iswupper(wint_t wc) { return rin_unicode_isupper((uint32_t)wc); }
static inline int iswxdigit(wint_t wc) { return rin_unicode_isxdigit((uint32_t)wc); }
static inline wint_t towlower(wint_t wc) { return (wint_t)rin_unicode_tolower((uint32_t)wc); }
static inline wint_t towupper(wint_t wc) { return (wint_t)rin_unicode_toupper((uint32_t)wc); }

static inline wctype_t wctype(const char* property) { return (wctype_t)rin_unicode_wctype(property); }
static inline int iswctype(wint_t wc, wctype_t desc) { return rin_unicode_iswctype((uint32_t)wc, (unsigned long)desc); }
static inline wctrans_t wctrans(const char* property) { return (wctrans_t)rin_unicode_wctrans(property); }
static inline wint_t towctrans(wint_t wc, wctrans_t desc) { return (wint_t)rin_unicode_towctrans((uint32_t)wc, (unsigned long)desc); }
#endif /* !MIDL_PASS */

#ifdef __cplusplus
}
#endif

#endif
