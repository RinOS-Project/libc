/* SPDX-License-Identifier: MIT */
/* Bounded narrow string scanf core.  Included by stdio.h after FILE exists. */

#ifndef _RIN_STDIO_SCANF_H
#define _RIN_STDIO_SCANF_H

#include "wchar.h"

#define RIN_SSCANF_MAX_INPUT 4096u
#define RIN_SSCANF_MAX_CONVERSIONS 64u

typedef char rin_sscanf_ptrdiff_unsigned_width_must_match[
    sizeof(size_t) == sizeof(ptrdiff_t) ? 1 : -1];

enum RinSscanfLength {
    RIN_SSCANF_LENGTH_NONE = 0,
    RIN_SSCANF_LENGTH_LONG = 1,
    RIN_SSCANF_LENGTH_LONG_LONG = 2,
    RIN_SSCANF_LENGTH_SIZE = 3,
    RIN_SSCANF_LENGTH_SHORT = 4,
    RIN_SSCANF_LENGTH_CHAR = 5,
    RIN_SSCANF_LENGTH_INTMAX = 6,
    RIN_SSCANF_LENGTH_PTRDIFF = 7,
    RIN_SSCANF_LENGTH_LONG_DOUBLE = 8
};

struct RinSscanfSpec {
    enum RinSscanfLength length;
    size_t width;
    char conversion;
    int has_width;
    int suppress;
    unsigned char scanset[32];
    int scanset_invert;
    const char* scanset_data;
    size_t scanset_bytes;
};

static int rin_sscanf_space(unsigned char value) {
    return value == ' ' || value == '\t' || value == '\n' ||
           value == '\r' || value == '\f' || value == '\v';
}

static int rin_sscanf_digit(unsigned char value, unsigned int* digit_out) {
    if (value >= '0' && value <= '9') {
        *digit_out = (unsigned int)(value - '0');
        return 1;
    }
    if (value >= 'a' && value <= 'f') {
        *digit_out = (unsigned int)(value - 'a') + 10u;
        return 1;
    }
    if (value >= 'A' && value <= 'F') {
        *digit_out = (unsigned int)(value - 'A') + 10u;
        return 1;
    }
    return 0;
}

static void rin_sscanf_scanset_add(struct RinSscanfSpec* spec,
                                   unsigned char value) {
    spec->scanset[value >> 3u] |= (unsigned char)(1u << (value & 7u));
}

static int rin_sscanf_scanset_contains(const struct RinSscanfSpec* spec,
                                       unsigned char value) {
    int contained = (spec->scanset[value >> 3u] &
                     (unsigned char)(1u << (value & 7u))) != 0u;
    return spec->scanset_invert ? !contained : contained;
}

static int rin_sscanf_parse_spec(const char* format, size_t* offset,
                                 struct RinSscanfSpec* spec) {
    size_t current = *offset;
    size_t width = 0u;
    size_t scanset_start = 0u;
    int have_width = 0;
    spec->length = RIN_SSCANF_LENGTH_NONE;
    spec->width = 0u;
    spec->conversion = '\0';
    spec->has_width = 0;
    spec->suppress = 0;
    spec->scanset_invert = 0;
    spec->scanset_data = NULL;
    spec->scanset_bytes = 0u;
    for (size_t index = 0u; index < sizeof(spec->scanset); ++index)
        spec->scanset[index] = 0u;
    if (format[current] == '%') {
        spec->conversion = '%';
        *offset = current + 1u;
        return 0;
    }
    if (format[current] == '*') {
        spec->suppress = 1;
        ++current;
    }
    while (format[current] >= '0' && format[current] <= '9') {
        size_t digit = (size_t)(format[current] - '0');
        have_width = 1;
        if (width > (RIN_SSCANF_MAX_INPUT - digit) / 10u) {
            errno = EOVERFLOW;
            return -1;
        }
        width = width * 10u + digit;
        ++current;
    }
    if (have_width && width == 0u) {
        errno = EINVAL;
        return -1;
    }
    if (format[current] == 'h') {
        spec->length = RIN_SSCANF_LENGTH_SHORT;
        if (format[++current] == 'h') {
            spec->length = RIN_SSCANF_LENGTH_CHAR;
            ++current;
        }
    } else if (format[current] == 'l') {
        spec->length = RIN_SSCANF_LENGTH_LONG;
        if (format[++current] == 'l') {
            spec->length = RIN_SSCANF_LENGTH_LONG_LONG;
            ++current;
        }
    } else if (format[current] == 'z') {
        spec->length = RIN_SSCANF_LENGTH_SIZE;
        ++current;
    } else if (format[current] == 'j') {
        spec->length = RIN_SSCANF_LENGTH_INTMAX;
        ++current;
    } else if (format[current] == 't') {
        spec->length = RIN_SSCANF_LENGTH_PTRDIFF;
        ++current;
    } else if (format[current] == 'L') {
        spec->length = RIN_SSCANF_LENGTH_LONG_DOUBLE;
        ++current;
    }
    if (format[current] == '\0') {
        errno = EINVAL;
        return -1;
    }
    spec->conversion = format[current++];
    spec->width = width;
    spec->has_width = have_width;
    if (spec->conversion == 'd' || spec->conversion == 'i' ||
        spec->conversion == 'u' || spec->conversion == 'o' ||
        spec->conversion == 'x' || spec->conversion == 'X') {
        if (spec->length == RIN_SSCANF_LENGTH_LONG_DOUBLE) {
            errno = ENOSYS;
            return -1;
        }
    } else if (spec->conversion == 'p') {
        if (spec->length != RIN_SSCANF_LENGTH_NONE) {
            errno = ENOSYS;
            return -1;
        }
    } else if (spec->conversion == 'a' || spec->conversion == 'A' ||
               spec->conversion == 'e' || spec->conversion == 'E' ||
               spec->conversion == 'f' || spec->conversion == 'F' ||
               spec->conversion == 'g' || spec->conversion == 'G') {
        if (spec->length != RIN_SSCANF_LENGTH_NONE &&
            spec->length != RIN_SSCANF_LENGTH_LONG &&
            spec->length != RIN_SSCANF_LENGTH_LONG_DOUBLE) {
            errno = ENOSYS;
            return -1;
        }
    } else if (spec->conversion == 'n') {
        /* A field width on %n has no input to limit.  The standard leaves the
         * count unchanged by the width; accepting it is safe because the
         * destination width is still checked by rin_sscanf_store_count(). */
        if (spec->suppress ||
            spec->length == RIN_SSCANF_LENGTH_LONG_DOUBLE) {
            errno = ENOSYS;
            return -1;
        }
    } else if (spec->conversion == 'c') {
        if (spec->length != RIN_SSCANF_LENGTH_NONE &&
            spec->length != RIN_SSCANF_LENGTH_LONG) {
            errno = ENOSYS;
            return -1;
        }
    } else if (spec->conversion == 's') {
        /* A width is optional for %s.  The standard leaves the destination
         * buffer size to the caller; this freestanding owner still bounds
         * the amount read from a stream/string to RIN_SSCANF_MAX_INPUT so a
         * missing delimiter cannot turn into an unbounded kernel read. */
        if (spec->length != RIN_SSCANF_LENGTH_NONE &&
            spec->length != RIN_SSCANF_LENGTH_LONG) {
            errno = ENOSYS;
            return -1;
        }
    } else if (spec->conversion == '[') {
        int have_member = 0;
        int have_previous = 0;
        unsigned char previous = 0u;
        /* Like %s, a scanset may omit its width.  The conversion loop uses
         * the same bounded default as stream integer/float readers. */
        if (spec->length != RIN_SSCANF_LENGTH_NONE &&
            spec->length != RIN_SSCANF_LENGTH_LONG) {
            errno = ENOSYS;
            return -1;
        }
        if (current > RIN_SSCANF_MAX_INPUT) {
            errno = EOVERFLOW;
            return -1;
        }
        scanset_start = current;
        if (format[current] == '^') {
            spec->scanset_invert = 1;
            ++current;
        }
        if (current > RIN_SSCANF_MAX_INPUT) {
            errno = EOVERFLOW;
            return -1;
        }
        scanset_start = current;
        if (format[current] == ']') {
            rin_sscanf_scanset_add(spec, (unsigned char)format[current]);
            previous = (unsigned char)format[current++];
            have_member = 1;
            have_previous = 1;
        }
        while (current <= RIN_SSCANF_MAX_INPUT &&
               format[current] != '\0' && format[current] != ']') {
            unsigned char member = (unsigned char)format[current++];
            if (member == '-' && have_previous &&
                current <= RIN_SSCANF_MAX_INPUT &&
                format[current] != '\0' && format[current] != ']') {
                unsigned char range_end = (unsigned char)format[current++];
                unsigned int range_value;
                if (previous > range_end) {
                    /* A descending range has no portable ordering.  Keep
                     * the hyphen and endpoint literal instead of turning a
                     * valid scanset into an implementation stub. */
                    rin_sscanf_scanset_add(spec, (unsigned char)'-');
                    rin_sscanf_scanset_add(spec, range_end);
                    previous = range_end;
                    have_member = 1;
                    have_previous = 1;
                    continue;
                }
                for (range_value = (unsigned int)previous;
                     range_value <= (unsigned int)range_end; ++range_value)
                    rin_sscanf_scanset_add(spec, (unsigned char)range_value);
                previous = range_end;
                have_member = 1;
                continue;
            }
            rin_sscanf_scanset_add(spec, member);
            previous = member;
            have_member = 1;
            have_previous = 1;
        }
        if (current > RIN_SSCANF_MAX_INPUT) {
            errno = EOVERFLOW;
            return -1;
        }
        if (!have_member || format[current] != ']') {
            errno = EINVAL;
            return -1;
        }
        spec->scanset_data = format + scanset_start;
        spec->scanset_bytes = current - scanset_start;
        ++current;
    } else {
        errno = ENOSYS;
        return -1;
    }
    *offset = current;
    return 0;
}

static int rin_sscanf_validate_format(const char* format) {
    size_t index = 0u;
    size_t conversions = 0u;
    if (!format) {
        errno = EINVAL;
        return -1;
    }
    for (;;) {
        unsigned char value;
        if (index > RIN_SSCANF_MAX_INPUT) {
            errno = EOVERFLOW;
            return -1;
        }
        value = (unsigned char)format[index];
        if (value == '\0') return 0;
        if (index == RIN_SSCANF_MAX_INPUT) {
            errno = EOVERFLOW;
            return -1;
        }
        ++index;
        if (value == '%') {
            struct RinSscanfSpec spec;
            if (++conversions > RIN_SSCANF_MAX_CONVERSIONS) {
                errno = EOVERFLOW;
                return -1;
            }
            if (rin_sscanf_parse_spec(format, &index, &spec) != 0) return -1;
        }
    }
}

static int rin_sscanf_parse_integer(
        const char* input, size_t input_size, size_t* offset,
        const struct RinSscanfSpec* spec, uint64_t* magnitude_out,
        int* negative_out, int* input_failure_out) {
    size_t current = *offset;
    size_t end;
    unsigned int base;
    uint64_t magnitude = 0u;
    int negative = 0;
    int have_digit = 0;
    while (current < input_size &&
           rin_sscanf_space((unsigned char)input[current]))
        ++current;
    if (current == input_size) {
        *input_failure_out = 1;
        return 0;
    }
    end = input_size;
    if (spec->has_width && spec->width < end - current)
        end = current + spec->width;
    if (spec->conversion != 'p' && current < end &&
        (input[current] == '+' || input[current] == '-')) {
        negative = input[current] == '-';
        ++current;
    }
    base = spec->conversion == 'o' ? 8u :
           (spec->conversion == 'x' || spec->conversion == 'X' ||
            spec->conversion == 'p') ? 16u :
           (spec->conversion == 'i' ? 0u : 10u);
    if ((base == 0u || base == 16u) && current + 2u < end &&
        input[current] == '0' &&
        (input[current + 1u] == 'x' || input[current + 1u] == 'X')) {
        unsigned int prefix_digit;
        if (rin_sscanf_digit((unsigned char)input[current + 2u],
                             &prefix_digit) && prefix_digit < 16u) {
            base = 16u;
            current += 2u;
        }
    }
    if (base == 0u)
        base = current < end && input[current] == '0' ? 8u : 10u;
    while (current < end) {
        unsigned int digit;
        if (!rin_sscanf_digit((unsigned char)input[current], &digit) ||
            digit >= base)
            break;
        if (magnitude > (UINT64_MAX - digit) / base) {
            errno = ERANGE;
            return -1;
        }
        magnitude = magnitude * base + digit;
        have_digit = 1;
        ++current;
    }
    if (!have_digit) return 0;
    *offset = current;
    *magnitude_out = magnitude;
    *negative_out = negative;
    return 1;
}

static int rin_sscanf_integer_in_range(const struct RinSscanfSpec* spec,
                                       uint64_t magnitude, int negative) {
    int is_signed = spec->conversion == 'd' || spec->conversion == 'i';
    uint64_t maximum;
    if (spec->conversion == 'p') {
        if (negative || magnitude > (uint64_t)UINTPTR_MAX) {
            errno = ERANGE;
            return 0;
        }
        return 1;
    }
    if (is_signed) {
        if (spec->length == RIN_SSCANF_LENGTH_LONG_LONG)
            maximum = (uint64_t)LLONG_MAX;
        else if (spec->length == RIN_SSCANF_LENGTH_LONG)
            maximum = (uint64_t)LONG_MAX;
        else if (spec->length == RIN_SSCANF_LENGTH_SIZE)
            maximum = (uint64_t)PTRDIFF_MAX;
        else if (spec->length == RIN_SSCANF_LENGTH_SHORT)
            maximum = (uint64_t)SHRT_MAX;
        else if (spec->length == RIN_SSCANF_LENGTH_CHAR)
            maximum = (uint64_t)SCHAR_MAX;
        else if (spec->length == RIN_SSCANF_LENGTH_INTMAX)
            maximum = (uint64_t)INT64_MAX;
        else if (spec->length == RIN_SSCANF_LENGTH_PTRDIFF)
            maximum = (uint64_t)PTRDIFF_MAX;
        else
            maximum = (uint64_t)INT_MAX;
        if (magnitude > maximum + (negative ? 1u : 0u)) {
            errno = ERANGE;
            return 0;
        }
    } else {
        if (spec->length == RIN_SSCANF_LENGTH_LONG_LONG)
            maximum = (uint64_t)ULLONG_MAX;
        else if (spec->length == RIN_SSCANF_LENGTH_LONG)
            maximum = (uint64_t)ULONG_MAX;
        else if (spec->length == RIN_SSCANF_LENGTH_SIZE)
            maximum = (uint64_t)SIZE_MAX;
        else if (spec->length == RIN_SSCANF_LENGTH_SHORT)
            maximum = (uint64_t)USHRT_MAX;
        else if (spec->length == RIN_SSCANF_LENGTH_CHAR)
            maximum = (uint64_t)UCHAR_MAX;
        else if (spec->length == RIN_SSCANF_LENGTH_INTMAX)
            maximum = (uint64_t)UINT64_MAX;
        else if (spec->length == RIN_SSCANF_LENGTH_PTRDIFF)
            maximum = (uint64_t)SIZE_MAX;
        else
            maximum = (uint64_t)UINT_MAX;
        if (magnitude > maximum) {
            errno = ERANGE;
            return 0;
        }
    }
    return 1;
}

/* `va_list` is a pointer type on the Windows x64 ABI.  Receive its address so
 * consuming a conversion advances the caller's list on both pointer and
 * array-based ABI representations. */
static int rin_sscanf_store_integer(va_list* arguments,
                                    const struct RinSscanfSpec* spec,
                                    uint64_t magnitude, int negative) {
    int is_signed = spec->conversion == 'd' || spec->conversion == 'i';
    if (!rin_sscanf_integer_in_range(spec, magnitude, negative)) return -1;
    if (spec->conversion == 'p') {
        void** output = va_arg(*arguments, void**);
        if (!output) { errno = EFAULT; return -1; }
        *output = (void*)(uintptr_t)magnitude;
        return 0;
    }
    if (is_signed) {
        int64_t value;
        if (!negative) value = (int64_t)magnitude;
        else if (magnitude == UINT64_C(0x8000000000000000))
            value = INT64_MIN;
        else
            value = -(int64_t)magnitude;
        if (spec->length == RIN_SSCANF_LENGTH_LONG_LONG) {
            long long* output = va_arg(*arguments, long long*);
            if (!output) { errno = EFAULT; return -1; }
            *output = (long long)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_LONG) {
            long* output = va_arg(*arguments, long*);
            if (!output) { errno = EFAULT; return -1; }
            *output = (long)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_SIZE) {
            ptrdiff_t* output = va_arg(*arguments, ptrdiff_t*);
            if (!output) { errno = EFAULT; return -1; }
            *output = (ptrdiff_t)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_SHORT) {
            short* output = va_arg(*arguments, short*);
            if (!output) { errno = EFAULT; return -1; }
            *output = (short)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_CHAR) {
            signed char* output = va_arg(*arguments, signed char*);
            if (!output) { errno = EFAULT; return -1; }
            *output = (signed char)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_INTMAX) {
            intmax_t* output = va_arg(*arguments, intmax_t*);
            if (!output) { errno = EFAULT; return -1; }
            *output = (intmax_t)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_PTRDIFF) {
            ptrdiff_t* output = va_arg(*arguments, ptrdiff_t*);
            if (!output) { errno = EFAULT; return -1; }
            *output = (ptrdiff_t)value;
        } else {
            int* output = va_arg(*arguments, int*);
            if (!output) { errno = EFAULT; return -1; }
            *output = (int)value;
        }
    } else {
        uint64_t value = negative ? UINT64_C(0) - magnitude : magnitude;
        if (spec->length == RIN_SSCANF_LENGTH_LONG_LONG) {
            unsigned long long* output = va_arg(*arguments,
                                                unsigned long long*);
            if (!output) { errno = EFAULT; return -1; }
            *output = (unsigned long long)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_LONG) {
            unsigned long* output = va_arg(*arguments, unsigned long*);
            if (!output) { errno = EFAULT; return -1; }
            *output = (unsigned long)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_SIZE) {
            size_t* output = va_arg(*arguments, size_t*);
            if (!output) { errno = EFAULT; return -1; }
            *output = (size_t)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_SHORT) {
            unsigned short* output = va_arg(*arguments, unsigned short*);
            if (!output) { errno = EFAULT; return -1; }
            *output = (unsigned short)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_CHAR) {
            unsigned char* output = va_arg(*arguments, unsigned char*);
            if (!output) { errno = EFAULT; return -1; }
            *output = (unsigned char)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_INTMAX) {
            uintmax_t* output = va_arg(*arguments, uintmax_t*);
            if (!output) { errno = EFAULT; return -1; }
            *output = (uintmax_t)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_PTRDIFF) {
            size_t* output = va_arg(*arguments, size_t*);
            if (!output) { errno = EFAULT; return -1; }
            *output = (size_t)value;
        } else {
            unsigned int* output = va_arg(*arguments, unsigned int*);
            if (!output) { errno = EFAULT; return -1; }
            *output = (unsigned int)value;
        }
    }
    return 0;
}

struct RinSscanfFloat {
    uint64_t bits;
    long double long_value;
};

static int rin_sscanf_parse_float(
        const char* input, size_t input_size, size_t* offset,
        const struct RinSscanfSpec* spec, struct RinSscanfFloat* value_out,
        int* input_failure_out) {
    char candidate[RIN_SSCANF_MAX_INPUT + 1u];
    const char* parsed_end;
    size_t current = *offset;
    size_t end = input_size;
    size_t count;

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
        rin_float_parse_result parsed = rin_float_parse_binary32(candidate);
        if (!parsed.converted) return 0;
        if (parsed.range_direction != 0 || parsed.subnormal) {
            errno = ERANGE;
            return -1;
        }
        value_out->bits = parsed.bits;
        parsed_end = parsed.end;
    } else if (spec->length == RIN_SSCANF_LENGTH_LONG) {
        rin_float_parse_result parsed = rin_float_parse_binary64(candidate);
        if (!parsed.converted) return 0;
        if (parsed.range_direction != 0 || parsed.subnormal) {
            errno = ERANGE;
            return -1;
        }
        value_out->bits = parsed.bits;
        parsed_end = parsed.end;
    } else {
#if defined(RIN_FP_HAS_BINARY80)
        rin_long_double_parse_result parsed = rin_float_parse_binary80(candidate);
        if (!parsed.converted) return 0;
        if (parsed.range_direction != 0 || parsed.subnormal) {
            errno = ERANGE;
            return -1;
        }
        value_out->long_value = parsed.value;
        parsed_end = parsed.end;
#else
        rin_float_parse_result parsed = rin_float_parse_binary64(candidate);
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

static int rin_sscanf_store_float(va_list* arguments,
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

/* Keep the `%n` destination-range check in a common unsigned width.  On
 * i686, comparing a 32-bit `size_t` directly with a 64-bit signed maximum is
 * statically known to be false and turns a valid freestanding translation
 * unit into a -Werror failure. */
static int rin_sscanf_count_exceeds_signed_maximum(size_t count,
                                                   uint64_t maximum) {
    return (uint64_t)count > maximum;
}

static int rin_sscanf_store_count(va_list* arguments,
                                  const struct RinSscanfSpec* spec,
                                  size_t count) {
    if (spec->length == RIN_SSCANF_LENGTH_LONG_LONG) {
        long long* output;
        if (rin_sscanf_count_exceeds_signed_maximum(count,
                                                    (uint64_t)LLONG_MAX)) {
            errno = ERANGE;
            return -1;
        }
        output = va_arg(*arguments, long long*);
        if (!output) { errno = EFAULT; return -1; }
        *output = (long long)count;
    } else if (spec->length == RIN_SSCANF_LENGTH_LONG) {
        long* output;
        if ((uint64_t)count > (uint64_t)LONG_MAX) {
            errno = ERANGE;
            return -1;
        }
        output = va_arg(*arguments, long*);
        if (!output) { errno = EFAULT; return -1; }
        *output = (long)count;
    } else if (spec->length == RIN_SSCANF_LENGTH_SIZE) {
        ptrdiff_t* output;
        if ((uint64_t)count > (uint64_t)PTRDIFF_MAX) {
            errno = ERANGE;
            return -1;
        }
        output = va_arg(*arguments, ptrdiff_t*);
        if (!output) { errno = EFAULT; return -1; }
        *output = (ptrdiff_t)count;
    } else if (spec->length == RIN_SSCANF_LENGTH_SHORT) {
        short* output;
        if ((uint64_t)count > (uint64_t)SHRT_MAX) {
            errno = ERANGE;
            return -1;
        }
        output = va_arg(*arguments, short*);
        if (!output) { errno = EFAULT; return -1; }
        *output = (short)count;
    } else if (spec->length == RIN_SSCANF_LENGTH_CHAR) {
        signed char* output;
        if ((uint64_t)count > (uint64_t)SCHAR_MAX) {
            errno = ERANGE;
            return -1;
        }
        output = va_arg(*arguments, signed char*);
        if (!output) { errno = EFAULT; return -1; }
        *output = (signed char)count;
    } else if (spec->length == RIN_SSCANF_LENGTH_INTMAX) {
        intmax_t* output;
        if (rin_sscanf_count_exceeds_signed_maximum(count,
                                                    (uint64_t)INT64_MAX)) {
            errno = ERANGE;
            return -1;
        }
        output = va_arg(*arguments, intmax_t*);
        if (!output) { errno = EFAULT; return -1; }
        *output = (intmax_t)count;
    } else if (spec->length == RIN_SSCANF_LENGTH_PTRDIFF) {
        ptrdiff_t* output;
        if ((uint64_t)count > (uint64_t)PTRDIFF_MAX) {
            errno = ERANGE;
            return -1;
        }
        output = va_arg(*arguments, ptrdiff_t*);
        if (!output) { errno = EFAULT; return -1; }
        *output = (ptrdiff_t)count;
    } else {
        int* output;
        if ((uint64_t)count > (uint64_t)INT_MAX) {
            errno = ERANGE;
            return -1;
        }
        output = va_arg(*arguments, int*);
        if (!output) { errno = EFAULT; return -1; }
        *output = (int)count;
    }
    return 0;
}

/* Decode a narrow input item into a private wide candidate before touching
 * the caller's destination.  The narrow scanf contract is UTF-8 in RinOS;
 * accepting only complete scalar sequences avoids byte-casting malformed
 * input into a wchar_t and keeps `%lc`/`%ls` failure-atomic. */
static int rin_sscanf_decode_wide_candidate(
        const char* input, size_t input_size, wchar_t* output,
        size_t output_capacity, size_t* output_count) {
    rin_unicode_mbstate_t state;
    size_t input_offset = 0u;
    size_t count = 0u;
    if (!input || !output || !output_count || output_capacity == 0u) {
        errno = EINVAL;
        return -1;
    }
    state.state = 0u;
    state.codepoint = 0u;
    while (input_offset < input_size) {
        uint32_t value = 0u;
        size_t consumed = rin_unicode_mbrtowc32(
            &value, input + input_offset, input_size - input_offset, &state);
        if (consumed == (size_t)-1 || consumed == (size_t)-2) {
            errno = EILSEQ;
            return -1;
        }
        if (consumed == 0u) consumed = 1u;
        if (consumed > input_size - input_offset || count >= output_capacity) {
            errno = EOVERFLOW;
            return -1;
        }
        output[count++] = (wchar_t)value;
        input_offset += consumed;
        state.state = 0u;
        state.codepoint = 0u;
    }
    *output_count = count;
    return 0;
}

static int rin_sscanf_utf8_first(const char* input, size_t input_size,
                                 size_t* consumed_out, uint32_t* value_out) {
    rin_unicode_mbstate_t state;
    size_t probe;
    if (!input || !consumed_out || !value_out || input_size == 0u) return 0;
    for (probe = 1u; probe <= input_size && probe <= 4u; ++probe) {
        size_t consumed;
        uint32_t value = 0u;
        state.state = 0u;
        state.codepoint = 0u;
        consumed = rin_unicode_mbrtowc32(&value, input, probe, &state);
        if (consumed == (size_t)-1) {
            errno = EILSEQ;
            return -1;
        }
        if (consumed == (size_t)-2) continue;
        if (consumed == 0u) consumed = 1u;
        *consumed_out = consumed;
        *value_out = value;
        return 1;
    }
    errno = EILSEQ;
    return -1;
}

/* Match a wide scanset against Unicode scalars from its UTF-8 format source.
 * The narrow byte bitset remains the fast path for ordinary `%[`; `%l[` uses
 * this parser so a multibyte literal or range is never compared bytewise. */
static int rin_sscanf_wide_scanset_contains(
        const struct RinSscanfSpec* spec, uint32_t value) {
    size_t offset = 0u;
    uint32_t previous = 0u;
    int have_previous = 0;
    int contained = 0;
    if (!spec || !spec->scanset_data || spec->scanset_bytes == 0u) {
        errno = EINVAL;
        return -1;
    }
    while (offset < spec->scanset_bytes) {
        size_t consumed;
        uint32_t member;
        int parsed = rin_sscanf_utf8_first(
            spec->scanset_data + offset, spec->scanset_bytes - offset,
            &consumed, &member);
        if (parsed <= 0) return -1;
        offset += consumed;
        if (member == (uint32_t)'-' && have_previous &&
            offset < spec->scanset_bytes) {
            uint32_t range_end;
            parsed = rin_sscanf_utf8_first(
                spec->scanset_data + offset,
                spec->scanset_bytes - offset, &consumed, &range_end);
            if (parsed <= 0) return -1;
            offset += consumed;
            if (previous <= range_end) {
                if (value >= previous && value <= range_end)
                    contained = 1;
            } else if (value == (uint32_t)'-' || value == range_end) {
                /* Keep descending ranges literal, matching the narrow
                 * scanset policy instead of inventing an ordering. */
                contained = 1;
            }
            previous = range_end;
            have_previous = 1;
            continue;
        }
        if (value == member) contained = 1;
        previous = member;
        have_previous = 1;
    }
    return spec->scanset_invert ? !contained : contained;
}

/* Collect complete UTF-8 scalars from a string-backed scanf input.  Width is
 * a scalar count for `%lc`/`%ls`, while the private byte candidate remains
 * bounded by the existing input limit. */
static int rin_sscanf_wide_field(
        const char* input, size_t input_size, size_t* input_offset,
        const struct RinSscanfSpec* spec, char* candidate,
        size_t candidate_capacity, size_t* byte_count_out,
        size_t* scalar_count_out, int* input_failure_out) {
    size_t current = *input_offset;
    size_t byte_count = 0u;
    size_t scalar_count = 0u;
    size_t scalar_limit = spec->has_width ? spec->width :
        (spec->conversion == 'c' ? 1u : RIN_SSCANF_MAX_INPUT);
    int first_result;
    if (!input || !input_offset || !spec || !candidate ||
        !byte_count_out || !scalar_count_out || !input_failure_out ||
        scalar_limit == 0u) {
        errno = EINVAL;
        return -1;
    }
    *input_failure_out = 0;
    if (spec->conversion == 's') {
        while (current < input_size &&
               rin_sscanf_space((unsigned char)input[current])) ++current;
    }
    while (current < input_size && scalar_count < scalar_limit) {
        size_t consumed;
        uint32_t value;
        first_result = rin_sscanf_utf8_first(
            input + current, input_size - current, &consumed, &value);
        if (first_result <= 0) return -1;
        if (spec->conversion == 's' && rin_sscanf_space(value)) break;
        if (spec->conversion == '[') {
            int matched = rin_sscanf_wide_scanset_contains(spec, value);
            if (matched < 0) return -1;
            if (matched == 0) break;
        }
        if (byte_count > candidate_capacity - consumed) {
            errno = EOVERFLOW;
            return -1;
        }
        for (size_t index = 0u; index < consumed; ++index)
            candidate[byte_count + index] = input[current + index];
        byte_count += consumed;
        current += consumed;
        ++scalar_count;
    }
    if (scalar_count == 0u) {
        if (current == input_size) *input_failure_out = 1;
        return 0;
    }
    *input_offset = current;
    *byte_count_out = byte_count;
    *scalar_count_out = scalar_count;
    return 1;
}

static int rin_sscanf_store_wide_field(
        va_list* arguments, const struct RinSscanfSpec* spec,
        const char* candidate, size_t byte_count, int suppress) {
    wchar_t wide_candidate[RIN_SSCANF_MAX_INPUT + 1u];
    size_t wide_count = 0u;
    if (rin_sscanf_decode_wide_candidate(
            candidate, byte_count, wide_candidate,
            sizeof(wide_candidate) / sizeof(wide_candidate[0]),
            &wide_count) != 0) return -1;
    if (suppress) return 0;
    {
        wchar_t* output = va_arg(*arguments, wchar_t*);
        if (!output) {
            errno = EFAULT;
            return -1;
        }
        for (size_t index = 0u; index < wide_count; ++index)
            output[index] = wide_candidate[index];
        if (spec->conversion == 's' || spec->conversion == '[')
            output[wide_count] = L'\0';
    }
    return 0;
}

static int rin_sscanf_input_error_result(int assignments) {
    return assignments == 0 ? EOF : assignments;
}

/* vsscanf - bounded string input.  Format validation happens before any
 * variadic argument is read or caller output is written. */
static inline int vsscanf(const char* string, const char* format, va_list ap) {
    char input[RIN_SSCANF_MAX_INPUT + 1u];
    size_t input_size = 0u;
    size_t input_offset = 0u;
    size_t format_offset = 0u;
    int assignments = 0;
    va_list arguments;
    if (!string || !format) {
        errno = EINVAL;
        return EOF;
    }
    if (rin_sscanf_validate_format(format) != 0) return EOF;
    while (input_size <= RIN_SSCANF_MAX_INPUT && string[input_size] != '\0')
        ++input_size;
    if (input_size > RIN_SSCANF_MAX_INPUT) {
        errno = EOVERFLOW;
        return EOF;
    }
    for (size_t index = 0u; index <= input_size; ++index)
        input[index] = string[index];
    va_copy(arguments, ap);
    while (format[format_offset] != '\0') {
        unsigned char format_value = (unsigned char)format[format_offset];
        if (rin_sscanf_space(format_value)) {
            while (rin_sscanf_space((unsigned char)format[format_offset]))
                ++format_offset;
            while (input_offset < input_size &&
                   rin_sscanf_space((unsigned char)input[input_offset]))
                ++input_offset;
            continue;
        }
        if (format_value != '%') {
            if (input_offset == input_size) {
                va_end(arguments);
                return rin_sscanf_input_error_result(assignments);
            }
            if ((unsigned char)input[input_offset] != format_value) {
                va_end(arguments);
                return assignments;
            }
            ++input_offset;
            ++format_offset;
            continue;
        }
        {
            struct RinSscanfSpec spec;
            ++format_offset;
            if (rin_sscanf_parse_spec(format, &format_offset, &spec) != 0) {
                va_end(arguments);
                return rin_sscanf_input_error_result(assignments);
            }
            if (spec.conversion == '%') {
                if (input_offset == input_size) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (input[input_offset] != '%') {
                    va_end(arguments);
                    return assignments;
                }
                ++input_offset;
                continue;
            }
            if (spec.conversion == 'n') {
                if (rin_sscanf_store_count(&arguments, &spec,
                                           input_offset) != 0) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                continue;
            }
            if (spec.length == RIN_SSCANF_LENGTH_LONG &&
                (spec.conversion == 'c' || spec.conversion == 's' ||
                 spec.conversion == '[')) {
                char candidate[RIN_SSCANF_MAX_INPUT + 1u];
                size_t byte_count = 0u;
                size_t scalar_count = 0u;
                int input_failure = 0;
                int parsed = rin_sscanf_wide_field(
                    input, input_size, &input_offset, &spec, candidate,
                    sizeof(candidate), &byte_count, &scalar_count,
                    &input_failure);
                (void)scalar_count;
                if (parsed < 0) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (parsed == 0) {
                    va_end(arguments);
                    return input_failure ?
                        rin_sscanf_input_error_result(assignments) : assignments;
                }
                if (rin_sscanf_store_wide_field(
                        &arguments, &spec, candidate, byte_count,
                        spec.suppress) != 0) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (!spec.suppress) ++assignments;
                continue;
            }
            if (spec.conversion == 'd' || spec.conversion == 'i' ||
                spec.conversion == 'u' || spec.conversion == 'o' ||
                spec.conversion == 'x' || spec.conversion == 'X' ||
                spec.conversion == 'p') {
                uint64_t magnitude;
                int negative;
                int input_failure = 0;
                int parsed = rin_sscanf_parse_integer(
                    input, input_size, &input_offset, &spec, &magnitude,
                    &negative, &input_failure);
                if (parsed < 0 || (parsed > 0 &&
                    !rin_sscanf_integer_in_range(&spec, magnitude, negative))) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (parsed == 0) {
                    va_end(arguments);
                    return input_failure ?
                        rin_sscanf_input_error_result(assignments) : assignments;
                }
                if (!spec.suppress) {
                    if (rin_sscanf_store_integer(&arguments, &spec, magnitude,
                                                 negative) != 0) {
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    ++assignments;
                }
                continue;
            }
            if (spec.conversion == 'a' || spec.conversion == 'A' ||
                spec.conversion == 'e' || spec.conversion == 'E' ||
                spec.conversion == 'f' || spec.conversion == 'F' ||
                spec.conversion == 'g' || spec.conversion == 'G') {
                struct RinSscanfFloat value;
                int input_failure = 0;
                int parsed = rin_sscanf_parse_float(
                    input, input_size, &input_offset, &spec, &value,
                    &input_failure);
                if (parsed < 0) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (parsed == 0) {
                    va_end(arguments);
                    return input_failure ?
                        rin_sscanf_input_error_result(assignments) : assignments;
                }
                if (!spec.suppress) {
                    if (rin_sscanf_store_float(&arguments, &spec, &value) != 0) {
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    ++assignments;
                }
                continue;
            }
            if (spec.conversion == '[') {
                size_t begin = input_offset;
                size_t count = 0u;
                size_t limit = spec.has_width ? spec.width : RIN_SSCANF_MAX_INPUT;
                while (input_offset < input_size && count < limit &&
                       rin_sscanf_scanset_contains(
                           &spec, (unsigned char)input[input_offset])) {
                    ++input_offset;
                    ++count;
                }
                if (count == 0u) {
                    va_end(arguments);
                    return input_offset == input_size ?
                        rin_sscanf_input_error_result(assignments) : assignments;
                }
                if (!spec.suppress) {
                    char* output = va_arg(arguments, char*);
                    if (!output) {
                        errno = EFAULT;
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    for (size_t index = 0u; index < count; ++index)
                        output[index] = input[begin + index];
                    output[count] = '\0';
                    ++assignments;
                }
                continue;
            }
            if (spec.conversion == 'c') {
                size_t count = spec.has_width ? spec.width : 1u;
                if (count > input_size - input_offset) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (!spec.suppress) {
                    char* output = va_arg(arguments, char*);
                    if (!output) {
                        errno = EFAULT;
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    for (size_t index = 0u; index < count; ++index)
                        output[index] = input[input_offset + index];
                    ++assignments;
                }
                input_offset += count;
                continue;
            }
            {
                size_t begin;
                size_t count = 0u;
                while (input_offset < input_size &&
                       rin_sscanf_space((unsigned char)input[input_offset]))
                    ++input_offset;
                if (input_offset == input_size) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                begin = input_offset;
                size_t limit = spec.has_width ? spec.width : RIN_SSCANF_MAX_INPUT;
                while (input_offset < input_size && count < limit &&
                       !rin_sscanf_space((unsigned char)input[input_offset])) {
                    ++input_offset;
                    ++count;
                }
                if (count == 0u) {
                    va_end(arguments);
                    return assignments;
                }
                if (!spec.suppress) {
                    char* output = va_arg(arguments, char*);
                    if (!output) {
                        errno = EFAULT;
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    for (size_t index = 0u; index < count; ++index)
                        output[index] = input[begin + index];
                    output[count] = '\0';
                    ++assignments;
                }
            }
        }
    }
    va_end(arguments);
    return assignments;
}

static inline int sscanf(const char* string, const char* format, ...) {
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = vsscanf(string, format, arguments);
    va_end(arguments);
    return result;
}

struct RinFscanfStreamInput {
    FILE* stream;
    size_t consumed;
    int finished;
    int terminal;
};

static int rin_fscanf_stream_read(struct RinFscanfStreamInput* input,
                                  unsigned char* value_out) {
    int value;
    if (input->finished) return input->terminal;
    value = fgetc(input->stream);
    if (value == EOF) {
        input->terminal = input->stream->error ? -1 : 0;
        input->finished = 1;
        return input->terminal;
    }
    if (input->consumed == SIZE_MAX) {
        errno = EOVERFLOW;
        input->stream->error = 1;
        input->terminal = -1;
        input->finished = 1;
        return -1;
    }
    ++input->consumed;
    *value_out = (unsigned char)value;
    return 1;
}

static int rin_fscanf_stream_unread(struct RinFscanfStreamInput* input,
                                    unsigned char value) {
    if (input->consumed == 0u) {
        errno = EIO;
        input->stream->error = 1;
        input->terminal = -1;
        input->finished = 1;
        return -1;
    }
    if (ungetc((int)value, input->stream) == EOF) {
        if (errno == 0) errno = EIO;
        input->stream->error = 1;
        input->terminal = -1;
        input->finished = 1;
        return -1;
    }
    --input->consumed;
    input->finished = 0;
    input->terminal = 0;
    return 0;
}

static int rin_fscanf_stream_skip_space(struct RinFscanfStreamInput* input) {
    unsigned char value = '\0';
    int status;
    for (;;) {
        status = rin_fscanf_stream_read(input, &value);
        if (status <= 0) return status;
        if (!rin_sscanf_space(value))
            return rin_fscanf_stream_unread(input, value) == 0 ? 1 : -1;
    }
}

/* Byte-oriented FILE input needs a small UTF-8 collector for the `l` string
 * and character conversions.  It reads only complete scalar sequences and
 * puts back the delimiter that terminates `%ls`; an incomplete sequence is a
 * conversion error rather than a truncated wide character. */
static int rin_fscanf_stream_wide_field(
        struct RinFscanfStreamInput* input,
        const struct RinSscanfSpec* spec, char* candidate,
        size_t candidate_capacity, size_t* byte_count_out,
        size_t* scalar_count_out, int* input_failure_out) {
    size_t byte_count = 0u;
    size_t scalar_count = 0u;
    size_t scalar_limit = spec->has_width ? spec->width :
        (spec->conversion == 'c' ? 1u : RIN_SSCANF_MAX_INPUT);
    int status;
    if (!input || !spec || !candidate || !byte_count_out ||
        !scalar_count_out || !input_failure_out || scalar_limit == 0u) {
        errno = EINVAL;
        return -1;
    }
    *input_failure_out = 0;
    if (spec->conversion == 's') {
        status = rin_fscanf_stream_skip_space(input);
        if (status <= 0) {
            if (status == 0) *input_failure_out = 1;
            return status < 0 ? -1 : 0;
        }
    }
    while (scalar_count < scalar_limit) {
        unsigned char first = 0u;
        status = rin_fscanf_stream_read(input, &first);
        if (status <= 0) {
            if (status == 0) *input_failure_out = 1;
            if ((spec->conversion == 's' || spec->conversion == '[') &&
                scalar_count != 0u) break;
            return status < 0 ? -1 : 0;
        }
        if (spec->conversion == 's' && rin_sscanf_space(first)) {
            if (rin_fscanf_stream_unread(input, first) != 0) return -1;
            break;
        }
        {
            rin_unicode_mbstate_t state;
            uint32_t value = 0u;
            char byte = (char)first;
            char scalar_bytes[4];
            size_t scalar_begin = byte_count;
            size_t scalar_byte_count = 0u;
            size_t conversion;
            state.state = 0u;
            state.codepoint = 0u;
            for (;;) {
                if (byte_count >= candidate_capacity) {
                    errno = EOVERFLOW;
                    return -1;
                }
                if (scalar_byte_count >= sizeof(scalar_bytes)) {
                    errno = EILSEQ;
                    return -1;
                }
                scalar_bytes[scalar_byte_count++] = byte;
                candidate[byte_count++] = byte;
                conversion = rin_unicode_mbrtowc32(
                    &value, &byte, 1u, &state);
                if (conversion == (size_t)-1) {
                    errno = EILSEQ;
                    return -1;
                }
                if (conversion != (size_t)-2) break;
                status = rin_fscanf_stream_read(input, &first);
                if (status <= 0) {
                    errno = status == 0 ? EILSEQ : errno;
                    return -1;
                }
                byte = (char)first;
            }
            if (spec->conversion == '[') {
                int matched = rin_sscanf_wide_scanset_contains(spec, value);
                if (matched < 0) return -1;
                if (matched == 0) {
                    while (scalar_byte_count != 0u) {
                        --scalar_byte_count;
                        if (rin_fscanf_stream_unread(
                                input, (unsigned char)scalar_bytes[
                                    scalar_byte_count]) != 0)
                            return -1;
                    }
                    byte_count = scalar_begin;
                    break;
                }
            }
            ++scalar_count;
        }
    }
    if (scalar_count == 0u) return 0;
    if (spec->conversion == 'c' && scalar_count < scalar_limit) {
        *input_failure_out = 1;
        return 0;
    }
    *byte_count_out = byte_count;
    *scalar_count_out = scalar_count;
    return 1;
}

static int rin_fscanf_stream_try_hex_prefix(
        struct RinFscanfStreamInput* input, size_t used, size_t limit,
        unsigned char* value, int* prefixed_out) {
    unsigned char prefix;
    unsigned char suffix;
    unsigned int digit;
    int status;
    *prefixed_out = 0;
    if (*value != '0' || limit - used < 3u) return 0;
    status = rin_fscanf_stream_read(input, &prefix);
    if (status < 0) return -1;
    if (status == 0) return 0;
    if (prefix != 'x' && prefix != 'X')
        return rin_fscanf_stream_unread(input, prefix);
    status = rin_fscanf_stream_read(input, &suffix);
    if (status > 0 && rin_sscanf_digit(suffix, &digit) && digit < 16u) {
        *value = suffix;
        *prefixed_out = 1;
        return 0;
    }
    if (status < 0) return -1;
    if (status > 0 && rin_fscanf_stream_unread(input, suffix) != 0)
        return -1;
    return rin_fscanf_stream_unread(input, prefix);
}

static int rin_fscanf_stream_parse_integer(
        struct RinFscanfStreamInput* input,
        const struct RinSscanfSpec* spec, uint64_t* magnitude_out,
        int* negative_out, int* input_failure_out, unsigned int base) {
    unsigned char value = '\0';
    size_t used = 0u;
    size_t limit = spec->has_width ? spec->width : RIN_SSCANF_MAX_INPUT;
    uint64_t magnitude = 0u;
    int negative = 0;
    int prefixed = 0;
    int status = rin_fscanf_stream_skip_space(input);

    *input_failure_out = 0;
    if (status <= 0) {
        if (status == 0) *input_failure_out = 1;
        return status < 0 ? -1 : 0;
    }
    status = rin_fscanf_stream_read(input, &value);
    if (status <= 0) {
        if (status == 0) *input_failure_out = 1;
        return status < 0 ? -1 : 0;
    }
    if (value == '+' || value == '-') {
        if (spec->conversion == 'p') {
            if (rin_fscanf_stream_unread(input, value) != 0) return -1;
            return 0;
        }
        negative = value == '-';
        ++used;
        if (used == limit) return 0;
        status = rin_fscanf_stream_read(input, &value);
        if (status <= 0) {
            if (status == 0) *input_failure_out = 1;
            return status < 0 ? -1 : 0;
        }
    }
    if ((base == 0u || base == 16u) &&
        rin_fscanf_stream_try_hex_prefix(input, used, limit, &value,
                                         &prefixed) != 0)
        return -1;
    if (prefixed) {
        base = 16u;
        used += 2u;
    } else if (base == 0u) {
        base = value == '0' ? 8u : 10u;
    }
    {
        unsigned int digit;
        if (!rin_sscanf_digit(value, &digit) || digit >= base) {
            if (rin_fscanf_stream_unread(input, value) != 0) return -1;
            return 0;
        }
    }
    for (;;) {
        unsigned int digit;
        if (!rin_sscanf_digit(value, &digit) || digit >= base) {
            if (rin_fscanf_stream_unread(input, value) != 0) return -1;
            break;
        }
        if (magnitude > (UINT64_MAX - (uint64_t)digit) /
                            (uint64_t)base) {
            if (rin_fscanf_stream_unread(input, value) != 0) return -1;
            errno = ERANGE;
            return -1;
        }
        magnitude = magnitude * (uint64_t)base + (uint64_t)digit;
        ++used;
        if (used == limit) break;
        status = rin_fscanf_stream_read(input, &value);
        if (status < 0) return -1;
        if (status == 0) break;
    }
    *magnitude_out = magnitude;
    *negative_out = negative;
    return 1;
}

static int rin_fscanf_stream_unread_float_tail(
        struct RinFscanfStreamInput* input, const unsigned char* values,
        size_t count) {
    while (count != 0u) {
        --count;
        if (rin_fscanf_stream_unread(input, values[count]) != 0) return -1;
    }
    return 0;
}

static int rin_fscanf_stream_case_equal(unsigned char value, char lower) {
    return value == (unsigned char)lower ||
           value == (unsigned char)(lower - ('a' - 'A'));
}

static int rin_fscanf_stream_nan_payload_char(unsigned char value) {
    return (value >= '0' && value <= '9') ||
           (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z') || value == '_';
}

static int rin_fscanf_stream_parse_float_candidate(
        char* candidate, size_t count, const struct RinSscanfSpec* spec,
        struct RinSscanfFloat* value_out, int* input_failure_out) {
    size_t candidate_offset = 0u;
    int parser_input_failure = 0;
    int parsed;

    *input_failure_out = 0;
    candidate[count] = '\0';
    parsed = rin_sscanf_parse_float(candidate, count, &candidate_offset, spec,
                                    value_out, &parser_input_failure);
    if (parsed == 0 && parser_input_failure) *input_failure_out = 1;
    if (parsed > 0 && candidate_offset != count) {
        errno = EIO;
        return -1;
    }
    return parsed;
}

/* `inf` is a complete conversion, while an optional incomplete `inity`
 * suffix belongs back in the cursor.  A malformed base word remains a
 * matching failure with its bounded candidate restored. */
static int rin_fscanf_stream_parse_special_float(
        struct RinFscanfStreamInput* input,
        const struct RinSscanfSpec* spec, char* candidate, size_t count,
        unsigned char first, struct RinSscanfFloat* value_out,
        int* input_failure_out) {
    unsigned char tail[5];
    const char* base_suffix =
        rin_fscanf_stream_case_equal(first, 'i') ? "nf" : "an";
    size_t limit = spec->has_width ? spec->width : RIN_SSCANF_MAX_INPUT;
    size_t suffix_index;
    size_t tail_count;
    int status;
    unsigned char value = '\0';

    *input_failure_out = 0;
    candidate[count++] = (char)first;
    for (suffix_index = 0u; base_suffix[suffix_index] != '\0';
         ++suffix_index) {
        if (count == limit) {
            for (tail_count = 0u; tail_count < count; ++tail_count)
                tail[tail_count] = (unsigned char)candidate[tail_count];
            if (rin_fscanf_stream_unread_float_tail(input, tail, count) != 0)
                return -1;
            return 0;
        }
        status = rin_fscanf_stream_read(input, &value);
        if (status < 0) return -1;
        if (status == 0) {
            *input_failure_out = 1;
            return 0;
        }
        if (!rin_fscanf_stream_case_equal(value, base_suffix[suffix_index])) {
            for (tail_count = 0u; tail_count < count; ++tail_count)
                tail[tail_count] = (unsigned char)candidate[tail_count];
            tail[count] = value;
            if (rin_fscanf_stream_unread_float_tail(input, tail, count + 1u)
                != 0)
                return -1;
            return 0;
        }
        candidate[count++] = (char)value;
    }

    if (rin_fscanf_stream_case_equal(first, 'n') && count < limit) {
        status = rin_fscanf_stream_read(input, &value);
        if (status < 0) return -1;
        if (status > 0 && value == '(') {
            unsigned char payload_tail[RIN_SSCANF_MAX_INPUT];
            size_t payload_start = count;
            size_t payload_tail_count = 1u;

            candidate[count++] = (char)value;
            payload_tail[0] = value;
            for (;;) {
                if (count == limit) {
                    goto rollback_nan_payload;
                }
                status = rin_fscanf_stream_read(input, &value);
                if (status < 0) return -1;
                if (status == 0) {
                    goto rollback_nan_payload;
                }
                if (value == ')') {
                    candidate[count++] = (char)value;
                    goto parse_candidate;
                }
                if (!rin_fscanf_stream_nan_payload_char(value)) {
                    payload_tail[payload_tail_count++] = value;
                    goto rollback_nan_payload;
                }
                candidate[count++] = (char)value;
                payload_tail[payload_tail_count++] = value;
            }

rollback_nan_payload:
            count = payload_start;
            if (rin_fscanf_stream_unread_float_tail(
                    input, payload_tail, payload_tail_count) != 0)
                return -1;
            goto parse_candidate;
        }
        if (status > 0 && rin_fscanf_stream_unread(input, value) != 0)
            return -1;
    }

    if (rin_fscanf_stream_case_equal(first, 'i') && limit - count >= 5u) {
        static const char infinity_suffix[] = "nity";
        status = rin_fscanf_stream_read(input, &value);
        if (status < 0) return -1;
        if (status > 0 && rin_fscanf_stream_case_equal(value, 'i')) {
            tail[0] = value;
            tail_count = 1u;
            for (suffix_index = 0u; infinity_suffix[suffix_index] != '\0';
                 ++suffix_index) {
                status = rin_fscanf_stream_read(input, &value);
                if (status < 0) return -1;
                if (status == 0 ||
                    !rin_fscanf_stream_case_equal(
                        value, infinity_suffix[suffix_index])) {
                    if (status > 0) tail[tail_count++] = value;
                    if (rin_fscanf_stream_unread_float_tail(
                            input, tail, tail_count) != 0)
                        return -1;
                    goto parse_candidate;
                }
                tail[tail_count++] = value;
            }
            for (suffix_index = 0u; suffix_index < tail_count;
                 ++suffix_index)
                candidate[count++] = (char)tail[suffix_index];
        } else if (status > 0 && rin_fscanf_stream_unread(input, value) != 0) {
            return -1;
        }
    }

parse_candidate:
    return rin_fscanf_stream_parse_float_candidate(
        candidate, count, spec, value_out, input_failure_out);
}

/* Direct hexadecimal float lexer.  A rejected `0x` prefix falls back to the
 * decimal zero conversion while restoring every non-hexadecimal byte. */
static int rin_fscanf_stream_parse_hex_float(
        struct RinFscanfStreamInput* input,
        const struct RinSscanfSpec* spec, char* candidate, size_t count,
        unsigned char prefix_marker, struct RinSscanfFloat* value_out,
        int* input_failure_out) {
    unsigned char value = '\0';
    unsigned char tail[3];
    size_t tail_count = 1u;
    size_t limit = spec->has_width ? spec->width : RIN_SSCANF_MAX_INPUT;
    int status;
    int any_digit = 0;
    int seen_point = 0;

    candidate[count++] = '0';
    candidate[count++] = (char)prefix_marker;
    tail[0] = prefix_marker;
    if (count == limit) goto decimal_zero;
    status = rin_fscanf_stream_read(input, &value);
    if (status < 0) return -1;
    if (status == 0) goto decimal_zero;
    for (;;) {
        int digit = rin_fp_hex_digit((char)value);
        if (digit >= 0) {
            any_digit = 1;
            candidate[count++] = (char)value;
        } else if (value == '.' && !seen_point) {
            seen_point = 1;
            candidate[count++] = (char)value;
            tail[tail_count++] = value;
        } else {
            break;
        }
        if (count == limit) {
            status = 2;
            break;
        }
        status = rin_fscanf_stream_read(input, &value);
        if (status <= 0) break;
    }
    if (status < 0) return -1;
    if (!any_digit) {
        if (status > 0) tail[tail_count++] = value;
        if (seen_point) --count;
        --count;
        if (rin_fscanf_stream_unread_float_tail(input, tail, tail_count) != 0)
            return -1;
        return rin_fscanf_stream_parse_float_candidate(
            candidate, count, spec, value_out, input_failure_out);
    }
    if (status == 0 || status == 2) goto parse_candidate;
    if (value == 'p' || value == 'P') {
        unsigned char marker = value;
        if (count + 1u >= limit) {
            if (rin_fscanf_stream_unread(input, marker) != 0) return -1;
            goto parse_candidate;
        }
        status = rin_fscanf_stream_read(input, &value);
        if (status < 0) return -1;
        if (status == 0) {
            if (rin_fscanf_stream_unread(input, marker) != 0) return -1;
            goto parse_candidate;
        }
        if (value >= '0' && value <= '9') {
            candidate[count++] = (char)marker;
            candidate[count++] = (char)value;
        } else if (value == '+' || value == '-') {
            unsigned char sign = value;
            if (count + 2u >= limit) {
                tail[0] = marker;
                tail[1] = sign;
                if (rin_fscanf_stream_unread_float_tail(input, tail, 2u) != 0)
                    return -1;
                goto parse_candidate;
            }
            status = rin_fscanf_stream_read(input, &value);
            if (status < 0) return -1;
            if (status == 0) {
                tail[0] = marker;
                tail[1] = sign;
                if (rin_fscanf_stream_unread_float_tail(input, tail, 2u) != 0)
                    return -1;
                goto parse_candidate;
            }
            if (value < '0' || value > '9') {
                tail[0] = marker;
                tail[1] = sign;
                tail[2] = value;
                if (rin_fscanf_stream_unread_float_tail(input, tail, 3u) != 0)
                    return -1;
                goto parse_candidate;
            }
            candidate[count++] = (char)marker;
            candidate[count++] = (char)sign;
            candidate[count++] = (char)value;
        } else {
            tail[0] = marker;
            tail[1] = value;
            if (rin_fscanf_stream_unread_float_tail(input, tail, 2u) != 0)
                return -1;
            goto parse_candidate;
        }
        while (count < limit) {
            status = rin_fscanf_stream_read(input, &value);
            if (status < 0) return -1;
            if (status == 0) break;
            if (value < '0' || value > '9') {
                if (rin_fscanf_stream_unread(input, value) != 0) return -1;
                break;
            }
            candidate[count++] = (char)value;
        }
        goto parse_candidate;
    }
    if (rin_fscanf_stream_unread(input, value) != 0) return -1;

parse_candidate:
    return rin_fscanf_stream_parse_float_candidate(
        candidate, count, spec, value_out, input_failure_out);

decimal_zero:
    --count;
    if (rin_fscanf_stream_unread(input, tail[0]) != 0) return -1;
    return rin_fscanf_stream_parse_float_candidate(
        candidate, count, spec, value_out, input_failure_out);
}

/* Direct float lexer.  It commits an exponent only after a digit; malformed
 * e/E plus sign suffixes fit the bounded narrow pushback owner. */
static int rin_fscanf_stream_parse_decimal_float(
        struct RinFscanfStreamInput* input,
        const struct RinSscanfSpec* spec, struct RinSscanfFloat* value_out,
        int* input_failure_out) {
    char candidate[RIN_SSCANF_MAX_INPUT + 1u];
    unsigned char value = '\0';
    unsigned char tail[3];
    size_t count = 0u;
    size_t limit = spec->has_width ? spec->width : RIN_SSCANF_MAX_INPUT;
    int status;
    int any_digit = 0;
    int seen_point = 0;

    *input_failure_out = 0;
    status = rin_fscanf_stream_skip_space(input);
    if (status <= 0) {
        if (status == 0) *input_failure_out = 1;
        return status < 0 ? -1 : 0;
    }
    status = rin_fscanf_stream_read(input, &value);
    if (status <= 0) {
        if (status == 0) *input_failure_out = 1;
        return status < 0 ? -1 : 0;
    }
    if (value == '+' || value == '-') {
        candidate[count++] = (char)value;
        if (count == limit) goto parse_candidate;
        status = rin_fscanf_stream_read(input, &value);
        if (status <= 0) {
            if (status == 0) *input_failure_out = 1;
            return status < 0 ? -1 : 0;
        }
    }
    if (value == '0' && count + 1u < limit) {
        status = rin_fscanf_stream_read(input, &value);
        if (status < 0) return -1;
        if (status > 0 && (value == 'x' || value == 'X'))
            return rin_fscanf_stream_parse_hex_float(
                input, spec, candidate, count, value, value_out,
                input_failure_out);
        if (status > 0 && rin_fscanf_stream_unread(input, value) != 0)
            return -1;
        value = '0';
    }
    if (value == 'i' || value == 'I' || value == 'n' || value == 'N')
        return rin_fscanf_stream_parse_special_float(
            input, spec, candidate, count, value, value_out,
            input_failure_out);
    for (;;) {
        if (value >= '0' && value <= '9') {
            any_digit = 1;
            candidate[count++] = (char)value;
        } else if (value == '.' && !seen_point) {
            seen_point = 1;
            candidate[count++] = (char)value;
        } else {
            break;
        }
        if (count == limit) {
            status = 2;
            break;
        }
        status = rin_fscanf_stream_read(input, &value);
        if (status <= 0) break;
    }
    if (status < 0) return -1;
    if (!any_digit) {
        if (status == 0) {
            *input_failure_out = 1;
            return 0;
        }
        if (count != 0u) tail[0] = (unsigned char)candidate[0];
        if (count == 2u) tail[1] = (unsigned char)candidate[1];
        tail[count] = value;
        if (rin_fscanf_stream_unread_float_tail(input, tail, count + 1u) != 0)
            return -1;
        return 0;
    }
    if (status == 0 || status == 2) goto parse_candidate;
    if ((value == 'x' || value == 'X') && !seen_point &&
        candidate[count - 1u] == '0' &&
        (count == 1u || (count == 2u &&
                         (candidate[0] == '+' || candidate[0] == '-')))) {
        tail[0] = (unsigned char)candidate[0];
        if (count == 2u) tail[1] = (unsigned char)candidate[1];
        tail[count] = value;
        if (rin_fscanf_stream_unread_float_tail(input, tail, count + 1u) != 0)
            return -1;
        return 0;
    }
    if (value == 'e' || value == 'E') {
        unsigned char marker = value;
        if (count + 1u >= limit) {
            if (rin_fscanf_stream_unread(input, marker) != 0) return -1;
            goto parse_candidate;
        }
        status = rin_fscanf_stream_read(input, &value);
        if (status < 0) return -1;
        if (status == 0) {
            if (rin_fscanf_stream_unread(input, marker) != 0) return -1;
            goto parse_candidate;
        }
        if (value >= '0' && value <= '9') {
            candidate[count++] = (char)marker;
            candidate[count++] = (char)value;
        } else if (value == '+' || value == '-') {
            unsigned char sign = value;
            if (count + 2u >= limit) {
                tail[0] = marker;
                tail[1] = sign;
                if (rin_fscanf_stream_unread_float_tail(input, tail, 2u) != 0)
                    return -1;
                goto parse_candidate;
            }
            status = rin_fscanf_stream_read(input, &value);
            if (status < 0) return -1;
            if (status == 0) {
                tail[0] = marker;
                tail[1] = sign;
                if (rin_fscanf_stream_unread_float_tail(input, tail, 2u) != 0)
                    return -1;
                goto parse_candidate;
            }
            if (value < '0' || value > '9') {
                tail[0] = marker;
                tail[1] = sign;
                tail[2] = value;
                if (rin_fscanf_stream_unread_float_tail(input, tail, 3u) != 0)
                    return -1;
                goto parse_candidate;
            }
            candidate[count++] = (char)marker;
            candidate[count++] = (char)sign;
            candidate[count++] = (char)value;
        } else {
            tail[0] = marker;
            tail[1] = value;
            if (rin_fscanf_stream_unread_float_tail(input, tail, 2u) != 0)
                return -1;
            goto parse_candidate;
        }
        while (count < limit) {
            status = rin_fscanf_stream_read(input, &value);
            if (status < 0) return -1;
            if (status == 0) break;
            if (value < '0' || value > '9') {
                if (rin_fscanf_stream_unread(input, value) != 0) return -1;
                break;
            }
            candidate[count++] = (char)value;
        }
        goto parse_candidate;
    }
    if (rin_fscanf_stream_unread(input, value) != 0) return -1;

parse_candidate:
    return rin_fscanf_stream_parse_float_candidate(
        candidate, count, spec, value_out, input_failure_out);
}

/* Validate every supported stream conversion before the first read.  Unsupported
 * grammar stays outside the cursor so it cannot consume a partial line. */
static int rin_fscanf_validate_format(const char* format) {
    size_t offset = 0u;
    if (rin_sscanf_validate_format(format) != 0) return -1;
    while (format[offset] != '\0') {
        struct RinSscanfSpec spec;
        if (format[offset++] != '%') continue;
        if (rin_sscanf_parse_spec(format, &offset, &spec) != 0) return -1;
        if (spec.conversion == 'e' || spec.conversion == 'E' ||
            spec.conversion == 'f' || spec.conversion == 'F' ||
            spec.conversion == 'g' || spec.conversion == 'G' ||
            spec.conversion == 'a' || spec.conversion == 'A') {
            if (spec.length != RIN_SSCANF_LENGTH_NONE &&
                spec.length != RIN_SSCANF_LENGTH_LONG &&
                spec.length != RIN_SSCANF_LENGTH_LONG_DOUBLE) {
                errno = ENOSYS;
                return -1;
            }
            continue;
        }
        if (spec.conversion == '%' || spec.conversion == 'c' ||
            spec.conversion == 's' || spec.conversion == '[' ||
            spec.conversion == 'n' ||
            spec.conversion == 'd' || spec.conversion == 'i' ||
            spec.conversion == 'u' || spec.conversion == 'o' ||
            spec.conversion == 'x' || spec.conversion == 'X' ||
            spec.conversion == 'p')
            continue;
        errno = ENOSYS;
        return -1;
    }
    return 0;
}

/* Direct byte-stream subset: literals, whitespace, %%, %c, width-bounded %s,
 * %n, and bounded integer/pointer conversions.  A delimiter is put back
 * before caller output is published. */
static inline int _rin_vfscanf_unlocked(FILE* stream, const char* format, va_list ap) {
    struct RinFscanfStreamInput input;
    va_list arguments;
    size_t format_offset = 0u;
    int assignments = 0;
    if (!stream || !format) {
        errno = EINVAL;
        return EOF;
    }
    if (rin_fscanf_validate_format(format) != 0) return EOF;
    if (!_rin_stdio_claim_byte_orientation(stream)) return EOF;
    input.stream = stream;
    input.consumed = 0u;
    input.finished = 0;
    input.terminal = 0;
    va_copy(arguments, ap);
    while (format[format_offset] != '\0') {
        unsigned char format_value = (unsigned char)format[format_offset];
        if (rin_sscanf_space(format_value)) {
            int status;
            while (rin_sscanf_space((unsigned char)format[format_offset]))
                ++format_offset;
            status = rin_fscanf_stream_skip_space(&input);
            if (status < 0) {
                va_end(arguments);
                return rin_sscanf_input_error_result(assignments);
            }
            continue;
        }
        if (format_value != '%') {
            unsigned char value = '\0';
            int status = rin_fscanf_stream_read(&input, &value);
            ++format_offset;
            if (status <= 0) {
                va_end(arguments);
                return rin_sscanf_input_error_result(assignments);
            }
            if (value != format_value) {
                if (rin_fscanf_stream_unread(&input, value) != 0) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                va_end(arguments);
                return assignments;
            }
            continue;
        }
        {
            struct RinSscanfSpec spec;
            char candidate[RIN_SSCANF_MAX_INPUT + 1u];
            size_t count;
            ++format_offset;
            if (rin_sscanf_parse_spec(format, &format_offset, &spec) != 0) {
                va_end(arguments);
                return rin_sscanf_input_error_result(assignments);
            }
            if (spec.conversion == '%') {
                unsigned char value = '\0';
                int status = rin_fscanf_stream_read(&input, &value);
                if (status <= 0) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (value != '%') {
                    if (rin_fscanf_stream_unread(&input, value) != 0) {
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    va_end(arguments);
                    return assignments;
                }
                continue;
            }
            if (spec.conversion == 'n') {
                if (rin_sscanf_store_count(&arguments, &spec,
                                           input.consumed) != 0) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                continue;
            }
            if (spec.length == RIN_SSCANF_LENGTH_LONG &&
                (spec.conversion == 'c' || spec.conversion == 's' ||
                 spec.conversion == '[')) {
                size_t byte_count = 0u;
                size_t scalar_count = 0u;
                int input_failure = 0;
                int parsed = rin_fscanf_stream_wide_field(
                    &input, &spec, candidate, sizeof(candidate),
                    &byte_count, &scalar_count, &input_failure);
                (void)scalar_count;
                if (parsed < 0) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (parsed == 0) {
                    va_end(arguments);
                    return input_failure ?
                        rin_sscanf_input_error_result(assignments) : assignments;
                }
                if (rin_sscanf_store_wide_field(
                        &arguments, &spec, candidate, byte_count,
                        spec.suppress) != 0) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (!spec.suppress) ++assignments;
                continue;
            }
            if (spec.conversion == 'd' || spec.conversion == 'i' ||
                spec.conversion == 'u' || spec.conversion == 'o' ||
                spec.conversion == 'x' || spec.conversion == 'X' ||
                spec.conversion == 'p') {
                uint64_t magnitude = 0u;
                int negative = 0;
                int input_failure = 0;
                int parsed = rin_fscanf_stream_parse_integer(
                    &input, &spec, &magnitude, &negative, &input_failure,
                    spec.conversion == 'o' ? 8u :
                    (spec.conversion == 'x' || spec.conversion == 'X' ||
                     spec.conversion == 'p') ?
                    16u : spec.conversion == 'i' ? 0u : 10u);
                if (parsed < 0 || (parsed > 0 &&
                    !rin_sscanf_integer_in_range(&spec, magnitude, negative))) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (parsed == 0) {
                    va_end(arguments);
                    return input_failure ?
                        rin_sscanf_input_error_result(assignments) : assignments;
                }
                if (!spec.suppress) {
                    if (rin_sscanf_store_integer(&arguments, &spec, magnitude,
                                                 negative) != 0) {
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    ++assignments;
                }
                continue;
            }
            if (spec.conversion == 'e' || spec.conversion == 'E' ||
                spec.conversion == 'f' || spec.conversion == 'F' ||
                spec.conversion == 'g' || spec.conversion == 'G' ||
                spec.conversion == 'a' || spec.conversion == 'A') {
                struct RinSscanfFloat value;
                int input_failure = 0;
                int parsed = rin_fscanf_stream_parse_decimal_float(
                    &input, &spec, &value, &input_failure);
                if (parsed < 0) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (parsed == 0) {
                    va_end(arguments);
                    return input_failure ?
                        rin_sscanf_input_error_result(assignments) : assignments;
                }
                if (!spec.suppress) {
                    if (rin_sscanf_store_float(&arguments, &spec, &value) != 0) {
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    ++assignments;
                }
                continue;
            }
            if (spec.conversion == '[') {
                int status = 1;
                size_t limit = spec.has_width ? spec.width : RIN_SSCANF_MAX_INPUT;
                count = 0u;
                while (count < limit) {
                    unsigned char value = '\0';
                    status = rin_fscanf_stream_read(&input, &value);
                    if (status < 0) {
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    if (status == 0) break;
                    if (!rin_sscanf_scanset_contains(&spec, value)) {
                        if (rin_fscanf_stream_unread(&input, value) != 0) {
                            va_end(arguments);
                            return rin_sscanf_input_error_result(assignments);
                        }
                        break;
                    }
                    candidate[count++] = (char)value;
                }
                if (count == 0u) {
                    va_end(arguments);
                    return status == 0 ?
                        rin_sscanf_input_error_result(assignments) : assignments;
                }
                if (!spec.suppress) {
                    char* output = va_arg(arguments, char*);
                    if (!output) {
                        errno = EFAULT;
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    for (size_t index = 0u; index < count; ++index)
                        output[index] = candidate[index];
                    output[count] = '\0';
                    ++assignments;
                }
                continue;
            }
            if (spec.conversion == 'c') {
                count = spec.has_width ? spec.width : 1u;
                for (size_t index = 0u; index < count; ++index) {
                    unsigned char value = '\0';
                    int status = rin_fscanf_stream_read(&input, &value);
                    if (status <= 0) {
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    candidate[index] = (char)value;
                }
                if (!spec.suppress) {
                    char* output = va_arg(arguments, char*);
                    if (!output) {
                        errno = EFAULT;
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    for (size_t index = 0u; index < count; ++index)
                        output[index] = candidate[index];
                    ++assignments;
                }
                continue;
            }
            {
                int status = rin_fscanf_stream_skip_space(&input);
                size_t limit = spec.has_width ? spec.width : RIN_SSCANF_MAX_INPUT;
                count = 0u;
                if (status <= 0) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                while (count < limit) {
                    unsigned char value = '\0';
                    status = rin_fscanf_stream_read(&input, &value);
                    if (status < 0) {
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    if (status == 0) break;
                    if (rin_sscanf_space(value)) {
                        if (rin_fscanf_stream_unread(&input, value) != 0) {
                            va_end(arguments);
                            return rin_sscanf_input_error_result(assignments);
                        }
                        break;
                    }
                    candidate[count++] = (char)value;
                }
                if (count == 0u) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (!spec.suppress) {
                    char* output = va_arg(arguments, char*);
                    if (!output) {
                        errno = EFAULT;
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    for (size_t index = 0u; index < count; ++index)
                        output[index] = candidate[index];
                    output[count] = '\0';
                    ++assignments;
                }
            }
        }
    }
    va_end(arguments);
    return assignments;
}

static inline int vfscanf(FILE* stream, const char* format, va_list ap) {
    int result;
    if (!stream) {
        errno = EINVAL;
        return EOF;
    }
    flockfile(stream);
    result = _rin_vfscanf_unlocked(stream, format, ap);
    funlockfile(stream);
    return result;
}

static inline int vscanf(const char* format, va_list ap) {
    return vfscanf(stdin, format, ap);
}

static inline int fscanf(FILE* stream, const char* format, ...) {
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = vfscanf(stream, format, arguments);
    va_end(arguments);
    return result;
}

static inline int scanf(const char* format, ...) {
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = vscanf(format, arguments);
    va_end(arguments);
    return result;
}

#endif /* _RIN_STDIO_SCANF_H */
