/*
 * RinOS libc - glob.h
 * パス名パターン展開
 */

#ifndef _GLOB_H
#define _GLOB_H

#include "stddef.h"
#include "errno.h"
#include "stdlib.h"
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
    void* (*gl_readdir)(void*);
    void* (*gl_opendir)(const char*);
    int (*gl_lstat)(const char*, void*);
    int (*gl_stat)(const char*, void*);
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
    int (*errfunc)(const char*, int);
    _RinGlobMatchList* matches;
} _RinGlobContext;

typedef struct _RinGlobComponent {
    const char* text;
    size_t length;
    int wildcard;
} _RinGlobComponent;

/* Forward declaration used by the private brace collector.  The public
 * entrypoint recursively compiles each bounded alternative into a private
 * result before publishing anything to the caller's glob_t. */
static inline int glob(const char* pattern, int flags,
                       int (*errfunc)(const char*, int),
                       glob_t* pglob);
static inline void globfree(glob_t* pglob);

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

static inline int _rin_glob_class_match(const char** pattern, char value,
                                        int flags)
{
    const char* cursor = *pattern;
    int negate = 0;
    int matched = 0;
    int have_item = 0;
    if (*cursor == '^' || *cursor == '!') {
        negate = 1;
        ++cursor;
    }
    if (*cursor == ']') {
        have_item = 1;
        if (value == ']') matched = 1;
        ++cursor;
    }
    while (*cursor != '\0' && *cursor != ']') {
        unsigned char first;
        unsigned char last;
        if (*cursor == '\\' && !(flags & GLOB_NOESCAPE)) {
            if (cursor[1] == '\0') return -1;
            first = (unsigned char)cursor[1];
            cursor += 2;
        } else {
            first = (unsigned char)*cursor++;
        }
        have_item = 1;
        last = first;
        if (*cursor == '-' && cursor[1] != '\0' && cursor[1] != ']') {
            ++cursor;
            if (*cursor == '\\' && !(flags & GLOB_NOESCAPE)) {
                if (cursor[1] == '\0') return -1;
                last = (unsigned char)cursor[1];
                cursor += 2;
            } else {
                last = (unsigned char)*cursor++;
            }
            if (first > last) return -1;
        }
        if ((unsigned char)value >= first && (unsigned char)value <= last)
            matched = 1;
    }
    if (*cursor != ']' || !have_item) return -1;
    *pattern = cursor + 1u;
    return negate ? !matched : matched;
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
            if (_rin_glob_class_match(&class_end, '\0', flags) < 0)
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
    class_end = pattern + 1u;
    class_result = _rin_glob_class_match(&class_end, '.', flags);
    return class_result > 0;
}

static inline int _rin_glob_component_match(const char* pattern,
                                            const char* value, int flags)
{
    const char* left = pattern;
    const char* right = value;
    while (*left != '\0') {
        if (*left == '*') {
            ++left;
            while (*left == '*') ++left;
            if (*left == '\0') return 1;
            for (const char* probe = right; *probe != '\0'; ++probe)
                if (_rin_glob_component_match(left, probe, flags)) return 1;
            return _rin_glob_component_match(left, right, flags);
        }
        if (*right == '\0') return 0;
        if (*left == '?') {
            ++left;
            ++right;
            continue;
        }
        if (*left == '[') {
            int class_result;
            const char* class_end = left + 1u;
            class_result = _rin_glob_class_match(&class_end, *right, flags);
            if (class_result < 0) return 0;
            if (!class_result) return 0;
            left = class_end;
            ++right;
            continue;
        }
        if (*left == '\\' && !(flags & GLOB_NOESCAPE)) {
            if (left[1] == '\0') return 0;
            ++left;
        }
        if (*left++ != *right++) return 0;
    }
    return *right == '\0';
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
                                               const struct dirent* entry,
                                               int* is_directory)
{
    struct stat status;
    if (!path || !is_directory) return GLOB_ABORTED;
    if (entry && entry->d_type == DT_DIR) {
        *is_directory = 1;
        return 0;
    }
    if (entry && entry->d_type != DT_UNKNOWN) {
        *is_directory = 0;
        return 0;
    }
    if (_RIN_GLOB_STAT(path, &status) != 0) return GLOB_ABORTED;
    *is_directory = S_ISDIR(status.st_mode) ? 1 : 0;
    return 0;
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
    char marked[_RIN_GLOB_MAX_PATH];
    if ((flags & GLOB_MARK) && is_directory &&
        (length == 0u || path[length - 1u] != '/')) {
        if (length + 2u > sizeof(marked)) return GLOB_NOSPACE;
        for (size_t index = 0; index < length; ++index)
            marked[index] = path[index];
        marked[length++] = '/';
        marked[length] = '\0';
        return _rin_glob_match_list_add(list, marked);
    }
    return _rin_glob_match_list_add(list, path);
}

static inline int _rin_glob_expand(_RinGlobContext* context,
                                   const _RinGlobComponent* components,
                                   size_t component_count, size_t index,
                                   const char* prefix)
{
    char raw_component[NAME_MAX + 1u];
    char literal_component[NAME_MAX + 1u];
    char* child;
    int result;
    if (!context || !components || !prefix || index > component_count)
        return GLOB_ABORTED;
    if (index == component_count) {
        struct stat status;
        if (prefix[0] == '\0' || _RIN_GLOB_STAT(prefix, &status) != 0)
            return 0;
        if ((context->flags & GLOB_ONLYDIR) && !S_ISDIR(status.st_mode))
            return 0;
        return _rin_glob_add_marked(context->matches, prefix, context->flags,
                                    S_ISDIR(status.st_mode));
    }
    if (components[index].length >= sizeof(raw_component)) return GLOB_NOSPACE;
    child = (char*)malloc(_RIN_GLOB_MAX_PATH);
    if (!child) return GLOB_NOSPACE;
    for (size_t offset = 0; offset < components[index].length; ++offset)
        raw_component[offset] = components[index].text[offset];
    raw_component[components[index].length] = '\0';
    if (!components[index].wildcard) {
        result = _rin_glob_component_copy(raw_component,
                                           components[index].length,
                                           context->flags, literal_component,
                                           sizeof(literal_component));
        if (result != 0) {
            free(child);
            return result;
        }
        result = _rin_glob_path_join(prefix, literal_component, child,
                                     _RIN_GLOB_MAX_PATH);
        if (result == 0)
            result = _rin_glob_expand(context, components, component_count,
                                      index + 1u, child);
        free(child);
        return result;
    }
    {
        const char* directory = prefix[0] != '\0' ? prefix : ".";
        DIR* directory_stream = _RIN_GLOB_OPENDIR(directory);
        struct dirent* entry;
        int dot_allowed = _rin_glob_component_allows_period(raw_component,
                                                             context->flags);
        if (!directory_stream) {
            int saved_errno = errno;
            if (context->errfunc &&
                context->errfunc(directory, saved_errno) != 0)
                result = GLOB_ABORTED;
            else
                result = (context->flags & GLOB_ERR) ? GLOB_ABORTED : 0;
            free(child);
            return result;
        }
        result = 0;
        while ((entry = _RIN_GLOB_READDIR(directory_stream)) != NULL) {
            int is_directory = 0;
            if (entry->d_name[0] == '\0' ||
                (entry->d_name[0] == '.' &&
                 (entry->d_name[1] == '\0' ||
                  (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) ||
                (!dot_allowed && entry->d_name[0] == '.') ||
                !_rin_glob_component_match(raw_component, entry->d_name,
                                            context->flags))
                continue;
            result = _rin_glob_path_join(prefix, entry->d_name, child,
                                         _RIN_GLOB_MAX_PATH);
            if (result != 0) break;
            if (index + 1u < component_count) {
                result = _rin_glob_entry_is_directory(child, entry,
                                                      &is_directory);
                if (result != 0) {
                    if (context->flags & GLOB_ERR) break;
                    result = 0;
                    continue;
                }
                if (!is_directory) continue;
            }
            result = _rin_glob_expand(context, components, component_count,
                                      index + 1u, child);
            if (result != 0) break;
        }
        if (_RIN_GLOB_CLOSEDIR(directory_stream) != 0 && result == 0 &&
            (context->flags & GLOB_ERR))
            result = GLOB_ABORTED;
        free(child);
        return result;
    }
}

static inline int _rin_glob_string_compare(const char* left, const char* right)
{
    size_t index = 0u;
    while (left[index] && right[index] && left[index] == right[index]) ++index;
    return (unsigned char)left[index] - (unsigned char)right[index];
}

static inline void _rin_glob_sort(_RinGlobMatchList* list)
{
    if (!list) return;
    for (size_t index = 1u; index < list->count; ++index) {
        char* value = list->items[index];
        size_t cursor = index;
        while (cursor != 0u &&
               _rin_glob_string_compare(list->items[cursor - 1u], value) > 0) {
            list->items[cursor] = list->items[cursor - 1u];
            --cursor;
        }
        list->items[cursor] = value;
    }
}

static inline int _rin_glob_commit(glob_t* pglob, int flags,
                                   _RinGlobMatchList* matches)
{
    size_t offsets;
    size_t old_count;
    size_t total;
    char** paths;
    char** old_paths = pglob->gl_pathv;
    size_t old_offsets = pglob->gl_offs;
    size_t old_pathc = pglob->gl_pathc;
    int append = (flags & GLOB_APPEND) != 0;
    if (append && old_pathc != 0u && !old_paths) {
        errno = EINVAL;
        return GLOB_ABORTED;
    }
    offsets = append ? old_offsets :
              ((flags & GLOB_DOOFFS) ? pglob->gl_offs : 0u);
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
    if (!append && old_paths) {
        for (size_t index = 0; index < old_pathc; ++index)
            free(old_paths[old_offsets + index]);
    }
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

/* Locate the first unescaped brace pair containing a top-level comma.  The
 * scanner understands nested pairs so an alternative may itself contain a
 * brace expression.  A malformed (unclosed) brace is reported separately;
 * brace pairs without a comma remain literal, matching the conservative
 * bounded policy used by this header. */
static inline int _rin_glob_find_brace(const char* pattern,
                                       int flags, size_t* open,
                                       size_t* close, size_t* comma)
{
    size_t index = 0u;
    size_t opens[64];
    size_t commas[64];
    size_t nested_open = 0u;
    size_t nested_close = 0u;
    size_t nested_comma = (size_t)-1;
    unsigned int depth = 0u;
    if (!pattern || !open || !close || !comma) return -1;
    while (pattern[index] != '\0') {
        if (pattern[index] == '\\' && !(flags & GLOB_NOESCAPE)) {
            if (pattern[index + 1u] == '\0') return -1;
            index += 2u;
            continue;
        }
        if (pattern[index] == '{') {
            if (depth >= sizeof(opens) / sizeof(opens[0])) return -1;
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
                                         glob_t* pglob, int* handled)
{
    size_t open;
    size_t close;
    size_t comma;
    int found;
    glob_t aggregate = {0};
    size_t part_start;
    int aggregate_has_result = 0;
    int alternative_flags;
    _RinGlobMatchList matches = {NULL, 0u, 0u};

    if (!pattern || !pglob || !handled) return GLOB_ABORTED;
    *handled = 0;
    found = _rin_glob_find_brace(pattern, flags, &open, &close, &comma);
    if (found < 0) {
        *handled = 1;
        return GLOB_NOSYS;
    }
    if (found == 0) return 0;
    *handled = 1;

    /* Alternatives are collected without caller offsets or fallback
     * literals.  Those policies are applied once, after every branch has
     * completed, so an error cannot expose a partial result. */
    alternative_flags = flags & ~(GLOB_APPEND | GLOB_DOOFFS |
                                   GLOB_NOCHECK | GLOB_NOMAGIC);
    part_start = open + 1u;
    for (;;) {
        size_t part_end = part_start;
        size_t suffix_length = _rin_glob_strlen(pattern + close + 1u);
        size_t prefix_length = open;
        size_t part_length;
        size_t total_length;
        size_t cursor;
        char alternative[_RIN_GLOB_MAX_PATTERN];
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

        result = glob(alternative,
                      aggregate_has_result ? alternative_flags | GLOB_APPEND
                                           : alternative_flags,
                      errfunc, &aggregate);
        if (result == 0) {
            aggregate_has_result = 1;
        } else if (result != GLOB_NOMATCH) {
            globfree(&aggregate);
            return result;
        }
        if (part_end == close) break;
        part_start = part_end + 1u;
    }

    if (aggregate.gl_pathc == 0u) {
        globfree(&aggregate);
        if ((flags & GLOB_NOCHECK) != 0) {
            if (_rin_glob_match_list_add(&matches, pattern) != 0)
                return GLOB_NOSPACE;
        } else {
            return GLOB_NOMATCH;
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
    found = _rin_glob_commit(pglob, flags, &matches);
    if (found != 0) _rin_glob_match_list_free(&matches);
    return found;
}

/* Expand the current user's home directory without consulting an ambient
 * user database.  The target environment owns HOME; arbitrary ~user lookup
 * would otherwise turn an untrusted name into an authority-bearing path. */
static inline int _rin_glob_tilde_expand(const char* pattern, int flags,
                                         int (*errfunc)(const char*, int),
                                         glob_t* pglob, int* handled)
{
    const char* home;
    const char* suffix;
    size_t home_length;
    size_t suffix_length;
    size_t total_length;
    char expanded[_RIN_GLOB_MAX_PATTERN];
    size_t cursor;

    if (!pattern || !pglob || !handled) return GLOB_ABORTED;
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
    return glob(expanded, flags & ~GLOB_TILDE, errfunc, pglob);
}

/* Expand pathname components with bounded directory traversal. */
static inline int glob(const char* pattern, int flags,
                       int (*errfunc)(const char*, int),
                       glob_t* pglob) {
    const int known_flags = GLOB_ERR | GLOB_MARK | GLOB_NOSORT |
                            GLOB_DOOFFS | GLOB_NOCHECK | GLOB_APPEND |
                            GLOB_NOESCAPE | GLOB_PERIOD | GLOB_BRACE |
                            GLOB_NOMAGIC | GLOB_TILDE | GLOB_ONLYDIR;
    size_t pattern_length;
    size_t component_count = 0u;
    size_t cursor = 0u;
    size_t old_pathc;
    int has_wildcard = 0;
    char prefix[_RIN_GLOB_MAX_PATH];
    _RinGlobComponent components[_RIN_GLOB_MAX_COMPONENTS];
    _RinGlobMatchList matches = { NULL, 0u, 0u };
    _RinGlobContext context;

    if (!pglob || !pattern) {
        errno = EINVAL;
        return GLOB_ABORTED;
    }
    if ((flags & ~known_flags) != 0)
        return GLOB_NOSYS;
    {
        int tilde_handled = 0;
        int tilde_result = _rin_glob_tilde_expand(
            pattern, flags, errfunc, pglob, &tilde_handled);
        if (tilde_handled) return tilde_result;
    }
    if (flags & GLOB_BRACE) {
        int brace_handled = 0;
        int brace_result = _rin_glob_brace_expand(
            pattern, flags, errfunc, pglob, &brace_handled);
        if (brace_handled) return brace_result;
    }
    pattern_length = _rin_glob_strlen(pattern);
    if (pattern_length == 0u || pattern_length >= _RIN_GLOB_MAX_PATTERN)
        return pattern_length == 0u ? GLOB_NOMATCH : GLOB_NOSPACE;
    prefix[0] = '\0';
    if (pattern[0] == '/') {
        prefix[0] = '/';
        prefix[1] = '\0';
        cursor = 1u;
    }
    while (cursor < pattern_length) {
        size_t begin;
        size_t length;
        char component_copy[_RIN_GLOB_MAX_PATH];
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
        if (length == 0u || length >= sizeof(component_copy))
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
    if (pattern[pattern_length - 1u] == '/' && prefix[0] != '/')
        return GLOB_NOSYS;
    context.flags = flags;
    context.errfunc = errfunc;
    context.matches = &matches;
    old_pathc = (flags & GLOB_APPEND) ? pglob->gl_pathc : 0u;
    if ((flags & GLOB_APPEND) && old_pathc != 0u && !pglob->gl_pathv) {
        errno = EINVAL;
        return GLOB_ABORTED;
    }
    {
        int result = _rin_glob_expand(&context, components, component_count,
                                      0u, prefix);
        if (result != 0) {
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
            return GLOB_NOMATCH;
        }
    }
    if (!(flags & GLOB_NOSORT)) _rin_glob_sort(&matches);
    {
        int result = _rin_glob_commit(pglob, flags, &matches);
        if (result != 0) _rin_glob_match_list_free(&matches);
        return result;
    }
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
