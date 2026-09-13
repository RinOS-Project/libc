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

/*
 * Returned strings live in scratch storage.  Keep that storage local to the
 * calling thread so a concurrent basename()/dirname() call cannot overwrite
 * a pointer that the caller is still using.
 */
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
 */
static inline char* basename(char* path) {
    if (path == NULL || path[0] == '\0') {
        _basename_buf[0] = '.';
        _basename_buf[1] = '\0';
        return _basename_buf;
    }

    /* 末尾のスラッシュを除去 */
    int len = 0;
    while (path[len]) len++;
    while (len > 1 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        len--;
    }

    /* 最後のスラッシュを探す */
    int last_slash = -1;
    for (int i = 0; i < len; i++) {
        if (path[i] == '/' || path[i] == '\\') {
            last_slash = i;
        }
    }

    /* ルートのみの場合 */
    if (last_slash == 0 && len == 1) {
        _basename_buf[0] = '/';
        _basename_buf[1] = '\0';
        return _basename_buf;
    }

    /* ファイル名部分をコピー */
    const char* start = (last_slash >= 0) ? path + last_slash + 1 : path;
    int copy_len = (last_slash >= 0) ? len - last_slash - 1 : len;

    if (copy_len >= (int)sizeof(_basename_buf)) {
        copy_len = sizeof(_basename_buf) - 1;
    }

    for (int i = 0; i < copy_len; i++) {
        _basename_buf[i] = start[i];
    }
    _basename_buf[copy_len] = '\0';

    return _basename_buf;
}

/* dirname - パスからディレクトリ部分を取得
 * "/usr/lib" -> "/usr"
 * "/usr/"    -> "/"
 * "usr"      -> "."
 * "/"        -> "/"
 * "."        -> "."
 * ".."       -> "."
 * NULL or "" -> "."
 */
static inline char* dirname(char* path) {
    if (path == NULL || path[0] == '\0') {
        _dirname_buf[0] = '.';
        _dirname_buf[1] = '\0';
        return _dirname_buf;
    }

    /* パスをコピー */
    int len = 0;
    while (path[len] && len < (int)sizeof(_dirname_buf) - 1) {
        _dirname_buf[len] = path[len];
        len++;
    }
    _dirname_buf[len] = '\0';

    /* 末尾のスラッシュを除去 */
    while (len > 1 && (_dirname_buf[len - 1] == '/' || _dirname_buf[len - 1] == '\\')) {
        len--;
        _dirname_buf[len] = '\0';
    }

    /* 最後のスラッシュを探す */
    int last_slash = -1;
    for (int i = 0; i < len; i++) {
        if (_dirname_buf[i] == '/' || _dirname_buf[i] == '\\') {
            last_slash = i;
        }
    }

    if (last_slash < 0) {
        /* スラッシュがない場合 */
        _dirname_buf[0] = '.';
        _dirname_buf[1] = '\0';
    } else if (last_slash == 0) {
        /* ルートディレクトリ */
        _dirname_buf[0] = '/';
        _dirname_buf[1] = '\0';
    } else {
        /* スラッシュの直前まで */
        _dirname_buf[last_slash] = '\0';
    }

    return _dirname_buf;
}

#ifdef __cplusplus
}
#endif

#endif /* _LIBGEN_H */
