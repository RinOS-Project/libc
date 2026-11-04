/*
 * RinOS libc - GNU argp command-line parser interface.
 * The ABI follows GNU libc's public argp structures and constants.
 */

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
#include_next <argp.h>
#else

#ifndef _ARGP_H
#define _ARGP_H

#include "features.h"

#if defined(_RIN_USE_GNU_ARGP) || defined(RIN_ARGP_IMPLEMENTATION)

#include "errno.h"
#include "stdio.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __error_t_defined
typedef int error_t;
#define __error_t_defined 1
#endif

struct argp;
struct argp_state;
struct argp_child;

struct argp_option {
    const char *name;
    int key;
    const char *arg;
    int flags;
    const char *doc;
    int group;
};

#define OPTION_ARG_OPTIONAL 0x1
#define OPTION_HIDDEN       0x2
#define OPTION_ALIAS        0x4
#define OPTION_DOC          0x8
#define OPTION_NO_USAGE     0x10

typedef error_t (*argp_parser_t)(int key, char *arg,
                                 struct argp_state *state);

/* E2BIG is reserved by GNU argp as the callback's "not handled" result. */
#define ARGP_ERR_UNKNOWN E2BIG

#define ARGP_KEY_ARG       0
#define ARGP_KEY_END       0x1000001
#define ARGP_KEY_NO_ARGS   0x1000002
#define ARGP_KEY_INIT      0x1000003
#define ARGP_KEY_SUCCESS   0x1000004
#define ARGP_KEY_ERROR     0x1000005
#define ARGP_KEY_ARGS      0x1000006
#define ARGP_KEY_FINI      0x1000007

#define ARGP_KEY_HELP_PRE_DOC       0x2000001
#define ARGP_KEY_HELP_POST_DOC      0x2000002
#define ARGP_KEY_HELP_HEADER        0x2000003
#define ARGP_KEY_HELP_EXTRA         0x2000004
#define ARGP_KEY_HELP_DUP_ARGS_NOTE 0x2000005
#define ARGP_KEY_HELP_ARGS_DOC      0x2000006

struct argp_child {
    const struct argp *argp;
    int flags;
    const char *header;
    int group;
};

struct argp {
    const struct argp_option *options;
    argp_parser_t parser;
    const char *args_doc;
    const char *doc;
    const struct argp_child *children;
    char *(*help_filter)(int key, const char *text, void *input);
    const char *argp_domain;
};

struct argp_state {
    const struct argp *root_argp;
    int argc;
    char **argv;
    int next;
    unsigned flags;
    unsigned arg_num;
    int quoted;
    void *input;
    void **child_inputs;
    void *hook;
    char *name;
    FILE *err_stream;
    FILE *out_stream;
    void *pstate;
};

#define ARGP_PARSE_ARGV0 0x01
#define ARGP_NO_ERRS      0x02
#define ARGP_NO_ARGS      0x04
#define ARGP_IN_ORDER     0x08
#define ARGP_NO_HELP      0x10
#define ARGP_NO_EXIT      0x20
#define ARGP_LONG_ONLY    0x40
#define ARGP_SILENT (ARGP_NO_EXIT | ARGP_NO_ERRS | ARGP_NO_HELP)

#define ARGP_HELP_USAGE       0x01
#define ARGP_HELP_SHORT_USAGE 0x02
#define ARGP_HELP_SEE         0x04
#define ARGP_HELP_LONG        0x08
#define ARGP_HELP_PRE_DOC     0x10
#define ARGP_HELP_POST_DOC    0x20
#define ARGP_HELP_DOC (ARGP_HELP_PRE_DOC | ARGP_HELP_POST_DOC)
#define ARGP_HELP_BUG_ADDR    0x40
#define ARGP_HELP_LONG_ONLY   0x80
#define ARGP_HELP_EXIT_ERR    0x100
#define ARGP_HELP_EXIT_OK     0x200
#define ARGP_HELP_STD_ERR \
    (ARGP_HELP_SEE | ARGP_HELP_EXIT_ERR)
#define ARGP_HELP_STD_USAGE \
    (ARGP_HELP_SHORT_USAGE | ARGP_HELP_SEE | ARGP_HELP_EXIT_ERR)
#define ARGP_HELP_STD_HELP \
    (ARGP_HELP_SHORT_USAGE | ARGP_HELP_LONG | ARGP_HELP_EXIT_OK | \
     ARGP_HELP_DOC | ARGP_HELP_BUG_ADDR)

error_t argp_parse(const struct argp *argp, int argc, char **argv,
                   unsigned flags, int *arg_index, void *input);
error_t __argp_parse(const struct argp *argp, int argc, char **argv,
                     unsigned flags, int *arg_index, void *input);

extern const char *argp_program_version;
extern void (*argp_program_version_hook)(FILE *stream,
                                         struct argp_state *state);
extern const char *argp_program_bug_address;
extern error_t argp_err_exit_status;

void argp_help(const struct argp *argp, FILE *stream, unsigned flags,
               char *name);
void __argp_help(const struct argp *argp, FILE *stream, unsigned flags,
                 char *name);
void argp_state_help(const struct argp_state *state, FILE *stream,
                     unsigned flags);
void __argp_state_help(const struct argp_state *state, FILE *stream,
                       unsigned flags);
void argp_usage(const struct argp_state *state);
void __argp_usage(const struct argp_state *state);
void argp_error(const struct argp_state *state, const char *format, ...)
    __attribute__((format(printf, 2, 3)));
void __argp_error(const struct argp_state *state, const char *format, ...)
    __attribute__((format(printf, 2, 3)));
void argp_failure(const struct argp_state *state, int status, int errnum,
                  const char *format, ...)
    __attribute__((format(printf, 4, 5)));
void __argp_failure(const struct argp_state *state, int status, int errnum,
                    const char *format, ...)
    __attribute__((format(printf, 4, 5)));
int _option_is_short(const struct argp_option *option);
int __option_is_short(const struct argp_option *option);
int _option_is_end(const struct argp_option *option);
int __option_is_end(const struct argp_option *option);
void *_argp_input(const struct argp *argp, const struct argp_state *state);
void *__argp_input(const struct argp *argp, const struct argp_state *state);

#ifdef __cplusplus
}
#endif

#endif /* GNU argp feature surface */
#endif /* _ARGP_H */

#endif /* hosted Linux uses its native argp interface */
