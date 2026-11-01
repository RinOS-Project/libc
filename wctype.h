/*
 * RinOS libc - wctype.h
 * Wide classification via LibUnicode
 */

#ifndef _WCTYPE_H
#define _WCTYPE_H

#include "stddef.h"
#include "stdint.h"
#include "../libunicode/rin_unicode.h"
#if defined(RIN_USERSPACE)
#include "locale.h"
#endif

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
#if defined(RIN_USERSPACE)
static inline int rin_wctype_current_test(wint_t wc, uint32_t operation) {
    return wc == WEOF ? 0 : rin_locale_current_wide_ctype_test(
        (uint32_t)wc, operation);
}
static inline int rin_wctype_locale_test(wint_t wc, uint32_t operation,
                                         locale_t locale) {
    return wc == WEOF ? 0 : rin_locale_wide_ctype_test(
        locale, (uint32_t)wc, operation);
}
static inline wint_t rin_wctype_current_map(wint_t wc, uint32_t operation) {
    return wc == WEOF ? WEOF : (wint_t)rin_locale_current_wide_ctype_map(
        (uint32_t)wc, operation);
}
static inline wint_t rin_wctype_locale_map(wint_t wc, uint32_t operation,
                                           locale_t locale) {
    return wc == WEOF ? WEOF : (wint_t)rin_locale_wide_ctype_map(
        locale, (uint32_t)wc, operation);
}
static inline int rin_wctype_current_desc_test(wint_t wc, wctype_t desc) {
    return wc == WEOF ? 0 : rin_locale_current_wide_ctype_iswctype(
        (uint32_t)wc, (unsigned long)desc);
}
static inline int rin_wctype_locale_desc_test(wint_t wc, wctype_t desc,
                                              locale_t locale) {
    return wc == WEOF ? 0 : rin_locale_wide_ctype_iswctype(
        locale, (uint32_t)wc, (unsigned long)desc);
}
static inline wint_t rin_wctype_current_transform(wint_t wc,
                                                  wctrans_t desc) {
    return wc == WEOF ? WEOF : (wint_t)rin_locale_current_wide_ctype_towctrans(
        (uint32_t)wc, (unsigned long)desc);
}
static inline wint_t rin_wctype_locale_transform(wint_t wc, wctrans_t desc,
                                                 locale_t locale) {
    return wc == WEOF ? WEOF : (wint_t)rin_locale_wide_ctype_towctrans(
        locale, (uint32_t)wc, (unsigned long)desc);
}
#endif

#if defined(RIN_USERSPACE)
static inline int iswalnum(wint_t wc) { return rin_wctype_current_test(wc, RIN_LOCALE_CTYPE_ALNUM); }
static inline int iswalpha(wint_t wc) { return rin_wctype_current_test(wc, RIN_LOCALE_CTYPE_ALPHA); }
static inline int iswblank(wint_t wc) { return rin_wctype_current_test(wc, RIN_LOCALE_CTYPE_BLANK); }
static inline int iswcntrl(wint_t wc) { return rin_wctype_current_test(wc, RIN_LOCALE_CTYPE_CNTRL); }
static inline int iswdigit(wint_t wc) { return rin_wctype_current_test(wc, RIN_LOCALE_CTYPE_DIGIT); }
static inline int iswgraph(wint_t wc) { return rin_wctype_current_test(wc, RIN_LOCALE_CTYPE_GRAPH); }
static inline int iswlower(wint_t wc) { return rin_wctype_current_test(wc, RIN_LOCALE_CTYPE_LOWER); }
static inline int iswprint(wint_t wc) { return rin_wctype_current_test(wc, RIN_LOCALE_CTYPE_PRINT); }
static inline int iswpunct(wint_t wc) { return rin_wctype_current_test(wc, RIN_LOCALE_CTYPE_PUNCT); }
static inline int iswspace(wint_t wc) { return rin_wctype_current_test(wc, RIN_LOCALE_CTYPE_SPACE); }
static inline int iswupper(wint_t wc) { return rin_wctype_current_test(wc, RIN_LOCALE_CTYPE_UPPER); }
static inline int iswxdigit(wint_t wc) { return rin_wctype_current_test(wc, RIN_LOCALE_CTYPE_XDIGIT); }
static inline wint_t towlower(wint_t wc) { return rin_wctype_current_map(wc, RIN_LOCALE_CTYPE_TOLOWER); }
static inline wint_t towupper(wint_t wc) { return rin_wctype_current_map(wc, RIN_LOCALE_CTYPE_TOUPPER); }
#else
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
#endif

static inline int rin_wctype_name_equal(const char* name,
                                        const char* expected) {
    size_t index = 0u;
    if (!name || !expected) return 0;
    while (name[index] != '\0' && expected[index] != '\0' &&
           name[index] == expected[index])
        ++index;
    return name[index] == '\0' && expected[index] == '\0';
}

/* POSIX descriptor names are case-sensitive. The Unicode backend's helper
 * accepts ASCII case variants for its internal callers, so validate the
 * public spelling before asking it for the canonical descriptor. */
static inline wctype_t rin_wctype_resolve(const char* property) {
    static const char* const names[] = {
        "alnum", "alpha", "blank", "cntrl", "digit", "graph",
        "lower", "print", "punct", "space", "upper", "xdigit"
    };
    size_t index;
    if (!property) return (wctype_t)0;
    for (index = 0u; index < sizeof(names) / sizeof(names[0]); ++index)
        if (rin_wctype_name_equal(property, names[index]))
            return (wctype_t)rin_unicode_wctype(names[index]);
    return (wctype_t)0;
}

static inline wctrans_t rin_wctrans_resolve(const char* property) {
    if (rin_wctype_name_equal(property, "tolower"))
        return (wctrans_t)rin_unicode_wctrans("tolower");
    if (rin_wctype_name_equal(property, "toupper"))
        return (wctrans_t)rin_unicode_wctrans("toupper");
    return (wctrans_t)0;
}

static inline wctype_t wctype(const char* property) {
    return rin_wctype_resolve(property);
}
#if defined(RIN_USERSPACE)
static inline int iswctype(wint_t wc, wctype_t desc) { return rin_wctype_current_desc_test(wc, desc); }
static inline wctype_t wctype_l(const char* property, locale_t locale) {
#if !defined(_MSVCRT_COMPAT)
    if (rin_locale_l_name(locale, LC_CTYPE) == NULL) return (wctype_t)0;
#else
    (void)locale;
#endif
    return rin_wctype_resolve(property);
}
static inline int iswalnum_l(wint_t wc, locale_t locale) { return rin_wctype_locale_test(wc, RIN_LOCALE_CTYPE_ALNUM, locale); }
static inline int iswalpha_l(wint_t wc, locale_t locale) { return rin_wctype_locale_test(wc, RIN_LOCALE_CTYPE_ALPHA, locale); }
static inline int iswblank_l(wint_t wc, locale_t locale) { return rin_wctype_locale_test(wc, RIN_LOCALE_CTYPE_BLANK, locale); }
static inline int iswcntrl_l(wint_t wc, locale_t locale) { return rin_wctype_locale_test(wc, RIN_LOCALE_CTYPE_CNTRL, locale); }
static inline int iswdigit_l(wint_t wc, locale_t locale) { return rin_wctype_locale_test(wc, RIN_LOCALE_CTYPE_DIGIT, locale); }
static inline int iswgraph_l(wint_t wc, locale_t locale) { return rin_wctype_locale_test(wc, RIN_LOCALE_CTYPE_GRAPH, locale); }
static inline int iswlower_l(wint_t wc, locale_t locale) { return rin_wctype_locale_test(wc, RIN_LOCALE_CTYPE_LOWER, locale); }
static inline int iswprint_l(wint_t wc, locale_t locale) { return rin_wctype_locale_test(wc, RIN_LOCALE_CTYPE_PRINT, locale); }
static inline int iswpunct_l(wint_t wc, locale_t locale) { return rin_wctype_locale_test(wc, RIN_LOCALE_CTYPE_PUNCT, locale); }
static inline int iswspace_l(wint_t wc, locale_t locale) { return rin_wctype_locale_test(wc, RIN_LOCALE_CTYPE_SPACE, locale); }
static inline int iswupper_l(wint_t wc, locale_t locale) { return rin_wctype_locale_test(wc, RIN_LOCALE_CTYPE_UPPER, locale); }
static inline int iswxdigit_l(wint_t wc, locale_t locale) { return rin_wctype_locale_test(wc, RIN_LOCALE_CTYPE_XDIGIT, locale); }
static inline wint_t towlower_l(wint_t wc, locale_t locale) { return rin_wctype_locale_map(wc, RIN_LOCALE_CTYPE_TOLOWER, locale); }
static inline wint_t towupper_l(wint_t wc, locale_t locale) { return rin_wctype_locale_map(wc, RIN_LOCALE_CTYPE_TOUPPER, locale); }
static inline int iswctype_l(wint_t wc, wctype_t desc, locale_t locale) { return rin_wctype_locale_desc_test(wc, desc, locale); }
#else
static inline int iswctype(wint_t wc, wctype_t desc) { return rin_unicode_iswctype((uint32_t)wc, (unsigned long)desc); }
#endif
static inline wctrans_t wctrans(const char* property) {
    return rin_wctrans_resolve(property);
}
#if defined(RIN_USERSPACE)
static inline wctrans_t wctrans_l(const char* property, locale_t locale) {
#if !defined(_MSVCRT_COMPAT)
    if (rin_locale_l_name(locale, LC_CTYPE) == NULL) return (wctrans_t)0;
#else
    (void)locale;
#endif
    return rin_wctrans_resolve(property);
}
static inline wint_t towctrans(wint_t wc, wctrans_t desc) { return rin_wctype_current_transform(wc, desc); }
static inline wint_t towctrans_l(wint_t wc, wctrans_t desc, locale_t locale) { return rin_wctype_locale_transform(wc, desc, locale); }
#else
static inline wint_t towctrans(wint_t wc, wctrans_t desc) { return (wint_t)rin_unicode_towctrans((uint32_t)wc, (unsigned long)desc); }
#endif
#endif /* !MIDL_PASS */

#ifdef __cplusplus
}
#endif

#endif
