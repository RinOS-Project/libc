/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - wchar.h
 * ワイド文字サポート
 */

#ifndef _WCHAR_H
#define _WCHAR_H

#include "stddef.h"
#include "stdint.h"
#include "limits.h"
#include "stdarg.h"
#include "wctype.h"
#include "errno.h"
#if defined(RIN_USERSPACE)
#include "locale.h"
#endif
#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define RIN_FP_PARSE_BINARY80 1
#endif
#include "internal/rin_float_parse.h"
#ifdef RIN_FP_PARSE_BINARY80
#undef RIN_FP_PARSE_BINARY80
#endif
#include "../libunicode/rin_unicode.h"

#ifdef __cplusplus
extern "C" {
#endif

/* wchar_t型 - C++ではビルトイン型、Cでのみ定義 */
#if !defined(__cplusplus) && !defined(_WCHAR_T_DEFINED)
#define _WCHAR_T_DEFINED
typedef int wchar_t;
#endif

/* NULL文字 */
#define WCHAR_NULL L'\0'

/* ═══════════════════════════════════════════════════════════════
 * ワイド文字列操作
 * _MSVCRT_COMPAT定義時はmsvcrt.cの実装を使用
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _MSVCRT_COMPAT

static inline size_t wcslen(const wchar_t* s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

static inline wchar_t* wcscpy(wchar_t* dest, const wchar_t* src) {
    wchar_t* d = dest;
    while ((*d++ = *src++));
    return dest;
}

static inline wchar_t* wcsncpy(wchar_t* dest, const wchar_t* src, size_t n) {
    wchar_t* d = dest;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = L'\0';
    return dest;
}

static inline wchar_t* wcscat(wchar_t* dest, const wchar_t* src) {
    wchar_t* d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

static inline int wcscmp(const wchar_t* s1, const wchar_t* s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *s1 < *s2 ? -1 : (*s1 > *s2 ? 1 : 0);
}

static inline int wcsncmp(const wchar_t* s1, const wchar_t* s2, size_t n) {
    while (n && *s1 && *s1 == *s2) { s1++; s2++; n--; }
    return n ? (*s1 < *s2 ? -1 : (*s1 > *s2 ? 1 : 0)) : 0;
}

static inline wchar_t* wcschr(const wchar_t* s, wchar_t c) {
    while (*s) {
        if (*s == c) return (wchar_t*)s;
        s++;
    }
    return (c == L'\0') ? (wchar_t*)s : NULL;
}

static inline wchar_t* wcsrchr(const wchar_t* s, wchar_t c) {
    const wchar_t* last = NULL;
    while (*s) {
        if (*s == c) last = s;
        s++;
    }
    return (c == L'\0') ? (wchar_t*)s : (wchar_t*)last;
}

static inline wchar_t* wcsstr(const wchar_t* haystack, const wchar_t* needle) {
    if (!*needle) return (wchar_t*)haystack;
    for (; *haystack; haystack++) {
        const wchar_t* h = haystack;
        const wchar_t* n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (wchar_t*)haystack;
    }
    return NULL;
}

static inline wchar_t* wcsncat(wchar_t* dest, const wchar_t* src, size_t n) {
    wchar_t* d = dest;
    while (*d) d++;
    while (n && *src) { *d++ = *src++; n--; }
    *d = L'\0';
    return dest;
}

static inline int wcscoll(const wchar_t* s1, const wchar_t* s2) {
    return rin_unicode_wcscoll32((const uint32_t*)s1, (const uint32_t*)s2);
}

static inline size_t wcsxfrm(wchar_t* dest, const wchar_t* src, size_t n) {
    return rin_unicode_wcsxfrm32((uint32_t*)dest, (const uint32_t*)src, n);
}

static inline size_t wcsspn(const wchar_t* s, const wchar_t* accept) {
    size_t count = 0;
    while (*s) {
        const wchar_t* a = accept;
        int found = 0;
        while (*a) { if (*s == *a++) { found = 1; break; } }
        if (!found) break;
        s++; count++;
    }
    return count;
}

static inline size_t wcscspn(const wchar_t* s, const wchar_t* reject) {
    size_t count = 0;
    while (*s) {
        const wchar_t* r = reject;
        while (*r) { if (*s == *r++) return count; }
        s++; count++;
    }
    return count;
}

static inline wchar_t* wcspbrk(const wchar_t* s, const wchar_t* accept) {
    while (*s) {
        const wchar_t* a = accept;
        while (*a) { if (*s == *a++) return (wchar_t*)s; }
        s++;
    }
    return NULL;
}

static inline wchar_t* wcstok(wchar_t* str, const wchar_t* delim, wchar_t** saveptr) {
    wchar_t* token;
    if (str) *saveptr = str;
    if (!*saveptr) return NULL;

    /* Skip leading delimiters */
    *saveptr += wcsspn(*saveptr, delim);
    if (!**saveptr) { *saveptr = NULL; return NULL; }

    token = *saveptr;
    *saveptr += wcscspn(*saveptr, delim);
    if (**saveptr) { **saveptr = L'\0'; (*saveptr)++; }
    else *saveptr = NULL;
    return token;
}

#endif /* !_MSVCRT_COMPAT */

/* ═══════════════════════════════════════════════════════════════
 * ワイドメモリ操作
 * ═══════════════════════════════════════════════════════════════*/

static inline wchar_t* wmemcpy(wchar_t* dest, const wchar_t* src, size_t n) {
    for (size_t i = 0; i < n; i++) dest[i] = src[i];
    return dest;
}

static inline wchar_t* wmemmove(wchar_t* dest, const wchar_t* src, size_t n) {
    if (dest < src) {
        for (size_t i = 0; i < n; i++) dest[i] = src[i];
    } else {
        for (size_t i = n; i > 0; i--) dest[i-1] = src[i-1];
    }
    return dest;
}

static inline int wmemcmp(const wchar_t* s1, const wchar_t* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] < s2[i] ? -1 : 1;
    }
    return 0;
}

static inline wchar_t* wmemchr(const wchar_t* s, wchar_t c, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s[i] == c) return (wchar_t*)&s[i];
    }
    return NULL;
}

static inline wchar_t* wmemset(wchar_t* s, wchar_t c, size_t n) {
    for (size_t i = 0; i < n; i++) s[i] = c;
    return s;
}

/* ═══════════════════════════════════════════════════════════════
 * ワイド文字I/O
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _FILE_DECLARED
#define _FILE_DECLARED
struct _FILE;
typedef struct _FILE FILE;
#endif

int fwide(FILE* stream, int mode);
wint_t fgetwc(FILE* stream);
wchar_t* fgetws(wchar_t* s, int n, FILE* stream);
wint_t fputwc(wchar_t c, FILE* stream);
int fputws(const wchar_t* s, FILE* stream);
wint_t getwc(FILE* stream);
wint_t getwchar(void);
wint_t putwc(wchar_t c, FILE* stream);
wint_t putwchar(wchar_t c);
wint_t ungetwc(wint_t c, FILE* stream);

/* ═══════════════════════════════════════════════════════════════
 * ワイド文字printf/scanf
 * ═══════════════════════════════════════════════════════════════*/

int fwprintf(FILE* stream, const wchar_t* format, ...);
int fwscanf(FILE* stream, const wchar_t* format, ...);
int vfwprintf(FILE* stream, const wchar_t* format, va_list ap);
int vfwscanf(FILE* stream, const wchar_t* format, va_list ap);
int vswprintf(wchar_t* s, size_t n, const wchar_t* format, va_list ap);
int vswscanf(const wchar_t* s, const wchar_t* format, va_list ap);
int vwprintf(const wchar_t* format, va_list ap);
int vwscanf(const wchar_t* format, va_list ap);
int wprintf(const wchar_t* format, ...);
int wscanf(const wchar_t* format, ...);
int swprintf(wchar_t* s, size_t n, const wchar_t* format, ...);
int swscanf(const wchar_t* s, const wchar_t* format, ...);

/* ═══════════════════════════════════════════════════════════════
 * Multibyte / wide conversion
 * ═══════════════════════════════════════════════════════════════*/

typedef rin_unicode_mbstate_t mbstate_t;

#define MB_CUR_MAX 4

/* これらの関数は stdlib.h でも定義されているので、重複を避ける */
#if !defined(_MSVCRT_COMPAT) && !defined(_STDLIB_WCHAR_FUNCS_DEFINED)
#define _STDLIB_WCHAR_FUNCS_DEFINED

static inline int mbtowc(wchar_t* pwc, const char* s, size_t n) {
    return rin_unicode_mbtowc32((uint32_t*)pwc, s, n);
}

static inline int wctomb(char* s, wchar_t wc) {
    if (!s) return 0;
    return rin_unicode_wctomb32(s, (uint32_t)wc);
}

static inline size_t mbstowcs(wchar_t* dest, const char* src, size_t n) {
    return rin_unicode_mbstowcs32((uint32_t*)dest, src, n);
}

static inline size_t wcstombs(char* dest, const wchar_t* src, size_t n) {
    return rin_unicode_wcstombs32(dest, (const uint32_t*)src, n);
}

#endif /* !_MSVCRT_COMPAT && !_STDLIB_WCHAR_FUNCS_DEFINED */

/* ═══════════════════════════════════════════════════════════════
 * ワイド文字数値変換
 * ═══════════════════════════════════════════════════════════════*/

#ifndef RIN_WC_FLOAT_MAX
#define RIN_WC_FLOAT_MAX 4096u
#endif

typedef struct rin_wc_float_input {
    char text[RIN_WC_FLOAT_MAX + 1u];
    const wchar_t* start;
    size_t length;
    int too_long;
} rin_wc_float_input;

static inline rin_wc_float_input rin_wc_float_prepare(const wchar_t* nptr) {
    rin_wc_float_input input;
    const wchar_t* cursor = nptr;
    input.start = nptr;
    input.length = 0u;
    input.too_long = 0;
    input.text[0] = '\0';
    if (!nptr) return input;
    /* The binary parser owns ASCII C-locale whitespace.  Skip any additional
     * wide whitespace here, while retaining the original pointer for the
     * no-conversion endptr contract. */
    while (iswspace((wint_t)*cursor)) ++cursor;
    input.start = cursor;
    while (*cursor && (unsigned int)*cursor <= 0x7fu) {
        if (input.length == RIN_WC_FLOAT_MAX) {
            input.too_long = 1;
            break;
        }
        input.text[input.length++] = (char)*cursor++;
    }
    input.text[input.length] = '\0';
    return input;
}

static inline double wcstod(const wchar_t* nptr, wchar_t** endptr) {
    rin_wc_float_input input = rin_wc_float_prepare(nptr);
    rin_float_parse_result parsed;
    const wchar_t* original = nptr;
    if (!nptr) {
        if (endptr) *endptr = (wchar_t*)0;
        errno = EINVAL;
        return 0.0;
    }
    if (input.too_long) {
        if (endptr) *endptr = (wchar_t*)(input.start + input.length);
        errno = ERANGE;
        return 0.0;
    }
    parsed = rin_float_parse_binary64(input.text);
    if (endptr) {
        *endptr = parsed.converted
                      ? (wchar_t*)(input.start + (parsed.end - input.text))
                      : (wchar_t*)original;
    }
    if (parsed.invalid_input) errno = EINVAL;
    else if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
    return rin_float_parse_binary64_value(parsed.bits);
}

static inline float wcstof(const wchar_t* nptr, wchar_t** endptr) {
    rin_wc_float_input input = rin_wc_float_prepare(nptr);
    rin_float_parse_result parsed;
    const wchar_t* original = nptr;
    if (!nptr) {
        if (endptr) *endptr = (wchar_t*)0;
        errno = EINVAL;
        return 0.0f;
    }
    if (input.too_long) {
        if (endptr) *endptr = (wchar_t*)(input.start + input.length);
        errno = ERANGE;
        return 0.0f;
    }
    parsed = rin_float_parse_binary32(input.text);
    if (endptr) {
        *endptr = parsed.converted
                      ? (wchar_t*)(input.start + (parsed.end - input.text))
                      : (wchar_t*)original;
    }
    if (parsed.invalid_input) errno = EINVAL;
    else if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
    return rin_float_parse_binary32_value(parsed.bits);
}

static inline long double wcstold(const wchar_t* nptr, wchar_t** endptr) {
    rin_wc_float_input input = rin_wc_float_prepare(nptr);
    const wchar_t* original = nptr;
    if (!nptr) {
        if (endptr) *endptr = (wchar_t*)0;
        errno = EINVAL;
        return 0.0L;
    }
    if (input.too_long) {
        if (endptr) *endptr = (wchar_t*)(input.start + input.length);
        errno = ERANGE;
        return 0.0L;
    }
#if defined(RIN_FP_HAS_BINARY80)
    {
        rin_long_double_parse_result parsed = rin_float_parse_binary80(input.text);
        if (endptr) {
            *endptr = parsed.converted
                          ? (wchar_t*)(input.start + (parsed.end - input.text))
                          : (wchar_t*)original;
        }
        if (parsed.invalid_input) errno = EINVAL;
        else if (parsed.range_direction != 0 || parsed.subnormal)
            errno = ERANGE;
        return parsed.value;
    }
#else
    {
        rin_float_parse_result parsed = rin_float_parse_binary64(input.text);
        if (endptr) {
            *endptr = parsed.converted
                          ? (wchar_t*)(input.start + (parsed.end - input.text))
                          : (wchar_t*)original;
        }
        if (parsed.invalid_input) errno = EINVAL;
        else if (parsed.range_direction != 0 || parsed.subnormal)
            errno = ERANGE;
        return (long double)rin_float_parse_binary64_value(parsed.bits);
    }
#endif
}

/* Wide integer conversion uses the same bounded, overflow-safe contract as
 * the narrow libc parser.  The previous implementation accumulated directly
 * in long, so wcstoll/wcstoull truncated on LLP64 and signed overflow was
 * undefined.  Keep the candidate magnitude unsigned until the target range
 * has been checked, and consume all valid digits after an overflow. */
typedef struct rin_wc_integer_result {
    unsigned long long magnitude;
    const wchar_t* end;
    int negative;
    int any;
    int overflow;
} rin_wc_integer_result;

static inline int rin_wc_integer_digit(wchar_t value) {
    if (value >= L'0' && value <= L'9') return (int)(value - L'0');
    if (value >= L'a' && value <= L'z') return (int)(value - L'a') + 10;
    if (value >= L'A' && value <= L'Z') return (int)(value - L'A') + 10;
    return -1;
}

static inline rin_wc_integer_result rin_wc_integer_parse(
    const wchar_t* nptr, int base, unsigned long long positive_limit,
    unsigned long long negative_limit) {
    rin_wc_integer_result result;
    const wchar_t* original = nptr;
    const wchar_t* cursor;
    unsigned long long limit;
    int digit;

    result.magnitude = 0ULL;
    result.end = nptr;
    result.negative = 0;
    result.any = 0;
    result.overflow = 0;
    if (!nptr) {
        errno = EINVAL;
        return result;
    }
    if (base != 0 && (base < 2 || base > 36)) {
        errno = EINVAL;
        return result;
    }
    cursor = nptr;
    while (iswspace((wint_t)*cursor)) ++cursor;
    if (*cursor == L'-') {
        result.negative = 1;
        ++cursor;
    } else if (*cursor == L'+') {
        ++cursor;
    }
    limit = result.negative ? negative_limit : positive_limit;

    if (base == 0) {
        if (cursor[0] == L'0') {
            if ((cursor[1] == L'x' || cursor[1] == L'X') &&
                rin_wc_integer_digit(cursor[2]) >= 0 &&
                rin_wc_integer_digit(cursor[2]) < 16) {
                base = 16;
                cursor += 2;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16 && cursor[0] == L'0' &&
               (cursor[1] == L'x' || cursor[1] == L'X') &&
               rin_wc_integer_digit(cursor[2]) >= 0 &&
               rin_wc_integer_digit(cursor[2]) < 16) {
        cursor += 2;
    }
    while ((digit = rin_wc_integer_digit(*cursor)) >= 0 && digit < base) {
        result.any = 1;
        if (!result.overflow) {
            const unsigned long long value = (unsigned long long)digit;
            const unsigned long long radix = (unsigned long long)base;
            if (result.magnitude > (limit - value) / radix) {
                result.magnitude = limit;
                result.overflow = 1;
            } else {
                result.magnitude = result.magnitude * radix + value;
            }
        }
        ++cursor;
    }
    if (!result.any) {
        result.end = original;
        return result;
    }
    result.end = cursor;
    if (result.overflow) errno = ERANGE;
    return result;
}

static inline long wcstol(const wchar_t* nptr, wchar_t** endptr, int base) {
    const unsigned long long positive_limit = (unsigned long long)LONG_MAX;
    const unsigned long long negative_limit = positive_limit + 1ULL;
    rin_wc_integer_result parsed = rin_wc_integer_parse(
        nptr, base, positive_limit, negative_limit);
    if (endptr) *endptr = (wchar_t*)parsed.end;
    if (!parsed.any) return 0L;
    if (parsed.overflow) return parsed.negative ? LONG_MIN : LONG_MAX;
    if (parsed.negative) {
        if (parsed.magnitude == negative_limit) return LONG_MIN;
        return -(long)parsed.magnitude;
    }
    return (long)parsed.magnitude;
}

static inline long long wcstoll(const wchar_t* nptr, wchar_t** endptr, int base) {
    const unsigned long long positive_limit = (unsigned long long)LLONG_MAX;
    const unsigned long long negative_limit = positive_limit + 1ULL;
    rin_wc_integer_result parsed = rin_wc_integer_parse(
        nptr, base, positive_limit, negative_limit);
    if (endptr) *endptr = (wchar_t*)parsed.end;
    if (!parsed.any) return 0LL;
    if (parsed.overflow) return parsed.negative ? LLONG_MIN : LLONG_MAX;
    if (parsed.negative) {
        if (parsed.magnitude == negative_limit) return LLONG_MIN;
        return -(long long)parsed.magnitude;
    }
    return (long long)parsed.magnitude;
}

static inline unsigned long wcstoul(const wchar_t* nptr, wchar_t** endptr, int base) {
    rin_wc_integer_result parsed = rin_wc_integer_parse(
        nptr, base, (unsigned long long)ULONG_MAX,
        (unsigned long long)ULONG_MAX);
    if (endptr) *endptr = (wchar_t*)parsed.end;
    if (!parsed.any) return 0UL;
    return parsed.negative ? (unsigned long)(0UL - (unsigned long)parsed.magnitude)
                           : (unsigned long)parsed.magnitude;
}

static inline unsigned long long wcstoull(const wchar_t* nptr, wchar_t** endptr,
                                          int base) {
    rin_wc_integer_result parsed = rin_wc_integer_parse(
        nptr, base, ULLONG_MAX, ULLONG_MAX);
    if (endptr) *endptr = (wchar_t*)parsed.end;
    if (!parsed.any) return 0ULL;
    return parsed.negative ? 0ULL - parsed.magnitude : parsed.magnitude;
}

/* ═══════════════════════════════════════════════════════════════
 * マルチバイト/ワイド文字変換 (追加関数)
 * ═══════════════════════════════════════════════════════════════*/

#ifndef EOF
#define EOF (-1)
#endif

static inline wint_t btowc(int c) {
    if (c == EOF) return WEOF;
    return (wint_t)(uint32_t)(unsigned char)c;
}

static inline int wctob(wint_t c) {
    if (c == WEOF || !rin_unicode_is_valid_scalar((uint32_t)c) || c > 0xFF) return EOF;
    return (int)(unsigned char)c;
}

static inline int mbsinit(const mbstate_t* ps) {
    return rin_unicode_mbsinit((const rin_unicode_mbstate_t*)ps);
}

static inline size_t mbrlen(const char* s, size_t n, mbstate_t* ps) {
    return rin_unicode_mbrlen(s, n, (rin_unicode_mbstate_t*)ps);
}

static inline size_t mbrtowc(wchar_t* pwc, const char* s, size_t n, mbstate_t* ps) {
    return rin_unicode_mbrtowc32((uint32_t*)pwc, s, n, (rin_unicode_mbstate_t*)ps);
}

static inline size_t wcrtomb(char* s, wchar_t wc, mbstate_t* ps) {
    return rin_unicode_wcrtomb32(s, (uint32_t)wc, (rin_unicode_mbstate_t*)ps);
}

static inline size_t mbsrtowcs(wchar_t* dest, const char** src, size_t len, mbstate_t* ps) {
    return rin_unicode_mbsrtowcs32((uint32_t*)dest, src, len, (rin_unicode_mbstate_t*)ps);
}

static inline size_t wcsrtombs(char* dest, const wchar_t** src, size_t len, mbstate_t* ps) {
    return rin_unicode_wcsrtombs32(dest, (const uint32_t**)src, len, (rin_unicode_mbstate_t*)ps);
}

/* ═══════════════════════════════════════════════════════════════
 * 時刻関数
 * ═══════════════════════════════════════════════════════════════*/

struct tm;  /* Forward declaration */

size_t wcsftime(wchar_t* s, size_t maxsize, const wchar_t* format,
                const struct tm* timeptr);
#if defined(RIN_USERSPACE)
/* POSIX locale-specific wide formatter.  The locale object is immutable for
 * the duration of the call; LC_GLOBAL_LOCALE selects the process locale. */
size_t wcsftime_l(wchar_t* s, size_t maxsize, const wchar_t* format,
                  const struct tm* timeptr, locale_t locale);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _WCHAR_H */
