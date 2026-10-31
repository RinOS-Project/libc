/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - stdlib.h
 * 一般ユーティリティ
 */

#ifndef _STDLIB_H
#define _STDLIB_H
#if !defined(__cplusplus) && !defined(RIN_FREESTANDING) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    defined(__MINGW32__) && !defined(_INC_ERRNO)
/* MinGW has no POSIX errno header owner for Rin's compatibility declarations.
 * Select the compiler CRT's errno before this header installs any wrappers. */
#include_next <errno.h>
#endif
#if defined(__cplusplus) && !defined(RIN_FREESTANDING) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    defined(RINCXX_CSTDLIB_H)
#define _RIN_STDLIB_HOSTED_CXX_OWNER
#endif
#define __STDC_VERSION_STDLIB_H__ 202311L


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
#if !defined(MIDL_PASS) && !defined(_MSVCRT_COMPAT) && \
    (defined(__RINOS__) || defined(RIN_USERSPACE) || \
     defined(RIN_FREESTANDING))
#include "signal.h"
#define _RIN_STDLIB_HAS_SIGNAL_API 1
#endif
#if !defined(__cplusplus) && !defined(RIN_FREESTANDING) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__
#define _RIN_STDLIB_HOSTED_SYSTEM_CRT_OWNER
#endif
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
    waitpid((pid_t)(pid), (status), (int)(options))
#else
#define _RIN_STDLIB_SYSTEM_WAITPID(pid, status, options) \
    ((void)(pid), (void)(status), (void)(options), ((intptr_t)-ENOSYS))
#endif
#endif
#ifndef _RIN_STDLIB_SYSTEM_WAITPID_RAW
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING
#define _RIN_STDLIB_SYSTEM_WAITPID_RAW(pid, status, options) \
    _RIN_STDLIB_SYSCALL3(SYS_WAIT, (pid), (status), (options))
#else
#define _RIN_STDLIB_SYSTEM_WAITPID_RAW(pid, status, options) \
    ((void)(pid), (void)(status), (void)(options), ((intptr_t)-ENOSYS))
#endif
#endif
#ifndef _RIN_STDLIB_SYSTEM_KILL_RAW
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING
#define _RIN_STDLIB_SYSTEM_KILL_RAW(pid, signal_number) \
    _RIN_STDLIB_SYSCALL2(SYS_KILL, (pid), (signal_number))
#else
#define _RIN_STDLIB_SYSTEM_KILL_RAW(pid, signal_number) \
    ((void)(pid), (void)(signal_number), ((intptr_t)-ENOSYS))
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
#ifndef MIDL_PASS
#ifdef __cplusplus
extern "C" {
#endif
int __rin_stdlib_system_available(void);
#ifdef __cplusplus
}
#endif
#endif
#define _RIN_STDLIB_SYSTEM_AVAILABLE() __rin_stdlib_system_available()
#endif

#ifndef MIDL_PASS
#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
#include "internal/rin_environment_runtime.h"
#endif
#if !defined(RINSDK_SPLIT_BUILD) && !defined(RIN_KERNEL) && \
    !defined(_RIN_STDLIB_HOSTED_SYSTEM_CRT_OWNER) && \
    !defined(_RIN_STDLIB_HOSTED_C_OWNER) && \
    !defined(_RIN_STDLIB_HOSTED_CXX_OWNER)
/* The system owner can include unistd.h, which pulls in poll.h and the
 * freestanding stdio implementation. Publish allocator and temporary-file
 * declarations before that recursive include path reaches inline bodies. */
#ifdef __cplusplus
extern "C" {
#endif
#ifndef malloc
void* malloc(size_t size);
#endif
#ifndef realloc
void* realloc(void* ptr, size_t size);
#endif
#ifndef free
void free(void* ptr);
#endif
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
static inline int mkostemp(char* tmpl, int flags);
#ifdef __cplusplus
}
#endif
#include "internal/rin_system_owner.h"
#endif
#endif /* !MIDL_PASS */

#ifndef RIN_STDLIB_SYSTEM_COMMAND_MAX
#define RIN_STDLIB_SYSTEM_COMMAND_MAX 4095u
#endif

#ifndef MIDL_PASS
#ifdef __cplusplus
extern "C" {
#endif

/* POSIX pseudo-terminal allocation API. */
int posix_openpt(int oflag);
int grantpt(int fd);
int unlockpt(int fd);
char* ptsname(int fd);
int ptsname_r(int fd, char* buffer, size_t buffer_size);

/* ═══════════════════════════════════════════════════════════════
 * alloca - スタック上のメモリ確保
 * ═══════════════════════════════════════════════════════════════*/
#if defined(__GNUC__) || defined(__clang__)
#ifndef alloca
#define alloca(size) __builtin_alloca(size)
#endif
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
void* reallocarray(void* ptr, size_t nmemb, size_t size);
size_t memalignment(const void* ptr);
#if !defined(__cplusplus)
int strfromd(char* restrict s, size_t n, const char* restrict format,
             double fp);
int strfromf(char* restrict s, size_t n, const char* restrict format,
             float fp);
int strfroml(char* restrict s, size_t n, const char* restrict format,
             long double fp);
#endif
#ifndef free
void free(void* ptr);
#endif
void free_sized(void* ptr, size_t size);
void free_aligned_sized(void* ptr, size_t alignment, size_t size);
#ifndef malloc_usable_size
size_t malloc_usable_size(void* ptr);
#endif

/* POSIX realpath() is exported by the libc runtime. */
#if defined(RIN_USERSPACE) && RIN_USERSPACE
char* realpath(const char* path, char* resolved_path);
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
#ifndef _RIN_CSTDLIB_GLOBAL_ABORT_DEFINED
#define _RIN_CSTDLIB_GLOBAL_ABORT_DEFINED
__attribute__((noreturn)) static inline void abort(void) {
    /* Debug: output abort message with stack trace via serial */
    const char* msg = "[ABORT] libc abort() called\n";
    _RIN_STDLIB_SYSCALL2(110, (uintptr_t)msg, (uintptr_t)28u);

    /* A failed stack query or malformed frame chain only suppresses the
     * optional trace; it must never make abort itself fault. */
    uintptr_t stack_low = 0u;
    uintptr_t stack_high = 0u;
#if defined(_RIN_STDLIB_HAS_SIGNAL_API)
    rin_thread_stack_info_v1 stack_info = {0};
    intptr_t tid = _RIN_STDLIB_SYSCALL0(SYS_GETTID);
    if (tid >= 0) {
        intptr_t stack_result = _RIN_STDLIB_SYSCALL3(
            SYS_THREAD_STACK_INFO, (uintptr_t)tid,
            (uintptr_t)&stack_info, (uintptr_t)sizeof(stack_info));
        const unsigned int known_stack_flags =
            RIN_THREAD_STACK_INFO_FLAG_GROWS_DOWN |
            RIN_THREAD_STACK_INFO_FLAG_HAS_GUARD |
            RIN_THREAD_STACK_INFO_FLAG_DETACHED;
        if (stack_result == 0 && stack_info.reserved == 0u &&
            (stack_info.flags & ~known_stack_flags) == 0u &&
            (stack_info.flags & RIN_THREAD_STACK_INFO_FLAG_GROWS_DOWN) != 0u &&
            stack_info.stack_base != 0u && stack_info.stack_size != 0u &&
            stack_info.stack_base <= (unsigned long long)UINTPTR_MAX &&
            stack_info.stack_size <= (unsigned long long)UINTPTR_MAX &&
            stack_info.stack_size <=
                (unsigned long long)(UINTPTR_MAX -
                    (uintptr_t)stack_info.stack_base)) {
            stack_low = (uintptr_t)stack_info.stack_base;
            stack_high = stack_low + (uintptr_t)stack_info.stack_size;
        }
    }
#endif

    /* Print only frames wholly inside the current thread's usable stack. */
    uintptr_t bp, ret_addr;
#if defined(__x86_64__) || defined(_M_X64)
    __asm__ volatile("mov %%rbp, %0" : "=r"(bp));
    const int hex_digits = 60;  /* 16 hex digits for 64-bit */
#else
    __asm__ volatile("movl %%ebp, %0" : "=r"(bp));
    const int hex_digits = 28;  /* 8 hex digits for 32-bit */
#endif

    const char* msg2 = "[ABORT] Stack trace:\n";
    _RIN_STDLIB_SYSCALL2(110, (uintptr_t)msg2, (uintptr_t)21u);

    /* Walk up to 8 stack frames */
    const uintptr_t frame_bytes = 2u * sizeof(uintptr_t);
    for (int i = 0; i < 8 && stack_high > stack_low &&
         stack_high - stack_low >= frame_bytes && bp >= stack_low &&
         bp <= stack_high - frame_bytes &&
         (bp & (sizeof(uintptr_t) - 1u)) == 0u; i++) {
        uintptr_t next_bp;
        ret_addr = ((const uintptr_t*)bp)[1];
        next_bp = ((const uintptr_t*)bp)[0];
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

        if (next_bp == 0u || next_bp <= bp || next_bp > stack_high - frame_bytes ||
            (next_bp & (sizeof(uintptr_t) - 1u)) != 0u)
            break;
        bp = next_bp;
    }

    /* POSIX abort delivers SIGABRT even if the caller blocked or ignored it.
     * If a handler returns, restore the default action and deliver it again. */
#if defined(_RIN_STDLIB_HAS_SIGNAL_API)
    sigset_t abort_set;
    if (sigemptyset(&abort_set) == 0 && sigaddset(&abort_set, SIGABRT) == 0)
        (void)sigprocmask(SIG_UNBLOCK, &abort_set, 0);
    (void)raise(SIGABRT);

    {
        struct sigaction default_action = {0};
        default_action.sa_handler = SIG_DFL;
        (void)sigemptyset(&default_action.sa_mask);
        (void)sigaction(SIGABRT, &default_action, 0);
    }
    if (sigemptyset(&abort_set) == 0 && sigaddset(&abort_set, SIGABRT) == 0)
        (void)sigprocmask(SIG_UNBLOCK, &abort_set, 0);
    (void)raise(SIGABRT);

    _RIN_STDLIB_SYSCALL1(SYS_EXIT, (uintptr_t)(128 + SIGABRT));
#else
    /* Non-RinOS compatibility builds have no RinOS signal ABI to invoke. */
    _RIN_STDLIB_SYSCALL1(SYS_EXIT, (uintptr_t)134u);
#endif
    for(;;) {}
}
#endif /* _RIN_CSTDLIB_GLOBAL_ABORT_DEFINED */

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
extern int   rin_clearenv(void);

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
/* The process-wide environment owner is implemented once in libc so it can
 * serialize mutations and reclaim retired tables across translation units. */
char* getenv(const char* name);
char* secure_getenv(const char* name);
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);
int putenv(char* string);
int clearenv(void);

#elif defined(_RIN_STDLIB_HOSTED_SYSTEM_CRT_OWNER) && defined(__MINGW32__)
/* MinGW's CRT provides getenv and putenv, while POSIX setenv/unsetenv/
 * clearenv are absent. Keep the host CRT as the environment owner and adapt
 * only the missing operations. Windows has no set-id secure-execution mode. */
char* getenv(const char* name);
extern int __cdecl _putenv(const char* string);
extern int __cdecl _putenv_s(const char* name, const char* value);
extern char*** __cdecl __p__environ(void);

static inline char* secure_getenv(const char* name) {
    return getenv(name);
}

static inline int _rin_hosted_environment_valid_name(const char* name) {
    size_t index;
    if (!name || name[0] == '\0') return 0;
    for (index = 0u; name[index] != '\0'; ++index) {
        if (name[index] == '=') return 0;
    }
    return 1;
}

static inline int setenv(const char* name, const char* value, int overwrite) {
    int result;
    if (!_rin_hosted_environment_valid_name(name) || !value) {
        errno = EINVAL;
        return -1;
    }
    if (!overwrite && getenv(name)) return 0;
    result = _putenv_s(name, value);
    if (result != 0) {
        errno = result;
        return -1;
    }
    return 0;
}

static inline int unsetenv(const char* name) {
    int result;
    if (!_rin_hosted_environment_valid_name(name)) {
        errno = EINVAL;
        return -1;
    }
    result = _putenv_s(name, "");
    if (result != 0) {
        errno = result;
        return -1;
    }
    return 0;
}

static inline int putenv(char* string) {
    int result;
    if (!string) {
        errno = EINVAL;
        return -1;
    }
    result = _putenv(string);
    if (result != 0) {
        if (errno == 0) errno = EINVAL;
        return -1;
    }
    return 0;
}

static inline int clearenv(void) {
    char*** environment_address = __p__environ();
    size_t removed;
    if (!environment_address) {
        errno = EFAULT;
        return -1;
    }

    /* _putenv_s may replace the environment block. Re-read it after every
     * removal, and leave Windows drive-current-directory entries intact. */
    for (removed = 0u; removed < 32768u; ++removed) {
        char** environment = *environment_address;
        size_t entry_index;
        int found = 0;
        if (!environment) return 0;

        for (entry_index = 0u; entry_index < 32768u &&
                                environment[entry_index]; ++entry_index) {
            const char* entry = environment[entry_index];
            size_t name_length = 0u;
            char name[32768];
            int result;
            while (entry[name_length] != '\0' && entry[name_length] != '=') {
                ++name_length;
            }
            /* Entries beginning with '=' encode per-drive working directories. */
            if (entry[name_length] != '=' || name_length == 0u) continue;
            if (name_length >= sizeof(name)) {
                errno = E2BIG;
                return -1;
            }
            for (size_t index = 0u; index < name_length; ++index) {
                name[index] = entry[index];
            }
            name[name_length] = '\0';
            result = _putenv_s(name, "");
            if (result != 0) {
                errno = result;
                return -1;
            }
            found = 1;
            break;
        }
        if (!found) return 0;
    }
    errno = E2BIG;
    return -1;
}

#elif defined(_RIN_STDLIB_HOSTED_SYSTEM_CRT_OWNER)
/* Hosted POSIX C programs use the native CRT's environment implementation.
 * In particular, secure_getenv must retain the host's secure-execution check. */
char* getenv(const char* name);
char* secure_getenv(const char* name);
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);
int putenv(char* string);
int clearenv(void);

#else
/* ホスト環境用スタブ */
static inline char* getenv(const char* name) {
    (void)name;
    return NULL;
}

static inline int setenv(const char* name, const char* value, int overwrite) {
    (void)name; (void)value; (void)overwrite;
    errno = ENOSYS;
    return -1;
}

static inline int unsetenv(const char* name) {
    (void)name;
    errno = ENOSYS;
    return -1;
}

static inline int putenv(char* string) {
    (void)string;
    errno = ENOSYS;
    return -1;
}
#endif /* RIN_FREESTANDING */
#endif /* !_MSVCRT_COMPAT */

#if !defined(_MSVCRT_COMPAT) && !defined(_RIN_STDLIB_HOSTED_CXX_OWNER) && \
    !(defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)) && \
    !defined(_RIN_STDLIB_HOSTED_SYSTEM_CRT_OWNER)
static inline char* secure_getenv(const char* name) {
    return getenv(name);
}

static inline int clearenv(void) {
#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
    return rin_clearenv();
#else
    if (environ) environ[0] = NULL;
    return 0;
#endif
}
#endif /* !_MSVCRT_COMPAT */

/* ═══════════════════════════════════════════════════════════════
 * 数値変換
 * ═══════════════════════════════════════════════════════════════*/

#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
static inline const char* rin_integer_skip_locale_space(
    const char* input, locale_t locale) {
    const char* cursor = input;
    if (!cursor) return cursor;
    while (*cursor != '\0' &&
           rin_locale_ctype_test(locale, (unsigned char)*cursor,
                                 RIN_LOCALE_CTYPE_SPACE))
        ++cursor;
    return cursor;
}

static inline rin_integer_parse_result rin_integer_parse_locale(
    const char* input, int base, unsigned long long positive_limit,
    unsigned long long negative_limit, locale_t locale) {
    return rin_integer_parse(
        rin_integer_skip_locale_space(input, locale), base, positive_limit,
        negative_limit);
}

static inline rin_integer_parse_result rin_integer_parse_current_locale(
    const char* input, int base, unsigned long long positive_limit,
    unsigned long long negative_limit) {
    return rin_integer_parse_locale(input, base, positive_limit,
                                    negative_limit, uselocale(NULL));
}
#else
static inline rin_integer_parse_result rin_integer_parse_current_locale(
    const char* input, int base, unsigned long long positive_limit,
    unsigned long long negative_limit) {
    return rin_integer_parse(input, base, positive_limit, negative_limit);
}
#endif

static inline long rin_integer_result_long(
    rin_integer_parse_result parsed, const char* original, char** endptr) {
    const unsigned long long positive_limit =
        (unsigned long long)__LONG_MAX__;
    const unsigned long long negative_limit = positive_limit + 1ULL;
    if (endptr)
        *endptr = (char*)(__UINTPTR_TYPE__)(parsed.any ? parsed.end : original);
    if (parsed.invalid_input || parsed.invalid_base) {
        errno = EINVAL;
        return 0;
    }
    if (parsed.overflow) {
        errno = ERANGE;
        return parsed.negative ? (-__LONG_MAX__ - 1L) : __LONG_MAX__;
    }
    if (!parsed.any) return 0;
    if (!parsed.negative) return (long)parsed.magnitude;
    if (parsed.magnitude == negative_limit) return -__LONG_MAX__ - 1L;
    return -(long)parsed.magnitude;
}

static inline unsigned long rin_integer_result_ulong(
    rin_integer_parse_result parsed, const char* original, char** endptr) {
    const unsigned long maximum =
        (unsigned long)__LONG_MAX__ * 2UL + 1UL;
    if (endptr)
        *endptr = (char*)(__UINTPTR_TYPE__)(parsed.any ? parsed.end : original);
    if (parsed.invalid_input || parsed.invalid_base) {
        errno = EINVAL;
        return 0;
    }
    if (parsed.overflow) {
        errno = ERANGE;
        return maximum;
    }
    if (!parsed.any) return 0;
    return parsed.negative
        ? 0UL - (unsigned long)parsed.magnitude
        : (unsigned long)parsed.magnitude;
}

static inline long long rin_integer_result_long_long(
    rin_integer_parse_result parsed, const char* original, char** endptr) {
    const unsigned long long positive_limit =
        (unsigned long long)__LONG_LONG_MAX__;
    const unsigned long long negative_limit = positive_limit + 1ULL;
    if (endptr)
        *endptr = (char*)(__UINTPTR_TYPE__)(parsed.any ? parsed.end : original);
    if (parsed.invalid_input || parsed.invalid_base) {
        errno = EINVAL;
        return 0;
    }
    if (parsed.overflow) {
        errno = ERANGE;
        return parsed.negative
            ? (-__LONG_LONG_MAX__ - 1LL) : __LONG_LONG_MAX__;
    }
    if (!parsed.any) return 0;
    if (!parsed.negative) return (long long)parsed.magnitude;
    if (parsed.magnitude == negative_limit)
        return -__LONG_LONG_MAX__ - 1LL;
    return -(long long)parsed.magnitude;
}

static inline unsigned long long rin_integer_result_ulong_long(
    rin_integer_parse_result parsed, const char* original, char** endptr) {
    const unsigned long long maximum =
        (unsigned long long)__LONG_LONG_MAX__ * 2ULL + 1ULL;
    if (endptr)
        *endptr = (char*)(__UINTPTR_TYPE__)(parsed.any ? parsed.end : original);
    if (parsed.invalid_input || parsed.invalid_base) {
        errno = EINVAL;
        return 0;
    }
    if (parsed.overflow) {
        errno = ERANGE;
        return maximum;
    }
    if (!parsed.any) return 0;
    return parsed.negative ? 0ULL - parsed.magnitude : parsed.magnitude;
}

#ifndef _STDLIB_SKIP_INLINE_DEFS

#ifndef _ATOF_DEFINED
#define _ATOF_DEFINED
static inline double atof(const char* s) {
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
    rin_float_parse_result parsed = rin_float_parse_binary64_radix(
        s, rin_locale_current_decimal_point());
#else
    rin_float_parse_result parsed = rin_float_parse_binary64(s);
#endif
    if (parsed.invalid_input) errno = EINVAL;
    else if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
    return rin_float_parse_binary64_value(parsed.bits);
}
#endif /* _ATOF_DEFINED */

static inline long strtol(const char* s, char** endptr, int base) {
    const unsigned long long positive_limit =
        (unsigned long long)__LONG_MAX__;
    const unsigned long long negative_limit = positive_limit + 1ULL;
    rin_integer_parse_result parsed = rin_integer_parse_current_locale(
        s, base, positive_limit, negative_limit);
    return rin_integer_result_long(parsed, s, endptr);
}

static inline unsigned long strtoul(const char* s, char** endptr, int base) {
    const unsigned long maximum =
        (unsigned long)__LONG_MAX__ * 2UL + 1UL;
    rin_integer_parse_result parsed = rin_integer_parse_current_locale(
        s, base, (unsigned long long)maximum,
        (unsigned long long)maximum);
    return rin_integer_result_ulong(parsed, s, endptr);
}

static inline long long strtoll(const char* s, char** endptr, int base) {
    const unsigned long long positive_limit =
        (unsigned long long)__LONG_LONG_MAX__;
    const unsigned long long negative_limit = positive_limit + 1ULL;
    rin_integer_parse_result parsed = rin_integer_parse_current_locale(
        s, base, positive_limit, negative_limit);
    return rin_integer_result_long_long(parsed, s, endptr);
}

static inline unsigned long long strtoull(const char* s, char** endptr, int base) {
    const unsigned long long maximum =
        (unsigned long long)__LONG_LONG_MAX__ * 2ULL + 1ULL;
    rin_integer_parse_result parsed = rin_integer_parse_current_locale(
        s, base, maximum, maximum);
    return rin_integer_result_ulong_long(parsed, s, endptr);
}

#ifndef atoi
static inline int atoi(const char* s) {
    const unsigned long long positive_limit =
        (unsigned long long)__INT_MAX__;
    const unsigned long long negative_limit = positive_limit + 1ULL;
    rin_integer_parse_result parsed = rin_integer_parse_current_locale(
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
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
    rin_float_parse_result parsed = rin_float_parse_binary64_radix(
        s, rin_locale_current_decimal_point());
#else
    rin_float_parse_result parsed = rin_float_parse_binary64(s);
#endif
    if (endptr) *endptr = (char*)(__UINTPTR_TYPE__)parsed.end;
    if (parsed.invalid_input) errno = EINVAL;
    else if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
    return rin_float_parse_binary64_value(parsed.bits);
}

static inline float strtof(const char* s, char** endptr) {
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
    rin_float_parse_result parsed = rin_float_parse_binary32_radix(
        s, rin_locale_current_decimal_point());
#else
    rin_float_parse_result parsed = rin_float_parse_binary32(s);
#endif
    if (endptr) *endptr = (char*)(__UINTPTR_TYPE__)parsed.end;
    if (parsed.invalid_input) errno = EINVAL;
    else if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
    return rin_float_parse_binary32_value(parsed.bits);
}

#if !defined(__RINOS__) || defined(RINOS_ABI_HAS_LONG_DOUBLE)
static inline long double strtold(const char* s, char** endptr) {
#if defined(RIN_FP_HAS_BINARY80)
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
    rin_long_double_parse_result parsed = rin_float_parse_binary80_radix(
        s, rin_locale_current_decimal_point());
#else
    rin_long_double_parse_result parsed = rin_float_parse_binary80(s);
#endif
    if (endptr) *endptr = (char*)(__UINTPTR_TYPE__)parsed.end;
    if (parsed.invalid_input) errno = EINVAL;
    else if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
    return parsed.value;
#else
    return (long double)strtod(s, endptr);
#endif
}
#endif

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

#if !defined(__RINOS__) || defined(RINOS_ABI_HAS_LONG_DOUBLE)
static inline long double strtold(const char* s, char** endptr) {
    return std::strtold(s, endptr);
}
#endif
#endif /* C++ <cstdlib> freestanding bridge */
#endif /* !_STDLIB_SKIP_INLINE_DEFS - 数値変換 */

/* Locale-explicit integer conversions use the selected LC_CTYPE for their
 * leading whitespace. The integer digit and base grammar is shared. */
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
static inline long strtol_l(const char* s, char** endptr, int base,
                            locale_t locale) {
    const unsigned long long positive_limit =
        (unsigned long long)__LONG_MAX__;
    const unsigned long long negative_limit = positive_limit + 1ULL;
    rin_integer_parse_result parsed = rin_integer_parse_locale(
        s, base, positive_limit, negative_limit, locale);
    return rin_integer_result_long(parsed, s, endptr);
}

static inline unsigned long strtoul_l(const char* s, char** endptr, int base,
                                      locale_t locale) {
    const unsigned long maximum =
        (unsigned long)__LONG_MAX__ * 2UL + 1UL;
    rin_integer_parse_result parsed = rin_integer_parse_locale(
        s, base, (unsigned long long)maximum,
        (unsigned long long)maximum, locale);
    return rin_integer_result_ulong(parsed, s, endptr);
}

static inline long long strtoll_l(const char* s, char** endptr, int base,
                                  locale_t locale) {
    const unsigned long long positive_limit =
        (unsigned long long)__LONG_LONG_MAX__;
    const unsigned long long negative_limit = positive_limit + 1ULL;
    rin_integer_parse_result parsed = rin_integer_parse_locale(
        s, base, positive_limit, negative_limit, locale);
    return rin_integer_result_long_long(parsed, s, endptr);
}

static inline unsigned long long strtoull_l(const char* s, char** endptr,
                                            int base, locale_t locale) {
    const unsigned long long maximum =
        (unsigned long long)__LONG_LONG_MAX__ * 2ULL + 1ULL;
    rin_integer_parse_result parsed = rin_integer_parse_locale(
        s, base, maximum, maximum, locale);
    return rin_integer_result_ulong_long(parsed, s, endptr);
}

static inline float strtof_l(const char* s, char** endptr, locale_t locale) {
    rin_float_parse_result parsed = rin_float_parse_binary32_radix(
        s, rin_locale_decimal_point_l(locale));
    if (endptr) *endptr = (char*)(__UINTPTR_TYPE__)parsed.end;
    if (parsed.invalid_input) errno = EINVAL;
    else if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
    return rin_float_parse_binary32_value(parsed.bits);
}

static inline double strtod_l(const char* s, char** endptr, locale_t locale) {
    rin_float_parse_result parsed = rin_float_parse_binary64_radix(
        s, rin_locale_decimal_point_l(locale));
    if (endptr) *endptr = (char*)(__UINTPTR_TYPE__)parsed.end;
    if (parsed.invalid_input) errno = EINVAL;
    else if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
    return rin_float_parse_binary64_value(parsed.bits);
}

#if !defined(__RINOS__) || defined(RINOS_ABI_HAS_LONG_DOUBLE)
static inline long double strtold_l(const char* s, char** endptr,
                                    locale_t locale) {
#if defined(RIN_FP_HAS_BINARY80)
    rin_long_double_parse_result parsed = rin_float_parse_binary80_radix(
        s, rin_locale_decimal_point_l(locale));
    if (endptr) *endptr = (char*)(__UINTPTR_TYPE__)parsed.end;
    if (parsed.invalid_input) errno = EINVAL;
    else if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
    return parsed.value;
#else
    rin_float_parse_result parsed = rin_float_parse_binary64_radix(
        s, rin_locale_decimal_point_l(locale));
    if (endptr) *endptr = (char*)(__UINTPTR_TYPE__)parsed.end;
    if (parsed.invalid_input) errno = EINVAL;
    else if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
    return (long double)rin_float_parse_binary64_value(parsed.bits);
#endif
}
#endif
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
    size_t storage_alignment;
    size_t max_value = (size_t)-1;
    size_t meta_bytes = rin_aligned_allocation_meta_size();
    void* raw;

    if (!_rin_is_power_of_two_size(alignment)) return NULL;
    if (require_size_multiple && ((size & (alignment - 1)) != 0)) return NULL;

    /* Keep the metadata naturally aligned even when aligned_alloc() is asked
     * for a valid fundamental alignment smaller than a pointer. Returning a
     * pointer aligned more strongly than requested is permitted. */
    storage_alignment = alignment < sizeof(void*) ? sizeof(void*) : alignment;
    if (size > (max_value - storage_alignment)) return NULL;
    total = size + storage_alignment;
    if (total > (max_value - meta_bytes)) return NULL;
    total += meta_bytes;

    raw = malloc(total);
    if (!raw) return NULL;

    addr = (size_t)raw + meta_bytes;
    if (addr > (max_value - (storage_alignment - 1))) {
        free(raw);
        return NULL;
    }
    aligned_addr = (addr + storage_alignment - 1) & ~(storage_alignment - 1);
    rin_store_aligned_allocation_metadata_ex((void*)aligned_addr, raw,
                                              storage_alignment, size);
    return (void*)aligned_addr;
}

/* Keep inline fallbacks for standalone freestanding consumers.  The userspace
 * runtime and ABI consumers use these public symbols. */
#if (defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)) || \
    defined(RIN_STDLIB_ALIGNED_ALLOC_RUNTIME_IMPLEMENTATION)
void* aligned_alloc(size_t alignment, size_t size);
void* memalign(size_t alignment, size_t size);
int posix_memalign(void** memptr, size_t alignment, size_t size);
#else
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
    int saved_errno;
    if (!memptr) return EINVAL;
    if (alignment < sizeof(void*) || !_rin_is_power_of_two_size(alignment))
        return EINVAL;
    saved_errno = errno;
    ptr = _rin_aligned_alloc_internal(alignment, size, 0);
    errno = saved_errno;
    if (!ptr) return ENOMEM;
    *memptr = ptr;
    return 0;
}
#endif

/* POSIX string duplication.  A standalone freestanding stdlib.h keeps the
 * inline fallback; userspace and mixed string.h/stdlib.h includes share the
 * external declaration from the string API/runtime owner. */
#if (defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)) || \
    defined(RIN_STRING_DUP_EXTERNAL_DECLARED) || \
    defined(RIN_STRING_RUNTIME_IMPLEMENTATION)
char* strdup(const char* string);
char* strndup(const char* string, size_t size);
#define RIN_STRING_DUP_EXTERNAL_DECLARED 1
#else
static inline size_t _strdup_strlen(const char* string) {
    size_t length = 0u;
    while (string[length] != '\0') ++length;
    return length;
}

#ifndef strdup
static inline char* strdup(const char* string) {
    size_t length;
    char* copy;
    if (!string) return NULL;
    length = _strdup_strlen(string);
    if (length == SIZE_MAX) {
        errno = ENOMEM;
        return NULL;
    }
    copy = (char*)malloc(length + 1u);
    if (!copy) {
        errno = ENOMEM;
        return NULL;
    }
    for (size_t index = 0u; index <= length; ++index)
        copy[index] = string[index];
    return copy;
}
#endif

static inline char* strndup(const char* string, size_t size) {
    size_t length = 0u;
    char* copy;
    if (!string) return NULL;
    while (length < size && string[length] != '\0') ++length;
    if (length == SIZE_MAX) {
        errno = ENOMEM;
        return NULL;
    }
    copy = (char*)malloc(length + 1u);
    if (!copy) {
        errno = ENOMEM;
        return NULL;
    }
    for (size_t index = 0u; index < length; ++index)
        copy[index] = string[index];
    copy[length] = '\0';
    return copy;
}
#define RIN_STRING_DUP_INLINE_AVAILABLE 1
#endif

/* ═══════════════════════════════════════════════════════════════
 * 算術関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int abs(int n) {
    /* C leaves abs(INT_MIN) undefined because its positive magnitude is not
     * representable. RinOS defines a deterministic extension: preserve the
     * minimum value instead of overflowing or converting out of range. */
    if (n == INT_MIN) return INT_MIN;
    return n < 0 ? -n : n;
}

static inline long labs(long n) {
    if (n == LONG_MIN) return LONG_MIN;
    return n < 0 ? -n : n;
}

static inline long long llabs(long long n) {
    if (n == LLONG_MIN) return LLONG_MIN;
    return n < 0 ? -n : n;
}

typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;
typedef struct { long long quot; long long rem; } lldiv_t;

static inline div_t div(int numer, int denom) {
    /* Division whose mathematical quotient is INT_MAX+1 is undefined in C.
     * RinOS keeps the result deterministic without evaluating either
     * overflowing operator. */
    if (numer == INT_MIN && denom == -1) {
        div_t minimum = { INT_MIN, 0 };
        return minimum;
    }
    div_t r = { numer / denom, numer % denom };
    return r;
}

static inline ldiv_t ldiv(long numer, long denom) {
    if (numer == LONG_MIN && denom == -1L) {
        ldiv_t minimum = { LONG_MIN, 0 };
        return minimum;
    }
    ldiv_t r = { numer / denom, numer % denom };
    return r;
}

static inline lldiv_t lldiv(long long numer, long long denom) {
    if (numer == LLONG_MIN && denom == -1LL) {
        lldiv_t minimum = { LLONG_MIN, 0 };
        return minimum;
    }
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
int rand_r(unsigned int* seed);
long random(void);
void srandom(unsigned int seed);
#endif

/* ═══════════════════════════════════════════════════════════════
 * ソート・検索
 * ═══════════════════════════════════════════════════════════════*/

typedef int (*_qsort_cmp_t)(const void*, const void*);

/* GNU's context-taking comparator ABI is available only when explicitly
 * requested; other systems assign qsort_r a different signature. */
#if (defined(_GNU_SOURCE) || defined(__USE_GNU) || \
     defined(_RIN_USE_GNU_QSORT_R)) && \
    !defined(_RIN_STDLIB_HOSTED_CXX_OWNER)
#ifdef __cplusplus
extern "C" {
#endif
void qsort_r(void* base, size_t nmemb, size_t size,
             int (*compar)(const void*, const void*, void*), void* context);
#ifdef __cplusplus
}
#endif
#endif

static inline void _rin_qsort_swap_bytes(unsigned char* left,
                                         unsigned char* right,
                                         size_t size) {
    for (size_t index = 0u; index < size; ++index) {
        unsigned char temporary = left[index];
        left[index] = right[index];
        right[index] = temporary;
    }
}

static inline void _rin_qsort_sift_down(unsigned char* bytes, size_t root,
                                        size_t end, size_t size,
                                        _qsort_cmp_t compar) {
    if (end == 0u) return;
    while (root <= (end - 1u) / 2u) {
        size_t child = root * 2u + 1u;
        size_t selected = root;
        if (compar(bytes + selected * size, bytes + child * size) < 0)
            selected = child;
        if (child < end &&
            compar(bytes + selected * size, bytes + (child + 1u) * size) < 0)
            selected = child + 1u;
        if (selected == root) return;
        _rin_qsort_swap_bytes(bytes + root * size,
                              bytes + selected * size, size);
        root = selected;
    }
}

static inline void qsort(void* base, size_t nmemb, size_t size, _qsort_cmp_t compar) {
    unsigned char* arr = (unsigned char*)base;
    if (!arr || !compar || size == 0u || nmemb < 2u ||
        nmemb > (size_t)-1 / size)
        return;

    size_t start = (nmemb - 2u) / 2u + 1u;
    while (start != 0u) {
        --start;
        _rin_qsort_sift_down(arr, start, nmemb - 1u, size, compar);
    }

    size_t end = nmemb - 1u;
    while (end != 0u) {
        _rin_qsort_swap_bytes(arr, arr + end * size, size);
        --end;
        _rin_qsort_sift_down(arr, 0u, end, size, compar);
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

/* Parse the next POSIX suboption in place.  Keeping the parser inline lets
 * freestanding consumers use the API without pulling in another runtime
 * object; stdlib_runtime.c also exports the same implementation as a symbol. */
static inline int _rin_stdlib_getsubopt_parse(char** optionp,
                                               char* const* keylistp,
                                               char** valuep) {
    char* token;
    char* separator;
    char* value;
    size_t token_length;
    int index;

    if (optionp == NULL || *optionp == NULL || keylistp == NULL ||
        valuep == NULL)
        return -1;

    token = *optionp;
    separator = token;
    while (*separator != '\0' && *separator != ',') ++separator;
    if (*separator == ',') {
        *separator = '\0';
        *optionp = separator + 1;
    } else {
        *optionp = separator;
    }

    value = NULL;
    for (char* cursor = token; *cursor != '\0'; ++cursor) {
        if (*cursor == '=') {
            *cursor = '\0';
            value = cursor + 1;
            break;
        }
    }
    token_length = 0u;
    while (token[token_length] != '\0') ++token_length;

    for (index = 0; keylistp[index] != NULL; ++index) {
        size_t key_length = 0u;
        while (keylistp[index][key_length] != '\0') ++key_length;
        if (key_length != token_length) continue;

        size_t offset = 0u;
        while (offset < token_length &&
               token[offset] == keylistp[index][offset])
            ++offset;
        if (offset == token_length) {
            *valuep = value;
            return index;
        }
    }

    /* POSIX leaves valuep unspecified for an unknown token.  Expose the
     * parsed value when present as a useful, compatible extension. */
    *valuep = value;
    return -1;
}

#if defined(RIN_STDLIB_NO_INLINE_GETSUBOPT)
int getsubopt(char** optionp, char* const* keylistp, char** valuep);
#else
static inline int getsubopt(char** optionp, char* const* keylistp,
                            char** valuep) {
    return _rin_stdlib_getsubopt_parse(optionp, keylistp, valuep);
}
#endif

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
            if (received == -(intptr_t)EINTR) continue;
            for (size_t index = 0; index < sizeof(random_bytes); ++index)
                random_bytes[index] = 0;
            errno = (int)-received;
            return -1;
        }
        if (received <= 0 ||
            (uintptr_t)received > (uintptr_t)sizeof(random_bytes)) {
            for (size_t index = 0; index < sizeof(random_bytes); ++index)
                random_bytes[index] = 0;
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

/* Create a unique temporary file, passing descriptor flags to the same
 * open operation that creates it so CLOEXEC cannot race with exec. */
static inline int _rin_mkostemps_internal(char* tmpl, int suffixlen,
                                         int flags) {
    if (!tmpl) {
        errno = EINVAL;
        return -1;
    }

    if (suffixlen < 0 ||
        (flags & ~(O_APPEND | O_CLOEXEC | O_SYNC)) != 0) {
        errno = EINVAL;
        return -1;
    }

    /* Find the XXXXXX suffix */
    size_t len = 0;
    char* p = tmpl;
    while (*p) { len++; p++; }

    if ((size_t)suffixlen > len || len - (size_t)suffixlen < 6) {
        errno = EINVAL;
        return -1;
    }

    /* Check for XXXXXX immediately before the caller-specified suffix. */
    char* suffix = tmpl + len - (size_t)suffixlen - 6;
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
            (uintptr_t)(O_RDWR | O_CREAT | O_EXCL | flags),
            (uintptr_t)0600u);
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

/* mkstemp - create a unique temporary file */
static inline int mkstemp(char* tmpl) {
    return _rin_mkostemps_internal(tmpl, 0, 0);
}

/* mkstemps - mkstemp with suffix length */
static inline int mkstemps(char* tmpl, int suffixlen) {
    return _rin_mkostemps_internal(tmpl, suffixlen, 0);
}

/* GNU-compatible variants atomically apply O_APPEND/O_CLOEXEC on creation. */
static inline int mkostemp(char* tmpl, int flags) {
    return _rin_mkostemps_internal(tmpl, 0, flags);
}

static inline int mkostemps(char* tmpl, int suffixlen, int flags) {
    return _rin_mkostemps_internal(tmpl, suffixlen, flags);
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

#if !defined(_RIN_STDLIB_HOSTED_C_OWNER) && \
    !defined(_RIN_STDLIB_HOSTED_CXX_OWNER)
#ifndef MIDL_PASS
#if !defined(__cplusplus) && !defined(RIN_FREESTANDING) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__
/* Hosted C consumers use the platform CRT for process creation and command
 * execution.  Keep the RinOS system() owner exclusively in freestanding
 * targets, where the shell lifecycle is backed by RinOS syscalls. */
int system(const char* command);
#elif defined(RINSDK_SPLIT_BUILD) || defined(RIN_KERNEL)
/* SDK libraries and the kernel are linked independently from libc's process
 * owner. */
int system(const char* command);
#else
static inline int system(const char* command) {
    return _rin_system_owner(command);
}
#endif
#else
int system(const char* command);
#endif /* !MIDL_PASS */
#endif

#ifndef MIDL_PASS
#ifdef __cplusplus
extern "C" {
#endif
int getloadavg(double loadavg[], int nelem);
#ifdef __cplusplus
}
#endif
#endif

/* ═══════════════════════════════════════════════════════════════
 * マルチバイト/ワイド文字変換 (簡易実装)
 * ═══════════════════════════════════════════════════════════════*/

#ifndef MB_CUR_MAX
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
#define MB_CUR_MAX rin_locale_current_mb_cur_max()
#else
#define MB_CUR_MAX 4
#endif
#endif

static inline int mblen(const char* s, size_t n) {
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
    return rin_locale_current_mblen(s, n);
#else
    rin_unicode_mbstate_t st;
    size_t rc;
    st.state = 0;
    st.codepoint = 0;
    if (!s) return 0;
    rc = rin_unicode_mbrlen(s, n, &st);
    if (rc == (size_t)-1 || rc == (size_t)-2) return -1;
    return (int)rc;
#endif
}

/* wchar.hとの重複を避けるためのガード */
#ifndef _STDLIB_WCHAR_FUNCS_DEFINED
#define _STDLIB_WCHAR_FUNCS_DEFINED

#ifndef _MSVCRT_COMPAT
static inline int mbtowc(wchar_t* pwc, const char* s, size_t n) {
#if defined(RIN_USERSPACE)
    return rin_locale_current_mbtowc32((uint32_t*)pwc, s, n);
#else
    return rin_unicode_mbtowc32((uint32_t*)pwc, s, n);
#endif
}

static inline int wctomb(char* s, wchar_t wc) {
#if defined(RIN_USERSPACE)
    return rin_locale_current_wctomb32(s, (uint32_t)wc);
#else
    if (!s) return 0;
    return rin_unicode_wctomb32(s, (uint32_t)wc);
#endif
}

static inline size_t mbstowcs(wchar_t* dest, const char* src, size_t n) {
#if defined(RIN_USERSPACE)
    return rin_locale_current_mbstowcs32((uint32_t*)dest, src, n);
#else
    return rin_unicode_mbstowcs32((uint32_t*)dest, src, n);
#endif
}

static inline size_t wcstombs(char* dest, const wchar_t* src, size_t n) {
#if defined(RIN_USERSPACE)
    return rin_locale_current_wcstombs32(dest, (const uint32_t*)src, n);
#else
    return rin_unicode_wcstombs32(dest, (const uint32_t*)src, n);
#endif
}
#endif /* !_MSVCRT_COMPAT */

#endif /* _STDLIB_WCHAR_FUNCS_DEFINED */

#endif /* !_RIN_STDLIB_HOSTED_CXX_OWNER */

#undef _RIN_STDLIB_HOSTED_C_OWNER
#undef _RIN_STDLIB_HOSTED_CXX_OWNER
#undef _RIN_STDLIB_HOSTED_SYSTEM_CRT_OWNER

#ifdef __cplusplus
}
#endif
#endif /* !MIDL_PASS */

#endif /* _STDLIB_H */
