/* SPDX-License-Identifier: MIT */
/* Shared parser and argument-promotion helpers for C23 wN/wfN formats. */

#ifndef _RIN_STDIO_WIDTH_SPECIFIER_H
#define _RIN_STDIO_WIDTH_SPECIFIER_H

#include "errno.h"
#include "limits.h"
#include "stdarg.h"
#include "stddef.h"
#include "stdint.h"
#include "wchar.h"

static inline int rin_stdio_width_is_supported(unsigned int bits) {
    return bits == 8u || bits == 16u || bits == 32u || bits == 64u;
}

static inline unsigned int rin_stdio_width_type_bits(unsigned int bits,
                                                     int fast) {
    if (!fast) return bits;
    switch (bits) {
        case 8u: return (unsigned int)(sizeof(int_fast8_t) * CHAR_BIT);
        case 16u: return (unsigned int)(sizeof(int_fast16_t) * CHAR_BIT);
        case 32u: return (unsigned int)(sizeof(int_fast32_t) * CHAR_BIT);
        case 64u: return (unsigned int)(sizeof(int_fast64_t) * CHAR_BIT);
        default: return 0u;
    }
}

static inline int rin_stdio_width_maximum(unsigned int bits, int fast,
                                          int is_signed,
                                          uint64_t* maximum_out) {
    unsigned int type_bits = rin_stdio_width_type_bits(bits, fast);
    if (!maximum_out || type_bits == 0u || type_bits > 64u) {
        errno = ENOSYS;
        return -1;
    }
    if (is_signed)
        *maximum_out = type_bits == 64u ? (uint64_t)INT64_MAX :
            (UINT64_C(1) << (type_bits - 1u)) - 1u;
    else
        *maximum_out = type_bits == 64u ? UINT64_MAX :
            (UINT64_C(1) << type_bits) - 1u;
    return 0;
}

static inline int rin_stdio_width_integer_in_range(
        unsigned int bits, int fast, int is_signed,
        uint64_t magnitude, int negative) {
    uint64_t maximum;
    if (rin_stdio_width_maximum(bits, fast, is_signed, &maximum) != 0)
        return 0;
    if ((is_signed && magnitude > maximum + (negative ? 1u : 0u)) ||
        (!is_signed && magnitude > maximum)) {
        errno = ERANGE;
        return 0;
    }
    return 1;
}

static inline int rin_stdio_store_width_integer(
        va_list* arguments, unsigned int bits, int fast, char conversion,
        uint64_t magnitude, int negative) {
    int is_signed = conversion == 'd' || conversion == 'i';
    int64_t signed_value = 0;
    uint64_t unsigned_value = negative
        ? UINT64_C(0) - magnitude : magnitude;
    if (!rin_stdio_width_integer_in_range(
            bits, fast, is_signed, magnitude, negative))
        return -1;
    if (is_signed) {
        if (negative && magnitude == UINT64_C(0x8000000000000000))
            signed_value = INT64_MIN;
        else
            signed_value = negative ? -(int64_t)magnitude :
                (int64_t)magnitude;
    }
#define RIN_STDIO_STORE_WIDTH_SIGNED(type) do { \
        type* output = va_arg(*arguments, type*); \
        if (!output) { errno = EFAULT; return -1; } \
        *output = (type)signed_value; \
        return 0; \
    } while (0)
#define RIN_STDIO_STORE_WIDTH_UNSIGNED(type) do { \
        type* output = va_arg(*arguments, type*); \
        if (!output) { errno = EFAULT; return -1; } \
        *output = (type)unsigned_value; \
        return 0; \
    } while (0)
    if (fast) {
        switch (bits) {
            case 8u:
                if (is_signed) RIN_STDIO_STORE_WIDTH_SIGNED(int_fast8_t);
                RIN_STDIO_STORE_WIDTH_UNSIGNED(uint_fast8_t);
            case 16u:
                if (is_signed) RIN_STDIO_STORE_WIDTH_SIGNED(int_fast16_t);
                RIN_STDIO_STORE_WIDTH_UNSIGNED(uint_fast16_t);
            case 32u:
                if (is_signed) RIN_STDIO_STORE_WIDTH_SIGNED(int_fast32_t);
                RIN_STDIO_STORE_WIDTH_UNSIGNED(uint_fast32_t);
            case 64u:
                if (is_signed) RIN_STDIO_STORE_WIDTH_SIGNED(int_fast64_t);
                RIN_STDIO_STORE_WIDTH_UNSIGNED(uint_fast64_t);
        }
    } else {
        switch (bits) {
            case 8u:
                if (is_signed) RIN_STDIO_STORE_WIDTH_SIGNED(int8_t);
                RIN_STDIO_STORE_WIDTH_UNSIGNED(uint8_t);
            case 16u:
                if (is_signed) RIN_STDIO_STORE_WIDTH_SIGNED(int16_t);
                RIN_STDIO_STORE_WIDTH_UNSIGNED(uint16_t);
            case 32u:
                if (is_signed) RIN_STDIO_STORE_WIDTH_SIGNED(int32_t);
                RIN_STDIO_STORE_WIDTH_UNSIGNED(uint32_t);
            case 64u:
                if (is_signed) RIN_STDIO_STORE_WIDTH_SIGNED(int64_t);
                RIN_STDIO_STORE_WIDTH_UNSIGNED(uint64_t);
        }
    }
#undef RIN_STDIO_STORE_WIDTH_SIGNED
#undef RIN_STDIO_STORE_WIDTH_UNSIGNED
    errno = ENOSYS;
    return -1;
}

static inline int rin_stdio_store_width_count(va_list* arguments,
                                              unsigned int bits, int fast,
                                              size_t count) {
    uint64_t maximum;
    if (rin_stdio_width_maximum(bits, fast, 1, &maximum) != 0) return -1;
    if ((uint64_t)count > maximum) {
        errno = ERANGE;
        return -1;
    }
#define RIN_STDIO_STORE_WIDTH_COUNT(type) do { \
        type* output = va_arg(*arguments, type*); \
        if (!output) { errno = EFAULT; return -1; } \
        *output = (type)count; \
        return 0; \
    } while (0)
    if (fast) {
        switch (bits) {
            case 8u: RIN_STDIO_STORE_WIDTH_COUNT(int_fast8_t);
            case 16u: RIN_STDIO_STORE_WIDTH_COUNT(int_fast16_t);
            case 32u: RIN_STDIO_STORE_WIDTH_COUNT(int_fast32_t);
            case 64u: RIN_STDIO_STORE_WIDTH_COUNT(int_fast64_t);
        }
    } else {
        switch (bits) {
            case 8u: RIN_STDIO_STORE_WIDTH_COUNT(int8_t);
            case 16u: RIN_STDIO_STORE_WIDTH_COUNT(int16_t);
            case 32u: RIN_STDIO_STORE_WIDTH_COUNT(int32_t);
            case 64u: RIN_STDIO_STORE_WIDTH_COUNT(int64_t);
        }
    }
#undef RIN_STDIO_STORE_WIDTH_COUNT
    errno = ENOSYS;
    return -1;
}

/* Parse the length modifier beginning at *format.  Only widths represented by
 * the public exact/least/fast integer types are enabled by this ABI. */
static inline int rin_stdio_parse_width_spec(const char** format,
                                             unsigned int* bits_out,
                                             int* fast_out) {
    const char* current;
    unsigned int bits = 0u;
    int fast = 0;
    if (!format || !*format || !bits_out || !fast_out) {
        errno = EINVAL;
        return -1;
    }
    if (**format != 'w') return 0;
    current = *format + 1;
    if (*current == 'f') {
        fast = 1;
        ++current;
    }
    if (*current < '0' || *current > '9') {
        errno = EINVAL;
        return -1;
    }
    if (*current == '0') {
        errno = EINVAL;
        return -1;
    }
    do {
        unsigned int digit = (unsigned int)(*current - '0');
        if (bits > (UINT_MAX - digit) / 10u) {
            errno = EOVERFLOW;
            return -1;
        }
        bits = bits * 10u + digit;
        ++current;
    } while (*current >= '0' && *current <= '9');
    if (!rin_stdio_width_is_supported(bits)) {
        errno = ENOSYS;
        return -1;
    }
    if (fast && (rin_stdio_width_type_bits(bits, fast) == 0u ||
                 rin_stdio_width_type_bits(bits, fast) > 64u)) {
        errno = ENOSYS;
        return -1;
    }
    *format = current;
    *bits_out = bits;
    *fast_out = fast;
    return 1;
}

static inline int rin_stdio_parse_width_spec_wide(
        const wchar_t* format, size_t* offset, size_t limit,
        unsigned int* bits_out, int* fast_out) {
    size_t current;
    unsigned int bits = 0u;
    int fast = 0;
    uint32_t value;
    if (!format || !offset || !bits_out || !fast_out) {
        errno = EINVAL;
        return -1;
    }
    current = *offset;
    if (current >= limit) {
        errno = EOVERFLOW;
        return -1;
    }
    if ((uint32_t)format[current] != (uint32_t)'w') return 0;
    ++current;
    if (current >= limit) {
        errno = EOVERFLOW;
        return -1;
    }
    if ((uint32_t)format[current] == (uint32_t)'f') {
        fast = 1;
        ++current;
        if (current >= limit) {
            errno = EOVERFLOW;
            return -1;
        }
    }
    value = (uint32_t)format[current];
    if (value < (uint32_t)'0' || value > (uint32_t)'9' ||
        value == (uint32_t)'0') {
        errno = EINVAL;
        return -1;
    }
    do {
        unsigned int digit = (unsigned int)(value - (uint32_t)'0');
        if (bits > (UINT_MAX - digit) / 10u) {
            errno = EOVERFLOW;
            return -1;
        }
        bits = bits * 10u + digit;
        ++current;
        if (current == limit) {
            errno = EOVERFLOW;
            return -1;
        }
        value = (uint32_t)format[current];
    } while (value >= (uint32_t)'0' && value <= (uint32_t)'9');
    if (!rin_stdio_width_is_supported(bits)) {
        errno = ENOSYS;
        return -1;
    }
    if (fast && (rin_stdio_width_type_bits(bits, fast) == 0u ||
                 rin_stdio_width_type_bits(bits, fast) > 64u)) {
        errno = ENOSYS;
        return -1;
    }
    *offset = current;
    *bits_out = bits;
    *fast_out = fast;
    return 1;
}

/* Variadic arguments narrower than int arrive after the integer promotions.
 * This macro returns the original integer type after fetching that promoted
 * type, which keeps wfN correct when a target's fastest type is wider than N. */
#if defined(__cplusplus) || (defined(_MSC_VER) && !defined(__clang__))
#define RIN_STDIO_VA_ARG_PROMOTED(arguments, type) \
    ((sizeof(type) < sizeof(int)) \
        ? (type)va_arg(arguments, int) : va_arg(arguments, type))
#else
#if USHRT_MAX <= INT_MAX
#define RIN_STDIO_VA_ARG_UNSIGNED_SHORT_PROMOTED(arguments, type) \
    (type)va_arg(arguments, int)
#else
#define RIN_STDIO_VA_ARG_UNSIGNED_SHORT_PROMOTED(arguments, type) \
    (type)va_arg(arguments, unsigned int)
#endif
#define RIN_STDIO_VA_ARG_PROMOTED(arguments, type) \
    _Generic((type)0, \
        signed char: (type)va_arg(arguments, int), \
        unsigned char: (type)va_arg(arguments, int), \
        short: (type)va_arg(arguments, int), \
        unsigned short: RIN_STDIO_VA_ARG_UNSIGNED_SHORT_PROMOTED( \
            arguments, type), \
        default: va_arg(arguments, type))
#endif

#endif
