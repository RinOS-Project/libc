/*
 * RinOS libc - fnmatch.h
 * パターンマッチング
 */

#ifndef _FNMATCH_H
#define _FNMATCH_H

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

static inline int fnmatch(const char* pattern, const char* string, int flags) {
    size_t length;
    unsigned budget = RIN_FNM_OPERATION_BUDGET;
    if (!pattern || !string) return FNM_NOMATCH;
    if ((flags & ~RIN_FNM_KNOWN_FLAGS) != 0) return FNM_NOSYS;
    for (length = 0; pattern[length] != '\0'; ++length) {
        if (!_fnm_budget_take(&budget)) return FNM_NOSYS;
        if (length == RIN_FNM_MAX_INPUT) return FNM_NOSYS;
    }
    for (length = 0; string[length] != '\0'; ++length) {
        if (!_fnm_budget_take(&budget)) return FNM_NOSYS;
        if (length == RIN_FNM_MAX_INPUT) return FNM_NOSYS;
    }
    return _fnm_match_bounded(pattern, string, flags, &budget);
}

#ifdef __cplusplus
}
#endif

#endif /* _FNMATCH_H */
