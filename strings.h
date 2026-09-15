/*
 * RinOS libc - strings.h
 * 追加文字列関数 (BSD互換)
 */

#ifndef _STRINGS_H
#define _STRINGS_H

#include "stddef.h"

#ifndef MIDL_PASS
#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * 大文字小文字変換ヘルパー
 * ═══════════════════════════════════════════════════════════════*/

static inline int _strings_tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

/* ═══════════════════════════════════════════════════════════════
 * 大文字小文字を無視した比較
 * ═══════════════════════════════════════════════════════════════*/

/* strcasecmp - 大文字小文字を無視して文字列を比較 */
static inline int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        int c1 = _strings_tolower((unsigned char)*s1);
        int c2 = _strings_tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return _strings_tolower((unsigned char)*s1) - _strings_tolower((unsigned char)*s2);
}

/* strncasecmp - 大文字小文字を無視してn文字比較 */
static inline int strncasecmp(const char* s1, const char* s2, size_t n) {
    while (n > 0 && *s1 && *s2) {
        int c1 = _strings_tolower((unsigned char)*s1);
        int c2 = _strings_tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return _strings_tolower((unsigned char)*s1) - _strings_tolower((unsigned char)*s2);
}

/* ═══════════════════════════════════════════════════════════════
 * メモリ操作 (BSD互換、非推奨)
 * ═══════════════════════════════════════════════════════════════*/

/* bzero - メモリをゼロクリア (memsetを使用推奨) */
static inline void bzero(void* s, size_t n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) *p++ = 0;
}

/* bcopy - メモリコピー (memmoveを使用推奨) */
static inline void bcopy(const void* src, void* dest, size_t n) {
    const unsigned char* s = (const unsigned char*)src;
    unsigned char* d = (unsigned char*)dest;

    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        s += n;
        d += n;
        while (n--) *--d = *--s;
    }
}

/* bcmp - メモリ比較 (memcmpを使用推奨) */
static inline int bcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;

    while (n--) {
        if (*p1++ != *p2++) return 1;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * ビット操作
 * ═══════════════════════════════════════════════════════════════*/

/* ffs - 最初の1ビットの位置を返す (1から開始、0なら0) */
static inline int ffs(int i) {
    if (i == 0) return 0;
    int pos = 1;
    while ((i & 1) == 0) {
        i >>= 1;
        pos++;
    }
    return pos;
}

/* ffsl - long版 */
static inline int ffsl(long i) {
    if (i == 0) return 0;
    int pos = 1;
    while ((i & 1) == 0) {
        i >>= 1;
        pos++;
    }
    return pos;
}

/* ffsll - long long版 */
static inline int ffsll(long long i) {
    if (i == 0) return 0;
    int pos = 1;
    while ((i & 1) == 0) {
        i >>= 1;
        pos++;
    }
    return pos;
}

/* ═══════════════════════════════════════════════════════════════
 * 文字列検索
 * ═══════════════════════════════════════════════════════════════*/

/* index - 文字を検索 (strchrと同等) */
static inline char* index(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return (c == '\0') ? (char*)s : NULL;
}

/* rindex - 文字を後ろから検索 (strrchrと同等) */
static inline char* rindex(const char* s, int c) {
    const char* last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == '\0') return (char*)s;
    return (char*)last;
}

#ifdef __cplusplus
}
#endif
#endif /* !MIDL_PASS */

#endif /* _STRINGS_H */
