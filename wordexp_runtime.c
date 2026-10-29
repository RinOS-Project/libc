/* SPDX-License-Identifier: MIT */
#include "wordexp.h"

#include "errno.h"
#include "fcntl.h"
#include "signal.h"
#include "stdlib.h"
#include "string.h"
#include "sys/wait.h"
#include "unistd.h"

#define WORDEXP_VALID_FLAGS \
    (WRDE_APPEND | WRDE_DOOFFS | WRDE_NOCMD | WRDE_REUSE | \
     WRDE_SHOWERR | WRDE_UNDEF)

static int wordexp_spawn_error(int error_number) {
    if (error_number == ENOMEM || error_number == EAGAIN ||
        error_number == EMFILE || error_number == ENFILE)
        return WRDE_NOSPACE;
    return WRDE_NOSYS;
}

static int wordexp_skip_arithmetic(const char* text, size_t length,
                                   size_t start, int no_command,
                                   size_t* end_out);
static int wordexp_skip_command(const char* text, size_t length,
                                size_t start, int no_command,
                                size_t* end_out);

static int wordexp_skip_backtick(const char* text, size_t length,
                                 size_t start, int no_command,
                                 size_t* end_out) {
    size_t index = start + 1u;
    if (no_command) return WRDE_CMDSUB;
    while (index < length) {
        if (text[index] == '\\') {
            if (index + 1u < length) index += 2u;
            else ++index;
            continue;
        }
        if (text[index] == '`') {
            *end_out = index + 1u;
            return 0;
        }
        ++index;
    }
    return WRDE_SYNTAX;
}

static int wordexp_skip_parameter(const char* text, size_t length,
                                  size_t start, int no_command,
                                  size_t* end_out) {
    size_t index = start + 2u;
    size_t depth = 1u;
    int quote = 0;
    while (index < length) {
        char ch = text[index];
        if (quote == '\'') {
            if (ch == '\'') quote = 0;
            ++index;
            continue;
        }
        if (quote == '"') {
            if (ch == '\\') {
                if (index + 1u < length) index += 2u;
                else ++index;
                continue;
            }
            if (ch == '"') {
                quote = 0;
                ++index;
                continue;
            }
        } else {
            if (ch == '\'') {
                quote = '\'';
                ++index;
                continue;
            }
            if (ch == '"') {
                quote = '"';
                ++index;
                continue;
            }
            if (ch == '\\') {
                if (index + 1u < length) index += 2u;
                else ++index;
                continue;
            }
        }
        if (ch == '$' && index + 2u < length &&
            text[index + 1u] == '(' && text[index + 2u] == '(') {
            size_t nested_end = 0u;
            int result = wordexp_skip_arithmetic(
                text, length, index, no_command, &nested_end);
            if (result != 0) return result;
            index = nested_end;
            continue;
        }
        if (ch == '$' && index + 1u < length && text[index + 1u] == '(') {
            size_t nested_end = 0u;
            int result = wordexp_skip_command(
                text, length, index, no_command, &nested_end);
            if (result != 0) return result;
            index = nested_end;
            continue;
        }
        if (ch == '$' && index + 1u < length && text[index + 1u] == '{') {
            size_t nested_end = 0u;
            int result = wordexp_skip_parameter(
                text, length, index, no_command, &nested_end);
            if (result != 0) return result;
            index = nested_end;
            continue;
        }
        if (ch == '`') {
            size_t nested_end = 0u;
            int result = wordexp_skip_backtick(
                text, length, index, no_command, &nested_end);
            if (result != 0) return result;
            index = nested_end;
            continue;
        }
        if (quote == 0 && ch == '{') ++depth;
        else if (quote == 0 && ch == '}' && --depth == 0u) {
            *end_out = index + 1u;
            return 0;
        }
        ++index;
    }
    return WRDE_SYNTAX;
}

static int wordexp_skip_arithmetic(const char* text, size_t length,
                                   size_t start, int no_command,
                                   size_t* end_out) {
    size_t index = start + 3u;
    size_t depth = 0u;
    int quote = 0;
    while (index < length) {
        char ch = text[index];
        if (quote == '\'') {
            if (ch == '\'') quote = 0;
            ++index;
            continue;
        }
        if (quote == '"') {
            if (ch == '\\') {
                if (index + 1u < length) index += 2u;
                else ++index;
                continue;
            }
            if (ch == '"') quote = 0;
        } else if (ch == '\'') {
            quote = '\'';
            ++index;
            continue;
        } else if (ch == '"') {
            quote = '"';
            ++index;
            continue;
        } else if (ch == '\\') {
            if (index + 1u < length) index += 2u;
            else ++index;
            continue;
        }
        if (ch == '$' && index + 2u < length &&
            text[index + 1u] == '(' && text[index + 2u] == '(') {
            size_t nested_end = 0u;
            int result = wordexp_skip_arithmetic(
                text, length, index, no_command, &nested_end);
            if (result != 0) return result;
            index = nested_end;
            continue;
        }
        if (ch == '$' && index + 1u < length && text[index + 1u] == '(') {
            size_t nested_end = 0u;
            int result = wordexp_skip_command(
                text, length, index, no_command, &nested_end);
            if (result != 0) return result;
            index = nested_end;
            continue;
        }
        if (ch == '$' && index + 1u < length && text[index + 1u] == '{') {
            size_t nested_end = 0u;
            int result = wordexp_skip_parameter(
                text, length, index, no_command, &nested_end);
            if (result != 0) return result;
            index = nested_end;
            continue;
        }
        if (ch == '`') {
            size_t nested_end = 0u;
            int result = wordexp_skip_backtick(
                text, length, index, no_command, &nested_end);
            if (result != 0) return result;
            index = nested_end;
            continue;
        }
        if (quote == 0 && ch == '(') {
            ++depth;
            ++index;
            continue;
        }
        if (quote == 0 && ch == ')') {
            if (depth != 0u) {
                --depth;
                ++index;
                continue;
            }
            if (index + 1u < length && text[index + 1u] == ')') {
                *end_out = index + 2u;
                return 0;
            }
        }
        ++index;
    }
    return WRDE_SYNTAX;
}

static int wordexp_skip_command(const char* text, size_t length,
                                size_t start, int no_command,
                                size_t* end_out) {
    size_t index = start + 2u;
    size_t depth = 1u;
    int quote = 0;
    int word_start = 1;
    if (no_command) return WRDE_CMDSUB;
    while (index < length) {
        char ch = text[index];
        if (quote == '\'') {
            if (ch == '\'') quote = 0;
            ++index;
            continue;
        }
        if (quote == '"') {
            if (ch == '\\') {
                if (index + 1u < length) index += 2u;
                else ++index;
                word_start = 0;
                continue;
            }
            if (ch == '"') {
                quote = 0;
                word_start = 0;
                ++index;
                continue;
            }
        } else {
            if (ch == '\'') {
                quote = '\'';
                word_start = 0;
                ++index;
                continue;
            }
            if (ch == '"') {
                quote = '"';
                word_start = 0;
                ++index;
                continue;
            }
            if (ch == '\\') {
                if (index + 1u < length) {
                    if (text[index + 1u] != '\n') word_start = 0;
                    index += 2u;
                } else ++index;
                continue;
            }
        }
        if (ch == '$' && index + 2u < length &&
            text[index + 1u] == '(' && text[index + 2u] == '(') {
            size_t nested_end = 0u;
            int result = wordexp_skip_arithmetic(
                text, length, index, 0, &nested_end);
            if (result != 0) return result;
            index = nested_end;
            word_start = 0;
            continue;
        }
        if (ch == '$' && index + 1u < length && text[index + 1u] == '(') {
            size_t nested_end = 0u;
            int result = wordexp_skip_command(
                text, length, index, 0, &nested_end);
            if (result != 0) return result;
            index = nested_end;
            word_start = 0;
            continue;
        }
        if (ch == '$' && index + 1u < length && text[index + 1u] == '{') {
            size_t nested_end = 0u;
            int result = wordexp_skip_parameter(
                text, length, index, 0, &nested_end);
            if (result != 0) return result;
            index = nested_end;
            word_start = 0;
            continue;
        }
        if (ch == '`') {
            size_t nested_end = 0u;
            int result = wordexp_skip_backtick(
                text, length, index, 0, &nested_end);
            if (result != 0) return result;
            index = nested_end;
            word_start = 0;
            continue;
        }
        if (quote == 0 && ch == '#' && word_start) {
            while (index < length && text[index] != '\n') ++index;
            word_start = 1;
            continue;
        }
        if (quote == 0 && ch == '(') {
            ++depth;
            word_start = 1;
        } else if (quote == 0 && ch == ')') {
            if (--depth == 0u) {
                *end_out = index + 1u;
                return 0;
            }
            word_start = 1;
        } else if (quote == 0 &&
                   (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' ||
                    ch == ';' || ch == '|' || ch == '&')) {
            word_start = 1;
        } else if (quote == 0) {
            word_start = 0;
        }
        ++index;
    }
    return WRDE_SYNTAX;
}

static int wordexp_validate_fragment(const char* text, size_t length,
                                     int no_command) {
    size_t index = 0u;
    int quote = 0;
    int word_start = 1;
    while (index < length) {
        char ch = text[index];
        if (quote == '\'') {
            if (ch == '\'') quote = 0;
            ++index;
            continue;
        }
        if (quote == '"') {
            if (ch == '\\') {
                if (index + 1u < length) index += 2u;
                else ++index;
                continue;
            }
            if (ch == '"') {
                quote = 0;
                ++index;
                continue;
            }
        } else {
            if (ch == '\'') {
                quote = '\'';
                word_start = 0;
                ++index;
                continue;
            }
            if (ch == '"') {
                quote = '"';
                word_start = 0;
                ++index;
                continue;
            }
            if (ch == '\\') {
                if (index + 1u < length) {
                    if (text[index + 1u] != '\n') word_start = 0;
                    index += 2u;
                } else ++index;
                continue;
            }
            if (ch == '\n') return WRDE_BADCHAR;
            if (ch == '#' && word_start) return WRDE_BADCHAR;
            if (ch == '|' || ch == '&' || ch == ';' || ch == '<' ||
                ch == '>' || ch == '(' || ch == ')' || ch == '{' ||
                ch == '}')
                return WRDE_BADCHAR;
            if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\v' ||
                ch == '\f') {
                word_start = 1;
                ++index;
                continue;
            }
        }
        if (ch == '$' && index + 2u < length &&
            text[index + 1u] == '(' && text[index + 2u] == '(') {
            size_t nested_end = 0u;
            int result = wordexp_skip_arithmetic(
                text, length, index, no_command, &nested_end);
            if (result != 0) return result;
            index = nested_end;
            word_start = 0;
            continue;
        }
        if (ch == '$' && index + 1u < length && text[index + 1u] == '(') {
            size_t nested_end = 0u;
            int result = wordexp_skip_command(
                text, length, index, no_command, &nested_end);
            if (result != 0) return result;
            index = nested_end;
            word_start = 0;
            continue;
        }
        if (ch == '$' && index + 1u < length && text[index + 1u] == '{') {
            size_t nested_end = 0u;
            int result = wordexp_skip_parameter(
                text, length, index, no_command, &nested_end);
            if (result != 0) return result;
            index = nested_end;
            word_start = 0;
            continue;
        }
        if (ch == '`') {
            size_t nested_end = 0u;
            int result = wordexp_skip_backtick(
                text, length, index, no_command, &nested_end);
            if (result != 0) return result;
            index = nested_end;
            word_start = 0;
            continue;
        }
        if (quote == 0 && ch != ' ' && ch != '\t' && ch != '\r' &&
            ch != '\v' && ch != '\f')
            word_start = 0;
        ++index;
    }
    return 0;
}

static int wordexp_wait_child(pid_t child, int* status_out) {
    intptr_t waited;
    do {
        waited = _rin_unistd_result(_RIN_WAIT_SYSCALL3(
            SYS_WAIT, (uintptr_t)(intptr_t)child, (uintptr_t)status_out, 0u));
    } while (waited < 0 && errno == EINTR);
    if (waited < 0) return wordexp_spawn_error(errno);
    if (waited != (intptr_t)child) {
        errno = EIO;
        return WRDE_NOSYS;
    }
    return 0;
}

static int wordexp_shell_run(char* script, int capture, int show_errors,
                             char** output_out,
                             size_t* output_length_out) {
    int pipe_fds[2] = {-1, -1};
    int status = 0;
    int result = 0;
    char* output = NULL;
    size_t output_length = 0u;
    size_t output_capacity = 0u;
    pid_t child;

    if (capture && pipe2(pipe_fds, O_CLOEXEC) != 0)
        return wordexp_spawn_error(errno);
    child = fork();
    if (child < 0) {
        int saved_error = errno;
        if (pipe_fds[0] >= 0) (void)close(pipe_fds[0]);
        if (pipe_fds[1] >= 0) (void)close(pipe_fds[1]);
        return wordexp_spawn_error(saved_error);
    }
    if (child == 0) {
        char* arguments[5];
        size_t argument_count = 0u;
        if (capture) {
            (void)close(pipe_fds[0]);
            if (pipe_fds[1] == STDOUT_FILENO) {
                /* pipe2() can return stdout itself when the caller closed
                 * fd 1.  dup2(fd, fd) would preserve FD_CLOEXEC, so clear it
                 * before exec and keep this descriptor open. */
                if (fcntl(pipe_fds[1], F_SETFD, 0) < 0) _exit(126);
            } else {
                if (dup2(pipe_fds[1], STDOUT_FILENO) < 0) _exit(126);
                (void)close(pipe_fds[1]);
            }
        }
        if (!show_errors) {
            int null_fd = open("/dev/null", O_WRONLY);
            if (null_fd >= 0) {
                if (dup2(null_fd, STDERR_FILENO) < 0) _exit(126);
                if (null_fd != STDERR_FILENO) (void)close(null_fd);
            }
        }
        arguments[argument_count++] = (char*)"/bin/sh";
        if (!capture) arguments[argument_count++] = (char*)"-n";
        arguments[argument_count++] = (char*)"-c";
        arguments[argument_count++] = script;
        arguments[argument_count] = NULL;
        execv("/bin/sh", arguments);
        _exit(127);
    }
    if (capture) {
        char block[512];
        (void)close(pipe_fds[1]);
        pipe_fds[1] = -1;
        for (;;) {
            intptr_t raw_count = _rin_unistd_result(_RIN_UNISTD_SYSCALL3(
                SYS_READ, (uintptr_t)(intptr_t)pipe_fds[0],
                (uintptr_t)block, sizeof(block)));
            ssize_t count;
            if (raw_count > (intptr_t)sizeof(block)) {
                errno = EIO;
                raw_count = -1;
            }
            count = (ssize_t)raw_count;
            if (count == 0) break;
            if (count < 0) {
                if (errno == EINTR) continue;
                result = wordexp_spawn_error(errno);
                (void)kill(child, SIGKILL);
                break;
            }
            if ((size_t)count > (size_t)-1 - output_length) {
                result = WRDE_NOSPACE;
                (void)kill(child, SIGKILL);
                break;
            }
            if (output_length + (size_t)count > output_capacity) {
                size_t next_capacity = output_capacity == 0u ? 512u :
                                      output_capacity;
                char* next_output;
                while (next_capacity < output_length + (size_t)count) {
                    if (next_capacity > (size_t)-1 / 2u) {
                        next_capacity = output_length + (size_t)count;
                        break;
                    }
                    next_capacity *= 2u;
                }
                next_output = (char*)realloc(output, next_capacity);
                if (!next_output) {
                    result = WRDE_NOSPACE;
                    (void)kill(child, SIGKILL);
                    break;
                }
                output = next_output;
                output_capacity = next_capacity;
            }
            memcpy(output + output_length, block, (size_t)count);
            output_length += (size_t)count;
        }
        (void)close(pipe_fds[0]);
    }
    if (result != 0) {
        (void)wordexp_wait_child(child, &status);
        free(output);
        return result;
    }
    result = wordexp_wait_child(child, &status);
    if (result != 0) {
        free(output);
        return result;
    }
    if (!WIFEXITED(status)) {
        free(output);
        return WRDE_NOSYS;
    }
    if (WEXITSTATUS(status) == 127) {
        free(output);
        return WRDE_NOSYS;
    }
    if (WEXITSTATUS(status) != 0) {
        free(output);
        return capture ? WRDE_BADVAL : WRDE_SYNTAX;
    }
    if (output_out) *output_out = output;
    else free(output);
    if (output_length_out) *output_length_out = output_length;
    return 0;
}

static void wordexp_free_words(char** words, size_t count) {
    size_t index;
    if (!words) return;
    for (index = 0u; index < count; ++index) free(words[index]);
    free(words);
}

static int wordexp_decode_output(const char* output, size_t length,
                                 char*** words_out, size_t* count_out) {
    size_t count = 0u;
    size_t index;
    size_t word_index = 0u;
    size_t start = 0u;
    char** words;
    if (length == 0u) {
        words = (char**)calloc(1u, sizeof(char*));
        if (!words) return WRDE_NOSPACE;
        *words_out = words;
        *count_out = 0u;
        return 0;
    }
    if (!output || output[length - 1u] != '\0') return WRDE_SYNTAX;
    for (index = 0u; index < length; ++index) {
        if (output[index] == '\0') {
            if (count == (size_t)-1) return WRDE_NOSPACE;
            ++count;
        }
    }
    if (count > ((size_t)-1 / sizeof(char*)) - 1u)
        return WRDE_NOSPACE;
    words = (char**)calloc(count + 1u, sizeof(char*));
    if (!words) return WRDE_NOSPACE;
    for (index = 0u; index < length; ++index) {
        if (output[index] != '\0') continue;
        {
            size_t word_length = index - start;
            if (word_length == (size_t)-1) {
                wordexp_free_words(words, word_index);
                return WRDE_NOSPACE;
            }
            words[word_index] = (char*)malloc(word_length + 1u);
            if (!words[word_index]) {
                wordexp_free_words(words, word_index);
                return WRDE_NOSPACE;
            }
            memcpy(words[word_index], output + start, word_length);
            words[word_index][word_length] = '\0';
            ++word_index;
        }
        start = index + 1u;
    }
    *words_out = words;
    *count_out = count;
    return 0;
}

void wordfree(wordexp_t* word_vector) {
    size_t index;
    if (!word_vector) return;
    if (word_vector->we_wordv) {
        for (index = 0u; index < word_vector->we_wordc; ++index)
            free(word_vector->we_wordv[word_vector->we_offs + index]);
        free(word_vector->we_wordv);
    }
    word_vector->we_wordc = 0u;
    word_vector->we_wordv = NULL;
    word_vector->we_offs = 0u;
}

int wordexp(const char* words, wordexp_t* word_vector, int flags) {
    static const char prefix[] = "set -- ";
    static const char undef_prefix[] = "set -u\nset -- ";
    static const char suffix[] =
        "\nif [ \"$#\" -gt 0 ]; then printf '%s\\000' \"$@\"; fi\n";
    const char* script_prefix;
    size_t prefix_length;
    size_t input_length;
    size_t script_length;
    size_t offset;
    size_t old_count = 0u;
    size_t old_offset = 0u;
    size_t total_count;
    size_t vector_slots;
    int reuse = (flags & WRDE_REUSE) != 0;
    int append = (flags & WRDE_APPEND) != 0;
    int do_offsets = (flags & WRDE_DOOFFS) != 0;
    int result;
    char* script;
    char* output = NULL;
    size_t output_length = 0u;
    char** expanded_words = NULL;
    size_t expanded_count = 0u;
    char** vector;

    if (!words || !word_vector) return WRDE_BADVAL;
    if ((flags & ~WORDEXP_VALID_FLAGS) != 0 || (reuse && append))
        return WRDE_BADVAL;
    offset = do_offsets ? word_vector->we_offs : 0u;
    if (append) {
        if (!word_vector->we_wordv) return WRDE_BADVAL;
        old_count = word_vector->we_wordc;
        old_offset = word_vector->we_offs;
        if (do_offsets && offset != old_offset) return WRDE_BADVAL;
        offset = old_offset;
    } else if (!do_offsets) {
        offset = 0u;
    }

    input_length = strlen(words);
    if (input_length > (size_t)-1 - sizeof(prefix) - sizeof(suffix))
        return WRDE_NOSPACE;
    script_prefix = (flags & WRDE_UNDEF) != 0 ? undef_prefix : prefix;
    prefix_length = (flags & WRDE_UNDEF) != 0
        ? sizeof(undef_prefix) - 1u : sizeof(prefix) - 1u;
    if (prefix_length > (size_t)-1 - input_length - sizeof(suffix))
        return WRDE_NOSPACE;
    script_length = prefix_length + input_length + sizeof(suffix);
    script = (char*)malloc(script_length);
    if (!script) return WRDE_NOSPACE;
    memcpy(script, script_prefix, prefix_length);
    memcpy(script + prefix_length, words, input_length);
    memcpy(script + prefix_length + input_length,
           suffix, sizeof(suffix));

    if (reuse) {
        wordfree(word_vector);
        word_vector->we_offs = do_offsets ? offset : 0u;
    }
    result = wordexp_validate_fragment(
        script + prefix_length, input_length, (flags & WRDE_NOCMD) != 0);
    if (result != 0) {
        free(script);
        return result;
    }

    result = wordexp_shell_run(script, 0,
                               (flags & WRDE_SHOWERR) != 0, NULL, NULL);
    if (result == 0)
        result = wordexp_shell_run(script, 1,
                                   (flags & WRDE_SHOWERR) != 0,
                                   &output, &output_length);
    free(script);
    if (result != 0) {
        free(output);
        return result;
    }
    result = wordexp_decode_output(output, output_length,
                                   &expanded_words, &expanded_count);
    free(output);
    if (result != 0) return result;
    if (old_count > (size_t)-1 - expanded_count) {
        wordexp_free_words(expanded_words, expanded_count);
        return WRDE_NOSPACE;
    }
    total_count = old_count + expanded_count;
    if (total_count == (size_t)-1 ||
        offset > (size_t)-1 - total_count - 1u) {
        wordexp_free_words(expanded_words, expanded_count);
        return WRDE_NOSPACE;
    }
    vector_slots = offset + total_count + 1u;
    if (vector_slots > (size_t)-1 / sizeof(char*)) {
        wordexp_free_words(expanded_words, expanded_count);
        return WRDE_NOSPACE;
    }
    vector = (char**)calloc(vector_slots, sizeof(char*));
    if (!vector) {
        wordexp_free_words(expanded_words, expanded_count);
        return WRDE_NOSPACE;
    }
    if (append && old_count != 0u)
        memcpy(vector + offset, word_vector->we_wordv + old_offset,
               old_count * sizeof(char*));
    if (expanded_count != 0u)
        memcpy(vector + offset + old_count, expanded_words,
               expanded_count * sizeof(char*));
    free(expanded_words);
    if (append) free(word_vector->we_wordv);
    word_vector->we_wordc = total_count;
    word_vector->we_wordv = vector;
    word_vector->we_offs = offset;
    return 0;
}
