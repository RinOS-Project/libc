/*
 * RinOS libc - glob.h
 * パス名パターン展開
 */

#ifndef _GLOB_H
#define _GLOB_H

#include "stddef.h"
#include "errno.h"
#include "stdlib.h"
#include "string.h"
#include "wchar.h"
#include "sys/stat.h"
#include "dirent.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * フラグ
 * ═══════════════════════════════════════════════════════════════*/

#define GLOB_ERR      0x0001  /* エラー時に停止 */
#define GLOB_MARK     0x0002  /* ディレクトリにスラッシュを追加 */
#define GLOB_NOSORT   0x0004  /* ソートしない */
#define GLOB_DOOFFS   0x0008  /* gl_offsを使用 */
#define GLOB_NOCHECK  0x0010  /* マッチなしでもパターンを返す */
#define GLOB_APPEND   0x0020  /* 結果を追加 */
#define GLOB_NOESCAPE 0x0040  /* エスケープを無効化 */
#define GLOB_PERIOD   0x0080  /* ドットファイルをマッチ */

/* GNU拡張 */
#define GLOB_BRACE    0x0100  /* ブレース展開 */
#define GLOB_NOMAGIC  0x0200  /* ワイルドカードなしでも展開 */
#define GLOB_TILDE    0x0400  /* チルダ展開 */
#define GLOB_ONLYDIR  0x1000  /* ディレクトリのみ */
#define GLOB_ALTDIRFUNC 0x2000 /* glob_tのfilesystem callbacksを使う */
#define GLOB_MAGCHAR   0x0800 /* glob_t.gl_flags出力専用 */

/* ═══════════════════════════════════════════════════════════════
 * エラーコード
 * ═══════════════════════════════════════════════════════════════*/

#define GLOB_NOSPACE  1  /* メモリ不足 */
#define GLOB_ABORTED  2  /* 読み取りエラー */
#define GLOB_NOMATCH  3  /* マッチなし */
#define GLOB_NOSYS    4  /* 未サポート */

#define _RIN_GLOB_MAX_PATTERN     4096u
#define _RIN_GLOB_MAX_COMPONENTS  64u
#define _RIN_GLOB_MAX_MATCHES     4096u
#define _RIN_GLOB_MAX_PATH        4096u
#define _RIN_GLOB_MAX_BRACE_RECURSION 8u
#define _RIN_GLOB_MAX_BRACE_NESTING 64u
#define _RIN_GLOB_MATCH_OPERATION_BUDGET 131072u
#define _RIN_GLOB_MATCH_BUDGET_EXHAUSTED (-3)

/* ═══════════════════════════════════════════════════════════════
 * glob_t構造体
 * ═══════════════════════════════════════════════════════════════*/

typedef struct {
    size_t gl_pathc;    /* マッチしたパス数 */
    char** gl_pathv;    /* パス一覧 */
    size_t gl_offs;     /* gl_pathvの先頭の空きスロット数 */
    int    gl_flags;    /* フラグ */

    /* GNU拡張 */
    void (*gl_closedir)(void*);
    struct dirent* (*gl_readdir)(void*);
    void* (*gl_opendir)(const char*);
    int (*gl_lstat)(const char*, struct stat*);
    int (*gl_stat)(const char*, struct stat*);
} glob_t;

static inline int glob(const char* pattern, int flags,
                       int (*errfunc)(const char*, int),
                       glob_t* pglob);

/* ═══════════════════════════════════════════════════════════════
 * glob関数
 * ═══════════════════════════════════════════════════════════════*/

static inline size_t _rin_glob_strlen(const char* text) {
    size_t size = 0;
    while (text && text[size]) ++size;
    return size;
}

#ifndef _RIN_GLOB_OPENDIR
#define _RIN_GLOB_OPENDIR opendir
#endif
#ifndef _RIN_GLOB_READDIR
#define _RIN_GLOB_READDIR readdir
#endif
#ifndef _RIN_GLOB_CLOSEDIR
#define _RIN_GLOB_CLOSEDIR closedir
#endif
#ifndef _RIN_GLOB_STAT
#define _RIN_GLOB_STAT stat
#endif
#ifndef _RIN_GLOB_GETENV
#define _RIN_GLOB_GETENV getenv
#endif

typedef struct _RinGlobMatchList {
    char** items;
    size_t count;
    size_t capacity;
} _RinGlobMatchList;

typedef struct _RinGlobContext {
    int flags;
    int require_directory;
    int (*errfunc)(const char*, int);
    glob_t* pglob;
    _RinGlobMatchList* matches;
} _RinGlobContext;

typedef struct _RinGlobComponent {
    const char* text;
    size_t length;
    int wildcard;
} _RinGlobComponent;

/* Recursive brace expansion owns one bounded scratch frame per active
 * expansion. Keeping these buffers on the heap makes GLOB_BRACE safe on the
 * minimum supported thread stack; recursion still has a fixed resource cap. */
typedef struct _RinGlobScratch {
    char prefix[_RIN_GLOB_MAX_PATH];
    _RinGlobComponent components[_RIN_GLOB_MAX_COMPONENTS];
    char component_copy[_RIN_GLOB_MAX_PATH];
    char alternative[_RIN_GLOB_MAX_PATTERN];
    char expanded[_RIN_GLOB_MAX_PATTERN];
    size_t brace_opens[_RIN_GLOB_MAX_BRACE_NESTING];
    size_t brace_commas[_RIN_GLOB_MAX_BRACE_NESTING];
} _RinGlobScratch;

typedef struct _RinGlobExpandScratch {
    char raw_component[NAME_MAX + 1u];
    char literal_component[NAME_MAX + 1u];
    char child[_RIN_GLOB_MAX_PATH];
} _RinGlobExpandScratch;

/* Forward declaration used by the private brace collector.  The public
 * entrypoint recursively compiles each bounded alternative into a private
 * result before publishing anything to the caller's glob_t. */
static inline int glob(const char* pattern, int flags,
                       int (*errfunc)(const char*, int),
                       glob_t* pglob);
static inline void globfree(glob_t* pglob);
static inline int _rin_glob_run(const char* pattern, int flags,
                                int (*errfunc)(const char*, int),
                                glob_t* pglob, unsigned int brace_depth);

static inline void _rin_glob_match_list_free(_RinGlobMatchList* list)
{
    if (!list) return;
    if (list->items) {
        for (size_t index = 0; index < list->count; ++index)
            free(list->items[index]);
        free(list->items);
    }
    list->items = NULL;
    list->count = 0u;
    list->capacity = 0u;
}

static inline int _rin_glob_match_list_add(_RinGlobMatchList* list,
                                           const char* path)
{
    size_t length;
    char* copy;
    char** replacement;
    size_t capacity;
    if (!list || !path) return GLOB_ABORTED;
    if (list->count >= _RIN_GLOB_MAX_MATCHES) return GLOB_NOSPACE;
    length = _rin_glob_strlen(path);
    if (length > _RIN_GLOB_MAX_PATH - 1u) return GLOB_NOSPACE;
    copy = (char*)malloc(length + 1u);
    if (!copy) return GLOB_NOSPACE;
    for (size_t index = 0; index <= length; ++index) copy[index] = path[index];
    if (list->count == list->capacity) {
        capacity = list->capacity == 0u ? 16u : list->capacity * 2u;
        if (capacity < list->capacity || capacity > _RIN_GLOB_MAX_MATCHES)
            capacity = _RIN_GLOB_MAX_MATCHES;
        if (capacity > (size_t)-1 / sizeof(char*)) {
            free(copy);
            return GLOB_NOSPACE;
        }
        replacement = (char**)malloc(capacity * sizeof(char*));
        if (!replacement) {
            free(copy);
            return GLOB_NOSPACE;
        }
        for (size_t index = 0; index < list->count; ++index)
            replacement[index] = list->items[index];
        if (list->items) free(list->items);
        list->items = replacement;
        list->capacity = capacity;
    }
    list->items[list->count++] = copy;
    return 0;
}

static inline int _rin_glob_component_wild(const char* pattern, int flags)
{
    if (!pattern) return 0;
    for (size_t index = 0; pattern[index] != '\0'; ++index) {
        if (pattern[index] == '\\' && !(flags & GLOB_NOESCAPE)) {
            if (pattern[index + 1u] == '\0') return -1;
            ++index;
            continue;
        }
        if (pattern[index] == '*' || pattern[index] == '?' ||
            pattern[index] == '[') return 1;
    }
    return 0;
}

static inline int _rin_glob_component_copy(const char* pattern,
                                           size_t length, int flags,
                                           char* output, size_t capacity)
{
    size_t cursor = 0u;
    if (!pattern || !output || capacity == 0u) return GLOB_ABORTED;
    for (size_t index = 0; index < length; ++index) {
        char value = pattern[index];
        if (value == '\\' && !(flags & GLOB_NOESCAPE)) {
            if (++index >= length) return GLOB_NOSYS;
            value = pattern[index];
        }
        if (cursor + 1u >= capacity) return GLOB_NOSPACE;
        output[cursor++] = value;
    }
    output[cursor] = '\0';
    return 0;
}

static inline int _rin_glob_decode_character(const char* input,
                                             const char** next,
                                             char encoded[5],
                                             wchar_t* value)
{
    mbstate_t state = {0};
    size_t available = 0u;
    size_t consumed;
    if (!input || !next || !encoded || !value || *input == '\0') return -1;
    while (available < 4u && input[available] != '\0') ++available;
    consumed = mbrtowc(value, input, available, &state);
    if (consumed == (size_t)-1 || consumed == (size_t)-2 || consumed > 4u)
        return -1;
    if (consumed == 0u) consumed = 1u;
    for (size_t index = 0u; index < consumed; ++index)
        encoded[index] = input[index];
    encoded[consumed] = '\0';
    *next = input + consumed;
    return 0;
}

static inline int _rin_glob_wide_class(const char* name, size_t length,
                                       wchar_t value)
{
    char property[16];
    wctype_t descriptor;
    if (length == 0u || length >= sizeof(property)) return -1;
    for (size_t index = 0u; index < length; ++index)
        property[index] = name[index];
    property[length] = '\0';
    descriptor = wctype(property);
    if (descriptor == (wctype_t)0u) return -1;
    return iswctype((wint_t)value, descriptor) != 0;
}

static inline int _rin_glob_class_symbol(const char* start,
                                         const char* end,
                                         char encoded[NAME_MAX + 1u],
                                         wchar_t* value)
{
    size_t length = (size_t)(end - start);
    if (length == 0u) return -1;
    if (length > NAME_MAX) return -1;
    {
        const char* cursor = start;
        const char* after;
        char first_character[5];
        if (_rin_glob_decode_character(cursor, &after, first_character,
                                       value) != 0 || after > end)
            return -1;
        while (after < end) {
            wchar_t ignored;
            char character[5];
            cursor = after;
            if (_rin_glob_decode_character(cursor, &after, character,
                                           &ignored) != 0 || after > end)
                return -1;
        }
    }
    memcpy(encoded, start, length);
    encoded[length] = '\0';
    return 0;
}

static inline int _rin_glob_class_match(const char** pattern,
                                        wchar_t value, int flags,
                                        const char* value_bytes,
                                        size_t value_length,
                                        size_t* value_consumed)
{
    const char* cursor = *pattern;
    int negate = 0;
    int matched = 0;
    int have_item = 0;
    int value_is_single_character = 0;
    if (value_consumed) *value_consumed = value_length;
    if (value_bytes && value_length != 0u) {
        const char* after_value;
        char encoded_value[5];
        wchar_t decoded_value;
        if (_rin_glob_decode_character(value_bytes, &after_value,
                                       encoded_value, &decoded_value) == 0 &&
            after_value == value_bytes + value_length) {
            value = decoded_value;
            value_is_single_character = 1;
        }
    }
    if (*cursor == '^' || *cursor == '!') {
        negate = 1;
        ++cursor;
    }
    while (*cursor != '\0' && (*cursor != ']' || !have_item)) {
        char first[NAME_MAX + 1u];
        wchar_t first_value;
        const char* after_first;

        if (*cursor == ']' && !have_item) {
            first[0] = ']';
            first[1] = '\0';
            first_value = L']';
            ++cursor;
        } else if (cursor[0] == '[' &&
            (cursor[1] == ':' || cursor[1] == '.' || cursor[1] == '=')) {
            char delimiter = cursor[1];
            const char* item_start = cursor + 2u;
            const char* item_end = item_start;
            while (*item_end != '\0' &&
                   !(item_end[0] == delimiter && item_end[1] == ']'))
                ++item_end;
            if (*item_end == '\0' || item_end == item_start) return -1;
            if (delimiter == ':') {
                int class_result = _rin_glob_wide_class(
                    item_start, (size_t)(item_end - item_start), value);
                if (class_result < 0) return -1;
                if (value_bytes && value_is_single_character && class_result)
                    matched = 1;
                cursor = item_end + 2u;
                have_item = 1;
                continue;
            }
            {
                int symbol_result = _rin_glob_class_symbol(
                    item_start, item_end, first, &first_value);
                if (symbol_result != 0) return symbol_result;
                cursor = item_end + 2u;
            }
        } else {
            const char* character = cursor;
            if (*cursor == '\\' && !(flags & GLOB_NOESCAPE)) {
                if (cursor[1] == '\0') return -1;
                character = cursor + 1u;
            }
            if (_rin_glob_decode_character(character, &after_first,
                                           first, &first_value) != 0)
                return -1;
            cursor = after_first;
        }
        have_item = 1;
        if (*cursor == '-' && cursor[1] != '\0' && cursor[1] != ']') {
            char last[NAME_MAX + 1u];
            wchar_t last_value;
            const char* character;
            const char* after_last;
            ++cursor;
            if (cursor[0] == '[' &&
                (cursor[1] == '.' || cursor[1] == '=')) {
                char delimiter = cursor[1];
                const char* item_start = cursor + 2u;
                const char* item_end = item_start;
                while (*item_end != '\0' &&
                       !(item_end[0] == delimiter && item_end[1] == ']'))
                    ++item_end;
                if (*item_end == '\0' || item_end == item_start) return -1;
                {
                    int symbol_result = _rin_glob_class_symbol(
                        item_start, item_end, last, &last_value);
                    if (symbol_result != 0) return symbol_result;
                }
                cursor = item_end + 2u;
            } else {
                character = cursor;
                if (*cursor == '\\' && !(flags & GLOB_NOESCAPE)) {
                    if (cursor[1] == '\0') return -1;
                    character = cursor + 1u;
                }
                if (_rin_glob_decode_character(character, &after_last,
                                               last, &last_value) != 0)
                    return -1;
                cursor = after_last;
            }
            if (strcoll(first, last) > 0) return -1;
            if (value_bytes && strcoll(first, value_bytes) <= 0 &&
                strcoll(value_bytes, last) <= 0)
                matched = 1;
        } else if (value_bytes && strcoll(first, value_bytes) == 0) {
            matched = 1;
        }
    }
    if (*cursor != ']' || !have_item) return -1;
    *pattern = cursor + 1u;
    /* This locale layer does not expose an inventory of every collating
     * element. Do not let a negated class consume an arbitrary multi-character
     * suffix; named collating/equivalence elements can still match explicitly. */
    if (negate)
        return value_is_single_character ? !matched : 0;
    return matched;
}

static inline int _rin_glob_component_validate(const char* pattern, int flags)
{
    if (!pattern) return -1;
    for (size_t index = 0u; pattern[index] != '\0'; ++index) {
        if (pattern[index] == '\\' && !(flags & GLOB_NOESCAPE)) {
            if (pattern[index + 1u] == '\0') return -1;
            ++index;
            continue;
        }
        if (pattern[index] == '[') {
            const char* class_end = pattern + index + 1u;
            if (_rin_glob_class_match(&class_end, 0, flags, NULL, 0u,
                                     NULL) < 0)
                return -1;
            index = (size_t)(class_end - pattern) - 1u;
        }
    }
    return 0;
}

static inline int _rin_glob_component_allows_period(const char* pattern,
                                                    int flags)
{
    const char* class_end;
    int class_result;
    if (flags & GLOB_PERIOD) return 1;
    if (!pattern || pattern[0] == '\0') return 0;
    if (pattern[0] == '.') return 1;
    if (pattern[0] == '\\' && !(flags & GLOB_NOESCAPE) &&
        pattern[1] == '.') return 1;
    if (pattern[0] != '[') return 0;
    if (pattern[1] == '!' || pattern[1] == '^') return 0;
    class_end = pattern + 1u;
    class_result = _rin_glob_class_match(
        &class_end, L'.', flags, ".", 1u, NULL);
    return class_result > 0;
}

/* A glob bracket may consume a collating element containing several active
 * locale characters. Memoize by pattern and byte offsets so those variable
 * length transitions remain bounded and '*' does not rescan suffixes. */
static inline int _rin_glob_component_match_at(
    const char* pattern, size_t pattern_length, const char* value,
    size_t value_length, size_t pattern_offset, size_t value_offset,
    int flags, unsigned char* memo, size_t memo_stride,
    size_t* operations, int* failure)
{
    size_t memo_index = pattern_offset * memo_stride + value_offset;
    unsigned char* state = &memo[memo_index];
    int matched = 0;
    if (*state == 2u) return 0;
    if (*state == 3u) return 1;
    if (*operations == 0u) {
        *failure = _RIN_GLOB_MATCH_BUDGET_EXHAUSTED;
        return 0;
    }
    --*operations;
    if (*state == 1u) return 0;
    *state = 1u;

    if (pattern_offset == pattern_length) {
        matched = value_offset == value_length;
    } else if (pattern[pattern_offset] == '*') {
        const char* next_value;
        char encoded[5];
        wchar_t ignored;
        if (_rin_glob_component_match_at(
                pattern, pattern_length, value, value_length,
                pattern_offset + 1u, value_offset, flags, memo,
                memo_stride, operations, failure)) {
            matched = 1;
        } else if (*failure == 0 && value_offset < value_length) {
            if (_rin_glob_decode_character(value + value_offset, &next_value,
                                           encoded, &ignored) != 0) {
                *failure = -1;
            } else {
                matched = _rin_glob_component_match_at(
                    pattern, pattern_length, value, value_length,
                    pattern_offset, (size_t)(next_value - value), flags,
                    memo, memo_stride, operations, failure);
            }
        }
    } else if (pattern[pattern_offset] == '?') {
        if (value_offset < value_length) {
            const char* next_value;
            char encoded[5];
            wchar_t ignored;
            if (_rin_glob_decode_character(value + value_offset, &next_value,
                                           encoded, &ignored) != 0) {
                *failure = -1;
            } else {
                matched = _rin_glob_component_match_at(
                    pattern, pattern_length, value, value_length,
                    pattern_offset + 1u, (size_t)(next_value - value), flags,
                    memo, memo_stride, operations, failure);
            }
        }
    } else if (pattern[pattern_offset] == '[') {
        const char* class_end = pattern + pattern_offset + 1u;
        size_t atom_end;
        if (_rin_glob_class_match(&class_end, 0, flags, NULL, 0u, NULL) < 0) {
            *failure = -1;
        } else {
            atom_end = (size_t)(class_end - pattern);
            for (size_t candidate_end = value_offset;
                 candidate_end < value_length && *failure == 0;) {
                const char* next_candidate;
                char encoded[5];
                char candidate[NAME_MAX + 1u];
                wchar_t first_value;
                size_t candidate_length;
                size_t pattern_cost = pattern_length - pattern_offset;
                size_t consumed = 0u;
                int class_result;
                if (_rin_glob_decode_character(
                        value + candidate_end, &next_candidate, encoded,
                        &first_value) != 0) {
                    *failure = -1;
                    break;
                }
                candidate_end = (size_t)(next_candidate - value);
                candidate_length = candidate_end - value_offset;
                if (pattern_cost > *operations) {
                    *failure = _RIN_GLOB_MATCH_BUDGET_EXHAUSTED;
                    break;
                }
                *operations -= pattern_cost;
                memcpy(candidate, value + value_offset, candidate_length);
                candidate[candidate_length] = '\0';
                class_end = pattern + pattern_offset + 1u;
                class_result = _rin_glob_class_match(
                    &class_end, first_value, flags, candidate,
                    candidate_length, &consumed);
                if (class_result < 0) {
                    *failure = -1;
                    break;
                }
                if (class_result > 0 && consumed == candidate_length &&
                    _rin_glob_component_match_at(
                        pattern, pattern_length, value, value_length,
                        atom_end, candidate_end, flags, memo, memo_stride,
                        operations, failure)) {
                    matched = 1;
                    break;
                }
            }
        }
    } else if (value_offset < value_length) {
        const char* literal = pattern + pattern_offset;
        const char* after_literal;
        const char* after_value;
        char literal_encoded[5];
        char value_encoded[5];
        wchar_t ignored;
        if (pattern[pattern_offset] == '\\' &&
            !(flags & GLOB_NOESCAPE)) {
            if (pattern_offset + 1u >= pattern_length) {
                *failure = -1;
            } else {
                literal = pattern + pattern_offset + 1u;
            }
        }
        if (*failure == 0 &&
            (_rin_glob_decode_character(literal, &after_literal,
                                        literal_encoded, &ignored) != 0 ||
             _rin_glob_decode_character(value + value_offset, &after_value,
                                        value_encoded, &ignored) != 0)) {
            *failure = -1;
        } else if (*failure == 0) {
            size_t literal_length = (size_t)(after_literal - literal);
            size_t atom_end = (size_t)(after_literal - pattern);
            if (literal == pattern + pattern_offset &&
                pattern[pattern_offset] == '\\' &&
                !(flags & GLOB_NOESCAPE))
                atom_end = pattern_offset + 1u + literal_length;
            if (literal_length == (size_t)(after_value - (value + value_offset)) &&
                memcmp(literal, value + value_offset, literal_length) == 0)
                matched = _rin_glob_component_match_at(
                    pattern, pattern_length, value, value_length, atom_end,
                    (size_t)(after_value - value), flags, memo, memo_stride,
                    operations, failure);
        }
    }

    *state = matched ? 3u : 2u;
    return matched;
}

static inline int _rin_glob_component_match_bounded(
    const char* pattern, const char* value, int flags)
{
    size_t pattern_length;
    size_t value_length;
    size_t memo_stride;
    size_t memo_size;
    size_t operations = _RIN_GLOB_MATCH_OPERATION_BUDGET;
    unsigned char* memo;
    int failure = 0;
    int matched;

    if (!pattern || !value) return -1;
    pattern_length = _rin_glob_strlen(pattern);
    value_length = _rin_glob_strlen(value);
    if (pattern_length > NAME_MAX || value_length > NAME_MAX) return -1;
    memo_stride = value_length + 1u;
    memo_size = (pattern_length + 1u) * memo_stride;
    memo = (unsigned char*)malloc(memo_size);
    if (!memo) return _RIN_GLOB_MATCH_BUDGET_EXHAUSTED;
    memset(memo, 0, memo_size);
    matched = _rin_glob_component_match_at(
        pattern, pattern_length, value, value_length, 0u, 0u, flags, memo,
        memo_stride, &operations, &failure);
    free(memo);
    return failure != 0 ? failure : matched;
}

static inline int _rin_glob_component_match(const char* pattern,
                                            const char* value, int flags)
{
    const char* cursor;
    int result;
    if (!pattern || !value) return 0;
    /* Reject an invalid or incomplete multibyte filename consistently, even
     * when a leading '*' could otherwise accept it without decoding. */
    for (cursor = value; *cursor != '\0';) {
        const char* next;
        char encoded[5];
        wchar_t ignored;
        if (_rin_glob_decode_character(cursor, &next, encoded, &ignored) != 0)
            return 0;
        cursor = next;
    }
    result = _rin_glob_component_match_bounded(pattern, value, flags);
    return result;
}

static inline int _rin_glob_path_join(const char* prefix, const char* name,
                                      char* output, size_t capacity)
{
    size_t prefix_length = _rin_glob_strlen(prefix);
    size_t name_length = _rin_glob_strlen(name);
    size_t separator = prefix_length != 0u &&
                       prefix[prefix_length - 1u] != '/' ? 1u : 0u;
    if (prefix_length > capacity || name_length > capacity - prefix_length ||
        separator > capacity - prefix_length - name_length ||
        prefix_length + separator + name_length + 1u > capacity)
        return GLOB_NOSPACE;
    for (size_t index = 0; index < prefix_length; ++index)
        output[index] = prefix[index];
    if (separator) output[prefix_length++] = '/';
    for (size_t index = 0; index < name_length; ++index)
        output[prefix_length + index] = name[index];
    output[prefix_length + name_length] = '\0';
    return 0;
}

static inline int _rin_glob_entry_is_directory(const char* path,
                                               _RinGlobContext* context,
                                               const struct dirent* entry,
                                               int* is_directory)
{
    struct stat status;
    int alternative;
    if (!path || !context || !is_directory) return GLOB_ABORTED;
    alternative = (context->flags & GLOB_ALTDIRFUNC) != 0;
    if (!alternative && entry && entry->d_type == DT_DIR) {
        *is_directory = 1;
        return 0;
    }
    if (!alternative && entry && entry->d_type != DT_UNKNOWN &&
        entry->d_type != DT_LNK) {
        *is_directory = 0;
        return 0;
    }
    if (alternative) {
        if (context->pglob->gl_lstat(path, &status) != 0) {
            if (errno == ENOENT || errno == ENOTDIR) {
                *is_directory = 0;
                return 0;
            }
            return GLOB_ABORTED;
        }
        if (!S_ISLNK(status.st_mode)) {
            *is_directory = S_ISDIR(status.st_mode) ? 1 : 0;
            return 0;
        }
    }
    if ((alternative ? context->pglob->gl_stat(path, &status)
                     : _RIN_GLOB_STAT(path, &status)) != 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            *is_directory = 0;
            return 0;
        }
        return GLOB_ABORTED;
    }
    *is_directory = S_ISDIR(status.st_mode) ? 1 : 0;
    return 0;
}

static inline void* _rin_glob_opendir(_RinGlobContext* context,
                                      const char* path)
{
    if (context->flags & GLOB_ALTDIRFUNC)
        return context->pglob->gl_opendir(path);
    return (void*)_RIN_GLOB_OPENDIR(path);
}

static inline struct dirent* _rin_glob_readdir(_RinGlobContext* context,
                                               void* directory)
{
    if (context->flags & GLOB_ALTDIRFUNC)
        return context->pglob->gl_readdir(directory);
    return _RIN_GLOB_READDIR((DIR*)directory);
}

static inline int _rin_glob_closedir(_RinGlobContext* context,
                                     void* directory)
{
    if (context->flags & GLOB_ALTDIRFUNC) {
        context->pglob->gl_closedir(directory);
        return 0;
    }
    return _RIN_GLOB_CLOSEDIR((DIR*)directory);
}

static inline int _rin_glob_path_stat(_RinGlobContext* context,
                                      const char* path, struct stat* status,
                                      int no_follow)
{
    if (context->flags & GLOB_ALTDIRFUNC) {
        int (*callback)(const char*, struct stat*) = no_follow
            ? context->pglob->gl_lstat : context->pglob->gl_stat;
        return callback(path, status);
    }
    return _RIN_GLOB_STAT(path, status);
}

static inline int _rin_glob_report_error(_RinGlobContext* context,
                                         const char* path, int error)
{
    if (context->errfunc && context->errfunc(path, error) != 0)
        return GLOB_ABORTED;
    return (context->flags & GLOB_ERR) ? GLOB_ABORTED : 0;
}

static inline int _rin_glob_split_component(const char* pattern,
                                            size_t* cursor, const char** start,
                                            size_t* length)
{
    size_t begin;
    if (!pattern || !cursor || !start || !length) return GLOB_ABORTED;
    while (pattern[*cursor] == '/') ++*cursor;
    begin = *cursor;
    while (pattern[*cursor] != '\0' && pattern[*cursor] != '/') ++*cursor;
    *start = pattern + begin;
    *length = *cursor - begin;
    return *length == 0u ? 1 : 0;
}

static inline int _rin_glob_add_marked(_RinGlobMatchList* list,
                                       const char* path, int flags,
                                       int is_directory)
{
    size_t length = _rin_glob_strlen(path);
    if ((flags & GLOB_MARK) && is_directory &&
        (length == 0u || path[length - 1u] != '/')) {
        char* marked;
        int result;
        if (length + 2u > _RIN_GLOB_MAX_PATH) return GLOB_NOSPACE;
        marked = (char*)malloc(length + 2u);
        if (!marked) return GLOB_NOSPACE;
        for (size_t index = 0; index < length; ++index)
            marked[index] = path[index];
        marked[length++] = '/';
        marked[length] = '\0';
        result = _rin_glob_match_list_add(list, marked);
        free(marked);
        return result;
    }
    return _rin_glob_match_list_add(list, path);
}

static inline int _rin_glob_expand(_RinGlobContext* context,
                                   const _RinGlobComponent* components,
                                   size_t component_count, size_t index,
                                   const char* prefix,
                                   _RinGlobExpandScratch* scratch_frames);

static inline int _rin_glob_expand_body(
    _RinGlobContext* context, const _RinGlobComponent* components,
    size_t component_count, size_t index, const char* prefix,
    _RinGlobExpandScratch* scratch,
    _RinGlobExpandScratch* scratch_frames)
{
    char* raw_component;
    char* literal_component;
    char* child;
    int result;
    if (!context || !components || !prefix || !scratch ||
        index > component_count)
        return GLOB_ABORTED;
    raw_component = scratch->raw_component;
    literal_component = scratch->literal_component;
    child = scratch->child;
    if (index == component_count) {
        struct stat status;
        if (prefix[0] == '\0') return 0;
        int no_follow = (context->flags & GLOB_ALTDIRFUNC) &&
                        !(context->flags & GLOB_ONLYDIR) &&
                        !context->require_directory &&
                        !(context->flags & GLOB_MARK);
        if (_rin_glob_path_stat(context, prefix, &status, no_follow) != 0) {
            int saved_errno = errno;
            if (saved_errno == ENOENT || saved_errno == ENOTDIR) return 0;
            return _rin_glob_report_error(context, prefix, saved_errno);
        }
        if (((context->flags & GLOB_ONLYDIR) || context->require_directory) &&
            !S_ISDIR(status.st_mode))
            return 0;
        return _rin_glob_add_marked(context->matches, prefix, context->flags,
                                    S_ISDIR(status.st_mode));
    }
    if (components[index].length >= sizeof(scratch->raw_component))
        return GLOB_NOSPACE;
    for (size_t offset = 0; offset < components[index].length; ++offset)
        raw_component[offset] = components[index].text[offset];
    raw_component[components[index].length] = '\0';
    if (!components[index].wildcard) {
        result = _rin_glob_component_copy(raw_component,
                                           components[index].length,
                                           context->flags, literal_component,
                                           sizeof(scratch->literal_component));
        if (result != 0) return result;
        result = _rin_glob_path_join(prefix, literal_component, child,
                                     _RIN_GLOB_MAX_PATH);
        if (result == 0)
            result = _rin_glob_expand(context, components, component_count,
                                      index + 1u, child, scratch_frames);
        return result;
    }
    {
        const char* directory = prefix[0] != '\0' ? prefix : ".";
        void* directory_stream = _rin_glob_opendir(context, directory);
        struct dirent* entry;
        int dot_allowed = _rin_glob_component_allows_period(raw_component,
                                                             context->flags);
        if (!directory_stream) {
            int saved_errno = errno;
            result = _rin_glob_report_error(context, directory, saved_errno);
            return result;
        }
        result = 0;
        for (;;) {
            int is_directory = 0;
            errno = 0;
            entry = _rin_glob_readdir(context, directory_stream);
            if (!entry) {
                int saved_errno = errno;
                if (saved_errno != 0)
                    result = _rin_glob_report_error(context, directory,
                                                    saved_errno);
                break;
            }
            if (entry->d_name[0] == '\0' ||
                (entry->d_name[0] == '.' &&
                 (entry->d_name[1] == '\0' ||
                  (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) ||
                (!dot_allowed && entry->d_name[0] == '.'))
                continue;
            {
                int component_match = _rin_glob_component_match(
                    raw_component, entry->d_name, context->flags);
                if (component_match == _RIN_GLOB_MATCH_BUDGET_EXHAUSTED) {
                    errno = EOVERFLOW;
                    result = GLOB_NOSPACE;
                    break;
                }
                if (component_match < 0) {
                    result = GLOB_NOSYS;
                    break;
                }
                if (component_match == 0) continue;
            }
            result = _rin_glob_path_join(prefix, entry->d_name, child,
                                         _RIN_GLOB_MAX_PATH);
            if (result != 0) break;
            if (index + 1u < component_count) {
                result = _rin_glob_entry_is_directory(child, context, entry,
                                                      &is_directory);
                if (result != 0) {
                    int saved_errno = errno;
                    result = _rin_glob_report_error(context, child,
                                                    saved_errno);
                    if (result != 0) break;
                    continue;
                }
                if (!is_directory) continue;
            }
            result = _rin_glob_expand(context, components, component_count,
                                      index + 1u, child, scratch_frames);
            if (result != 0) break;
        }
        if (_rin_glob_closedir(context, directory_stream) != 0 &&
            result == 0) {
            int saved_errno = errno;
            result = _rin_glob_report_error(context, directory, saved_errno);
        }
        return result;
    }
}

static inline int _rin_glob_expand(_RinGlobContext* context,
                                   const _RinGlobComponent* components,
                                   size_t component_count, size_t index,
                                   const char* prefix,
                                   _RinGlobExpandScratch* scratch_frames)
{
    if (!scratch_frames || index > component_count ||
        index > _RIN_GLOB_MAX_COMPONENTS)
        return GLOB_ABORTED;
    return _rin_glob_expand_body(context, components, component_count, index,
                                  prefix, &scratch_frames[index],
                                  scratch_frames);
}

static inline int _rin_glob_string_compare(const char* left, const char* right)
{
    return strcoll(left, right);
}

static inline void _rin_glob_heap_sift_down(_RinGlobMatchList* list,
                                             size_t root, size_t count)
{
    while (root < count / 2u) {
        size_t child = root * 2u + 1u;
        char* temporary;
        if (child + 1u < count &&
            _rin_glob_string_compare(list->items[child],
                                     list->items[child + 1u]) < 0)
            ++child;
        if (_rin_glob_string_compare(list->items[root],
                                     list->items[child]) >= 0)
            return;
        temporary = list->items[root];
        list->items[root] = list->items[child];
        list->items[child] = temporary;
        root = child;
    }
}

static inline void _rin_glob_sort(_RinGlobMatchList* list)
{
    size_t start;
    size_t end;
    if (!list || list->count < 2u) return;
    /* Heap sort keeps the fixed match cap's worst-case work O(n log n)
     * without allocating scratch space or changing the bytewise order. */
    for (start = list->count / 2u; start != 0u; --start)
        _rin_glob_heap_sift_down(list, start - 1u, list->count);
    for (end = list->count; end > 1u; --end) {
        char* temporary = list->items[0];
        list->items[0] = list->items[end - 1u];
        list->items[end - 1u] = temporary;
        _rin_glob_heap_sift_down(list, 0u, end - 1u);
    }
}

static inline int _rin_glob_commit(glob_t* pglob, int flags,
                                   _RinGlobMatchList* matches)
{
    size_t offsets;
    size_t old_count;
    size_t total;
    char** paths;
    int append = (flags & GLOB_APPEND) != 0;
    size_t old_pathc = append ? pglob->gl_pathc : 0u;
    char** old_paths = append && old_pathc != 0u ? pglob->gl_pathv : NULL;
    size_t old_offsets = (append || (flags & GLOB_DOOFFS) != 0) ?
                         pglob->gl_offs : 0u;
    if (append && old_pathc != 0u && !old_paths) {
        errno = EINVAL;
        return GLOB_ABORTED;
    }
    offsets = old_offsets;
    old_count = append ? old_pathc : 0u;
    if (offsets > (size_t)-1 - old_count ||
        offsets + old_count > (size_t)-1 - matches->count - 1u)
        return GLOB_NOSPACE;
    total = offsets + old_count + matches->count + 1u;
    if (total > (size_t)-1 / sizeof(char*)) return GLOB_NOSPACE;
    paths = (char**)malloc(total * sizeof(char*));
    if (!paths) return GLOB_NOSPACE;
    for (size_t index = 0; index < offsets; ++index) paths[index] = NULL;
    for (size_t index = 0; index < old_count; ++index)
        paths[offsets + index] = old_paths[old_offsets + index];
    for (size_t index = 0; index < matches->count; ++index)
        paths[offsets + old_count + index] = matches->items[index];
    paths[offsets + old_count + matches->count] = NULL;
    if (old_paths) free(old_paths);
    free(matches->items);
    matches->items = NULL;
    matches->count = 0u;
    matches->capacity = 0u;
    pglob->gl_pathv = paths;
    pglob->gl_pathc = old_count + (total - offsets - old_count - 1u);
    pglob->gl_offs = offsets;
    pglob->gl_flags = append ? pglob->gl_flags | flags : flags;
    return 0;
}

/* A fresh no-match result must not leave uninitialized or stale output
 * counters behind.  With GLOB_APPEND the prior result remains valid. */
static inline int _rin_glob_no_match(glob_t* pglob, int flags)
{
    if (!(flags & GLOB_APPEND)) {
        pglob->gl_pathc = 0u;
        pglob->gl_pathv = NULL;
        pglob->gl_flags = flags;
        if (!(flags & GLOB_DOOFFS)) pglob->gl_offs = 0u;
    } else
        pglob->gl_flags |= flags;
    return GLOB_NOMATCH;
}

/* Locate the first unescaped brace pair containing a top-level comma.  The
 * scanner understands nested pairs so an alternative may itself contain a
 * brace expression.  A malformed (unclosed) brace is reported separately;
 * brace pairs without a comma remain literal, matching the conservative
 * bounded policy used by this header. */
static inline int _rin_glob_find_brace(const char* pattern,
                                       int flags, size_t* open,
                                       size_t* close, size_t* comma,
                                       _RinGlobScratch* scratch)
{
    size_t index = 0u;
    size_t* opens;
    size_t* commas;
    size_t nested_open = 0u;
    size_t nested_close = 0u;
    size_t nested_comma = (size_t)-1;
    unsigned int depth = 0u;
    if (!pattern || !open || !close || !comma || !scratch) return -1;
    opens = scratch->brace_opens;
    commas = scratch->brace_commas;
    while (pattern[index] != '\0') {
        if (pattern[index] == '\\' && !(flags & GLOB_NOESCAPE)) {
            if (pattern[index + 1u] == '\0') return -1;
            index += 2u;
            continue;
        }
        if (pattern[index] == '{') {
            if (depth >= _RIN_GLOB_MAX_BRACE_NESTING) return -1;
            opens[depth] = index;
            commas[depth] = (size_t)-1;
            ++depth;
        } else if (pattern[index] == '}') {
            if (depth == 0u) return -1;
            --depth;
            if (depth == 0u && commas[depth] != (size_t)-1) {
                *open = opens[depth];
                *close = index;
                *comma = commas[depth];
                return 1;
            }
            if (depth != 0u && commas[depth] != (size_t)-1 &&
                nested_comma == (size_t)-1) {
                nested_open = opens[depth];
                nested_close = index;
                nested_comma = commas[depth];
            }
        } else if (pattern[index] == ',' && depth == 1u) {
            commas[0] = index;
        } else if (pattern[index] == ',' && depth != 0u) {
            if (commas[depth - 1u] == (size_t)-1)
                commas[depth - 1u] = index;
        }
        ++index;
    }
    if (depth != 0u) return -1;
    if (nested_comma != (size_t)-1) {
        *open = nested_open;
        *close = nested_close;
        *comma = nested_comma;
        return 1;
    }
    return 0;
}

static inline int _rin_glob_brace_expand(const char* pattern, int flags,
                                         int (*errfunc)(const char*, int),
                                         glob_t* pglob, int* handled,
                                         _RinGlobScratch* scratch,
                                         unsigned int brace_depth)
{
    size_t open;
    size_t close;
    size_t comma;
    int found;
    glob_t aggregate = {0};
    size_t part_start;
    int aggregate_has_result = 0;
    int aggregate_result = 0;
    int alternative_flags;
    int aggregate_output_flags = 0;
    _RinGlobMatchList matches = {NULL, 0u, 0u};
    char* alternative;

    if (!pattern || !pglob || !handled || !scratch) return GLOB_ABORTED;
    *handled = 0;
    if (flags & GLOB_ALTDIRFUNC) {
        aggregate.gl_closedir = pglob->gl_closedir;
        aggregate.gl_readdir = pglob->gl_readdir;
        aggregate.gl_opendir = pglob->gl_opendir;
        aggregate.gl_lstat = pglob->gl_lstat;
        aggregate.gl_stat = pglob->gl_stat;
    }
    found = _rin_glob_find_brace(
        pattern, flags, &open, &close, &comma, scratch);
    if (found < 0) {
        *handled = 1;
        return GLOB_NOSYS;
    }
    if (found == 0) return 0;
    *handled = 1;

    /* Alternatives are collected without caller offsets or fallback
     * literals. Those policies are applied once, after every branch or an
     * aborting traversal has completed. */
    alternative_flags = flags & ~(GLOB_APPEND | GLOB_DOOFFS |
                                   GLOB_NOCHECK | GLOB_NOMAGIC);
    alternative = scratch->alternative;
    part_start = open + 1u;
    for (;;) {
        size_t part_end = part_start;
        size_t suffix_length = _rin_glob_strlen(pattern + close + 1u);
        size_t prefix_length = open;
        size_t part_length;
        size_t total_length;
        size_t cursor;
        int result;
        unsigned int depth = 0u;

        while (part_end < close) {
            char value = pattern[part_end];
            if (value == '\\' && !(flags & GLOB_NOESCAPE)) {
                if (part_end + 1u >= close) {
                    globfree(&aggregate);
                    return GLOB_NOSYS;
                }
                part_end += 2u;
                continue;
            }
            if (value == '{') ++depth;
            else if (value == '}' && depth != 0u) --depth;
            if (value == ',' && depth == 0u) break;
            ++part_end;
        }
        part_length = part_end - part_start;
        if (prefix_length > _RIN_GLOB_MAX_PATTERN - 1u ||
            part_length > _RIN_GLOB_MAX_PATTERN - 1u - prefix_length ||
            suffix_length > _RIN_GLOB_MAX_PATTERN - 1u -
                             prefix_length - part_length) {
            globfree(&aggregate);
            return GLOB_NOSPACE;
        }
        total_length = prefix_length + part_length + suffix_length;
        if (total_length >= _RIN_GLOB_MAX_PATTERN) {
            globfree(&aggregate);
            return GLOB_NOSPACE;
        }
        cursor = 0u;
        while (cursor < prefix_length) {
            alternative[cursor] = pattern[cursor];
            ++cursor;
        }
        for (size_t index = 0u; index < part_length; ++index)
            alternative[cursor + index] = pattern[part_start + index];
        cursor += part_length;
        for (size_t index = 0u; index < suffix_length; ++index)
            alternative[cursor + index] = pattern[close + 1u + index];
        alternative[total_length] = '\0';

        result = _rin_glob_run(
            alternative,
            aggregate_has_result ? alternative_flags | GLOB_APPEND
                                 : alternative_flags,
            errfunc, &aggregate, brace_depth + 1u);
        aggregate_output_flags |= aggregate.gl_flags & GLOB_MAGCHAR;
        if (result == 0) {
            aggregate_has_result = 1;
        } else if (result == GLOB_ABORTED) {
            aggregate_result = result;
            break;
        } else if (result != GLOB_NOMATCH) {
            globfree(&aggregate);
            return result;
        }
        if (part_end == close) break;
        part_start = part_end + 1u;
    }

    if (aggregate.gl_pathc == 0u) {
        globfree(&aggregate);
        if (aggregate_result != 0) return aggregate_result;
        if ((flags & GLOB_NOCHECK) != 0) {
            if (_rin_glob_match_list_add(&matches, pattern) != 0)
                return GLOB_NOSPACE;
        } else {
            return _rin_glob_no_match(
                pglob, flags | aggregate_output_flags);
        }
    } else {
        matches.items = (char**)malloc(
            aggregate.gl_pathc * sizeof(*matches.items));
        if (!matches.items) {
            globfree(&aggregate);
            return GLOB_NOSPACE;
        }
        matches.count = aggregate.gl_pathc;
        matches.capacity = aggregate.gl_pathc;
        for (size_t index = 0u; index < aggregate.gl_pathc; ++index)
            matches.items[index] = aggregate.gl_pathv[index];
        free(aggregate.gl_pathv);
        aggregate.gl_pathv = NULL;
        aggregate.gl_pathc = 0u;
    }
    if (!(flags & GLOB_NOSORT)) _rin_glob_sort(&matches);
    found = _rin_glob_commit(
        pglob, flags | aggregate_output_flags, &matches);
    if (found != 0) _rin_glob_match_list_free(&matches);
    return found != 0 ? found : aggregate_result;
}

/* Expand the current user's home directory without consulting an ambient
 * user database.  The target environment owns HOME; arbitrary ~user lookup
 * would otherwise turn an untrusted name into an authority-bearing path. */
static inline int _rin_glob_tilde_expand(const char* pattern, int flags,
                                         int (*errfunc)(const char*, int),
                                         glob_t* pglob, int* handled,
                                         _RinGlobScratch* scratch,
                                         unsigned int brace_depth)
{
    const char* home;
    const char* suffix;
    size_t home_length;
    size_t suffix_length;
    size_t total_length;
    char* expanded;
    size_t cursor;

    if (!pattern || !pglob || !handled || !scratch) return GLOB_ABORTED;
    *handled = 0;
    if (!(flags & GLOB_TILDE) || pattern[0] != '~') return 0;
    *handled = 1;
    if (pattern[1] != '\0' && pattern[1] != '/')
        return GLOB_NOSYS;

    home = _RIN_GLOB_GETENV("HOME");
    home_length = _rin_glob_strlen(home);
    if (!home || home_length == 0u || home[0] != '/')
        return GLOB_NOSYS;
    suffix = pattern + 1u;
    expanded = scratch->expanded;
    suffix_length = _rin_glob_strlen(suffix);
    if (home_length > _RIN_GLOB_MAX_PATTERN - 1u ||
        suffix_length > _RIN_GLOB_MAX_PATTERN - 1u - home_length)
        return GLOB_NOSPACE;
    total_length = home_length + suffix_length;
    if (suffix_length != 0u && suffix[0] == '/' &&
        home[home_length - 1u] == '/') {
        ++suffix;
        --suffix_length;
        --total_length;
    }
    if (total_length >= _RIN_GLOB_MAX_PATTERN)
        return GLOB_NOSPACE;
    for (cursor = 0u; cursor < home_length; ++cursor)
        expanded[cursor] = home[cursor];
    for (size_t index = 0u; index < suffix_length; ++index)
        expanded[home_length + index] = suffix[index];
    expanded[total_length] = '\0';
    return _rin_glob_run(
        expanded, flags & ~GLOB_TILDE, errfunc, pglob, brace_depth);
}

/* Expand pathname components with bounded directory traversal. */
static inline int _rin_glob_run_body(
    const char* pattern, int flags, int (*errfunc)(const char*, int),
    glob_t* pglob, _RinGlobScratch* scratch, unsigned int brace_depth)
{
    const int known_flags = GLOB_ERR | GLOB_MARK | GLOB_NOSORT |
                            GLOB_DOOFFS | GLOB_NOCHECK | GLOB_APPEND |
                            GLOB_NOESCAPE | GLOB_PERIOD | GLOB_BRACE |
                            GLOB_NOMAGIC | GLOB_TILDE | GLOB_ONLYDIR |
                            GLOB_ALTDIRFUNC;
    size_t pattern_length;
    size_t component_count = 0u;
    size_t cursor = 0u;
    size_t old_pathc;
    int has_wildcard = 0;
    int output_flags;
    int trailing_slash;
    char* prefix;
    _RinGlobComponent* components;
    _RinGlobMatchList matches = { NULL, 0u, 0u };
    _RinGlobContext context;
    _RinGlobExpandScratch* scratch_frames;

    if (!pglob || !pattern || !scratch) {
        errno = EINVAL;
        return GLOB_ABORTED;
    }
    prefix = scratch->prefix;
    components = scratch->components;
    if ((flags & ~known_flags) != 0)
        return GLOB_NOSYS;
    if ((flags & GLOB_ALTDIRFUNC) &&
        (!pglob->gl_opendir || !pglob->gl_readdir ||
         !pglob->gl_closedir || !pglob->gl_lstat || !pglob->gl_stat))
        return GLOB_NOSYS;
    {
        int tilde_handled = 0;
        int tilde_result = _rin_glob_tilde_expand(
            pattern, flags, errfunc, pglob, &tilde_handled, scratch,
            brace_depth);
        if (tilde_handled) return tilde_result;
    }
    if (flags & GLOB_BRACE) {
        int brace_handled = 0;
        int brace_result = _rin_glob_brace_expand(
            pattern, flags, errfunc, pglob, &brace_handled, scratch,
            brace_depth);
        if (brace_handled) return brace_result;
    }
    pattern_length = _rin_glob_strlen(pattern);
    if (pattern_length >= _RIN_GLOB_MAX_PATTERN) return GLOB_NOSPACE;
    trailing_slash = pattern_length != 0u &&
                     pattern[pattern_length - 1u] == '/';
    prefix[0] = '\0';
    if (pattern[0] == '/') {
        prefix[0] = '/';
        prefix[1] = '\0';
        cursor = 1u;
    }
    while (cursor < pattern_length) {
        size_t begin;
        size_t length;
        char* component_copy = scratch->component_copy;
        while (cursor < pattern_length && pattern[cursor] == '/') ++cursor;
        if (cursor == pattern_length) break;
        if (component_count >= _RIN_GLOB_MAX_COMPONENTS)
            return GLOB_NOSPACE;
        begin = cursor;
        while (cursor < pattern_length && pattern[cursor] != '/') {
            if (pattern[cursor] == '\\' && !(flags & GLOB_NOESCAPE) &&
                cursor + 1u < pattern_length) {
                if (pattern[cursor + 1u] == '/') return GLOB_NOSYS;
                cursor += 2u;
            } else
                ++cursor;
        }
        length = cursor - begin;
        if (length == 0u || length >= _RIN_GLOB_MAX_PATH)
            return GLOB_NOSPACE;
        for (size_t index = 0; index < length; ++index)
            component_copy[index] = pattern[begin + index];
        component_copy[length] = '\0';
        components[component_count].text = pattern + begin;
        components[component_count].length = length;
        components[component_count].wildcard =
            _rin_glob_component_wild(component_copy, flags);
        if (components[component_count].wildcard < 0)
            return GLOB_NOSYS;
        if (_rin_glob_component_validate(component_copy, flags) != 0)
            return GLOB_NOSYS;
        if (components[component_count].wildcard > 0) has_wildcard = 1;
        ++component_count;
    }
    output_flags = flags | (has_wildcard ? GLOB_MAGCHAR : 0);
    context.flags = flags | (trailing_slash ? GLOB_MARK : 0);
    context.require_directory = trailing_slash;
    context.errfunc = errfunc;
    context.pglob = pglob;
    context.matches = &matches;
    old_pathc = (flags & GLOB_APPEND) ? pglob->gl_pathc : 0u;
    if ((flags & GLOB_APPEND) && old_pathc != 0u && !pglob->gl_pathv) {
        errno = EINVAL;
        return GLOB_ABORTED;
    }
    scratch_frames = (_RinGlobExpandScratch*)malloc(
        (component_count + 1u) * sizeof(*scratch_frames));
    if (!scratch_frames) return GLOB_NOSPACE;
    {
        int result = _rin_glob_expand(&context, components, component_count,
                                      0u, prefix, scratch_frames);
        free(scratch_frames);
        if (result != 0) {
            if (result == GLOB_ABORTED && matches.count != 0u) {
                int publish_result;
                if (!(flags & GLOB_NOSORT)) _rin_glob_sort(&matches);
                publish_result = _rin_glob_commit(
                    pglob, output_flags, &matches);
                if (publish_result != 0) {
                    _rin_glob_match_list_free(&matches);
                    return publish_result;
                }
                return result;
            }
            _rin_glob_match_list_free(&matches);
            return result;
        }
    }
    if (matches.count == 0u) {
        if ((flags & (GLOB_NOCHECK | GLOB_NOMAGIC)) &&
            !(flags & GLOB_ONLYDIR) &&
            ((flags & GLOB_NOCHECK) || !has_wildcard)) {
            if (_rin_glob_match_list_add(&matches, pattern) != 0) {
                _rin_glob_match_list_free(&matches);
                return GLOB_NOSPACE;
            }
        } else {
            _rin_glob_match_list_free(&matches);
            return _rin_glob_no_match(pglob, output_flags);
        }
    }
    if (!(flags & GLOB_NOSORT)) _rin_glob_sort(&matches);
    {
        int result = _rin_glob_commit(pglob, output_flags, &matches);
        if (result != 0) _rin_glob_match_list_free(&matches);
        return result;
    }
}

static inline int _rin_glob_run(const char* pattern, int flags,
                                int (*errfunc)(const char*, int),
                                glob_t* pglob, unsigned int brace_depth)
{
    _RinGlobScratch* scratch;
    int result;
    if (brace_depth > _RIN_GLOB_MAX_BRACE_RECURSION)
        return GLOB_NOSPACE;
    scratch = (_RinGlobScratch*)malloc(sizeof(*scratch));
    if (!scratch) return GLOB_NOSPACE;
    result = _rin_glob_run_body(
        pattern, flags, errfunc, pglob, scratch, brace_depth);
    free(scratch);
    return result;
}

static inline int glob(const char* pattern, int flags,
                       int (*errfunc)(const char*, int),
                       glob_t* pglob)
{
    return _rin_glob_run(pattern, flags, errfunc, pglob, 0u);
}

/* globfree - glob結果を解放 */
static inline void globfree(glob_t* pglob) {
    if (pglob) {
        if (pglob->gl_pathv) {
            for (size_t index = 0; index < pglob->gl_pathc; ++index)
                free(pglob->gl_pathv[pglob->gl_offs + index]);
            free(pglob->gl_pathv);
        }
        pglob->gl_pathc = 0;
        pglob->gl_pathv = NULL;
        pglob->gl_offs = 0;
        pglob->gl_flags = 0;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* _GLOB_H */
