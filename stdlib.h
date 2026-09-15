/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - stdlib.h
 * 一般ユーティリティ
 */

#ifndef _STDLIB_H
#define _STDLIB_H

#include "stddef.h"
#include "stdint.h"
#include "limits.h"
#if defined(__cplusplus) && !defined(RIN_FREESTANDING) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <errno.h>
#else
#include "errno.h"
#endif
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
#include "locale.h"
#endif
#include "fcntl_flags.h"
#ifndef MIDL_PASS
#include "rin_aligned_alloc_meta.h"
#include "sys/syscall.h"
#include "internal/rin_integer_parse.h"
#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define RIN_FP_PARSE_BINARY80 1
#endif
#include "internal/rin_float_parse.h"
#undef RIN_FP_PARSE_BINARY80
#include "../libunicode/rin_unicode.h"
#endif /* !MIDL_PASS */

#ifndef _RIN_STDLIB_SYSCALL1
#define _RIN_STDLIB_SYSCALL1(number, arg1) \
    _syscall1((uintptr_t)(number), (uintptr_t)(arg1))
#endif
#ifndef _RIN_STDLIB_SYSCALL0
#define _RIN_STDLIB_SYSCALL0(number) \
    _syscall0((uintptr_t)(number))
#endif
#ifndef _RIN_STDLIB_SYSCALL2
#define _RIN_STDLIB_SYSCALL2(number, arg1, arg2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(arg1), (uintptr_t)(arg2))
#endif
#ifndef _RIN_STDLIB_SYSCALL3
#define _RIN_STDLIB_SYSCALL3(number, arg1, arg2, arg3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(arg1), \
              (uintptr_t)(arg2), (uintptr_t)(arg3))
#endif

/* `system()` is a process frontend, so keep each lifecycle operation
 * replaceable by a target owner or a linked contract test. */
#ifndef _RIN_STDLIB_SYSTEM_FORK
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING
#define _RIN_STDLIB_SYSTEM_FORK() _RIN_STDLIB_SYSCALL0(SYS_FORK)
#else
#define _RIN_STDLIB_SYSTEM_FORK() ((intptr_t)-ENOSYS)
#endif
#endif
#ifndef _RIN_STDLIB_SYSTEM_EXECVE
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING
#define _RIN_STDLIB_SYSTEM_EXECVE(pathname, arguments, environment) \
    _RIN_STDLIB_SYSCALL3(SYS_EXEC, (pathname), (arguments), (environment))
#else
#define _RIN_STDLIB_SYSTEM_EXECVE(pathname, arguments, environment) \
    ((void)(pathname), (void)(arguments), (void)(environment), \
     ((intptr_t)-ENOSYS))
#endif
#endif
#ifndef _RIN_STDLIB_SYSTEM_WAITPID
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING
#define _RIN_STDLIB_SYSTEM_WAITPID(pid, status, options) \
    _RIN_STDLIB_SYSCALL3(SYS_WAIT, (pid), (status), (options))
#else
#define _RIN_STDLIB_SYSTEM_WAITPID(pid, status, options) \
    ((void)(pid), (void)(status), (void)(options), ((intptr_t)-ENOSYS))
#endif
#endif
#ifndef _RIN_STDLIB_SYSTEM_EXIT
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING
#define _RIN_STDLIB_SYSTEM_EXIT(status) do { \
    (void)_RIN_STDLIB_SYSCALL1(SYS_EXIT, (uintptr_t)(intptr_t)(status)); \
    for (;;) {} \
} while (0)
#else
#define _RIN_STDLIB_SYSTEM_EXIT(status) ((void)(status))
#endif
#endif
#ifndef _RIN_STDLIB_SYSTEM_AVAILABLE
#define _RIN_STDLIB_SYSTEM_AVAILABLE() 0
#endif

#ifndef MIDL_PASS
#include "internal/rin_system_owner.h"
#endif /* !MIDL_PASS */

#ifndef RIN_STDLIB_SYSTEM_COMMAND_MAX
#define RIN_STDLIB_SYSTEM_COMMAND_MAX 4095u
#endif

#ifndef MIDL_PASS
#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * alloca - スタック上のメモリ確保
 * ═══════════════════════════════════════════════════════════════*/
#if defined(__GNUC__) || defined(__clang__)
#ifndef alloca
#define alloca(size) __builtin_alloca(size)
#endif

/* Status-only stdlib syscalls must not interpret a positive backend word as
 * an errno (or silently report success).  Keep target-width ownership until
 * this boundary and preserve documented negative errno publication. */
static inline int _rin_stdlib_status_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    if (result > (intptr_t)INT_MAX || result < (intptr_t)INT_MIN) {
        errno = EOVERFLOW;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}
#endif

/* Skip C inline definitions if C++ cstdlib has already provided them */
#if defined(__cplusplus) && defined(RINCXX_CSTDLIB_H)
#define _STDLIB_SKIP_INLINE_DEFS
#endif

/* A hosted C++ consumer can reach this Rin C header transitively through
 * stdio after its toolchain's <stdlib.h> is already active.  The host owns
 * the global C declarations in that configuration; defining Rin static
 * inline counterparts gives them incompatible linkage. */
#if defined(__cplusplus) && !defined(RIN_FREESTANDING) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (defined(RINCXX_CSTDLIB_HOSTED_C) || defined(_INC_STDLIB))
#define _RIN_STDLIB_HOSTED_CXX_OWNER
#define _STDLIB_SKIP_INLINE_DEFS
#endif

/* Hosted C consumers can arrive after MinGW's process.h through pthread.h.
 * Its exit/abort/system declarations own those global names, so retain Rin's
 * allocator and environment helpers but defer this process subset to CRT. */
#if !defined(__cplusplus) && !defined(RIN_FREESTANDING) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (defined(_INC_PROCESS) || defined(_CRT_TERMINATE_DEFINED))
#define _RIN_STDLIB_HOSTED_C_OWNER
#endif

#if !defined(__RIN_ENVIRON_DECLARED) && !defined(environ)
#define __RIN_ENVIRON_DECLARED
extern char** environ;
#endif

#ifndef malloc
void* malloc(size_t size);
#endif
#ifndef calloc
void* calloc(size_t nmemb, size_t size);
#endif
#ifndef realloc
void* realloc(void* ptr, size_t size);
#endif
#ifndef free
void free(void* ptr);
#endif
#ifndef malloc_usable_size
size_t malloc_usable_size(void* ptr);
#endif

/* ═══════════════════════════════════════════════════════════════
 * プロセス制御
 * ═══════════════════════════════════════════════════════════════*/

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

/* In C++, these are provided by cstdlib.h to avoid conflicts */
#if (!defined(__cplusplus) || !defined(RINCXX_CSTDLIB_H)) && \
    !defined(_RIN_STDLIB_HOSTED_CXX_OWNER) && \
    !defined(_RIN_STDLIB_HOSTED_C_OWNER)

#if !defined(_MSVCRT_COMPAT) && !defined(_RIN_STDLIB_HOSTED_CXX_OWNER) && \
    !defined(_RIN_STDLIB_HOSTED_C_OWNER)
void rin_exit(int status) __attribute__((noreturn));

__attribute__((noreturn)) static inline void exit(int status) {
    rin_exit(status);
}

__attribute__((noreturn)) static inline void _Exit(int status) {
    _RIN_STDLIB_SYSCALL1(SYS_EXIT, (uintptr_t)(intptr_t)status);
    for(;;) {} /* Never returns */
}
#endif /* !_MSVCRT_COMPAT */

#ifndef _MSVCRT_COMPAT
__attribute__((noreturn)) static inline void abort(void) {
    /* Debug: output abort message with stack trace via serial */
    const char* msg = "[ABORT] libc abort() called\n";
    _RIN_STDLIB_SYSCALL2(110, (uintptr_t)msg, (uintptr_t)29u);

    /* Print return address and stack frames */
    uintptr_t bp, ret_addr;
#if defined(__x86_64__) || defined(_M_X64)
    __asm__ volatile("mov %%rbp, %0" : "=r"(bp));
    const uintptr_t limit = UINTPTR_MAX / 2u + 1u;
    const int hex_digits = 60;  /* 16 hex digits for 64-bit */
#else
    __asm__ volatile("movl %%ebp, %0" : "=r"(bp));
    const uintptr_t limit = UINTPTR_MAX / 2u + 1u;
    const int hex_digits = 28;  /* 8 hex digits for 32-bit */
#endif

    const char* msg2 = "[ABORT] Stack trace:\n";
    _RIN_STDLIB_SYSCALL2(110, (uintptr_t)msg2, (uintptr_t)21u);

    /* Walk up to 8 stack frames */
    for (int i = 0; i < 8 && bp != 0 && bp < limit; i++) {
        ret_addr = ((uintptr_t*)bp)[1];
        if (ret_addr == 0) break;

        /* Print frame: "  [N] 0xXXXXXXXX...\n" */
        char buf[32];
        char* p = buf;
        *p++ = ' '; *p++ = ' '; *p++ = '[';
        *p++ = '0' + i;
        *p++ = ']'; *p++ = ' '; *p++ = '0'; *p++ = 'x';
        for (int j = hex_digits; j >= 0; j -= 4) {
            int d = (ret_addr >> j) & 0xF;
            *p++ = d < 10 ? '0' + d : 'A' + d - 10;
        }
        *p++ = '\n'; *p = 0;
        _RIN_STDLIB_SYSCALL2(110, (uintptr_t)buf,
                             (uintptr_t)(p - buf));

        bp = ((uintptr_t*)bp)[0];
    }

    _RIN_STDLIB_SYSCALL1(SYS_EXIT, (uintptr_t)1u);
    for(;;) {}
}

/* atexit - 宣言のみ、実装はmalloc.cにある */
int atexit(void (*func)(void));

/* C11/C++11 quick termination owns a callback registry distinct from normal
 * atexit/C++ destructor finalization.  The implementation lives with the
 * shared application runtime registry so callbacks have process lifetime. */
int at_quick_exit(void (*func)(void));
__attribute__((noreturn)) void quick_exit(int status);
#endif /* !_MSVCRT_COMPAT */
#endif /* !__cplusplus || !RINCXX_CSTDLIB_H */

/* ═══════════════════════════════════════════════════════════════
 * 環境変数
 * ═══════════════════════════════════════════════════════════════*/

#if !defined(_MSVCRT_COMPAT) && !defined(_RIN_STDLIB_HOSTED_CXX_OWNER)
#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
/* カーネル環境変数API (env.c) */
extern char* rin_getenv(const char* name);
extern int   rin_setenv(const char* name, const char* value, int overwrite);
extern int   rin_unsetenv(const char* name);
extern int   rin_putenv(char* string);

static inline char* getenv(const char* name) {
    return rin_getenv(name);
}

static inline int setenv(const char* name, const char* value, int overwrite) {
    return rin_setenv(name, value, overwrite);
}

static inline int unsetenv(const char* name) {
    return rin_unsetenv(name);
}

static inline int putenv(char* string) {
    return rin_putenv(string);
}

#elif defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
/* Userspace environment support backed by the process-local environ table. */
static inline size_t __rin_env_strlen(char const* string)
{
    size_t length = 0;
    if (!string)
        return 0;
    while (string[length] != '\0')
        ++length;
    return length;
}

static inline int __rin_env_name_is_valid(char const* name)
{
    if (!name || *name == '\0')
        return 0;

    for (char const* ch = name; *ch != '\0'; ++ch) {
        if (*ch == '=')
            return 0;
    }

    return 1;
}

static inline int __rin_env_name_matches(char const* entry, char const* name)
{
    size_t index = 0;

    if (!entry || !name)
        return 0;

    while (name[index] != '\0') {
        if (entry[index] != name[index])
            return 0;
        ++index;
    }

    return entry[index] == '=';
}

static inline size_t __rin_env_count(void)
{
    size_t count = 0;

    if (!environ)
        return 0;

    while (environ[count] != NULL)
        ++count;

    return count;
}

static inline char* __rin_env_make_entry(char const* name, char const* value)
{
    size_t name_length;
    size_t value_length;
    char* entry;
    size_t offset = 0;

    if (!name || !value) {
        errno = EINVAL;
        return NULL;
    }

    name_length = __rin_env_strlen(name);
    value_length = __rin_env_strlen(value);
    entry = (char*)malloc(name_length + value_length + 2);
    if (!entry) {
        errno = ENOMEM;
        return NULL;
    }

    for (size_t i = 0; i < name_length; ++i)
        entry[offset++] = name[i];
    entry[offset++] = '=';
    for (size_t i = 0; i < value_length; ++i)
        entry[offset++] = value[i];
    entry[offset] = '\0';
    return entry;
}

static inline char** __rin_env_clone_table(size_t entry_count)
{
    char** table = (char**)malloc(sizeof(char*) * (entry_count + 1));

    if (!table) {
        errno = ENOMEM;
        return NULL;
    }

    for (size_t i = 0; i < entry_count; ++i)
        table[i] = (environ && environ[i]) ? environ[i] : NULL;
    table[entry_count] = NULL;
    return table;
}

static inline int __rin_env_find_index(char const* name)
{
    size_t index = 0;

    if (!environ || !__rin_env_name_is_valid(name))
        return -1;

    while (environ[index] != NULL) {
        if (__rin_env_name_matches(environ[index], name))
            return (int)index;
        ++index;
    }

    return -1;
}

static inline int __rin_env_install_empty_table(void)
{
    char** empty_table = (char**)malloc(sizeof(char*));

    if (!empty_table) {
        errno = ENOMEM;
        return -1;
    }

    empty_table[0] = NULL;
    environ = empty_table;
    return 0;
}

static inline char* getenv(const char* name) {
    size_t name_length;
    int index;

    if (!__rin_env_name_is_valid(name))
        return NULL;

    index = __rin_env_find_index(name);
    if (index < 0)
        return NULL;

    name_length = __rin_env_strlen(name);
    return environ[index] + name_length + 1;
}

static inline int setenv(const char* name, const char* value, int overwrite) {
    size_t count;
    int index;
    char* new_entry;
    char** new_table;

    if (!__rin_env_name_is_valid(name) || !value) {
        errno = EINVAL;
        return -1;
    }

    count = __rin_env_count();
    index = __rin_env_find_index(name);

    if (index >= 0 && !overwrite)
        return 0;

    new_entry = __rin_env_make_entry(name, value);
    if (!new_entry)
        return -1;

    new_table = __rin_env_clone_table(index >= 0 ? count : count + 1);
    if (!new_table)
        return -1;

    if (index >= 0) {
        new_table[index] = new_entry;
    } else {
        new_table[count] = new_entry;
        new_table[count + 1] = NULL;
    }

    environ = new_table;
    return 0;
}

static inline int unsetenv(const char* name) {
    size_t count;
    int index;
    char** new_table;
    size_t write_index = 0;

    if (!__rin_env_name_is_valid(name)) {
        errno = EINVAL;
        return -1;
    }

    count = __rin_env_count();
    index = __rin_env_find_index(name);
    if (index < 0)
        return 0;

    if (count == 1)
        return __rin_env_install_empty_table();

    new_table = (char**)malloc(sizeof(char*) * count);
    if (!new_table) {
        errno = ENOMEM;
        return -1;
    }

    for (size_t read_index = 0; read_index < count; ++read_index) {
        if ((int)read_index == index)
            continue;
        new_table[write_index++] = environ[read_index];
    }
    new_table[write_index] = NULL;

    environ = new_table;
    return 0;
}

static inline int putenv(char* string) {
    char* equals = NULL;
    size_t name_length;
    char* name;

    if (!string) {
        errno = EINVAL;
        return -1;
    }

    for (char* ch = string; *ch != '\0'; ++ch) {
        if (*ch == '=') {
            equals = ch;
            break;
        }
    }

    if (!equals || equals == string) {
        errno = EINVAL;
        return -1;
    }

    name_length = (size_t)(equals - string);
    name = (char*)malloc(name_length + 1);
    if (!name) {
        errno = ENOMEM;
        return -1;
    }

    for (size_t i = 0; i < name_length; ++i)
        name[i] = string[i];
    name[name_length] = '\0';

    return setenv(name, equals + 1, 1);
}

#else
/* ホスト環境用スタブ */
static inline char* getenv(const char* name) {
    (void)name;
    return NULL;
}

static inline int setenv(const char* name, const char* value, int overwrite) {
    (void)name; (void)value; (void)overwrite;
    return -1;
}

static inline int unsetenv(const char* name) {
    (void)name;
    return -1;
}

static inline int putenv(char* string) {
    (void)string;
    return -1;
}
#endif /* RIN_FREESTANDING */
#endif /* !_MSVCRT_COMPAT */

#if !defined(_MSVCRT_COMPAT) && !defined(_RIN_STDLIB_HOSTED_CXX_OWNER)
static inline char* secure_getenv(const char* name) {
    return getenv(name);
}

static inline int clearenv(void) {
#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
    return __rin_env_install_empty_table();
#else
    if (environ) {
        environ[0] = NULL;
    } else {
        errno = ENOSYS;
        return -1;
    }
    return 0;
#endif
}
#endif /* !_MSVCRT_COMPAT */

/* ═══════════════════════════════════════════════════════════════
 * 数値変換
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _STDLIB_SKIP_INLINE_DEFS

#ifndef _ATOF_DEFINED
#define _ATOF_DEFINED
static inline double atof(const char* s) {
    rin_float_parse_result parsed = rin_float_parse_binary64(s);
    if (parsed.invalid_input) errno = EINVAL;
    else if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
    return rin_float_parse_binary64_value(parsed.bits);
}
#endif /* _ATOF_DEFINED */

static inline long strtol(const char* s, char** endptr, int base) {
    const unsigned long long positive_limit =
        (unsigned long long)__LONG_MAX__;
    const unsigned long long negative_limit = positive_limit + 1ULL;
    rin_integer_parse_result parsed = rin_integer_parse(
        s, base, positive_limit, negative_limit);

    if (endptr)
        *endptr = (char*)(__UINTPTR_TYPE__)parsed.end;
    if (parsed.invalid_input || parsed.invalid_base) {
        errno = EINVAL;
        return 0;
    }
    if (parsed.overflow) {
        errno = ERANGE;
        return parsed.negative ? (-__LONG_MAX__ - 1L) : __LONG_MAX__;
    }
    if (!parsed.any)
        return 0;
    if (!parsed.negative)
        return (long)parsed.magnitude;
    if (parsed.magnitude == negative_limit)
        return -__LONG_MAX__ - 1L;
    return -(long)parsed.magnitude;
}

static inline unsigned long strtoul(const char* s, char** endptr, int base) {
    const unsigned long maximum =
        (unsigned long)__LONG_MAX__ * 2UL + 1UL;
    rin_integer_parse_result parsed = rin_integer_parse(
        s, base, (unsigned long long)maximum,
        (unsigned long long)maximum);

    if (endptr)
        *endptr = (char*)(__UINTPTR_TYPE__)parsed.end;
    if (parsed.invalid_input || parsed.invalid_base) {
        errno = EINVAL;
        return 0;
    }
    if (parsed.overflow) {
        errno = ERANGE;
        return maximum;
    }
    if (!parsed.any)
        return 0;
    return parsed.negative
        ? 0UL - (unsigned long)parsed.magnitude
        : (unsigned long)parsed.magnitude;
}

static inline long long strtoll(const char* s, char** endptr, int base) {
    const unsigned long long positive_limit =
        (unsigned long long)__LONG_LONG_MAX__;
    const unsigned long long negative_limit = positive_limit + 1ULL;
    rin_integer_parse_result parsed = rin_integer_parse(
        s, base, positive_limit, negative_limit);

    if (endptr)
        *endptr = (char*)(__UINTPTR_TYPE__)parsed.end;
    if (parsed.invalid_input || parsed.invalid_base) {
        errno = EINVAL;
        return 0;
    }
    if (parsed.overflow) {
        errno = ERANGE;
        return parsed.negative
            ? (-__LONG_LONG_MAX__ - 1LL) : __LONG_LONG_MAX__;
    }
    if (!parsed.any)
        return 0;
    if (!parsed.negative)
        return (long long)parsed.magnitude;
    if (parsed.magnitude == negative_limit)
        return -__LONG_LONG_MAX__ - 1LL;
    return -(long long)parsed.magnitude;
}

static inline unsigned long long strtoull(const char* s, char** endptr, int base) {
    const unsigned long long maximum =
        (unsigned long long)__LONG_LONG_MAX__ * 2ULL + 1ULL;
    rin_integer_parse_result parsed = rin_integer_parse(
        s, base, maximum, maximum);

    if (endptr)
        *endptr = (char*)(__UINTPTR_TYPE__)parsed.end;
    if (parsed.invalid_input || parsed.invalid_base) {
        errno = EINVAL;
        return 0;
    }
    if (parsed.overflow) {
        errno = ERANGE;
        return maximum;
    }
    if (!parsed.any)
        return 0;
    return parsed.negative ? 0ULL - parsed.magnitude : parsed.magnitude;
}

#ifndef atoi
static inline int atoi(const char* s) {
    const unsigned long long positive_limit =
        (unsigned long long)__INT_MAX__;
    const unsigned long long negative_limit = positive_limit + 1ULL;
    rin_integer_parse_result parsed = rin_integer_parse(
        s, 10, positive_limit, negative_limit);

    if (parsed.invalid_input) {
        errno = EINVAL;
        return 0;
    }
    if (parsed.overflow) {
        errno = ERANGE;
        return parsed.negative ? (-__INT_MAX__ - 1) : __INT_MAX__;
    }
    if (!parsed.any)
        return 0;
    if (!parsed.negative)
        return (int)parsed.magnitude;
    if (parsed.magnitude == negative_limit)
        return -__INT_MAX__ - 1;
    return -(int)parsed.magnitude;
}
#endif /* atoi */

static inline long atol(const char* s) {
    return strtol(s, (char**)0, 10);
}

static inline long long atoll(const char* s) {
    return strtoll(s, (char**)0, 10);
}

/* 浮動小数点数変換 */
static inline double strtod(const char* s, char** endptr) {
    rin_float_parse_result parsed = rin_float_parse_binary64(s);
    if (endptr) *endptr = (char*)(__UINTPTR_TYPE__)parsed.end;
    if (parsed.invalid_input) errno = EINVAL;
    else if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
    return rin_float_parse_binary64_value(parsed.bits);
}

static inline float strtof(const char* s, char** endptr) {
    rin_float_parse_result parsed = rin_float_parse_binary32(s);
    if (endptr) *endptr = (char*)(__UINTPTR_TYPE__)parsed.end;
    if (parsed.invalid_input) errno = EINVAL;
    else if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
    return rin_float_parse_binary32_value(parsed.bits);
}

static inline long double strtold(const char* s, char** endptr) {
#if defined(RIN_FP_HAS_BINARY80)
    rin_long_double_parse_result parsed = rin_float_parse_binary80(s);
    if (endptr) *endptr = (char*)(__UINTPTR_TYPE__)parsed.end;
    if (parsed.invalid_input) errno = EINVAL;
    else if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
    return parsed.value;
#else
    return (long double)strtod(s, endptr);
#endif
}

#else
/* When <cstdlib> comes first in a freestanding C++ translation unit, it
 * owns the conversion implementations in namespace std and suppresses the
 * Rin C inline definitions above.  Keep the C header surface available for
 * C headers which depend on these global names (for example <inttypes.h>). */
#if defined(__cplusplus) && defined(RINCXX_CSTDLIB_H) && \
    !defined(_RIN_STDLIB_HOSTED_CXX_OWNER)
#ifndef _ATOF_DEFINED
#define _ATOF_DEFINED
static inline double atof(const char* s) {
    return std::atof(s);
}
#endif /* _ATOF_DEFINED */

static inline long strtol(const char* s, char** endptr, int base) {
    return std::strtol(s, endptr, base);
}

static inline unsigned long strtoul(const char* s, char** endptr, int base) {
    return std::strtoul(s, endptr, base);
}

static inline long long strtoll(const char* s, char** endptr, int base) {
    return std::strtoll(s, endptr, base);
}

static inline unsigned long long strtoull(const char* s, char** endptr,
                                          int base) {
    return std::strtoull(s, endptr, base);
}

#ifndef atoi
static inline int atoi(const char* s) {
    return std::atoi(s);
}
#endif /* atoi */

static inline long atol(const char* s) {
    return std::atol(s);
}

static inline long long atoll(const char* s) {
    return std::atoll(s);
}

static inline double strtod(const char* s, char** endptr) {
    return std::strtod(s, endptr);
}

static inline float strtof(const char* s, char** endptr) {
    return std::strtof(s, endptr);
}

static inline long double strtold(const char* s, char** endptr) {
    return std::strtold(s, endptr);
}
#endif /* C++ <cstdlib> freestanding bridge */
#endif /* !_STDLIB_SKIP_INLINE_DEFS - 数値変換 */

/* libc++'s locale adapters use the POSIX locale-aware conversion entrypoints.
 * RinOS currently has one UTF-8 numeric grammar, so these wrappers preserve
 * the conversion contract while accepting the selected locale object. */
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
static inline long long strtoll_l(const char* s, char** endptr, int base,
                                  locale_t locale) {
    (void)locale;
    return strtoll(s, endptr, base);
}

static inline unsigned long long strtoull_l(const char* s, char** endptr,
                                            int base, locale_t locale) {
    (void)locale;
    return strtoull(s, endptr, base);
}

static inline float strtof_l(const char* s, char** endptr, locale_t locale) {
    (void)locale;
    return strtof(s, endptr);
}

static inline double strtod_l(const char* s, char** endptr, locale_t locale) {
    (void)locale;
    return strtod(s, endptr);
}

static inline long double strtold_l(const char* s, char** endptr,
                                    locale_t locale) {
    (void)locale;
    return strtold(s, endptr);
}
#endif /* RIN_USERSPACE && !_MSVCRT_COMPAT */

#ifndef _RIN_STDLIB_HOSTED_CXX_OWNER

static inline int _rin_is_power_of_two_size(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

static inline void* _rin_aligned_alloc_internal(size_t alignment, size_t size,
                                                int require_size_multiple) {
    size_t total;
    size_t aligned_addr;
    size_t addr;
    size_t max_value = (size_t)-1;
    size_t meta_bytes = rin_aligned_allocation_meta_size();
    void* raw;

    if (alignment < sizeof(void*)) return NULL;
    if (!_rin_is_power_of_two_size(alignment)) return NULL;
    if (require_size_multiple && ((size & (alignment - 1)) != 0)) return NULL;

    if (size > (max_value - alignment)) return NULL;
    total = size + alignment;
    if (total > (max_value - meta_bytes)) return NULL;
    total += meta_bytes;

    raw = malloc(total);
    if (!raw) return NULL;

    addr = (size_t)raw + meta_bytes;
    if (addr > (max_value - (alignment - 1))) {
        free(raw);
        return NULL;
    }

    aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    rin_store_aligned_allocation_metadata((void*)aligned_addr, raw);
    return (void*)aligned_addr;
}

/* aligned_alloc - C11準拠アラインドメモリ確保 */
static inline void* aligned_alloc(size_t alignment, size_t size) {
    return _rin_aligned_alloc_internal(alignment, size, 1);
}

/* GNU/POSIX compatibility used by bundled native libraries.  Unlike
 * aligned_alloc, memalign does not require size to be a multiple of the
 * alignment. */
static inline void* memalign(size_t alignment, size_t size) {
    return _rin_aligned_alloc_internal(alignment, size, 0);
}

/* posix_memalign - POSIX準拠アラインドメモリ確保 */
static inline int posix_memalign(void** memptr, size_t alignment, size_t size) {
    void* ptr;
    if (!memptr) return 22;  /* EINVAL */
    if (alignment < sizeof(void*) || !_rin_is_power_of_two_size(alignment)) return 22;  /* EINVAL */
    ptr = _rin_aligned_alloc_internal(alignment, size, 0);
    if (!ptr) return 12;  /* ENOMEM */
    *memptr = ptr;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * 文字列複製 (POSIX) - mallocの後に定義
 * ═══════════════════════════════════════════════════════════════*/

/* Simple inline strlen for strdup (to avoid dependency on string.h) */
static inline size_t _strdup_strlen(const char* s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

#ifndef strdup
static inline char* strdup(const char* s) {
    if (!s) return NULL;
    size_t len = _strdup_strlen(s) + 1;
    char* dup = (char*)malloc(len);
    if (dup) {
        const char* src = s;
        char* dst = dup;
        while (len--) *dst++ = *src++;
    }
    return dup;
}
#endif

static inline char* strndup(const char* s, size_t n) {
    if (!s) return NULL;
    size_t len = _strdup_strlen(s);
    if (len > n) len = n;
    char* dup = (char*)malloc(len + 1);
    if (dup) {
        const char* src = s;
        char* dst = dup;
        size_t i;
        for (i = 0; i < len; i++) *dst++ = *src++;
        *dst = '\0';
    }
    return dup;
}

/* ═══════════════════════════════════════════════════════════════
 * 算術関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int abs(int n) {
    if (n >= 0) return n;
    /* Form the magnitude in the unsigned domain so INT_MIN does not invoke
     * signed overflow.  Converting the modulo result back preserves the
     * implementation's two's-complement minimum representation. */
    return (int)(0u - (unsigned int)n);
}

static inline long labs(long n) {
    if (n >= 0) return n;
    return (long)(0UL - (unsigned long)n);
}

static inline long long llabs(long long n) {
    if (n >= 0) return n;
    return (long long)(0ULL - (unsigned long long)n);
}

typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;
typedef struct { long long quot; long long rem; } lldiv_t;

static inline div_t div(int numer, int denom) {
    div_t r = { numer / denom, numer % denom };
    return r;
}

static inline ldiv_t ldiv(long numer, long denom) {
    ldiv_t r = { numer / denom, numer % denom };
    return r;
}

static inline lldiv_t lldiv(long long numer, long long denom) {
    lldiv_t r = { numer / denom, numer % denom };
    return r;
}

/* ═══════════════════════════════════════════════════════════════
 * Deterministic C random numbers (not a cryptographic generator)
 * ═══════════════════════════════════════════════════════════════*/

#define RAND_MAX 0x7FFFFFFF

#if !defined(__cplusplus) || !defined(RINCXX_CSTDLIB_H)
int rand(void);
void srand(unsigned int seed);
long random(void);
void srandom(unsigned int seed);
#endif

/* ═══════════════════════════════════════════════════════════════
 * ソート・検索
 * ═══════════════════════════════════════════════════════════════*/

typedef int (*_qsort_cmp_t)(const void*, const void*);

static inline void qsort(void* base, size_t nmemb, size_t size, _qsort_cmp_t compar) {
    /* 簡易バブルソート */
    unsigned char* arr = (unsigned char*)base;
    if (!arr || !compar || size == 0u || nmemb < 2u ||
        nmemb > (size_t)-1 / size)
        return;
    for (size_t i = 0; i < nmemb - 1; i++) {
        for (size_t j = 0; j < nmemb - i - 1; j++) {
            void* a = arr + j * size;
            void* b = arr + (j + 1) * size;
            if (compar(a, b) > 0) {
                /* スワップ */
                for (size_t k = 0; k < size; k++) {
                    unsigned char tmp = ((unsigned char*)a)[k];
                    ((unsigned char*)a)[k] = ((unsigned char*)b)[k];
                    ((unsigned char*)b)[k] = tmp;
                }
            }
        }
    }
}

static inline void* bsearch(const void* key, const void* base, size_t nmemb,
                            size_t size, _qsort_cmp_t compar) {
    const unsigned char* arr = (const unsigned char*)base;
    size_t low = 0, high = nmemb;
    if (!key || !arr || !compar || size == 0u ||
        nmemb > (size_t)-1 / size)
        return NULL;

    while (low < high) {
        size_t mid = low + (high - low) / 2;
        const void* elem = arr + mid * size;
        int cmp = compar(key, elem);
        if (cmp == 0) return (void*)(__UINTPTR_TYPE__)elem;
        if (cmp < 0) high = mid;
        else low = mid + 1;
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════
 * 一時ファイル
 * ═══════════════════════════════════════════════════════════════*/

static inline int _rin_temp_random_suffix(char suffix[6]) {
    static const char characters[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    unsigned char random_bytes[16];
    size_t generated = 0;

    while (generated < 6) {
        intptr_t received = _RIN_STDLIB_SYSCALL3(
            __NR_getrandom, (uintptr_t)random_bytes,
            (uintptr_t)sizeof(random_bytes), (uintptr_t)0u);
        if (received < 0 && received >= -4095) {
            errno = (int)-received;
            return -1;
        }
        if (received <= 0 ||
            (uintptr_t)received > (uintptr_t)sizeof(random_bytes)) {
            errno = EIO;
            return -1;
        }
        for (size_t index = 0u;
             index < (size_t)(uintptr_t)received && generated < 6u;
             ++index) {
            /* 248 is the largest multiple of 62 below 256.  Rejection keeps
             * every filename character equally likely. */
            if (random_bytes[index] < 248u) {
                suffix[generated++] =
                    characters[random_bytes[index] % 62u];
            }
        }
    }
    for (size_t index = 0; index < sizeof(random_bytes); ++index)
        random_bytes[index] = 0;
    return 0;
}

/* mkstemp - create a unique temporary file */
static inline int mkstemp(char* tmpl) {
    if (!tmpl) {
        errno = EINVAL;
        return -1;
    }

    /* Find the XXXXXX suffix */
    size_t len = 0;
    char* p = tmpl;
    while (*p) { len++; p++; }

    if (len < 6) {
        errno = EINVAL;
        return -1;
    }

    /* Check for XXXXXX suffix */
    char* suffix = tmpl + len - 6;
    for (int i = 0; i < 6; i++) {
        if (suffix[i] != 'X') {
            errno = EINVAL;
            return -1;
        }
    }

    for (int attempt = 0; attempt < 100; attempt++) {
        intptr_t result;
        if (_rin_temp_random_suffix(suffix) != 0) return -1;

        result = _RIN_STDLIB_SYSCALL3(
            SYS_OPEN, (uintptr_t)tmpl,
            (uintptr_t)(O_RDWR | O_CREAT | O_EXCL), (uintptr_t)0600u);
        if (result >= 0) {
            if ((uintptr_t)result > (uintptr_t)INT_MAX) {
                (void)_RIN_STDLIB_SYSCALL1(SYS_CLOSE, (uintptr_t)result);
                errno = EOVERFLOW;
                return -1;
            }
            return (int)result;
        }
        if (result != -EEXIST) {
            errno = (result >= -4095) ? (int)-result : EIO;
            return -1;
        }
    }

    errno = EEXIST;
    return -1;  /* Failed to create unique file */
}

/* mkstemps - mkstemp with suffix length */
static inline int mkstemps(char* tmpl, int suffixlen) {
    if (!tmpl || suffixlen < 0) {
        errno = EINVAL;
        return -1;
    }

    size_t len = 0;
    char* p = tmpl;
    while (*p) { len++; p++; }

    if ((size_t)suffixlen > len || len - (size_t)suffixlen < 6) {
        errno = EINVAL;
        return -1;
    }

    /* Point to XXXXXX before the suffix */
    char* pattern = tmpl + len - 6 - suffixlen;
    for (int i = 0; i < 6; i++) {
        if (pattern[i] != 'X') {
            errno = EINVAL;
            return -1;
        }
    }

    for (int attempt = 0; attempt < 100; attempt++) {
        intptr_t result;
        if (_rin_temp_random_suffix(pattern) != 0) return -1;

        result = _RIN_STDLIB_SYSCALL3(
            SYS_OPEN, (uintptr_t)tmpl,
            (uintptr_t)(O_RDWR | O_CREAT | O_EXCL), (uintptr_t)0600u);
        if (result >= 0) {
            if ((uintptr_t)result > (uintptr_t)INT_MAX) {
                (void)_RIN_STDLIB_SYSCALL1(SYS_CLOSE, (uintptr_t)result);
                errno = EOVERFLOW;
                return -1;
            }
            return (int)result;
        }
        if (result != -EEXIST) {
            errno = (result >= -4095) ? (int)-result : EIO;
            return -1;
        }
    }

    errno = EEXIST;
    return -1;
}

/* mkdtemp - create a unique temporary directory */
static inline char* mkdtemp(char* tmpl) {
    if (!tmpl) {
        errno = EINVAL;
        return NULL;
    }

    size_t len = 0;
    char* p = tmpl;
    while (*p) { len++; p++; }

    if (len < 6) {
        errno = EINVAL;
        return NULL;
    }

    char* suffix = tmpl + len - 6;
    for (int i = 0; i < 6; i++) {
        if (suffix[i] != 'X') {
            errno = EINVAL;
            return NULL;
        }
    }

    for (int attempt = 0; attempt < 100; attempt++) {
        intptr_t result;
        if (_rin_temp_random_suffix(suffix) != 0) return NULL;

        result = _RIN_STDLIB_SYSCALL2(
            SYS_MKDIR, (uintptr_t)tmpl, (uintptr_t)0700u);
        if (result == 0) return tmpl;
        if (result != -EEXIST) {
            (void)_rin_stdlib_status_result(result);
            return NULL;
        }
    }

    errno = EEXIST;
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════
 * システム
 * ═══════════════════════════════════════════════════════════════*/

#if !defined(_RIN_STDLIB_HOSTED_C_OWNER)
#ifndef MIDL_PASS
static inline int system(const char* command) {
    return _rin_system_owner(command);
}
#else
int system(const char* command);
#endif /* !MIDL_PASS */
#endif

/* ═══════════════════════════════════════════════════════════════
 * マルチバイト/ワイド文字変換 (簡易実装)
 * ═══════════════════════════════════════════════════════════════*/

#ifndef MB_CUR_MAX
#define MB_CUR_MAX 4
#endif

static inline int mblen(const char* s, size_t n) {
    rin_unicode_mbstate_t st;
    size_t rc;
    st.state = 0;
    st.codepoint = 0;
    if (!s) return 0;
    rc = rin_unicode_mbrlen(s, n, &st);
    if (rc == (size_t)-1 || rc == (size_t)-2) return -1;
    return (int)rc;
}

/* wchar.hとの重複を避けるためのガード */
#ifndef _STDLIB_WCHAR_FUNCS_DEFINED
#define _STDLIB_WCHAR_FUNCS_DEFINED

#ifndef _MSVCRT_COMPAT
static inline int mbtowc(wchar_t* pwc, const char* s, size_t n) {
    return rin_unicode_mbtowc32((uint32_t*)pwc, s, n);
}

static inline int wctomb(char* s, wchar_t wc) {
    if (!s) return 0;
    return rin_unicode_wctomb32(s, (uint32_t)wc);
}

static inline size_t mbstowcs(wchar_t* dest, const char* src, size_t n) {
    return rin_unicode_mbstowcs32((uint32_t*)dest, src, n);
}

static inline size_t wcstombs(char* dest, const wchar_t* src, size_t n) {
    return rin_unicode_wcstombs32(dest, (const uint32_t*)src, n);
}
#endif /* !_MSVCRT_COMPAT */

#endif /* _STDLIB_WCHAR_FUNCS_DEFINED */

#endif /* !_RIN_STDLIB_HOSTED_CXX_OWNER */

#undef _RIN_STDLIB_HOSTED_C_OWNER
#undef _RIN_STDLIB_HOSTED_CXX_OWNER

#ifdef __cplusplus
}
#endif
#endif /* !MIDL_PASS */

#endif /* _STDLIB_H */
