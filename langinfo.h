/* SPDX-License-Identifier: MIT */
/* POSIX locale catalog query interface. */

#ifndef _RIN_LANGINFO_H
#define _RIN_LANGINFO_H

#include "errno.h"
#include "locale.h"

#ifndef _MSVCRT_COMPAT

typedef int nl_item;

#define CODESET 1
#define D_T_FMT 2
#define D_FMT 3
#define T_FMT 4
#define T_FMT_AMPM 5
#define AM_STR 6
#define PM_STR 7
#define DAY_1 8
#define DAY_2 9
#define DAY_3 10
#define DAY_4 11
#define DAY_5 12
#define DAY_6 13
#define DAY_7 14
#define ABDAY_1 15
#define ABDAY_2 16
#define ABDAY_3 17
#define ABDAY_4 18
#define ABDAY_5 19
#define ABDAY_6 20
#define ABDAY_7 21
#define MON_1 22
#define MON_2 23
#define MON_3 24
#define MON_4 25
#define MON_5 26
#define MON_6 27
#define MON_7 28
#define MON_8 29
#define MON_9 30
#define MON_10 31
#define MON_11 32
#define MON_12 33
#define ABMON_1 34
#define ABMON_2 35
#define ABMON_3 36
#define ABMON_4 37
#define ABMON_5 38
#define ABMON_6 39
#define ABMON_7 40
#define ABMON_8 41
#define ABMON_9 42
#define ABMON_10 43
#define ABMON_11 44
#define ABMON_12 45
#define ERA 46
#define ERA_D_FMT 47
#define ERA_D_T_FMT 48
#define ERA_T_FMT 49
#define ALT_DIGITS 50
#define RADIXCHAR 51
#define THOUSEP 52
#define YESEXPR 53
#define NOEXPR 54
#define YESSTR 55
#define NOSTR 56
#define CRNCYSTR 57

/* Catalog-owned strings are immutable. Synthesized CRNCYSTR storage is
 * thread-local and is replaced by the next CRNCYSTR query in this thread.
 * Invalid items/locales return an empty string and set EINVAL; valid catalog
 * items without product data return an empty string without an error. */
static inline char* rin_langinfo_for_locale(nl_item item, locale_t locale) {
    struct lconv* numeric = NULL;
    const char* value = NULL;

    switch (item) {
    case CODESET:
        value = rin_locale_l_codeset(locale);
        break;
    case D_T_FMT:
        value = rin_locale_l_time_format(locale, 'c');
        break;
    case D_FMT:
        value = rin_locale_l_time_format(locale, 'x');
        break;
    case T_FMT:
        value = rin_locale_l_time_format(locale, 'X');
        break;
    case T_FMT_AMPM:
        value = "";
        break;
    case AM_STR:
        value = rin_locale_l_am_pm(locale, 0);
        break;
    case PM_STR:
        value = rin_locale_l_am_pm(locale, 12);
        break;
    case DAY_1: value = rin_locale_l_weekday(locale, 0, 0); break;
    case DAY_2: value = rin_locale_l_weekday(locale, 1, 0); break;
    case DAY_3: value = rin_locale_l_weekday(locale, 2, 0); break;
    case DAY_4: value = rin_locale_l_weekday(locale, 3, 0); break;
    case DAY_5: value = rin_locale_l_weekday(locale, 4, 0); break;
    case DAY_6: value = rin_locale_l_weekday(locale, 5, 0); break;
    case DAY_7: value = rin_locale_l_weekday(locale, 6, 0); break;
    case ABDAY_1: value = rin_locale_l_weekday(locale, 0, 1); break;
    case ABDAY_2: value = rin_locale_l_weekday(locale, 1, 1); break;
    case ABDAY_3: value = rin_locale_l_weekday(locale, 2, 1); break;
    case ABDAY_4: value = rin_locale_l_weekday(locale, 3, 1); break;
    case ABDAY_5: value = rin_locale_l_weekday(locale, 4, 1); break;
    case ABDAY_6: value = rin_locale_l_weekday(locale, 5, 1); break;
    case ABDAY_7: value = rin_locale_l_weekday(locale, 6, 1); break;
    case MON_1: value = rin_locale_l_month(locale, 0, 0); break;
    case MON_2: value = rin_locale_l_month(locale, 1, 0); break;
    case MON_3: value = rin_locale_l_month(locale, 2, 0); break;
    case MON_4: value = rin_locale_l_month(locale, 3, 0); break;
    case MON_5: value = rin_locale_l_month(locale, 4, 0); break;
    case MON_6: value = rin_locale_l_month(locale, 5, 0); break;
    case MON_7: value = rin_locale_l_month(locale, 6, 0); break;
    case MON_8: value = rin_locale_l_month(locale, 7, 0); break;
    case MON_9: value = rin_locale_l_month(locale, 8, 0); break;
    case MON_10: value = rin_locale_l_month(locale, 9, 0); break;
    case MON_11: value = rin_locale_l_month(locale, 10, 0); break;
    case MON_12: value = rin_locale_l_month(locale, 11, 0); break;
    case ABMON_1: value = rin_locale_l_month(locale, 0, 1); break;
    case ABMON_2: value = rin_locale_l_month(locale, 1, 1); break;
    case ABMON_3: value = rin_locale_l_month(locale, 2, 1); break;
    case ABMON_4: value = rin_locale_l_month(locale, 3, 1); break;
    case ABMON_5: value = rin_locale_l_month(locale, 4, 1); break;
    case ABMON_6: value = rin_locale_l_month(locale, 5, 1); break;
    case ABMON_7: value = rin_locale_l_month(locale, 6, 1); break;
    case ABMON_8: value = rin_locale_l_month(locale, 7, 1); break;
    case ABMON_9: value = rin_locale_l_month(locale, 8, 1); break;
    case ABMON_10: value = rin_locale_l_month(locale, 9, 1); break;
    case ABMON_11: value = rin_locale_l_month(locale, 10, 1); break;
    case ABMON_12: value = rin_locale_l_month(locale, 11, 1); break;
    case RADIXCHAR:
    case THOUSEP:
    case CRNCYSTR:
        numeric = rin_localeconv_for_locale(locale);
        if (numeric) {
            if (item == RADIXCHAR) value = numeric->decimal_point;
            else if (item == THOUSEP) value = numeric->thousands_sep;
            else value = rin_locale_l_currency_string(locale);
        }
        break;
    case ERA:
    case ERA_D_FMT:
    case ERA_D_T_FMT:
    case ERA_T_FMT:
    case ALT_DIGITS:
    case YESEXPR:
    case NOEXPR:
    case YESSTR:
    case NOSTR:
        /* The current product catalog has no values for these items. */
        value = "";
        break;
    default:
        errno = EINVAL;
        return (char*)"";
    }

    if (!value) {
        errno = EINVAL;
        return (char*)"";
    }
    return (char*)value;
}

static inline char* nl_langinfo_l(nl_item item, locale_t locale) {
    return rin_langinfo_for_locale(item, locale);
}

static inline char* nl_langinfo(nl_item item) {
    return nl_langinfo_l(item, uselocale(NULL));
}

#endif /* !_MSVCRT_COMPAT */

#endif /* _RIN_LANGINFO_H */
