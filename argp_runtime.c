/*
 * RinOS libc - GNU argp parser runtime.
 *
 * Keep parser state private to each argp_parse invocation.  In particular,
 * this runtime deliberately does not use getopt's process-global optind and
 * optarg state, so invoking argp and getopt in one process cannot reset or
 * reorder the other's cursor.
 */
#define RIN_ARGP_IMPLEMENTATION 1
#include "argp.h"

#include "errno.h"
#include "stdarg.h"
#include "stdlib.h"
#include "string.h"

#define RIN_ARGP_MAX_PARSERS 256u
#define RIN_ARGP_MAX_DEPTH 64u
#define RIN_ARGP_MAX_OPTIONS 4096u
#define RIN_ARGP_MAX_ARGC 1048576

const char *argp_program_version = NULL;
void (*argp_program_version_hook)(FILE *, struct argp_state *) = NULL;
const char *argp_program_bug_address = NULL;
error_t argp_err_exit_status = 64;

typedef struct rin_argp_node {
    const struct argp *argp;
    int parent;
    size_t child_index;
    size_t child_count;
    const char *child_header;
    void *input;
    void *hook;
    void **owned_child_inputs;
    void **child_inputs;
    unsigned args_processed;
    int initialized;
} rin_argp_node;

typedef struct rin_argp_run {
    struct argp_state state;
    rin_argp_node *nodes;
    size_t node_count;
    size_t node_capacity;
    int input_argc;
    char **input_argv;
    int saw_nonoption;
    int early_success;
} rin_argp_run;

typedef struct rin_argp_option_match {
    int node_index;
    const struct argp_option *entry;
    const struct argp_option *real;
    int key;
} rin_argp_option_match;

static int rin_argp_option_end(const struct argp_option *option)
{
    return option && !option->name && !option->key && !option->doc &&
           !option->group;
}

int _option_is_short(const struct argp_option *option)
{
    return option && !(option->flags & OPTION_DOC) && option->key > 0 &&
           option->key <= 0x7e && option->key >= 0x21;
}

int __option_is_short(const struct argp_option *option)
{
    return _option_is_short(option);
}

int _option_is_end(const struct argp_option *option)
{
    return rin_argp_option_end(option);
}

int __option_is_end(const struct argp_option *option)
{
    return _option_is_end(option);
}

static error_t rin_argp_reserve_nodes(rin_argp_run *run, size_t needed)
{
    size_t capacity;
    rin_argp_node *nodes;
    if (needed > RIN_ARGP_MAX_PARSERS) return E2BIG;
    if (needed <= run->node_capacity) return 0;
    capacity = run->node_capacity ? run->node_capacity : 8u;
    while (capacity < needed) {
        if (capacity > RIN_ARGP_MAX_PARSERS / 2u) {
            capacity = RIN_ARGP_MAX_PARSERS;
            break;
        }
        capacity *= 2u;
    }
    nodes = (rin_argp_node *)realloc(run->nodes,
                                    capacity * sizeof(*nodes));
    if (!nodes) return ENOMEM;
    run->nodes = nodes;
    run->node_capacity = capacity;
    return 0;
}

static error_t rin_argp_collect(rin_argp_run *run, const struct argp *argp,
                                int parent, size_t child_index,
                                const char *child_header,
                                const struct argp **path, size_t depth)
{
    size_t index;
    size_t children = 0;
    error_t error;
    if (depth > RIN_ARGP_MAX_DEPTH) return E2BIG;
    if (argp) {
        for (size_t ancestor = 0; ancestor < depth; ++ancestor)
            if (path[ancestor] == argp) return EINVAL;
        path[depth] = argp;
        if (argp->children) {
            while (children < RIN_ARGP_MAX_PARSERS &&
                   argp->children[children].argp) ++children;
            if (children == RIN_ARGP_MAX_PARSERS) return E2BIG;
        }
    }
    error = rin_argp_reserve_nodes(run, run->node_count + 1u);
    if (error) return error;
    index = run->node_count++;
    run->nodes[index].argp = argp;
    run->nodes[index].parent = parent;
    run->nodes[index].child_index = child_index;
    run->nodes[index].child_count = children;
    run->nodes[index].child_header = child_header;
    run->nodes[index].input = NULL;
    run->nodes[index].hook = NULL;
    run->nodes[index].owned_child_inputs = NULL;
    run->nodes[index].child_inputs = NULL;
    run->nodes[index].args_processed = 0;
    run->nodes[index].initialized = 0;

    for (size_t child = 0; child < children; ++child) {
        error = rin_argp_collect(run, argp->children[child].argp,
                                 (int)index, child,
                                 argp->children[child].header,
                                 path, depth + 1u);
        if (error) return error;
    }
    return 0;
}

static error_t rin_argp_call(rin_argp_run *run, size_t node_index,
                             int key, char *argument)
{
    rin_argp_node *node = &run->nodes[node_index];
    error_t result;
    if (!node->argp || !node->argp->parser) return ARGP_ERR_UNKNOWN;
    run->state.input = node->input;
    run->state.child_inputs = node->child_inputs;
    run->state.hook = node->hook;
    run->state.arg_num = node->args_processed;
    result = node->argp->parser(key, argument, &run->state);
    node->input = run->state.input;
    node->child_inputs = run->state.child_inputs;
    node->hook = run->state.hook;
    if (run->state.argc < 0 || run->state.argc > run->input_argc ||
        (run->state.argc > 0 && !run->state.argv) ||
        run->state.next < 0 || run->state.next > run->state.argc) {
        run->state.argc = run->input_argc;
        run->state.argv = run->input_argv;
        if (run->state.next < 0) run->state.next = 0;
        if (run->state.next > run->state.argc)
            run->state.next = run->state.argc;
        return EINVAL;
    }
    if (key == ARGP_KEY_ARG && result == 0)
        node->args_processed = run->state.arg_num + 1u;
    else if (key == ARGP_KEY_ARG)
        node->args_processed = run->state.arg_num;
    return result;
}

static void rin_argp_free_run(rin_argp_run *run)
{
    for (size_t index = 0; index < run->node_count; ++index)
        free(run->nodes[index].owned_child_inputs);
    free(run->nodes);
    run->nodes = NULL;
    run->node_count = 0;
    run->node_capacity = 0;
}

static error_t rin_argp_initialize(rin_argp_run *run, void *input)
{
    error_t result;
    for (size_t index = 0; index < run->node_count; ++index) {
        rin_argp_node *node = &run->nodes[index];
        if (node->child_count) {
            if (node->child_count > ((size_t)-1) / sizeof(void *))
                return E2BIG;
            node->owned_child_inputs = (void **)calloc(
                node->child_count, sizeof(void *));
            if (!node->owned_child_inputs) return ENOMEM;
            node->child_inputs = node->owned_child_inputs;
        }
        if (node->parent < 0) {
            node->input = input;
        } else {
            rin_argp_node *parent = &run->nodes[node->parent];
            node->input = parent->child_inputs
                ? parent->child_inputs[node->child_index] : NULL;
        }
        result = rin_argp_call(run, index, ARGP_KEY_INIT, NULL);
        if (result && result != ARGP_ERR_UNKNOWN) return result;
        node->initialized = 1;
    }
    return 0;
}

static void rin_argp_notify_error(rin_argp_run *run)
{
    for (size_t index = 0; index < run->node_count; ++index) {
        rin_argp_node *node = &run->nodes[index];
        if (node->initialized)
            (void)rin_argp_call(run, index, ARGP_KEY_ERROR, NULL);
    }
}

static void rin_argp_notify_fini(rin_argp_run *run)
{
    for (size_t index = run->node_count; index > 0; --index) {
        rin_argp_node *node = &run->nodes[index - 1u];
        if (node->initialized)
            (void)rin_argp_call(run, index - 1u, ARGP_KEY_FINI, NULL);
    }
}

static error_t rin_argp_notify_success(rin_argp_run *run)
{
    error_t result;
    for (size_t index = run->node_count; index > 0; --index) {
        result = rin_argp_call(run, index - 1u, ARGP_KEY_SUCCESS, NULL);
        if (result != 0 && result != ARGP_ERR_UNKNOWN) return result;
    }
    return 0;
}

static error_t rin_argp_finish_arguments(rin_argp_run *run)
{
    error_t result;
    for (size_t index = 0; index < run->node_count; ++index) {
        if (run->nodes[index].args_processed == 0) {
            result = rin_argp_call(run, index, ARGP_KEY_NO_ARGS, NULL);
            if (result != 0 && result != ARGP_ERR_UNKNOWN) return result;
        }
    }
    for (size_t index = run->node_count; index > 0; --index) {
        result = rin_argp_call(run, index - 1u, ARGP_KEY_END, NULL);
        if (result != 0 && result != ARGP_ERR_UNKNOWN) return result;
    }
    return 0;
}

static int rin_argp_match_name(const char *candidate, const char *name,
                               size_t length, int require_exact)
{
    if (!candidate || !name || !length) return 0;
    if (strncmp(candidate, name, length) != 0) return 0;
    return require_exact ? candidate[length] == '\0' : 1;
}

static int rin_argp_find_long(rin_argp_run *run, const char *name,
                              size_t length, rin_argp_option_match *match,
                              int *ambiguous)
{
    size_t matches = 0;
    const char *first_name = NULL;
    *ambiguous = 0;
    /* An exact spelling wins over every abbreviation. */
    for (size_t node_index = 0; node_index < run->node_count; ++node_index) {
        const struct argp *argp = run->nodes[node_index].argp;
        const struct argp_option *options = argp ? argp->options : NULL;
        const struct argp_option *real = NULL;
        if (!options) continue;
        for (size_t option_index = 0;
             option_index < RIN_ARGP_MAX_OPTIONS; ++option_index) {
            const struct argp_option *entry = &options[option_index];
            if (rin_argp_option_end(entry)) break;
            if (!(entry->flags & OPTION_ALIAS)) real = entry;
            if ((entry->flags & OPTION_DOC) || !entry->name ||
                !rin_argp_match_name(entry->name, name, length, 1)) continue;
            if (!real) continue;
            match->node_index = (int)node_index;
            match->entry = entry;
            match->real = real;
            match->key = entry->key ? entry->key : real->key;
            return 1;
        }
    }
    for (size_t node_index = 0; node_index < run->node_count; ++node_index) {
        const struct argp *argp = run->nodes[node_index].argp;
        const struct argp_option *options = argp ? argp->options : NULL;
        const struct argp_option *real = NULL;
        if (!options) continue;
        for (size_t option_index = 0;
             option_index < RIN_ARGP_MAX_OPTIONS; ++option_index) {
            const struct argp_option *entry = &options[option_index];
            if (rin_argp_option_end(entry)) break;
            if (!(entry->flags & OPTION_ALIAS)) real = entry;
            if ((entry->flags & OPTION_DOC) || !entry->name ||
                !rin_argp_match_name(entry->name, name, length, 0)) continue;
            if (!real) continue;
            if (!first_name) {
                first_name = entry->name;
                matches = 1u;
                match->node_index = (int)node_index;
                match->entry = entry;
                match->real = real;
                match->key = entry->key ? entry->key : real->key;
            } else if (strcmp(first_name, entry->name) != 0) {
                matches = 2u;
                *ambiguous = 1;
                return 0;
            }
        }
    }
    return matches == 1u;
}

static int rin_argp_find_short(rin_argp_run *run, int key,
                               rin_argp_option_match *match)
{
    if (key <= 0 || key > 0x7e || key < 0x21) return 0;
    for (size_t node_index = 0; node_index < run->node_count; ++node_index) {
        const struct argp *argp = run->nodes[node_index].argp;
        const struct argp_option *options = argp ? argp->options : NULL;
        const struct argp_option *real = NULL;
        if (!options) continue;
        for (size_t option_index = 0;
             option_index < RIN_ARGP_MAX_OPTIONS; ++option_index) {
            const struct argp_option *entry = &options[option_index];
            int effective_key;
            if (rin_argp_option_end(entry)) break;
            if (!(entry->flags & OPTION_ALIAS)) real = entry;
            if (entry->flags & OPTION_DOC) continue;
            if (!real) continue;
            effective_key = entry->key ? entry->key : real->key;
            if (effective_key != key || effective_key > 0x7e ||
                effective_key < 0x21) continue;
            match->node_index = (int)node_index;
            match->entry = entry;
            match->real = real;
            match->key = effective_key;
            return 1;
        }
    }
    return 0;
}

static void rin_argp_message(rin_argp_run *run, const char *message,
                             const char *argument)
{
    if (run->state.flags & ARGP_NO_ERRS) return;
    if (argument)
        fprintf(run->state.err_stream ? run->state.err_stream : stderr,
                "%s: %s: %s\n", run->state.name ? run->state.name : "?",
                message, argument);
    else
        fprintf(run->state.err_stream ? run->state.err_stream : stderr,
                "%s: %s\n", run->state.name ? run->state.name : "?",
                message);
}

static char *rin_argp_filter(const struct argp *argp,
                             const struct argp_state *state,
                             int key, const char *text)
{
    void *input;
    if (!text || !argp || !argp->help_filter) return (char *)text;
    input = _argp_input(argp, state);
    return argp->help_filter(key, text, input);
}

static void rin_argp_print_filtered(FILE *stream, const struct argp *argp,
                                    const struct argp_state *state,
                                    int key, const char *text)
{
    char *filtered = rin_argp_filter(argp, state, key, text);
    if (filtered) {
        fputs(filtered, stream);
        if (filtered != text) free(filtered);
    }
}

static void rin_argp_render_usage(FILE *stream, const struct argp *argp,
                                  const struct argp_state *state)
{
    const char *name = state && state->name ? state->name : "program";
    const char *args = argp && argp->args_doc ? argp->args_doc : NULL;
    fprintf(stream, "Usage: %s [OPTION...]", name);
    if (args && *args) fprintf(stream, " %s", args);
    fputc('\n', stream);
}

static void rin_argp_render_options(FILE *stream, const struct argp *argp,
                                    const struct argp_state *state)
{
    const struct argp_option *options = argp ? argp->options : NULL;
    const struct argp_option *real = NULL;
    if (!options) return;
    for (size_t index = 0; index < RIN_ARGP_MAX_OPTIONS; ++index) {
        const struct argp_option *entry = &options[index];
        const struct argp_option *effective;
        if (rin_argp_option_end(entry)) break;
        if (!(entry->flags & OPTION_ALIAS)) real = entry;
        if (entry->flags & OPTION_DOC) {
            if (entry->doc) {
                rin_argp_print_filtered(stream, argp, state,
                                        ARGP_KEY_HELP_HEADER, entry->doc);
                fputc('\n', stream);
            }
            continue;
        }
        if ((entry->flags & (OPTION_HIDDEN | OPTION_ALIAS)) || !entry->name)
            continue;
        effective = real ? real : entry;
        fputs("  ", stream);
        if (entry->key > 0 && entry->key <= 0x7e && entry->key >= 0x21)
            fprintf(stream, "-%c, ", entry->key);
        fprintf(stream, "--%s", entry->name);
        if (effective->arg) {
            fprintf(stream, "%s%s", (effective->flags & OPTION_ARG_OPTIONAL)
                        ? "[=" : "=", effective->arg);
            if (effective->flags & OPTION_ARG_OPTIONAL) fputc(']', stream);
        }
        if (entry->doc) {
            fputs("\t", stream);
            rin_argp_print_filtered(stream, argp, state, entry->key,
                                    entry->doc);
        }
        fputc('\n', stream);
    }
}

static void rin_argp_render_help(FILE *stream, const struct argp *argp,
                                 const struct argp_state *state,
                                 unsigned flags)
{
    if (!stream) return;
    if (flags & (ARGP_HELP_USAGE | ARGP_HELP_SHORT_USAGE))
        rin_argp_render_usage(stream, argp, state);
    if ((flags & ARGP_HELP_LONG) && argp && argp->doc) {
        const char *separator = strchr(argp->doc, '\v');
        if (!separator && (flags & ARGP_HELP_PRE_DOC)) {
            rin_argp_print_filtered(stream, argp, state,
                                    ARGP_KEY_HELP_PRE_DOC, argp->doc);
            fputc('\n', stream);
        } else if (separator) {
            if (flags & ARGP_HELP_PRE_DOC) {
                size_t length = (size_t)(separator - argp->doc);
                char *before = (char *)malloc(length + 1u);
                if (before) {
                    memcpy(before, argp->doc, length);
                    before[length] = '\0';
                    rin_argp_print_filtered(stream, argp, state,
                                            ARGP_KEY_HELP_PRE_DOC, before);
                    fputc('\n', stream);
                    free(before);
                }
            }
            if ((flags & ARGP_HELP_POST_DOC) && separator[1]) {
                rin_argp_print_filtered(stream, argp, state,
                                        ARGP_KEY_HELP_POST_DOC, separator + 1);
                fputc('\n', stream);
            }
        }
    }
    if (flags & ARGP_HELP_LONG) {
        if (argp && argp->args_doc) {
            fputs("Arguments:\n  ", stream);
            rin_argp_print_filtered(stream, argp, state,
                                    ARGP_KEY_HELP_ARGS_DOC, argp->args_doc);
            fputc('\n', stream);
        }
        fputs("Options:\n", stream);
        if (state && state->pstate) {
            const rin_argp_run *run = (const rin_argp_run *)state->pstate;
            for (size_t index = 0; index < run->node_count; ++index) {
                const rin_argp_node *node = &run->nodes[index];
                if (!node->argp) continue;
                if (node->child_header && *node->child_header) {
                    rin_argp_print_filtered(stream, node->argp, state,
                                            ARGP_KEY_HELP_HEADER,
                                            node->child_header);
                    fputs(":\n", stream);
                }
                rin_argp_render_options(stream, node->argp, state);
            }
        } else {
            rin_argp_render_options(stream, argp, state);
        }
        fputs("  -?, --help\tGive this help list\n", stream);
        fputs("      --usage\tGive a short usage message\n", stream);
        if (argp_program_version || argp_program_version_hook)
            fputs("  -V, --version\tPrint program version\n", stream);
    }
    if ((flags & ARGP_HELP_BUG_ADDR) && argp_program_bug_address)
        fprintf(stream, "Report bugs to %s.\n", argp_program_bug_address);
    if (flags & ARGP_HELP_SEE) {
        const char *name = state && state->name ? state->name : "program";
        fprintf(stream, "Try '%s --help' for more information.\n", name);
    }
}

void argp_state_help(const struct argp_state *state, FILE *stream,
                     unsigned flags)
{
    if (!state) return;
    rin_argp_render_help(stream, state->root_argp, state, flags);
    if ((flags & ARGP_HELP_EXIT_OK) && !(state->flags & ARGP_NO_EXIT))
        exit(0);
    if ((flags & ARGP_HELP_EXIT_ERR) && !(state->flags & ARGP_NO_EXIT))
        exit(argp_err_exit_status);
}

void __argp_state_help(const struct argp_state *state, FILE *stream,
                       unsigned flags)
{
    argp_state_help(state, stream, flags);
}

void argp_help(const struct argp *argp, FILE *stream, unsigned flags,
               char *name)
{
    struct argp_state state;
    rin_argp_run run;
    const struct argp *path[RIN_ARGP_MAX_DEPTH + 1u];
    error_t error;
    memset(&run, 0, sizeof(run));
    memset(&state, 0, sizeof(state));
    state.root_argp = argp;
    state.name = name;
    state.err_stream = stderr;
    state.out_stream = stdout;
    memset(path, 0, sizeof(path));
    error = rin_argp_collect(&run, argp, -1, 0, NULL, path, 0);
    if (!error) state.pstate = &run;
    rin_argp_render_help(stream, argp, &state, flags);
    rin_argp_free_run(&run);
}

void __argp_help(const struct argp *argp, FILE *stream, unsigned flags,
                 char *name)
{
    argp_help(argp, stream, flags, name);
}

void argp_usage(const struct argp_state *state)
{
    if (!state) return;
    argp_state_help(state, state->err_stream ? state->err_stream : stderr,
                    ARGP_HELP_STD_USAGE);
}

void __argp_usage(const struct argp_state *state)
{
    argp_usage(state);
}

static void rin_argp_vfailure(const struct argp_state *state, int status,
                              int errnum, const char *format, va_list args,
                              int parse_error)
{
    FILE *stream;
    unsigned flags;
    if (!state) return;
    if (state->flags & ARGP_NO_ERRS) return;
    stream = state->err_stream ? state->err_stream : stderr;
    fprintf(stream, "%s: ", state->name ? state->name : "?");
    vfprintf(stream, format ? format : "", args);
    if (errnum) {
        const char *description = strerror(errnum);
        if (description) fprintf(stream, ": %s", description);
    }
    fputc('\n', stream);
    flags = parse_error ? ARGP_HELP_STD_ERR : 0u;
    if (flags) rin_argp_render_help(stream, state->root_argp, state, flags);
    if (!(state->flags & ARGP_NO_EXIT) && status) exit(status);
}

void argp_error(const struct argp_state *state, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    rin_argp_vfailure(state, argp_err_exit_status, 0, format, args, 1);
    va_end(args);
}

void __argp_error(const struct argp_state *state, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    rin_argp_vfailure(state, argp_err_exit_status, 0, format, args, 1);
    va_end(args);
}

void argp_failure(const struct argp_state *state, int status, int errnum,
                  const char *format, ...)
{
    va_list args;
    va_start(args, format);
    rin_argp_vfailure(state, status, errnum, format, args, 0);
    va_end(args);
}

void __argp_failure(const struct argp_state *state, int status, int errnum,
                    const char *format, ...)
{
    va_list args;
    va_start(args, format);
    rin_argp_vfailure(state, status, errnum, format, args, 0);
    va_end(args);
}

void *_argp_input(const struct argp *argp, const struct argp_state *state)
{
    rin_argp_run *run;
    if (!argp || !state || !state->pstate) return NULL;
    run = (rin_argp_run *)state->pstate;
    for (size_t index = 0; index < run->node_count; ++index)
        if (run->nodes[index].argp == argp) return run->nodes[index].input;
    return NULL;
}

void *__argp_input(const struct argp *argp, const struct argp_state *state)
{
    return _argp_input(argp, state);
}

static error_t rin_argp_dispatch_option(rin_argp_run *run,
                                       const rin_argp_option_match *match,
                                       char *argument)
{
    error_t result = rin_argp_call(run, (size_t)match->node_index,
                                   match->key, argument);
    if (result == ARGP_ERR_UNKNOWN) return EINVAL;
    return result;
}

static error_t rin_argp_dispatch_arg(rin_argp_run *run, char *argument,
                                     int argv_index, int *handled)
{
    error_t result = ARGP_ERR_UNKNOWN;
    *handled = 0;
    for (size_t index = 0; index < run->node_count; ++index) {
        run->state.next = argv_index;
        ++run->state.next;
        result = rin_argp_call(run, index, ARGP_KEY_ARG, argument);
        if (result == 0 && run->state.next > argv_index) {
            run->saw_nonoption = 1;
            *handled = 1;
            return 0;
        }
        if (result == 0) {
            run->nodes[index].args_processed = run->state.arg_num;
            *handled = 1;
            return 0;
        }
        if (result != ARGP_ERR_UNKNOWN) return result;

        run->state.next = argv_index;
        result = rin_argp_call(run, index, ARGP_KEY_ARGS, NULL);
        if (result == 0) {
            if (run->state.next == argv_index)
                run->state.next = run->state.argc;
            if (run->state.next > argv_index) {
                run->nodes[index].args_processed +=
                    (unsigned)(run->state.next - argv_index);
                run->saw_nonoption = 1;
                *handled = 1;
                return 0;
            }
        } else if (result != ARGP_ERR_UNKNOWN) {
            return result;
        }
    }
    run->state.next = argv_index;
    return result;
}

static int rin_argp_is_nonoption(const char *argument)
{
    return !argument || argument[0] != '-' || argument[1] == '\0';
}

static error_t rin_argp_builtin(rin_argp_run *run, int key, const char *name,
                                int *handled)
{
    *handled = 1;
    if (run->state.flags & ARGP_NO_HELP) {
        *handled = 0;
        return 0;
    }
    if (key == '?' || (name && strcmp(name, "help") == 0)) {
        argp_state_help(&run->state, run->state.out_stream,
                        ARGP_HELP_STD_HELP);
        run->early_success = 1;
        return 0;
    }
    if (name && strcmp(name, "usage") == 0) {
        argp_state_help(&run->state, run->state.out_stream,
                        ARGP_HELP_USAGE | ARGP_HELP_EXIT_OK);
        run->early_success = 1;
        return 0;
    }
    if (key == 'V' || (name && strcmp(name, "version") == 0)) {
        if (!argp_program_version && !argp_program_version_hook) {
            *handled = 0;
            return 0;
        }
        if (argp_program_version_hook)
            argp_program_version_hook(run->state.out_stream, &run->state);
        else
            fprintf(run->state.out_stream, "%s\n", argp_program_version);
        if (!(run->state.flags & ARGP_NO_EXIT)) exit(0);
        run->early_success = 1;
        return 0;
    }
    *handled = 0;
    return 0;
}

static error_t rin_argp_long(rin_argp_run *run, const char *text,
                             int argv_index, unsigned char *kind,
                             int *next_index, int single_dash)
{
    const char *name = text + (single_dash ? 1 : 2);
    const char *equals = NULL;
    size_t length = 0;
    int ambiguous = 0;
    int handled;
    error_t result;
    rin_argp_option_match match;
    char *argument = NULL;
    while (name[length] && name[length] != '=') ++length;
    if (name[length] == '=') equals = name + length + 1u;
    if (!length) return EINVAL;
    if (rin_argp_find_long(run, name, length, &match, &ambiguous)) {
        const struct argp_option *real = match.real;
        int argument_optional = (real->flags & OPTION_ARG_OPTIONAL) != 0;
        if (!real->arg && equals) {
            rin_argp_message(run, "option does not allow an argument", text);
            return EINVAL;
        }
        if (real->arg) {
            if (equals) {
                argument = (char *)equals;
            } else if (!argument_optional) {
                if (argv_index + 1 >= run->state.argc) {
                    rin_argp_message(run, "option requires an argument", text);
                    return EINVAL;
                }
                argument = run->state.argv[argv_index + 1];
                kind[argv_index + 1] = 2u;
                *next_index = argv_index + 2;
            } else {
                *next_index = argv_index + 1;
            }
        }
        run->state.next = *next_index;
        result = rin_argp_dispatch_option(run, &match, argument);
        if (run->state.next >= 0 && run->state.next <= run->state.argc)
            *next_index = run->state.next;
        return result;
    }
    if (!ambiguous &&
        ((length == 4u && strncmp(name, "help", 4u) == 0) ||
         (length == 5u && strncmp(name, "usage", 5u) == 0) ||
         (length == 7u && strncmp(name, "version", 7u) == 0))) {
        if (equals) return EINVAL;
        if (length == 4u)
            result = rin_argp_builtin(run, '?', "help", &handled);
        else if (length == 5u)
            result = rin_argp_builtin(run, -3, "usage", &handled);
        else
            result = rin_argp_builtin(run, 'V', "version", &handled);
        if (result || handled) {
            *next_index = argv_index + 1;
            run->state.next = *next_index;
            return result;
        }
    }
    rin_argp_message(run, ambiguous ? "option is ambiguous"
                                     : "unrecognized option", text);
    return EINVAL;
}

static error_t rin_argp_option_token(rin_argp_run *run, int argv_index,
                                     unsigned char *kind, int *next_index)
{
    char *argument = run->state.argv[argv_index];
    size_t offset = 1u;
    error_t result;
    int handled;
    rin_argp_option_match match;
    if (!argument || argument[0] != '-') return EINVAL;
    if (argument[1] == '-') {
        if (argument[2] == '\0') {
            kind[argv_index] = 3u;
            run->state.quoted = argv_index + 1;
            *next_index = argv_index + 1;
            return 0;
        }
        return rin_argp_long(run, argument, argv_index, kind,
                             next_index, 0);
    }
    if ((run->state.flags & ARGP_LONG_ONLY) && argument[2]) {
        if (!rin_argp_find_short(run, (unsigned char)argument[1], &match)) {
            const char *equal = strchr(argument + 1, '=');
            size_t length = equal ? (size_t)(equal - (argument + 1))
                                  : strlen(argument + 1);
            int ambiguous = 0;
            if (rin_argp_find_long(run, argument + 1, length,
                                   &match, &ambiguous) || ambiguous)
                return rin_argp_long(run, argument, argv_index, kind,
                                     next_index, 1);
        }
    }
    kind[argv_index] = 1u;
    *next_index = argv_index + 1;
    while (argument[offset]) {
        int key = (unsigned char)argument[offset++];
        char *value = NULL;
        if (!rin_argp_find_short(run, key, &match)) {
            if (offset == 2u) {
                result = rin_argp_builtin(run, key, NULL, &handled);
                if (result || handled) return result;
            }
            rin_argp_message(run, "unrecognized option", argument);
            return EINVAL;
        }
        if (match.real->arg) {
            if (argument[offset]) {
                value = argument + offset;
                *next_index = argv_index + 1;
            } else if (match.real->flags & OPTION_ARG_OPTIONAL) {
                value = NULL;
            } else if (argv_index + 1 < run->state.argc) {
                value = run->state.argv[argv_index + 1];
                kind[argv_index + 1] = 2u;
                *next_index = argv_index + 2;
            } else {
                rin_argp_message(run, "option requires an argument", argument);
                return EINVAL;
            }
            run->state.next = *next_index;
            result = rin_argp_dispatch_option(run, &match, value);
            if (run->state.next >= 0 && run->state.next <= run->state.argc)
                *next_index = run->state.next;
            return result;
        }
        run->state.next = argv_index + 1;
        result = rin_argp_dispatch_option(run, &match, NULL);
        if (result) return result;
        if (run->state.next != argv_index + 1) {
            *next_index = run->state.next;
            return 0;
        }
    }
    return 0;
}

static error_t rin_argp_reorder(rin_argp_run *run, unsigned char *kind,
                                char **ordered, int first, int *option_end)
{
    int count = run->state.argc;
    int output = first;
    int marker = -1;
    if (!ordered && count > first) return ENOMEM;
    for (int index = 0; index < first; ++index) ordered[index] = run->state.argv[index];
    for (int index = first; index < count; ++index)
        if (kind[index] == 1u || kind[index] == 2u)
            ordered[output++] = run->state.argv[index];
    *option_end = output;
    for (int index = first; index < count; ++index)
        if (kind[index] == 0u || kind[index] == 4u)
            ordered[output++] = run->state.argv[index];
    for (int index = first; index < count; ++index)
        if (kind[index] == 3u) { marker = index; break; }
    if (marker >= 0) ordered[output++] = run->state.argv[marker];
    for (int index = first; index < count; ++index)
        if (kind[index] == 5u)
            ordered[output++] = run->state.argv[index];
    if (output != count) return EINVAL;
    if (count)
        memcpy(run->state.argv, ordered, (size_t)count * sizeof(char *));
    if (marker >= 0) {
        int prequoted_operands = 0;
        for (int index = first; index < marker; ++index)
            if (kind[index] == 0u || kind[index] == 4u)
                ++prequoted_operands;
        run->state.quoted = *option_end + prequoted_operands + 1;
    }
    else run->state.quoted = 0;
    run->state.next = *option_end;
    return 0;
}

static error_t rin_argp_parse_options(rin_argp_run *run,
                                     unsigned char *kind,
                                     char **ordered,
                                     int *operand_start,
                                     int *unhandled_operand)
{
    int first = (run->state.flags & ARGP_PARSE_ARGV0) ? 0 : 1;
    int index = first;
    int option_end = first;
    error_t result = 0;
    *unhandled_operand = -1;
    if (first > run->state.argc) first = run->state.argc;
    while (index < run->state.argc && !run->early_success) {
        char *argument = run->state.argv[index];
        if (rin_argp_is_nonoption(argument)) {
            kind[index] = 4u;
            if (run->state.flags & ARGP_IN_ORDER) {
                if (run->state.flags & ARGP_NO_ARGS) {
                    run->state.next = index + 1;
                    ++index;
                    continue;
                }
                int handled = 0;
                result = rin_argp_dispatch_arg(run, argument, index, &handled);
                if (result && result != ARGP_ERR_UNKNOWN) return result;
                if (!handled) {
                    *unhandled_operand = index;
                    return 0;
                }
                if (run->state.next > index) index = run->state.next;
                else ++index;
                continue;
            }
            ++index;
            continue;
        }
        if (argument[1] == '-' && argument[2] == '\0') {
            kind[index] = 3u;
            run->state.quoted = index + 1;
            for (int tail = index + 1; tail < run->state.argc; ++tail)
                if (kind[tail] == 0u) kind[tail] = 5u;
            if (run->state.flags & ARGP_IN_ORDER) {
                if (run->state.flags & ARGP_NO_ARGS) {
                    run->state.next = run->state.argc;
                    index = run->state.argc;
                    break;
                }
                for (int tail = index + 1; tail < run->state.argc;) {
                    int handled = 0;
                    result = rin_argp_dispatch_arg(run,
                                                   run->state.argv[tail],
                                                   tail, &handled);
                    if (result && result != ARGP_ERR_UNKNOWN) return result;
                    if (!handled) {
                        *unhandled_operand = tail;
                        return 0;
                    }
                    if (run->state.next > tail) tail = run->state.next;
                    else ++tail;
                }
                run->state.next = run->state.argc;
            }
            break;
        }
        kind[index] = 1u;
        run->state.next = index + 1;
        result = rin_argp_option_token(run, index, kind, &index);
        if (result) return result;
        if (index < 0 || index > run->state.argc) return EINVAL;
    }
    if (run->state.flags & ARGP_IN_ORDER) {
        if (*unhandled_operand < 0) run->state.next = run->state.argc;
        *operand_start = run->state.next;
        return 0;
    }
    if (run->early_success) {
        *operand_start = run->state.argc;
        return 0;
    }
    result = rin_argp_reorder(run, kind, ordered, first, &option_end);
    if (result) return result;
    *operand_start = run->state.next;
    return 0;
}

static error_t rin_argp_parse_operands(rin_argp_run *run, int start,
                                       int *unhandled_operand)
{
    error_t result;
    if (run->state.flags & ARGP_NO_ARGS) {
        run->state.next = run->state.argc;
        return 0;
    }
    for (int index = start; index < run->state.argc;) {
        char *argument = run->state.argv[index];
        int handled = 0;
        if (run->state.quoted && index == run->state.quoted - 1 &&
            argument && strcmp(argument, "--") == 0) {
            ++index;
            continue;
        }
        result = rin_argp_dispatch_arg(run, argument, index, &handled);
        if (result && result != ARGP_ERR_UNKNOWN) return result;
        if (!handled) {
            *unhandled_operand = index;
            run->state.next = index;
            return ARGP_ERR_UNKNOWN;
        }
        index = run->state.next;
        if (index <= start || index <= 0) index = start + 1;
        start = index;
    }
    run->state.next = run->state.argc;
    return 0;
}

static void rin_argp_exit_on_error(rin_argp_run *run, error_t error,
                                   int unhandled_operand,
                                   int has_arg_index)
{
    if (run->state.flags & ARGP_NO_ERRS) return;
    if (unhandled_operand >= 0 && !has_arg_index) {
        fprintf(run->state.err_stream, "%s: Too many arguments\n",
                run->state.name ? run->state.name : "?");
        argp_state_help(&run->state, run->state.err_stream,
                        ARGP_HELP_STD_ERR);
    } else if (error && error != ARGP_ERR_UNKNOWN) {
        argp_state_help(&run->state, run->state.err_stream,
                        ARGP_HELP_STD_ERR);
    }
}

error_t argp_parse(const struct argp *argp, int argc, char **argv,
                   unsigned flags, int *arg_index, void *input)
{
    rin_argp_run run;
    const struct argp *path[RIN_ARGP_MAX_DEPTH + 1u];
    unsigned char *kind = NULL;
    char **ordered = NULL;
    int operand_start = 0;
    int unhandled_operand = -1;
    error_t result;
    int initialized = 0;
    memset(&run, 0, sizeof(run));
    if (argc < 0 || argc > RIN_ARGP_MAX_ARGC || (argc && !argv)) return EINVAL;
    if ((size_t)argc > (size_t)-1 / sizeof(char *)) return E2BIG;
    if (flags & ARGP_NO_ERRS) flags |= ARGP_NO_EXIT;
    run.state.root_argp = argp;
    run.state.argc = argc;
    run.input_argc = argc;
    run.state.argv = argv;
    run.input_argv = argv;
    run.state.flags = flags;
    run.state.next = (flags & ARGP_PARSE_ARGV0) ? 0 : 1;
    if (run.state.next > argc) run.state.next = argc;
    run.state.input = input;
    run.state.err_stream = stderr;
    run.state.out_stream = stdout;
    run.state.name = (argc > 0 && argv && argv[0]) ? argv[0] : (char *)"program";
    if (run.state.name) {
        char *slash = strrchr(run.state.name, '/');
        if (slash) run.state.name = slash + 1;
    }
    run.state.pstate = &run;
    memset(path, 0, sizeof(path));
    result = rin_argp_collect(&run, argp, -1, 0, NULL, path, 0);
    if (result) goto done;
    if (argc) {
        kind = (unsigned char *)calloc((size_t)argc, 1u);
        if (!kind) { result = ENOMEM; goto done; }
        if (!(flags & ARGP_IN_ORDER)) {
            ordered = (char **)malloc((size_t)argc * sizeof(char *));
            if (!ordered) { result = ENOMEM; goto done; }
        }
    }
    result = rin_argp_initialize(&run, input);
    initialized = 1;
    if (result) goto finalize;
    result = rin_argp_parse_options(&run, kind, ordered,
                                    &operand_start, &unhandled_operand);
    if (result) goto finalize;
    if (!(flags & ARGP_IN_ORDER) && !run.early_success &&
        !(flags & ARGP_NO_ARGS)) {
        result = rin_argp_parse_operands(&run, operand_start,
                                         &unhandled_operand);
        if (result == ARGP_ERR_UNKNOWN && arg_index) {
            run.state.next = unhandled_operand;
            result = 0;
        }
    }
    if (result == ARGP_ERR_UNKNOWN && arg_index) result = 0;
    if (result == 0 && unhandled_operand >= 0 && !arg_index)
        result = ARGP_ERR_UNKNOWN;
    if (result == 0 && !run.early_success &&
        run.state.next < run.state.argc && !arg_index &&
        !(flags & ARGP_NO_ARGS))
        result = ARGP_ERR_UNKNOWN;

finalize:
    if (initialized) {
        if (result) {
            rin_argp_notify_error(&run);
        } else {
            if (unhandled_operand < 0 && run.state.next >= run.state.argc)
                result = rin_argp_finish_arguments(&run);
            if (!result) result = rin_argp_notify_success(&run);
        }
        rin_argp_exit_on_error(&run, result, unhandled_operand,
                               arg_index != NULL);
        rin_argp_notify_fini(&run);
    }
    if (arg_index) *arg_index = run.state.next;
    if (result == ARGP_ERR_UNKNOWN && arg_index) result = 0;
done:
    free(kind);
    free(ordered);
    rin_argp_free_run(&run);
    if (!initialized && result && !(flags & ARGP_NO_ERRS)) {
        /* Construction/allocation failures are returned without callbacks. */
        if (!(flags & ARGP_NO_EXIT)) return result;
    }
    return result;
}

error_t __argp_parse(const struct argp *argp, int argc, char **argv,
                     unsigned flags, int *arg_index, void *input)
{
    return argp_parse(argp, argc, argv, flags, arg_index, input);
}
