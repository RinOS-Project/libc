/*
 * RinOS libc - getopt.h
 * コマンドライン引数解析
 */

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
#include_next <getopt.h>
#else

#ifndef _GETOPT_H
#define _GETOPT_H

#include "stddef.h"
#include "stdio.h"

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

static inline int _rin_getopt_match_long_option(
    const struct option* longopts, const char* name, int name_len,
    int* ambiguous_out) {
    int prefix_index = -1;
    int prefix_count = 0;
    if (ambiguous_out) *ambiguous_out = 0;
    if (!longopts || !name || !ambiguous_out || name_len <= 0) return -1;
    for (int index = 0; longopts[index].name; ++index) {
        const char* option_name = longopts[index].name;
        int matched = 0;
        while (matched < name_len && option_name[matched] != '\0' &&
               option_name[matched] == name[matched]) {
            ++matched;
        }
        if (matched != name_len) continue;
        if (option_name[matched] == '\0') return index;
        prefix_index = index;
        ++prefix_count;
    }
    if (prefix_count == 1) return prefix_index;
    if (prefix_count > 1) *ambiguous_out = 1;
    return -1;
}

/* has_argの値 */
#define no_argument       0
#define required_argument 1
#define optional_argument 2

/* ═══════════════════════════════════════════════════════════════
 * 内部状態
 * ═══════════════════════════════════════════════════════════════*/

extern int _rin_getopt_pos;  /* 現在のargv[optind]内の位置 */
extern int _rin_getopt_first_nonopt;
extern int _rin_getopt_last_nonopt;
extern int _rin_getopt_ordering;
extern int _rin_getopt_initialized;
extern int _rin_getopt_finished;
int _rin_getopt_posixly_correct(void);

enum {
    _RIN_GETOPT_REQUIRE_ORDER = 0,
    _RIN_GETOPT_PERMUTE = 1,
    _RIN_GETOPT_RETURN_IN_ORDER = 2
};

static inline int _rin_getopt_is_nonoption(const char* argument) {
    return !argument || argument[0] != '-' || argument[1] == '\0';
}

static inline int _rin_getopt_has_missing_argument_colon(
    const char* optstring) {
    if (!optstring) return 0;
    if (optstring[0] == '+' || optstring[0] == '-') ++optstring;
    return optstring[0] == ':';
}

static inline const char* _rin_getopt_short_options(
    const char* optstring) {
    if (!optstring) return "";
    if (optstring[0] == '+' || optstring[0] == '-') ++optstring;
    if (optstring[0] == ':') ++optstring;
    return optstring;
}

static inline void _rin_getopt_initialize(const char* optstring) {
    if (!optreset && optind != 0 && _rin_getopt_initialized &&
        !(_rin_getopt_finished && optind == 1)) return;
    optind = 1;
    _rin_getopt_pos = 0;
    _rin_getopt_first_nonopt = 1;
    _rin_getopt_last_nonopt = 1;
    _rin_getopt_finished = 0;
    optreset = 0;
    if (optstring && optstring[0] == '+') {
        _rin_getopt_ordering = _RIN_GETOPT_REQUIRE_ORDER;
    } else if (optstring && optstring[0] == '-') {
        _rin_getopt_ordering = _RIN_GETOPT_RETURN_IN_ORDER;
    } else {
        _rin_getopt_ordering = _rin_getopt_posixly_correct()
            ? _RIN_GETOPT_REQUIRE_ORDER : _RIN_GETOPT_PERMUTE;
    }
    _rin_getopt_initialized = 1;
}

/* Exchange argv[first_nonopt,last_nonopt) with the intervening option block.
 * The block-swap rotation is stable and uses no allocation. */
static inline void _rin_getopt_exchange(char* const argv[], int optind_value) {
    char** mutable_argv = (char**)argv;
    int bottom = _rin_getopt_first_nonopt;
    int middle = _rin_getopt_last_nonopt;
    int top = optind_value;
    int nonoptions = middle - bottom;
    int options = top - middle;
    while (nonoptions > 0 && options > 0) {
        if (options > nonoptions) {
            int offset = top - nonoptions;
            for (int index = 0; index < nonoptions; ++index) {
                char* value = mutable_argv[bottom + index];
                mutable_argv[bottom + index] = mutable_argv[offset + index];
                mutable_argv[offset + index] = value;
            }
            top -= nonoptions;
            options -= nonoptions;
        } else {
            for (int index = 0; index < options; ++index) {
                char* value = mutable_argv[bottom + index];
                mutable_argv[bottom + index] = mutable_argv[middle + index];
                mutable_argv[middle + index] = value;
            }
            bottom += options;
            nonoptions -= options;
        }
    }
    _rin_getopt_first_nonopt += optind_value - _rin_getopt_last_nonopt;
    _rin_getopt_last_nonopt = optind_value;
}

/* Return 0 at end, 1 for an option, or 2 for a return-in-order operand. */
static inline int _rin_getopt_prepare(int argc, char* const argv[],
                                      const char* optstring) {
    _rin_getopt_initialize(optstring);
    if (optind < 1) optind = 1;
    if (!argv) return 0;
    if (_rin_getopt_finished) return 0;
    if (_rin_getopt_ordering != _RIN_GETOPT_PERMUTE && optind >= argc)
        return 0;

    if (_rin_getopt_ordering == _RIN_GETOPT_PERMUTE) {
        if (_rin_getopt_pos == 0) {
            if (_rin_getopt_first_nonopt != _rin_getopt_last_nonopt &&
                _rin_getopt_last_nonopt != optind) {
                _rin_getopt_exchange(argv, optind);
            } else if (_rin_getopt_last_nonopt != optind) {
                _rin_getopt_first_nonopt = optind;
            }
            while (optind < argc &&
                   _rin_getopt_is_nonoption(argv[optind]))
                ++optind;
            _rin_getopt_last_nonopt = optind;
        }

        if (optind < argc && argv[optind] &&
            argv[optind][0] == '-' && argv[optind][1] == '-' &&
            argv[optind][2] == '\0') {
            ++optind;
            if (_rin_getopt_first_nonopt != _rin_getopt_last_nonopt &&
                _rin_getopt_last_nonopt != optind) {
                _rin_getopt_exchange(argv, optind);
            } else if (_rin_getopt_first_nonopt ==
                       _rin_getopt_last_nonopt) {
                _rin_getopt_first_nonopt = optind;
            }
            _rin_getopt_last_nonopt = argc;
            optind = argc;
        }
        if (optind >= argc) {
            if (_rin_getopt_first_nonopt != _rin_getopt_last_nonopt)
                optind = _rin_getopt_first_nonopt;
            _rin_getopt_finished = 1;
            return 0;
        }
        return 1;
    }

    if (argv[optind] && argv[optind][0] == '-' &&
        argv[optind][1] == '-' && argv[optind][2] == '\0') {
        ++optind;
        _rin_getopt_finished = 1;
        return 0;
    }
    if (_rin_getopt_is_nonoption(argv[optind])) {
        if (_rin_getopt_ordering == _RIN_GETOPT_RETURN_IN_ORDER)
            return 2;
        _rin_getopt_finished = 1;
        return 0;
    }
    if (optind >= argc) _rin_getopt_finished = 1;
    return 1;
}

static inline void _rin_getopt_diagnostic(char* const argv[],
                                          const char* message, int option) {
    if (!opterr) return;
    const char* program = argv && argv[0] ? argv[0] : "?";
    fprintf(stderr, "%s: %s -- '%c'\n", program, message, option);
}

static inline void _rin_getopt_long_diagnostic(char* const argv[],
                                               const char* message,
                                               const char* argument) {
    if (!opterr) return;
    const char* program = argv && argv[0] ? argv[0] : "?";
    fprintf(stderr, "%s: %s -- '%s'\n", program, message,
            argument ? argument : "?");
}

static inline int _rin_getopt_return_nonoption(char* const argv[],
                                                const char* optstring) {
    if (!optstring || optstring[0] != '-') return -1;
    optarg = argv[optind++];
    _rin_getopt_pos = 0;
    return 1;
}

/* ═══════════════════════════════════════════════════════════════
 * getopt関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int getopt(int argc, char* const argv[], const char* optstring) {
    optarg = NULL;
    int prepared = _rin_getopt_prepare(argc, argv, optstring);
    if (prepared == 0) return -1;
    if (prepared == 2) return _rin_getopt_return_nonoption(argv, optstring);

    const char* arg = argv[optind];
    if (!arg) return -1;

    /* オプションではない */
    if (arg[0] != '-' || arg[1] == '\0')
        return _rin_getopt_return_nonoption(argv, optstring);

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
    const char* p = _rin_getopt_short_options(optstring);

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
                } else if (p[2] != ':' && optind < argc) {
                    /* 次のargvが引数 */
                    optarg = argv[optind++];
                } else if (p[2] != ':') {
                    /* 必須引数がない */
                    optopt = c;
                    if (_rin_getopt_has_missing_argument_colon(optstring))
                        return ':';
                    _rin_getopt_diagnostic(argv,
                                           "option requires an argument", c);
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
    if (!_rin_getopt_has_missing_argument_colon(optstring))
        _rin_getopt_diagnostic(argv, "invalid option", c);
    return '?';
}

/* ═══════════════════════════════════════════════════════════════
 * getopt_long関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int getopt_long(int argc, char* const argv[],
                              const char* optstring,
                              const struct option* longopts,
                              int* longindex) {
    optarg = NULL;
    int prepared = _rin_getopt_prepare(argc, argv, optstring);
    if (prepared == 0) return -1;
    if (prepared == 2) return _rin_getopt_return_nonoption(argv, optstring);

    const char* arg = argv[optind];
    if (!arg) return -1;

    /* オプションではない */
    if (arg[0] != '-')
        return _rin_getopt_return_nonoption(argv, optstring);

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

        /* 完全一致を優先し、一意な前方一致だけを受理する。 */
        int ambiguous = 0;
        int option_index = _rin_getopt_match_long_option(
            longopts, name, name_len, &ambiguous);
        if (option_index >= 0) {
                int i = option_index;
                optind++;
                if (longindex) *longindex = i;

                /* 引数処理 */
                if (longopts[i].has_arg == no_argument && eq) {
                    optopt = longopts[i].val;
                    _rin_getopt_long_diagnostic(
                        argv, "option does not allow an argument", arg);
                    return '?';
                }
                if (longopts[i].has_arg == required_argument ||
                    longopts[i].has_arg == optional_argument) {
                    if (eq) {
                        optarg = (char*)eq;
                    } else if (longopts[i].has_arg == required_argument && optind < argc) {
                        optarg = argv[optind++];
                    } else if (longopts[i].has_arg == required_argument) {
                        optopt = longopts[i].val;
                        if (_rin_getopt_has_missing_argument_colon(optstring))
                            return ':';
                        _rin_getopt_long_diagnostic(
                            argv, "option requires an argument", arg);
                        return '?';
                    }
                }

                if (longopts[i].flag) {
                    *longopts[i].flag = longopts[i].val;
                    return 0;
                }
                return longopts[i].val;
        }

        if (ambiguous) {
            optind++;
            optopt = 0;
            _rin_getopt_long_diagnostic(argv, "option is ambiguous", arg);
            return '?';
        }
        /* 見つからない */
        optind++;
        optopt = 0;
        _rin_getopt_long_diagnostic(argv, "unrecognized option", arg);
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
    optarg = NULL;
    int prepared = _rin_getopt_prepare(argc, argv, optstring);
    if (prepared == 0) return -1;
    if (prepared == 2) return _rin_getopt_return_nonoption(argv, optstring);

    const char* arg = argv[optind];
    if (!arg) return -1;
    if (arg[0] != '-' || arg[1] == '\0')
        return _rin_getopt_return_nonoption(argv, optstring);

    /* GNU getopt_long_only gives a matching short option precedence. */
    if (arg[1] != '-' && longopts && optstring) {
        const char* short_options = _rin_getopt_short_options(optstring);
        int short_match = 0;
        for (const char* option = short_options; *option; ++option) {
            if (*option == arg[1]) {
                short_match = 1;
                break;
            }
            if (option[1] == ':') ++option;
            if (option[1] == ':') ++option;
        }
        if (short_match) return getopt_long(argc, argv, optstring,
                                            longopts, longindex);

        const char* name = arg + 1;
        int name_len = 0;
        const char* eq = NULL;
        while (name[name_len] && name[name_len] != '=') ++name_len;
        if (name[name_len] == '=') eq = name + name_len + 1;

        int ambiguous = 0;
        int option_index = _rin_getopt_match_long_option(
            longopts, name, name_len, &ambiguous);
        if (option_index >= 0) {
            int i = option_index;
            ++optind;
            _rin_getopt_pos = 0;
            if (longindex) *longindex = i;
            if (longopts[i].has_arg == no_argument && eq) {
                optopt = longopts[i].val;
                _rin_getopt_long_diagnostic(
                    argv, "option does not allow an argument", arg);
                return '?';
            }
            if (longopts[i].has_arg == required_argument ||
                longopts[i].has_arg == optional_argument) {
                if (eq) {
                    optarg = (char*)eq;
                } else if (longopts[i].has_arg == required_argument &&
                           optind < argc) {
                    optarg = argv[optind++];
                } else if (longopts[i].has_arg == required_argument) {
                    optopt = longopts[i].val;
                    if (_rin_getopt_has_missing_argument_colon(optstring))
                        return ':';
                    _rin_getopt_long_diagnostic(
                        argv, "option requires an argument", arg);
                    return '?';
                }
            }
            if (longopts[i].flag) {
                *longopts[i].flag = longopts[i].val;
                return 0;
            }
            return longopts[i].val;
        }
        if (ambiguous) {
            ++optind;
            optopt = 0;
            _rin_getopt_long_diagnostic(argv, "option is ambiguous", arg);
            return '?';
        }
    }

    /* Preserve short-option clusters and unknown-option handling. */
    return getopt_long(argc, argv, optstring, longopts, longindex);
}

#ifdef __cplusplus
}
#endif

#endif /* _GETOPT_H */

#endif /* hosted Linux delegates to its native option parser */
