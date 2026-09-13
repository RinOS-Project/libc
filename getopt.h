/*
 * RinOS libc - getopt.h
 * コマンドライン引数解析
 */

#ifndef _GETOPT_H
#define _GETOPT_H

#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * グローバル変数
 * ═══════════════════════════════════════════════════════════════*/

/* POSIX getopt state is process-owned.  Keep one libc definition instead of
 * a header-local copy that gives every translation unit a different cursor. */
extern char* optarg;
extern int optind;
extern int opterr;
extern int optopt;
extern int optreset;

/* ═══════════════════════════════════════════════════════════════
 * getopt_long用構造体
 * ═══════════════════════════════════════════════════════════════*/

struct option {
    const char* name;     /* 長いオプション名 */
    int         has_arg;  /* 引数要求 */
    int*        flag;     /* フラグへのポインタ (NULL = 値を返す) */
    int         val;      /* flagがNULLの時の戻り値、またはflagに設定する値 */
};

/* has_argの値 */
#define no_argument       0
#define required_argument 1
#define optional_argument 2

/* ═══════════════════════════════════════════════════════════════
 * 内部状態
 * ═══════════════════════════════════════════════════════════════*/

extern int _rin_getopt_pos;  /* 現在のargv[optind]内の位置 */

/* ═══════════════════════════════════════════════════════════════
 * getopt関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int getopt(int argc, char* const argv[], const char* optstring) {
    if (optreset) {
        optind = 1;
        _rin_getopt_pos = 0;
        optreset = 0;
    }

    if (optind >= argc) return -1;

    const char* arg = argv[optind];
    if (!arg) return -1;

    /* オプションではない */
    if (arg[0] != '-' || arg[1] == '\0') return -1;

    /* "--" は終了 */
    if (arg[1] == '-' && arg[2] == '\0') {
        optind++;
        return -1;
    }

    /* 現在位置を進める */
    if (_rin_getopt_pos == 0) _rin_getopt_pos = 1;

    char c = arg[_rin_getopt_pos++];

    /* 次の文字がない場合、次のargvへ */
    if (arg[_rin_getopt_pos] == '\0') {
        optind++;
        _rin_getopt_pos = 0;
    }

    /* optstring内でオプションを検索 */
    const char* p = optstring;
    if (*p == ':' || *p == '+' || *p == '-') p++;  /* 特殊接頭辞をスキップ */

    while (*p) {
        if (*p == c) {
            /* オプションが見つかった */
            if (p[1] == ':') {
                /* 引数が必要 */
                if (_rin_getopt_pos > 0 && arg[_rin_getopt_pos] != '\0') {
                    /* 同じargvに引数がある */
                    optarg = (char*)&arg[_rin_getopt_pos];
                    optind++;
                    _rin_getopt_pos = 0;
                } else if (optind < argc) {
                    /* 次のargvが引数 */
                    optarg = argv[optind++];
                } else if (p[2] != ':') {
                    /* 必須引数がない */
                    optopt = c;
                    if (optstring[0] == ':') return ':';
                    return '?';
                }
            }
            return c;
        }
        p++;
        if (*p == ':') p++;  /* :をスキップ */
        if (*p == ':') p++;  /* ::もスキップ */
    }

    /* オプションが見つからない */
    optopt = c;
    return '?';
}

/* ═══════════════════════════════════════════════════════════════
 * getopt_long関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int getopt_long(int argc, char* const argv[],
                              const char* optstring,
                              const struct option* longopts,
                              int* longindex) {
    if (optreset) {
        optind = 1;
        _rin_getopt_pos = 0;
        optreset = 0;
    }

    if (optind >= argc) return -1;

    const char* arg = argv[optind];
    if (!arg) return -1;

    /* オプションではない */
    if (arg[0] != '-') return -1;

    /* "--" は終了 */
    if (arg[1] == '-' && arg[2] == '\0') {
        optind++;
        return -1;
    }

    /* 長いオプション "--xxx" */
    if (arg[1] == '-' && longopts) {
        const char* name = arg + 2;
        int name_len = 0;
        const char* eq = NULL;

        /* =を探す */
        while (name[name_len] && name[name_len] != '=') name_len++;
        if (name[name_len] == '=') eq = name + name_len + 1;

        /* longoptsを検索 */
        for (int i = 0; longopts[i].name; i++) {
            const char* opt_name = longopts[i].name;
            int match = 1;

            /* 名前を比較 */
            for (int j = 0; j < name_len; j++) {
                if (opt_name[j] != name[j]) {
                    match = 0;
                    break;
                }
            }
            if (match && opt_name[name_len] != '\0') {
                match = 0;  /* 部分一致は不可 */
            }

            if (match) {
                optind++;
                if (longindex) *longindex = i;

                /* 引数処理 */
                if (longopts[i].has_arg == required_argument ||
                    longopts[i].has_arg == optional_argument) {
                    if (eq) {
                        optarg = (char*)eq;
                    } else if (longopts[i].has_arg == required_argument && optind < argc) {
                        optarg = argv[optind++];
                    } else if (longopts[i].has_arg == required_argument) {
                        optopt = longopts[i].val;
                        if (optstring && optstring[0] == ':') return ':';
                        return '?';
                    }
                }

                if (longopts[i].flag) {
                    *longopts[i].flag = longopts[i].val;
                    return 0;
                }
                return longopts[i].val;
            }
        }

        /* 見つからない */
        optind++;
        return '?';
    }

    /* 短いオプション */
    return getopt(argc, argv, optstring);
}

/* ═══════════════════════════════════════════════════════════════
 * getopt_long_only関数
 * -xxxを長いオプションとして解釈
 * ═══════════════════════════════════════════════════════════════*/

static inline int getopt_long_only(int argc, char* const argv[],
                                   const char* optstring,
                                   const struct option* longopts,
                                   int* longindex) {
    if (optind >= argc) return -1;

    const char* arg = argv[optind];
    if (!arg || arg[0] != '-' || arg[1] == '\0') return -1;

    /* -xxxを--xxxとして処理を試みる */
    if (arg[1] != '-' && longopts) {
        const char* name = arg + 1;
        int name_len = 0;
        while (name[name_len] && name[name_len] != '=') name_len++;

        /* 長いオプションとして検索 */
        for (int i = 0; longopts[i].name; i++) {
            const char* opt_name = longopts[i].name;
            int match = 1;
            for (int j = 0; j < name_len; j++) {
                if (opt_name[j] != name[j]) {
                    match = 0;
                    break;
                }
            }
            if (match && opt_name[name_len] == '\0') {
                /* 長いオプションとして処理 */
                return getopt_long(argc, argv, optstring, longopts, longindex);
            }
        }
    }

    /* 通常のgetopt_long */
    return getopt_long(argc, argv, optstring, longopts, longindex);
}

#ifdef __cplusplus
}
#endif

#endif /* _GETOPT_H */
