/*
 * RinOS libc - libgen.h
 * パス名操作
 */

#ifndef _LIBGEN_H
#define _LIBGEN_H

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * パス名操作関数
 * ═══════════════════════════════════════════════════════════════*/

/* The dot fallbacks live in thread-local storage. */
#if defined(__cplusplus)
#define RIN_LIBGEN_THREAD_LOCAL thread_local
#else
#define RIN_LIBGEN_THREAD_LOCAL _Thread_local
#endif
static RIN_LIBGEN_THREAD_LOCAL char _basename_buf[256];
static RIN_LIBGEN_THREAD_LOCAL char _dirname_buf[256];
#undef RIN_LIBGEN_THREAD_LOCAL

/* basename - パスからファイル名部分を取得
 * "/usr/lib" -> "lib"
 * "/usr/"    -> "usr"
 * "usr"      -> "usr"
 * "/"        -> "/"
 * "."        -> "."
 * ".."       -> ".."
 * NULL or "" -> "."
 * A non-empty path may be modified, as permitted by the POSIX interface.
 */
static inline char* basename(char* path) {
    if (path == NULL || path[0] == '\0') {
        _basename_buf[0] = '.';
        _basename_buf[1] = '\0';
        return _basename_buf;
    }

    char* end = path;
    while (*end != '\0') ++end;
    while (end > path && end[-1] == '/') --end;

    /* The path consists only of root separators. */
    if (end == path) {
        path[1] = '\0';
        return path;
    }

    /* Terminate trailing separators, then return the final component in-place. */
    *end = '\0';
    char* component = end;
    while (component > path && component[-1] != '/') --component;
    return component;
}

/* dirname - パスからディレクトリ部分を取得
 * "/usr/lib" -> "/usr"
 * "/usr/"    -> "/"
 * "usr"      -> "."
 * "/"        -> "/"
 * "."        -> "."
 * ".."       -> "."
 * NULL or "" -> "."
 * A non-empty path may be modified, as permitted by the POSIX interface.
 */
static inline char* dirname(char* path) {
    if (path == NULL || path[0] == '\0') {
        _dirname_buf[0] = '.';
        _dirname_buf[1] = '\0';
        return _dirname_buf;
    }

    char* end = path;
    while (*end != '\0') ++end;
    while (end > path && end[-1] == '/') --end;

    /* The path consists only of root separators. */
    if (end == path) {
        path[1] = '\0';
        return path;
    }

    char* component = end;
    while (component > path && component[-1] != '/') --component;
    if (component == path) {
        _dirname_buf[0] = '.';
        _dirname_buf[1] = '\0';
        return _dirname_buf;
    }

    /* Remove the separator run before the final component. */
    char* separator = component - 1;
    while (separator > path && separator[-1] == '/') --separator;
    if (separator == path) {
        path[1] = '\0';
    } else {
        *separator = '\0';
    }
    return path;
}

#ifdef __cplusplus
}
#endif

#endif /* _LIBGEN_H */
