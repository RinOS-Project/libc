/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - stdio.h
 * 標準入出力
 */

#ifndef _RIN_STDIO_IMPL_H
#define _RIN_STDIO_IMPL_H

#if !defined(RIN_STDIO_OWNER_BUILD) && !defined(RIN_STDIO_LOCAL_FALLBACK)
#error "stdio_impl.h may only be included by the stdio runtime owner or compatibility fallback"
#endif

#include "stdio.h"
#include "stdio_internal.h"

#include "stddef.h"
#include "stdint.h"
#include "stdarg.h"
#include "stdio_width_specifier.h"
#include "sys/types.h"
#include "errno.h"
#include "limits.h"
#include "sys/syscall.h"
#include "fcntl_flags.h"
#include "stdlib.h"
#if defined(RIN_USERSPACE)
#include "rin_atomic_wait.h"
#endif

#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif

#define _RIN_STDIO_MAX_SYSCALL_TRANSFER_BYTES 16777216u

#ifndef _RIN_STDIO_SYSCALL1
#define _RIN_STDIO_SYSCALL1(number, arg1) \
    _syscall1((uintptr_t)(number), (uintptr_t)(arg1))
#endif
#ifndef _RIN_STDIO_SYSCALL2
#define _RIN_STDIO_SYSCALL2(number, arg1, arg2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(arg1), (uintptr_t)(arg2))
#endif
#ifndef _RIN_STDIO_SYSCALL3
#define _RIN_STDIO_SYSCALL3(number, arg1, arg2, arg3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(arg1), \
              (uintptr_t)(arg2), (uintptr_t)(arg3))
#endif
#ifndef _RIN_STDIO_SYSCALL5
#define _RIN_STDIO_SYSCALL5(number, arg1, arg2, arg3, arg4, arg5) \
    _syscall5((uintptr_t)(number), (uintptr_t)(arg1), \
              (uintptr_t)(arg2), (uintptr_t)(arg3), \
              (uintptr_t)(arg4), (uintptr_t)(arg5))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * 定数
 * ═══════════════════════════════════════════════════════════════*/

#define EOF (-1)
#define BUFSIZ 512
/* Bound the allocation and arithmetic surface of setvbuf().  The legacy
 * name is retained because internal consumers use it as the maximum valid
 * byte position, rather than as an encoding mask. */
#define _RIN_STDIO_BUFFER_POSITION_MASK ((size_t)-1 / 2u)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* バッファリングモード */
#define _IOFBF 0  /* Full buffering */
#define _IOLBF 1  /* Line buffering */
#define _IONBF 2  /* No buffering */

/* ファイル名/パス最大長 */
#define FILENAME_MAX 256
#define FOPEN_MAX    STREAM_MAX
#define TMP_MAX      238328
#define L_tmpnam     20

/* ═══════════════════════════════════════════════════════════════
 * FILE構造体
 * ═══════════════════════════════════════════════════════════════*/

/* Forward declaration for other headers */
#ifndef _FILE_DECLARED
#define _FILE_DECLARED
struct _FILE;
typedef struct _FILE FILE;
#endif

/* Full definition - only in stdio.h */
#ifndef _FILE_DEFINED
#define _FILE_DEFINED
struct _FILE {
    int fd;
    int eof;
    int error;
    int mode;
    unsigned char* buf;
    size_t buf_size;
    size_t buf_pos;
    size_t buf_end;
    int buf_mode;
    int orientation;
    uint32_t wide_pushback;
    int wide_pushback_valid;
    uint32_t wide_pushback_second;
    int wide_pushback_second_valid;
    /* Nonzero only when setvbuf() allocated buf and must release it. */
    int buf_owned;
};
#endif

#ifndef MIDL_PASS
/* 静的FILEプール */
#define _STDIO_MAX_FILES 16
static FILE _stdio_files[_STDIO_MAX_FILES];
static int _stdio_files_used[_STDIO_MAX_FILES] = {1, 1, 1, 0};

#if defined(RIN_STDIO_OWNER_BUILD)
#define _RIN_STDIO_API
#define _RIN_STDIO_INTERNAL
#else
#define _RIN_STDIO_API static inline
#define _RIN_STDIO_INTERNAL static inline
#endif

/* FILE cannot grow without breaking the freestanding ABI, so explicit stream
 * locking lives in a bounded side table. The owner token is thread-local and
 * recursion is counted. User-space waiters park on a per-entry generation;
 * fclose keeps a closed entry until the FILE slot is opened again so queued
 * callers cannot mistake a reused FILE address for the old stream. */
#define _RIN_STDIO_LOCK_CAPACITY 32u
struct _RinStdioLockOwner {
    FILE* stream;
    uintptr_t owner;
    uint32_t depth;
    volatile uint32_t wait_seq;
    uint32_t waiters;
    uint32_t closing;
};
static struct _RinStdioLockOwner
    _rin_stdio_lock_owners[_RIN_STDIO_LOCK_CAPACITY];
static volatile uint32_t _rin_stdio_lock_table_guard;
static volatile uint32_t _rin_stdio_lock_table_seq;

#if defined(RIN_USERSPACE)
#ifndef _RIN_STDIO_FUTEX_WAIT
#define _RIN_STDIO_FUTEX_WAIT(address, expected) \
    rin_atomic_wait_u32((address), (expected), NULL)
#endif
#ifndef _RIN_STDIO_FUTEX_WAKE
#define _RIN_STDIO_FUTEX_WAKE(address, count) \
    rin_atomic_notify_all_u32((address))
#endif
#endif

static inline void _rin_stdio_lock_table_enter(void) {
    while (__atomic_exchange_n(&_rin_stdio_lock_table_guard, 1u,
                               __ATOMIC_ACQUIRE) != 0u) {
        while (__atomic_load_n(&_rin_stdio_lock_table_guard,
                               __ATOMIC_RELAXED) != 0u) {
        }
    }
}

static inline void _rin_stdio_lock_table_leave(void) {
    __atomic_store_n(&_rin_stdio_lock_table_guard, 0u, __ATOMIC_RELEASE);
}

static inline int _rin_stdio_lock_table_try_enter(void) {
    return __atomic_exchange_n(&_rin_stdio_lock_table_guard, 1u,
                               __ATOMIC_ACQUIRE) == 0u;
}

static inline void _rin_stdio_wait_word(volatile uint32_t* address,
                                        uint32_t expected) {
#if defined(RIN_USERSPACE)
    (void)_RIN_STDIO_FUTEX_WAIT(address, expected);
#else
    while (__atomic_load_n(address, __ATOMIC_ACQUIRE) == expected) {
    }
#endif
}

static inline void _rin_stdio_wake_word(volatile uint32_t* address,
                                        int count) {
#if defined(RIN_USERSPACE)
    (void)count;
    (void)_RIN_STDIO_FUTEX_WAKE(address, count);
#else
    (void)address;
    (void)count;
#endif
}

static inline uintptr_t _rin_stdio_lock_owner_token(void) {
#if defined(__cplusplus)
    static thread_local unsigned char token;
#else
    static _Thread_local unsigned char token;
#endif
    return (uintptr_t)&token;
}

static inline int _rin_stdio_stream_lock_try(FILE* stream) {
    uintptr_t owner;
    size_t index;
    size_t free_index = _RIN_STDIO_LOCK_CAPACITY;
    if (!stream) return 1;
    owner = _rin_stdio_lock_owner_token();
    if (!_rin_stdio_lock_table_try_enter()) return 1;
    for (index = 0u; index < _RIN_STDIO_LOCK_CAPACITY; ++index) {
        struct _RinStdioLockOwner* entry = &_rin_stdio_lock_owners[index];
        if (entry->stream == stream) {
            int success = 0;
            if (entry->closing == 0u && entry->owner == owner &&
                entry->depth != UINT32_MAX) {
                ++entry->depth;
                success = 1;
            } else if (entry->closing == 0u && entry->owner == 0u &&
                       entry->waiters == 0u) {
                entry->owner = owner;
                entry->depth = 1u;
                success = 1;
            }
            _rin_stdio_lock_table_leave();
            return success ? 0 : 1;
        }
        if (!entry->stream && entry->waiters == 0u &&
            free_index == _RIN_STDIO_LOCK_CAPACITY)
            free_index = index;
    }
    if (free_index != _RIN_STDIO_LOCK_CAPACITY) {
        struct _RinStdioLockOwner* entry =
            &_rin_stdio_lock_owners[free_index];
        entry->stream = stream;
        entry->owner = owner;
        entry->depth = 1u;
        entry->wait_seq = 0u;
        entry->waiters = 0u;
        entry->closing = 0u;
        _rin_stdio_lock_table_leave();
        return 0;
    }
    _rin_stdio_lock_table_leave();
    return 1;
}

/* Return 0 to retry, 1 when the stream is closing, or 2 if this thread
 * acquired a recursive level while the first try was contending on the table
 * guard. */
static inline int _rin_stdio_stream_lock_wait(FILE* stream) {
    uintptr_t owner;
    size_t index;
    size_t free_index = _RIN_STDIO_LOCK_CAPACITY;
    size_t wait_index = _RIN_STDIO_LOCK_CAPACITY;
    volatile uint32_t* wait_word = NULL;
    uint32_t expected = 0u;
    int wait_for_entry = 0;

    if (!stream) return 1;
    owner = _rin_stdio_lock_owner_token();
    _rin_stdio_lock_table_enter();
    for (index = 0u; index < _RIN_STDIO_LOCK_CAPACITY; ++index) {
        struct _RinStdioLockOwner* entry = &_rin_stdio_lock_owners[index];
        if (entry->stream == stream) {
            if (entry->closing != 0u) {
                _rin_stdio_lock_table_leave();
                return 1;
            }
            if (entry->owner == owner && entry->depth != UINT32_MAX) {
                ++entry->depth;
                _rin_stdio_lock_table_leave();
                return 2;
            }
            if (entry->owner == 0u && entry->waiters == 0u) {
                entry->owner = owner;
                entry->depth = 1u;
                _rin_stdio_lock_table_leave();
                return 2;
            }
            if (entry->waiters == UINT32_MAX) {
                wait_word = &_rin_stdio_lock_table_seq;
                expected = __atomic_load_n(wait_word, __ATOMIC_RELAXED);
            } else {
                ++entry->waiters;
                wait_index = index;
                wait_word = &entry->wait_seq;
                expected = __atomic_load_n(wait_word, __ATOMIC_RELAXED);
                wait_for_entry = 1;
            }
            break;
        }
        if (!entry->stream && entry->waiters == 0u &&
            free_index == _RIN_STDIO_LOCK_CAPACITY)
            free_index = index;
    }

    if (wait_word) {
        _rin_stdio_lock_table_leave();
        _rin_stdio_wait_word(wait_word, expected);
        if (!wait_for_entry) return 0;

        _rin_stdio_lock_table_enter();
        {
            struct _RinStdioLockOwner* entry =
                &_rin_stdio_lock_owners[wait_index];
            int closing = entry->closing != 0u;
            int acquired = 0;
            int notify_table = 0;
            /* Claim the released lock while still under the table guard.
             * Otherwise every woken waiter can requeue behind the remaining
             * waiter count after the generation already changed. */
            if (!closing && entry->owner == 0u) {
                entry->owner = owner;
                entry->depth = 1u;
                acquired = 1;
            }
            if (entry->waiters > 0u) --entry->waiters;
            if (entry->waiters == 0u) {
                __atomic_add_fetch(&_rin_stdio_lock_table_seq, 1u,
                                   __ATOMIC_RELEASE);
                notify_table = 1;
            }
            _rin_stdio_lock_table_leave();
            if (notify_table)
                _rin_stdio_wake_word(&_rin_stdio_lock_table_seq,
                                     0x7fffffff);
            return closing ? 1 : (acquired ? 2 : 0);
        }
    }

    if (free_index != _RIN_STDIO_LOCK_CAPACITY) {
        _rin_stdio_lock_table_leave();
        return 0;
    }
    wait_word = &_rin_stdio_lock_table_seq;
    expected = __atomic_load_n(wait_word, __ATOMIC_RELAXED);
    _rin_stdio_lock_table_leave();
    _rin_stdio_wait_word(wait_word, expected);
    return 0;
}

_RIN_STDIO_API void flockfile(FILE* stream) {
    if (!stream) return;
    while (_rin_stdio_stream_lock_try(stream) != 0) {
        int wait_result = _rin_stdio_stream_lock_wait(stream);
        if (wait_result == 1 || wait_result == 2) return;
    }
}

_RIN_STDIO_API int ftrylockfile(FILE* stream) {
    return _rin_stdio_stream_lock_try(stream);
}

_RIN_STDIO_API void funlockfile(FILE* stream) {
    uintptr_t owner;
    size_t index;
    int saved_errno;
    if (!stream) return;
    saved_errno = errno;
    owner = _rin_stdio_lock_owner_token();
    _rin_stdio_lock_table_enter();
    for (index = 0u; index < _RIN_STDIO_LOCK_CAPACITY; ++index) {
        struct _RinStdioLockOwner* entry = &_rin_stdio_lock_owners[index];
        if (entry->stream == stream && entry->owner == owner) {
            if (entry->depth > 1u) {
                --entry->depth;
            } else {
                entry->owner = 0u;
                entry->depth = 0u;
                /* Keep the identity published until queued callers have
                 * observed this generation and retried. Otherwise a new
                 * caller can allocate a second lock record for this FILE. */
                if (entry->closing == 0u && entry->waiters == 0u)
                    entry->stream = NULL;
                __atomic_add_fetch(&entry->wait_seq, 1u, __ATOMIC_RELEASE);
                __atomic_add_fetch(&_rin_stdio_lock_table_seq, 1u,
                                   __ATOMIC_RELEASE);
                _rin_stdio_lock_table_leave();
                _rin_stdio_wake_word(&entry->wait_seq, 0x7fffffff);
                _rin_stdio_wake_word(&_rin_stdio_lock_table_seq,
                                     0x7fffffff);
                errno = saved_errno;
                return;
            }
            break;
        }
    }
    _rin_stdio_lock_table_leave();
    errno = saved_errno;
}

static inline void _rin_stdio_stream_lock_reset_closed(FILE* stream) {
    size_t index;
    for (index = 0u; index < _RIN_STDIO_LOCK_CAPACITY; ++index) {
        struct _RinStdioLockOwner* entry = &_rin_stdio_lock_owners[index];
        if (entry->stream == stream && entry->closing != 0u &&
            entry->owner == 0u && entry->waiters == 0u) {
            entry->closing = 0u;
            entry->depth = 0u;
            return;
        }
    }
}

static inline int _rin_stdio_stream_lock_close_begin(FILE* stream) {
    uintptr_t owner = _rin_stdio_lock_owner_token();
    struct _RinStdioLockOwner* closing_entry = NULL;
    size_t index;

    if (!stream) return 0;
    _rin_stdio_lock_table_enter();
    for (index = 0u; index < _RIN_STDIO_LOCK_CAPACITY; ++index) {
        struct _RinStdioLockOwner* entry = &_rin_stdio_lock_owners[index];
        if (entry->stream == stream && entry->owner == owner &&
            entry->closing == 0u) {
            entry->closing = 1u;
            __atomic_add_fetch(&entry->wait_seq, 1u, __ATOMIC_RELEASE);
            __atomic_add_fetch(&_rin_stdio_lock_table_seq, 1u,
                               __ATOMIC_RELEASE);
            closing_entry = entry;
            break;
        }
    }
    _rin_stdio_lock_table_leave();
    if (!closing_entry) return 0;

    _rin_stdio_wake_word(&closing_entry->wait_seq, 0x7fffffff);
    _rin_stdio_wake_word(&_rin_stdio_lock_table_seq, 0x7fffffff);
    for (;;) {
        uint32_t expected;
        _rin_stdio_lock_table_enter();
        if (closing_entry->waiters == 0u) {
            _rin_stdio_lock_table_leave();
            return 1;
        }
        expected = __atomic_load_n(&_rin_stdio_lock_table_seq,
                                  __ATOMIC_RELAXED);
        _rin_stdio_lock_table_leave();
        _rin_stdio_wait_word(&_rin_stdio_lock_table_seq, expected);
    }
}

/* Called by the pthread runtime after TLS destructors. A thread that exits
 * while holding FILE locks must release each owner record and wake waiters. */
_RIN_STDIO_API void rin_stdio_thread_exit_cleanup(void) {
    uintptr_t owner = _rin_stdio_lock_owner_token();
    volatile uint32_t* wake_entries[_RIN_STDIO_LOCK_CAPACITY];
    size_t wake_count = 0u;
    size_t index;

    _rin_stdio_lock_table_enter();
    for (index = 0u; index < _RIN_STDIO_LOCK_CAPACITY; ++index) {
        struct _RinStdioLockOwner* entry = &_rin_stdio_lock_owners[index];
        if (entry->stream && entry->owner == owner) {
            entry->owner = 0u;
            entry->depth = 0u;
            /* Waiters still refer to this record by index while parked. */
            if (entry->closing == 0u && entry->waiters == 0u)
                entry->stream = NULL;
            __atomic_add_fetch(&entry->wait_seq, 1u, __ATOMIC_RELEASE);
            wake_entries[wake_count++] = &entry->wait_seq;
        }
    }
    if (wake_count != 0u) {
        __atomic_add_fetch(&_rin_stdio_lock_table_seq, 1u, __ATOMIC_RELEASE);
    }
    _rin_stdio_lock_table_leave();

    for (index = 0u; index < wake_count; ++index)
        _rin_stdio_wake_word(wake_entries[index], 0x7fffffff);
    if (wake_count != 0u)
        _rin_stdio_wake_word(&_rin_stdio_lock_table_seq, 0x7fffffff);
}

/* 標準ストリーム */
#if defined(RIN_STDIO_OWNER_BUILD)
FILE _stdin_file  = { 0, 0, 0, O_RDONLY, NULL, 0, 0, 0, _IONBF, 0, 0u, 0, 0u, 0, 0 };
FILE _stdout_file = { 1, 0, 0, O_WRONLY, NULL, 0, 0, 0, _IONBF, 0, 0u, 0, 0u, 0, 0 };
FILE _stderr_file = { 2, 0, 0, O_WRONLY, NULL, 0, 0, 0, _IONBF, 0, 0u, 0, 0u, 0, 0 };
#else
static FILE _stdin_file  = { 0, 0, 0, O_RDONLY, NULL, 0, 0, 0, _IONBF, 0, 0u, 0, 0u, 0, 0 };
static FILE _stdout_file = { 1, 0, 0, O_WRONLY, NULL, 0, 0, 0, _IONBF, 0, 0u, 0, 0u, 0, 0 };
static FILE _stderr_file = { 2, 0, 0, O_WRONLY, NULL, 0, 0, 0, _IONBF, 0, 0u, 0, 0u, 0, 0 };
#endif

#ifdef stdin
#undef stdin
#endif
#ifdef stdout
#undef stdout
#endif
#ifdef stderr
#undef stderr
#endif
#define stdin  (&_stdin_file)
#define stdout (&_stdout_file)
#define stderr (&_stderr_file)

_RIN_STDIO_API FILE* rin_stdio_get_stdin(void) {
    return &_stdin_file;
}

_RIN_STDIO_API FILE* rin_stdio_get_stdout(void) {
    return &_stdout_file;
}

_RIN_STDIO_API FILE* rin_stdio_get_stderr(void) {
    return &_stderr_file;
}

/* C guarantees at least one byte of pushback per open stream.  Keep the first
 * five bytes inline for normal cursor work, then allocate a bounded spill only
 * when a scanner must restore a longer rejected token. */
#define _RIN_STDIO_UNGETC_INLINE_CAPACITY 5u
#define _RIN_STDIO_UNGETC_CAPACITY 4096u
static unsigned char _rin_stdio_ungetc_byte[_STDIO_MAX_FILES];
static unsigned char _rin_stdio_ungetc_valid[_STDIO_MAX_FILES];
static unsigned char _rin_stdio_ungetc_second_byte[_STDIO_MAX_FILES];
static unsigned char _rin_stdio_ungetc_second_valid[_STDIO_MAX_FILES];
static unsigned char _rin_stdio_ungetc_third_byte[_STDIO_MAX_FILES];
static unsigned char _rin_stdio_ungetc_third_valid[_STDIO_MAX_FILES];
static unsigned char _rin_stdio_ungetc_fourth_byte[_STDIO_MAX_FILES];
static unsigned char _rin_stdio_ungetc_fourth_valid[_STDIO_MAX_FILES];
static unsigned char _rin_stdio_ungetc_fifth_byte[_STDIO_MAX_FILES];
static unsigned char _rin_stdio_ungetc_fifth_valid[_STDIO_MAX_FILES];
static unsigned char* _rin_stdio_ungetc_spill[_STDIO_MAX_FILES];
static size_t _rin_stdio_ungetc_spill_count[_STDIO_MAX_FILES];
/* Wide scanners need to restore a complete rejected numeric token, not just
 * the two scalars required by the basic ungetwc contract.  Keep the common
 * case inline in FILE and allocate this bounded spill only on demand. */
#define _RIN_STDIO_WIDE_UNGETWC_CAPACITY 4096u
struct _RinStdioWideUngetwcOwner {
    FILE* stream;
    uint32_t* spill;
    size_t count;
    struct _RinStdioWideUngetwcOwner* next;
};
static struct _RinStdioWideUngetwcOwner*
    _rin_stdio_wide_ungetwc_owners;

static inline void _rin_stdio_ungetc_clear_inline(int stream_index) {
    _rin_stdio_ungetc_byte[stream_index] = 0u;
    _rin_stdio_ungetc_valid[stream_index] = 0u;
    _rin_stdio_ungetc_second_byte[stream_index] = 0u;
    _rin_stdio_ungetc_second_valid[stream_index] = 0u;
    _rin_stdio_ungetc_third_byte[stream_index] = 0u;
    _rin_stdio_ungetc_third_valid[stream_index] = 0u;
    _rin_stdio_ungetc_fourth_byte[stream_index] = 0u;
    _rin_stdio_ungetc_fourth_valid[stream_index] = 0u;
    _rin_stdio_ungetc_fifth_byte[stream_index] = 0u;
    _rin_stdio_ungetc_fifth_valid[stream_index] = 0u;
}

static inline size_t _rin_stdio_ungetc_count(int stream_index) {
    if (_rin_stdio_ungetc_spill[stream_index])
        return _rin_stdio_ungetc_spill_count[stream_index];
    return (size_t)(_rin_stdio_ungetc_valid[stream_index] != 0u) +
           (size_t)(_rin_stdio_ungetc_second_valid[stream_index] != 0u) +
           (size_t)(_rin_stdio_ungetc_third_valid[stream_index] != 0u) +
           (size_t)(_rin_stdio_ungetc_fourth_valid[stream_index] != 0u) +
           (size_t)(_rin_stdio_ungetc_fifth_valid[stream_index] != 0u);
}

static inline void _rin_stdio_ungetc_clear(int stream_index) {
    if (_rin_stdio_ungetc_spill[stream_index]) {
        free(_rin_stdio_ungetc_spill[stream_index]);
        _rin_stdio_ungetc_spill[stream_index] = (unsigned char*)0;
        _rin_stdio_ungetc_spill_count[stream_index] = 0u;
    }
    _rin_stdio_ungetc_clear_inline(stream_index);
}

static inline int _rin_stdio_ungetc_pop(int stream_index,
                                        unsigned char* value_out) {
    unsigned char* spill = _rin_stdio_ungetc_spill[stream_index];
    size_t count;
    size_t index;

    if (spill) {
        count = _rin_stdio_ungetc_spill_count[stream_index];
        if (count == 0u) return 0;
        *value_out = spill[0];
        if (count == 1u) {
            free(spill);
            _rin_stdio_ungetc_spill[stream_index] = (unsigned char*)0;
            _rin_stdio_ungetc_spill_count[stream_index] = 0u;
            return 1;
        }
        for (index = 1u; index < count; ++index) spill[index - 1u] = spill[index];
        _rin_stdio_ungetc_spill_count[stream_index] = count - 1u;
        return 1;
    }
    if (!_rin_stdio_ungetc_valid[stream_index]) return 0;
    *value_out = _rin_stdio_ungetc_byte[stream_index];
    _rin_stdio_ungetc_byte[stream_index] =
        _rin_stdio_ungetc_second_byte[stream_index];
    _rin_stdio_ungetc_valid[stream_index] =
        _rin_stdio_ungetc_second_valid[stream_index];
    _rin_stdio_ungetc_second_byte[stream_index] =
        _rin_stdio_ungetc_third_byte[stream_index];
    _rin_stdio_ungetc_second_valid[stream_index] =
        _rin_stdio_ungetc_third_valid[stream_index];
    _rin_stdio_ungetc_third_byte[stream_index] =
        _rin_stdio_ungetc_fourth_byte[stream_index];
    _rin_stdio_ungetc_third_valid[stream_index] =
        _rin_stdio_ungetc_fourth_valid[stream_index];
    _rin_stdio_ungetc_fourth_byte[stream_index] =
        _rin_stdio_ungetc_fifth_byte[stream_index];
    _rin_stdio_ungetc_fourth_valid[stream_index] =
        _rin_stdio_ungetc_fifth_valid[stream_index];
    _rin_stdio_ungetc_fifth_byte[stream_index] = 0u;
    _rin_stdio_ungetc_fifth_valid[stream_index] = 0u;
    return 1;
}

static inline int _rin_stdio_ungetc_push(int stream_index,
                                         unsigned char value) {
    unsigned char* spill = _rin_stdio_ungetc_spill[stream_index];
    size_t count = _rin_stdio_ungetc_count(stream_index);
    size_t index;

    if (count == _RIN_STDIO_UNGETC_CAPACITY) return 0;
    if (!spill && count == _RIN_STDIO_UNGETC_INLINE_CAPACITY) {
        spill = (unsigned char*)malloc(_RIN_STDIO_UNGETC_CAPACITY);
        if (!spill) return 0;
        spill[0] = _rin_stdio_ungetc_byte[stream_index];
        spill[1] = _rin_stdio_ungetc_second_byte[stream_index];
        spill[2] = _rin_stdio_ungetc_third_byte[stream_index];
        spill[3] = _rin_stdio_ungetc_fourth_byte[stream_index];
        spill[4] = _rin_stdio_ungetc_fifth_byte[stream_index];
        _rin_stdio_ungetc_spill[stream_index] = spill;
        _rin_stdio_ungetc_spill_count[stream_index] = count;
        _rin_stdio_ungetc_clear_inline(stream_index);
    }
    if (spill) {
        for (index = count; index != 0u; --index) spill[index] = spill[index - 1u];
        spill[0] = value;
        _rin_stdio_ungetc_spill_count[stream_index] = count + 1u;
        return 1;
    }
    if (_rin_stdio_ungetc_valid[stream_index]) {
        _rin_stdio_ungetc_fifth_byte[stream_index] =
            _rin_stdio_ungetc_fourth_byte[stream_index];
        _rin_stdio_ungetc_fifth_valid[stream_index] =
            _rin_stdio_ungetc_fourth_valid[stream_index];
        _rin_stdio_ungetc_fourth_byte[stream_index] =
            _rin_stdio_ungetc_third_byte[stream_index];
        _rin_stdio_ungetc_fourth_valid[stream_index] =
            _rin_stdio_ungetc_third_valid[stream_index];
        _rin_stdio_ungetc_third_byte[stream_index] =
            _rin_stdio_ungetc_second_byte[stream_index];
        _rin_stdio_ungetc_third_valid[stream_index] =
            _rin_stdio_ungetc_second_valid[stream_index];
        _rin_stdio_ungetc_second_byte[stream_index] =
            _rin_stdio_ungetc_byte[stream_index];
        _rin_stdio_ungetc_second_valid[stream_index] = 1u;
    }
    _rin_stdio_ungetc_byte[stream_index] = value;
    _rin_stdio_ungetc_valid[stream_index] = 1u;
    return 1;
}

static inline int _rin_stdio_stream_index(FILE* stream) {
    if (stream == stdin) return 0;
    if (stream == stdout) return 1;
    if (stream == stderr) return 2;
    for (int i = 3; i < _STDIO_MAX_FILES; ++i) {
        if (&_stdio_files[i] == stream && _stdio_files_used[i]) return i;
    }
    return -1;
}

_RIN_STDIO_INTERNAL void _rin_stdio_wide_pushback_clear(FILE* stream) {
    struct _RinStdioWideUngetwcOwner* previous = NULL;
    struct _RinStdioWideUngetwcOwner* owner =
        _rin_stdio_wide_ungetwc_owners;
    while (owner && owner->stream != stream) {
        previous = owner;
        owner = owner->next;
    }
    if (owner) {
        if (owner->spill) free(owner->spill);
        if (previous) previous->next = owner->next;
        else _rin_stdio_wide_ungetwc_owners = owner->next;
        free(owner);
    }
    if (stream) {
        stream->wide_pushback = 0u;
        stream->wide_pushback_valid = 0;
        stream->wide_pushback_second = 0u;
        stream->wide_pushback_second_valid = 0;
    }
}

_RIN_STDIO_INTERNAL int _rin_stdio_wide_pushback_pop(FILE* stream,
                                               uint32_t* value_out) {
    struct _RinStdioWideUngetwcOwner* owner;
    size_t count;
    if (!stream || !value_out) return 0;
    owner = _rin_stdio_wide_ungetwc_owners;
    while (owner && owner->stream != stream) owner = owner->next;
    if (owner && owner->spill) {
        count = owner->count;
        if (count == 0u) return 0;
        *value_out = owner->spill[count - 1u];
        owner->count = count - 1u;
        if (count == 1u) {
            free(owner->spill);
            owner->spill = (uint32_t*)0;
            _rin_stdio_wide_pushback_clear(stream);
        }
        return 1;
    }
    if (!stream->wide_pushback_valid) return 0;
    *value_out = stream->wide_pushback;
    if (stream->wide_pushback_second_valid) {
        stream->wide_pushback = stream->wide_pushback_second;
        stream->wide_pushback_second = 0u;
        stream->wide_pushback_second_valid = 0;
    } else {
        stream->wide_pushback = 0u;
        stream->wide_pushback_valid = 0;
    }
    return 1;
}

_RIN_STDIO_INTERNAL int _rin_stdio_wide_pushback_push(FILE* stream,
                                                uint32_t value) {
    struct _RinStdioWideUngetwcOwner* owner;
    uint32_t* spill;
    size_t count;
    if (!stream) return 0;
    owner = _rin_stdio_wide_ungetwc_owners;
    while (owner && owner->stream != stream) owner = owner->next;
    if (owner && owner->spill) {
        count = owner->count;
        if (count >= _RIN_STDIO_WIDE_UNGETWC_CAPACITY) return 0;
        owner->spill[count] = value;
        owner->count = count + 1u;
        return 1;
    }
    if (!stream->wide_pushback_valid) {
        stream->wide_pushback = value;
        stream->wide_pushback_valid = 1;
        return 1;
    }
    if (!stream->wide_pushback_second_valid) {
        stream->wide_pushback_second = stream->wide_pushback;
        stream->wide_pushback_second_valid = 1;
        stream->wide_pushback = value;
        return 1;
    }
    spill = (uint32_t*)malloc(_RIN_STDIO_WIDE_UNGETWC_CAPACITY *
                              sizeof(uint32_t));
    if (!spill) return 0;
    /* The inline representation is top-first; the spill is bottom-first so
     * pop() remains LIFO after the transition. */
    spill[0] = stream->wide_pushback_second;
    spill[1] = stream->wide_pushback;
    spill[2] = value;
    owner = (struct _RinStdioWideUngetwcOwner*)malloc(sizeof(*owner));
    if (!owner) {
        free(spill);
        return 0;
    }
    owner->stream = stream;
    owner->spill = spill;
    owner->count = 3u;
    owner->next = _rin_stdio_wide_ungetwc_owners;
    _rin_stdio_wide_ungetwc_owners = owner;
    stream->wide_pushback = 0u;
    stream->wide_pushback_valid = 0;
    stream->wide_pushback_second = 0u;
    stream->wide_pushback_second_valid = 0;
    return 1;
}

/* The syscall ABI returns a signed target word, whereas FILE retains an int
 * descriptor and ftell has a long public result.  Never narrow either result
 * before its representability has been established. */
static inline int _rin_stdio_fd_from_result(intptr_t result, int* output) {
    if (!output) {
        errno = EINVAL;
        return -1;
    }
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    *output = (int)result;
    return 0;
}

static inline int _rin_stdio_long_from_result(intptr_t result, long* output) {
    if (!output) {
        errno = EINVAL;
        return -1;
    }
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    if ((uintptr_t)result > (uintptr_t)LONG_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    *output = (long)result;
    return 0;
}

/* Normalize raw stdio syscall words once at the shared result boundary.  The
 * generic POSIXizer only recognizes the documented errno range; a backend
 * word outside it must not leak as a stale-errno failure through I/O/status
 * callers. */
static inline intptr_t _rin_stdio_syscall_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0 && result != -1) {
        errno = EIO;
        return -1;
    }
    return result;
}

static inline intptr_t _rin_stdio_read_backend(FILE* stream,
                                                unsigned char* output,
                                                size_t length) {
#if defined(RIN_USERSPACE)
    intptr_t result = 0;
    if (rin_stdio_memstream_read(stream, output, length, &result))
        return result;
#endif
    if (length > (size_t)_RIN_STDIO_MAX_SYSCALL_TRANSFER_BYTES)
        length = (size_t)_RIN_STDIO_MAX_SYSCALL_TRANSFER_BYTES;
    return _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL3(
        SYS_READ, (uintptr_t)stream->fd, (uintptr_t)output,
        (uintptr_t)length));
}

static inline intptr_t _rin_stdio_write_backend(FILE* stream,
                                                 const unsigned char* input,
                                                 size_t length) {
#if defined(RIN_USERSPACE)
    intptr_t result = 0;
    if (rin_stdio_memstream_write(stream, input, length, &result))
        return result;
#endif
    if (length > (size_t)_RIN_STDIO_MAX_SYSCALL_TRANSFER_BYTES)
        length = (size_t)_RIN_STDIO_MAX_SYSCALL_TRANSFER_BYTES;
    return _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL3(
        SYS_WRITE, (uintptr_t)stream->fd, (uintptr_t)input,
        (uintptr_t)length));
}

static inline int _rin_stdio_claim_byte_orientation(FILE* stream) {
    if (!stream) return 0;
    if (stream->orientation > 0) {
        errno = EINVAL;
        stream->error = 1;
        return 0;
    }
    if (stream->orientation == 0) stream->orientation = -1;
    return 1;
}

_RIN_STDIO_INTERNAL int _rin_stdio_claim_wide_orientation(FILE* stream) {
    if (!stream) return 0;
    if (stream->orientation < 0) {
        errno = EINVAL;
        stream->error = 1;
        return 0;
    }
    if (stream->orientation == 0) {
        _rin_stdio_wide_pushback_clear(stream);
        stream->orientation = 1;
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════
 * 文字出力
 * ═══════════════════════════════════════════════════════════════*/

static inline int _rin_stdio_write_all_raw(FILE* stream, const char* data,
                                           size_t length) {
    size_t written = 0;
    if (!stream || (!data && length != 0)) {
        errno = EINVAL;
        if (stream) stream->error = 1;
        return -1;
    }
    while (written < length) {
        intptr_t result = _rin_stdio_write_backend(
            stream, (const unsigned char*)(data + written),
            length - written);
        if (result < 0) {
            stream->error = 1;
            return -1;
        }
        if (result == 0) {
            errno = EIO;
            stream->error = 1;
            return -1;
        }
        if ((uintptr_t)result > (uintptr_t)(length - written)) {
            errno = EIO;
            stream->error = 1;
            return -1;
        }
        written += (size_t)(uintptr_t)result;
    }
    return 0;
}

static inline int _rin_stdio_output_buffer_enabled(const FILE* stream) {
    return stream && (stream->mode & O_ACCMODE) != O_RDONLY &&
        stream->buf && stream->buf_size != 0u &&
        (stream->buf_mode == _IOFBF || stream->buf_mode == _IOLBF);
}

static inline int _rin_stdio_output_is_buffered(const FILE* stream) {
    return _rin_stdio_output_buffer_enabled(stream);
}

static inline size_t _rin_stdio_buffer_position(const FILE* stream) {
    return stream ? stream->buf_pos : 0u;
}

static inline int _rin_stdio_input_buffer_enabled(const FILE* stream) {
    return stream && (stream->mode & O_ACCMODE) == O_RDONLY &&
        stream->buf && stream->buf_size != 0u &&
        (stream->buf_mode == _IOFBF || stream->buf_mode == _IOLBF);
}

static inline int _rin_stdio_input_buffer_valid(FILE* stream) {
    if (!_rin_stdio_input_buffer_enabled(stream)) return 1;
    if (stream->buf_pos <= stream->buf_end &&
        stream->buf_end <= stream->buf_size)
        return 1;
    errno = EIO;
    stream->error = 1;
    return 0;
}

static inline size_t _rin_stdio_input_buffer_unread(FILE* stream) {
    if (!_rin_stdio_input_buffer_enabled(stream)) return 0u;
    if (!_rin_stdio_input_buffer_valid(stream)) return (size_t)-1;
    return stream->buf_end - stream->buf_pos;
}

static inline void _rin_stdio_replace_buffer(FILE* stream,
                                             unsigned char* buffer,
                                             size_t size, int mode,
                                             int owned) {
    unsigned char* previous_buffer = stream->buf;
    int previous_owned = stream->buf_owned;

    stream->buf = buffer;
    stream->buf_size = size;
    stream->buf_pos = 0u;
    stream->buf_end = 0u;
    stream->buf_mode = mode;
    stream->buf_owned = owned;

    if (previous_owned && previous_buffer) free(previous_buffer);
}

static inline int _rin_stdio_flush_output_buffer(FILE* stream) {
    size_t consumed = 0u;
    size_t pending;
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    if (!_rin_stdio_output_buffer_enabled(stream)) return 0;
    if (stream->buf_pos > stream->buf_size) {
        errno = EIO;
        stream->error = 1;
        return -1;
    }

    pending = stream->buf_pos;
    while (consumed < pending) {
        intptr_t result = _rin_stdio_write_backend(
            stream, stream->buf + consumed, pending - consumed);
        if (result < 0 || result == 0 ||
            (uintptr_t)result > (uintptr_t)(pending - consumed)) {
            if (result == 0 || (result > 0 &&
                                (uintptr_t)result >
                                    (uintptr_t)(pending - consumed))) {
                errno = EIO;
            }
            if (consumed != 0u) {
                size_t remaining = pending - consumed;
                for (size_t index = 0u; index < remaining; ++index)
                    stream->buf[index] = stream->buf[consumed + index];
                stream->buf_pos = remaining;
            }
            stream->error = 1;
            return -1;
        }
        consumed += (size_t)(uintptr_t)result;
    }
    stream->buf_pos = 0u;
    return 0;
}

/* Returns the number of source bytes copied into the stream or handed to the
 * raw backend.  A flush error can occur after bytes have entered the caller's
 * buffer, so callers receive both that count and the failure state. */
_RIN_STDIO_INTERNAL size_t _rin_stdio_write_buffered(FILE* stream,
                                               const char* data,
                                               size_t length,
                                               int* failed) {
    size_t copied = 0u;
    if (failed) *failed = 0;
    if (!stream || (!data && length != 0u)) {
        errno = EINVAL;
        if (stream) stream->error = 1;
        if (failed) *failed = 1;
        return 0u;
    }
    if (!_rin_stdio_output_buffer_enabled(stream)) {
        if (_rin_stdio_write_all_raw(stream, data, length) != 0) {
            if (failed) *failed = 1;
            return 0u;
        }
        return length;
    }

    if (stream->buf_pos > stream->buf_size) {
        errno = EIO;
        stream->error = 1;
        if (failed) *failed = 1;
        return 0u;
    }

    while (copied < length) {
        size_t available;
        size_t chunk;
        int flush_line = 0;
        if (stream->buf_pos == stream->buf_size &&
            _rin_stdio_flush_output_buffer(stream) != 0) {
            if (failed) *failed = 1;
            return copied;
        }
        available = stream->buf_size - stream->buf_pos;
        chunk = length - copied;
        if (chunk > available) chunk = available;
        if (stream->buf_mode == _IOLBF) {
            for (size_t index = 0u; index < chunk; ++index) {
                if ((unsigned char)data[copied + index] == (unsigned char)'\n') {
                    chunk = index + 1u;
                    break;
                }
            }
        }
        for (size_t index = 0u; index < chunk; ++index) {
            unsigned char value = (unsigned char)data[copied + index];
            stream->buf[stream->buf_pos + index] = value;
            if (stream->buf_mode == _IOLBF && value == (unsigned char)'\n')
                flush_line = 1;
        }
        stream->buf_pos += chunk;
        copied += chunk;
        if (stream->buf_pos == stream->buf_size || flush_line) {
            if (_rin_stdio_flush_output_buffer(stream) != 0) {
                if (failed) *failed = 1;
                return copied;
            }
        }
    }
    return copied;
}

static inline int _rin_stdio_write_all_buffered(FILE* stream,
                                                 const char* data,
                                                 size_t length) {
    int failed = 0;
    size_t copied = _rin_stdio_write_buffered(stream, data, length, &failed);
    return (failed || copied != length) ? -1 : 0;
}

static inline int _rin_stdio_write_all(FILE* stream, const char* data,
                                       size_t length) {
    int result;
    if (!stream) return -1;
    flockfile(stream);
    if (!_rin_stdio_claim_byte_orientation(stream)) {
        funlockfile(stream);
        return -1;
    }
    result = _rin_stdio_write_all_buffered(stream, data, length);
    funlockfile(stream);
    return result;
}

static inline int _rin_fputc_unlocked(int c, FILE* stream) {
    char ch = (char)c;
    if (!stream) {
        errno = EINVAL;
        return EOF;
    }
    if (!_rin_stdio_claim_byte_orientation(stream)) return EOF;
    return _rin_stdio_write_all_buffered(stream, &ch, 1u) == 0
        ? (unsigned char)c : EOF;
}

_RIN_STDIO_API int putchar(int c) {
    char ch = (char)c;
    return _rin_stdio_write_all(stdout, &ch, 1) == 0
        ? (unsigned char)c : EOF;
}

_RIN_STDIO_API int putchar_unlocked(int c) {
    return _rin_fputc_unlocked(c, stdout);
}

#ifndef puts
_RIN_STDIO_API int puts(const char* s) {
    size_t length = 0;
    int result;
    if (!s) {
        errno = EINVAL;
        return EOF;
    }
    while (s[length]) ++length;
    flockfile(stdout);
    if (!_rin_stdio_claim_byte_orientation(stdout)) {
        funlockfile(stdout);
        return EOF;
    }
    result = _rin_stdio_write_all_buffered(stdout, s, length);
    if (result == 0) result = _rin_stdio_write_all_buffered(stdout, "\n", 1);
    funlockfile(stdout);
    return result == 0 ? 0 : EOF;
}
#endif /* puts */

_RIN_STDIO_API int fputc(int c, FILE* stream) {
    char ch = (char)c;
    return _rin_stdio_write_all(stream, &ch, 1) == 0
        ? (unsigned char)c : EOF;
}

_RIN_STDIO_API int putc_unlocked(int c, FILE* stream) {
    return _rin_fputc_unlocked(c, stream);
}

_RIN_STDIO_API int fputs(const char* s, FILE* stream) {
    size_t length = 0;
    if (!s) {
        errno = EINVAL;
        if (stream) stream->error = 1;
        return EOF;
    }
    while (s[length]) ++length;
    return _rin_stdio_write_all(stream, s, length) == 0 ? 0 : EOF;
}

/* ═══════════════════════════════════════════════════════════════
 * 文字入力
 * ═══════════════════════════════════════════════════════════════*/

/* Read at most one backend block.  For caller-provided input buffers, keep
 * the unread suffix in FILE and report only bytes actually copied to output.
 * The caller owns eof/error indicator transitions. */
_RIN_STDIO_INTERNAL intptr_t _rin_stdio_read_block(FILE* stream,
                                             unsigned char* output,
                                             size_t length) {
    size_t available;
    size_t copied;
    intptr_t result;
    if (!stream || (!output && length != 0u)) {
        errno = EINVAL;
        if (stream) stream->error = 1;
        return -1;
    }
    if (length == 0u) return 0;
    if (!_rin_stdio_input_buffer_enabled(stream)) {
        return _rin_stdio_read_backend(stream, output, length);
    }
    if (!_rin_stdio_input_buffer_valid(stream)) return -1;
    available = stream->buf_end - stream->buf_pos;
    if (available == 0u) {
        size_t filled = 0u;
        /* Refill in blocks for line-buffered input as well.  Bytes beyond a
         * newline remain in FILE for later calls, avoiding one syscall per
         * byte while preserving the caller-visible line boundary. */
        result = _rin_stdio_read_backend(stream, stream->buf,
                                         stream->buf_size);
        if (result < 0) return result;
        if ((uintptr_t)result > (uintptr_t)stream->buf_size) {
            errno = EIO;
            return -1;
        }
        filled = (size_t)(uintptr_t)result;
        stream->buf_pos = 0u;
        stream->buf_end = filled;
        available = stream->buf_end;
    }
    if (available == 0u) return 0;
    copied = length < available ? length : available;
    if (copied > (size_t)INTPTR_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    for (size_t index = 0u; index < copied; ++index)
        output[index] = stream->buf[stream->buf_pos + index];
    stream->buf_pos += copied;
    return (intptr_t)copied;
}

_RIN_STDIO_API int fgetc(FILE* stream);

_RIN_STDIO_API int getchar(void) {
    return fgetc(stdin);
}

static inline int _rin_fgetc_unlocked(FILE* stream) {
    unsigned char c = 0u;
    intptr_t n;
    int stream_index;
    if (!stream) {
        errno = EINVAL;
        return EOF;
    }
    if (!_rin_stdio_claim_byte_orientation(stream)) return EOF;
    if (_rin_stdio_flush_output_buffer(stream) != 0) return EOF;
    stream_index = _rin_stdio_stream_index(stream);
    if (stream_index >= 0 &&
        _rin_stdio_ungetc_pop(stream_index, &c)) {
        stream->eof = 0;
        return (int)c;
    }
    n = _rin_stdio_read_block(stream, &c, 1u);
    if (n < 0) {
        stream->error = 1;
        return EOF;
    }
    if (n == 0) {
        stream->eof = 1;
        return EOF;
    }
    if (n != 1) {
        errno = EIO;
        stream->error = 1;
        return EOF;
    }
    return (int)c;
}

_RIN_STDIO_API int getchar_unlocked(void) {
    return _rin_fgetc_unlocked(stdin);
}

_RIN_STDIO_API int fgetc(FILE* stream) {
    int result;
    if (!stream) {
        errno = EINVAL;
        return EOF;
    }
    flockfile(stream);
    result = _rin_fgetc_unlocked(stream);
    funlockfile(stream);
    return result;
}

_RIN_STDIO_API int getc_unlocked(FILE* stream) {
    return _rin_fgetc_unlocked(stream);
}

/* getc/putc - fgetc/fputc と同等 */
_RIN_STDIO_API int getc(FILE* stream) {
    return fgetc(stream);
}

_RIN_STDIO_API int putc(int c, FILE* stream) {
    return fputc(c, stream);
}

static inline char* _rin_fgets_unlocked(char* s, int size, FILE* stream) {
    char* p = s;
    int c;
    if (!s || !stream) {
        errno = EINVAL;
        if (stream) stream->error = 1;
        return NULL;
    }
    if (size <= 0) {
        errno = EINVAL;
        stream->error = 1;
        return NULL;
    }
    if (size == 1) {
        s[0] = '\0';
        return s;
    }
    while (--size > 0 && (c = _rin_fgetc_unlocked(stream)) != EOF) {
        *p++ = (char)c;
        if (c == '\n') break;
    }
    if (p == s) {
        if (stream->error) s[0] = '\0';
        return NULL;
    }
    *p = '\0';
    return s;
}

_RIN_STDIO_API char* fgets(char* s, int size, FILE* stream) {
    char* result;
    if (!stream) {
        errno = EINVAL;
        return NULL;
    }
    flockfile(stream);
    result = _rin_fgets_unlocked(s, size, stream);
    funlockfile(stream);
    return result;
}

/* POSIX getline() over the same bounded FILE/fgetc implementation.  The
 * buffer is caller-owned and grows geometrically; no host stdio is involved.
 */
_RIN_STDIO_API ssize_t getline(char** lineptr, size_t* n, FILE* stream) {
    size_t capacity;
    size_t length = 0u;
    char* replacement;

    if (!lineptr || !n || !stream) {
        errno = EINVAL;
        return (ssize_t)-1;
    }

    capacity = *n;
    if (!*lineptr || capacity < 2u) {
        capacity = capacity < 128u ? 128u : capacity;
        replacement = *lineptr ? (char*)realloc(*lineptr, capacity)
                               : (char*)malloc(capacity);
        if (!replacement) {
            errno = ENOMEM;
            return (ssize_t)-1;
        }
        *lineptr = replacement;
        *n = capacity;
    }

    flockfile(stream);
    for (;;) {
        int c = _rin_fgetc_unlocked(stream);
        if (c == EOF) {
            if (stream->error) {
                (*lineptr)[length] = '\0';
                funlockfile(stream);
                return (ssize_t)-1;
            }
            if (length == 0u) {
                funlockfile(stream);
                return (ssize_t)-1;
            }
            break;
        }
        if (length + 1u >= capacity) {
            size_t next_capacity = capacity <= ((size_t)-1 / 2u)
                ? capacity * 2u
                : (size_t)-1;
            if (next_capacity <= capacity || next_capacity < length + 2u) {
                errno = ENOMEM;
                funlockfile(stream);
                return (ssize_t)-1;
            }
            replacement = (char*)realloc(*lineptr, next_capacity);
            if (!replacement) {
                errno = ENOMEM;
                funlockfile(stream);
                return (ssize_t)-1;
            }
            *lineptr = replacement;
            *n = capacity = next_capacity;
        }
        (*lineptr)[length++] = (char)c;
        if (c == '\n')
            break;
    }
    (*lineptr)[length] = '\0';
    funlockfile(stream);
    return (ssize_t)length;
}

/* POSIX getdelim() using the same bounded FILE/fgetc implementation as
 * getline().  Keep the delimiter as an int so callers can pass any unsigned
 * char value as well as EOF-compatible values. */
_RIN_STDIO_API ssize_t getdelim(char** lineptr, size_t* n, int delimiter, FILE* stream) {
    size_t capacity;
    size_t length = 0u;
    char* replacement;

    if (!lineptr || !n || !stream) {
        errno = EINVAL;
        return (ssize_t)-1;
    }

    capacity = *n;
    if (!*lineptr || capacity < 2u) {
        capacity = capacity < 128u ? 128u : capacity;
        replacement = *lineptr ? (char*)realloc(*lineptr, capacity)
                               : (char*)malloc(capacity);
        if (!replacement) {
            errno = ENOMEM;
            return (ssize_t)-1;
        }
        *lineptr = replacement;
        *n = capacity;
    }

    flockfile(stream);
    for (;;) {
        int c = _rin_fgetc_unlocked(stream);
        if (c == EOF) {
            if (stream->error) {
                (*lineptr)[length] = '\0';
                funlockfile(stream);
                return (ssize_t)-1;
            }
            if (length == 0u) {
                funlockfile(stream);
                return (ssize_t)-1;
            }
            break;
        }
        if (length + 1u >= capacity) {
            size_t next_capacity = capacity <= ((size_t)-1 / 2u)
                ? capacity * 2u
                : (size_t)-1;
            if (next_capacity <= capacity || next_capacity < length + 2u) {
                errno = ENOMEM;
                funlockfile(stream);
                return (ssize_t)-1;
            }
            replacement = (char*)realloc(*lineptr, next_capacity);
            if (!replacement) {
                errno = ENOMEM;
                funlockfile(stream);
                return (ssize_t)-1;
            }
            *lineptr = replacement;
            *n = capacity = next_capacity;
        }
        (*lineptr)[length++] = (char)c;
        if (c == delimiter)
            break;
    }
    (*lineptr)[length] = '\0';
    funlockfile(stream);
    return (ssize_t)length;
}

/* ungetc - C17 guarantees one byte; this bounded owner accepts 4 KiB. */
static inline int _rin_ungetc_unlocked(int c, FILE* stream) {
    int idx;
    if (c == EOF || !stream) return EOF;
    idx = _rin_stdio_stream_index(stream);
    if (idx < 0 || !_rin_stdio_claim_byte_orientation(stream) ||
        !_rin_stdio_ungetc_push(idx, (unsigned char)c)) return EOF;
    stream->eof = 0;
    return (int)(unsigned char)c;
}

_RIN_STDIO_API int ungetc(int c, FILE* stream) {
    int result;
    if (!stream) return EOF;
    flockfile(stream);
    result = _rin_ungetc_unlocked(c, stream);
    funlockfile(stream);
    return result;
}

/* ═══════════════════════════════════════════════════════════════
 * ファイル操作
 * ═══════════════════════════════════════════════════════════════*/

static inline int _rin_stdio_parse_mode(const char* mode, unsigned int* flags) {
    int update = 0;
    int binary = 0;
    int exclusive = 0;
    size_t index;
    if (!mode || !mode[0] || !flags) {
        errno = EINVAL;
        return -1;
    }
    for (index = 1u; mode[index]; ++index) {
        if (mode[index] == '+') {
            if (update) {
                errno = EINVAL;
                return -1;
            }
            update = 1;
        } else if (mode[index] == 'b') {
            if (binary) {
                errno = EINVAL;
                return -1;
            }
            binary = 1;
        } else if (mode[index] == 'x') {
            if (exclusive) {
                errno = EINVAL;
                return -1;
            }
            exclusive = 1;
        } else {
            errno = EINVAL;
            return -1;
        }
    }
    if (mode[0] == 'r') {
        *flags = update ? O_RDWR : O_RDONLY;
    } else if (mode[0] == 'w') {
        *flags = (update ? O_RDWR : O_WRONLY) | O_CREATE | O_TRUNC;
    } else if (mode[0] == 'a') {
        *flags = (update ? O_RDWR : O_WRONLY) | O_APPEND | O_CREATE;
    } else {
        errno = EINVAL;
        return -1;
    }
    if (exclusive) {
        if (mode[0] != 'w') {
            errno = EINVAL;
            return -1;
        }
        *flags |= O_EXCL;
    }
    return 0;
}

_RIN_STDIO_API FILE* fopen(const char* path, const char* mode) {
    unsigned int flags = 0;

    if (!path) {
        errno = EINVAL;
        return NULL;
    }
    if (_rin_stdio_parse_mode(mode, &flags) != 0) return NULL;

    intptr_t raw_fd = _rin_stdio_syscall_result(
        _RIN_STDIO_SYSCALL2(SYS_OPEN, (uintptr_t)path,
                            (uintptr_t)flags));
    int fd;
    if (_rin_stdio_fd_from_result(raw_fd, &fd) != 0) {
        if (raw_fd >= 0)
            (void)_RIN_STDIO_SYSCALL1(SYS_CLOSE, (uintptr_t)raw_fd);
        return NULL;
    }

    /* 空きスロットを探す */
    _rin_stdio_lock_table_enter();
    for (int i = 3; i < _STDIO_MAX_FILES; i++) {
        if (!_stdio_files_used[i]) {
            _stdio_files_used[i] = 1;
            FILE* f = &_stdio_files[i];
            f->fd = fd;
            f->eof = 0;
            f->error = 0;
            f->mode = (int)flags;
            f->buf = NULL;
            f->buf_size = 0;
            f->buf_pos = 0;
            f->buf_end = 0;
            f->buf_mode = _IONBF;
            f->orientation = 0;
            f->wide_pushback = 0u;
            f->wide_pushback_valid = 0;
            f->wide_pushback_second = 0u;
            f->wide_pushback_second_valid = 0;
            f->buf_owned = 0;
            _rin_stdio_stream_lock_reset_closed(f);
            _rin_stdio_lock_table_leave();
            _rin_stdio_ungetc_clear(i);
            return f;
        }
    }

    _rin_stdio_lock_table_leave();
    /* スロットがない場合はfdを閉じて失敗 */
    _RIN_STDIO_SYSCALL1(SYS_CLOSE, (uintptr_t)fd);
    errno = EMFILE;
    return NULL;
}

static inline int _rin_fclose_unlocked(FILE* stream) {
    intptr_t ret;
    int stream_index;
    int flush_failed;
#if defined(RIN_USERSPACE)
    int memstream_handled = 0;
#endif
    unsigned char* previous_buffer;
    int previous_owned;
    if (!stream) {
        errno = EINVAL;
        return EOF;
    }

    stream_index = _rin_stdio_stream_index(stream);
    if (stream_index < 0 || !_stdio_files_used[stream_index]) {
        errno = EINVAL;
        return EOF;
    }
    flush_failed = _rin_stdio_flush_output_buffer(stream) != 0;

#if defined(RIN_USERSPACE)
    ret = rin_stdio_memstream_close(stream, &memstream_handled);
    if (!memstream_handled)
#endif
    {
        ret = _rin_stdio_syscall_result(
            _RIN_STDIO_SYSCALL1(SYS_CLOSE, (uintptr_t)stream->fd));
    }
    if (ret != 0) {
        if (ret > 0) errno = EIO;
        stream->error = 1;
    }
    if (stream_index >= 0) _rin_stdio_ungetc_clear(stream_index);
    _rin_stdio_wide_pushback_clear(stream);
    previous_buffer = stream->buf;
    previous_owned = stream->buf_owned;
    stream->buf = NULL;
    stream->buf_size = 0u;
    stream->buf_pos = 0u;
    stream->buf_end = 0u;
    stream->buf_mode = _IONBF;
    stream->buf_owned = 0;
    if (previous_owned && previous_buffer) free(previous_buffer);

    /* スロットを解放 */
    _rin_stdio_lock_table_enter();
    for (int i = 3; i < _STDIO_MAX_FILES; i++) {
        if (&_stdio_files[i] == stream) {
            _stdio_files_used[i] = 0;
            break;
        }
    }
    _rin_stdio_lock_table_leave();

    return (ret != 0 || flush_failed) ? EOF : 0;
}

_RIN_STDIO_API int fclose(FILE* stream) {
    int result;
    int stream_index;
    if (!stream) {
        errno = EINVAL;
        return EOF;
    }
    stream_index = _rin_stdio_stream_index(stream);
    if (stream_index < 0 || !_stdio_files_used[stream_index]) {
        errno = EINVAL;
        return EOF;
    }
    flockfile(stream);
    if (!_rin_stdio_stream_lock_close_begin(stream)) {
        funlockfile(stream);
        errno = EBADF;
        return EOF;
    }
    result = _rin_fclose_unlocked(stream);
    funlockfile(stream);
    return result;
}

static inline void _rin_freopen_discard_stream(FILE* stream, int stream_index) {
    unsigned char* previous_buffer = stream->buf;
    int previous_owned = stream->buf_owned;

    stream->fd = -1;
    stream->eof = 0;
    stream->error = 1;
    stream->mode = 0;
    stream->buf = NULL;
    stream->buf_size = 0u;
    stream->buf_pos = 0u;
    stream->buf_end = 0u;
    stream->buf_mode = _IONBF;
    stream->orientation = 0;
    _rin_stdio_wide_pushback_clear(stream);
    stream->buf_owned = 0;
    (void)_rin_stdio_stream_lock_close_begin(stream);
    _rin_stdio_lock_table_enter();
    _stdio_files_used[stream_index] = 0;
    _rin_stdio_lock_table_leave();
    _rin_stdio_ungetc_clear(stream_index);
    if (previous_owned && previous_buffer) free(previous_buffer);
}

static inline FILE* _rin_freopen_unlocked(const char* path, const char* mode, FILE* stream) {
    unsigned int flags;
    intptr_t new_fd;
    intptr_t close_result;
    int stream_index;
    unsigned char* previous_buffer;
    int previous_owned;
    if (!stream || !path) {
        errno = EINVAL;
        return NULL;
    }
#if defined(RIN_USERSPACE)
    if (rin_stdio_memstream_kind(stream) != RIN_STDIO_MEMSTREAM_NONE) {
        errno = EBADF;
        return NULL;
    }
#endif
    stream_index = _rin_stdio_stream_index(stream);
    if (stream_index < 0 || !_stdio_files_used[stream_index]) {
        errno = EINVAL;
        return NULL;
    }
    if (_rin_stdio_parse_mode(mode, &flags) != 0) {
        int saved_errno = errno;
        (void)_RIN_STDIO_SYSCALL1(SYS_CLOSE, (uintptr_t)stream->fd);
        _rin_freopen_discard_stream(stream, stream_index);
        errno = saved_errno;
        return NULL;
    }
    if (_rin_stdio_flush_output_buffer(stream) != 0) {
        int saved_errno = errno;
        (void)_RIN_STDIO_SYSCALL1(SYS_CLOSE, (uintptr_t)stream->fd);
        _rin_freopen_discard_stream(stream, stream_index);
        errno = saved_errno;
        return NULL;
    }
    new_fd = _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL2(
        SYS_OPEN, (uintptr_t)path, (uintptr_t)flags));
    if (new_fd < 0) {
        int saved_errno = errno;
        (void)_RIN_STDIO_SYSCALL1(SYS_CLOSE, (uintptr_t)stream->fd);
        _rin_freopen_discard_stream(stream, stream_index);
        errno = saved_errno;
        return NULL;
    }
    if ((uintptr_t)new_fd > (uintptr_t)INT_MAX) {
        int saved_errno = EOVERFLOW;
        (void)_RIN_STDIO_SYSCALL1(SYS_CLOSE, (uintptr_t)new_fd);
        (void)_RIN_STDIO_SYSCALL1(SYS_CLOSE, (uintptr_t)stream->fd);
        _rin_freopen_discard_stream(stream, stream_index);
        errno = saved_errno;
        return NULL;
    }
    close_result = _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL1(
        SYS_CLOSE, (uintptr_t)stream->fd));
    if (close_result != 0) {
        if (close_result > 0) errno = EIO;
        (void)_RIN_STDIO_SYSCALL1(SYS_CLOSE, (uintptr_t)new_fd);
        _rin_freopen_discard_stream(stream, stream_index);
        return NULL;
    }
    previous_buffer = stream->buf;
    previous_owned = stream->buf_owned;
    stream->fd = (int)new_fd;
    stream->eof = 0;
    stream->error = 0;
    stream->mode = (int)flags;
    stream->buf = NULL;
    stream->buf_size = 0u;
    stream->buf_pos = 0u;
    stream->buf_end = 0u;
    stream->buf_mode = _IONBF;
    stream->orientation = 0;
    _rin_stdio_wide_pushback_clear(stream);
    stream->buf_owned = 0;
    _rin_stdio_ungetc_clear(stream_index);
    if (previous_owned && previous_buffer) free(previous_buffer);
    return stream;
}

_RIN_STDIO_API FILE* freopen(const char* path, const char* mode, FILE* stream) {
    FILE* result;
    if (!stream) {
        errno = EINVAL;
        return NULL;
    }
    flockfile(stream);
    result = _rin_freopen_unlocked(path, mode, stream);
    funlockfile(stream);
    return result;
}

/* fdopen - open file stream from file descriptor */
_RIN_STDIO_API FILE* fdopen(int fd, const char* mode) {
    unsigned int flags = 0;
    intptr_t descriptor_flags;
    int descriptor_mode;
    if (fd < 0 || !mode || !mode[0]) {
        errno = EINVAL;
        return NULL;
    }
    if (_rin_stdio_parse_mode(mode, &flags) != 0) return NULL;
    flags &= ~(O_CREATE | O_TRUNC);
    descriptor_flags = _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL3(
        SYS_FCNTL, (uintptr_t)fd, (uintptr_t)F_GETFL, (uintptr_t)0u));
    if (descriptor_flags < 0) return NULL;
    if ((uintptr_t)descriptor_flags > (uintptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return NULL;
    }
    descriptor_mode = (int)descriptor_flags & O_ACCMODE;
    if (descriptor_mode != O_RDWR && descriptor_mode != (int)(flags & O_ACCMODE)) {
        errno = EINVAL;
        return NULL;
    }
    if ((flags & O_APPEND) != 0u && ((unsigned int)descriptor_flags & O_APPEND) == 0u) {
        intptr_t set_flags_result = _rin_stdio_syscall_result(
            _RIN_STDIO_SYSCALL3(
                SYS_FCNTL, (uintptr_t)fd, (uintptr_t)F_SETFL,
                (uintptr_t)((unsigned int)descriptor_flags | O_APPEND)));
        if (set_flags_result != 0) {
            if (set_flags_result > 0) errno = EIO;
            return NULL;
        }
    }

    /* 空きスロットを探す */
    _rin_stdio_lock_table_enter();
    for (int i = 3; i < _STDIO_MAX_FILES; i++) {
        if (!_stdio_files_used[i]) {
            _stdio_files_used[i] = 1;
            FILE* f = &_stdio_files[i];
            f->fd = fd;
            f->eof = 0;
            f->error = 0;
            f->mode = (int)flags;
            f->buf = NULL;
            f->buf_size = 0;
            f->buf_pos = 0;
            f->buf_end = 0;
            f->buf_mode = _IONBF;
            f->orientation = 0;
            f->wide_pushback = 0u;
            f->wide_pushback_valid = 0;
            f->wide_pushback_second = 0u;
            f->wide_pushback_second_valid = 0;
            f->buf_owned = 0;
            _rin_stdio_stream_lock_reset_closed(f);
            _rin_stdio_lock_table_leave();
            _rin_stdio_ungetc_clear(i);
            return f;
        }
    }
    _rin_stdio_lock_table_leave();
    errno = EMFILE;
    return NULL;
}

_RIN_STDIO_API int fseek(FILE* stream, long offset, int whence);

static inline int _rin_setvbuf_unlocked(FILE* stream, char* buf, int mode, size_t size) {
    int input_stream;
    unsigned char* replacement;
    int replacement_owned = 0;
    if (!stream || (mode != _IOFBF && mode != _IOLBF && mode != _IONBF)) {
        errno = EINVAL;
        return -1;
    }
    input_stream = (stream->mode & O_ACCMODE) == O_RDONLY;
    if (mode != _IONBF && size == 0u) {
        errno = EINVAL;
        return -1;
    }
    if (mode != _IONBF && size > _RIN_STDIO_BUFFER_POSITION_MASK) {
        errno = EOVERFLOW;
        return -1;
    }
    if (input_stream && !_rin_stdio_input_buffer_valid(stream)) return -1;
    if (input_stream && _rin_stdio_input_buffer_enabled(stream) &&
        stream->buf_pos != stream->buf_end) {
        /* Reconfiguration must first return prefetched bytes to the
         * descriptor position; otherwise it would silently discard input. */
        if (fseek(stream, 0L, SEEK_CUR) != 0) return -1;
    }
    replacement = (unsigned char*)buf;
    if (mode != _IONBF && !replacement) {
        replacement = (unsigned char*)malloc(size);
        if (!replacement) {
            errno = ENOMEM;
            return -1;
        }
        replacement_owned = 1;
    }
    if (_rin_stdio_flush_output_buffer(stream) != 0) {
        if (replacement_owned) free(replacement);
        return -1;
    }
    if (mode == _IONBF) {
        _rin_stdio_replace_buffer(stream, NULL, 0u, _IONBF, 0);
        return 0;
    }
    _rin_stdio_replace_buffer(stream, replacement, size, mode,
                              replacement_owned);
    return 0;
}

_RIN_STDIO_API int setvbuf(FILE* stream, char* buf, int mode, size_t size) {
    int result;
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    flockfile(stream);
    result = _rin_setvbuf_unlocked(stream, buf, mode, size);
    funlockfile(stream);
    return result;
}

_RIN_STDIO_API void setbuf(FILE* stream, char* buf) {
    if (!stream) {
        errno = EINVAL;
        return;
    }
    if (buf) {
        (void)setvbuf(stream, buf, _IOFBF, BUFSIZ);
    } else {
        (void)setvbuf(stream, NULL, _IONBF, 0u);
    }
}

static inline size_t _rin_fread_unlocked(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t total;
    size_t prefix = 0;
    intptr_t n;
    int buffered_input;
    int stream_index;
    unsigned char* output = (unsigned char*)ptr;
    if (!stream || (!ptr && size != 0 && nmemb != 0)) {
        errno = EINVAL;
        if (stream) stream->error = 1;
        return 0;
    }
    if (!_rin_stdio_claim_byte_orientation(stream)) return 0;
    if (size == 0 || nmemb == 0) return 0;
    if (_rin_stdio_flush_output_buffer(stream) != 0) return 0;
    if (nmemb > (size_t)-1 / size) {
        errno = EOVERFLOW;
        stream->error = 1;
        return 0;
    }
    total = size * nmemb;
    buffered_input = _rin_stdio_input_buffer_enabled(stream);
    if (buffered_input && !_rin_stdio_input_buffer_valid(stream)) return 0;
    stream_index = _rin_stdio_stream_index(stream);
    while (stream_index >= 0 && prefix < total &&
           _rin_stdio_ungetc_pop(stream_index, output + prefix)) {
        stream->eof = 0;
        ++prefix;
    }
    if (prefix == total) return nmemb;
    do {
        n = _rin_stdio_read_block(stream, output + prefix, total - prefix);
        if (n < 0) { stream->error = 1; return prefix / size; }
        if ((uintptr_t)n > (uintptr_t)(total - prefix)) {
            errno = EIO;
            stream->error = 1;
            return prefix / size;
        }
        if (n == 0) {
            stream->eof = 1;
            break;
        }
        prefix += (size_t)(uintptr_t)n;
    } while (prefix < total);
    return prefix / size;
}

_RIN_STDIO_API size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t result;
    if (!stream) {
        errno = EINVAL;
        return 0u;
    }
    flockfile(stream);
    result = _rin_fread_unlocked(ptr, size, nmemb, stream);
    funlockfile(stream);
    return result;
}

static inline size_t _rin_fwrite_unlocked(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t total;
    size_t copied;
    intptr_t n;
    if (!stream || (!ptr && size != 0 && nmemb != 0)) {
        errno = EINVAL;
        if (stream) stream->error = 1;
        return 0;
    }
    if (!_rin_stdio_claim_byte_orientation(stream)) return 0;
    if (size == 0 || nmemb == 0) return 0;
    if (nmemb > (size_t)-1 / size) {
        errno = EOVERFLOW;
        stream->error = 1;
        return 0;
    }
    total = size * nmemb;
    if (_rin_stdio_output_buffer_enabled(stream)) {
        copied = _rin_stdio_write_buffered(stream, (const char*)ptr, total,
                                           NULL);
        return copied / size;
    }
    copied = 0u;
    while (copied < total) {
        size_t remaining = total - copied;
        n = _rin_stdio_write_backend(
            stream, (const unsigned char*)ptr + copied, remaining);
        if (n < 0) {
            stream->error = 1;
            return copied / size;
        }
        if (n == 0 || (uintptr_t)n > (uintptr_t)remaining) {
            errno = EIO;
            stream->error = 1;
            return copied / size;
        }
        copied += (size_t)(uintptr_t)n;
    }
    return nmemb;
}

_RIN_STDIO_API size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t result;
    if (!stream) {
        errno = EINVAL;
        return 0u;
    }
    flockfile(stream);
    result = _rin_fwrite_unlocked(ptr, size, nmemb, stream);
    funlockfile(stream);
    return result;
}

static inline int _rin_fseek_unlocked(FILE* stream, long offset, int whence) {
    intptr_t ret;
    int64_t position = 0;
    size_t unread;
    int stream_index;
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    if (_rin_stdio_flush_output_buffer(stream) != 0) return -1;
    unread = _rin_stdio_input_buffer_unread(stream);
    if (unread == (size_t)-1) return -1;
    stream_index = _rin_stdio_stream_index(stream);
    if (whence == SEEK_CUR) {
        if (unread > (size_t)__LONG_MAX__ ||
            offset < (-__LONG_MAX__ - 1L) + (long)unread) {
            errno = EOVERFLOW;
            return -1;
        }
        offset -= (long)unread;
        if (stream_index >= 0) {
            size_t pushback = _rin_stdio_ungetc_count(stream_index);
            if (pushback > (size_t)__LONG_MAX__ ||
                offset < (-__LONG_MAX__ - 1L) + (long)pushback) {
                errno = EOVERFLOW;
                return -1;
            }
            offset -= (long)pushback;
        }
    }
#if defined(RIN_USERSPACE)
    if (rin_stdio_memstream_kind(stream) != RIN_STDIO_MEMSTREAM_NONE) {
        ret = rin_stdio_memstream_seek(stream, (int64_t)offset, whence) == 0
            ? 0 : -1;
    } else
#endif
    {
        uint64_t offset_bits = (uint64_t)(int64_t)offset;
        ret = _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL5(
            SYS_LSEEK64, (uintptr_t)stream->fd,
            (uintptr_t)(uint32_t)offset_bits,
            (uintptr_t)(uint32_t)(offset_bits >> 32),
            (uintptr_t)whence, (uintptr_t)&position));
        if (ret == 0 && position < 0) {
            errno = EIO;
            ret = -1;
        }
    }
    if (ret < 0) {
        stream->error = 1;
        return -1;
    }
    if (stream_index >= 0) _rin_stdio_ungetc_clear(stream_index);
    _rin_stdio_wide_pushback_clear(stream);
    stream->buf_pos = 0u;
    stream->buf_end = 0u;
    stream->eof = 0;
    return 0;
}

_RIN_STDIO_API int fseek(FILE* stream, long offset, int whence) {
    int result;
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    flockfile(stream);
    result = _rin_fseek_unlocked(stream, offset, whence);
    funlockfile(stream);
    return result;
}

static inline long _rin_ftell_unlocked(FILE* stream) {
    long position;
    intptr_t raw_position;
    int64_t wide_position = 0;
    size_t unread;
    int stream_index;
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
#if defined(RIN_USERSPACE)
    if (rin_stdio_memstream_kind(stream) != RIN_STDIO_MEMSTREAM_NONE) {
        int64_t memory_position;
        if (rin_stdio_memstream_tell(stream, &memory_position) != 0)
            return -1;
        if (memory_position < 0 || memory_position > (int64_t)LONG_MAX) {
            errno = EOVERFLOW;
            return -1;
        }
        position = (long)memory_position;
    } else
#endif
    {
        raw_position = _rin_stdio_syscall_result(
            _RIN_STDIO_SYSCALL5(SYS_LSEEK64, (uintptr_t)stream->fd,
                                0u, 0u, (uintptr_t)SEEK_CUR,
                                (uintptr_t)&wide_position));
        if (raw_position < 0) return -1;
        if (raw_position != 0 || wide_position < 0) {
            errno = EIO;
            return -1;
        }
        if ((uint64_t)wide_position > (uint64_t)LONG_MAX) {
            errno = EOVERFLOW;
            return -1;
        }
        position = (long)wide_position;
    }
    if (_rin_stdio_output_buffer_enabled(stream)) {
        if (stream->buf_pos > (size_t)__LONG_MAX__ ||
            position > __LONG_MAX__ - (long)stream->buf_pos) {
            errno = EOVERFLOW;
            return -1;
        }
        position += (long)stream->buf_pos;
    }
    unread = _rin_stdio_input_buffer_unread(stream);
    if (unread == (size_t)-1) return -1;
    if (unread != 0u) {
        if (unread > (size_t)__LONG_MAX__ || position < (long)unread) {
            errno = EIO;
            stream->error = 1;
            return -1;
        }
        position -= (long)unread;
    }
    stream_index = _rin_stdio_stream_index(stream);
    if (stream_index >= 0) {
        size_t pushback = _rin_stdio_ungetc_count(stream_index);
        if (pushback > (size_t)__LONG_MAX__) {
            errno = EIO;
            stream->error = 1;
            return -1;
        }
        if ((long)pushback <= position) position -= (long)pushback;
    }
    return position;
}

_RIN_STDIO_API long ftell(FILE* stream) {
    long result;
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    flockfile(stream);
    result = _rin_ftell_unlocked(stream);
    funlockfile(stream);
    return result;
}

/* POSIX large-file spellings used by libc++'s fstream implementation.  RinOS
 * exposes long/off_t with the same width on the x86_64 target, so preserve the
 * existing checked buffering and syscall path instead of introducing a second
 * seek implementation. */
_RIN_STDIO_API int fseeko(FILE* stream, off_t offset, int whence) {
    if (offset > (off_t)__LONG_MAX__ || offset < (off_t)(-__LONG_MAX__ - 1L)) {
        errno = EOVERFLOW;
        return -1;
    }
    return fseek(stream, (long)offset, whence);
}

_RIN_STDIO_API off_t ftello(FILE* stream) {
    long result = ftell(stream);
    if (result < 0) return (off_t)-1;
    return (off_t)result;
}

static inline int _rin_fseeko64_unlocked(FILE* stream, off64_t offset,
                                         int whence) {
    int64_t adjusted = (int64_t)offset;
    int64_t position = 0;
    intptr_t status;
    size_t unread;
    int stream_index;

    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    if (_rin_stdio_flush_output_buffer(stream) != 0) return -1;
    unread = _rin_stdio_input_buffer_unread(stream);
    if (unread == (size_t)-1) return -1;
    stream_index = _rin_stdio_stream_index(stream);
    if (whence == SEEK_CUR) {
        if (unread > (size_t)INT64_MAX ||
            adjusted < INT64_MIN + (int64_t)unread) {
            errno = EOVERFLOW;
            return -1;
        }
        adjusted -= (int64_t)unread;
        if (stream_index >= 0) {
            size_t pushback = _rin_stdio_ungetc_count(stream_index);
            if (pushback > (size_t)INT64_MAX ||
                adjusted < INT64_MIN + (int64_t)pushback) {
                errno = EOVERFLOW;
                return -1;
            }
            adjusted -= (int64_t)pushback;
        }
    }
#if defined(RIN_USERSPACE)
    if (rin_stdio_memstream_kind(stream) != RIN_STDIO_MEMSTREAM_NONE) {
        status = rin_stdio_memstream_seek(stream, adjusted, whence) == 0
            ? 0 : -1;
    } else
#endif
    {
        status = _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL5(
            SYS_LSEEK64, (uintptr_t)stream->fd,
            (uintptr_t)(uint32_t)(uint64_t)adjusted,
            (uintptr_t)(uint32_t)((uint64_t)adjusted >> 32),
            (uintptr_t)whence, (uintptr_t)&position));
    }
    if (status < 0) {
        stream->error = 1;
        return -1;
    }
    if (status != 0) {
        errno = EIO;
        stream->error = 1;
        return -1;
    }
    if (stream_index >= 0) _rin_stdio_ungetc_clear(stream_index);
    _rin_stdio_wide_pushback_clear(stream);
    stream->buf_pos = 0u;
    stream->buf_end = 0u;
    stream->eof = 0;
    return 0;
}

_RIN_STDIO_API int fseeko64(FILE* stream, off64_t offset, int whence) {
    int result;
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    flockfile(stream);
    result = _rin_fseeko64_unlocked(stream, offset, whence);
    funlockfile(stream);
    return result;
}

static inline off64_t _rin_ftello64_unlocked(FILE* stream) {
    int64_t position = 0;
    intptr_t status;
    size_t unread;
    int stream_index;

    if (!stream) {
        errno = EINVAL;
        return (off64_t)-1;
    }
#if defined(RIN_USERSPACE)
    if (rin_stdio_memstream_kind(stream) != RIN_STDIO_MEMSTREAM_NONE) {
        if (rin_stdio_memstream_tell(stream, &position) != 0)
            return (off64_t)-1;
    } else
#endif
    {
        status = _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL5(
            SYS_LSEEK64, (uintptr_t)stream->fd, 0u, 0u,
            (uintptr_t)SEEK_CUR, (uintptr_t)&position));
        if (status < 0) return (off64_t)-1;
        if (status != 0 || position < 0) {
            errno = EIO;
            return (off64_t)-1;
        }
    }
    if (_rin_stdio_output_buffer_enabled(stream)) {
        if (stream->buf_pos > (size_t)INT64_MAX ||
            position > INT64_MAX - (int64_t)stream->buf_pos) {
            errno = EOVERFLOW;
            return (off64_t)-1;
        }
        position += (int64_t)stream->buf_pos;
    }
    unread = _rin_stdio_input_buffer_unread(stream);
    if (unread == (size_t)-1) return (off64_t)-1;
    if (unread != 0u) {
        if (unread > (size_t)INT64_MAX || position < (int64_t)unread) {
            errno = EIO;
            stream->error = 1;
            return (off64_t)-1;
        }
        position -= (int64_t)unread;
    }
    stream_index = _rin_stdio_stream_index(stream);
    if (stream_index >= 0) {
        size_t pushback = _rin_stdio_ungetc_count(stream_index);
        if (pushback > (size_t)INT64_MAX) {
            errno = EIO;
            stream->error = 1;
            return (off64_t)-1;
        }
        if ((int64_t)pushback <= position) position -= (int64_t)pushback;
    }
    return (off64_t)position;
}

_RIN_STDIO_API off64_t ftello64(FILE* stream) {
    off64_t result;
    if (!stream) {
        errno = EINVAL;
        return (off64_t)-1;
    }
    flockfile(stream);
    result = _rin_ftello64_unlocked(stream);
    funlockfile(stream);
    return result;
}

static inline void _rin_rewind_unlocked(FILE* stream) {
    fseek(stream, 0, SEEK_SET);
    stream->eof = 0;
    stream->error = 0;
}

_RIN_STDIO_API void rewind(FILE* stream) {
    if (!stream) return;
    flockfile(stream);
    _rin_rewind_unlocked(stream);
    funlockfile(stream);
}

static inline int _rin_fgetpos_unlocked(FILE* stream, fpos_t* pos) {
#if defined(__RIN_FILE_OFFSET_BITS64)
    off64_t p = ftello64(stream);
#else
    long p = ftell(stream);
#endif
    if (p < 0) return -1;
    *pos = (fpos_t)p;
    return 0;
}

_RIN_STDIO_API int fgetpos(FILE* stream, fpos_t* pos) {
    int result;
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    flockfile(stream);
    result = _rin_fgetpos_unlocked(stream, pos);
    funlockfile(stream);
    return result;
}

static inline int _rin_fsetpos_unlocked(FILE* stream, const fpos_t* pos) {
#if defined(__RIN_FILE_OFFSET_BITS64)
    return _rin_fseeko64_unlocked(stream, (off64_t)*pos, SEEK_SET);
#else
    return fseek(stream, (long)*pos, SEEK_SET);
#endif
}

_RIN_STDIO_API int fsetpos(FILE* stream, const fpos_t* pos) {
    int result;
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    flockfile(stream);
    result = _rin_fsetpos_unlocked(stream, pos);
    funlockfile(stream);
    return result;
}

_RIN_STDIO_API int feof(FILE* stream) {
    int result;
    if (!stream) {
        errno = EINVAL;
        return 0;
    }
    flockfile(stream);
    result = stream->eof;
    funlockfile(stream);
    return result;
}

_RIN_STDIO_API int ferror(FILE* stream) {
    int result;
    if (!stream) {
        errno = EINVAL;
        return 0;
    }
    flockfile(stream);
    result = stream->error;
    funlockfile(stream);
    return result;
}

_RIN_STDIO_API void clearerr(FILE* stream) {
    if (!stream) return;
    flockfile(stream);
    stream->eof = 0;
    stream->error = 0;
    funlockfile(stream);
}

static inline int _rin_fflush_unlocked(FILE* stream) {
    int failed = 0;
    if (stream) {
        int stream_index;
        if (_rin_stdio_flush_output_buffer(stream) != 0) return EOF;
#if defined(RIN_USERSPACE)
        if (rin_stdio_memstream_flush(stream) < 0) return EOF;
#endif
        stream_index = _rin_stdio_stream_index(stream);
        if (_rin_stdio_input_buffer_enabled(stream) ||
            (stream_index >= 0 && _rin_stdio_ungetc_count(stream_index) != 0u))
            return fseek(stream, 0L, SEEK_CUR) == 0 ? 0 : EOF;
        return 0;
    }
    if (_rin_stdio_flush_output_buffer(stdout) != 0) failed = 1;
    if (_rin_stdio_flush_output_buffer(stderr) != 0) failed = 1;
    for (int index = 3; index < _STDIO_MAX_FILES; ++index) {
        if (_stdio_files_used[index] &&
            _rin_stdio_flush_output_buffer(&_stdio_files[index]) != 0)
            failed = 1;
    }
    return failed ? EOF : 0;
}

static inline int _rin_fflush_all(void) {
    FILE* streams[_STDIO_MAX_FILES];
    size_t count = 0u;
    int failed = 0;
    int index;

    /* Standard streams are fixed objects; keep their owner through the
     * complete flush.  Pool entries are static too, so snapshot only their
     * publication identity under the pool guard before taking FILE locks. */
    flockfile(stdout);
    flockfile(stderr);
    _rin_stdio_lock_table_enter();
    for (index = 3; index < _STDIO_MAX_FILES; ++index) {
        if (_stdio_files_used[index] && count < _STDIO_MAX_FILES)
            streams[count++] = &_stdio_files[index];
    }
    _rin_stdio_lock_table_leave();

    if (_rin_fflush_unlocked(stdout) != 0) failed = 1;
    if (_rin_fflush_unlocked(stderr) != 0) failed = 1;
    for (index = 0; index < (int)count; ++index) {
        flockfile(streams[index]);
        if (_rin_fflush_unlocked(streams[index]) != 0) failed = 1;
        funlockfile(streams[index]);
    }
    funlockfile(stderr);
    funlockfile(stdout);
    return failed ? EOF : 0;
}

_RIN_STDIO_API int fflush(FILE* stream) {
    int result;
    if (stream) {
        flockfile(stream);
        result = _rin_fflush_unlocked(stream);
        funlockfile(stream);
        return result;
    }
    return _rin_fflush_all();
}

_RIN_STDIO_API int fileno(FILE* stream) {
    int result;
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    flockfile(stream);
#if defined(RIN_USERSPACE)
    if (rin_stdio_memstream_kind(stream) != RIN_STDIO_MEMSTREAM_NONE) {
        funlockfile(stream);
        errno = EBADF;
        return -1;
    }
#endif
    result = stream->fd;
    funlockfile(stream);
    return result;
}

/* ═══════════════════════════════════════════════════════════════
 * フォーマット出力
 * _STDIO_GLOBAL_IMPL定義時はグローバルシンボルとして出力
 * ═══════════════════════════════════════════════════════════════*/

/* フォーマット関数のリンケージ制御 */
#ifdef _STDIO_GLOBAL_IMPL
  #define _STDIO_PRINTF_FN  /* グローバル（外部リンケージ） */
#else
  #define _STDIO_PRINTF_FN  static inline
#endif

/* フォーマット関数のコンパイル条件:
 * sprintfがマクロ未定義 または _STDIO_GLOBAL_IMPL定義時に有効。
 * カーネルコンポーネント(rin.h/msvcrt.c)ではsprintfがマクロ定義済みのためスキップ。 */
#if !defined(sprintf) || defined(_STDIO_GLOBAL_IMPL)
typedef struct _RinStdioFormatSink {
    char* buffer;
    size_t capacity;
    size_t length;
} _RinStdioFormatSink;

static inline int _rin_stdio_format_emit(_RinStdioFormatSink* sink,
                                         char value) {
    if (!sink || sink->length >= (size_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    if (sink->buffer && sink->capacity > 0u &&
        sink->length < sink->capacity - 1u)
        sink->buffer[sink->length] = value;
    ++sink->length;
    return 0;
}

static inline int _rin_stdio_format_repeat(_RinStdioFormatSink* sink,
                                          char value, size_t count) {
    size_t original_count = count;
    size_t writable = 0u;
    if (!sink || count > (size_t)INT_MAX - sink->length) {
        errno = EOVERFLOW;
        return -1;
    }
    if (sink->buffer && sink->capacity > 0u &&
        sink->length < sink->capacity - 1u) {
        writable = sink->capacity - 1u - sink->length;
        if (writable > count) writable = count;
        for (size_t index = 0u; index < writable; ++index)
            sink->buffer[sink->length + index] = value;
    }
    sink->length += original_count;
    return 0;
}

static inline int _rin_stdio_format_append(_RinStdioFormatSink* sink,
                                           const char* text, size_t length) {
    if (!sink || (!text && length != 0u)) {
        errno = EINVAL;
        return -1;
    }
    if (length > (size_t)INT_MAX - sink->length) {
        errno = EOVERFLOW;
        return -1;
    }
    if (sink->buffer && sink->capacity > 0u &&
        sink->length < sink->capacity - 1u) {
        size_t writable = sink->capacity - 1u - sink->length;
        if (writable > length) writable = length;
        for (size_t index = 0u; index < writable; ++index)
            sink->buffer[sink->length + index] = text[index];
    }
    sink->length += length;
    return 0;
}

static inline int _rin_stdio_format_integer(
        _RinStdioFormatSink* sink, uintmax_t magnitude, int is_signed,
        int negative, unsigned int base, int uppercase, int width,
        int left_align, int zero, int plus, int space, int alternate,
        int precision) {
    const char* alphabet = uppercase
        ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        : "0123456789abcdefghijklmnopqrstuvwxyz";
    char reversed[sizeof(uintmax_t) * CHAR_BIT];
    size_t digit_count = 0u;
    size_t precision_zeroes = 0u;
    size_t prefix_length = 0u;
    size_t content_length;
    size_t field_padding;
    int zero_width_pad = zero && !left_align && precision < 0;
    int suppress_zero = magnitude == 0u && precision == 0;
    char sign = '\0';
    char prefix[2];

    if (base < 2u || base > 36u) {
        errno = EINVAL;
        return -1;
    }
    if (is_signed && !suppress_zero) {
        if (negative) sign = '-';
        else if (plus) sign = '+';
        else if (space) sign = ' ';
    }
    if (magnitude == 0u && precision == 0) {
        digit_count = 0u;
    } else {
        do {
            reversed[digit_count++] = alphabet[magnitude % base];
            magnitude /= base;
        } while (magnitude != 0u);
    }

    if (alternate && base == 8u) {
        if (digit_count == 0u) {
            precision = 1;
        } else if (reversed[digit_count - 1u] != '0' &&
                   (precision < 0 || (size_t)precision <= digit_count)) {
            precision = (int)digit_count + 1;
        }
    } else if (alternate && digit_count != 0u &&
               (base == 16u || base == 2u)) {
        prefix[0] = '0';
        prefix[1] = base == 2u
            ? (uppercase ? 'B' : 'b')
            : (uppercase ? 'X' : 'x');
        prefix_length = 2u;
    }

    if (precision >= 0 && (size_t)precision > digit_count)
        precision_zeroes = (size_t)precision - digit_count;
    if (digit_count > (size_t)-1 - precision_zeroes - prefix_length -
                          (sign != '\0' ? 1u : 0u)) {
        errno = EOVERFLOW;
        return -1;
    }
    content_length = digit_count + precision_zeroes + prefix_length +
                     (sign != '\0' ? 1u : 0u);
    field_padding = (width > 0 && (size_t)width > content_length)
        ? (size_t)width - content_length : 0u;

    if (!left_align && !zero_width_pad &&
        _rin_stdio_format_repeat(sink, ' ', field_padding) != 0)
        return -1;
    if (sign != '\0' && _rin_stdio_format_emit(sink, sign) != 0)
        return -1;
    if (prefix_length != 0u &&
        _rin_stdio_format_append(sink, prefix, prefix_length) != 0)
        return -1;
    if (zero_width_pad &&
        _rin_stdio_format_repeat(sink, '0', field_padding) != 0)
        return -1;
    if (_rin_stdio_format_repeat(sink, '0', precision_zeroes) != 0)
        return -1;
    while (digit_count != 0u) {
        if (_rin_stdio_format_emit(sink, reversed[--digit_count]) != 0)
            return -1;
    }
    if (left_align &&
        _rin_stdio_format_repeat(sink, ' ', field_padding) != 0)
        return -1;
    return 0;
}

_STDIO_PRINTF_FN int vsnprintf(char* buf, size_t size, const char* fmt, va_list ap) {
    _RinStdioFormatSink sink = {buf, size, 0u};
    char* format_allocation = (char*)0;
    if (!fmt || (!buf && size != 0u)) {
        errno = EINVAL;
        return -1;
    }

#define _RIN_STDIO_FORMAT_PUT(value) \
    do { if (_rin_stdio_format_emit(&sink, (char)(value)) != 0) goto format_error; } while (0)
#define _RIN_STDIO_FORMAT_REPEAT(value, count) \
    do { if (_rin_stdio_format_repeat(&sink, (char)(value), (size_t)(count)) != 0) goto format_error; } while (0)
#define _RIN_STDIO_FORMAT_APPEND(text, count) \
    do { if (_rin_stdio_format_append(&sink, (text), (size_t)(count)) != 0) goto format_error; } while (0)

    while (*fmt) {
        if (*fmt != '%') {
            _RIN_STDIO_FORMAT_PUT(*fmt++);
            continue;
        }
        fmt++; /* skip '%' */
        if (!*fmt) {
            _RIN_STDIO_FORMAT_PUT('%');
            break;
        }

        /* フラグとパディング */
        int pad = 0, zero = 0, left_align = 0;
        int plus = 0, space = 0, alternate = 0;
        int precision = -1;
        int width_set = 0;

        /* フラグ処理 */
        for (;;) {
            if (*fmt == '-') { left_align = 1; fmt++; continue; }
            if (*fmt == '0') { zero = 1; fmt++; continue; }
            if (*fmt == '+') { plus = 1; fmt++; continue; }
            if (*fmt == ' ') { space = 1; fmt++; continue; }
            if (*fmt == '#') { alternate = 1; fmt++; continue; }
            break;
        }

        /* 幅 (dynamic: %*d も扱う) */
        if (*fmt == '*') {
            width_set = 1;
            pad = va_arg(ap, int);
            if (pad == INT_MIN) { errno = EOVERFLOW; goto format_error; }
            if (pad < 0) { left_align = 1; pad = -pad; }
            fmt++;
        } else {
            width_set = *fmt >= '0' && *fmt <= '9';
            while (*fmt >= '0' && *fmt <= '9') {
                int digit = *fmt++ - '0';
                if (pad > (INT_MAX - digit) / 10) {
                    errno = EOVERFLOW;
                    goto format_error;
                }
                pad = pad * 10 + digit;
            }
        }

        /* 精度 (.n、.* 両対応) */
        if (*fmt == '.') {
            fmt++;
            if (*fmt == '*') {
                precision = va_arg(ap, int);
                fmt++;
            } else {
                precision = 0;
                while (*fmt >= '0' && *fmt <= '9') {
                    int digit = *fmt++ - '0';
                    if (precision > (INT_MAX - digit) / 10) {
                        errno = EOVERFLOW;
                        goto format_error;
                    }
                    precision = precision * 10 + digit;
                }
            }
        }

        /* 長さ修飾子 (l, ll, h) - スキップ */
        int is_long = 0;
        int is_longlong = 0;
        int is_longdouble = 0;
        int is_short = 0;
        int is_char = 0;
        int is_intmax = 0;
        int is_ptrdiff = 0;
        int is_size = 0;
        int has_width_modifier = 0;
        int width_fast = 0;
        unsigned int width_bits = 0u;
        if (*fmt == 'L') {
            fmt++;
            is_longdouble = 1;
        } else if (*fmt == 'l') {
            fmt++;
            is_long = 1;
            if (*fmt == 'l') { fmt++; is_longlong = 1; }
        } else if (*fmt == 'h') {
            fmt++;
            if (*fmt == 'h') {
                fmt++;
                is_char = 1;
            } else {
                is_short = 1;
            }
        } else if (*fmt == 'z') {
            fmt++;
            is_size = 1;
        } else if (*fmt == 'j') {
            fmt++;
            is_intmax = 1;
        } else if (*fmt == 't') {
            fmt++;
            is_ptrdiff = 1;
        } else if (*fmt == 'w') {
            int parsed = rin_stdio_parse_width_spec(
                &fmt, &width_bits, &width_fast);
            if (parsed != 1) goto format_error;
            has_width_modifier = 1;
        }

        if (has_width_modifier && *fmt != 'd' && *fmt != 'i' && *fmt != 'u' &&
            *fmt != 'o' && *fmt != 'x' && *fmt != 'X' && *fmt != 'b' &&
            *fmt != 'B' && *fmt != 'n') {
            errno = EINVAL;
            goto format_error;
        }
        if (*fmt == 'w') {
            errno = EINVAL;
            goto format_error;
        }

        switch (*fmt) {
            case 'd':
            case 'i':
            case 'u':
            case 'o':
            case 'x':
            case 'X':
            case 'b':
            case 'B': {
                int is_signed = *fmt == 'd' || *fmt == 'i';
                int negative = 0;
                unsigned int base = *fmt == 'o' ? 8u :
                    (*fmt == 'x' || *fmt == 'X') ? 16u :
                    (*fmt == 'b' || *fmt == 'B') ? 2u : 10u;
                uintmax_t magnitude;
                if (is_signed) {
                    intmax_t value;
                    if (has_width_modifier && width_bits == 8u)
                        value = width_fast
                            ? (int_fast8_t)RIN_STDIO_VA_ARG_PROMOTED(ap, int_fast8_t)
                            : (int8_t)RIN_STDIO_VA_ARG_PROMOTED(ap, int8_t);
                    else if (has_width_modifier && width_bits == 16u)
                        value = width_fast
                            ? (int_fast16_t)RIN_STDIO_VA_ARG_PROMOTED(ap, int_fast16_t)
                            : (int16_t)RIN_STDIO_VA_ARG_PROMOTED(ap, int16_t);
                    else if (has_width_modifier && width_bits == 32u)
                        value = width_fast
                            ? (int_fast32_t)RIN_STDIO_VA_ARG_PROMOTED(ap, int_fast32_t)
                            : (int32_t)RIN_STDIO_VA_ARG_PROMOTED(ap, int32_t);
                    else if (has_width_modifier && width_bits == 64u)
                        value = width_fast
                            ? (int_fast64_t)RIN_STDIO_VA_ARG_PROMOTED(ap, int_fast64_t)
                            : (int64_t)RIN_STDIO_VA_ARG_PROMOTED(ap, int64_t);
                    else if (is_char)
                        value = (signed char)va_arg(ap, int);
                    else if (is_short)
                        value = (short)va_arg(ap, int);
                    else if (is_longlong)
                        value = va_arg(ap, long long);
                    else if (is_long)
                        value = va_arg(ap, long);
                    else if (is_size || is_ptrdiff)
                        value = va_arg(ap, ptrdiff_t);
                    else if (is_intmax)
                        value = va_arg(ap, intmax_t);
                    else
                        value = va_arg(ap, int);
                    negative = value < 0;
                    magnitude = negative
                        ? (uintmax_t)(-(value + 1)) + 1u
                        : (uintmax_t)value;
                } else {
                    if (has_width_modifier && width_bits == 8u)
                        magnitude = width_fast
                            ? (uint_fast8_t)RIN_STDIO_VA_ARG_PROMOTED(ap, uint_fast8_t)
                            : (uint8_t)RIN_STDIO_VA_ARG_PROMOTED(ap, uint8_t);
                    else if (has_width_modifier && width_bits == 16u)
                        magnitude = width_fast
                            ? (uint_fast16_t)RIN_STDIO_VA_ARG_PROMOTED(ap, uint_fast16_t)
                            : (uint16_t)RIN_STDIO_VA_ARG_PROMOTED(ap, uint16_t);
                    else if (has_width_modifier && width_bits == 32u)
                        magnitude = width_fast
                            ? (uint_fast32_t)RIN_STDIO_VA_ARG_PROMOTED(ap, uint_fast32_t)
                            : (uint32_t)RIN_STDIO_VA_ARG_PROMOTED(ap, uint32_t);
                    else if (has_width_modifier && width_bits == 64u)
                        magnitude = width_fast
                            ? (uint_fast64_t)RIN_STDIO_VA_ARG_PROMOTED(ap, uint_fast64_t)
                            : (uint64_t)RIN_STDIO_VA_ARG_PROMOTED(ap, uint64_t);
                    else if (is_char)
                        magnitude = (unsigned char)va_arg(ap, int);
                    else if (is_short)
                        magnitude = (unsigned short)va_arg(ap, int);
                    else if (is_longlong)
                        magnitude = va_arg(ap, unsigned long long);
                    else if (is_long)
                        magnitude = va_arg(ap, unsigned long);
                    else if (is_size || is_ptrdiff)
                        magnitude = va_arg(ap, size_t);
                    else if (is_intmax)
                        magnitude = va_arg(ap, uintmax_t);
                    else
                        magnitude = va_arg(ap, unsigned int);
                }
                if (_rin_stdio_format_integer(
                        &sink, magnitude, is_signed, negative, base,
                        *fmt == 'X' || *fmt == 'B', pad, left_align, zero,
                        plus, space, alternate, precision) != 0)
                    goto format_error;
                break;
            }
            case 'n': {
                if (width_set || precision >= 0 || zero || left_align ||
                    plus || space || alternate || is_longdouble) {
                    errno = EINVAL;
                    goto format_error;
                }
#define RIN_STDIO_STORE_N(type, maximum) do { \
                    type* output; \
                    if ((uint64_t)sink.length > (uint64_t)(maximum)) { \
                        errno = ERANGE; \
                        goto format_error; \
                    } \
                    output = va_arg(ap, type*); \
                    if (!output) { errno = EFAULT; goto format_error; } \
                    *output = (type)sink.length; \
                } while (0)
                if (has_width_modifier) {
                    unsigned int type_bits = rin_stdio_width_type_bits(
                        width_bits, width_fast);
                    if (type_bits == 0u || type_bits > 64u) {
                        errno = ENOSYS;
                        goto format_error;
                    }
                    uint64_t maximum = type_bits == 64u
                        ? (uint64_t)INT64_MAX
                        : (UINT64_C(1) << (type_bits - 1u)) - 1u;
                    if (width_fast) {
                        switch (width_bits) {
                            case 8u: RIN_STDIO_STORE_N(int_fast8_t, maximum); break;
                            case 16u: RIN_STDIO_STORE_N(int_fast16_t, maximum); break;
                            case 32u: RIN_STDIO_STORE_N(int_fast32_t, maximum); break;
                            case 64u: RIN_STDIO_STORE_N(int_fast64_t, maximum); break;
                            default: errno = EINVAL; goto format_error;
                        }
                    } else {
                        switch (width_bits) {
                            case 8u: RIN_STDIO_STORE_N(int8_t, maximum); break;
                            case 16u: RIN_STDIO_STORE_N(int16_t, maximum); break;
                            case 32u: RIN_STDIO_STORE_N(int32_t, maximum); break;
                            case 64u: RIN_STDIO_STORE_N(int64_t, maximum); break;
                            default: errno = EINVAL; goto format_error;
                        }
                    }
                } else if (is_char) {
                    RIN_STDIO_STORE_N(signed char, SCHAR_MAX);
                } else if (is_short) {
                    RIN_STDIO_STORE_N(short, SHRT_MAX);
                } else if (is_longlong) {
                    RIN_STDIO_STORE_N(long long, LLONG_MAX);
                } else if (is_long) {
                    RIN_STDIO_STORE_N(long, LONG_MAX);
                } else if (is_size || is_ptrdiff) {
                    RIN_STDIO_STORE_N(ptrdiff_t, PTRDIFF_MAX);
                } else if (is_intmax) {
                    RIN_STDIO_STORE_N(intmax_t, INTMAX_MAX);
                } else {
                    RIN_STDIO_STORE_N(int, INT_MAX);
                }
#undef RIN_STDIO_STORE_N
                break;
            }
            case 'p': {
                uintptr_t n = (uintptr_t)va_arg(ap, void*);
                _RIN_STDIO_FORMAT_PUT('0');
                _RIN_STDIO_FORMAT_PUT('x');
                char tmp[2u * sizeof(uintptr_t)]; int i = 0;
                do { tmp[i++] = "0123456789abcdef"[n & 0xF]; n >>= 4; } while (n);
                while (i < (int)(2u * sizeof(uintptr_t))) tmp[i++] = '0';
                while (i--) _RIN_STDIO_FORMAT_PUT(tmp[i]);
                break;
            }
            case 'f':
            case 'F':
            case 'e':
            case 'E':
            case 'g':
            case 'G': {
#if !defined(__RINOS__) || defined(RINOS_ABI_HAS_LONG_DOUBLE)
                typedef long double rin_stdio_format_fp_t;
                rin_stdio_format_fp_t val = is_longdouble
                    ? va_arg(ap, long double)
                    : (rin_stdio_format_fp_t)va_arg(ap, double);
#else
                typedef double rin_stdio_format_fp_t;
                if (is_longdouble) {
                    errno = EINVAL;
                    goto format_error;
                }
                rin_stdio_format_fp_t val = va_arg(ap, double);
#endif
                int prec = (precision >= 0) ? precision : 6;
                char stack_ftmp[4096];
                size_t fcapacity = prec > 4000
                    ? (size_t)prec + 64u : sizeof(stack_ftmp);
                char* ftmp = stack_ftmp;
                int fi = 0;
                if (fcapacity > sizeof(stack_ftmp)) {
                    format_allocation = (char*)malloc(fcapacity);
                    if (!format_allocation) {
                        errno = ENOMEM;
                        goto format_error;
                    }
                    ftmp = format_allocation;
                }

                /* Handle special values */
                /* Check for NaN: NaN != NaN */
                if (val != val) {
                    const char* ns = "nan";
                    if (*fmt == 'F' || *fmt == 'E' || *fmt == 'G') ns = "NAN";
                    while (*ns && (size_t)fi + 1u < fcapacity)
                        ftmp[fi++] = *ns++;
                } else {
                    /* Check for infinity: val > large or val < -large */
                    int is_inf = 0;
                    if (val > 1e18) {
                        rin_stdio_format_fp_t t = val;
                        /* Keep doubling threshold - if val keeps being larger, it's inf */
                        for (int k = 0; k < 30; k++) t *= 0.5;
                        if (t > 1e18) is_inf = 1;
                    } else if (val < -1e18) {
                        rin_stdio_format_fp_t t = val;
                        for (int k = 0; k < 30; k++) t *= 0.5;
                        if (t < -1e18) is_inf = 1;
                    }
                    if (is_inf) {
                        if (val < 0 && (size_t)fi + 1u < fcapacity)
                            ftmp[fi++] = '-';
                        const char* ns = "inf";
                        if (*fmt == 'F' || *fmt == 'E' || *fmt == 'G') ns = "INF";
                        while (*ns && (size_t)fi + 1u < fcapacity)
                            ftmp[fi++] = *ns++;
                    } else {
                        /* Normal number */
                        if (val < 0) { ftmp[fi++] = '-'; val = -val; }

                        int scientific = (*fmt == 'e' || *fmt == 'E');
                        const int general = (*fmt == 'g' || *fmt == 'G');
                        int scientific_exponent = 0;
                        if (scientific || general) {
                            rin_stdio_format_fp_t normalized = val;
                            if (normalized != 0.0) {
                                while (normalized >= 10.0 &&
                                       scientific_exponent < 20000) {
                                    normalized /= 10.0;
                                    ++scientific_exponent;
                                }
                                while (normalized < 1.0 &&
                                       scientific_exponent > -20000) {
                                    normalized *= 10.0;
                                    --scientific_exponent;
                                }
                            }
                            if (general && (scientific_exponent >= prec ||
                                            scientific_exponent < -4)) {
                                scientific = 1;
                            }
                            if (scientific) val = normalized;
                        }

                        /* Separate integer and fractional parts */
                        unsigned long long ipart = (unsigned long long)val;
                        rin_stdio_format_fp_t frac =
                            val - (rin_stdio_format_fp_t)ipart;

                        /* Round the fractional part */
                        rin_stdio_format_fp_t round_add = 0.5;
                        for (int k = 0; k < prec && round_add != 0.0; k++)
                            round_add /= 10.0;
                        frac += round_add;
                        if (frac >= 1.0) { ipart++; frac -= 1.0; }

                        /* Integer part to string (reverse) */
                        char itmp[24]; int ii = 0;
                        if (ipart == 0) { itmp[ii++] = '0'; }
                        else { while (ipart > 0 && ii < 23) { itmp[ii++] = '0' + (int)(ipart % 10); ipart /= 10; } }
                        while (ii-- > 0 && (size_t)fi + 1u < fcapacity)
                            ftmp[fi++] = itmp[ii];

                        /* Decimal point and fractional digits */
                        int fractional_precision = scientific
                            ? (general ? (prec > 0 ? prec - 1 : 0) : prec)
                            : prec;
                        if (general && !scientific) {
                            fractional_precision = prec -
                                (scientific_exponent + 1);
                            if (fractional_precision < 0)
                                fractional_precision = 0;
                        }
                        if (fractional_precision > 0) {
                            if ((size_t)fi + 1u < fcapacity) ftmp[fi++] = '.';
                            for (int k = 0; k < fractional_precision &&
                                 (size_t)fi + 1u < fcapacity; k++) {
                                frac *= 10.0;
                                int digit = (int)frac;
                                if (digit > 9) digit = 9;
                                ftmp[fi++] = '0' + digit;
                                frac -= (rin_stdio_format_fp_t)digit;
                            }
                        }
                        if (scientific && (size_t)fi + 1u < fcapacity) {
                            ftmp[fi++] = (*fmt == 'E' || *fmt == 'G') ? 'E' : 'e';
                            if (scientific_exponent < 0) {
                                ftmp[fi++] = '-';
                                scientific_exponent = -scientific_exponent;
                            } else {
                                ftmp[fi++] = '+';
                            }
                            char exponent_digits[16];
                            int exponent_size = 0;
                            do {
                                exponent_digits[exponent_size++] = (char)(
                                    '0' + scientific_exponent % 10);
                                scientific_exponent /= 10;
                            } while (scientific_exponent != 0);
                            if (exponent_size < 2) exponent_digits[exponent_size++] = '0';
                            while (exponent_size-- > 0 &&
                                   (size_t)fi + 1u < fcapacity)
                                ftmp[fi++] = exponent_digits[exponent_size];
                        }
                        if (general && !alternate) {
                            int end = fi;
                            while (end > 0 && ftmp[end - 1] == '0') --end;
                            if (end > 0 && ftmp[end - 1] == '.') --end;
                            if (scientific) {
                                int exponent_mark = end;
                                while (exponent_mark > 0 &&
                                       ftmp[exponent_mark - 1] != 'e' &&
                                       ftmp[exponent_mark - 1] != 'E') --exponent_mark;
                                if (exponent_mark != 0) {
                                    int mantissa_end = exponent_mark - 1;
                                    while (mantissa_end > 0 &&
                                           ftmp[mantissa_end - 1] == '0')
                                        --mantissa_end;
                                    if (mantissa_end > 0 &&
                                        ftmp[mantissa_end - 1] == '.')
                                        --mantissa_end;
                                    int tail = fi - exponent_mark;
                                    for (int k = 0; k < tail; ++k)
                                        ftmp[mantissa_end + k] = ftmp[exponent_mark + k];
                                    fi = mantissa_end + tail;
                                } else {
                                    fi = end;
                                }
                            } else {
                                fi = end;
                            }
                        }
                    }
                }
                ftmp[fi] = '\0';

                /* Apply width/padding */
                if (plus && (fi == 0 || ftmp[0] != '-') &&
                    (size_t)fi + 1u < fcapacity) {
                    for (int k = fi; k > 0; --k) ftmp[k] = ftmp[k - 1];
                    ftmp[0] = '+';
                    ++fi;
                }
                if (space && !plus && (fi == 0 || ftmp[0] != '-') &&
                    (size_t)fi + 1u < fcapacity) {
                    for (int k = fi; k > 0; --k) ftmp[k] = ftmp[k - 1];
                    ftmp[0] = ' ';
                    ++fi;
                }
                if (alternate && prec == 0 &&
                    (size_t)fi + 1u < fcapacity) {
                    int point = 0;
                    while (point < fi && ftmp[point] != 'e' &&
                           ftmp[point] != 'E') ++point;
                    if (point == fi) ftmp[fi++] = '.';
                    else {
                        for (int k = fi; k > point; --k)
                            ftmp[k] = ftmp[k - 1];
                        ftmp[point] = '.';
                        ++fi;
                    }
                }
                if (!left_align && zero && fi > 0 &&
                    (ftmp[0] == '-' || ftmp[0] == '+' || ftmp[0] == ' ')) {
                    _RIN_STDIO_FORMAT_PUT(ftmp[0]);
                    _RIN_STDIO_FORMAT_REPEAT('0',
                        pad > fi ? (size_t)(pad - fi) : 0u);
                    _RIN_STDIO_FORMAT_APPEND(ftmp + 1, (size_t)(fi - 1));
                } else {
                    if (!left_align) {
                        _RIN_STDIO_FORMAT_REPEAT(' ',
                            pad > fi ? (size_t)(pad - fi) : 0u);
                    }
                    _RIN_STDIO_FORMAT_APPEND(ftmp, (size_t)fi);
                }
                if (left_align) {
                    _RIN_STDIO_FORMAT_REPEAT(' ',
                        pad > fi ? (size_t)(pad - fi) : 0u);
                }
                if (format_allocation) {
                    free(format_allocation);
                    format_allocation = (char*)0;
                }
                break;
            }
            case 's': {
                const char* s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                size_t len = 0u;
                if (precision >= 0) {
                    /* %.*s: 非 null 終端バッファも許容、precision まで読む */
                    while (len < (size_t)precision && s[len]) len++;
                } else {
                    while (s[len]) len++;
                }
                if (!left_align) {
                    _RIN_STDIO_FORMAT_REPEAT(' ',
                        (size_t)pad > len ? (size_t)pad - len : 0u);
                }
                _RIN_STDIO_FORMAT_APPEND(s, len);
                if (left_align) {
                    _RIN_STDIO_FORMAT_REPEAT(' ',
                        (size_t)pad > len ? (size_t)pad - len : 0u);
                }
                break;
            }
            case 'c':
                _RIN_STDIO_FORMAT_PUT(va_arg(ap, int));
                break;
            case '%':
                _RIN_STDIO_FORMAT_PUT('%');
                break;
            default:
                errno = EINVAL;
                goto format_error;
        }
        fmt++;
    }
    if (sink.buffer && sink.capacity > 0u) {
        size_t terminator = sink.length < sink.capacity
            ? sink.length : sink.capacity - 1u;
        sink.buffer[terminator] = '\0';
    }
#undef _RIN_STDIO_FORMAT_PUT
#undef _RIN_STDIO_FORMAT_REPEAT
#undef _RIN_STDIO_FORMAT_APPEND
    return (int)sink.length;

format_error:
    if (format_allocation) free(format_allocation);
    if (sink.buffer && sink.capacity > 0u) {
        size_t terminator = sink.length < sink.capacity
            ? sink.length : sink.capacity - 1u;
        sink.buffer[terminator] = '\0';
    }
#undef _RIN_STDIO_FORMAT_PUT
#undef _RIN_STDIO_FORMAT_REPEAT
#undef _RIN_STDIO_FORMAT_APPEND
    return -1;
}

_STDIO_PRINTF_FN int snprintf(char* buf, size_t size, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

_STDIO_PRINTF_FN int sprintf(char* buf, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, (size_t)-1, fmt, ap);
    va_end(ap);
    return n;
}

_STDIO_PRINTF_FN int vsprintf(char* buf, const char* fmt, va_list ap) {
    return vsnprintf(buf, (size_t)-1, fmt, ap);
}

static inline int _rin_stdio_vformat_write(FILE* stream, const char* fmt,
                                           va_list ap) {
    va_list copy;
    int required;
    int written;
    char* buffer;
    size_t capacity;

    if (!stream || !fmt) {
        errno = EINVAL;
        return -1;
    }
    va_copy(copy, ap);
    required = vsnprintf((char*)0, 0u, fmt, copy);
    va_end(copy);
    if (required < 0) return -1;
    if ((size_t)required >= (size_t)-1) {
        errno = EOVERFLOW;
        return -1;
    }
    capacity = (size_t)required + 1u;
    buffer = (char*)malloc(capacity);
    if (!buffer) {
        errno = ENOMEM;
        return -1;
    }
    va_copy(copy, ap);
    written = vsnprintf(buffer, capacity, fmt, copy);
    va_end(copy);
    if (written < 0 || written != required) {
        free(buffer);
        if (written >= 0) errno = EINVAL;
        return -1;
    }
    if (_rin_stdio_write_all(stream, buffer, (size_t)written) != 0) {
        free(buffer);
        return -1;
    }
    free(buffer);
    return written;
}

_STDIO_PRINTF_FN int vprintf(const char* fmt, va_list ap) {
    return _rin_stdio_vformat_write(stdout, fmt, ap);
}

_STDIO_PRINTF_FN int printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

_STDIO_PRINTF_FN int vfprintf(FILE* stream, const char* fmt, va_list ap) {
    return _rin_stdio_vformat_write(stream, fmt, ap);
}

_STDIO_PRINTF_FN int fprintf(FILE* stream, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vfprintf(stream, fmt, ap);
    va_end(ap);
    return n;
}
#endif /* !sprintf || _STDIO_GLOBAL_IMPL */

/* POSIX formatted-allocation helpers.  Size the allocation from the formatter's
 * required-length result and preserve the caller's va_list with va_copy. */
#if !defined(vasprintf) || defined(_STDIO_GLOBAL_IMPL)
_STDIO_PRINTF_FN int vasprintf(char** result, const char* fmt, va_list ap) {
    size_t capacity;
    va_list copy;
    int required;
    int written;
    char* buffer;
    if (!result || !fmt) {
        errno = EINVAL;
        return -1;
    }
    *result = (char*)0;
    va_copy(copy, ap);
    required = vsnprintf((char*)0, 0u, fmt, copy);
    va_end(copy);
    if (required < 0) return -1;
    if ((size_t)required >= (size_t)-1) {
        errno = EOVERFLOW;
        return -1;
    }
    capacity = (size_t)required + 1u;
    buffer = (char*)malloc(capacity);
    if (!buffer) {
        errno = ENOMEM;
        return -1;
    }
    va_copy(copy, ap);
    written = vsnprintf(buffer, capacity, fmt, copy);
    va_end(copy);
    if (written < 0 || written != required) {
        free(buffer);
        if (written >= 0) errno = EINVAL;
        return -1;
    }
    *result = buffer;
    return written;
}
#endif /* !vasprintf || _STDIO_GLOBAL_IMPL */

#if !defined(asprintf) || defined(_STDIO_GLOBAL_IMPL)
_STDIO_PRINTF_FN int asprintf(char** result, const char* fmt, ...) {
    va_list ap;
    int written;
    va_start(ap, fmt);
    written = vasprintf(result, fmt, ap);
    va_end(ap);
    return written;
}
#endif /* !asprintf || _STDIO_GLOBAL_IMPL */

/* ═══════════════════════════════════════════════════════════════
 * フォーマット入力
 * ═══════════════════════════════════════════════════════════════*/
#include "stdio_scanf_impl.h"

/* perror - エラーメッセージ出力 */
_RIN_STDIO_API void perror(const char* s) {
    size_t length = 0;
    int result = 0;
    flockfile(stderr);
    if (!_rin_stdio_claim_byte_orientation(stderr)) {
        funlockfile(stderr);
        return;
    }
    if (s && *s) {
        while (s[length]) ++length;
        if (_rin_stdio_write_all_buffered(stderr, s, length) != 0) result = -1;
        if (_rin_stdio_write_all_buffered(stderr, ": ", 2) != 0) result = -1;
    }
    if (_rin_stdio_write_all_buffered(stderr, "Error\n", 6) != 0) result = -1;
    (void)result;
    funlockfile(stderr);
}

/* remove - ファイル削除 */
_RIN_STDIO_API int remove(const char* pathname) {
    intptr_t result = _rin_stdio_syscall_result(
        _RIN_STDIO_SYSCALL1(SYS_UNLINK, (uintptr_t)pathname));
    if (result < 0 && errno == EISDIR) {
        result = _rin_stdio_syscall_result(
            _RIN_STDIO_SYSCALL1(SYS_RMDIR, (uintptr_t)pathname));
    }
    if (result < 0) return -1;
    if (result != 0) {
        errno = EOVERFLOW;
        return -1;
    }
    return 0;
}

/* rename - ファイル名変更 */
_RIN_STDIO_API int rename(const char* oldpath, const char* newpath) {
    intptr_t result = _rin_stdio_syscall_result(
        _RIN_STDIO_SYSCALL2(SYS_RENAME, (uintptr_t)oldpath,
                            (uintptr_t)newpath));
    if (result < 0) return -1;
    if (result != 0) {
        errno = EOVERFLOW;
        return -1;
    }
    return 0;
}

/* tmpfile - 一時ファイルを開く */
_RIN_STDIO_API FILE* tmpfile(void) {
    char name[] = "/tmp/rinXXXXXX";
    int fd = mkstemp(name);
    FILE* stream;
    if (fd < 0) return NULL;
    intptr_t unlink_result = _rin_stdio_syscall_result(
        _RIN_STDIO_SYSCALL1(SYS_UNLINK, (uintptr_t)name));
    if (unlink_result != 0) {
        if (unlink_result > 0) errno = EIO;
        int unlink_error = errno;
        (void)_RIN_STDIO_SYSCALL1(SYS_CLOSE, (uintptr_t)fd);
        errno = unlink_error;
        return NULL;
    }
    stream = fdopen(fd, "w+");
    if (!stream) {
        int fdopen_error = errno;
        (void)_RIN_STDIO_SYSCALL1(SYS_CLOSE, (uintptr_t)fd);
        errno = fdopen_error;
        return NULL;
    }
    return stream;
}

/* tmpnam - 一時ファイル名を生成 */
_RIN_STDIO_API char* tmpnam(char* s) {
#if defined(__cplusplus)
    static thread_local char fallback[L_tmpnam];
#else
    static _Thread_local char fallback[L_tmpnam];
#endif
    char generated[] = "/tmp/rinXXXXXX";
    char* destination = s != NULL ? s : fallback;
    size_t length = 0u;
    int fd = mkstemp(generated);
    intptr_t result;

    if (fd < 0) return NULL;
    /* mkstemp gives us an exclusive reservation.  tmpnam returns only the
     * name, so close and unlink the reservation before publishing it.  The
     * caller still owns the standard tmpnam race; callers needing atomic
     * creation must use mkstemp/tmpfile instead. */
    result = _rin_stdio_syscall_result(
        _RIN_STDIO_SYSCALL1(SYS_CLOSE, (uintptr_t)fd));
    if (result < 0) {
        int close_error = errno;
        (void)_rin_stdio_syscall_result(
            _RIN_STDIO_SYSCALL1(SYS_UNLINK, (uintptr_t)generated));
        errno = close_error;
        return NULL;
    }
    if (result != 0) {
        errno = EIO;
        (void)_RIN_STDIO_SYSCALL1(SYS_UNLINK, (uintptr_t)generated);
        return NULL;
    }
    result = _rin_stdio_syscall_result(
        _RIN_STDIO_SYSCALL1(SYS_UNLINK, (uintptr_t)generated));
    if (result < 0) return NULL;
    if (result != 0) {
        errno = EIO;
        return NULL;
    }
    while (generated[length] != '\0') ++length;
    if (length + 1u > L_tmpnam) {
        errno = EOVERFLOW;
        return NULL;
    }
    for (size_t index = 0u; index <= length; ++index)
        destination[index] = generated[index];
    return destination;
}

#endif /* !MIDL_PASS */

#ifdef __cplusplus
}
#endif

#if defined(__RIN_FILE_OFFSET_BITS64) && !defined(MIDL_PASS)
#define fseeko fseeko64
#define ftello ftello64
#endif

#undef _RIN_STDIO_API
#undef _RIN_STDIO_INTERNAL
#endif /* _RIN_STDIO_IMPL_H */
