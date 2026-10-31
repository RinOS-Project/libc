/* SPDX-License-Identifier: MIT */
#define RIN_DIRENT_RUNTIME_LEGACY_LAYOUT 1
#define _RIN_USE_GNU_DIRENT 1
#define _RIN_USE_GNU_STRVERSCMP 1

#include "dirent.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

typedef int (*RinDirentCompare)(const struct dirent**, const struct dirent**);
typedef int (*RinDirent64Compare)(const struct dirent64**,
                                  const struct dirent64**);

static int rin_dirent_sort(struct dirent** entries, size_t count,
                           RinDirentCompare compare)
{
    struct dirent** workspace;
    struct dirent** source;
    struct dirent** destination;
    size_t width;
    if (count < 2u) return 0;
    if (count > SIZE_MAX / sizeof(*entries)) {
        errno = EOVERFLOW;
        return -1;
    }
    workspace = (struct dirent**)malloc(count * sizeof(*entries));
    if (workspace == NULL) {
        errno = ENOMEM;
        return -1;
    }
    source = entries;
    destination = workspace;
    for (width = 1u; width < count;) {
        size_t start = 0u;
        while (start < count) {
            size_t middle = count - start > width ? start + width : count;
            size_t end = count - middle > width ? middle + width : count;
            size_t left = start;
            size_t right = middle;
            size_t output = start;
            while (left < middle && right < end) {
                const struct dirent* left_entry = source[left];
                const struct dirent* right_entry = source[right];
                if (compare(&left_entry, &right_entry) <= 0)
                    destination[output++] = source[left++];
                else
                    destination[output++] = source[right++];
            }
            while (left < middle) destination[output++] = source[left++];
            while (right < end) destination[output++] = source[right++];
            start = end;
        }
        {
            struct dirent** swap = source;
            source = destination;
            destination = swap;
        }
        if (width > count / 2u) width = count;
        else width *= 2u;
    }
    if (source != entries) memcpy(entries, source, count * sizeof(*entries));
    free(workspace);
    return 0;
}

static int rin_dirent64_sort(struct dirent64** entries, size_t count,
                             RinDirent64Compare compare)
{
    struct dirent64** workspace;
    struct dirent64** source;
    struct dirent64** destination;
    size_t width;
    if (count < 2u) return 0;
    if (count > SIZE_MAX / sizeof(*entries)) {
        errno = EOVERFLOW;
        return -1;
    }
    workspace = (struct dirent64**)malloc(count * sizeof(*entries));
    if (workspace == NULL) {
        errno = ENOMEM;
        return -1;
    }
    source = entries;
    destination = workspace;
    for (width = 1u; width < count;) {
        size_t start = 0u;
        while (start < count) {
            size_t middle = count - start > width ? start + width : count;
            size_t end = count - middle > width ? middle + width : count;
            size_t left = start;
            size_t right = middle;
            size_t output = start;
            while (left < middle && right < end) {
                const struct dirent64* left_entry = source[left];
                const struct dirent64* right_entry = source[right];
                if (compare(&left_entry, &right_entry) <= 0)
                    destination[output++] = source[left++];
                else
                    destination[output++] = source[right++];
            }
            while (left < middle) destination[output++] = source[left++];
            while (right < end) destination[output++] = source[right++];
            start = end;
        }
        {
            struct dirent64** swap = source;
            source = destination;
            destination = swap;
        }
        if (width > count / 2u) width = count;
        else width *= 2u;
    }
    if (source != entries) memcpy(entries, source, count * sizeof(*entries));
    free(workspace);
    return 0;
}

static void rin_dirent_free_entries(struct dirent** entries, size_t count)
{
    size_t index;
    for (index = 0u; index < count; ++index) free(entries[index]);
    free(entries);
}

static void rin_dirent64_free_entries(struct dirent64** entries,
                                      size_t count)
{
    size_t index;
    for (index = 0u; index < count; ++index) free(entries[index]);
    free(entries);
}

#define RIN_DEFINE_SCANDIRAT(function_name, entry_type, read_function, \
                             sort_function, free_function) \
int function_name(int dirfd, const char* directory, \
                  entry_type*** namelist, \
                  int (*selector)(const entry_type*), \
                  int (*compar)(const entry_type**, \
                                const entry_type**)) \
{ \
    DIR* stream; \
    entry_type** entries = NULL; \
    size_t count = 0u; \
    size_t capacity = 0u; \
    int saved_error = 0; \
    int fd; \
    if (namelist == NULL) { errno = EINVAL; return -1; } \
    *namelist = NULL; \
    if (directory == NULL) { errno = EFAULT; return -1; } \
    fd = openat(dirfd, directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC); \
    if (fd < 0) return -1; \
    stream = fdopendir(fd); \
    if (stream == NULL) { \
        saved_error = errno; \
        (void)close(fd); \
        errno = saved_error; \
        return -1; \
    } \
    for (;;) { \
        entry_type* entry; \
        entry_type* copy; \
        size_t name_length; \
        errno = 0; \
        entry = read_function(stream); \
        if (entry == NULL) { \
            if (errno != 0) saved_error = errno; \
            break; \
        } \
        if (selector != NULL && !selector(entry)) continue; \
        for (name_length = 0u; name_length < sizeof(entry->d_name); \
             ++name_length) \
            if (entry->d_name[name_length] == '\0') break; \
        if (name_length == sizeof(entry->d_name)) { \
            saved_error = EIO; \
            break; \
        } \
        if (count == (size_t)INT_MAX) { saved_error = EOVERFLOW; break; } \
        copy = (entry_type*)malloc(sizeof(*copy)); \
        if (copy == NULL) { saved_error = ENOMEM; break; } \
        memcpy(copy, entry, sizeof(*copy)); \
        if (count == capacity) { \
            size_t next_capacity = capacity == 0u ? 16u : capacity * 2u; \
            entry_type** grown; \
            if (next_capacity <= capacity || \
                next_capacity > SIZE_MAX / sizeof(*entries)) { \
                free(copy); saved_error = EOVERFLOW; break; \
            } \
            grown = (entry_type**)realloc( \
                entries, next_capacity * sizeof(*entries)); \
            if (grown == NULL) { free(copy); saved_error = ENOMEM; break; } \
            entries = grown; \
            capacity = next_capacity; \
        } \
        entries[count++] = copy; \
    } \
    if (closedir(stream) < 0 && saved_error == 0) saved_error = errno; \
    if (saved_error != 0) { \
        free_function(entries, count); \
        errno = saved_error; \
        return -1; \
    } \
    if (count == 0u) { \
        entries = (entry_type**)malloc(sizeof(*entries)); \
        if (entries == NULL) { errno = ENOMEM; return -1; } \
    } \
    if (compar != NULL && sort_function(entries, count, compar) < 0) { \
        saved_error = errno; \
        free_function(entries, count); \
        errno = saved_error; \
        return -1; \
    } \
    *namelist = entries; \
    return (int)count; \
}

RIN_DEFINE_SCANDIRAT(scandirat, struct dirent, readdir, rin_dirent_sort, \
                     rin_dirent_free_entries)
RIN_DEFINE_SCANDIRAT(scandirat64, struct dirent64, readdir64,
                     rin_dirent64_sort, rin_dirent64_free_entries)
#undef RIN_DEFINE_SCANDIRAT

int strverscmp(const char* left, const char* right)
{
    const unsigned char* a = (const unsigned char*)left;
    const unsigned char* b = (const unsigned char*)right;
    for (;;) {
        if (*a >= (unsigned char)'0' && *a <= (unsigned char)'9' &&
            *b >= (unsigned char)'0' && *b <= (unsigned char)'9') {
            const unsigned char* a_end = a;
            const unsigned char* b_end = b;
            size_t a_zeroes = 0u;
            size_t b_zeroes = 0u;
            size_t a_length;
            size_t b_length;
            size_t index;
            while (*a_end >= (unsigned char)'0' &&
                   *a_end <= (unsigned char)'9') ++a_end;
            while (*b_end >= (unsigned char)'0' &&
                   *b_end <= (unsigned char)'9') ++b_end;
            while (a + a_zeroes < a_end &&
                   a[a_zeroes] == (unsigned char)'0') ++a_zeroes;
            while (b + b_zeroes < b_end &&
                   b[b_zeroes] == (unsigned char)'0') ++b_zeroes;
            if (a_zeroes != b_zeroes)
                return a_zeroes > b_zeroes ? -1 : 1;
            a_length = (size_t)(a_end - a);
            b_length = (size_t)(b_end - b);
            if (a_zeroes == 0u) {
                if (a_length != b_length)
                    return a_length < b_length ? -1 : 1;
                for (index = 0u; index < a_length; ++index)
                    if (a[index] != b[index])
                        return a[index] < b[index] ? -1 : 1;
            } else if (a_length == b_length) {
                for (index = 0u; index < a_length; ++index)
                    if (a[index] != b[index])
                        return a[index] < b[index] ? -1 : 1;
            } else {
                size_t common = a_length < b_length ? a_length : b_length;
                for (index = 0u; index < common; ++index)
                    if (a[index] != b[index])
                        return a[index] < b[index] ? -1 : 1;
                /* A shorter zero-prefixed run is extended by its next byte,
                 * while the longer run is truncated to match it. */
                if (a_length < b_length)
                    return *a_end < b[common] ? -1 : 1;
                return a[common] < *b_end ? -1 : 1;
            }
            a = a_end;
            b = b_end;
            continue;
        }
        if (*a != *b) return *a < *b ? -1 : 1;
        if (*a == 0u) return 0;
        ++a;
        ++b;
    }
}

int versionsort(const struct dirent** left, const struct dirent** right)
{
    return strverscmp((*left)->d_name, (*right)->d_name);
}

int versionsort64(const struct dirent64** left,
                  const struct dirent64** right)
{
    return strverscmp((*left)->d_name, (*right)->d_name);
}
