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
                                   int* explicit_period) {
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
        if (!_fnm_class_atom(&p, end, flags, &lo)) break;
        hi = lo;
        if (p < end && *p == '-' && p + 1 < end) {
            const char* candidate = p + 1;
            unsigned char range_end = 0;
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
                                     int flags, unsigned depth,
                                     int component_start, unsigned* budget) {
    const char* p = pattern;
    const char* s = string;

    if (depth > RIN_FNM_MAX_INPUT || !budget || *budget == 0u) {
        return FNM_NOSYS;
    }
    --*budget;
    while (*p) {
        unsigned char pc = (unsigned char)*p;
        if (pc == '*') {
            int result;
            do { ++p; } while (*p == '*');
            /* Try the empty span before consuming any input. */
            result = _fnm_match_bounded(p, s, flags, depth + 1u,
                                        component_start, budget);
            if (result == 0) return 0;
            if (result == FNM_NOSYS) return FNM_NOSYS;
            while (*s) {
                unsigned char c = (unsigned char)*s;
                if ((flags & FNM_PATHNAME) && c == '/') break;
                if ((flags & FNM_PERIOD) && component_start && c == '.') break;
                component_start = ((flags & FNM_PATHNAME) && c == '/');
                ++s;
                result = _fnm_match_bounded(p, s, flags, depth + 1u,
                                            component_start, budget);
                if (result == 0) return 0;
                if (result == FNM_NOSYS) return FNM_NOSYS;
            }
            return FNM_NOMATCH;
        }
        if (!*s) return FNM_NOMATCH;
        if (pc == '?') {
            if ((flags & FNM_PATHNAME) && *s == '/') return FNM_NOMATCH;
            if ((flags & FNM_PERIOD) && component_start && *s == '.') {
                return FNM_NOMATCH;
            }
            component_start = ((flags & FNM_PATHNAME) && *s == '/');
            ++p;
            ++s;
            continue;
        }
        if (pc == '[') {
            const char* after = p;
            int explicit_period = 0;
            int class_result = _fnm_class_match(p, (unsigned char)*s,
                                                flags, &after, &explicit_period);
            if (class_result == -1) {
                /* An unmatched '[' is an ordinary literal. */
                class_result = _fnm_chareq((unsigned char)'[',
                                           (unsigned char)*s, flags);
                after = p + 1;
            }
            if (!class_result) return FNM_NOMATCH;
            if ((flags & FNM_PERIOD) && component_start && *s == '.' &&
                !explicit_period) return FNM_NOMATCH;
            if ((flags & FNM_PATHNAME) && *s == '/') return FNM_NOMATCH;
            ++s;
            p = after;
            component_start = ((flags & FNM_PATHNAME) && s[-1] == '/');
            continue;
        }
        if (pc == '\\' && !(flags & FNM_NOESCAPE) && p[1]) {
            ++p;
            pc = (unsigned char)*p;
        }
        if ((flags & FNM_PERIOD) && component_start && *s == '.' && pc != '.') {
            return FNM_NOMATCH;
        }
        if ((flags & FNM_PATHNAME) && *s == '/' && pc != '/') return FNM_NOMATCH;
        if (!_fnm_chareq(pc, (unsigned char)*s, flags)) return FNM_NOMATCH;
        ++p;
        component_start = ((flags & FNM_PATHNAME) && *s == '/');
        ++s;
    }
    if (*s == '\0') return 0;
    if ((flags & FNM_LEADING_DIR) && *s == '/') return 0;
    return FNM_NOMATCH;
}

static inline int fnmatch(const char* pattern, const char* string, int flags) {
    unsigned length;
    if (!pattern || !string) return FNM_NOMATCH;
    if ((flags & ~RIN_FNM_KNOWN_FLAGS) != 0) return FNM_NOSYS;
    for (length = 0; pattern[length] != '\0'; ++length) {
        if (length == RIN_FNM_MAX_INPUT) return FNM_NOSYS;
    }
    for (length = 0; string[length] != '\0'; ++length) {
        if (length == RIN_FNM_MAX_INPUT) return FNM_NOSYS;
    }
    {
        unsigned budget = RIN_FNM_OPERATION_BUDGET;
        return _fnm_match_bounded(pattern, string, flags, 0u, 1, &budget);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* _FNMATCH_H */
