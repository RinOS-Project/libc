/* SPDX-License-Identifier: MIT */
/* Process-global narrow stdio owner for each user process runtime. */

#define RIN_STDIO_OWNER_BUILD 1
#define _STDIO_GLOBAL_IMPL 1
#include "stdio_impl.h"
#include "wchar.h"

#ifndef RIN_STDIO_MEMSTREAM_MALLOC
#define RIN_STDIO_MEMSTREAM_MALLOC(size) malloc(size)
#endif
#ifndef RIN_STDIO_MEMSTREAM_REALLOC
#define RIN_STDIO_MEMSTREAM_REALLOC(pointer, size) realloc((pointer), (size))
#endif
#ifndef RIN_STDIO_MEMSTREAM_FREE
#define RIN_STDIO_MEMSTREAM_FREE(pointer) free(pointer)
#endif

uint32_t _rin_stdio_tmpnam_next_sequence(void) {
    static volatile uint32_t sequence;
    const uint32_t limit = (uint32_t)TMP_MAX;
    uint32_t current = __atomic_load_n(&sequence, __ATOMIC_RELAXED);
    for (;;) {
        if (current >= limit) return UINT32_MAX;
        if (__atomic_compare_exchange_n(&sequence, &current, current + 1u, 0,
                                        __ATOMIC_RELAXED,
                                        __ATOMIC_RELAXED))
            return current;
    }
}

typedef struct RinStdioMemstreamOwner {
    FILE* stream;
    unsigned char* data;
    size_t capacity;
    size_t length;
    size_t position;
    int kind;
    int owns_data;
    char** byte_output;
    wchar_t** wide_output;
    size_t* size_output;
    struct RinStdioMemstreamOwner* next;
} RinStdioMemstreamOwner;

static RinStdioMemstreamOwner* _rin_stdio_memstream_owners;

static RinStdioMemstreamOwner* _rin_stdio_memstream_find(FILE* stream) {
    RinStdioMemstreamOwner* found = NULL;
    if (!stream) return NULL;
    _rin_stdio_lock_table_enter();
    for (RinStdioMemstreamOwner* owner = _rin_stdio_memstream_owners;
         owner; owner = owner->next) {
        if (owner->stream == stream) {
            found = owner;
            break;
        }
    }
    _rin_stdio_lock_table_leave();
    return found;
}

int rin_stdio_memstream_kind(FILE* stream) {
    RinStdioMemstreamOwner* owner = _rin_stdio_memstream_find(stream);
    return owner ? owner->kind : RIN_STDIO_MEMSTREAM_NONE;
}

static FILE* _rin_stdio_memstream_open(int kind, unsigned int mode,
                                        int orientation,
                                        unsigned char* data,
                                        size_t capacity, int owns_data,
                                        char** byte_output,
                                        wchar_t** wide_output,
                                        size_t* size_output) {
    RinStdioMemstreamOwner* owner;
    FILE* stream = NULL;
    owner = (RinStdioMemstreamOwner*)RIN_STDIO_MEMSTREAM_MALLOC(
        sizeof(*owner));
    if (!owner) {
        errno = ENOMEM;
        return NULL;
    }
    memset(owner, 0, sizeof(*owner));
    stream = _rin_stdio_stream_allocate(-1, mode, orientation, 0);
    if (!stream) {
        RIN_STDIO_MEMSTREAM_FREE(owner);
        return NULL;
    }
    owner->stream = stream;
    owner->data = data;
    owner->capacity = capacity;
    owner->length = 0u;
    owner->position = 0u;
    owner->kind = kind;
    owner->owns_data = owns_data;
    owner->byte_output = byte_output;
    owner->wide_output = wide_output;
    owner->size_output = size_output;
    _rin_stdio_lock_table_enter();
    owner->next = _rin_stdio_memstream_owners;
    _rin_stdio_memstream_owners = owner;
    _rin_stdio_lock_table_leave();
    return stream;
}

static int _rin_stdio_memstream_writable(const RinStdioMemstreamOwner* owner) {
    return owner && (owner->stream->mode & O_ACCMODE) != O_RDONLY;
}

static int _rin_stdio_memstream_readable(const RinStdioMemstreamOwner* owner) {
    return owner && (owner->stream->mode & O_ACCMODE) != O_WRONLY;
}

int rin_stdio_memstream_read(FILE* stream, unsigned char* output,
                             size_t capacity, intptr_t* result) {
    RinStdioMemstreamOwner* owner = _rin_stdio_memstream_find(stream);
    size_t copied;
    if (!owner || owner->kind == RIN_STDIO_MEMSTREAM_WIDE) return 0;
    if (!result || (!output && capacity != 0u)) {
        errno = EINVAL;
        if (result) *result = -1;
        return 1;
    }
    if (!_rin_stdio_memstream_readable(owner)) {
        errno = EBADF;
        stream->error = 1;
        *result = -1;
        return 1;
    }
    if (capacity == 0u || owner->position >= owner->length) {
        *result = 0;
        return 1;
    }
    copied = owner->length - owner->position;
    if (copied > capacity) copied = capacity;
    if (copied > (size_t)INTPTR_MAX) copied = (size_t)INTPTR_MAX;
    for (size_t index = 0u; index < copied; ++index)
        output[index] = owner->data[owner->position + index];
    owner->position += copied;
    *result = (intptr_t)copied;
    return 1;
}

static size_t _rin_stdio_memstream_element_size(
    const RinStdioMemstreamOwner* owner) {
    return owner->kind == RIN_STDIO_MEMSTREAM_WIDE ? sizeof(wchar_t) : 1u;
}

static int _rin_stdio_memstream_reserve(RinStdioMemstreamOwner* owner,
                                         size_t required) {
    size_t element_size = _rin_stdio_memstream_element_size(owner);
    size_t maximum = (size_t)-1 / element_size;
    size_t capacity;
    unsigned char* replacement;
    if (required > maximum) {
        errno = EOVERFLOW;
        return -1;
    }
    if (required <= owner->capacity) return 0;
    capacity = owner->capacity ? owner->capacity : 16u;
    if (capacity > maximum) capacity = maximum;
    while (capacity < required) {
        if (capacity > maximum / 2u) {
            capacity = maximum;
            break;
        }
        capacity *= 2u;
    }
    if (capacity < required) {
        errno = EOVERFLOW;
        return -1;
    }
    replacement = (unsigned char*)RIN_STDIO_MEMSTREAM_REALLOC(
        owner->data, capacity * element_size);
    if (!replacement) {
        errno = ENOMEM;
        return -1;
    }
    owner->data = replacement;
    owner->capacity = capacity;
    return 0;
}

static intptr_t _rin_stdio_memstream_write_bytes(
    RinStdioMemstreamOwner* owner, const unsigned char* input,
    size_t length) {
    size_t position;
    size_t available;
    size_t copied;
    if (!_rin_stdio_memstream_writable(owner)) {
        errno = EBADF;
        owner->stream->error = 1;
        return -1;
    }
    if (owner->kind == RIN_STDIO_MEMSTREAM_FIXED) {
        if ((owner->stream->mode & O_APPEND) != 0)
            owner->position = owner->length;
        position = owner->position;
        if (position > owner->capacity) {
            errno = EIO;
            owner->stream->error = 1;
            return -1;
        }
        available = owner->capacity - position;
        copied = length < available ? length : available;
        for (size_t index = 0u; index < copied; ++index)
            owner->data[position + index] = input[index];
        owner->position += copied;
        if (owner->position > owner->length)
            owner->length = owner->position;
        if (copied != length) {
            errno = ENOSPC;
            owner->stream->error = 1;
            if (copied == 0u) return -1;
        }
        return (intptr_t)copied;
    }
    position = owner->position;
    if (length > (size_t)INTPTR_MAX ||
        position > (size_t)-1 - length - 1u) {
        errno = EOVERFLOW;
        owner->stream->error = 1;
        return -1;
    }
    if (length == 0u) return 0;
    if (_rin_stdio_memstream_reserve(owner, position + length + 1u) != 0) {
        owner->stream->error = 1;
        return -1;
    }
    for (size_t index = owner->length; index < position; ++index)
        owner->data[index] = 0u;
    for (size_t index = 0u; index < length; ++index)
        owner->data[position + index] = input[index];
    owner->position = position + length;
    if (owner->position > owner->length) owner->length = owner->position;
    return (intptr_t)length;
}

int rin_stdio_memstream_write(FILE* stream, const unsigned char* input,
                              size_t length, intptr_t* result) {
    RinStdioMemstreamOwner* owner = _rin_stdio_memstream_find(stream);
    if (!owner || owner->kind == RIN_STDIO_MEMSTREAM_WIDE) return 0;
    if (!result || (!input && length != 0u)) {
        errno = EINVAL;
        if (result) *result = -1;
        return 1;
    }
    *result = _rin_stdio_memstream_write_bytes(owner, input, length);
    return 1;
}

int rin_stdio_memstream_read_wide(FILE* stream, uint32_t* value) {
    RinStdioMemstreamOwner* owner = _rin_stdio_memstream_find(stream);
    wchar_t* data;
    if (!owner || owner->kind != RIN_STDIO_MEMSTREAM_WIDE) return -2;
    if (!value) {
        errno = EINVAL;
        return -1;
    }
    if (!_rin_stdio_memstream_readable(owner)) {
        errno = EBADF;
        stream->error = 1;
        return -1;
    }
    if (owner->position >= owner->length) return 0;
    data = (wchar_t*)owner->data;
    *value = (uint32_t)data[owner->position++];
    return 1;
}

int rin_stdio_memstream_write_wide(FILE* stream, uint32_t value) {
    RinStdioMemstreamOwner* owner = _rin_stdio_memstream_find(stream);
    wchar_t* data;
    size_t position;
    size_t required;
    if (!owner || owner->kind != RIN_STDIO_MEMSTREAM_WIDE) return 0;
    if (!_rin_stdio_memstream_writable(owner)) {
        errno = EBADF;
        stream->error = 1;
        return -1;
    }
    if (value > 0x10ffffu || (value >= 0xd800u && value <= 0xdfffu)) {
        errno = EILSEQ;
        stream->error = 1;
        return -1;
    }
    position = owner->position;
    if (position > (size_t)-1 - 2u) {
        errno = EOVERFLOW;
        stream->error = 1;
        return -1;
    }
    required = position + 2u;
    if (_rin_stdio_memstream_reserve(owner, required) != 0) {
        stream->error = 1;
        return -1;
    }
    data = (wchar_t*)owner->data;
    for (size_t index = owner->length; index < position; ++index)
        data[index] = L'\0';
    data[position] = (wchar_t)value;
    owner->position = position + 1u;
    if (owner->position > owner->length) owner->length = owner->position;
    return 1;
}

static int _rin_stdio_memstream_target(RinStdioMemstreamOwner* owner,
                                        int64_t offset, int whence,
                                        size_t* target) {
    int64_t base;
    int64_t candidate;
    size_t element_size = _rin_stdio_memstream_element_size(owner);
    if (whence == SEEK_SET) base = 0;
    else if (whence == SEEK_CUR) {
        if (owner->position > (size_t)INT64_MAX) {
            errno = EOVERFLOW;
            return -1;
        }
        base = (int64_t)owner->position;
    } else if (whence == SEEK_END) {
        if (owner->length > (size_t)INT64_MAX) {
            errno = EOVERFLOW;
            return -1;
        }
        base = (int64_t)owner->length;
    } else {
        errno = EINVAL;
        return -1;
    }
    if (offset < 0) {
        if (offset < -base) {
            errno = EINVAL;
            return -1;
        }
        candidate = base + offset;
    } else {
        if (base > INT64_MAX - offset) {
            errno = EOVERFLOW;
            return -1;
        }
        candidate = base + offset;
    }
    if ((uint64_t)candidate > (uint64_t)((size_t)-1 / element_size)) {
        errno = EOVERFLOW;
        return -1;
    }
    if (owner->kind == RIN_STDIO_MEMSTREAM_FIXED &&
        (uint64_t)candidate > (uint64_t)owner->capacity) {
        errno = EINVAL;
        return -1;
    }
    *target = (size_t)candidate;
    return 0;
}

int rin_stdio_memstream_seek(FILE* stream, int64_t offset, int whence) {
    RinStdioMemstreamOwner* owner = _rin_stdio_memstream_find(stream);
    size_t target;
    if (!owner) return 1;
    if (_rin_stdio_memstream_target(owner, offset, whence, &target) != 0)
        return -1;
    owner->position = target;
    stream->eof = 0;
    return 0;
}

int rin_stdio_memstream_tell(FILE* stream, int64_t* position) {
    RinStdioMemstreamOwner* owner = _rin_stdio_memstream_find(stream);
    if (!owner) return 1;
    if (!position) {
        errno = EINVAL;
        return -1;
    }
    if (owner->position > (size_t)INT64_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    *position = (int64_t)owner->position;
    return 0;
}

int rin_stdio_memstream_flush(FILE* stream) {
    RinStdioMemstreamOwner* owner = _rin_stdio_memstream_find(stream);
    size_t visible;
    if (!owner) return 0;
    if (owner->kind == RIN_STDIO_MEMSTREAM_FIXED) {
        if (_rin_stdio_memstream_writable(owner) &&
            owner->position < owner->capacity)
            owner->data[owner->position] = 0u;
        return 1;
    }
    visible = owner->position < owner->length
        ? owner->position : owner->length;
    if (visible == (size_t)-1 ||
        _rin_stdio_memstream_reserve(owner, visible + 1u) != 0) {
        stream->error = 1;
        return -1;
    }
    if (owner->kind == RIN_STDIO_MEMSTREAM_WIDE)
        ((wchar_t*)owner->data)[visible] = L'\0';
    else
        owner->data[visible] = 0u;
    if (owner->byte_output) *owner->byte_output = (char*)owner->data;
    if (owner->wide_output) *owner->wide_output = (wchar_t*)owner->data;
    if (owner->size_output) *owner->size_output = visible;
    return 1;
}

int rin_stdio_memstream_close(FILE* stream, int* handled) {
    RinStdioMemstreamOwner* owner = _rin_stdio_memstream_find(stream);
    RinStdioMemstreamOwner** link;
    int result = 0;
    if (handled) *handled = owner != NULL;
    if (!owner) return 0;
    if (rin_stdio_memstream_flush(stream) < 0) result = -1;
    if (owner->kind == RIN_STDIO_MEMSTREAM_BYTES ||
        owner->kind == RIN_STDIO_MEMSTREAM_WIDE) {
        size_t visible = owner->position < owner->length
            ? owner->position : owner->length;
        if (owner->byte_output) *owner->byte_output = (char*)owner->data;
        if (owner->wide_output) *owner->wide_output = (wchar_t*)owner->data;
        if (owner->size_output) *owner->size_output = visible;
        owner->data = NULL;
    } else if (owner->owns_data && owner->data) {
        RIN_STDIO_MEMSTREAM_FREE(owner->data);
    }
    _rin_stdio_lock_table_enter();
    link = &_rin_stdio_memstream_owners;
    while (*link && *link != owner) link = &(*link)->next;
    if (*link == owner) {
        *link = owner->next;
        owner->stream = NULL;
        owner->data = NULL;
        owner->capacity = 0u;
        owner->length = 0u;
        owner->position = 0u;
        owner->kind = RIN_STDIO_MEMSTREAM_NONE;
        owner->owns_data = 0;
        owner->byte_output = NULL;
        owner->wide_output = NULL;
        owner->size_output = NULL;
    }
    _rin_stdio_lock_table_leave();
    RIN_STDIO_MEMSTREAM_FREE(owner);
    return result;
}

static int _rin_stdio_memstream_mode(const char* mode, unsigned int* flags,
                                     int* append) {
    int update = 0;
    int binary = 0;
    size_t index;
    if (!mode || !mode[0] || !flags || !append) {
        errno = EINVAL;
        return -1;
    }
    for (index = 1u; mode[index]; ++index) {
        if (mode[index] == '+') {
            if (update) { errno = EINVAL; return -1; }
            update = 1;
        } else if (mode[index] == 'b') {
            if (binary) { errno = EINVAL; return -1; }
            binary = 1;
        } else {
            errno = EINVAL;
            return -1;
        }
    }
    if (mode[0] == 'r') {
        *flags = update ? O_RDWR : O_RDONLY;
        *append = 0;
    } else if (mode[0] == 'w') {
        *flags = update ? O_RDWR : O_WRONLY;
        *append = 0;
    } else if (mode[0] == 'a') {
        *flags = (update ? O_RDWR : O_WRONLY) | O_APPEND;
        *append = 1;
    } else {
        errno = EINVAL;
        return -1;
    }
    (void)binary;
    return 0;
}

FILE* fmemopen(void* buffer, size_t size, const char* mode) {
    unsigned int flags;
    int append;
    int owns_data = 0;
    size_t length;
    unsigned char* data = (unsigned char*)buffer;
    FILE* stream;
    if (size == 0u || _rin_stdio_memstream_mode(mode, &flags, &append) != 0) {
        if (size == 0u) errno = EINVAL;
        return NULL;
    }
    if (!data) {
        data = (unsigned char*)RIN_STDIO_MEMSTREAM_MALLOC(size);
        if (!data) { errno = ENOMEM; return NULL; }
        owns_data = 1;
        for (size_t index = 0u; index < size; ++index) data[index] = 0u;
    }
    if (mode[0] == 'w') length = 0u;
    else if (append) {
        length = 0u;
        while (length < size && data[length] != 0u) ++length;
    } else length = size;
    stream = _rin_stdio_memstream_open(RIN_STDIO_MEMSTREAM_FIXED, flags, 0,
        data, size, owns_data, NULL, NULL, NULL);
    if (!stream) {
        if (owns_data) RIN_STDIO_MEMSTREAM_FREE(data);
        return NULL;
    }
    {
        RinStdioMemstreamOwner* owner = _rin_stdio_memstream_find(stream);
        owner->length = length;
        owner->position = append ? length : 0u;
    }
    if (mode[0] == 'w') data[0] = 0u;
    return stream;
}

static FILE* _rin_stdio_open_dynamic_memstream(int kind, void* output,
                                                size_t* size_output) {
    FILE* stream;
    if (!output || !size_output) {
        errno = EINVAL;
        return NULL;
    }
    stream = _rin_stdio_memstream_open(kind, O_WRONLY,
        kind == RIN_STDIO_MEMSTREAM_WIDE ? 1 : -1,
        NULL, 0u, 1, kind == RIN_STDIO_MEMSTREAM_BYTES
            ? (char**)output : NULL,
        kind == RIN_STDIO_MEMSTREAM_WIDE ? (wchar_t**)output : NULL,
        size_output);
    if (!stream) return NULL;
    if (kind == RIN_STDIO_MEMSTREAM_BYTES) *(char**)output = NULL;
    else *(wchar_t**)output = NULL;
    *size_output = 0u;
    return stream;
}

FILE* open_memstream(char** buffer, size_t* size) {
    return _rin_stdio_open_dynamic_memstream(
        RIN_STDIO_MEMSTREAM_BYTES, buffer, size);
}

FILE* open_wmemstream(wchar_t** buffer, size_t* size) {
    return _rin_stdio_open_dynamic_memstream(
        RIN_STDIO_MEMSTREAM_WIDE, buffer, size);
}

static void _rin_stdio_flush_at_exit(void) {
    (void)fflush(NULL);
}

static void _rin_stdio_register_exit_flush(void)
    __attribute__((constructor(101)));
static void _rin_stdio_register_exit_flush(void) {
    (void)atexit(_rin_stdio_flush_at_exit);
}
