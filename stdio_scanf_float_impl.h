/* SPDX-License-Identifier: MIT */
/* Shared floating-point scanf support for narrow and wide scanners. */

#ifndef _RIN_STDIO_SCANF_FLOAT_IMPL_H
#define _RIN_STDIO_SCANF_FLOAT_IMPL_H

#include "errno.h"
#include "stdarg.h"
#include "stddef.h"
#include "stdint.h"
#include "wchar.h"
#include "stdio_width_specifier.h"

#define RIN_SSCANF_MAX_INPUT 4096u

enum RinSscanfLength {
    RIN_SSCANF_LENGTH_NONE = 0,
    RIN_SSCANF_LENGTH_LONG = 1,
    RIN_SSCANF_LENGTH_LONG_LONG = 2,
    RIN_SSCANF_LENGTH_SIZE = 3,
    RIN_SSCANF_LENGTH_SHORT = 4,
    RIN_SSCANF_LENGTH_CHAR = 5,
    RIN_SSCANF_LENGTH_INTMAX = 6,
    RIN_SSCANF_LENGTH_PTRDIFF = 7,
    RIN_SSCANF_LENGTH_LONG_DOUBLE = 8,
    RIN_SSCANF_LENGTH_WIDTH = 9,
    RIN_SSCANF_LENGTH_FAST_WIDTH = 10
};

struct RinSscanfSpec {
    enum RinSscanfLength length;
    size_t width;
    unsigned int width_bits;
    char conversion;
    int has_width;
    int suppress;
    int width_fast;
    unsigned char scanset[32];
    int scanset_invert;
    const char* scanset_data;
    size_t scanset_bytes;
};

static inline int rin_sscanf_space(unsigned char value) {
    return value == ' ' || value == '\t' || value == '\n' ||
           value == '\r' || value == '\f' || value == '\v';
}

struct RinSscanfFloat {
    uint64_t bits;
    long double long_value;
};

static inline char rin_sscanf_decimal_point(void) {
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
    return rin_locale_current_decimal_point();
#else
    return '.';
#endif
}

static inline int rin_sscanf_parse_float(
        const char* input, size_t input_size, size_t* offset,
        const struct RinSscanfSpec* spec, struct RinSscanfFloat* value_out,
        int* input_failure_out) {
    char candidate[RIN_SSCANF_MAX_INPUT + 1u];
    const char* parsed_end;
    size_t current = *offset;
    size_t end = input_size;
    size_t count;
    char decimal_point = rin_sscanf_decimal_point();

    *input_failure_out = 0;
    value_out->bits = 0u;
    value_out->long_value = 0.0L;
    while (current < input_size &&
           rin_sscanf_space((unsigned char)input[current]))
        ++current;
    if (current == input_size) {
        *input_failure_out = 1;
        return 0;
    }
    if (spec->has_width && spec->width < end - current)
        end = current + spec->width;
    count = end - current;
    for (size_t index = 0u; index < count; ++index)
        candidate[index] = input[current + index];
    candidate[count] = '\0';
    if (spec->length == RIN_SSCANF_LENGTH_NONE) {
        rin_float_parse_result parsed = rin_float_parse_binary32_radix(
            candidate, decimal_point);
        if (!parsed.converted) return 0;
        if (parsed.range_direction != 0 || parsed.subnormal) {
            errno = ERANGE;
            return -1;
        }
        value_out->bits = parsed.bits;
        parsed_end = parsed.end;
    } else if (spec->length == RIN_SSCANF_LENGTH_LONG) {
        rin_float_parse_result parsed = rin_float_parse_binary64_radix(
            candidate, decimal_point);
        if (!parsed.converted) return 0;
        if (parsed.range_direction != 0 || parsed.subnormal) {
            errno = ERANGE;
            return -1;
        }
        value_out->bits = parsed.bits;
        parsed_end = parsed.end;
    } else {
#if defined(RIN_FP_HAS_BINARY80)
        rin_long_double_parse_result parsed = rin_float_parse_binary80_radix(
            candidate, decimal_point);
        if (!parsed.converted) return 0;
        if (parsed.range_direction != 0 || parsed.subnormal) {
            errno = ERANGE;
            return -1;
        }
        value_out->long_value = parsed.value;
        parsed_end = parsed.end;
#else
        rin_float_parse_result parsed = rin_float_parse_binary64_radix(
            candidate, decimal_point);
        if (!parsed.converted) return 0;
        if (parsed.range_direction != 0 || parsed.subnormal) {
            errno = ERANGE;
            return -1;
        }
        value_out->long_value = (long double)rin_float_parse_binary64_value(
            parsed.bits);
        parsed_end = parsed.end;
#endif
    }
    *offset = current + (size_t)(parsed_end - candidate);
    return 1;
}

static inline int rin_sscanf_store_float(va_list* arguments,
                                         const struct RinSscanfSpec* spec,
                                         const struct RinSscanfFloat* value) {
    if (spec->length == RIN_SSCANF_LENGTH_NONE) {
        float* output = va_arg(*arguments, float*);
        if (!output) { errno = EFAULT; return -1; }
        *output = rin_float_parse_binary32_value(value->bits);
    } else if (spec->length == RIN_SSCANF_LENGTH_LONG) {
        double* output = va_arg(*arguments, double*);
        if (!output) { errno = EFAULT; return -1; }
        *output = rin_float_parse_binary64_value(value->bits);
    } else {
        long double* output = va_arg(*arguments, long double*);
        if (!output) { errno = EFAULT; return -1; }
        *output = value->long_value;
    }
    return 0;
}

#endif /* _RIN_STDIO_SCANF_FLOAT_IMPL_H */
