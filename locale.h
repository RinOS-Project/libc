/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - locale.h
 * ロケール関数 (カーネルlocale.c連携版)
 */

#ifndef _LOCALE_H
#define _LOCALE_H

#include "errno.h"
#include "stddef.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * ロケールカテゴリ
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _MSVCRT_COMPAT
#    define LC_CTYPE    0   /* 文字分類・変換 */
#    define LC_NUMERIC  1   /* 数値フォーマット */
#    define LC_TIME     2   /* 日時フォーマット */
#    define LC_COLLATE  3   /* 文字列照合 */
#    define LC_MONETARY 4   /* 通貨フォーマット */
#    define LC_MESSAGES 5   /* メッセージ */
#    define LC_ALL      6   /* すべてのカテゴリ */

#    define LC_CTYPE_MASK    (1 << LC_CTYPE)
#    define LC_NUMERIC_MASK  (1 << LC_NUMERIC)
#    define LC_TIME_MASK     (1 << LC_TIME)
#    define LC_COLLATE_MASK  (1 << LC_COLLATE)
#    define LC_MONETARY_MASK (1 << LC_MONETARY)
#    define LC_MESSAGES_MASK (1 << LC_MESSAGES)
#    define LC_ALL_MASK      0x3F
#endif

/* ═══════════════════════════════════════════════════════════════
 * lconv構造体 - 数値・通貨フォーマット情報
 * ═══════════════════════════════════════════════════════════════*/

struct lconv {
    /* 数値フォーマット (LC_NUMERIC) */
    char* decimal_point;     /* 小数点 */
    char* thousands_sep;     /* 千の位区切り */
    char* grouping;          /* グループ化 */

    /* 通貨フォーマット (LC_MONETARY) */
    char* int_curr_symbol;   /* 国際通貨記号 */
    char* currency_symbol;   /* ローカル通貨記号 */
    char* mon_decimal_point; /* 通貨小数点 */
    char* mon_thousands_sep; /* 通貨千の位区切り */
    char* mon_grouping;      /* 通貨グループ化 */
    char* positive_sign;     /* 正の符号 */
    char* negative_sign;     /* 負の符号 */
    char  int_frac_digits;   /* 国際小数桁数 */
    char  frac_digits;       /* ローカル小数桁数 */
    char  p_cs_precedes;     /* 正: 通貨記号が先 */
    char  p_sep_by_space;    /* 正: 空白で分離 */
    char  n_cs_precedes;     /* 負: 通貨記号が先 */
    char  n_sep_by_space;    /* 負: 空白で分離 */
    char  p_sign_posn;       /* 正の符号位置 */
    char  n_sign_posn;       /* 負の符号位置 */
    char  int_p_cs_precedes;
    char  int_p_sep_by_space;
    char  int_n_cs_precedes;
    char  int_n_sep_by_space;
    char  int_p_sign_posn;
    char  int_n_sign_posn;
};

/* ═══════════════════════════════════════════════════════════════
 * カーネルlocale.c API宣言
 * ═══════════════════════════════════════════════════════════════*/

#ifndef MIDL_PASS
/* 基本ロケール関数 */
char* rin_setlocale(int category, const char* locale);
struct lconv* rin_localeconv(void);

/* ロケール情報取得 */
const char* rin_locale_name(int category);
const char* rin_locale_environment_name(int category);
const char* rin_locale_language(void);
const char* rin_locale_territory(void);
const char* rin_locale_codeset(void);
int rin_locale_is_japanese(void);
int rin_locale_is_utf8(void);
const char* rin_locale_weekday(int weekday, int abbreviated);
const char* rin_locale_month(int month, int abbreviated);
const char* rin_locale_am_pm(int hour);
const char* rin_locale_time_format(char conversion);

/* 文字分類関数 (ロケール対応) */
int rin_isalnum(int c);
int rin_isalpha(int c);
int rin_isblank(int c);
int rin_iscntrl(int c);
int rin_isdigit(int c);
int rin_isgraph(int c);
int rin_islower(int c);
int rin_isprint(int c);
int rin_ispunct(int c);
int rin_isspace(int c);
int rin_isupper(int c);
int rin_isxdigit(int c);

/* 大文字小文字変換 */
int rin_tolower(int c);
int rin_toupper(int c);

#endif /* !MIDL_PASS */

/* 文字列照合 (ロケール対応) - 宣言は string.h */

/* ═══════════════════════════════════════════════════════════════
 * 標準ロケール関数 (カーネルAPIラッパー)
 * ═══════════════════════════════════════════════════════════════*/

#ifndef MIDL_PASS
#ifndef _MSVCRT_COMPAT
/* setlocale - ロケールを設定
 * 対応ロケール: "C", "POSIX", "ja_JP.UTF-8", "ja_JP", "en_US.UTF-8", "en_US"
 */
static inline char* setlocale(int category, const char* locale) {
    return rin_setlocale(category, locale);
}

/* localeconv - ロケールのフォーマット情報を取得 */
static inline struct lconv* localeconv(void) {
    return rin_localeconv();
}
#endif

/* strcoll と strxfrm は string.h で定義 (C standard) */

/* ═══════════════════════════════════════════════════════════════
 * POSIX拡張 (locale_t)
 * ═══════════════════════════════════════════════════════════════*/

struct RinLocaleObject;
typedef struct RinLocaleObject* locale_t;

#define LC_GLOBAL_LOCALE ((locale_t)-1)

locale_t newlocale(int category_mask, const char* locale, locale_t base);
locale_t duplocale(locale_t locobj);
void freelocale(locale_t locobj);
locale_t uselocale(locale_t newloc);

#define RIN_LOCALE_CTYPE_ALNUM 1u
#define RIN_LOCALE_CTYPE_ALPHA 2u
#define RIN_LOCALE_CTYPE_DIGIT 3u
#define RIN_LOCALE_CTYPE_XDIGIT 4u
#define RIN_LOCALE_CTYPE_LOWER 5u
#define RIN_LOCALE_CTYPE_UPPER 6u
#define RIN_LOCALE_CTYPE_SPACE 7u
#define RIN_LOCALE_CTYPE_BLANK 8u
#define RIN_LOCALE_CTYPE_CNTRL 9u
#define RIN_LOCALE_CTYPE_PRINT 10u
#define RIN_LOCALE_CTYPE_GRAPH 11u
#define RIN_LOCALE_CTYPE_PUNCT 12u
#define RIN_LOCALE_CTYPE_TOLOWER 13u
#define RIN_LOCALE_CTYPE_TOUPPER 14u

int rin_locale_ctype_test(locale_t locobj, int character,
                          uint32_t operation);
int rin_locale_current_ctype_test(int character, uint32_t operation);
int rin_locale_ctype_map(locale_t locobj, int character,
                         uint32_t operation);
int rin_locale_current_ctype_map(int character, uint32_t operation);

/* Locale-owned LC_TIME data for the *_l time parsing/formatting entrypoints.
 * The returned strings are immutable and remain valid for the lifetime of
 * the locale object.  LC_GLOBAL_LOCALE selects the process locale. */
const char* rin_locale_l_weekday(locale_t locobj, int weekday, int abbreviated);
const char* rin_locale_l_month(locale_t locobj, int month, int abbreviated);
const char* rin_locale_l_am_pm(locale_t locobj, int hour);
const char* rin_locale_l_time_format(locale_t locobj, char conversion);

/* ═══════════════════════════════════════════════════════════════
 * RinOS拡張関数
 * ═══════════════════════════════════════════════════════════════*/

/* 現在のロケール名取得 */
static inline const char* locale_name(int category) {
    return rin_locale_name(category);
}

/* 言語コード取得 (e.g., "ja") */
static inline const char* locale_language(void) {
    return rin_locale_language();
}

/* 地域コード取得 (e.g., "JP") */
static inline const char* locale_territory(void) {
    return rin_locale_territory();
}

/* 文字コード取得 (e.g., "UTF-8") */
static inline const char* locale_codeset(void) {
    return rin_locale_codeset();
}

/* ロケールが日本語かどうか */
static inline int locale_is_japanese(void) {
    return rin_locale_is_japanese();
}

/* ロケールがUTF-8かどうか */
static inline int locale_is_utf8(void) {
    return rin_locale_is_utf8();
}
#endif /* !MIDL_PASS */

#ifdef __cplusplus
}
#endif

#endif /* _LOCALE_H */
