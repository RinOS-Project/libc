/* SPDX-License-Identifier: MIT */

#include "wchar.h"
#include "stdio.h"
#include "limits.h"

#define RIN_WPRINTF_FORMAT_LIMIT 4096u
#define RIN_WPRINTF_STRING_LIMIT 4096u
#define RIN_WSCANF_FORMAT_LIMIT 4096u
#define RIN_WSCANF_FIELD_LIMIT 4096u
#define RIN_WSCANF_MAX_INPUT RIN_WSCANF_FORMAT_LIMIT

/* Focused tests replace only this scalar sink; product builds share fputwc(). */
#ifndef RIN_WPRINTF_PUTWC
#define RIN_WPRINTF_PUTWC(value, stream) fputwc((value), (stream))
#endif

/* Focused tests replace these wide input primitives; products use fgetwc(). */
#ifndef RIN_WSCANF_GETWC
#define RIN_WSCANF_GETWC(stream) fgetwc((stream))
#endif
#ifndef RIN_WSCANF_UNGETWC
#define RIN_WSCANF_UNGETWC(value, stream) ungetwc((value), (stream))
#endif

typedef int (*RinWprintfEmit)(void* context, wchar_t value);

typedef struct {
    size_t length;
} RinWprintfCounter;

typedef struct {
    wchar_t* output;
    size_t capacity;
    size_t length;
} RinWprintfBuffer;

typedef struct {
    FILE* stream;
} RinWprintfStream;

static int rin_wchar_scalar_valid(uint32_t value) {
    return value <= 0x10FFFFu && !(value >= 0xD800u && value <= 0xDFFFu);
}

static int rin_wprintf_count_emit(void* context, wchar_t value) {
    RinWprintfCounter* counter = (RinWprintfCounter*)context;
    (void)value;
    if (counter->length == SIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    ++counter->length;
    return 0;
}

static int rin_wprintf_buffer_emit(void* context, wchar_t value) {
    RinWprintfBuffer* buffer = (RinWprintfBuffer*)context;
    if (buffer->length >= buffer->capacity) {
        errno = EOVERFLOW;
        return -1;
    }
    buffer->output[buffer->length++] = value;
    return 0;
}

static int rin_wprintf_stream_emit(void* context, wchar_t value) {
    RinWprintfStream* output = (RinWprintfStream*)context;
    return RIN_WPRINTF_PUTWC(value, output->stream) == WEOF ? -1 : 0;
}

static int rin_wprintf_scalar(RinWprintfEmit emit, void* context,
                              uint32_t value) {
    if (!rin_wchar_scalar_valid(value)) {
        errno = EILSEQ;
        return -1;
    }
#if defined(__SIZEOF_WCHAR_T__) && __SIZEOF_WCHAR_T__ == 2
    if (value > 0xffffu) {
        uint32_t high;
        uint32_t low;
        high = 0xd800u + ((value - 0x10000u) >> 10u);
        low = 0xdc00u + ((value - 0x10000u) & 0x3ffu);
        if (emit(context, (wchar_t)high) != 0) return -1;
        return emit(context, (wchar_t)low);
    }
#endif
    return emit(context, (wchar_t)value);
}

static int rin_wprintf_unsigned(RinWprintfEmit emit, void* context,
                                unsigned long long value, unsigned int base,
                                int uppercase) {
    wchar_t reversed[65];
    size_t length = 0u;
    do {
        unsigned int digit = (unsigned int)(value % base);
        reversed[length++] = (wchar_t)(digit < 10u ? '0' + digit :
            (uppercase ? 'A' : 'a') + (digit - 10u));
        value /= base;
    } while (value != 0u);
    while (length != 0u) {
        --length;
        if (emit(context, reversed[length]) != 0) return -1;
    }
    return 0;
}

static int rin_wprintf_signed(RinWprintfEmit emit, void* context,
                              long long value) {
    unsigned long long magnitude;
    if (value < 0) {
        if (emit(context, L'-') != 0) return -1;
        magnitude = (unsigned long long)(-(value + 1LL)) + 1ULL;
    } else {
        magnitude = (unsigned long long)value;
    }
    return rin_wprintf_unsigned(emit, context, magnitude, 10u, 0);
}

static int rin_wprintf_string(RinWprintfEmit emit, void* context,
                               const wchar_t* text) {
    size_t index;
    if (!text) {
        errno = EINVAL;
        return -1;
    }
    for (index = 0u; index < RIN_WPRINTF_STRING_LIMIT; ++index) {
        uint32_t value = (uint32_t)text[index];
        if (value == 0u) return 0;
        if (rin_wprintf_scalar(emit, context, value) != 0) return -1;
    }
    errno = EOVERFLOW;
    return -1;
}

static int rin_wprintf_narrow_character(RinWprintfEmit emit, void* context,
                                        unsigned char byte) {
    rin_unicode_mbstate_t state = {0u, 0u};
    uint32_t value = 0u;
    size_t conversion = rin_unicode_mbrtowc32(
        &value, (const char*)&byte, 1u, &state);
    if (conversion == (size_t)-1 || conversion == (size_t)-2) {
        errno = EILSEQ;
        return -1;
    }
    return rin_wprintf_scalar(emit, context, value);
}

static int rin_wprintf_narrow_string(RinWprintfEmit emit, void* context,
                                      const char* text) {
    rin_unicode_mbstate_t state = {0u, 0u};
    size_t index;
    if (!text) {
        errno = EINVAL;
        return -1;
    }
    for (index = 0u; index < RIN_WPRINTF_STRING_LIMIT; ++index) {
        uint32_t value = 0u;
        unsigned char byte = (unsigned char)text[index];
        size_t conversion;
        if (byte == 0u) {
            if (state.state != 0u) {
                errno = EILSEQ;
                return -1;
            }
            return 0;
        }
        conversion = rin_unicode_mbrtowc32(
            &value, (const char*)&byte, 1u, &state);
        if (conversion == (size_t)-1) {
            errno = EILSEQ;
            return -1;
        }
        if (conversion != (size_t)-2 &&
            rin_wprintf_scalar(emit, context, value) != 0)
            return -1;
    }
    errno = EOVERFLOW;
    return -1;
}

/* Apply the supported string/character width after converting into a private
 * candidate.  This keeps the two-pass formatter failure-atomic even when a
 * multibyte source expands to more than one wide scalar. */
static int rin_wprintf_text_formatted(RinWprintfEmit emit, void* context,
                                       const wchar_t* candidate,
                                       size_t length, unsigned int width,
                                       int left) {
    size_t padding;
    size_t index;
    if ((size_t)width <= length) {
        for (index = 0u; index < length; ++index)
            if (emit(context, candidate[index]) != 0) return -1;
        return 0;
    }
    padding = (size_t)width - length;
    if (!left) {
        for (index = 0u; index < padding; ++index)
            if (emit(context, L' ') != 0) return -1;
    }
    for (index = 0u; index < length; ++index)
        if (emit(context, candidate[index]) != 0) return -1;
    if (left) {
        for (index = 0u; index < padding; ++index)
            if (emit(context, L' ') != 0) return -1;
    }
    return 0;
}

static int rin_wprintf_text_candidate(RinWprintfEmit emit, void* context,
                                       int kind, const void* value,
                                       unsigned int width, int left,
                                       unsigned int precision,
                                       int precision_set) {
    wchar_t candidate[RIN_WPRINTF_STRING_LIMIT * 2u];
    RinWprintfBuffer buffer;
    int result;
    buffer.output = candidate;
    buffer.capacity = sizeof(candidate) / sizeof(candidate[0]);
    buffer.length = 0u;
    if (kind == 0) {
        result = rin_wprintf_narrow_string(rin_wprintf_buffer_emit, &buffer,
                                           (const char*)value);
    } else if (kind == 1) {
        result = rin_wprintf_string(rin_wprintf_buffer_emit, &buffer,
                                    (const wchar_t*)value);
    } else if (kind == 2) {
        result = rin_wprintf_narrow_character(rin_wprintf_buffer_emit,
                                              &buffer,
                                              (unsigned char)(uintptr_t)value);
    } else {
        result = rin_wprintf_scalar(rin_wprintf_buffer_emit, &buffer,
                                    (uint32_t)(uintptr_t)value);
    }
    if (result != 0) return -1;
    if (precision_set && buffer.length > (size_t)precision)
        buffer.length = (size_t)precision;
    return rin_wprintf_text_formatted(emit, context, candidate, buffer.length,
                                      width, left);
}

/* `%p` keeps its `0x` prefix ahead of zero padding.  The candidate is bounded
 * by the target pointer width, and only the ordinary width/left/zero subset
 * is admitted; locale/prefix alternatives remain explicitly unsupported. */
static int rin_wprintf_pointer_formatted(RinWprintfEmit emit, void* context,
                                          const void* value,
                                          unsigned int width, int left,
                                          int zero) {
    wchar_t candidate[2u + sizeof(uintptr_t) * 2u];
    RinWprintfBuffer buffer;
    size_t padding;
    size_t index;
    buffer.output = candidate;
    buffer.capacity = sizeof(candidate) / sizeof(candidate[0]);
    buffer.length = 0u;
    candidate[buffer.length++] = L'0';
    candidate[buffer.length++] = L'x';
    if (rin_wprintf_unsigned(rin_wprintf_buffer_emit, &buffer,
                             (unsigned long long)(uintptr_t)value,
                             16u, 0) != 0)
        return -1;
    if ((size_t)width <= buffer.length)
        return rin_wprintf_text_formatted(emit, context, candidate,
                                          buffer.length, width, left);
    padding = (size_t)width - buffer.length;
    if (left)
        return rin_wprintf_text_formatted(emit, context, candidate,
                                          buffer.length, width, left);
    if (!zero)
        return rin_wprintf_text_formatted(emit, context, candidate,
                                          buffer.length, width, 0);
    if (emit(context, candidate[0]) != 0 || emit(context, candidate[1]) != 0)
        return -1;
    for (index = 0u; index < padding; ++index)
        if (emit(context, L'0') != 0) return -1;
    for (index = 2u; index < buffer.length; ++index)
        if (emit(context, candidate[index]) != 0) return -1;
    return 0;
}

/* Render one integer conversion into a bounded private candidate before
 * applying width.  The formatter is executed twice (count, then publish),
 * so keeping the whole conversion local preserves the existing
 * failure-atomic string contract when the destination is too small. */
static int rin_wprintf_integer_formatted(
    RinWprintfEmit emit, void* context, long long signed_value,
    unsigned long long unsigned_value, int is_signed, unsigned int base,
    int uppercase, unsigned int width, int left, int zero, int plus,
    int space, int alternate, unsigned int precision, int precision_set) {
    wchar_t candidate[128];
    RinWprintfBuffer buffer;
    size_t length;
    size_t padding;
    size_t index;
    size_t alternate_prefix = 0u;

    buffer.output = candidate;
    buffer.capacity = sizeof(candidate) / sizeof(candidate[0]);
    buffer.length = 0u;
    if (is_signed) {
        if (rin_wprintf_signed(rin_wprintf_buffer_emit, &buffer,
                               signed_value) != 0)
            return -1;
        if (candidate[0] != L'-' && (plus || space)) {
            if (buffer.length >= buffer.capacity) {
                errno = EOVERFLOW;
                return -1;
            }
            for (index = buffer.length; index != 0u; --index)
                candidate[index] = candidate[index - 1u];
            candidate[0] = plus ? L'+' : L' ';
            ++buffer.length;
        }
    } else if (rin_wprintf_unsigned(rin_wprintf_buffer_emit, &buffer,
                                    unsigned_value, base, uppercase) != 0) {
        return -1;
    }
    length = buffer.length;
    if (!is_signed && alternate && base == 16u &&
        !(length == 1u && candidate[0] == L'0')) {
        if (length > sizeof(candidate) / sizeof(candidate[0]) - 2u) {
            errno = EOVERFLOW;
            return -1;
        }
        for (index = length; index != 0u; --index)
            candidate[index + 1u] = candidate[index - 1u];
        candidate[0] = L'0';
        candidate[1] = uppercase ? L'X' : L'x';
        length += 2u;
        alternate_prefix = 2u;
    }
    if (precision_set) {
        size_t prefix = 0u;
        size_t digits;
        if (is_signed && length != 0u &&
            (candidate[0] == L'-' || candidate[0] == L'+' ||
             candidate[0] == L' '))
            prefix = 1u;
        if (alternate_prefix != 0u)
            prefix += alternate_prefix;
        digits = length - prefix;
        if (precision == 0u && digits == 1u &&
            candidate[prefix] == L'0') {
            length = prefix;
            digits = 0u;
        }
        if ((size_t)precision > digits) {
            size_t zeros = (size_t)precision - digits;
            if (length > sizeof(candidate) / sizeof(candidate[0]) - zeros) {
                errno = EOVERFLOW;
                return -1;
            }
            for (index = length; index > prefix; --index)
                candidate[index + zeros - 1u] = candidate[index - 1u];
            for (index = 0u; index < zeros; ++index)
                candidate[prefix + index] = L'0';
            length += zeros;
        }
        zero = 0;
    }
    if (!is_signed && alternate && base == 8u) {
        if (length == 0u) {
            candidate[0] = L'0';
            length = 1u;
        } else if (candidate[0] != L'0') {
            if (length >= sizeof(candidate) / sizeof(candidate[0])) {
                errno = EOVERFLOW;
                return -1;
            }
            for (index = length; index != 0u; --index)
                candidate[index] = candidate[index - 1u];
            candidate[0] = L'0';
            ++length;
        }
    }
    if ((size_t)width <= length) {
        for (index = 0u; index < length; ++index)
            if (emit(context, candidate[index]) != 0) return -1;
        return 0;
    }
    padding = (size_t)width - length;
    if (left) {
        for (index = 0u; index < length; ++index)
            if (emit(context, candidate[index]) != 0) return -1;
        while (padding-- != 0u)
            if (emit(context, L' ') != 0) return -1;
        return 0;
    }
    if (!precision_set && zero && alternate_prefix != 0u) {
        if (emit(context, candidate[0]) != 0 ||
            emit(context, candidate[1]) != 0)
            return -1;
        for (index = 0u; index < padding; ++index)
            if (emit(context, L'0') != 0) return -1;
        for (index = 2u; index < length; ++index)
            if (emit(context, candidate[index]) != 0) return -1;
        return 0;
    }
    if (!precision_set && zero && candidate[0] == L'-') {
        if (emit(context, candidate[0]) != 0) return -1;
        for (index = 0u; index < padding; ++index)
            if (emit(context, L'0') != 0) return -1;
        for (index = 1u; index < length; ++index)
            if (emit(context, candidate[index]) != 0) return -1;
        return 0;
    }
    if (!precision_set && zero && is_signed &&
        (candidate[0] == L'+' || candidate[0] == L' ')) {
        if (emit(context, candidate[0]) != 0) return -1;
        for (index = 0u; index < padding; ++index)
            if (emit(context, L'0') != 0) return -1;
        for (index = 1u; index < length; ++index)
            if (emit(context, candidate[index]) != 0) return -1;
        return 0;
    }
    if (!precision_set && zero) {
        for (index = 0u; index < padding; ++index)
            if (emit(context, L'0') != 0) return -1;
        for (index = 0u; index < length; ++index)
            if (emit(context, candidate[index]) != 0) return -1;
        return 0;
    }
    while (padding-- != 0u)
        if (emit(context, L' ') != 0) return -1;
    for (index = 0u; index < length; ++index)
        if (emit(context, candidate[index]) != 0) return -1;
    return 0;
}

/* Render the binary64 hexadecimal form locally instead of routing through the
 * decimal-only narrow formatter.  The 52 fraction bits are exactly thirteen
 * hexadecimal digits, so the bounded candidate can round a requested
 * precision without depending on locale or an unbounded conversion helper. */
static int rin_wprintf_hex_float_formatted(
    RinWprintfEmit emit, void* context, double value, int uppercase,
    unsigned int width, int left, int zero, int plus, int space,
    int alternate, unsigned int precision, int precision_set) {
    union { double value; uint64_t bits; } representation;
    char candidate[RIN_WPRINTF_STRING_LIMIT * 2u];
    wchar_t wide[RIN_WPRINTF_STRING_LIMIT * 2u];
    const char* digits = uppercase ? "0123456789ABCDEF" :
                                      "0123456789abcdef";
    uint64_t fraction;
    unsigned int exponent_bits;
    int negative;
    int exponent = 0;
    int subnormal = 0;
    char leading;
    unsigned int available = 13u;
    unsigned int shown;
    unsigned int index;
    unsigned int guard;
    int sticky;
    int round_up = 0;
    size_t length = 0u;
    size_t padding;
    size_t wide_length;
    size_t wide_index;

    if (precision_set && precision >= RIN_WPRINTF_STRING_LIMIT) {
        errno = EOVERFLOW;
        return -1;
    }
    representation.value = value;
    negative = (int)(representation.bits >> 63u);
    exponent_bits = (unsigned int)((representation.bits >> 52u) & 0x7ffu);
    fraction = representation.bits & UINT64_C(0x000fffffffffffff);
    if (negative) candidate[length++] = '-';
    else if (plus) candidate[length++] = '+';
    else if (space) candidate[length++] = ' ';

    if (exponent_bits == 0x7ffu) {
        const char* text = fraction != 0u ?
            (uppercase ? "NAN" : "nan") :
            (uppercase ? "INF" : "inf");
        for (index = 0u; text[index] != '\0'; ++index)
            candidate[length++] = text[index];
    } else if (exponent_bits == 0u && fraction == 0u) {
        candidate[length++] = '0';
        candidate[length++] = uppercase ? 'X' : 'x';
        candidate[length++] = '0';
        if (precision_set && precision != 0u) {
            candidate[length++] = '.';
            for (index = 0u; index < precision; ++index)
                candidate[length++] = '0';
        } else if (alternate) {
            candidate[length++] = '.';
        }
        candidate[length++] = uppercase ? 'P' : 'p';
        candidate[length++] = '+';
        candidate[length++] = '0';
    } else {
        if (exponent_bits == 0u) {
            subnormal = 1;
            exponent = -1022;
            leading = '0';
        } else {
            exponent = (int)exponent_bits - 1023;
            leading = '1';
        }
        candidate[length++] = '0';
        candidate[length++] = uppercase ? 'X' : 'x';
        candidate[length++] = leading;
        shown = precision_set ? precision : available;
        if (shown < available) {
            unsigned int guard_shift = 52u - (shown + 1u) * 4u;
            guard = (unsigned int)((fraction >> guard_shift) & 0xfu);
            sticky = 0;
            if (guard_shift != 0u) {
                uint64_t discarded = fraction &
                    ((UINT64_C(1) << guard_shift) - 1u);
                sticky = discarded != 0u;
            } else {
                sticky = fraction != 0u;
            }
            round_up = guard > 8u || (guard == 8u &&
                                      (sticky || (shown == 0u ? 1u :
                                       (((fraction >> (52u - shown * 4u)) &
                                         0xfu) & 1u))));
        }
        if (shown != 0u || alternate) candidate[length++] = '.';
        for (index = 0u; index < shown; ++index) {
            unsigned int nibble = 0u;
            if (index < available) {
                unsigned int shift = 52u - (index + 1u) * 4u;
                nibble = (unsigned int)((fraction >> shift) & 0xfu);
            }
            candidate[length++] = digits[nibble];
        }
        if (!precision_set) {
            while (length > 0u && candidate[length - 1u] == '0') --length;
            if (length > 0u && candidate[length - 1u] == '.') --length;
        } else if (shown < available && round_up) {
            size_t point = 0u;
            size_t cursor;
            int carried = 1;
            while (point < length && candidate[point] != '.') ++point;
            if (shown == 0u) {
                if (subnormal) candidate[point - 1u] = '1';
                else ++exponent;
            } else {
                cursor = length;
                while (cursor > point + 1u && carried) {
                    unsigned int nibble;
                    --cursor;
                    for (nibble = 0u; nibble < 16u; ++nibble)
                        if (digits[nibble] == candidate[cursor]) break;
                    if (nibble == 0u) candidate[cursor] = digits[15u];
                    else {
                        candidate[cursor] = digits[nibble - 1u];
                        carried = 0;
                    }
                }
                if (carried) {
                    /* 0x1.ffff rounded at finite precision becomes
                     * 0x1.0p+1. */
                    for (cursor = point + 1u; cursor < length; ++cursor)
                        candidate[cursor] = '0';
                    if (subnormal) candidate[point - 1u] = '1';
                    else ++exponent;
                }
            }
        }
        candidate[length++] = uppercase ? 'P' : 'p';
        if (exponent < 0) {
            candidate[length++] = '-';
            exponent = -exponent;
        } else {
            candidate[length++] = '+';
        }
        {
            char reversed[8];
            unsigned int count = 0u;
            do {
                reversed[count++] = (char)('0' + (exponent % 10));
                exponent /= 10;
            } while (exponent != 0);
            while (count != 0u) candidate[length++] = reversed[--count];
        }
    }
    if (length >= sizeof(candidate)) {
        errno = EOVERFLOW;
        return -1;
    }
    candidate[length] = '\0';
    {
        RinWprintfBuffer buffer;
        buffer.output = wide;
        buffer.capacity = sizeof(wide) / sizeof(wide[0]);
        buffer.length = 0u;
        if (rin_wprintf_narrow_string(rin_wprintf_buffer_emit, &buffer,
                                      candidate) != 0)
            return -1;
        wide_length = buffer.length;
    }
    if ((size_t)width <= wide_length) {
        for (wide_index = 0u; wide_index < wide_length; ++wide_index)
            if (emit(context, wide[wide_index]) != 0) return -1;
        return 0;
    }
    padding = (size_t)width - wide_length;
    if (left) {
        for (wide_index = 0u; wide_index < wide_length; ++wide_index)
            if (emit(context, wide[wide_index]) != 0) return -1;
        while (padding-- != 0u)
            if (emit(context, L' ') != 0) return -1;
        return 0;
    }
    if (zero && (wide[0] == L'-' || wide[0] == L'+' || wide[0] == L' ')) {
        if (emit(context, wide[0]) != 0) return -1;
        wide_index = 1u;
        if (wide_length > 2u && wide[wide_index] == L'0' &&
            (wide[wide_index + 1u] == L'x' ||
             wide[wide_index + 1u] == L'X')) {
            if (emit(context, wide[wide_index++]) != 0 ||
                emit(context, wide[wide_index++]) != 0)
                return -1;
        }
        while (padding-- != 0u)
            if (emit(context, L'0') != 0) return -1;
        while (wide_index < wide_length)
            if (emit(context, wide[wide_index++]) != 0) return -1;
        return 0;
    }
    if (zero && wide_length > 2u && wide[0] == L'0' &&
        (wide[1] == L'x' || wide[1] == L'X')) {
        if (emit(context, wide[0]) != 0 || emit(context, wide[1]) != 0)
            return -1;
        wide_index = 2u;
        while (padding-- != 0u)
            if (emit(context, L'0') != 0) return -1;
        while (wide_index < wide_length)
            if (emit(context, wide[wide_index++]) != 0) return -1;
        return 0;
    }
    while (padding-- != 0u)
        if (emit(context, zero ? L'0' : L' ') != 0) return -1;
    for (wide_index = 0u; wide_index < wide_length; ++wide_index)
        if (emit(context, wide[wide_index]) != 0) return -1;
    return 0;
}

static unsigned int rin_wprintf_binary80_nibble(uint64_t fraction,
                                                unsigned int index) {
    if (index == 15u) return (unsigned int)((fraction & UINT64_C(7)) << 1u);
    return (unsigned int)((fraction >> (59u - index * 4u)) & 0xfu);
}

/* The product x86 targets expose long double as little-endian x87 binary80
 * in a padded object.  Keep the byte-level ABI check here: unsupported
 * binary128/double-double or malformed unnormal values must remain explicit
 * failures rather than being silently narrowed to binary64. */
static int rin_wprintf_hex_long_double_formatted(
    RinWprintfEmit emit, void* context, long double value, int uppercase,
    unsigned int width, int left, int zero, int plus, int space,
    int alternate, unsigned int precision, int precision_set) {
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    union {
        long double value;
        unsigned char bytes[sizeof(long double)];
    } representation;
    char candidate[RIN_WPRINTF_STRING_LIMIT * 2u];
    wchar_t wide[RIN_WPRINTF_STRING_LIMIT * 2u];
    const char* digits = uppercase ? "0123456789ABCDEF" :
                                      "0123456789abcdef";
    uint64_t significand = 0u;
    uint64_t fraction;
    uint16_t exponent_sign;
    unsigned int exponent_bits;
    int negative;
    int exponent = 0;
    int subnormal = 0;
    char leading;
    const unsigned int available = 16u;
    unsigned int shown;
    unsigned int index;
    unsigned int guard;
    int sticky;
    int round_up = 0;
    size_t length = 0u;
    size_t padding;
    size_t wide_length;
    size_t wide_index;

    if (sizeof(long double) < 10u) {
        errno = ENOSYS;
        return -1;
    }
    if (precision_set && precision >= RIN_WPRINTF_STRING_LIMIT) {
        errno = EOVERFLOW;
        return -1;
    }
    representation.value = value;
    for (index = 0u; index < 8u; ++index)
        significand |= (uint64_t)representation.bytes[index] << (index * 8u);
    exponent_sign = (uint16_t)representation.bytes[8] |
                    (uint16_t)representation.bytes[9] << 8u;
    negative = (int)(exponent_sign >> 15u);
    exponent_bits = (unsigned int)(exponent_sign & 0x7fffu);
    fraction = significand & UINT64_C(0x7fffffffffffffff);
    if (negative) candidate[length++] = '-';
    else if (plus) candidate[length++] = '+';
    else if (space) candidate[length++] = ' ';

    if (exponent_bits == 0x7fffu) {
        const char* text;
        if ((significand & UINT64_C(0x7fffffffffffffff)) == 0u &&
            (significand & UINT64_C(0x8000000000000000)) != 0u)
            text = uppercase ? "INF" : "inf";
        else
            text = uppercase ? "NAN" : "nan";
        for (index = 0u; text[index] != '\0'; ++index)
            candidate[length++] = text[index];
    } else if (exponent_bits == 0u && fraction == 0u) {
        candidate[length++] = '0';
        candidate[length++] = uppercase ? 'X' : 'x';
        candidate[length++] = '0';
        if (precision_set && precision != 0u) {
            candidate[length++] = '.';
            for (index = 0u; index < precision; ++index)
                candidate[length++] = '0';
        } else if (alternate) {
            candidate[length++] = '.';
        }
        candidate[length++] = uppercase ? 'P' : 'p';
        candidate[length++] = '+';
        candidate[length++] = '0';
    } else {
        if (exponent_bits == 0u) {
            subnormal = 1;
            exponent = -16382;
            leading = '0';
        } else {
            if ((significand & UINT64_C(0x8000000000000000)) == 0u) {
                errno = EINVAL;
                return -1;
            }
            exponent = (int)exponent_bits - 16383;
            leading = '1';
        }
        candidate[length++] = '0';
        candidate[length++] = uppercase ? 'X' : 'x';
        candidate[length++] = leading;
        shown = precision_set ? precision : available;
        if (shown < available) {
            if (shown < 15u) {
                unsigned int guard_shift = 59u - shown * 4u;
                guard = (unsigned int)((fraction >> guard_shift) & 0xfu);
                sticky = guard_shift == 0u ? 0 :
                    (fraction & ((UINT64_C(1) << guard_shift) - 1u)) != 0u;
            } else {
                guard = (unsigned int)((fraction & UINT64_C(7)) << 1u);
                sticky = 0;
            }
            if (shown == 0u)
                round_up = guard > 8u || (guard == 8u &&
                    (sticky || (!subnormal)));
            else {
                unsigned int previous =
                    rin_wprintf_binary80_nibble(fraction, shown - 1u);
                round_up = guard > 8u ||
                    (guard == 8u && (sticky || (previous & 1u) != 0u));
            }
        }
        if (shown != 0u || alternate) candidate[length++] = '.';
        for (index = 0u; index < shown; ++index)
            candidate[length++] = index < available
                ? digits[rin_wprintf_binary80_nibble(fraction, index)] : '0';
        if (!precision_set) {
            while (length > 0u && candidate[length - 1u] == '0') --length;
            if (length > 0u && candidate[length - 1u] == '.') --length;
        } else if (shown < available && round_up) {
            size_t point = 0u;
            size_t cursor;
            int carried = 1;
            while (point < length && candidate[point] != '.') ++point;
            if (shown == 0u) {
                if (subnormal) candidate[point - 1u] = '1';
                else ++exponent;
            } else {
                cursor = length;
                while (cursor > point + 1u && carried) {
                    unsigned int nibble;
                    --cursor;
                    for (nibble = 0u; nibble < 16u; ++nibble)
                        if (digits[nibble] == candidate[cursor]) break;
                    if (nibble == 0u) candidate[cursor] = digits[15u];
                    else {
                        candidate[cursor] = digits[nibble - 1u];
                        carried = 0;
                    }
                }
                if (carried) {
                    for (cursor = point + 1u; cursor < length; ++cursor)
                        candidate[cursor] = '0';
                    if (subnormal) candidate[point - 1u] = '1';
                    else ++exponent;
                }
            }
        }
        candidate[length++] = uppercase ? 'P' : 'p';
        if (exponent < 0) {
            candidate[length++] = '-';
            exponent = -exponent;
        } else {
            candidate[length++] = '+';
        }
        {
            char reversed[8];
            unsigned int count = 0u;
            do {
                reversed[count++] = (char)('0' + exponent % 10);
                exponent /= 10;
            } while (exponent != 0);
            while (count != 0u) candidate[length++] = reversed[--count];
        }
    }
    if (length >= sizeof(candidate)) {
        errno = EOVERFLOW;
        return -1;
    }
    candidate[length] = '\0';
    {
        RinWprintfBuffer buffer;
        buffer.output = wide;
        buffer.capacity = sizeof(wide) / sizeof(wide[0]);
        buffer.length = 0u;
        if (rin_wprintf_narrow_string(rin_wprintf_buffer_emit, &buffer,
                                      candidate) != 0)
            return -1;
        wide_length = buffer.length;
    }
    if ((size_t)width <= wide_length) {
        for (wide_index = 0u; wide_index < wide_length; ++wide_index)
            if (emit(context, wide[wide_index]) != 0) return -1;
        return 0;
    }
    padding = (size_t)width - wide_length;
    if (left) {
        for (wide_index = 0u; wide_index < wide_length; ++wide_index)
            if (emit(context, wide[wide_index]) != 0) return -1;
        while (padding-- != 0u)
            if (emit(context, L' ') != 0) return -1;
        return 0;
    }
    if (zero && (wide[0] == L'-' || wide[0] == L'+' || wide[0] == L' ')) {
        if (emit(context, wide[0]) != 0) return -1;
        wide_index = 1u;
        if (wide_length > 2u && wide[wide_index] == L'0' &&
            (wide[wide_index + 1u] == L'x' ||
             wide[wide_index + 1u] == L'X')) {
            if (emit(context, wide[wide_index++]) != 0 ||
                emit(context, wide[wide_index++]) != 0)
                return -1;
        }
        while (padding-- != 0u)
            if (emit(context, L'0') != 0) return -1;
        while (wide_index < wide_length)
            if (emit(context, wide[wide_index++]) != 0) return -1;
        return 0;
    }
    if (zero && wide_length > 2u && wide[0] == L'0' &&
        (wide[1] == L'x' || wide[1] == L'X')) {
        if (emit(context, wide[0]) != 0 || emit(context, wide[1]) != 0)
            return -1;
        wide_index = 2u;
        while (padding-- != 0u)
            if (emit(context, L'0') != 0) return -1;
        while (wide_index < wide_length)
            if (emit(context, wide[wide_index++]) != 0) return -1;
        return 0;
    }
    while (padding-- != 0u)
        if (emit(context, zero ? L'0' : L' ') != 0) return -1;
    for (wide_index = 0u; wide_index < wide_length; ++wide_index)
        if (emit(context, wide[wide_index]) != 0) return -1;
    return 0;
#else
    (void)emit; (void)context; (void)value; (void)uppercase;
    (void)width; (void)left; (void)zero; (void)plus; (void)space;
    (void)alternate; (void)precision; (void)precision_set;
    errno = ENOSYS;
    return -1;
#endif
}

/* Format the bounded decimal floating subset through the existing narrow
 * formatter, then widen its complete ASCII candidate before applying the
 * wide-field padding.  Keeping the candidate private preserves the same
 * count/publish failure boundary as integer and string conversions. */
static int rin_wprintf_float_formatted(
    RinWprintfEmit emit, void* context, long double value,
    uint32_t conversion, int long_double,
    unsigned int width, int left, int zero, int plus, int space,
    int alternate, unsigned int precision, int precision_set) {
    char format[64];
    char candidate[RIN_WPRINTF_STRING_LIMIT * 2u];
    wchar_t wide[RIN_WPRINTF_STRING_LIMIT * 2u];
    RinWprintfBuffer buffer;
    size_t format_length = 0u;
    size_t length;
    size_t padding;
    size_t index;
    int result;

    if (precision_set && precision >= RIN_WPRINTF_STRING_LIMIT) {
        errno = EOVERFLOW;
        return -1;
    }
    format[format_length++] = '%';
    if (plus) format[format_length++] = '+';
    if (alternate) format[format_length++] = '#';
    /* stdio.h's bounded formatter has no space flag, so add it to the
     * completed positive candidate below instead of losing the sign rule. */
    if (precision_set) {
        char reversed[16];
        size_t digits = 0u;
        unsigned int rest = precision;
        format[format_length++] = '.';
        do {
            reversed[digits++] = (char)('0' + rest % 10u);
            rest /= 10u;
        } while (rest != 0u);
        while (digits != 0u)
            format[format_length++] = reversed[--digits];
    }
    if (format_length + 2u >= sizeof(format)) {
        errno = EOVERFLOW;
        return -1;
    }
    if (long_double) format[format_length++] = 'L';
    format[format_length++] = (char)conversion;
    format[format_length] = '\0';
    if (long_double)
        result = snprintf(candidate, sizeof(candidate), format, value);
    else
        result = snprintf(candidate, sizeof(candidate), format, (double)value);
    if (result < 0 || (size_t)result >= sizeof(candidate) - 1u) {
        errno = EOVERFLOW;
        return -1;
    }
    if (space && candidate[0] != '-' && candidate[0] != '+') {
        if ((size_t)result + 1u >= sizeof(candidate)) {
            errno = EOVERFLOW;
            return -1;
        }
        for (index = (size_t)result; index != 0u; --index)
            candidate[index] = candidate[index - 1u];
        candidate[0] = ' ';
        ++result;
    }
    buffer.output = wide;
    buffer.capacity = sizeof(wide) / sizeof(wide[0]);
    buffer.length = 0u;
    if (rin_wprintf_narrow_string(rin_wprintf_buffer_emit, &buffer,
                                  candidate) != 0)
        return -1;
    length = buffer.length;
    if ((size_t)width <= length) {
        for (index = 0u; index < length; ++index)
            if (emit(context, wide[index]) != 0) return -1;
        return 0;
    }
    padding = (size_t)width - length;
    if (left) {
        for (index = 0u; index < length; ++index)
            if (emit(context, wide[index]) != 0) return -1;
        while (padding-- != 0u)
            if (emit(context, L' ') != 0) return -1;
        return 0;
    }
    if (zero && (wide[0] == L'-' || wide[0] == L'+' || wide[0] == L' ')) {
        if (emit(context, wide[0]) != 0) return -1;
        for (index = 0u; index < padding; ++index)
            if (emit(context, L'0') != 0) return -1;
        for (index = 1u; index < length; ++index)
            if (emit(context, wide[index]) != 0) return -1;
        return 0;
    }
    if (zero) {
        for (index = 0u; index < padding; ++index)
            if (emit(context, L'0') != 0) return -1;
    } else {
        for (index = 0u; index < padding; ++index)
            if (emit(context, L' ') != 0) return -1;
    }
    for (index = 0u; index < length; ++index)
        if (emit(context, wide[index]) != 0) return -1;
    return 0;
}

static int rin_wprintf_format(RinWprintfEmit emit, void* context,
                               const wchar_t* format, va_list arguments) {
    size_t offset = 0u;
    if (!format) {
        errno = EINVAL;
        return -1;
    }
    while (offset < RIN_WPRINTF_FORMAT_LIMIT) {
        uint32_t conversion;
        int length = 0;
        int left = 0;
        int zero = 0;
        int plus = 0;
        int space = 0;
        int alternate = 0;
        int precision_set = 0;
        unsigned int width = 0u;
        unsigned int precision = 0u;
        unsigned int width_digit;
        uint32_t value = (uint32_t)format[offset++];
        if (value == 0u) return 0;
        if (!rin_wchar_scalar_valid(value)) {
            errno = EILSEQ;
            return -1;
        }
        if (value != (uint32_t)'%') {
            if (emit(context, (wchar_t)value) != 0) return -1;
            continue;
        }
        if (offset == RIN_WPRINTF_FORMAT_LIMIT) {
            errno = EOVERFLOW;
            return -1;
        }
        conversion = (uint32_t)format[offset++];
        if (conversion == 0u) {
            errno = EINVAL;
            return -1;
        }
        if (!rin_wchar_scalar_valid(conversion)) {
            errno = EILSEQ;
            return -1;
        }
        while (conversion == (uint32_t)'-' || conversion == (uint32_t)'+' ||
               conversion == (uint32_t)' ' || conversion == (uint32_t)'0' ||
               conversion == (uint32_t)'#') {
            if (conversion == (uint32_t)'-') left = 1;
            else if (conversion == (uint32_t)'+') plus = 1;
            else if (conversion == (uint32_t)' ') space = 1;
            else if (conversion == (uint32_t)'#') alternate = 1;
            else zero = 1;
            if (offset == RIN_WPRINTF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return -1;
            }
            conversion = (uint32_t)format[offset++];
            if (!rin_wchar_scalar_valid(conversion)) {
                errno = EILSEQ;
                return -1;
            }
        }
        if (conversion == (uint32_t)'*') {
            int dynamic_width = va_arg(arguments, int);
            if (dynamic_width < 0) {
                /* Avoid negating INT_MIN in the signed domain. */
                left = 1;
                width = (unsigned int)(-(dynamic_width + 1));
                ++width;
            } else {
                width = (unsigned int)dynamic_width;
            }
            if (offset == RIN_WPRINTF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return -1;
            }
            conversion = (uint32_t)format[offset++];
            if (!rin_wchar_scalar_valid(conversion)) {
                errno = EILSEQ;
                return -1;
            }
        } else {
            while (conversion >= (uint32_t)'0' &&
                   conversion <= (uint32_t)'9') {
                width_digit = (unsigned int)(conversion - (uint32_t)'0');
                if (width > (UINT_MAX - width_digit) / 10u) {
                    errno = EOVERFLOW;
                    return -1;
                }
                width = width * 10u + width_digit;
                if (offset == RIN_WPRINTF_FORMAT_LIMIT) {
                    errno = EOVERFLOW;
                    return -1;
                }
                conversion = (uint32_t)format[offset++];
                if (!rin_wchar_scalar_valid(conversion)) {
                    errno = EILSEQ;
                    return -1;
                }
            }
        }
        if (conversion == (uint32_t)'.') {
            precision_set = 1;
            if (offset == RIN_WPRINTF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return -1;
            }
            conversion = (uint32_t)format[offset++];
            if (!rin_wchar_scalar_valid(conversion)) {
                errno = EILSEQ;
                return -1;
            }
            if (conversion == (uint32_t)'*') {
                int dynamic_precision = va_arg(arguments, int);
                /* A negative dynamic precision is the standard's omitted
                 * precision, so it must not disable the conversion. */
                if (dynamic_precision < 0) {
                    precision_set = 0;
                    precision = 0u;
                } else {
                    precision = (unsigned int)dynamic_precision;
                }
                if (offset == RIN_WPRINTF_FORMAT_LIMIT) {
                    errno = EOVERFLOW;
                    return -1;
                }
                conversion = (uint32_t)format[offset++];
                if (!rin_wchar_scalar_valid(conversion)) {
                    errno = EILSEQ;
                    return -1;
                }
            } else {
                while (conversion >= (uint32_t)'0' &&
                       conversion <= (uint32_t)'9') {
                    width_digit = (unsigned int)(conversion - (uint32_t)'0');
                    if (precision > (UINT_MAX - width_digit) / 10u) {
                        errno = EOVERFLOW;
                        return -1;
                    }
                    precision = precision * 10u + width_digit;
                    if (offset == RIN_WPRINTF_FORMAT_LIMIT) {
                        errno = EOVERFLOW;
                        return -1;
                    }
                    conversion = (uint32_t)format[offset++];
                    if (!rin_wchar_scalar_valid(conversion)) {
                        errno = EILSEQ;
                        return -1;
                    }
                }
            }
        }
        if (conversion == (uint32_t)'l') {
            length = 1;
            if (offset == RIN_WPRINTF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return -1;
            }
            conversion = (uint32_t)format[offset++];
            if (conversion == (uint32_t)'l') {
                length = 2;
                if (offset == RIN_WPRINTF_FORMAT_LIMIT) {
                    errno = EOVERFLOW;
                    return -1;
                }
                conversion = (uint32_t)format[offset++];
            }
            if (!rin_wchar_scalar_valid(conversion)) {
                errno = EILSEQ;
                return -1;
            }
        } else if (conversion == (uint32_t)'z') {
            length = 3;
            if (offset == RIN_WPRINTF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return -1;
            }
            conversion = (uint32_t)format[offset++];
            if (!rin_wchar_scalar_valid(conversion)) {
                errno = EILSEQ;
                return -1;
            }
        } else if (conversion == (uint32_t)'L') {
            length = 4;
            if (offset == RIN_WPRINTF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return -1;
            }
            conversion = (uint32_t)format[offset++];
            if (!rin_wchar_scalar_valid(conversion)) {
                errno = EILSEQ;
                return -1;
            }
        }
        switch (conversion) {
            case '%':
                if (length != 0 || width != 0u || precision_set || left ||
                    zero || plus || space || alternate)
                    break;
                if (emit(context, L'%') != 0) return -1;
                continue;
            case 'c':
                if (precision_set || zero || plus || space || alternate) break;
                if (length == 0) {
                    if (rin_wprintf_text_candidate(
                            emit, context, 2,
                            (const void*)(uintptr_t)(unsigned int)
                                va_arg(arguments, int),
                            width == 0u ? 1u : width, left, 0u, 0) != 0)
                        return -1;
                } else if (length == 1) {
                    if (rin_wprintf_text_candidate(
                            emit, context, 3,
                            (const void*)(uintptr_t)(unsigned int)
                                va_arg(arguments, int),
                            width == 0u ? 1u : width, left, 0u, 0) != 0)
                        return -1;
                } else {
                    break;
                }
                continue;
            case 's':
                if (zero || plus || space || alternate) break;
                if (length == 0) {
                    if (rin_wprintf_text_candidate(
                            emit, context, 0, va_arg(arguments, const char*),
                            width, left, precision, precision_set) != 0)
                        return -1;
                } else if (length == 1) {
                    if (rin_wprintf_text_candidate(
                            emit, context, 1, va_arg(arguments, const wchar_t*),
                            width, left, precision, precision_set) != 0)
                        return -1;
                } else {
                    break;
                }
                continue;
            case 'd': case 'i': {
                long long signed_value;
                if (alternate) break;
                if (length == 0) signed_value = (long long)va_arg(arguments, int);
                else if (length == 1) signed_value = (long long)va_arg(arguments, long);
                else if (length == 3) signed_value = (long long)va_arg(arguments, ptrdiff_t);
                else signed_value = va_arg(arguments, long long);
                if (rin_wprintf_integer_formatted(
                        emit, context, signed_value, 0u, 1, 10u, 0, width,
                        left, zero, plus, space, 0, precision, precision_set) != 0)
                    return -1;
                continue;
            }
            case 'u': case 'o': case 'x': case 'X': {
                unsigned long long unsigned_value;
                unsigned int base = conversion == (uint32_t)'o' ? 8u :
                                    conversion == (uint32_t)'u' ? 10u : 16u;
                int uppercase = conversion == (uint32_t)'X';
                if (plus || space) break;
                if (length == 0) unsigned_value = (unsigned long long)va_arg(arguments, unsigned int);
                else if (length == 1) unsigned_value = (unsigned long long)va_arg(arguments, unsigned long);
                else if (length == 3) unsigned_value = (unsigned long long)va_arg(arguments, size_t);
                else unsigned_value = va_arg(arguments, unsigned long long);
                if (rin_wprintf_integer_formatted(
                        emit, context, 0, unsigned_value, 0, base, uppercase,
                        width, left, zero, 0, 0, alternate, precision,
                        precision_set) != 0)
                    return -1;
                continue;
            }
            case 'a': case 'A':
                if (length == 4) {
                    if (rin_wprintf_hex_long_double_formatted(
                            emit, context, va_arg(arguments, long double),
                            conversion == (uint32_t)'A', width, left, zero,
                            plus, space, alternate, precision,
                            precision_set) != 0)
                        return -1;
                } else if (rin_wprintf_hex_float_formatted(
                               emit, context, va_arg(arguments, double),
                               conversion == (uint32_t)'A', width, left, zero,
                               plus, space, alternate, precision,
                               precision_set) != 0)
                    return -1;
                continue;
            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
                long double floating_value;
                if (length == 2 || length == 3)
                    break;
                if (length == 4)
                    floating_value = va_arg(arguments, long double);
                else
                    floating_value = (long double)va_arg(arguments, double);
                if (rin_wprintf_float_formatted(
                        emit, context, floating_value, conversion,
                        length == 4, width, left, zero, plus, space, alternate,
                        precision, precision_set) != 0)
                    return -1;
                continue;
            }
            case 'p':
                if (length != 0 || precision_set || plus || space || alternate)
                    break;
                if (rin_wprintf_pointer_formatted(
                        emit, context, va_arg(arguments, void*), width, left,
                        zero) != 0)
                    return -1;
                continue;
            default:
                break;
        }
        errno = EINVAL;
        return -1;
    }
    errno = EOVERFLOW;
    return -1;
}

static int _rin_vfwprintf_unlocked(FILE* stream, const wchar_t* format, va_list arguments) {
    RinWprintfCounter counter = {0u};
    RinWprintfStream output;
    va_list copied;
    int saved_errno = errno;
    if (!stream || !format) {
        errno = EINVAL;
        return -1;
    }
    va_copy(copied, arguments);
    if (rin_wprintf_format(rin_wprintf_count_emit, &counter, format,
                           copied) != 0) {
        va_end(copied);
        return -1;
    }
    va_end(copied);
    if (counter.length > (size_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    if (!_rin_stdio_claim_wide_orientation(stream)) return -1;
    output.stream = stream;
    va_copy(copied, arguments);
    if (rin_wprintf_format(rin_wprintf_stream_emit, &output, format,
                           copied) != 0) {
        va_end(copied);
        return -1;
    }
    va_end(copied);
    errno = saved_errno;
    return (int)counter.length;
}

int vfwprintf(FILE* stream, const wchar_t* format, va_list arguments) {
    int result;
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    flockfile(stream);
    result = _rin_vfwprintf_unlocked(stream, format, arguments);
    funlockfile(stream);
    return result;
}

int fwprintf(FILE* stream, const wchar_t* format, ...) {
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = vfwprintf(stream, format, arguments);
    va_end(arguments);
    return result;
}

int vswprintf(wchar_t* destination, size_t maximum,
              const wchar_t* format, va_list arguments) {
    RinWprintfCounter counter = {0u};
    RinWprintfBuffer output;
    va_list copied;
    int saved_errno = errno;
    if (!destination || maximum == 0u || !format) {
        errno = EINVAL;
        return -1;
    }
    va_copy(copied, arguments);
    if (rin_wprintf_format(rin_wprintf_count_emit, &counter, format,
                            copied) != 0) {
        va_end(copied);
        return -1;
    }
    va_end(copied);
    if (counter.length > (size_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    if (counter.length >= maximum) {
        errno = EOVERFLOW;
        return -1;
    }
    output.output = destination;
    output.capacity = maximum - 1u;
    output.length = 0u;
    va_copy(copied, arguments);
    if (rin_wprintf_format(rin_wprintf_buffer_emit, &output, format,
                            copied) != 0 || output.length != counter.length) {
        va_end(copied);
        if (errno == 0) errno = EIO;
        return -1;
    }
    va_end(copied);
    destination[output.length] = L'\0';
    errno = saved_errno;
    return (int)output.length;
}

int swprintf(wchar_t* destination, size_t maximum,
             const wchar_t* format, ...) {
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = vswprintf(destination, maximum, format, arguments);
    va_end(arguments);
    return result;
}

int vwprintf(const wchar_t* format, va_list arguments) {
    return vfwprintf(stdout, format, arguments);
}

int wprintf(const wchar_t* format, ...) {
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = vwprintf(format, arguments);
    va_end(arguments);
    return result;
}

static int _rin_fwide_unlocked(FILE* stream, int mode) {
    if (!stream) return 0;
    if (stream->orientation == 0) {
        _rin_stdio_wide_pushback_clear(stream);
        if (mode > 0) stream->orientation = 1;
        else if (mode < 0) stream->orientation = -1;
    }
    return stream->orientation;
}

static wint_t _rin_fgetwc_unlocked(FILE* stream) {
    rin_unicode_mbstate_t state = {0u, 0u};
    uint32_t codepoint = 0u;
    unsigned int byte_index;
    if (!stream) {
        errno = EINVAL;
        return WEOF;
    }
    if (!_rin_stdio_claim_wide_orientation(stream)) return WEOF;
    {
        uint32_t value;
        if (_rin_stdio_wide_pushback_pop(stream, &value)) {
            stream->eof = 0;
            return (wint_t)value;
        }
    }
    for (byte_index = 0u; byte_index < 4u; ++byte_index) {
        char byte = 0;
        long count = _rin_stdio_read_block(
            stream, (unsigned char*)&byte, 1u);
        size_t conversion;
        if (count < 0) {
            stream->error = 1;
            return WEOF;
        }
        if (count == 0) {
            stream->eof = 1;
            if (byte_index != 0u) {
                errno = EILSEQ;
                stream->error = 1;
            }
            return WEOF;
        }
        if (count != 1) {
            errno = EIO;
            stream->error = 1;
            return WEOF;
        }
        conversion = rin_unicode_mbrtowc32(&codepoint, &byte, 1u, &state);
        if (conversion == (size_t)-1) {
            errno = EILSEQ;
            stream->error = 1;
            return WEOF;
        }
        if (conversion != (size_t)-2) return (wint_t)codepoint;
    }
    errno = EILSEQ;
    stream->error = 1;
    return WEOF;
}

int fwide(FILE* stream, int mode) {
    int result;
    if (!stream) return 0;
    flockfile(stream);
    result = _rin_fwide_unlocked(stream, mode);
    funlockfile(stream);
    return result;
}

wint_t fgetwc(FILE* stream) {
    wint_t result;
    if (!stream) {
        errno = EINVAL;
        return WEOF;
    }
    flockfile(stream);
    result = _rin_fgetwc_unlocked(stream);
    funlockfile(stream);
    return result;
}

static wchar_t* _rin_fgetws_unlocked(wchar_t* s, int n, FILE* stream) {
    int index = 0;
    if (!s || !stream || n <= 0) {
        errno = EINVAL;
        if (stream) stream->error = 1;
        return NULL;
    }
    if (!_rin_stdio_claim_wide_orientation(stream)) return NULL;
    while (index < n - 1) {
        wint_t value = _rin_fgetwc_unlocked(stream);
        if (value == WEOF) break;
        s[index++] = (wchar_t)value;
        if (value == (wint_t)'\n') break;
    }
    if (index == 0 && n > 1 && (stream->eof || stream->error)) return NULL;
    s[index] = L'\0';
    return s;
}

wchar_t* fgetws(wchar_t* s, int n, FILE* stream) {
    wchar_t* result;
    if (!stream) {
        errno = EINVAL;
        return NULL;
    }
    flockfile(stream);
    result = _rin_fgetws_unlocked(s, n, stream);
    funlockfile(stream);
    return result;
}

static wint_t _rin_fputwc_unlocked(wchar_t c, FILE* stream) {
    rin_unicode_mbstate_t state = {0u, 0u};
    char encoded[4];
    size_t length;
    int failed = 0;
    uint32_t value = (uint32_t)c;
    if (!stream) {
        errno = EINVAL;
        return WEOF;
    }
    if (!_rin_stdio_claim_wide_orientation(stream)) return WEOF;
    if (!rin_wchar_scalar_valid(value)) {
        errno = EILSEQ;
        stream->error = 1;
        return WEOF;
    }
    length = rin_unicode_wcrtomb32(encoded, value, &state);
    if (length == (size_t)-1 || length == 0u || length > sizeof(encoded)) {
        errno = EILSEQ;
        stream->error = 1;
        return WEOF;
    }
    if (_rin_stdio_write_buffered(stream, encoded, length, &failed) != length ||
        failed)
        return WEOF;
    return (wint_t)value;
}

wint_t fputwc(wchar_t c, FILE* stream) {
    wint_t result;
    if (!stream) {
        errno = EINVAL;
        return WEOF;
    }
    flockfile(stream);
    result = _rin_fputwc_unlocked(c, stream);
    funlockfile(stream);
    return result;
}

static int _rin_fputws_unlocked(const wchar_t* s, FILE* stream) {
    const wchar_t* cursor;
    if (!s || !stream) {
        errno = EINVAL;
        if (stream) stream->error = 1;
        return -1;
    }
    if (!_rin_stdio_claim_wide_orientation(stream)) return -1;
    for (cursor = s; *cursor != L'\0'; ++cursor) {
        if (!rin_wchar_scalar_valid((uint32_t)*cursor)) {
            errno = EILSEQ;
            stream->error = 1;
            return -1;
        }
    }
    for (cursor = s; *cursor != L'\0'; ++cursor) {
        if (_rin_fputwc_unlocked(*cursor, stream) == WEOF) return -1;
    }
    return 0;
}

int fputws(const wchar_t* s, FILE* stream) {
    int result;
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    flockfile(stream);
    result = _rin_fputws_unlocked(s, stream);
    funlockfile(stream);
    return result;
}

wint_t getwc(FILE* stream) { return fgetwc(stream); }
wint_t getwchar(void) { return fgetwc(stdin); }
wint_t putwc(wchar_t c, FILE* stream) { return fputwc(c, stream); }
wint_t putwchar(wchar_t c) { return fputwc(c, stdout); }

static wint_t _rin_ungetwc_unlocked(wint_t c, FILE* stream) {
    uint32_t value = (uint32_t)c;
    if (!stream || c == WEOF || !rin_wchar_scalar_valid(value)) return WEOF;
    if (!_rin_stdio_claim_wide_orientation(stream) ||
        !_rin_stdio_wide_pushback_push(stream, value)) return WEOF;
    stream->eof = 0;
    return c;
}

wint_t ungetwc(wint_t c, FILE* stream) {
    wint_t result;
    if (!stream) return WEOF;
    flockfile(stream);
    result = _rin_ungetwc_unlocked(c, stream);
    funlockfile(stream);
    return result;
}

typedef struct {
    const wchar_t* text;
    size_t offset;
} RinWscanfString;

typedef struct {
    FILE* stream;
} RinWscanfStream;

typedef struct {
    int (*read)(void* context, uint32_t* value);
    int (*unread)(void* context, uint32_t value);
    void* context;
    size_t consumed;
    int string_backed;
} RinWscanfInput;

typedef struct {
    const wchar_t* format;
    size_t first;
    size_t last;
    int inverted;
} RinWscanfScanSet;

enum {
    RIN_WSCANF_MATCH = 1,
    RIN_WSCANF_NOMATCH = 0,
    RIN_WSCANF_EOF = -1,
    RIN_WSCANF_ERROR = -2,
    RIN_WSCANF_WIDTH = 2
};

enum {
    RIN_WSCANF_LENGTH_DEFAULT,
    RIN_WSCANF_LENGTH_LONG,
    RIN_WSCANF_LENGTH_LONG_LONG,
    RIN_WSCANF_LENGTH_SIZE,
    RIN_WSCANF_LENGTH_PTRDIFF,
    RIN_WSCANF_LENGTH_SHORT,
    RIN_WSCANF_LENGTH_CHAR,
    RIN_WSCANF_LENGTH_INTMAX,
    RIN_WSCANF_LENGTH_LONG_DOUBLE
};

static int rin_wscanf_space(uint32_t value) {
    return value == (uint32_t)' ' || value == (uint32_t)'\t' ||
           value == (uint32_t)'\n' || value == (uint32_t)'\r' ||
           value == (uint32_t)'\f' || value == (uint32_t)'\v';
}

static int rin_wscanf_string_read(void* context, uint32_t* value) {
    RinWscanfString* input = (RinWscanfString*)context;
    uint32_t candidate = (uint32_t)input->text[input->offset];
    if (candidate == 0u) return RIN_WSCANF_EOF;
    if (!rin_wchar_scalar_valid(candidate)) {
        errno = EILSEQ;
        return RIN_WSCANF_ERROR;
    }
    ++input->offset;
    *value = candidate;
    return RIN_WSCANF_MATCH;
}

static int rin_wscanf_string_unread(void* context, uint32_t value) {
    RinWscanfString* input = (RinWscanfString*)context;
    if (input->offset == 0u ||
        (uint32_t)input->text[input->offset - 1u] != value) {
        errno = EIO;
        return RIN_WSCANF_ERROR;
    }
    --input->offset;
    return RIN_WSCANF_MATCH;
}

static int rin_wscanf_stream_read(void* context, uint32_t* value) {
    RinWscanfStream* input = (RinWscanfStream*)context;
    wint_t candidate = RIN_WSCANF_GETWC(input->stream);
    if (candidate == WEOF) {
        return input->stream->error ? RIN_WSCANF_ERROR : RIN_WSCANF_EOF;
    }
    if (!rin_wchar_scalar_valid((uint32_t)candidate)) {
        errno = EILSEQ;
        input->stream->error = 1;
        return RIN_WSCANF_ERROR;
    }
    *value = (uint32_t)candidate;
    return RIN_WSCANF_MATCH;
}

static int rin_wscanf_stream_unread(void* context, uint32_t value) {
    RinWscanfStream* input = (RinWscanfStream*)context;
    if (RIN_WSCANF_UNGETWC((wint_t)value, input->stream) == WEOF) {
        if (errno == 0) errno = EIO;
        input->stream->error = 1;
        return RIN_WSCANF_ERROR;
    }
    return RIN_WSCANF_MATCH;
}

/* Keep the conversion count independent from implementation-level lookahead.
 * A character returned to the input is not part of the externally consumed
 * sequence and therefore must not be reported through %n. */
static int rin_wscanf_read(RinWscanfInput* input, uint32_t* value) {
    int status = input->read(input->context, value);
    if (status == RIN_WSCANF_MATCH) ++input->consumed;
    return status;
}

static int rin_wscanf_unread(RinWscanfInput* input, uint32_t value) {
    int status = input->unread(input->context, value);
    if (status == RIN_WSCANF_MATCH && input->consumed != 0u)
        --input->consumed;
    return status;
}

static int rin_wscanf_skip_space(RinWscanfInput* input) {
    for (;;) {
        uint32_t value;
        int status = rin_wscanf_read(input, &value);
        if (status != RIN_WSCANF_MATCH) return status;
        if (!rin_wscanf_space(value)) return rin_wscanf_unread(input, value);
    }
}

static int rin_wscanf_take(RinWscanfInput* input, unsigned int width,
                           unsigned int* used, uint32_t* value) {
    int status;
    if (*used >= width) return RIN_WSCANF_WIDTH;
    status = rin_wscanf_read(input, value);
    if (status == RIN_WSCANF_MATCH) ++*used;
    return status;
}

static int rin_wscanf_digit(uint32_t value, unsigned int base,
                            unsigned int* digit) {
    unsigned int candidate;
    if (value >= (uint32_t)'0' && value <= (uint32_t)'9') {
        candidate = (unsigned int)(value - (uint32_t)'0');
    } else if (value >= (uint32_t)'a' && value <= (uint32_t)'f') {
        candidate = (unsigned int)(value - (uint32_t)'a') + 10u;
    } else if (value >= (uint32_t)'A' && value <= (uint32_t)'F') {
        candidate = (unsigned int)(value - (uint32_t)'A') + 10u;
    } else {
        return 0;
    }
    if (candidate >= base) return 0;
    *digit = candidate;
    return 1;
}

static int rin_wscanf_number(RinWscanfInput* input, unsigned int width,
                             uint32_t conversion, int* negative,
                             unsigned long long* magnitude) {
    uint32_t value;
    unsigned int base;
    unsigned int used = 0u;
    unsigned int digits = 0u;
    int status;
    int have_value = 0;

    *negative = 0;
    *magnitude = 0u;
    status = rin_wscanf_take(input, width, &used, &value);
    if (status != RIN_WSCANF_MATCH) return status == RIN_WSCANF_WIDTH ?
        RIN_WSCANF_NOMATCH : status;
    if (value == (uint32_t)'+' || value == (uint32_t)'-') {
        *negative = value == (uint32_t)'-';
        status = rin_wscanf_take(input, width, &used, &value);
        if (status != RIN_WSCANF_MATCH) return status == RIN_WSCANF_WIDTH ?
            RIN_WSCANF_NOMATCH : status;
    }

    base = conversion == (uint32_t)'o' ? 8u :
           (conversion == (uint32_t)'x' || conversion == (uint32_t)'X') ?
           16u : 10u;
    if ((conversion == (uint32_t)'i' || base == 16u) &&
        value == (uint32_t)'0') {
        *magnitude = 0u;
        digits = 1u;
        if (conversion == (uint32_t)'i') base = 8u;
        status = rin_wscanf_take(input, width, &used, &value);
        if (status == RIN_WSCANF_MATCH &&
            (value == (uint32_t)'x' || value == (uint32_t)'X')) {
            unsigned int prefix_digit;
            base = 16u;
            digits = 0u;
            status = rin_wscanf_take(input, width, &used, &value);
            if (status == RIN_WSCANF_MATCH &&
                rin_wscanf_digit(value, base, &prefix_digit)) {
                have_value = 1;
            } else if (status == RIN_WSCANF_WIDTH ||
                       status == RIN_WSCANF_EOF ||
                       (status == RIN_WSCANF_MATCH &&
                        !rin_wscanf_digit(value, base, &prefix_digit))) {
                /* A hexadecimal introducer is only part of the conversion
                 * once a hexadecimal digit follows it.  Keep the valid
                 * leading zero and return the rejected `x` (and lookahead)
                 * so `%x` follows the same non-destructive token boundary
                 * rule as the other integer conversions. */
                if (status == RIN_WSCANF_MATCH &&
                    rin_wscanf_unread(input, value) != RIN_WSCANF_MATCH)
                    return RIN_WSCANF_ERROR;
                if (rin_wscanf_unread(input, (uint32_t)'x') != RIN_WSCANF_MATCH)
                    return RIN_WSCANF_ERROR;
                return RIN_WSCANF_MATCH;
            } else {
                return status;
            }
        } else if (status == RIN_WSCANF_MATCH) {
            have_value = 1;
        } else {
            return RIN_WSCANF_MATCH;
        }
    } else {
        have_value = 1;
    }

    while (have_value) {
        unsigned int digit;
        have_value = 0;
        if (!rin_wscanf_digit(value, base, &digit)) {
            if (rin_wscanf_unread(input, value) != RIN_WSCANF_MATCH)
                return RIN_WSCANF_ERROR;
            return digits == 0u ? RIN_WSCANF_NOMATCH : RIN_WSCANF_MATCH;
        }
        if (*magnitude > (ULLONG_MAX - (unsigned long long)digit) /
                         (unsigned long long)base) {
            errno = ERANGE;
            return RIN_WSCANF_ERROR;
        }
        *magnitude = *magnitude * (unsigned long long)base +
                     (unsigned long long)digit;
        ++digits;
        status = rin_wscanf_take(input, width, &used, &value);
        if (status == RIN_WSCANF_WIDTH) return RIN_WSCANF_MATCH;
        if (status == RIN_WSCANF_EOF) return RIN_WSCANF_MATCH;
        if (status != RIN_WSCANF_MATCH) return status;
        have_value = 1;
    }
    return digits == 0u ? RIN_WSCANF_NOMATCH : RIN_WSCANF_MATCH;
}

static int rin_wscanf_store_number(uint32_t conversion, int length,
                                   int negative, unsigned long long magnitude,
                                   va_list* arguments) {
    if (conversion == (uint32_t)'d' || conversion == (uint32_t)'i') {
        const unsigned long long signed_limit =
            (unsigned long long)LLONG_MAX + 1ULL;
        long long value;
        if (magnitude > signed_limit) {
            errno = ERANGE;
            return RIN_WSCANF_ERROR;
        }
        if (negative) {
            value = magnitude == signed_limit ? LLONG_MIN :
                -(long long)magnitude;
        } else {
            if (magnitude > (unsigned long long)LLONG_MAX) {
                errno = ERANGE;
                return RIN_WSCANF_ERROR;
            }
            value = (long long)magnitude;
        }
        if (length == RIN_WSCANF_LENGTH_DEFAULT) {
            int* output = va_arg(*arguments, int*);
            if (!output || value < (long long)INT_MIN || value > (long long)INT_MAX) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = (int)value;
        } else if (length == RIN_WSCANF_LENGTH_SHORT) {
            short* output = va_arg(*arguments, short*);
            if (!output || value < (long long)SHRT_MIN ||
                value > (long long)SHRT_MAX) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = (short)value;
        } else if (length == RIN_WSCANF_LENGTH_CHAR) {
            signed char* output = va_arg(*arguments, signed char*);
            if (!output || value < (long long)SCHAR_MIN ||
                value > (long long)SCHAR_MAX) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = (signed char)value;
        } else if (length == RIN_WSCANF_LENGTH_LONG) {
            long* output = va_arg(*arguments, long*);
            if (!output || value < (long long)LONG_MIN ||
                value > (long long)LONG_MAX) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = (long)value;
        } else if (length == RIN_WSCANF_LENGTH_LONG_LONG) {
            long long* output = va_arg(*arguments, long long*);
            if (!output) {
                errno = EFAULT;
                return RIN_WSCANF_ERROR;
            }
            *output = value;
        } else if (length == RIN_WSCANF_LENGTH_INTMAX) {
            intmax_t* output = va_arg(*arguments, intmax_t*);
            if (!output || (!negative &&
                            magnitude > (unsigned long long)INTMAX_MAX) ||
                (negative && magnitude >
                 (unsigned long long)INTMAX_MAX + 1ULL)) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = (intmax_t)value;
        } else {
            ptrdiff_t* output = va_arg(*arguments, ptrdiff_t*);
            if (!output || value < (long long)PTRDIFF_MIN ||
                value > (long long)PTRDIFF_MAX) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = (ptrdiff_t)value;
        }
        return RIN_WSCANF_MATCH;
    }

    {
        if (length == RIN_WSCANF_LENGTH_DEFAULT) {
            unsigned int* output = va_arg(*arguments, unsigned int*);
            if (!output || (!negative && magnitude > (unsigned long long)UINT_MAX)) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = negative ? 0u - (unsigned int)magnitude :
                (unsigned int)magnitude;
        } else if (length == RIN_WSCANF_LENGTH_SHORT) {
            unsigned short* output = va_arg(*arguments, unsigned short*);
            if (!output || (!negative &&
                            magnitude > (unsigned long long)USHRT_MAX)) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = negative ? (unsigned short)(0u - (unsigned short)magnitude) :
                (unsigned short)magnitude;
        } else if (length == RIN_WSCANF_LENGTH_CHAR) {
            unsigned char* output = va_arg(*arguments, unsigned char*);
            if (!output || (!negative &&
                            magnitude > (unsigned long long)UCHAR_MAX)) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = negative ? (unsigned char)(0u - (unsigned char)magnitude) :
                (unsigned char)magnitude;
        } else if (length == RIN_WSCANF_LENGTH_LONG) {
            unsigned long* output = va_arg(*arguments, unsigned long*);
            if (!output || (!negative &&
                            magnitude > (unsigned long long)ULONG_MAX)) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = negative ? 0UL - (unsigned long)magnitude :
                (unsigned long)magnitude;
        } else if (length == RIN_WSCANF_LENGTH_LONG_LONG) {
            unsigned long long* output = va_arg(*arguments, unsigned long long*);
            if (!output) {
                errno = EFAULT;
                return RIN_WSCANF_ERROR;
            }
            *output = negative ? 0ULL - magnitude : magnitude;
        } else if (length == RIN_WSCANF_LENGTH_INTMAX) {
            uintmax_t* output = va_arg(*arguments, uintmax_t*);
            if (!output || (!negative &&
                            magnitude > (unsigned long long)UINTMAX_MAX)) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = negative ? (uintmax_t)0 - (uintmax_t)magnitude :
                (uintmax_t)magnitude;
        } else {
            size_t* output = va_arg(*arguments, size_t*);
            if (!output || (!negative && magnitude > (unsigned long long)SIZE_MAX)) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = negative ? (size_t)0 - (size_t)magnitude :
                (size_t)magnitude;
        }
    }
    return RIN_WSCANF_MATCH;
}

/* `%p` has a pointer destination rather than an integer length family.  Keep
 * the parsed magnitude private until both the sign and target-width checks
 * pass so a malformed or out-of-range token cannot partially publish a
 * pointer. */
static int rin_wscanf_store_pointer(int negative,
                                    unsigned long long magnitude,
                                    va_list* arguments) {
    void** output;
    if (negative || magnitude > (unsigned long long)UINTPTR_MAX) {
        errno = ERANGE;
        return RIN_WSCANF_ERROR;
    }
    output = va_arg(*arguments, void**);
    if (!output) {
        errno = EFAULT;
        return RIN_WSCANF_ERROR;
    }
    *output = (void*)(uintptr_t)magnitude;
    return RIN_WSCANF_MATCH;
}

/* Collect a bounded ASCII candidate from either a wide string or a stream.
 * The stream path restores the unconsumed suffix (or the complete rejected
 * candidate) through FILE's bounded wide pushback owner, keeping conversion
 * and destination writes failure-atomic. */
static int rin_wscanf_string_float(unsigned int width, int length,
                                   uint32_t conversion,
                                   int suppress, RinWscanfInput* input,
                                   va_list* arguments) {
    struct RinSscanfSpec spec = {};
    struct RinSscanfFloat value;
    char* candidate;
    unsigned int count = 0u;
    size_t parsed_offset = 0u;
    int input_failure = 0;
    int parsed;

    if (length != RIN_WSCANF_LENGTH_DEFAULT &&
        length != RIN_WSCANF_LENGTH_LONG &&
        length != RIN_WSCANF_LENGTH_LONG_DOUBLE) {
        errno = ENOSYS;
        return RIN_WSCANF_ERROR;
    }
    if (width == 0u || width > RIN_WSCANF_FIELD_LIMIT)
        width = RIN_WSCANF_FIELD_LIMIT;
    candidate = (char*)malloc((size_t)width + 1u);
    if (!candidate) {
        errno = ENOMEM;
        return RIN_WSCANF_ERROR;
    }
    while (count < width) {
        uint32_t current;
        int status = rin_wscanf_read(input, &current);
        if (status == RIN_WSCANF_EOF) break;
        if (status != RIN_WSCANF_MATCH) {
            free(candidate);
            return status;
        }
        if (rin_wscanf_space(current)) {
            if (rin_wscanf_unread(input, current) != RIN_WSCANF_MATCH) {
                free(candidate);
                return RIN_WSCANF_ERROR;
            }
            break;
        }
        if (current > 0x7fu) {
            free(candidate);
            errno = EILSEQ;
            return RIN_WSCANF_ERROR;
        }
        candidate[count++] = (char)current;
    }
    candidate[count] = '\0';
    spec.length = length == RIN_WSCANF_LENGTH_LONG
        ? RIN_SSCANF_LENGTH_LONG
        : length == RIN_WSCANF_LENGTH_LONG_DOUBLE
            ? RIN_SSCANF_LENGTH_LONG_DOUBLE : RIN_SSCANF_LENGTH_NONE;
    spec.conversion = (char)conversion;
    spec.has_width = 1;
    spec.width = count;
    parsed = rin_sscanf_parse_float(candidate, count, &parsed_offset, &spec,
                                    &value, &input_failure);
    if (parsed == 0) {
        while (count != 0u) {
            --count;
            if (rin_wscanf_unread(input, (uint32_t)(unsigned char)candidate[count]) !=
                    RIN_WSCANF_MATCH) {
                free(candidate);
                return RIN_WSCANF_ERROR;
            }
        }
        free(candidate);
        return input_failure ? RIN_WSCANF_ERROR : RIN_WSCANF_NOMATCH;
    }
    if (parsed < 0) {
        free(candidate);
        return RIN_WSCANF_ERROR;
    }
    while (count > parsed_offset) {
        --count;
        if (rin_wscanf_unread(input, (uint32_t)(unsigned char)candidate[count]) !=
                RIN_WSCANF_MATCH) {
                free(candidate);
            return RIN_WSCANF_ERROR;
        }
    }
    if (!suppress && rin_sscanf_store_float(arguments, &spec, &value) != 0) {
        free(candidate);
        return RIN_WSCANF_ERROR;
    }
    free(candidate);
    return RIN_WSCANF_MATCH;
}

static int rin_wscanf_store_count(int length, size_t count,
                                  va_list* arguments) {
    if (length == RIN_WSCANF_LENGTH_DEFAULT) {
        int* output = va_arg(*arguments, int*);
        if (!output) {
            errno = EFAULT;
            return RIN_WSCANF_ERROR;
        }
        if (count > (size_t)INT_MAX) {
            errno = EOVERFLOW;
            return RIN_WSCANF_ERROR;
        }
        *output = (int)count;
    } else if (length == RIN_WSCANF_LENGTH_SHORT) {
        short* output = va_arg(*arguments, short*);
        if (!output) {
            errno = EFAULT;
            return RIN_WSCANF_ERROR;
        }
        if (count > (size_t)SHRT_MAX) {
            errno = EOVERFLOW;
            return RIN_WSCANF_ERROR;
        }
        *output = (short)count;
    } else if (length == RIN_WSCANF_LENGTH_CHAR) {
        signed char* output = va_arg(*arguments, signed char*);
        if (!output) {
            errno = EFAULT;
            return RIN_WSCANF_ERROR;
        }
        if (count > (size_t)SCHAR_MAX) {
            errno = EOVERFLOW;
            return RIN_WSCANF_ERROR;
        }
        *output = (signed char)count;
    } else if (length == RIN_WSCANF_LENGTH_LONG) {
        long* output = va_arg(*arguments, long*);
        if (!output) {
            errno = EFAULT;
            return RIN_WSCANF_ERROR;
        }
        if (count > (size_t)LONG_MAX) {
            errno = EOVERFLOW;
            return RIN_WSCANF_ERROR;
        }
        *output = (long)count;
    } else if (length == RIN_WSCANF_LENGTH_LONG_LONG) {
        long long* output = va_arg(*arguments, long long*);
        if (!output) {
            errno = EFAULT;
            return RIN_WSCANF_ERROR;
        }
        if (count > (size_t)LLONG_MAX) {
            errno = EOVERFLOW;
            return RIN_WSCANF_ERROR;
        }
        *output = (long long)count;
    } else if (length == RIN_WSCANF_LENGTH_INTMAX) {
        intmax_t* output = va_arg(*arguments, intmax_t*);
        if (!output) {
            errno = EFAULT;
            return RIN_WSCANF_ERROR;
        }
        if (count > (size_t)INTMAX_MAX) {
            errno = EOVERFLOW;
            return RIN_WSCANF_ERROR;
        }
        *output = (intmax_t)count;
    } else if (length == RIN_WSCANF_LENGTH_SIZE) {
        size_t* output = va_arg(*arguments, size_t*);
        if (!output) {
            errno = EFAULT;
            return RIN_WSCANF_ERROR;
        }
        *output = count;
    } else {
        ptrdiff_t* output = va_arg(*arguments, ptrdiff_t*);
        if (!output) {
            errno = EFAULT;
            return RIN_WSCANF_ERROR;
        }
        if (count > (size_t)PTRDIFF_MAX) {
            errno = EOVERFLOW;
            return RIN_WSCANF_ERROR;
        }
        *output = (ptrdiff_t)count;
    }
    return RIN_WSCANF_MATCH;
}

static int rin_wscanf_store_characters(const uint32_t* candidate,
                                       unsigned int length, int string,
                                       int wide, va_list* arguments) {
    unsigned int index;
    if (wide) {
        wchar_t* output = va_arg(*arguments, wchar_t*);
        if (!output) {
            errno = EFAULT;
            return RIN_WSCANF_ERROR;
        }
        for (index = 0u; index < length; ++index)
            output[index] = (wchar_t)candidate[index];
        if (string) output[length] = L'\0';
        return RIN_WSCANF_MATCH;
    }
    {
        char* output = va_arg(*arguments, char*);
        if (!output) {
            errno = EFAULT;
            return RIN_WSCANF_ERROR;
        }
        /* RinOS' byte-oriented scanf destination is explicitly ASCII.  This
         * avoids silent UTF-8 expansion where scanf has no destination-size
         * parameter and preserves transactional destination writes. */
        for (index = 0u; index < length; ++index) {
            if (candidate[index] > 0x7Fu) {
                errno = EILSEQ;
                return RIN_WSCANF_ERROR;
            }
        }
        for (index = 0u; index < length; ++index)
            output[index] = (char)candidate[index];
        if (string) output[length] = '\0';
    }
    return RIN_WSCANF_MATCH;
}

static int rin_wscanf_characters(RinWscanfInput* input, unsigned int width,
                                 int string, int wide, int suppress,
                                 va_list* arguments) {
    uint32_t candidate[RIN_WSCANF_FIELD_LIMIT];
    unsigned int index = 0u;
    int status;
    if (string) {
        status = rin_wscanf_skip_space(input);
        if (status != RIN_WSCANF_MATCH) return status;
        while (index < width) {
            uint32_t value;
            status = rin_wscanf_read(input, &value);
            if (status == RIN_WSCANF_EOF) break;
            if (status != RIN_WSCANF_MATCH) return status;
            if (rin_wscanf_space(value)) {
                if (rin_wscanf_unread(input, value) != RIN_WSCANF_MATCH)
                    return RIN_WSCANF_ERROR;
                break;
            }
            candidate[index++] = value;
        }
        if (index == 0u) return RIN_WSCANF_NOMATCH;
        return suppress ? RIN_WSCANF_MATCH :
            rin_wscanf_store_characters(candidate, index, 1, wide, arguments);
    }

    while (index < width) {
        uint32_t value;
        status = rin_wscanf_read(input, &value);
        if (status != RIN_WSCANF_MATCH) return status;
        candidate[index++] = value;
    }
    return suppress ? RIN_WSCANF_MATCH :
        rin_wscanf_store_characters(candidate, index, 0, wide, arguments);
}

/* A scanset stays a view over the already validated format text.  This keeps
 * its grammar allocation-free and lets the field itself retain the existing
 * candidate-then-commit destination rule.  `]` is a member only in the first
 * position; `-` is a range operator only between two non-`]` scalars. */
static int rin_wscanf_scanset_parse(const wchar_t* format, size_t* offset,
                                    RinWscanfScanSet* output) {
    RinWscanfScanSet candidate;
    size_t cursor = *offset;
    if (cursor >= RIN_WSCANF_FORMAT_LIMIT) {
        errno = EOVERFLOW;
        return RIN_WSCANF_ERROR;
    }
    candidate.format = format;
    candidate.inverted = 0;
    if ((uint32_t)format[cursor] == (uint32_t)'^') {
        candidate.inverted = 1;
        ++cursor;
    }
    if (cursor >= RIN_WSCANF_FORMAT_LIMIT) {
        errno = EOVERFLOW;
        return RIN_WSCANF_ERROR;
    }
    candidate.first = cursor;
    if ((uint32_t)format[cursor] == (uint32_t)']') ++cursor;
    while (cursor < RIN_WSCANF_FORMAT_LIMIT) {
        uint32_t value = (uint32_t)format[cursor];
        if (value == 0u) {
            errno = EINVAL;
            return RIN_WSCANF_ERROR;
        }
        if (!rin_wchar_scalar_valid(value)) {
            errno = EILSEQ;
            return RIN_WSCANF_ERROR;
        }
        if (value == (uint32_t)']') {
            size_t index;
            candidate.last = cursor;
            if (candidate.first == candidate.last) {
                errno = EINVAL;
                return RIN_WSCANF_ERROR;
            }
            for (index = candidate.first; index < candidate.last;) {
                uint32_t lower = (uint32_t)format[index];
                if (lower != (uint32_t)'-' &&
                    index + 2u < candidate.last &&
                    (uint32_t)format[index + 1u] == (uint32_t)'-' &&
                    (uint32_t)format[index + 2u] != (uint32_t)'-') {
                    uint32_t upper = (uint32_t)format[index + 2u];
                    if (upper < lower) {
                        errno = EINVAL;
                        return RIN_WSCANF_ERROR;
                    }
                    index += 3u;
                } else {
                    ++index;
                }
            }
            *offset = cursor + 1u;
            *output = candidate;
            return RIN_WSCANF_MATCH;
        }
        ++cursor;
    }
    errno = EOVERFLOW;
    return RIN_WSCANF_ERROR;
}

static int rin_wscanf_scanset_contains(const RinWscanfScanSet* set,
                                       uint32_t value) {
    int contains = 0;
    size_t index;
    for (index = set->first; index < set->last;) {
        uint32_t lower = (uint32_t)set->format[index];
        if (lower != (uint32_t)'-' && index + 2u < set->last &&
            (uint32_t)set->format[index + 1u] == (uint32_t)'-' &&
            (uint32_t)set->format[index + 2u] != (uint32_t)'-') {
            uint32_t upper = (uint32_t)set->format[index + 2u];
            if (value >= lower && value <= upper) contains = 1;
            index += 3u;
        } else {
            if (value == lower) contains = 1;
            ++index;
        }
    }
    return set->inverted ? !contains : contains;
}

static int rin_wscanf_scanset(RinWscanfInput* input,
                               const RinWscanfScanSet* set,
                               unsigned int width, int wide, int suppress,
                               va_list* arguments) {
    uint32_t candidate[RIN_WSCANF_FIELD_LIMIT];
    unsigned int index = 0u;
    for (;;) {
        uint32_t value;
        int status;
        if (index == width) break;
        status = rin_wscanf_read(input, &value);
        if (status == RIN_WSCANF_EOF) break;
        if (status != RIN_WSCANF_MATCH) return status;
        if (!rin_wscanf_scanset_contains(set, value)) {
            if (rin_wscanf_unread(input, value) != RIN_WSCANF_MATCH)
                return RIN_WSCANF_ERROR;
            break;
        }
        candidate[index++] = value;
    }
    if (index == 0u) return RIN_WSCANF_NOMATCH;
    return suppress ? RIN_WSCANF_MATCH :
        rin_wscanf_store_characters(candidate, index, 1, wide, arguments);
}

static int rin_wscanf_literal(RinWscanfInput* input, uint32_t expected) {
    uint32_t value;
    int status = rin_wscanf_read(input, &value);
    if (status != RIN_WSCANF_MATCH) return status;
    if (value == expected) return RIN_WSCANF_MATCH;
    if (rin_wscanf_unread(input, value) != RIN_WSCANF_MATCH)
        return RIN_WSCANF_ERROR;
    return RIN_WSCANF_NOMATCH;
}

static int rin_wscanf_format(RinWscanfInput* input, const wchar_t* format,
                             va_list* arguments) {
    size_t offset = 0u;
    int assignments = 0;
    if (!input || !format) {
        errno = EINVAL;
        return EOF;
    }
    while (offset < RIN_WSCANF_FORMAT_LIMIT) {
        uint32_t value = (uint32_t)format[offset++];
        int status;
        if (value == 0u) return assignments;
        if (!rin_wchar_scalar_valid(value)) {
            errno = EILSEQ;
            return EOF;
        }
        if (rin_wscanf_space(value)) {
            while (offset < RIN_WSCANF_FORMAT_LIMIT &&
                   rin_wscanf_space((uint32_t)format[offset]))
                ++offset;
            status = rin_wscanf_skip_space(input);
            if (status == RIN_WSCANF_ERROR) return EOF;
            if (status == RIN_WSCANF_EOF)
                return assignments == 0 ? EOF : assignments;
            continue;
        }
        if (value != (uint32_t)'%') {
            status = rin_wscanf_literal(input, value);
            if (status == RIN_WSCANF_MATCH) continue;
            if (status == RIN_WSCANF_ERROR) return EOF;
            return status == RIN_WSCANF_EOF && assignments == 0 ? EOF : assignments;
        }

        {
            int suppress = 0;
            int produces_assignment = 1;
            int length = RIN_WSCANF_LENGTH_DEFAULT;
            int width_present = 0;
            unsigned int width = 0u;
            uint32_t conversion;
            if (offset == RIN_WSCANF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return EOF;
            }
            if ((uint32_t)format[offset] == (uint32_t)'*') {
                suppress = 1;
                ++offset;
            }
            while (offset < RIN_WSCANF_FORMAT_LIMIT &&
                   (uint32_t)format[offset] >= (uint32_t)'0' &&
                   (uint32_t)format[offset] <= (uint32_t)'9') {
                unsigned int digit = (unsigned int)((uint32_t)format[offset] -
                                                     (uint32_t)'0');
                if (width > (RIN_WSCANF_FIELD_LIMIT - digit) / 10u) {
                    errno = EOVERFLOW;
                    return EOF;
                }
                width = width * 10u + digit;
                width_present = 1;
                ++offset;
            }
            if (width_present && width == 0u) {
                errno = EINVAL;
                return EOF;
            }
            if (offset == RIN_WSCANF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return EOF;
            }
            conversion = (uint32_t)format[offset++];
            if (conversion == (uint32_t)'L') {
                length = RIN_WSCANF_LENGTH_LONG_DOUBLE;
                if (offset == RIN_WSCANF_FORMAT_LIMIT) {
                    errno = EOVERFLOW;
                    return EOF;
                }
                conversion = (uint32_t)format[offset++];
            } else if (conversion == (uint32_t)'l') {
                length = RIN_WSCANF_LENGTH_LONG;
                if (offset == RIN_WSCANF_FORMAT_LIMIT) {
                    errno = EOVERFLOW;
                    return EOF;
                }
                conversion = (uint32_t)format[offset++];
                if (conversion == (uint32_t)'l') {
                    length = RIN_WSCANF_LENGTH_LONG_LONG;
                    if (offset == RIN_WSCANF_FORMAT_LIMIT) {
                        errno = EOVERFLOW;
                        return EOF;
                    }
                    conversion = (uint32_t)format[offset++];
                }
            } else if (conversion == (uint32_t)'h') {
                length = RIN_WSCANF_LENGTH_SHORT;
                if (offset == RIN_WSCANF_FORMAT_LIMIT) {
                    errno = EOVERFLOW;
                    return EOF;
                }
                conversion = (uint32_t)format[offset++];
                if (conversion == (uint32_t)'h') {
                    length = RIN_WSCANF_LENGTH_CHAR;
                    if (offset == RIN_WSCANF_FORMAT_LIMIT) {
                        errno = EOVERFLOW;
                        return EOF;
                    }
                    conversion = (uint32_t)format[offset++];
                }
            } else if (conversion == (uint32_t)'z') {
                length = RIN_WSCANF_LENGTH_SIZE;
                if (offset == RIN_WSCANF_FORMAT_LIMIT) {
                    errno = EOVERFLOW;
                    return EOF;
                }
                conversion = (uint32_t)format[offset++];
            } else if (conversion == (uint32_t)'j') {
                length = RIN_WSCANF_LENGTH_INTMAX;
                if (offset == RIN_WSCANF_FORMAT_LIMIT) {
                    errno = EOVERFLOW;
                    return EOF;
                }
                conversion = (uint32_t)format[offset++];
            } else if (conversion == (uint32_t)'t') {
                /* ptrdiff_t's unsigned counterpart is size_t on the Rin
                 * targets, so `%td`/`%tu` use the existing signed/unsigned
                 * size branches without passing through `long`. */
                length = RIN_WSCANF_LENGTH_PTRDIFF;
                if (offset == RIN_WSCANF_FORMAT_LIMIT) {
                    errno = EOVERFLOW;
                    return EOF;
                }
                conversion = (uint32_t)format[offset++];
            }
            if (!rin_wchar_scalar_valid(conversion)) {
                errno = EILSEQ;
                return EOF;
            }
            if (length == RIN_WSCANF_LENGTH_LONG_DOUBLE &&
                conversion != (uint32_t)'a' && conversion != (uint32_t)'A' &&
                conversion != (uint32_t)'e' && conversion != (uint32_t)'E' &&
                conversion != (uint32_t)'f' && conversion != (uint32_t)'F' &&
                conversion != (uint32_t)'g' && conversion != (uint32_t)'G') {
                errno = EINVAL;
                return EOF;
            }
            if (conversion == (uint32_t)'%') {
                if (suppress || length != RIN_WSCANF_LENGTH_DEFAULT || width_present) {
                    errno = EINVAL;
                    return EOF;
                }
                produces_assignment = 0;
                status = rin_wscanf_literal(input, (uint32_t)'%');
            } else if (conversion == (uint32_t)'c' ||
                       conversion == (uint32_t)'s') {
                if (length != RIN_WSCANF_LENGTH_DEFAULT &&
                    length != RIN_WSCANF_LENGTH_LONG) {
                    errno = EINVAL;
                    return EOF;
                }
                if (!width_present) {
                    /* A width-less `%s` still needs a bounded destination
                     * contract: use the same field limit as numeric scans.
                     * The candidate is private until the complete token has
                     * been collected, so the terminator is always within the
                     * caller's advertised maximum field size. */
                    width = conversion == (uint32_t)'s' ?
                        RIN_WSCANF_FIELD_LIMIT : 1u;
                }
                status = rin_wscanf_characters(input, width,
                    conversion == (uint32_t)'s',
                    length == RIN_WSCANF_LENGTH_LONG, suppress, arguments);
            } else if (conversion == (uint32_t)'[') {
                RinWscanfScanSet set;
                if (length != RIN_WSCANF_LENGTH_DEFAULT &&
                    length != RIN_WSCANF_LENGTH_LONG) {
                    errno = EINVAL;
                    return EOF;
                }
                if (!width_present) {
                    /* `%[` has no implicit terminator in the input.  Bound
                     * the private candidate to the same 4,096-scalar field
                     * limit used by width-less `%s`; the first non-member is
                     * unread and remains available to the next conversion. */
                    width = RIN_WSCANF_FIELD_LIMIT;
                }
                status = rin_wscanf_scanset_parse(format, &offset, &set);
                if (status == RIN_WSCANF_MATCH)
                    status = rin_wscanf_scanset(input, &set, width,
                                                length == RIN_WSCANF_LENGTH_LONG,
                                                suppress, arguments);
            } else if (conversion == (uint32_t)'d' || conversion == (uint32_t)'i' ||
                       conversion == (uint32_t)'u' || conversion == (uint32_t)'o' ||
                       conversion == (uint32_t)'x' || conversion == (uint32_t)'X') {
                int negative;
                unsigned long long magnitude;
                if (!width_present) width = RIN_WSCANF_FIELD_LIMIT;
                status = rin_wscanf_skip_space(input);
                if (status == RIN_WSCANF_MATCH)
                    status = rin_wscanf_number(input, width, conversion,
                                               &negative, &magnitude);
                if (status == RIN_WSCANF_MATCH && !suppress)
                    status = rin_wscanf_store_number(conversion, length,
                                                      negative, magnitude,
                                                      arguments);
            } else if (conversion == (uint32_t)'n') {
                if (suppress || width_present) {
                    errno = ENOSYS;
                    return EOF;
                }
                produces_assignment = 0;
                status = rin_wscanf_store_count(length, input->consumed,
                                                arguments);
            } else if (conversion == (uint32_t)'p') {
                int negative;
                unsigned long long magnitude;
                if (length != RIN_WSCANF_LENGTH_DEFAULT) {
                    errno = EINVAL;
                    return EOF;
                }
                if (!width_present) width = RIN_WSCANF_FIELD_LIMIT;
                status = rin_wscanf_skip_space(input);
                if (status == RIN_WSCANF_MATCH)
                    status = rin_wscanf_number(input, width, (uint32_t)'x',
                                               &negative, &magnitude);
                if (status == RIN_WSCANF_MATCH && !suppress)
                    status = rin_wscanf_store_pointer(negative, magnitude,
                                                      arguments);
            } else if (conversion == (uint32_t)'a' || conversion == (uint32_t)'A' ||
                       conversion == (uint32_t)'e' || conversion == (uint32_t)'E' ||
                       conversion == (uint32_t)'f' || conversion == (uint32_t)'F' ||
                       conversion == (uint32_t)'g' || conversion == (uint32_t)'G') {
                if (!width_present) width = RIN_WSCANF_FIELD_LIMIT;
                status = rin_wscanf_string_float(width, length, conversion,
                                                 suppress,
                                                 input, arguments);
            } else {
                errno = EINVAL;
                return EOF;
            }
            if (status == RIN_WSCANF_MATCH) {
                if (produces_assignment && !suppress) ++assignments;
                continue;
            }
            if (status == RIN_WSCANF_ERROR) return EOF;
            return status == RIN_WSCANF_EOF && assignments == 0 ? EOF : assignments;
        }
    }
    errno = EOVERFLOW;
    return EOF;
}

int vswscanf(const wchar_t* text, const wchar_t* format, va_list arguments) {
    RinWscanfString string_input;
    RinWscanfInput input;
    va_list copied;
    int result;
    size_t index;
    if (!text || !format) {
        errno = EINVAL;
        return EOF;
    }
    /* A string-backed scan must not walk an unbounded caller buffer.  Check
     * the terminating scalar before consuming or modifying any destination. */
    for (index = 0u; index <= RIN_WSCANF_MAX_INPUT; ++index) {
        uint32_t value = (uint32_t)text[index];
        if (value == 0u) break;
        if (!rin_wchar_scalar_valid(value)) {
            errno = EILSEQ;
            return EOF;
        }
        if (index == RIN_WSCANF_MAX_INPUT) {
            errno = EOVERFLOW;
            return EOF;
        }
    }
    string_input.text = text;
    string_input.offset = 0u;
    input.read = rin_wscanf_string_read;
    input.unread = rin_wscanf_string_unread;
    input.context = &string_input;
    input.consumed = 0u;
    input.string_backed = 1;
    va_copy(copied, arguments);
    result = rin_wscanf_format(&input, format, &copied);
    va_end(copied);
    return result;
}

int swscanf(const wchar_t* text, const wchar_t* format, ...) {
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = vswscanf(text, format, arguments);
    va_end(arguments);
    return result;
}

static int _rin_vfwscanf_unlocked(FILE* stream, const wchar_t* format, va_list arguments) {
    RinWscanfStream stream_input;
    RinWscanfInput input;
    va_list copied;
    int result;
    if (!stream || !format) {
        errno = EINVAL;
        return EOF;
    }
    if (!_rin_stdio_claim_wide_orientation(stream)) return EOF;
    stream_input.stream = stream;
    input.read = rin_wscanf_stream_read;
    input.unread = rin_wscanf_stream_unread;
    input.context = &stream_input;
    input.consumed = 0u;
    input.string_backed = 0;
    va_copy(copied, arguments);
    result = rin_wscanf_format(&input, format, &copied);
    va_end(copied);
    return result;
}

int vfwscanf(FILE* stream, const wchar_t* format, va_list arguments) {
    int result;
    if (!stream) {
        errno = EINVAL;
        return EOF;
    }
    flockfile(stream);
    result = _rin_vfwscanf_unlocked(stream, format, arguments);
    funlockfile(stream);
    return result;
}

int fwscanf(FILE* stream, const wchar_t* format, ...) {
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = vfwscanf(stream, format, arguments);
    va_end(arguments);
    return result;
}

int vwscanf(const wchar_t* format, va_list arguments) {
    return vfwscanf(stdin, format, arguments);
}

int wscanf(const wchar_t* format, ...) {
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = vwscanf(format, arguments);
    va_end(arguments);
    return result;
}
