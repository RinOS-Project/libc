/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc checked-operation layer.
 *
 * This is intentionally enabled only for a RinOS userspace build that opts
 * into RIN_FORTIFY_SOURCE.  Kernel code and hosted consumers keep their
 * existing contracts, while release applications get a fail-closed check
 * whenever Clang/GCC can determine a destination object's extent.
 */
#ifndef RIN_LIBC_FORTIFY_H
#define RIN_LIBC_FORTIFY_H

#include "stddef.h"

#if defined(RIN_USERSPACE) && defined(RIN_FORTIFY_SOURCE) && \
    RIN_FORTIFY_SOURCE > 0 && (defined(__clang__) || defined(__GNUC__))

#ifdef __cplusplus
extern "C" {
#endif
__attribute__((noreturn)) void __rin_fortify_fail(const char* operation);
#ifdef __cplusplus
}
#endif

#define RIN_FORTIFY_UNKNOWN_SIZE ((size_t)-1)

static inline void __rin_fortify_check(size_t capacity, size_t required,
                                       const char* operation)
{
    if (capacity != RIN_FORTIFY_UNKNOWN_SIZE && required > capacity)
        __rin_fortify_fail(operation);
}

static inline size_t __rin_fortify_bounded_length(const char* text,
                                                  size_t capacity,
                                                  const char* operation)
{
    size_t length = 0u;
    if (text == (const char*)0) __rin_fortify_fail(operation);
    if (capacity == RIN_FORTIFY_UNKNOWN_SIZE) {
        while (text[length] != '\0') ++length;
        return length;
    }
    while (length < capacity && text[length] != '\0') ++length;
    if (length == capacity) __rin_fortify_fail(operation);
    return length;
}

static inline void* __rin_fortify_memcpy(void* destination, const void* source,
                                         size_t count, size_t capacity)
{
    __rin_fortify_check(capacity, count, "memcpy");
    return rin_memory_copy_fast(destination, source, count);
}

static inline void* __rin_fortify_memmove(void* destination, const void* source,
                                          size_t count, size_t capacity)
{
    __rin_fortify_check(capacity, count, "memmove");
    return rin_memory_move_fast(destination, source, count);
}

static inline void* __rin_fortify_memset(void* destination, int value,
                                         size_t count, size_t capacity)
{
    __rin_fortify_check(capacity, count, "memset");
    return rin_memory_set_fast(destination, value, count);
}

static inline char* __rin_fortify_strcpy(char* destination, const char* source,
                                         size_t capacity)
{
    size_t length = __rin_fortify_bounded_length(
        source, RIN_FORTIFY_UNKNOWN_SIZE, "strcpy");
    size_t index;
    __rin_fortify_check(capacity, length + 1u, "strcpy");
    for (index = 0u; index <= length; ++index) destination[index] = source[index];
    return destination;
}

static inline char* __rin_fortify_strncpy(char* destination, const char* source,
                                          size_t count, size_t capacity)
{
    size_t index = 0u;
    __rin_fortify_check(capacity, count, "strncpy");
    if (source == (const char*)0) __rin_fortify_fail("strncpy");
    while (index < count && source[index] != '\0') {
        destination[index] = source[index];
        ++index;
    }
    while (index < count) destination[index++] = '\0';
    return destination;
}

static inline char* __rin_fortify_strcat(char* destination, const char* source,
                                         size_t capacity)
{
    size_t length = __rin_fortify_bounded_length(destination, capacity, "strcat");
    size_t source_length = __rin_fortify_bounded_length(
        source, RIN_FORTIFY_UNKNOWN_SIZE, "strcat");
    size_t index;
    if (source_length > (size_t)-1 - length - 1u)
        __rin_fortify_fail("strcat");
    __rin_fortify_check(capacity, length + source_length + 1u, "strcat");
    for (index = 0u; index <= source_length; ++index)
        destination[length + index] = source[index];
    return destination;
}

static inline char* __rin_fortify_strncat(char* destination, const char* source,
                                          size_t count, size_t capacity)
{
    size_t length = __rin_fortify_bounded_length(destination, capacity, "strncat");
    size_t source_length = 0u;
    size_t index;
    if (source == (const char*)0) __rin_fortify_fail("strncat");
    while (source_length < count && source[source_length] != '\0') ++source_length;
    if (source_length > (size_t)-1 - length - 1u)
        __rin_fortify_fail("strncat");
    __rin_fortify_check(capacity, length + source_length + 1u, "strncat");
    for (index = 0u; index < source_length; ++index)
        destination[length + index] = source[index];
    destination[length + source_length] = '\0';
    return destination;
}

/* Object-size builtins inspect their operand without evaluating it, so every
 * user expression below is evaluated exactly once by the helper call. */
#ifndef memcpy
#define memcpy(destination, source, count) \
    __rin_fortify_memcpy((destination), (source), (count), \
                         __builtin_object_size((destination), 0))
#endif
#ifndef memmove
#define memmove(destination, source, count) \
    __rin_fortify_memmove((destination), (source), (count), \
                          __builtin_object_size((destination), 0))
#endif
#ifndef memset
#define memset(destination, value, count) \
    __rin_fortify_memset((destination), (value), (count), \
                         __builtin_object_size((destination), 0))
#endif
#ifndef strcpy
#define strcpy(destination, source) \
    __rin_fortify_strcpy((destination), (source), \
                         __builtin_object_size((destination), 1))
#endif
#ifndef strncpy
#define strncpy(destination, source, count) \
    __rin_fortify_strncpy((destination), (source), (count), \
                          __builtin_object_size((destination), 1))
#endif
#ifndef strcat
#define strcat(destination, source) \
    __rin_fortify_strcat((destination), (source), \
                         __builtin_object_size((destination), 1))
#endif
#ifndef strncat
#define strncat(destination, source, count) \
    __rin_fortify_strncat((destination), (source), (count), \
                          __builtin_object_size((destination), 1))
#endif

#endif /* enabled RinOS userspace fortify */

#endif /* RIN_LIBC_FORTIFY_H */
