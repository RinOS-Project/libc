/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - stdio.h
 * 標準入出力
 */

#ifndef _STDIO_H
#define _STDIO_H

#include "stddef.h"
#include "stdint.h"
#include "stdarg.h"
#include "sys/types.h"
#include "errno.h"
#include "limits.h"
#include "sys/syscall.h"
#include "fcntl_flags.h"
#include "stdlib.h"

#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif

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
#define FOPEN_MAX    16
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

/* ファイル位置型 */
typedef long fpos_t;

#ifndef MIDL_PASS
/* 静的FILEプール */
#define _STDIO_MAX_FILES 16
static FILE _stdio_files[_STDIO_MAX_FILES];
static int _stdio_files_used[_STDIO_MAX_FILES] = {1, 1, 1, 0};

/* FILE cannot grow without breaking the freestanding ABI, so explicit stream
 * locking lives in a bounded side table.  The owner token is thread-local and
 * recursion is counted, which lets an outer flockfile() safely call helpers
 * such as fputs() without self-deadlocking. */
#define _RIN_STDIO_LOCK_CAPACITY 32u
struct _RinStdioLockOwner {
    FILE* stream;
    uintptr_t owner;
    uint32_t depth;
};
static struct _RinStdioLockOwner
    _rin_stdio_lock_owners[_RIN_STDIO_LOCK_CAPACITY];
static volatile uint32_t _rin_stdio_lock_table_guard;

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
    _rin_stdio_lock_table_enter();
    for (index = 0u; index < _RIN_STDIO_LOCK_CAPACITY; ++index) {
        struct _RinStdioLockOwner* entry = &_rin_stdio_lock_owners[index];
        if (entry->stream == stream) {
            int success = 0;
            if (entry->owner == owner && entry->depth != UINT32_MAX) {
                ++entry->depth;
                success = 1;
            }
            _rin_stdio_lock_table_leave();
            return success ? 0 : 1;
        }
        if (!entry->stream && free_index == _RIN_STDIO_LOCK_CAPACITY)
            free_index = index;
    }
    if (free_index != _RIN_STDIO_LOCK_CAPACITY) {
        struct _RinStdioLockOwner* entry =
            &_rin_stdio_lock_owners[free_index];
        entry->stream = stream;
        entry->owner = owner;
        entry->depth = 1u;
        _rin_stdio_lock_table_leave();
        return 0;
    }
    _rin_stdio_lock_table_leave();
    return 1;
}

static inline void flockfile(FILE* stream) {
    if (!stream) return;
    while (_rin_stdio_stream_lock_try(stream) != 0) {
    }
}

static inline int ftrylockfile(FILE* stream) {
    return _rin_stdio_stream_lock_try(stream);
}

static inline void funlockfile(FILE* stream) {
    uintptr_t owner;
    size_t index;
    if (!stream) return;
    owner = _rin_stdio_lock_owner_token();
    _rin_stdio_lock_table_enter();
    for (index = 0u; index < _RIN_STDIO_LOCK_CAPACITY; ++index) {
        struct _RinStdioLockOwner* entry = &_rin_stdio_lock_owners[index];
        if (entry->stream == stream && entry->owner == owner) {
            if (entry->depth > 1u) {
                --entry->depth;
            } else {
                entry->stream = NULL;
                entry->owner = 0u;
                entry->depth = 0u;
            }
            break;
        }
    }
    _rin_stdio_lock_table_leave();
}

/* 標準ストリーム */
static FILE _stdin_file  = { 0, 0, 0, O_RDONLY, NULL, 0, 0, 0, _IONBF, 0, 0u, 0, 0u, 0, 0 };
static FILE _stdout_file = { 1, 0, 0, O_WRONLY, NULL, 0, 0, 0, _IONBF, 0, 0u, 0, 0u, 0, 0 };
static FILE _stderr_file = { 2, 0, 0, O_WRONLY, NULL, 0, 0, 0, _IONBF, 0, 0u, 0, 0u, 0, 0 };

#define stdin  (&_stdin_file)
#define stdout (&_stdout_file)
#define stderr (&_stderr_file)

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

static inline void _rin_stdio_wide_pushback_clear(FILE* stream) {
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

static inline int _rin_stdio_wide_pushback_pop(FILE* stream,
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

static inline int _rin_stdio_wide_pushback_push(FILE* stream,
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

static inline int _rin_stdio_claim_wide_orientation(FILE* stream) {
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
        intptr_t result = _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL3(
            SYS_WRITE, (uintptr_t)stream->fd,
            (uintptr_t)(data + written),
            (uintptr_t)(length - written)));
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
        intptr_t result = _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL3(
            SYS_WRITE, (uintptr_t)stream->fd,
            (uintptr_t)(stream->buf + consumed),
            (uintptr_t)(pending - consumed)));
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
static inline size_t _rin_stdio_write_buffered(FILE* stream,
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
    if (!stream || !_rin_stdio_claim_byte_orientation(stream)) return -1;
    if (_rin_stdio_stream_lock_try(stream) != 0) return -1;
    result = _rin_stdio_write_all_buffered(stream, data, length);
    funlockfile(stream);
    return result;
}

static inline int putchar(int c) {
    char ch = (char)c;
    return _rin_stdio_write_all(stdout, &ch, 1) == 0
        ? (unsigned char)c : EOF;
}

#ifndef puts
static inline int puts(const char* s) {
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

static inline int fputc(int c, FILE* stream) {
    char ch = (char)c;
    return _rin_stdio_write_all(stream, &ch, 1) == 0
        ? (unsigned char)c : EOF;
}

static inline int fputs(const char* s, FILE* stream) {
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
static inline intptr_t _rin_stdio_read_block(FILE* stream,
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
        return _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL3(
            SYS_READ, (uintptr_t)stream->fd, (uintptr_t)output,
            (uintptr_t)length));
    }
    if (!_rin_stdio_input_buffer_valid(stream)) return -1;
    available = stream->buf_end - stream->buf_pos;
    if (available == 0u) {
        size_t filled = 0u;
        if (stream->buf_mode == _IOLBF) {
            /* Input line buffering must not consume bytes from the next
             * logical line.  Read one byte at a time until newline, EOF, or
             * the caller-owned buffer is full; the existing cursor then keeps
             * the complete line prefix failure-atomically. */
            while (filled < stream->buf_size) {
                result = _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL3(
                    SYS_READ, (uintptr_t)stream->fd,
                    (uintptr_t)(stream->buf + filled), (uintptr_t)1u));
                if (result < 0) return result;
                if (result == 0) break;
                if (result != 1) {
                    errno = EIO;
                    return -1;
                }
                ++filled;
                if (stream->buf[filled - 1u] == (unsigned char)'\n') break;
            }
        } else {
            result = _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL3(
                SYS_READ, (uintptr_t)stream->fd, (uintptr_t)stream->buf,
                (uintptr_t)stream->buf_size));
            if (result < 0) return result;
            if ((uintptr_t)result > (uintptr_t)stream->buf_size) {
                errno = EIO;
                return -1;
            }
            filled = (size_t)(uintptr_t)result;
        }
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

static inline int fgetc(FILE* stream);

static inline int getchar(void) {
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

static inline int fgetc(FILE* stream) {
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

/* getc/putc - fgetc/fputc と同等 */
static inline int getc(FILE* stream) {
    return fgetc(stream);
}

static inline int putc(int c, FILE* stream) {
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

static inline char* fgets(char* s, int size, FILE* stream) {
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
static inline ssize_t getline(char** lineptr, size_t* n, FILE* stream) {
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

    for (;;) {
        int c = fgetc(stream);
        if (c == EOF) {
            if (length == 0u)
                return (ssize_t)-1;
            break;
        }
        if (length + 1u >= capacity) {
            size_t next_capacity = capacity <= ((size_t)-1 / 2u)
                ? capacity * 2u
                : (size_t)-1;
            if (next_capacity <= capacity || next_capacity < length + 2u) {
                errno = ENOMEM;
                return (ssize_t)-1;
            }
            replacement = (char*)realloc(*lineptr, next_capacity);
            if (!replacement) {
                errno = ENOMEM;
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

static inline int ungetc(int c, FILE* stream) {
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
    return 0;
}

static inline FILE* fopen(const char* path, const char* mode) {
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
            _rin_stdio_ungetc_clear(i);
            _rin_stdio_lock_table_leave();
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

    ret = _rin_stdio_syscall_result(
        _RIN_STDIO_SYSCALL1(SYS_CLOSE, (uintptr_t)stream->fd));
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

static inline int fclose(FILE* stream) {
    int result;
    if (!stream) {
        errno = EINVAL;
        return EOF;
    }
    flockfile(stream);
    result = _rin_fclose_unlocked(stream);
    funlockfile(stream);
    return result;
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
    stream_index = _rin_stdio_stream_index(stream);
    if (stream_index < 0 || !_stdio_files_used[stream_index]) {
        errno = EINVAL;
        return NULL;
    }
    if (_rin_stdio_parse_mode(mode, &flags) != 0) return NULL;
    if (_rin_stdio_flush_output_buffer(stream) != 0) return NULL;
    new_fd = _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL2(
        SYS_OPEN, (uintptr_t)path, (uintptr_t)flags));
    if (new_fd < 0) return NULL;
    if ((uintptr_t)new_fd > (uintptr_t)INT_MAX) {
        (void)_RIN_STDIO_SYSCALL1(SYS_CLOSE, (uintptr_t)new_fd);
        errno = EOVERFLOW;
        return NULL;
    }
    close_result = _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL1(
        SYS_CLOSE, (uintptr_t)stream->fd));
    if (close_result != 0) {
        if (close_result > 0) errno = EIO;
        (void)_RIN_STDIO_SYSCALL1(SYS_CLOSE, (uintptr_t)new_fd);
        previous_buffer = stream->buf;
        previous_owned = stream->buf_owned;
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
        _rin_stdio_lock_table_enter();
        _stdio_files_used[stream_index] = 0;
        _rin_stdio_lock_table_leave();
        _rin_stdio_ungetc_clear(stream_index);
        if (previous_owned && previous_buffer) free(previous_buffer);
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

static inline FILE* freopen(const char* path, const char* mode, FILE* stream) {
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
static inline FILE* fdopen(int fd, const char* mode) {
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
            _rin_stdio_ungetc_clear(i);
            _rin_stdio_lock_table_leave();
            return f;
        }
    }
    _rin_stdio_lock_table_leave();
    errno = EMFILE;
    return NULL;
}

static inline int fseek(FILE* stream, long offset, int whence);

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

static inline int setvbuf(FILE* stream, char* buf, int mode, size_t size) {
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

static inline void setbuf(FILE* stream, char* buf) {
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
    } while (buffered_input && prefix < total);
    return prefix / size;
}

static inline size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
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
    n = _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL3(
        SYS_WRITE, (uintptr_t)stream->fd,
        (uintptr_t)ptr, (uintptr_t)total));
    if (n < 0) { stream->error = 1; return 0; }
    if ((uintptr_t)n > (uintptr_t)total) {
        errno = EIO;
        stream->error = 1;
        return 0;
    }
    return (size_t)(uintptr_t)n / size;
}

static inline size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
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
    ret = _rin_stdio_syscall_result(_RIN_STDIO_SYSCALL3(
        SYS_SEEK, (uintptr_t)stream->fd,
        (uintptr_t)(intptr_t)offset, (uintptr_t)whence));
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

static inline int fseek(FILE* stream, long offset, int whence) {
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
    size_t unread;
    int stream_index;
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    raw_position = _rin_stdio_syscall_result(
        _RIN_STDIO_SYSCALL3(SYS_SEEK, (uintptr_t)stream->fd,
                            (uintptr_t)0u, (uintptr_t)SEEK_CUR));
    if (_rin_stdio_long_from_result(raw_position, &position) != 0)
        return -1;
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

static inline long ftell(FILE* stream) {
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
static inline int fseeko(FILE* stream, off_t offset, int whence) {
    if (offset > (off_t)__LONG_MAX__ || offset < (off_t)(-__LONG_MAX__ - 1L)) {
        errno = EOVERFLOW;
        return -1;
    }
    return fseek(stream, (long)offset, whence);
}

static inline off_t ftello(FILE* stream) {
    long result = ftell(stream);
    if (result < 0) return (off_t)-1;
    return (off_t)result;
}

static inline void _rin_rewind_unlocked(FILE* stream) {
    fseek(stream, 0, SEEK_SET);
    stream->eof = 0;
    stream->error = 0;
}

static inline void rewind(FILE* stream) {
    if (!stream) return;
    flockfile(stream);
    _rin_rewind_unlocked(stream);
    funlockfile(stream);
}

static inline int _rin_fgetpos_unlocked(FILE* stream, fpos_t* pos) {
    long p = ftell(stream);
    if (p < 0) return -1;
    *pos = (fpos_t)p;
    return 0;
}

static inline int fgetpos(FILE* stream, fpos_t* pos) {
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
    return fseek(stream, (long)*pos, SEEK_SET);
}

static inline int fsetpos(FILE* stream, const fpos_t* pos) {
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

static inline int feof(FILE* stream) {
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

static inline int ferror(FILE* stream) {
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

static inline void clearerr(FILE* stream) {
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

static inline int fflush(FILE* stream) {
    int result;
    if (stream) {
        flockfile(stream);
        result = _rin_fflush_unlocked(stream);
        funlockfile(stream);
        return result;
    }
    return _rin_fflush_all();
}

static inline int fileno(FILE* stream) {
    int result;
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    flockfile(stream);
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
_STDIO_PRINTF_FN int vsnprintf(char* buf, size_t size, const char* fmt, va_list ap) {

    char* p = buf;
    char* end = buf + size - 1;

    while (*fmt && p < end) {
        if (*fmt != '%') {
            *p++ = *fmt++;
            continue;
        }
        fmt++; /* skip '%' */

        /* フラグとパディング */
        int pad = 0, zero = 0, left_align = 0;
        int plus = 0, alternate = 0;
        int precision = -1;

        /* フラグ処理 */
        for (;;) {
            if (*fmt == '-') { left_align = 1; fmt++; continue; }
            if (*fmt == '0') { zero = 1; fmt++; continue; }
            if (*fmt == '+') { plus = 1; fmt++; continue; }
            if (*fmt == '#') { alternate = 1; fmt++; continue; }
            break;
        }

        /* 幅 (dynamic: %*d も扱う) */
        if (*fmt == '*') {
            pad = va_arg(ap, int);
            if (pad < 0) { left_align = 1; pad = -pad; }
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9') {
                pad = pad * 10 + (*fmt++ - '0');
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
                    precision = precision * 10 + (*fmt++ - '0');
                }
            }
        }

        /* 長さ修飾子 (l, ll, h) - スキップ */
        int is_long = 0;
        int is_longlong = 0;
        int is_longdouble = 0;
        if (*fmt == 'L') {
            fmt++;
            is_longdouble = 1;
        } else if (*fmt == 'l') {
            fmt++;
            is_long = 1;
            if (*fmt == 'l') { fmt++; is_longlong = 1; }
        } else if (*fmt == 'h') {
            fmt++;
            if (*fmt == 'h') fmt++;
        } else if (*fmt == 'z') {
            fmt++;
            is_long = 1;
        }

        switch (*fmt) {
            case 'd':
            case 'i': {
                long long n;
                if (is_longlong) n = va_arg(ap, long long);
                else if (is_long) n = va_arg(ap, long);
                else n = va_arg(ap, int);

                if (n < 0) { if (p < end) *p++ = '-'; n = -n; }
                char tmp[24]; int i = 0;
                do { tmp[i++] = '0' + n % 10; n /= 10; } while (n);
                if (!left_align) {
                    while (pad > i && p < end) { *p++ = zero ? '0' : ' '; pad--; }
                }
                while (i-- && p < end) *p++ = tmp[i];
                if (left_align) {
                    while (pad > 0 && p < end) { *p++ = ' '; pad--; }
                }
                break;
            }
            case 'u': {
                unsigned long long n;
                if (is_longlong) n = va_arg(ap, unsigned long long);
                else if (is_long) n = va_arg(ap, unsigned long);
                else n = va_arg(ap, unsigned int);

                char tmp[24]; int i = 0;
                do { tmp[i++] = '0' + n % 10; n /= 10; } while (n);
                if (!left_align) {
                    while (pad > i && p < end) { *p++ = zero ? '0' : ' '; pad--; }
                }
                while (i-- && p < end) *p++ = tmp[i];
                if (left_align) {
                    while (pad > 0 && p < end) { *p++ = ' '; pad--; }
                }
                break;
            }
            case 'x':
            case 'X': {
                unsigned long long n;
                if (is_longlong) n = va_arg(ap, unsigned long long);
                else if (is_long) n = va_arg(ap, unsigned long);
                else n = va_arg(ap, unsigned int);

                const char* hex = (*fmt == 'x') ? "0123456789abcdef" : "0123456789ABCDEF";
                char tmp[24]; int i = 0;
                do { tmp[i++] = hex[n & 0xF]; n >>= 4; } while (n);
                if (!left_align) {
                    while (pad > i && p < end) { *p++ = zero ? '0' : ' '; pad--; }
                }
                while (i-- && p < end) *p++ = tmp[i];
                if (left_align) {
                    while (pad > 0 && p < end) { *p++ = ' '; pad--; }
                }
                break;
            }
            case 'p': {
                uintptr_t n = (uintptr_t)va_arg(ap, void*);
                if (p < end) *p++ = '0';
                if (p < end) *p++ = 'x';
                char tmp[2u * sizeof(uintptr_t)]; int i = 0;
                do { tmp[i++] = "0123456789abcdef"[n & 0xF]; n >>= 4; } while (n);
                while (i < (int)(2u * sizeof(uintptr_t))) tmp[i++] = '0';
                while (i-- && p < end) *p++ = tmp[i];
                break;
            }
            case 'f':
            case 'F':
            case 'e':
            case 'E':
            case 'g':
            case 'G': {
                long double val = is_longdouble
                    ? va_arg(ap, long double)
                    : (long double)va_arg(ap, double);
                int prec = (precision >= 0) ? precision : 6;
                char ftmp[4096];
                int fi = 0;

                /* Handle special values */
                /* Check for NaN: NaN != NaN */
                if (val != val) {
                    const char* ns = "nan";
                    if (*fmt == 'F' || *fmt == 'E' || *fmt == 'G') ns = "NAN";
                    while (*ns && fi < 4095) ftmp[fi++] = *ns++;
                } else {
                    /* Check for infinity: val > large or val < -large */
                    int is_inf = 0;
                    if (val > 1e18) {
                        long double t = val;
                        /* Keep doubling threshold - if val keeps being larger, it's inf */
                        for (int k = 0; k < 30; k++) t *= 0.5;
                        if (t > 1e18) is_inf = 1;
                    } else if (val < -1e18) {
                        long double t = val;
                        for (int k = 0; k < 30; k++) t *= 0.5;
                        if (t < -1e18) is_inf = 1;
                    }
                    if (is_inf) {
                        if (val < 0 && fi < 4095) ftmp[fi++] = '-';
                        const char* ns = "inf";
                        if (*fmt == 'F' || *fmt == 'E' || *fmt == 'G') ns = "INF";
                        while (*ns && fi < 4095) ftmp[fi++] = *ns++;
                    } else {
                        /* Normal number */
                        if (val < 0) { ftmp[fi++] = '-'; val = -val; }

                        int scientific = (*fmt == 'e' || *fmt == 'E');
                        const int general = (*fmt == 'g' || *fmt == 'G');
                        int scientific_exponent = 0;
                        if (scientific || general) {
                            long double normalized = val;
                            if (normalized != 0.0L) {
                                while (normalized >= 10.0L &&
                                       scientific_exponent < 20000) {
                                    normalized *= 0.1L;
                                    ++scientific_exponent;
                                }
                                while (normalized < 1.0L &&
                                       scientific_exponent > -20000) {
                                    normalized *= 10.0L;
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
                        long double frac = val - (long double)ipart;

                        /* Round the fractional part */
                        long double round_add = 0.5L;
                        for (int k = 0; k < prec; k++) round_add *= 0.1L;
                        frac += round_add;
                        if (frac >= 1.0) { ipart++; frac -= 1.0; }

                        /* Integer part to string (reverse) */
                        char itmp[24]; int ii = 0;
                        if (ipart == 0) { itmp[ii++] = '0'; }
                        else { while (ipart > 0 && ii < 23) { itmp[ii++] = '0' + (int)(ipart % 10); ipart /= 10; } }
                        while (ii-- > 0 && fi < 4095) ftmp[fi++] = itmp[ii];

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
                            if (fi < 4095) ftmp[fi++] = '.';
                            for (int k = 0; k < fractional_precision &&
                                 fi < 4095; k++) {
                                frac *= 10.0;
                                int digit = (int)frac;
                                if (digit > 9) digit = 9;
                                ftmp[fi++] = '0' + digit;
                                frac -= (long double)digit;
                            }
                        }
                        if (scientific && fi < 4095) {
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
                            while (exponent_size-- > 0 && fi < 4095)
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
                if (plus && (fi == 0 || ftmp[0] != '-') && fi < 4095) {
                    for (int k = fi; k > 0; --k) ftmp[k] = ftmp[k - 1];
                    ftmp[0] = '+';
                    ++fi;
                }
                if (alternate && prec == 0 && fi < 4095) {
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
                if (!left_align) {
                    while (pad > fi && p < end) { *p++ = zero ? '0' : ' '; pad--; }
                }
                for (int k = 0; k < fi && p < end; k++) *p++ = ftmp[k];
                if (left_align) {
                    while (pad > fi && p < end) { *p++ = ' '; pad--; }
                }
                break;
            }
            case 's': {
                const char* s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                int len = 0;
                if (precision >= 0) {
                    /* %.*s: 非 null 終端バッファも許容、precision まで読む */
                    while (len < precision && s[len]) len++;
                } else {
                    while (s[len]) len++;
                }
                if (!left_align) {
                    while (pad > len && p < end) { *p++ = ' '; pad--; }
                }
                int copied = 0;
                while (copied < len && p < end) { *p++ = s[copied]; copied++; }
                if (left_align) {
                    while (pad > len && p < end) { *p++ = ' '; pad--; }
                }
                break;
            }
            case 'c':
                if (p < end) *p++ = (char)va_arg(ap, int);
                break;
            case '%':
                if (p < end) *p++ = '%';
                break;
            default:
                break;
        }
        fmt++;
    }
    *p = '\0';
    return (int)(p - buf);
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
    int n = vsnprintf(buf, 4096, fmt, ap);
    va_end(ap);
    return n;
}

_STDIO_PRINTF_FN int vsprintf(char* buf, const char* fmt, va_list ap) {
    return vsnprintf(buf, 4096, fmt, ap);
}

_STDIO_PRINTF_FN int vprintf(const char* fmt, va_list ap) {
    char buf[512];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n < 0) return -1;
    if (_rin_stdio_write_all(stdout, buf, (size_t)n) != 0) return -1;
    return n;
}

_STDIO_PRINTF_FN int printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

_STDIO_PRINTF_FN int vfprintf(FILE* stream, const char* fmt, va_list ap) {
    char buf[512];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n < 0) return -1;
    if (_rin_stdio_write_all(stream, buf, (size_t)n) != 0) return -1;
    return n;
}

_STDIO_PRINTF_FN int fprintf(FILE* stream, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vfprintf(stream, fmt, ap);
    va_end(ap);
    return n;
}
#endif /* !sprintf || _STDIO_GLOBAL_IMPL */

/* POSIX formatted-allocation helper used by libc++'s locale fallback.  Keep
 * the allocation bounded and use va_copy so the caller's va_list remains
 * usable after the sizing/growth passes. */
#if !defined(vasprintf) || defined(_STDIO_GLOBAL_IMPL)
_STDIO_PRINTF_FN int vasprintf(char** result, const char* fmt, va_list ap) {
    size_t capacity = 128u;
    if (!result || !fmt) {
        errno = EINVAL;
        return -1;
    }
    *result = (char*)0;
    for (;;) {
        char* buffer = (char*)malloc(capacity);
        int required;
        va_list copy;
        if (!buffer) {
            errno = ENOMEM;
            return -1;
        }
        va_copy(copy, ap);
        required = vsnprintf(buffer, capacity, fmt, copy);
        va_end(copy);
        if (required >= 0 && (size_t)required < capacity) {
            *result = buffer;
            return required;
        }
        free(buffer);
        if (required >= 0) {
            if ((size_t)required == (size_t)-1 ||
                (size_t)required >= (size_t)-1 - 1u) {
                errno = EOVERFLOW;
                return -1;
            }
            capacity = (size_t)required + 1u;
        } else {
            if (capacity > (size_t)-1 / 2u) {
                errno = EOVERFLOW;
                return -1;
            }
            capacity *= 2u;
        }
    }
}
#endif /* !vasprintf || _STDIO_GLOBAL_IMPL */

/* ═══════════════════════════════════════════════════════════════
 * フォーマット入力
 * ═══════════════════════════════════════════════════════════════*/
#include "stdio_scanf.h"

/* perror - エラーメッセージ出力 */
static inline void perror(const char* s) {
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
static inline int remove(const char* pathname) {
    intptr_t result = _rin_stdio_syscall_result(
        _RIN_STDIO_SYSCALL1(SYS_UNLINK, (uintptr_t)pathname));
    if (result < 0) return -1;
    if (result != 0) {
        errno = EOVERFLOW;
        return -1;
    }
    return 0;
}

/* rename - ファイル名変更 */
static inline int rename(const char* oldpath, const char* newpath) {
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
static inline FILE* tmpfile(void) {
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
        (void)_RIN_STDIO_SYSCALL1(SYS_CLOSE, (uintptr_t)fd);
        errno = EMFILE;
        return NULL;
    }
    return stream;
}

/* tmpnam - 一時ファイル名を生成 */
static inline char* tmpnam(char* s) {
    static char fallback[L_tmpnam];
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

#endif /* _STDIO_H */
