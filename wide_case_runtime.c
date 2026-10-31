/* SPDX-License-Identifier: MIT */
/* POSIX wide-character case-insensitive comparison runtime. */

#define RIN_WCHAR_IMPLEMENTATION 1
#include "wchar.h"
#include "locale.h"

static wint_t rin_towlower_current_locale(wint_t character)
{
    if (character == WEOF) return WEOF;
    return (wint_t)rin_locale_current_wide_ctype_map(
        (uint32_t)character, RIN_LOCALE_CTYPE_TOLOWER);
}

static int rin_wcsncasecmp_current_locale(const wchar_t* left,
                                           const wchar_t* right,
                                           size_t count)
{
    size_t index;
    for (index = 0u; index < count; ++index) {
        const wchar_t left_value = left[index];
        const wchar_t right_value = right[index];
        const wint_t left_folded = rin_towlower_current_locale(
            (wint_t)left_value);
        const wint_t right_folded = rin_towlower_current_locale(
            (wint_t)right_value);
        if (left_folded != right_folded)
            return left_folded < right_folded ? -1 : 1;
        if (left_value == L'\0' || right_value == L'\0') return 0;
    }
    return 0;
}

int wcscasecmp(const wchar_t* left, const wchar_t* right)
{
    return rin_wcsncasecmp_current_locale(left, right, SIZE_MAX);
}

int wcsncasecmp(const wchar_t* left, const wchar_t* right, size_t count)
{
    return rin_wcsncasecmp_current_locale(left, right, count);
}
