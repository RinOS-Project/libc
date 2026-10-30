/*
 * RinOS libc - fnmatch.h
 * パターンマッチング
 */

#ifndef _FNMATCH_H
#define _FNMATCH_H

#include "stddef.h"
#include "stdlib.h"
#include "wchar.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * フラグ
 * ═══════════════════════════════════════════════════════════════*/

#define FNM_NOMATCH     1   /* マッチしない */
#define FNM_NOSYS       2   /* 未サポート */

#define FNM_NOESCAPE    0x01  /* バックスラッシュをエスケープとして扱わない */
#define FNM_PATHNAME    0x02  /* スラッシュを特別扱い */
#define FNM_PERIOD      0x04  /* ピリオドを特別扱い */
#define FNM_LEADING_DIR 0x08  /* 先頭ディレクトリのみマッチ */
#define FNM_CASEFOLD    0x10  /* 大文字小文字を区別しない */

/* 非標準 */
#define FNM_FILE_NAME   FNM_PATHNAME

/* ═══════════════════════════════════════════════════════════════
 * fnmatch関数
 * ═══════════════════════════════════════════════════════════════*/

#define RIN_FNM_MAX_INPUT 4096u
#define RIN_FNM_OPERATION_BUDGET 1048576u
#define RIN_FNM_KNOWN_FLAGS \
    (FNM_NOESCAPE | FNM_PATHNAME | FNM_PERIOD | FNM_LEADING_DIR | FNM_CASEFOLD)

static inline int _fnm_tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static inline int _fnm_chareq(unsigned char a, unsigned char b, int flags) {
    if (flags & FNM_CASEFOLD) {
        return _fnm_tolower(a) == _fnm_tolower(b);
    }
    return a == b;
}

static inline int _fnm_budget_take(unsigned* budget) {
    if (!budget || *budget == 0u) return 0;
    --*budget;
    return 1;
}

/* Read one bracket-expression atom without walking past its closing ']'. */
static inline int _fnm_class_atom(const char** cursor, const char* end,
                                  int flags, unsigned char* value) {
    const char* p = *cursor;
    if (p >= end) return 0;
    if (*p == '\\' && !(flags & FNM_NOESCAPE) && p + 1 < end) ++p;
    *value = (unsigned char)*p++;
    *cursor = p;
    return 1;
}

/* Return 1/0 for a class match, or -1 for an unterminated class. */
static inline int _fnm_class_match(const char* pattern, unsigned char input,
                                   int flags, const char** after,
                                   int* explicit_period,
                                   unsigned* budget) {
    const char* p = pattern + 1;
    const char* end = p;
    int invert = 0;
    int saw = 0;
    int matched = 0;

    if (explicit_period) *explicit_period = 0;

    if (*end == '!' || *end == '^') {
        invert = 1;
        ++end;
    }
    /* POSIX permits ']' as the first class character. */
    if (*end == ']') {
        ++end;
        saw = 1;
    }
    while (*end) {
        if (!_fnm_budget_take(budget)) return -2;
        if (*end == ']' && saw) break;
        if (*end == '\\' && !(flags & FNM_NOESCAPE) && end[1]) ++end;
        ++end;
        saw = 1;
    }
    if (!*end) return -1;

    p = pattern + 1;
    if (*p == '!' || *p == '^') ++p;
    if (*p == ']') {
        if (_fnm_chareq((unsigned char)']', input, flags)) matched = 1;
        ++p;
    }
    while (p < end) {
        unsigned char lo = 0;
        unsigned char hi = 0;
        if (!_fnm_budget_take(budget)) return -2;
        if (!_fnm_class_atom(&p, end, flags, &lo)) break;
        hi = lo;
        if (p < end && *p == '-' && p + 1 < end) {
            const char* candidate = p + 1;
            unsigned char range_end = 0;
            if (!_fnm_budget_take(budget)) return -2;
            if (_fnm_class_atom(&candidate, end, flags, &range_end)) {
                hi = range_end;
                p = candidate;
            }
        }
        if ((flags & FNM_CASEFOLD) != 0) {
            lo = (unsigned char)_fnm_tolower(lo);
            hi = (unsigned char)_fnm_tolower(hi);
        }
        if (explicit_period && lo <= '.' && '.' <= hi) {
            *explicit_period = 1;
        }
        {
            unsigned char c = input;
            if (flags & FNM_CASEFOLD) c = (unsigned char)_fnm_tolower(c);
            if (lo <= hi && c >= lo && c <= hi) matched = 1;
        }
    }
    *after = end + 1;
    return invert ? !matched : matched;
}

static inline int _fnm_match_bounded(const char* pattern, const char* string,
                                     int flags, unsigned* budget) {
    const char* p = pattern;
    const char* s = string;
    const char* star_pattern = NULL;
    const char* star_retry = NULL;
    int component_start = 1;

    for (;;) {
        const char* next_pattern = p;
        unsigned char pattern_byte = (unsigned char)*p;
        unsigned char string_byte = (unsigned char)*s;
        int atom_matches = 0;

        if (!_fnm_budget_take(budget)) return FNM_NOSYS;
        if (pattern_byte == '*') {
            do {
                if (!_fnm_budget_take(budget)) return FNM_NOSYS;
                ++p;
            } while (*p == '*');
            star_pattern = p;
            star_retry = s;
            continue;
        }

        if (pattern_byte == '\0') {
            if (*s == '\0' ||
                ((flags & FNM_LEADING_DIR) != 0 && *s == '/'))
                return 0;
        } else if (*s != '\0') {
            if (pattern_byte == '?') {
                atom_matches = !((flags & FNM_PATHNAME) && *s == '/') &&
                    !((flags & FNM_PERIOD) && component_start && *s == '.');
                next_pattern = p + 1;
            } else if (pattern_byte == '[') {
                int explicit_period = 0;
                int class_result = _fnm_class_match(
                    p, string_byte, flags, &next_pattern, &explicit_period,
                    budget);
                if (class_result == -2) return FNM_NOSYS;
                if (class_result == -1) {
                    /* An unmatched '[' is an ordinary literal. */
                    atom_matches = _fnm_chareq(
                        (unsigned char)'[', string_byte, flags);
                    next_pattern = p + 1;
                } else {
                    atom_matches = class_result != 0 &&
                        !((flags & FNM_PATHNAME) && *s == '/') &&
                        !((flags & FNM_PERIOD) && component_start &&
                          *s == '.' && !explicit_period);
                }
            } else {
                unsigned char literal = pattern_byte;
                if (literal == '\\' && !(flags & FNM_NOESCAPE) && p[1]) {
                    if (!_fnm_budget_take(budget)) return FNM_NOSYS;
                    ++next_pattern;
                    literal = (unsigned char)*next_pattern;
                }
                atom_matches =
                    !((flags & FNM_PERIOD) && component_start &&
                      *s == '.' && literal != '.') &&
                    !((flags & FNM_PATHNAME) && *s == '/' && literal != '/') &&
                    _fnm_chareq(literal, string_byte, flags);
                ++next_pattern;
            }

            if (atom_matches) {
                p = next_pattern;
                component_start =
                    (flags & FNM_PATHNAME) != 0 && *s == '/';
                ++s;
                continue;
            }
        }

        if (!star_pattern || !star_retry || *star_retry == '\0' ||
            ((flags & FNM_PATHNAME) && *star_retry == '/') ||
            ((flags & FNM_PERIOD) && *star_retry == '.' &&
             (star_retry == string ||
              ((flags & FNM_PATHNAME) && star_retry > string &&
               star_retry[-1] == '/'))))
            return FNM_NOMATCH;
        if (!_fnm_budget_take(budget)) return FNM_NOSYS;
        ++star_retry;
        s = star_retry;
        p = star_pattern;
        component_start = 0;
    }
}

typedef struct _FnmClassElement {
    uint32_t kind;
    wctype_t property;
    size_t count;
    wchar_t characters[32];
} _FnmClassElement;

#define _FNM_ELEMENT_CHARACTER 1u
#define _FNM_ELEMENT_PROPERTY  2u
#define _FNM_ELEMENT_COLLATE   3u
#define _FNM_ELEMENT_EQUIV     4u

/* 呼出スレッドのLC_CTYPEに従い、1文字を正確にデコードする。 */
static inline int _fnm_decode_one(const char** cursor, const char* end,
                                  wchar_t* character, size_t* width,
                                  unsigned* budget) {
    mbstate_t state = {0};
    size_t remaining;
    size_t consumed;
    size_t encoded_width;
    if (!cursor || !*cursor || *cursor >= end || !character || !width)
        return 0;
    if (!_fnm_budget_take(budget)) return -2;
    remaining = (size_t)(end - *cursor);
    consumed = mbrtowc(character, *cursor, remaining, &state);
    if (consumed == (size_t)-1 || consumed == (size_t)-2 || consumed == 0u ||
        consumed > remaining)
        return -1;
    encoded_width = consumed;
    for (size_t index = 1u; index < encoded_width; ++index) {
        if (!_fnm_budget_take(budget)) return -2;
    }
    *width = encoded_width;
    *cursor += encoded_width;
    return 1;
}

static inline int _fnm_pair_close(const char* start, const char* end,
                                 char marker, const char** close,
                                 unsigned* budget) {
    const char* cursor = start;
    while (cursor < end) {
        if (!_fnm_budget_take(budget)) return -2;
        if (*cursor == marker && cursor + 1 < end && cursor[1] == ']') {
            *close = cursor;
            return 1;
        }
        ++cursor;
    }
    return 0;
}

static inline int _fnm_parse_class_element(const char** cursor,
                                           const char* end, int flags,
                                           _FnmClassElement* element,
                                           unsigned* budget) {
    const char* p = *cursor;
    if (!element || p >= end) return 0;
    element->kind = _FNM_ELEMENT_CHARACTER;
    element->property = (wctype_t)0u;
    element->count = 0u;
    if (p + 1 < end && p[0] == '[' &&
        (p[1] == ':' || p[1] == '.' || p[1] == '=')) {
        const char* close = NULL;
        int found = _fnm_pair_close(p + 2, end, p[1], &close, budget);
        if (found <= 0) return found == -2 ? -2 : -1;
        if (p[1] == ':') {
            char property_name[32];
            size_t length = (size_t)(close - (p + 2));
            if (length == 0u || length >= sizeof(property_name)) return -3;
            for (size_t index = 0u; index < length; ++index) {
                unsigned char value = (unsigned char)p[2u + index];
                if (!_fnm_budget_take(budget)) return -2;
                if (value < 0x21u || value > 0x7eu) return -3;
                property_name[index] = (char)value;
            }
            property_name[length] = '\0';
            element->property = wctype(property_name);
            if (element->property == (wctype_t)0u) return -3;
            element->kind = _FNM_ELEMENT_PROPERTY;
            *cursor = close + 2;
            return 1;
        }
        element->kind = p[1] == '.' ? _FNM_ELEMENT_COLLATE
                                     : _FNM_ELEMENT_EQUIV;
        {
            const char* value_cursor = p + 2;
            while (value_cursor < close) {
                size_t width = 0u;
                int decoded;
                if (element->count == sizeof(element->characters) /
                                         sizeof(element->characters[0]))
                    return -3;
                decoded = _fnm_decode_one(&value_cursor, close,
                    &element->characters[element->count], &width, budget);
                if (decoded <= 0 || width == 0u)
                    return decoded == -2 ? -2 : -3;
                ++element->count;
            }
            if (element->count == 0u) return -3;
        }
        *cursor = close + 2;
        return 1;
    }
    if (*p == '\\' && !(flags & FNM_NOESCAPE) && p + 1 < end) ++p;
    {
        size_t width = 0u;
        int decoded = _fnm_decode_one(&p, end, &element->characters[0],
                                      &width, budget);
        if (decoded <= 0) return decoded == -2 ? -2 : -1;
    }
    element->count = 1u;
    *cursor = p;
    return 1;
}

static inline wchar_t _fnm_fold_character(wchar_t character, int flags) {
    return (flags & FNM_CASEFOLD) ? (wchar_t)towlower((wint_t)character)
                                  : character;
}

static inline int _fnm_collate_sequence_order(
    const wchar_t* left, size_t left_count, const wchar_t* right,
    size_t right_count, int flags) {
    wchar_t left_text[33];
    wchar_t right_text[33];
    if (!left || !right || left_count == 0u || right_count == 0u ||
        left_count > 32u || right_count > 32u)
        return 0;
    for (size_t index = 0u; index < left_count; ++index)
        left_text[index] = _fnm_fold_character(left[index], flags);
    for (size_t index = 0u; index < right_count; ++index)
        right_text[index] = _fnm_fold_character(right[index], flags);
    left_text[left_count] = L'\0';
    right_text[right_count] = L'\0';
    return wcscoll(left_text, right_text);
}

static inline int _fnm_sequence_collates_equal(
    const _FnmClassElement* element, const char* string,
    const char* string_end, size_t* matched_bytes, int flags,
    unsigned* budget) {
    wchar_t candidate[33] = {0};
    wchar_t expected[33] = {0};
    const char* cursor = string;
    size_t width = 0u;
    size_t candidate_count = 0u;
    if (!element || element->count == 0u || element->count > 32u)
        return -1;
    while (cursor < string_end) {
        size_t character_width = 0u;
        int decoded = _fnm_decode_one(&cursor, string_end,
                                      &candidate[candidate_count],
                                      &character_width,
                                      budget);
        if (decoded == -2) return -2;
        if (decoded <= 0) return 0;
        if (character_width > (size_t)-1 - width) return -1;
        width += character_width;
        ++candidate_count;
        if (candidate_count == 32u && cursor < string_end) return 0;
    }
    if (candidate_count == 0u) return 0;
    for (size_t index = 0u; index < element->count; ++index)
        expected[index] = element->characters[index];
    for (size_t index = 0u; index < candidate_count; ++index)
        candidate[index] = _fnm_fold_character(candidate[index], flags);
    for (size_t index = 0u; index < element->count; ++index)
        expected[index] = _fnm_fold_character(expected[index], flags);
    if (wcscoll(candidate, expected) != 0) return 0;
    *matched_bytes = width;
    return 1;
}

static inline int _fnm_decode_class_sequence(
    const char* start, const char* end, wchar_t characters[33],
    size_t* count, unsigned* budget) {
    const char* cursor = start;
    *count = 0u;
    while (cursor < end) {
        size_t width = 0u;
        int decoded;
        if (*count == 32u) return 0;
        decoded = _fnm_decode_one(&cursor, end, &characters[*count], &width,
                                  budget);
        if (decoded == -2) return -2;
        if (decoded <= 0 || width == 0u) return -1;
        ++*count;
    }
    return *count == 0u ? 0 : 1;
}

/* POSIX bracket expressionを照合する。戻り値-1は終端のないclass、
 * -2は処理予算超過、-3は未対応classまたは照合列上限を表す。 */
static inline int _fnm_class_match_multibyte(
    const char* pattern, const char* pattern_end, const char* string,
    const char* string_end, wchar_t input, size_t input_width, int flags,
    const char** after, size_t* matched_width, int* explicit_period,
    unsigned* budget) {
    const char* p = pattern + 1;
    int invert = 0;
    int saw = 0;
    int matched = 0;
    size_t best_width = input_width;
    if (explicit_period) *explicit_period = 0;
    if (p < pattern_end && (*p == '!' || *p == '^')) {
        invert = 1;
        ++p;
    }
    while (p < pattern_end) {
        _FnmClassElement left = {0};
        _FnmClassElement right = {0};
        int parsed;
        int element_matches = 0;
        int dot_explicit = 0;
        size_t element_width = input_width;
        const char* element_end = p;
        int first = !saw;
        if (!_fnm_budget_take(budget)) return -2;
        if (*p == ']' && saw) {
            *after = p + 1;
            *matched_width = best_width;
            return invert ? !matched : matched;
        }
        if (*p == ']' && !saw) {
            left.kind = _FNM_ELEMENT_CHARACTER;
            left.count = 1u;
            left.characters[0] = L']';
            element_end = p + 1;
            parsed = 1;
        } else {
            parsed = _fnm_parse_class_element(&element_end, pattern_end,
                                              flags, &left, budget);
        }
        if (parsed == -2) return -2;
        if (parsed == -3) return -3;
        if (parsed <= 0) return -1;

        if ((left.kind == _FNM_ELEMENT_CHARACTER ||
             left.kind == _FNM_ELEMENT_COLLATE) && !first &&
            element_end < pattern_end && *element_end == '-' &&
            element_end + 1 < pattern_end && element_end[1] != ']') {
            const char* right_end = element_end + 1;
            parsed = _fnm_parse_class_element(&right_end, pattern_end, flags,
                                              &right, budget);
            if (parsed == -2) return -2;
            if (parsed == -3) return -3;
            if (parsed > 0 &&
                (right.kind == _FNM_ELEMENT_CHARACTER ||
                 right.kind == _FNM_ELEMENT_COLLATE)) {
                wchar_t candidate[33];
                wchar_t dot[1] = {L'.'};
                size_t candidate_count = 0u;
                int candidate_result = _fnm_decode_class_sequence(
                    string, string_end, candidate, &candidate_count, budget);
                if (candidate_result == -2) return -2;
                if (candidate_result < 0) return -1;
                if (candidate_result > 0 &&
                    _fnm_collate_sequence_order(
                        left.characters, left.count, right.characters,
                        right.count, flags) <= 0 &&
                    _fnm_collate_sequence_order(
                        left.characters, left.count, candidate,
                        candidate_count, flags) <= 0 &&
                    _fnm_collate_sequence_order(
                        candidate, candidate_count, right.characters,
                        right.count, flags) <= 0) {
                    element_matches = 1;
                    element_width = (size_t)(string_end - string);
                }
                if (_fnm_collate_sequence_order(
                        left.characters, left.count, right.characters,
                        right.count, flags) <= 0 &&
                    _fnm_collate_sequence_order(
                        left.characters, left.count, dot, 1u, flags) <= 0 &&
                    _fnm_collate_sequence_order(
                        dot, 1u, right.characters, right.count, flags) <= 0)
                    dot_explicit = 1;
                element_end = right_end;
            }
        }

        if (!element_matches) {
            if (left.kind == _FNM_ELEMENT_CHARACTER) {
                element_matches = _fnm_fold_character(left.characters[0],
                    flags) == _fnm_fold_character(input, flags);
                dot_explicit = left.characters[0] == L'.';
            } else if (left.kind == _FNM_ELEMENT_PROPERTY) {
                element_matches = iswctype((wint_t)input, left.property) != 0;
            } else if (left.kind == _FNM_ELEMENT_COLLATE ||
                       left.kind == _FNM_ELEMENT_EQUIV) {
                int comparison = _fnm_sequence_collates_equal(
                    &left, string, string_end, &element_width, flags, budget);
                if (comparison == -2) return -2;
                if (comparison < 0) return -3;
                element_matches = comparison != 0;
                dot_explicit = left.count == 1u &&
                               left.characters[0] == L'.';
            }
        }
        if (element_matches) {
            matched = 1;
            if (element_width > best_width) best_width = element_width;
        }
        if (dot_explicit && explicit_period && !invert)
            *explicit_period = 1;
        p = element_end;
        saw = 1;
    }
    return -1;
}

static inline int _fnm_state_test(const unsigned char* states,
                                  size_t stride, size_t pattern_offset,
                                  size_t string_offset) {
    size_t bit = pattern_offset * stride + string_offset;
    return (states[bit >> 3u] & (unsigned char)(1u << (bit & 7u))) != 0u;
}

static inline void _fnm_state_set(unsigned char* states, size_t stride,
                                  size_t pattern_offset,
                                  size_t string_offset) {
    size_t bit = pattern_offset * stride + string_offset;
    states[bit >> 3u] |= (unsigned char)(1u << (bit & 7u));
}

/* A bitset DP keeps wildcard, pathname and multicharacter bracket
 * transitions bounded without recursive backtracking or a large C stack. */
static inline int _fnm_match_multibyte(const char* pattern,
                                       const char* pattern_end,
                                       const char* string,
                                       const char* string_end, int flags,
                                       unsigned* budget) {
    size_t pattern_length = (size_t)(pattern_end - pattern);
    size_t string_length = (size_t)(string_end - string);
    size_t stride = string_length + 1u;
    size_t state_count = (pattern_length + 1u) * stride;
    size_t state_bytes = (state_count + 7u) / 8u;
    unsigned char* states = (unsigned char*)calloc(state_bytes, 1u);
    int result = FNM_NOMATCH;
    if (!states) return FNM_NOSYS;
    _fnm_state_set(states, stride, 0u, 0u);

    for (size_t string_offset = 0u; string_offset <= string_length;
         ++string_offset) {
        int component_start = string_offset == 0u ||
            ((flags & FNM_PATHNAME) != 0 &&
             string[string_offset - 1u] == '/');
        for (size_t pattern_offset = 0u;
             pattern_offset <= pattern_length; ++pattern_offset) {
            const char* input_cursor;
            wchar_t input = 0;
            size_t input_width = 0u;
            int decoded;
            if (!_fnm_budget_take(budget)) {
                result = FNM_NOSYS;
                goto done;
            }
            if (!_fnm_state_test(states, stride, pattern_offset,
                                 string_offset))
                continue;
            if (pattern_offset == pattern_length) {
                if (string_offset == string_length ||
                    ((flags & FNM_LEADING_DIR) != 0 &&
                     string_offset < string_length &&
                     string[string_offset] == '/')) {
                    result = 0;
                    goto done;
                }
                continue;
            }
            if (pattern[pattern_offset] == '*') {
                _fnm_state_set(states, stride, pattern_offset + 1u,
                               string_offset);
                if (string_offset < string_length) {
                    input_cursor = string + string_offset;
                    decoded = _fnm_decode_one(&input_cursor, string_end,
                                              &input, &input_width, budget);
                    if (decoded == -2) {
                        result = FNM_NOSYS;
                        goto done;
                    }
                    if (decoded > 0 &&
                        !((flags & FNM_PATHNAME) && input == L'/') &&
                        !((flags & FNM_PERIOD) && component_start &&
                          input == L'.'))
                        _fnm_state_set(states, stride, pattern_offset,
                            (size_t)(input_cursor - string));
                }
                continue;
            }
            if (string_offset == string_length) continue;
            input_cursor = string + string_offset;
            decoded = _fnm_decode_one(&input_cursor, string_end, &input,
                                      &input_width, budget);
            if (decoded == -2) {
                result = FNM_NOSYS;
                goto done;
            }
            if (decoded <= 0) continue;

            if (pattern[pattern_offset] == '?') {
                if (!((flags & FNM_PATHNAME) && input == L'/') &&
                    !((flags & FNM_PERIOD) && component_start &&
                      input == L'.'))
                    _fnm_state_set(states, stride, pattern_offset + 1u,
                                   (size_t)(input_cursor - string));
            } else if (pattern[pattern_offset] == '[') {
                const char* first_candidate_end = input_cursor;
                const char* class_after = pattern + pattern_offset;
                size_t matched_width = input_width;
                int explicit_period = 0;
                int class_result = _fnm_class_match_multibyte(
                    pattern + pattern_offset, pattern_end, string,
                    first_candidate_end, input, input_width, flags,
                    &class_after, &matched_width, &explicit_period, budget);
                if (class_result == -2 || class_result == -3) {
                    result = FNM_NOSYS;
                    goto done;
                }
                if (class_result == -1) {
                    if (_fnm_fold_character(L'[', flags) ==
                            _fnm_fold_character(input, flags) &&
                        !((flags & FNM_PERIOD) && component_start &&
                          input == L'.') &&
                        !((flags & FNM_PATHNAME) && input == L'/'))
                        _fnm_state_set(states, stride, pattern_offset + 1u,
                                       (size_t)(input_cursor - string));
                    continue;
                }
                {
                    const char* candidate_end = first_candidate_end;
                    int first_candidate = 1;
                    while (candidate_end <= string_end) {
                        const char* candidate_after = class_after;
                        size_t candidate_width = input_width;
                        int candidate_explicit_period = 0;
                        int candidate_result = class_result;
                        int pathname_separator = 0;
                        if (!first_candidate) {
                            candidate_result = _fnm_class_match_multibyte(
                                pattern + pattern_offset, pattern_end, string,
                                candidate_end, input, input_width, flags,
                                &candidate_after, &candidate_width,
                                &candidate_explicit_period, budget);
                            if (candidate_result == -2 ||
                                candidate_result == -3) {
                                result = FNM_NOSYS;
                                goto done;
                            }
                        } else {
                            candidate_width = matched_width;
                            candidate_explicit_period = explicit_period;
                        }
                        if (candidate_result > 0 &&
                            candidate_after == class_after &&
                            candidate_width ==
                                (size_t)(candidate_end - (string + string_offset))) {
                            if (flags & FNM_PATHNAME) {
                                for (const char* check = string + string_offset;
                                     check < candidate_end; ++check) {
                                    if (!_fnm_budget_take(budget)) {
                                        result = FNM_NOSYS;
                                        goto done;
                                    }
                                    if (*check == '/') {
                                        pathname_separator = 1;
                                        break;
                                    }
                                }
                            }
                            if (!pathname_separator &&
                                !((flags & FNM_PERIOD) && component_start &&
                                  input == L'.' &&
                                  !candidate_explicit_period))
                                _fnm_state_set(states, stride,
                                    (size_t)(class_after - pattern),
                                    (size_t)(candidate_end - string));
                        }
                        if (candidate_end == string_end) break;
                        {
                            const char* next_candidate = candidate_end;
                            wchar_t ignored;
                            size_t ignored_width = 0u;
                            int next_decoded = _fnm_decode_one(
                                &next_candidate, string_end, &ignored,
                                &ignored_width, budget);
                            if (next_decoded == -2) {
                                result = FNM_NOSYS;
                                goto done;
                            }
                            if (next_decoded <= 0) break;
                            candidate_end = next_candidate;
                        }
                        first_candidate = 0;
                    }
                }
            } else {
                const char* literal_cursor = pattern + pattern_offset;
                wchar_t pattern_character = 0;
                size_t pattern_width = 0u;
                if (*literal_cursor == '\\' &&
                    !(flags & FNM_NOESCAPE) &&
                    literal_cursor + 1 < pattern_end)
                    ++literal_cursor;
                decoded = _fnm_decode_one(&literal_cursor, pattern_end,
                    &pattern_character, &pattern_width, budget);
                if (decoded == -2) {
                    result = FNM_NOSYS;
                    goto done;
                }
                if (decoded > 0 && pattern_width != 0u &&
                    !((flags & FNM_PERIOD) && component_start &&
                      input == L'.' && pattern_character != L'.') &&
                    !((flags & FNM_PATHNAME) && input == L'/' &&
                      pattern_character != L'/') &&
                    _fnm_fold_character(pattern_character, flags) ==
                    _fnm_fold_character(input, flags))
                    _fnm_state_set(states, stride,
                        (size_t)(literal_cursor - pattern),
                        (size_t)(input_cursor - string));
            }
        }
    }
done:
    free(states);
    return result;
}

static inline int fnmatch(const char* pattern, const char* string, int flags) {
    size_t pattern_length;
    size_t string_length;
    unsigned budget = RIN_FNM_OPERATION_BUDGET;
    if (!pattern || !string) return FNM_NOMATCH;
    if ((flags & ~RIN_FNM_KNOWN_FLAGS) != 0) return FNM_NOSYS;
    for (pattern_length = 0u; pattern[pattern_length] != '\0';
         ++pattern_length) {
        if (!_fnm_budget_take(&budget)) return FNM_NOSYS;
        if (pattern_length == RIN_FNM_MAX_INPUT) return FNM_NOSYS;
    }
    for (string_length = 0u; string[string_length] != '\0';
         ++string_length) {
        if (!_fnm_budget_take(&budget)) return FNM_NOSYS;
        if (string_length == RIN_FNM_MAX_INPUT) return FNM_NOSYS;
    }
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
    return _fnm_match_multibyte(pattern, pattern + pattern_length, string,
                                string + string_length, flags, &budget);
#else
    return _fnm_match_bounded(pattern, string, flags, &budget);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* _FNMATCH_H */
