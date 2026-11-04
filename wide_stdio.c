/* SPDX-License-Identifier: MIT */

#include "wchar.h"
#include "stdio.h"
#include "stdlib.h"
#if defined(RIN_USERSPACE)
#include "stdio_internal.h"
#endif
#include "stdio_scanf_float_impl.h"
#include "float.h"
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
    RinWprintfEmit emit;
    void* context;
    size_t written;
} RinWprintfTrackedEmit;

static int rin_wprintf_tracked_emit(void* context, wchar_t value) {
    RinWprintfTrackedEmit* tracked = (RinWprintfTrackedEmit*)context;
    if (tracked->written == SIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    if (tracked->emit(tracked->context, value) != 0) return -1;
    ++tracked->written;
    return 0;
}

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
    /* Surrogate splitting keys off the actual wchar_t width, not the
     * compiler's __SIZEOF_WCHAR_T__ builtin: hosted fixtures may
     * compile this TU with a 32-bit wchar_t model on a 16-bit-wchar
     * host (and vice versa), and the emitter must match the type it
     * writes through. Folds to a constant in real builds. */
    if (sizeof(wchar_t) == 2u && value > 0xffffu) {
        uint32_t high;
        uint32_t low;
        high = 0xd800u + ((value - 0x10000u) >> 10u);
        low = 0xdc00u + ((value - 0x10000u) & 0x3ffu);
        if (emit(context, (wchar_t)high) != 0) return -1;
        return emit(context, (wchar_t)low);
    }
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
    size_t conversion =
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
        rin_locale_current_mbrtowc32(
            &value, (const char*)&byte, 1u, &state);
#else
        rin_unicode_mbrtowc32(&value, (const char*)&byte, 1u, &state);
#endif
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
        conversion =
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
            rin_locale_current_mbrtowc32(
                &value, (const char*)&byte, 1u, &state);
#else
            rin_unicode_mbrtowc32(
                &value, (const char*)&byte, 1u, &state);
#endif
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
        if (precision_set && precision == 0u && signed_value == 0)
            buffer.length = 0u;
    } else if (rin_wprintf_unsigned(rin_wprintf_buffer_emit, &buffer,
                                    unsigned_value, base, uppercase) != 0) {
        return -1;
    }
    length = buffer.length;
    if (!is_signed && alternate && (base == 16u || base == 2u) &&
        !(length == 1u && candidate[0] == L'0')) {
        if (length > sizeof(candidate) / sizeof(candidate[0]) - 2u) {
            errno = EOVERFLOW;
            return -1;
        }
        for (index = length; index != 0u; --index)
            candidate[index + 1u] = candidate[index - 1u];
        candidate[0] = L'0';
        candidate[1] = base == 2u
            ? (uppercase ? L'B' : L'b')
            : (uppercase ? L'X' : L'x');
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

/* Binary80 ABIs use little-endian x87 in a padded object.  Keep the byte-level
 * ABI check here: unsupported binary128/double-double or malformed unnormal
 * values must remain explicit failures rather than being silently narrowed. */
static int rin_wprintf_hex_long_double_formatted(
    RinWprintfEmit emit, void* context, long double value, int uppercase,
    unsigned int width, int left, int zero, int plus, int space,
    int alternate, unsigned int precision, int precision_set) {
#if defined(LDBL_MANT_DIG) && defined(DBL_MANT_DIG) && \
    defined(LDBL_MAX_EXP) && defined(DBL_MAX_EXP) && \
    LDBL_MANT_DIG == DBL_MANT_DIG && LDBL_MAX_EXP == DBL_MAX_EXP
    /* RinOS long double uses the same binary64 representation and calling
     * convention as double.  Reuse the binary64 formatter instead of
     * interpreting its 8-byte value as an x87 binary80 object. */
    return rin_wprintf_hex_float_formatted(
        emit, context, (double)value, uppercase, width, left, zero, plus,
        space, alternate, precision, precision_set);
#elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
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
    RinWprintfTrackedEmit tracked;
    if (!format) {
        errno = EINVAL;
        return -1;
    }
    tracked.emit = emit;
    tracked.context = context;
    tracked.written = 0u;
    emit = rin_wprintf_tracked_emit;
    context = &tracked;
    while (offset < RIN_WPRINTF_FORMAT_LIMIT) {
        uint32_t conversion;
        int length = 0;
        int left = 0;
        int zero = 0;
        int plus = 0;
        int space = 0;
        int alternate = 0;
        int precision_set = 0;
        int width_set = 0;
        unsigned int width_bits = 0u;
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
            width_set = 1;
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
            width_set = conversion >= (uint32_t)'0' &&
                        conversion <= (uint32_t)'9';
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
        } else if (conversion == (uint32_t)'h') {
            length = 5;
            if (offset == RIN_WPRINTF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return -1;
            }
            conversion = (uint32_t)format[offset++];
            if (conversion == (uint32_t)'h') {
                length = 6;
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
        } else if (conversion == (uint32_t)'j') {
            length = 7;
            if (offset == RIN_WPRINTF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return -1;
            }
            conversion = (uint32_t)format[offset++];
            if (!rin_wchar_scalar_valid(conversion)) {
                errno = EILSEQ;
                return -1;
            }
        } else if (conversion == (uint32_t)'t') {
            length = 8;
            if (offset == RIN_WPRINTF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return -1;
            }
            conversion = (uint32_t)format[offset++];
            if (!rin_wchar_scalar_valid(conversion)) {
                errno = EILSEQ;
                return -1;
            }
        } else if (conversion == (uint32_t)'w') {
            int fast = 0;
            size_t width_offset = offset - 1u;
            int parsed = rin_stdio_parse_width_spec_wide(
                format, &width_offset,
                RIN_WPRINTF_FORMAT_LIMIT, &width_bits, &fast);
            if (parsed != 1) return -1;
            length = fast ? RIN_SSCANF_LENGTH_FAST_WIDTH
                          : RIN_SSCANF_LENGTH_WIDTH;
            if (width_offset == RIN_WPRINTF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return -1;
            }
            conversion = (uint32_t)format[width_offset++];
            offset = width_offset;
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
        if ((length == RIN_SSCANF_LENGTH_WIDTH ||
             length == RIN_SSCANF_LENGTH_FAST_WIDTH) &&
            conversion != (uint32_t)'b' && conversion != (uint32_t)'B' &&
            conversion != (uint32_t)'d' && conversion != (uint32_t)'i' &&
            conversion != (uint32_t)'o' && conversion != (uint32_t)'u' &&
            conversion != (uint32_t)'x' && conversion != (uint32_t)'X' &&
            conversion != (uint32_t)'n') {
            errno = EINVAL;
            return -1;
        }
        if (conversion == (uint32_t)'w') {
            errno = EINVAL;
            return -1;
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
            case 'C':
                /* glibc extension: %C acts like %lc; any length
                 * modifier is ignored. */
                if (precision_set || zero || plus || space || alternate) break;
                if (rin_wprintf_text_candidate(
                        emit, context, 3,
                        (const void*)(uintptr_t)(unsigned int)
                            va_arg(arguments, int),
                        width == 0u ? 1u : width, left, 0u, 0) != 0)
                    return -1;
                continue;
            case 'S':
                /* glibc extension: %S acts like %ls; any length
                 * modifier is ignored. */
                if (zero || plus || space || alternate) break;
                if (rin_wprintf_text_candidate(
                        emit, context, 1, va_arg(arguments, const wchar_t*),
                        width, left, precision, precision_set) != 0)
                    return -1;
                continue;
            case 'd': case 'i': {
                long long signed_value;
                if (alternate) break;
                if (length == RIN_SSCANF_LENGTH_WIDTH ||
                    length == RIN_SSCANF_LENGTH_FAST_WIDTH) {
                    int fast = length == RIN_SSCANF_LENGTH_FAST_WIDTH;
                    if (width_bits == 8u)
                        signed_value = fast
                            ? (long long)(int_fast8_t)RIN_STDIO_VA_ARG_PROMOTED(
                                arguments, int_fast8_t)
                            : (long long)(int8_t)RIN_STDIO_VA_ARG_PROMOTED(
                                arguments, int8_t);
                    else if (width_bits == 16u)
                        signed_value = fast
                            ? (long long)(int_fast16_t)RIN_STDIO_VA_ARG_PROMOTED(
                                arguments, int_fast16_t)
                            : (long long)(int16_t)RIN_STDIO_VA_ARG_PROMOTED(
                                arguments, int16_t);
                    else if (width_bits == 32u)
                        signed_value = fast
                            ? (long long)(int_fast32_t)RIN_STDIO_VA_ARG_PROMOTED(
                                arguments, int_fast32_t)
                            : (long long)(int32_t)RIN_STDIO_VA_ARG_PROMOTED(
                                arguments, int32_t);
                    else
                        signed_value = fast
                            ? (long long)(int_fast64_t)RIN_STDIO_VA_ARG_PROMOTED(
                                arguments, int_fast64_t)
                            : (long long)(int64_t)RIN_STDIO_VA_ARG_PROMOTED(
                                arguments, int64_t);
                } else if (length == 0)
                    signed_value = (long long)va_arg(arguments, int);
                else if (length == 5)
                    signed_value = (short)va_arg(arguments, int);
                else if (length == 6)
                    signed_value = (signed char)va_arg(arguments, int);
                else if (length == 1)
                    signed_value = (long long)va_arg(arguments, long);
                else if (length == 3 || length == 8)
                    signed_value = (long long)va_arg(arguments, ptrdiff_t);
                else if (length == 7)
                    signed_value = (long long)va_arg(arguments, intmax_t);
                else
                    signed_value = va_arg(arguments, long long);
                if (rin_wprintf_integer_formatted(
                        emit, context, signed_value, 0u, 1, 10u, 0, width,
                        left, zero, plus, space, 0, precision, precision_set) != 0)
                    return -1;
                continue;
            }
            case 'u': case 'o': case 'x': case 'X':
            case 'b': case 'B': {
                unsigned long long unsigned_value;
                unsigned int base = conversion == (uint32_t)'o' ? 8u :
                                    conversion == (uint32_t)'u' ? 10u :
                                    (conversion == (uint32_t)'b' ||
                                     conversion == (uint32_t)'B') ? 2u : 16u;
                int uppercase = conversion == (uint32_t)'X' ||
                                conversion == (uint32_t)'B';
                if (plus || space) break;
                if (length == RIN_SSCANF_LENGTH_WIDTH ||
                    length == RIN_SSCANF_LENGTH_FAST_WIDTH) {
                    int fast = length == RIN_SSCANF_LENGTH_FAST_WIDTH;
                    if (width_bits == 8u)
                        unsigned_value = fast
                            ? (unsigned long long)(uint_fast8_t)
                                RIN_STDIO_VA_ARG_PROMOTED(arguments, uint_fast8_t)
                            : (unsigned long long)(uint8_t)
                                RIN_STDIO_VA_ARG_PROMOTED(arguments, uint8_t);
                    else if (width_bits == 16u)
                        unsigned_value = fast
                            ? (unsigned long long)(uint_fast16_t)
                                RIN_STDIO_VA_ARG_PROMOTED(arguments, uint_fast16_t)
                            : (unsigned long long)(uint16_t)
                                RIN_STDIO_VA_ARG_PROMOTED(arguments, uint16_t);
                    else if (width_bits == 32u)
                        unsigned_value = fast
                            ? (unsigned long long)(uint_fast32_t)
                                RIN_STDIO_VA_ARG_PROMOTED(arguments, uint_fast32_t)
                            : (unsigned long long)(uint32_t)
                                RIN_STDIO_VA_ARG_PROMOTED(arguments, uint32_t);
                    else
                        unsigned_value = fast
                            ? (unsigned long long)(uint_fast64_t)
                                RIN_STDIO_VA_ARG_PROMOTED(arguments, uint_fast64_t)
                            : (unsigned long long)(uint64_t)
                                RIN_STDIO_VA_ARG_PROMOTED(arguments, uint64_t);
                } else if (length == 0)
                    unsigned_value = (unsigned long long)va_arg(arguments, unsigned int);
                else if (length == 5)
                    unsigned_value = (unsigned short)va_arg(arguments, int);
                else if (length == 6)
                    unsigned_value = (unsigned char)va_arg(arguments, int);
                else if (length == 1)
                    unsigned_value = (unsigned long long)va_arg(arguments, unsigned long);
                else if (length == 3 || length == 8)
                    unsigned_value = (unsigned long long)va_arg(arguments, size_t);
                else if (length == 7)
                    unsigned_value = (unsigned long long)va_arg(arguments, uintmax_t);
                else
                    unsigned_value = va_arg(arguments, unsigned long long);
                if (rin_wprintf_integer_formatted(
                        emit, context, 0, unsigned_value, 0, base, uppercase,
                        width, left, zero, 0, 0, alternate, precision,
                        precision_set) != 0)
                    return -1;
                continue;
            }
            case 'n': {
                unsigned int type_bits;
                uint64_t maximum;
                if (width_set || precision_set || left || zero || plus ||
                    space || alternate || length == 4) {
                    errno = EINVAL;
                    return -1;
                }
#define RIN_WPRINTF_STORE_N(type) do { \
                    type* output; \
                    if (tracked.written > (size_t)maximum) { \
                        errno = ERANGE; \
                        return -1; \
                    } \
                    output = va_arg(arguments, type*); \
                    if (!output) { errno = EFAULT; return -1; } \
                    *output = (type)tracked.written; \
                } while (0)
                if (length == RIN_SSCANF_LENGTH_WIDTH ||
                    length == RIN_SSCANF_LENGTH_FAST_WIDTH) {
                    int fast = length == RIN_SSCANF_LENGTH_FAST_WIDTH;
                    type_bits = rin_stdio_width_type_bits(width_bits, fast);
                    if (type_bits == 0u || type_bits > 64u) {
                        errno = ENOSYS;
                        return -1;
                    }
                    maximum = type_bits == 64u
                        ? (uint64_t)INT64_MAX
                        : (UINT64_C(1) << (type_bits - 1u)) - 1u;
                    if (fast) {
                        if (width_bits == 8u) RIN_WPRINTF_STORE_N(int_fast8_t);
                        else if (width_bits == 16u) RIN_WPRINTF_STORE_N(int_fast16_t);
                        else if (width_bits == 32u) RIN_WPRINTF_STORE_N(int_fast32_t);
                        else RIN_WPRINTF_STORE_N(int_fast64_t);
                    } else {
                        if (width_bits == 8u) RIN_WPRINTF_STORE_N(int8_t);
                        else if (width_bits == 16u) RIN_WPRINTF_STORE_N(int16_t);
                        else if (width_bits == 32u) RIN_WPRINTF_STORE_N(int32_t);
                        else RIN_WPRINTF_STORE_N(int64_t);
                    }
                } else {
                    if (length == 6) maximum = (uint64_t)SCHAR_MAX;
                    else if (length == 5) maximum = (uint64_t)SHRT_MAX;
                    else if (length == 1) maximum = (uint64_t)LONG_MAX;
                    else if (length == 2) maximum = (uint64_t)LLONG_MAX;
                    else if (length == 3 || length == 8)
                        maximum = (uint64_t)PTRDIFF_MAX;
                    else if (length == 7) maximum = (uint64_t)INTMAX_MAX;
                    else maximum = (uint64_t)INT_MAX;
                    if (length == 6) RIN_WPRINTF_STORE_N(signed char);
                    else if (length == 5) RIN_WPRINTF_STORE_N(short);
                    else if (length == 1) RIN_WPRINTF_STORE_N(long);
                    else if (length == 2) RIN_WPRINTF_STORE_N(long long);
                    else if (length == 3 || length == 8)
                        RIN_WPRINTF_STORE_N(ptrdiff_t);
                    else if (length == 7) RIN_WPRINTF_STORE_N(intmax_t);
                    else RIN_WPRINTF_STORE_N(int);
                }
#undef RIN_WPRINTF_STORE_N
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
    rin_unicode_mbstate_t state;
    uint32_t codepoint = 0u;
    unsigned int byte_index;
    int state_pending;
    if (!stream) {
        errno = EINVAL;
        return WEOF;
    }
    if (!_rin_stdio_claim_wide_orientation(stream)) return WEOF;
    _rin_stdio_wide_decode_state_get(
        stream, &state.state, &state.codepoint);
    state_pending = state.state != 0u || state.codepoint != 0u;
    {
        uint32_t value;
        if (_rin_stdio_wide_pushback_pop(stream, &value)) {
            stream->eof = 0;
            return (wint_t)value;
        }
    }
#if defined(RIN_USERSPACE)
    if (rin_stdio_memstream_kind(stream) == RIN_STDIO_MEMSTREAM_WIDE) {
        uint32_t value = 0u;
        int status = rin_stdio_memstream_read_wide(stream, &value);
        if (status > 0) return (wint_t)value;
        if (status == 0) stream->eof = 1;
        else stream->error = 1;
        return WEOF;
    }
#endif
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
            if (byte_index != 0u || state_pending) {
                /* Truncated sequence at end-of-file is an encoding
                 * error, not a clean EOF (glibc parity: EILSEQ with
                 * the error indicator, EOF indicator clear). */
                errno = EILSEQ;
                stream->error = 1;
                _rin_stdio_wide_decode_state_set(stream, 0u, 0u);
            } else {
                stream->eof = 1;
            }
            return WEOF;
        }
        if (count != 1) {
            errno = EIO;
            stream->error = 1;
            return WEOF;
        }
        conversion =
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
            rin_locale_current_mbrtowc32(&codepoint, &byte, 1u, &state);
#else
            rin_unicode_mbrtowc32(&codepoint, &byte, 1u, &state);
#endif
        if (conversion == (size_t)-1) {
            errno = EILSEQ;
            stream->error = 1;
            _rin_stdio_wide_decode_state_set(stream, 0u, 0u);
            return WEOF;
        }
        _rin_stdio_wide_decode_state_set(
            stream, state.state, state.codepoint);
        if (conversion != (size_t)-2) return (wint_t)codepoint;
    }
    errno = EILSEQ;
    stream->error = 1;
    _rin_stdio_wide_decode_state_set(stream, 0u, 0u);
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
#if defined(RIN_USERSPACE)
    if (rin_stdio_memstream_kind(stream) == RIN_STDIO_MEMSTREAM_WIDE) {
        if (rin_stdio_memstream_write_wide(stream, value) != 1)
            return WEOF;
        return (wint_t)value;
    }
#endif
    if (!rin_wchar_scalar_valid(value)) {
        errno = EILSEQ;
        stream->error = 1;
        return WEOF;
    }
    length =
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
        rin_locale_current_wcrtomb32(encoded, value, &state);
#else
        rin_unicode_wcrtomb32(encoded, value, &state);
#endif
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

/* B58: growable stream-wscanf candidate (wide mirror of narrow's
 * RinFscanfCandidate). Stream fields cannot be measured before they are
 * consumed, so width-omitted (and very wide) conversions accumulate here
 * instead of the legacy min(width, 4096) heap prefix. Inline storage
 * covers ordinary fields with no heap traffic; larger fields spill to
 * the heap with geometric, overflow-checked growth. Appends always keep
 * room for a trailing NUL, so data[count] is writable after any
 * successful append. The owning collector releases its candidate on
 * every exit; stack cost is input-length independent. Growth failure
 * fails fast (partial scalars stay consumed), matching narrow B56. */
#define RIN_WSCANF_CANDIDATE_INLINE 64u
typedef struct {
    uint32_t* data;
    size_t count;
    size_t capacity;
    uint32_t inline_data[RIN_WSCANF_CANDIDATE_INLINE];
} RinWscanfCandidate;

#ifdef RIN_WSCANF_TEST_ALLOC_HOOKS
/* Test seam (B58): the unbounded-field test injects allocation failures
 * and checks for leaks through these. Zero cost when the macro is off. */
extern void* (*rin_wscanf_candidate_malloc_hook)(size_t size);
extern void (*rin_wscanf_candidate_free_hook)(void* pointer);
#endif

static void* rin_wscanf_candidate_alloc(size_t size) {
#ifdef RIN_WSCANF_TEST_ALLOC_HOOKS
    if (rin_wscanf_candidate_malloc_hook)
        return rin_wscanf_candidate_malloc_hook(size);
#endif
    return malloc(size);
}

static void rin_wscanf_candidate_dealloc(void* pointer) {
    if (!pointer) return;
#ifdef RIN_WSCANF_TEST_ALLOC_HOOKS
    if (rin_wscanf_candidate_free_hook) {
        rin_wscanf_candidate_free_hook(pointer);
        return;
    }
#endif
    free(pointer);
}

static void rin_wscanf_candidate_init(RinWscanfCandidate* cand) {
    cand->data = cand->inline_data;
    cand->count = 0u;
    cand->capacity = RIN_WSCANF_CANDIDATE_INLINE;
}

static void rin_wscanf_candidate_release(RinWscanfCandidate* cand) {
    if (cand->data != cand->inline_data) {
        rin_wscanf_candidate_dealloc(cand->data);
        cand->data = cand->inline_data;
    }
    cand->count = 0u;
    cand->capacity = RIN_WSCANF_CANDIDATE_INLINE;
}

/* Single exact-size allocation for string inputs, whose extent is
 * measured up front; keeps B47 malloc-before-any-read on strings. */
static int rin_wscanf_candidate_reserve(RinWscanfCandidate* cand,
                                        size_t count) {
    size_t need = count + 1u;
    uint32_t* storage;
    if (need <= cand->capacity) return 0;
    if (count > (SIZE_MAX - sizeof(uint32_t)) / sizeof(uint32_t)) {
        errno = ENOMEM;
        return -1;
    }
    storage = (uint32_t*)rin_wscanf_candidate_alloc(
        need * sizeof(uint32_t));
    if (!storage) {
        errno = ENOMEM;
        return -1;
    }
    if (cand->data != cand->inline_data)
        rin_wscanf_candidate_dealloc(cand->data);
    cand->data = storage;
    cand->capacity = need;
    return 0;
}

/* Appends one scalar, growing the heap on demand. Returns 0 on success,
 * -1 with errno == ENOMEM when growth fails (partial scalars stay). */
static int rin_wscanf_candidate_append(RinWscanfCandidate* cand,
                                       uint32_t value) {
    if (cand->count + 1u >= cand->capacity) {
        size_t grown;
        uint32_t* replacement;
        size_t index;
        if (cand->capacity > (SIZE_MAX - 1u) / 2u) {
            errno = ENOMEM;
            return -1;
        }
        grown = cand->capacity * 2u;
        if (grown > SIZE_MAX / sizeof(uint32_t)) {
            errno = ENOMEM;
            return -1;
        }
        replacement = (uint32_t*)rin_wscanf_candidate_alloc(
            grown * sizeof(uint32_t));
        if (!replacement) {
            errno = ENOMEM;
            return -1;
        }
        for (index = 0u; index < cand->count; ++index)
            replacement[index] = cand->data[index];
        if (cand->data != cand->inline_data)
            rin_wscanf_candidate_dealloc(cand->data);
        cand->data = replacement;
        cand->capacity = grown;
    }
    cand->data[cand->count++] = value;
    return 0;
}

/* B58: byte twin of the scalar candidate for the stream float collector,
 * which feeds bytes to the shared narrow float parser. Same growth,
 * reserve, and hook contract as above. */
#define RIN_WSCANF_BYTE_CANDIDATE_INLINE 256u
typedef struct {
    char* data;
    size_t count;
    size_t capacity;
    char inline_data[RIN_WSCANF_BYTE_CANDIDATE_INLINE];
} RinWscanfByteCandidate;

static void rin_wscanf_byte_candidate_init(RinWscanfByteCandidate* cand) {
    cand->data = cand->inline_data;
    cand->count = 0u;
    cand->capacity = RIN_WSCANF_BYTE_CANDIDATE_INLINE;
}

static void rin_wscanf_byte_candidate_release(RinWscanfByteCandidate* cand) {
    if (cand->data != cand->inline_data) {
        rin_wscanf_candidate_dealloc(cand->data);
        cand->data = cand->inline_data;
    }
    cand->count = 0u;
    cand->capacity = RIN_WSCANF_BYTE_CANDIDATE_INLINE;
}

static int rin_wscanf_byte_candidate_reserve(RinWscanfByteCandidate* cand,
                                             size_t count) {
    size_t need = count + 1u;
    char* storage;
    if (need <= cand->capacity) return 0;
    if (count == SIZE_MAX) {
        errno = ENOMEM;
        return -1;
    }
    storage = (char*)rin_wscanf_candidate_alloc(need);
    if (!storage) {
        errno = ENOMEM;
        return -1;
    }
    if (cand->data != cand->inline_data)
        rin_wscanf_candidate_dealloc(cand->data);
    cand->data = storage;
    cand->capacity = need;
    return 0;
}

static int rin_wscanf_byte_candidate_append(RinWscanfByteCandidate* cand,
                                            char value) {
    if (cand->count + 1u >= cand->capacity) {
        size_t grown;
        char* replacement;
        size_t index;
        if (cand->capacity > (SIZE_MAX - 1u) / 2u) {
            errno = ENOMEM;
            return -1;
        }
        grown = cand->capacity * 2u;
        replacement = (char*)rin_wscanf_candidate_alloc(grown);
        if (!replacement) {
            errno = ENOMEM;
            return -1;
        }
        for (index = 0u; index < cand->count; ++index)
            replacement[index] = cand->data[index];
        if (cand->data != cand->inline_data)
            rin_wscanf_candidate_dealloc(cand->data);
        cand->data = replacement;
        cand->capacity = grown;
    }
    cand->data[cand->count++] = value;
    return 0;
}

typedef struct {
    const wchar_t* format;
    size_t first;
    size_t last;
    int inverted;
} RinWscanfScanSet;

typedef struct {
    size_t argument_index;
    void* argument_override;
    int has_argument_override;
    int suppress;
    int allocate;
    int length;
    int width_present;
    unsigned int width_bits;
    int width_fast;
    unsigned int width;
    uint32_t conversion;
} RinWscanfSpec;

typedef struct {
    void* destination_slot;
    void* buffer;
} RinWscanfAllocatedField;

enum RinWscanfArgumentKind {
    RIN_WSCANF_ARG_NONE = 0,
    RIN_WSCANF_ARG_VOID_POINTER,
    RIN_WSCANF_ARG_FLOAT_POINTER,
    RIN_WSCANF_ARG_DOUBLE_POINTER,
    RIN_WSCANF_ARG_LONG_DOUBLE_POINTER,
    RIN_WSCANF_ARG_CHAR_POINTER,
    RIN_WSCANF_ARG_WCHAR_POINTER,
    RIN_WSCANF_ARG_CHAR_POINTER_POINTER,
    RIN_WSCANF_ARG_WCHAR_POINTER_POINTER,
    RIN_WSCANF_ARG_INT_POINTER,
    RIN_WSCANF_ARG_SHORT_POINTER,
    RIN_WSCANF_ARG_SCHAR_POINTER,
    RIN_WSCANF_ARG_LONG_POINTER,
    RIN_WSCANF_ARG_LLONG_POINTER,
    RIN_WSCANF_ARG_INTMAX_POINTER,
    RIN_WSCANF_ARG_PTRDIFF_POINTER,
    RIN_WSCANF_ARG_UINT_POINTER,
    RIN_WSCANF_ARG_USHORT_POINTER,
    RIN_WSCANF_ARG_UCHAR_POINTER,
    RIN_WSCANF_ARG_ULONG_POINTER,
    RIN_WSCANF_ARG_ULLONG_POINTER,
    RIN_WSCANF_ARG_UINTMAX_POINTER,
    RIN_WSCANF_ARG_SIZE_POINTER,
    RIN_WSCANF_ARG_I8_POINTER,
    RIN_WSCANF_ARG_I16_POINTER,
    RIN_WSCANF_ARG_I32_POINTER,
    RIN_WSCANF_ARG_I64_POINTER,
    RIN_WSCANF_ARG_U8_POINTER,
    RIN_WSCANF_ARG_U16_POINTER,
    RIN_WSCANF_ARG_U32_POINTER,
    RIN_WSCANF_ARG_U64_POINTER,
    RIN_WSCANF_ARG_FAST_I8_POINTER,
    RIN_WSCANF_ARG_FAST_I16_POINTER,
    RIN_WSCANF_ARG_FAST_I32_POINTER,
    RIN_WSCANF_ARG_FAST_I64_POINTER,
    RIN_WSCANF_ARG_FAST_U8_POINTER,
    RIN_WSCANF_ARG_FAST_U16_POINTER,
    RIN_WSCANF_ARG_FAST_U32_POINTER,
    RIN_WSCANF_ARG_FAST_U64_POINTER
};

typedef struct {
    enum RinWscanfArgumentKind kind;
    void* value;
} RinWscanfArgumentSlot;

typedef struct {
    RinWscanfArgumentSlot slots[64];
    size_t maximum_index;
    size_t sequential_count;
    int positional;
} RinWscanfArgumentPlan;

static int rin_wscanf_scanset_parse(const wchar_t* format, size_t* offset,
                                    RinWscanfScanSet* output);
static void* rin_wscanf_argument(va_list* arguments,
                                  const RinWscanfSpec* spec,
                                  enum RinWscanfArgumentKind kind);

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
    RIN_WSCANF_LENGTH_LONG_DOUBLE,
    RIN_WSCANF_LENGTH_WIDTH = RIN_SSCANF_LENGTH_WIDTH,
    RIN_WSCANF_LENGTH_FAST_WIDTH = RIN_SSCANF_LENGTH_FAST_WIDTH
};

static int rin_wscanf_space(uint32_t value) {
    return iswspace((wint_t)value);
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

    base = conversion == (uint32_t)'b' || conversion == (uint32_t)'B' ? 2u :
           conversion == (uint32_t)'o' ? 8u :
           (conversion == (uint32_t)'x' || conversion == (uint32_t)'X') ?
           16u : conversion == (uint32_t)'i'
               ? (value == (uint32_t)'0' ? 0u : 10u) : 10u;
    if ((conversion == (uint32_t)'i' || base == 16u || base == 2u) &&
        value == (uint32_t)'0') {
        *magnitude = 0u;
        digits = 1u;
        if (conversion == (uint32_t)'i') base = 8u;
        status = rin_wscanf_take(input, width, &used, &value);
        if (status == RIN_WSCANF_MATCH &&
            (((conversion == (uint32_t)'i' || base == 16u) &&
              (value == (uint32_t)'x' || value == (uint32_t)'X')) ||
             ((conversion == (uint32_t)'i' || base == 2u) &&
              (value == (uint32_t)'b' || value == (uint32_t)'B')))) {
            uint32_t prefix = value;
            unsigned int prefix_base =
                prefix == (uint32_t)'x' || prefix == (uint32_t)'X'
                    ? 16u : 2u;
            unsigned int prefix_digit;
            digits = 0u;
            status = rin_wscanf_take(input, width, &used, &value);
            if (status == RIN_WSCANF_MATCH &&
                rin_wscanf_digit(value, prefix_base, &prefix_digit)) {
                base = prefix_base;
                have_value = 1;
            } else if (status == RIN_WSCANF_WIDTH ||
                       status == RIN_WSCANF_EOF ||
                       status == RIN_WSCANF_MATCH) {
                /* B46: a bare 0x/0b prefix without following digits
                 * still consumes through the marker with value 0
                 * (glibc, mirrored from the narrow parser); only
                 * a non-digit lookahead goes back. */
                if (status == RIN_WSCANF_MATCH &&
                    rin_wscanf_unread(input, value) != RIN_WSCANF_MATCH)
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
                                   va_list* arguments,
                                   const RinWscanfSpec* spec) {
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
            int* output = (int*)rin_wscanf_argument(
                arguments, spec, RIN_WSCANF_ARG_INT_POINTER);
            if (!output || value < (long long)INT_MIN || value > (long long)INT_MAX) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = (int)value;
        } else if (length == RIN_WSCANF_LENGTH_SHORT) {
            short* output = (short*)rin_wscanf_argument(
                arguments, spec, RIN_WSCANF_ARG_SHORT_POINTER);
            if (!output || value < (long long)SHRT_MIN ||
                value > (long long)SHRT_MAX) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = (short)value;
        } else if (length == RIN_WSCANF_LENGTH_CHAR) {
            signed char* output = (signed char*)rin_wscanf_argument(
                arguments, spec, RIN_WSCANF_ARG_SCHAR_POINTER);
            if (!output || value < (long long)SCHAR_MIN ||
                value > (long long)SCHAR_MAX) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = (signed char)value;
        } else if (length == RIN_WSCANF_LENGTH_LONG) {
            long* output = (long*)rin_wscanf_argument(
                arguments, spec, RIN_WSCANF_ARG_LONG_POINTER);
            if (!output || value < (long long)LONG_MIN ||
                value > (long long)LONG_MAX) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = (long)value;
        } else if (length == RIN_WSCANF_LENGTH_LONG_LONG) {
            long long* output = (long long*)rin_wscanf_argument(
                arguments, spec, RIN_WSCANF_ARG_LLONG_POINTER);
            if (!output) {
                errno = EFAULT;
                return RIN_WSCANF_ERROR;
            }
            *output = value;
        } else if (length == RIN_WSCANF_LENGTH_INTMAX) {
            intmax_t* output = (intmax_t*)rin_wscanf_argument(
                arguments, spec, RIN_WSCANF_ARG_INTMAX_POINTER);
            if (!output || (!negative &&
                            magnitude > (unsigned long long)INTMAX_MAX) ||
                (negative && magnitude >
                 (unsigned long long)INTMAX_MAX + 1ULL)) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = (intmax_t)value;
        } else {
            ptrdiff_t* output = (ptrdiff_t*)rin_wscanf_argument(
                arguments, spec, RIN_WSCANF_ARG_PTRDIFF_POINTER);
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
            unsigned int* output = (unsigned int*)rin_wscanf_argument(
                arguments, spec, RIN_WSCANF_ARG_UINT_POINTER);
            if (!output || (!negative && magnitude > (unsigned long long)UINT_MAX)) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = negative ? 0u - (unsigned int)magnitude :
                (unsigned int)magnitude;
        } else if (length == RIN_WSCANF_LENGTH_SHORT) {
            unsigned short* output = (unsigned short*)rin_wscanf_argument(
                arguments, spec, RIN_WSCANF_ARG_USHORT_POINTER);
            if (!output || (!negative &&
                            magnitude > (unsigned long long)USHRT_MAX)) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = negative ? (unsigned short)(0u - (unsigned short)magnitude) :
                (unsigned short)magnitude;
        } else if (length == RIN_WSCANF_LENGTH_CHAR) {
            unsigned char* output = (unsigned char*)rin_wscanf_argument(
                arguments, spec, RIN_WSCANF_ARG_UCHAR_POINTER);
            if (!output || (!negative &&
                            magnitude > (unsigned long long)UCHAR_MAX)) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = negative ? (unsigned char)(0u - (unsigned char)magnitude) :
                (unsigned char)magnitude;
        } else if (length == RIN_WSCANF_LENGTH_LONG) {
            unsigned long* output = (unsigned long*)rin_wscanf_argument(
                arguments, spec, RIN_WSCANF_ARG_ULONG_POINTER);
            if (!output || (!negative &&
                            magnitude > (unsigned long long)ULONG_MAX)) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = negative ? 0UL - (unsigned long)magnitude :
                (unsigned long)magnitude;
        } else if (length == RIN_WSCANF_LENGTH_LONG_LONG) {
            unsigned long long* output = (unsigned long long*)rin_wscanf_argument(
                arguments, spec, RIN_WSCANF_ARG_ULLONG_POINTER);
            if (!output) {
                errno = EFAULT;
                return RIN_WSCANF_ERROR;
            }
            *output = negative ? 0ULL - magnitude : magnitude;
        } else if (length == RIN_WSCANF_LENGTH_INTMAX) {
            uintmax_t* output = (uintmax_t*)rin_wscanf_argument(
                arguments, spec, RIN_WSCANF_ARG_UINTMAX_POINTER);
            if (!output || (!negative &&
                            magnitude > (unsigned long long)UINTMAX_MAX)) {
                errno = !output ? EFAULT : ERANGE;
                return RIN_WSCANF_ERROR;
            }
            *output = negative ? (uintmax_t)0 - (uintmax_t)magnitude :
                (uintmax_t)magnitude;
        } else {
            size_t* output = (size_t*)rin_wscanf_argument(
                arguments, spec, RIN_WSCANF_ARG_SIZE_POINTER);
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
                                    va_list* arguments,
                                    const RinWscanfSpec* spec) {
    void** output;
    /* B46: %p wraps like an unsigned conversion (glibc: "-1"
     * reads (void *)-1, mirrored from the narrow clamp); only
     * a positive magnitude past uintptr_t still fails. */
    if (negative) {
        magnitude = 0u - magnitude;
    } else if (magnitude > (unsigned long long)UINTPTR_MAX) {
        errno = ERANGE;
        return RIN_WSCANF_ERROR;
    }
    output = (void**)rin_wscanf_argument(
        arguments, spec, RIN_WSCANF_ARG_VOID_POINTER);
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
/* B46: input-item length of a failed narrow-candidate float (mirrors
 * the narrow stream lexer): inf/nan word failures keep every byte,
 * decimal and hex failures keep the item prefix and return the
 * terminator. */
static size_t rin_wscanf_float_failed_length(const char* candidate,
                                             size_t count) {
    size_t i = 0u;
    if (count == 0u) return 0u;
    if (candidate[0] == '+' || candidate[0] == '-') i = 1u;
    if (i >= count) return i;
    if (candidate[i] >= '0' && candidate[i] <= '9') {
        /* A digit start only fails for a digitless 0x prefix
         * (anything else converts); keep through the x. */
        if (candidate[i] == '0' && i + 1u < count &&
            (candidate[i + 1u] == 'x' || candidate[i + 1u] == 'X'))
            return i + 2u;
        return count;
    }
    if (candidate[i] == '.') return i + 1u;
    if (candidate[i] == 'i' || candidate[i] == 'I' ||
        candidate[i] == 'n' || candidate[i] == 'N')
        return count;
    return i;
}

/* Visible extent of a wide float field on a string input (B47): scan to
 * whitespace/NUL so the heap candidate fits without a fixed bound.
 * Non-ASCII stays included; the fill loop below still reports EILSEQ. */
static unsigned int rin_wscanf_string_float_extent(RinWscanfInput* input,
                                                   unsigned int width) {
    RinWscanfString* string_input = (RinWscanfString*)input->context;
    const wchar_t* text = string_input->text + string_input->offset;
    size_t extent = 0u;
    while (extent < (size_t)width && text[extent] != 0 &&
           !rin_wscanf_space((uint32_t)text[extent]))
        ++extent;
    if (extent > (size_t)UINT_MAX) extent = (size_t)UINT_MAX;
    return (unsigned int)extent;
}

static int rin_wscanf_string_float(unsigned int width, int width_present,
                                   int length, uint32_t conversion,
                                   int suppress, RinWscanfInput* input,
                                   va_list* arguments,
                                   const RinWscanfSpec* wide_spec) {
    struct RinSscanfSpec spec = {};
    struct RinSscanfFloat value;
    RinWscanfByteCandidate field;
    size_t parsed_offset = 0u;
    int input_failure = 0;
    int parsed;

    if (length != RIN_WSCANF_LENGTH_DEFAULT &&
        length != RIN_WSCANF_LENGTH_LONG &&
        length != RIN_WSCANF_LENGTH_LONG_DOUBLE) {
        errno = ENOSYS;
        return RIN_WSCANF_ERROR;
    }
    /* B58: exact-extent reserve on strings, growable candidate on
     * streams (widths of any representable size, narrow B56 parity). */
    rin_wscanf_byte_candidate_init(&field);
    if (input->string_backed) {
        width = rin_wscanf_string_float_extent(input, width);
        if (rin_wscanf_byte_candidate_reserve(&field, width) != 0) {
            rin_wscanf_byte_candidate_release(&field);
            return RIN_WSCANF_ERROR;
        }
    }
    while (field.count < width) {
        uint32_t current;
        int status = rin_wscanf_read(input, &current);
        if (status == RIN_WSCANF_EOF) break;
        if (status != RIN_WSCANF_MATCH) {
            rin_wscanf_byte_candidate_release(&field);
            return status;
        }
        if (rin_wscanf_space(current)) {
            if (rin_wscanf_unread(input, current) != RIN_WSCANF_MATCH) {
                rin_wscanf_byte_candidate_release(&field);
                return RIN_WSCANF_ERROR;
            }
            break;
        }
        if (current > 0x7fu) {
            rin_wscanf_byte_candidate_release(&field);
            errno = EILSEQ;
            return RIN_WSCANF_ERROR;
        }
        if (rin_wscanf_byte_candidate_append(&field, (char)current) != 0) {
            rin_wscanf_byte_candidate_release(&field);
            return RIN_WSCANF_ERROR;
        }
    }
    field.data[field.count] = '\0';
    spec.length = length == RIN_WSCANF_LENGTH_LONG
        ? RIN_SSCANF_LENGTH_LONG
        : length == RIN_WSCANF_LENGTH_LONG_DOUBLE
            ? RIN_SSCANF_LENGTH_LONG_DOUBLE : RIN_SSCANF_LENGTH_NONE;
    spec.conversion = (char)conversion;
    if (wide_spec != NULL) {
        spec.argument_index = wide_spec->argument_index;
        spec.argument_override = wide_spec->argument_override;
        spec.has_argument_override = wide_spec->has_argument_override;
    }
    /* B46: report the real width state, not width=count: forcing
     * a width hides a visible 0x terminator from the shared
     * parser and wrongly takes the width-cut decimal fallback. */
    spec.has_width = width_present;
    spec.width = width;
    parsed = rin_sscanf_parse_float(field.data, field.count,
                                        &parsed_offset, &spec,
                                        &value, &input_failure);
    if (parsed == 0) {
        size_t keep = rin_wscanf_float_failed_length(field.data, field.count);
        if (keep > field.count) keep = field.count;
        while (field.count > keep) {
            --field.count;
            if (rin_wscanf_unread(input, (uint32_t)(unsigned char)field.data[field.count]) !=
                    RIN_WSCANF_MATCH) {
                rin_wscanf_byte_candidate_release(&field);
                return RIN_WSCANF_ERROR;
            }
        }
        rin_wscanf_byte_candidate_release(&field);
        return input_failure ? RIN_WSCANF_ERROR : RIN_WSCANF_NOMATCH;
    }
    if (parsed < 0) {
        rin_wscanf_byte_candidate_release(&field);
        return RIN_WSCANF_ERROR;
    }
    while (field.count > parsed_offset) {
        --field.count;
        if (rin_wscanf_unread(input, (uint32_t)(unsigned char)field.data[field.count]) !=
                RIN_WSCANF_MATCH) {
                rin_wscanf_byte_candidate_release(&field);
            return RIN_WSCANF_ERROR;
        }
    }
    if (!suppress && rin_sscanf_store_float(arguments, &spec, &value) != 0) {
        rin_wscanf_byte_candidate_release(&field);
        return RIN_WSCANF_ERROR;
    }
    rin_wscanf_byte_candidate_release(&field);
    return RIN_WSCANF_MATCH;
}

static int rin_wscanf_store_count(int length, size_t count,
                                  va_list* arguments,
                                  const RinWscanfSpec* spec) {
    if (length == RIN_WSCANF_LENGTH_DEFAULT) {
        int* output = (int*)rin_wscanf_argument(
            arguments, spec, RIN_WSCANF_ARG_INT_POINTER);
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
        short* output = (short*)rin_wscanf_argument(
            arguments, spec, RIN_WSCANF_ARG_SHORT_POINTER);
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
        signed char* output = (signed char*)rin_wscanf_argument(
            arguments, spec, RIN_WSCANF_ARG_SCHAR_POINTER);
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
        long* output = (long*)rin_wscanf_argument(
            arguments, spec, RIN_WSCANF_ARG_LONG_POINTER);
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
        long long* output = (long long*)rin_wscanf_argument(
            arguments, spec, RIN_WSCANF_ARG_LLONG_POINTER);
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
        intmax_t* output = (intmax_t*)rin_wscanf_argument(
            arguments, spec, RIN_WSCANF_ARG_INTMAX_POINTER);
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
        ptrdiff_t* output = (ptrdiff_t*)rin_wscanf_argument(
            arguments, spec, RIN_WSCANF_ARG_PTRDIFF_POINTER);
        if (!output || count > (size_t)PTRDIFF_MAX) {
            errno = !output ? EFAULT : EOVERFLOW;
            return RIN_WSCANF_ERROR;
        }
        *output = (ptrdiff_t)count;
    } else if (length == RIN_WSCANF_LENGTH_PTRDIFF) {
        ptrdiff_t* output = (ptrdiff_t*)rin_wscanf_argument(
            arguments, spec, RIN_WSCANF_ARG_PTRDIFF_POINTER);
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
                                       int wide, va_list* arguments,
                                       const RinWscanfSpec* spec,
                                       RinWscanfAllocatedField* allocated_field) {
    unsigned int index;
    if (wide) {
        wchar_t* output;
        if (spec && spec->allocate) {
            wchar_t** allocated = allocated_field
                ? (wchar_t**)allocated_field->destination_slot : NULL;
            wchar_t* buffer = allocated_field
                ? (wchar_t*)allocated_field->buffer : NULL;
            if (!allocated) {
                errno = EFAULT;
                return RIN_WSCANF_ERROR;
            }
            if (!buffer) {
                /* B58: deferred stream %m sizing (see prepare above). */
                size_t units = (size_t)length + (string ? 1u : 0u);
                if (units == 0u) units = 1u;
                if (units > SIZE_MAX / sizeof(wchar_t)) {
                    errno = EOVERFLOW;
                    return RIN_WSCANF_ERROR;
                }
                buffer = (wchar_t*)rin_wscanf_candidate_alloc(
                    units * sizeof(wchar_t));
                if (!buffer) {
                    errno = ENOMEM;
                    return RIN_WSCANF_ERROR;
                }
                allocated_field->buffer = buffer;
            }
            for (index = 0u; index < length; ++index)
                buffer[index] = (wchar_t)candidate[index];
            if (string) buffer[length] = L'\0';
            *allocated = buffer;
            allocated_field->buffer = NULL;
            return RIN_WSCANF_MATCH;
        }
        output = (wchar_t*)rin_wscanf_argument(
            arguments, spec, RIN_WSCANF_ARG_WCHAR_POINTER);
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
        char* output;
        char** allocated = NULL;
        if (spec && spec->allocate) {
            allocated = allocated_field
                ? (char**)allocated_field->destination_slot : NULL;
            if (!allocated) {
                errno = EFAULT;
                return RIN_WSCANF_ERROR;
            }
        } else {
            output = (char*)rin_wscanf_argument(
                arguments, spec, RIN_WSCANF_ARG_CHAR_POINTER);
            if (!output) {
                errno = EFAULT;
                return RIN_WSCANF_ERROR;
            }
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
        if (allocated != NULL) {
            char* buffer = (char*)allocated_field->buffer;
            if (!buffer) {
                /* B58: deferred stream %m sizing (see prepare above);
                 * validation already passed, so this cannot leak. */
                size_t units = (size_t)length + (string ? 1u : 0u);
                if (units == 0u) units = 1u;
                buffer = (char*)rin_wscanf_candidate_alloc(units);
                if (!buffer) {
                    errno = ENOMEM;
                    return RIN_WSCANF_ERROR;
                }
                allocated_field->buffer = buffer;
            }
            for (index = 0u; index < length; ++index)
                buffer[index] = (char)candidate[index];
            if (string) buffer[length] = '\0';
            *allocated = buffer;
            allocated_field->buffer = NULL;
        } else {
            for (index = 0u; index < length; ++index)
                output[index] = (char)candidate[index];
            if (string) output[length] = '\0';
        }
    }
    return RIN_WSCANF_MATCH;
}

/* Exact string extent for %s/%c (B47): spaces skipped for %s in the
 * peek so the heap candidate fits without a fixed bound. */
static size_t rin_wscanf_string_characters_extent(RinWscanfInput* input,
                                                  unsigned int width,
                                                  int string) {
    RinWscanfString* string_input = (RinWscanfString*)input->context;
    const wchar_t* text = string_input->text + string_input->offset;
    size_t skip = 0u;
    size_t extent = 0u;
    if (string) {
        while (text[skip] != 0 &&
               rin_wscanf_space((uint32_t)text[skip]))
            ++skip;
    }
    while (extent < (size_t)width && text[skip + extent] != 0 &&
           (!string ||
            !rin_wscanf_space((uint32_t)text[skip + extent])))
        ++extent;
    if (extent > (size_t)UINT_MAX) extent = (size_t)UINT_MAX;
    return extent;
}

static int rin_wscanf_allocate_field_buffer(
    size_t capacity, int string, int wide,
    RinWscanfAllocatedField* allocated_field) {
    size_t units;
    size_t element_size;
    if (capacity == 0u) return RIN_WSCANF_MATCH;
    if (string && capacity == SIZE_MAX) {
        errno = EOVERFLOW;
        return RIN_WSCANF_ERROR;
    }
    units = capacity + (string ? 1u : 0u);
    if (units == 0u) return RIN_WSCANF_MATCH;
    element_size = wide ? sizeof(wchar_t) : sizeof(char);
    if (units > SIZE_MAX / element_size) {
        errno = EOVERFLOW;
        return RIN_WSCANF_ERROR;
    }
    allocated_field->buffer =
        rin_wscanf_candidate_alloc(units * element_size);
    if (!allocated_field->buffer) {
        errno = ENOMEM;
        return RIN_WSCANF_ERROR;
    }
    return RIN_WSCANF_MATCH;
}

static int rin_wscanf_prepare_allocated_field(
    RinWscanfInput* input, unsigned int width, int string, int wide,
    va_list* arguments, const RinWscanfSpec* spec,
    RinWscanfAllocatedField* allocated_field) {
    size_t capacity;
    enum RinWscanfArgumentKind argument_kind;
    if (!allocated_field) {
        errno = EINVAL;
        return RIN_WSCANF_ERROR;
    }
    allocated_field->destination_slot = NULL;
    allocated_field->buffer = NULL;
    if (!spec || !spec->allocate) return RIN_WSCANF_MATCH;
    argument_kind = wide ? RIN_WSCANF_ARG_WCHAR_POINTER_POINTER
                         : RIN_WSCANF_ARG_CHAR_POINTER_POINTER;
    allocated_field->destination_slot = rin_wscanf_argument(
        arguments, spec, argument_kind);
    if (!allocated_field->destination_slot) {
        errno = EFAULT;
        return RIN_WSCANF_ERROR;
    }
    if (input->string_backed) {
        capacity = rin_wscanf_string_characters_extent(input, width, string);
    } else {
        /* B58: stream %m buffers are sized from the measured field at
         * store time (narrow B56 parity); the destination slot above
         * is still validated before anything is read. */
        allocated_field->buffer = NULL;
        return RIN_WSCANF_MATCH;
    }
    return rin_wscanf_allocate_field_buffer(capacity, string, wide,
                                             allocated_field);
}

static int rin_wscanf_characters(RinWscanfInput* input, unsigned int width,
                                 int string, int wide, int suppress,
                                 va_list* arguments,
                                 const RinWscanfSpec* spec) {
    /* B58: exact-extent reserve on strings (B47 malloc-before-read
     * kept), growable candidate on streams (narrow B56 parity).
     * Stream %m buffers are sized at store time; only the destination
     * slot is validated before reading. Growth failure fails fast with
     * partial scalars consumed. */
    RinWscanfCandidate field;
    RinWscanfAllocatedField allocated_field = {NULL, NULL};
    unsigned int index = 0u;
    int status;
    rin_wscanf_candidate_init(&field);
    if (input->string_backed) {
        size_t extent = rin_wscanf_string_characters_extent(input, width,
                                                             string);
        if (rin_wscanf_candidate_reserve(&field, extent) != 0) {
            rin_wscanf_candidate_release(&field);
            return RIN_WSCANF_ERROR;
        }
    }
    status = rin_wscanf_prepare_allocated_field(input, width, string, wide,
                                                 arguments, spec,
                                                 &allocated_field);
    if (status != RIN_WSCANF_MATCH) {
        rin_wscanf_candidate_release(&field);
        return status;
    }
    if (string) {
        status = rin_wscanf_skip_space(input);
        if (status != RIN_WSCANF_MATCH) {
            rin_wscanf_candidate_dealloc(allocated_field.buffer);
            rin_wscanf_candidate_release(&field);
            return status;
        }
        while (index < width) {
            uint32_t value;
            status = rin_wscanf_read(input, &value);
            if (status == RIN_WSCANF_EOF) break;
            if (status != RIN_WSCANF_MATCH) {
                rin_wscanf_candidate_dealloc(allocated_field.buffer);
                rin_wscanf_candidate_release(&field);
                return status;
            }
            if (rin_wscanf_space(value)) {
                if (rin_wscanf_unread(input, value) != RIN_WSCANF_MATCH) {
                    rin_wscanf_candidate_dealloc(allocated_field.buffer);
                    rin_wscanf_candidate_release(&field);
                    return RIN_WSCANF_ERROR;
                }
                break;
            }
            if (rin_wscanf_candidate_append(&field, value) != 0) {
                rin_wscanf_candidate_dealloc(allocated_field.buffer);
                rin_wscanf_candidate_release(&field);
                return RIN_WSCANF_ERROR;
            }
            ++index;
        }
        if (index == 0u) {
            rin_wscanf_candidate_dealloc(allocated_field.buffer);
            rin_wscanf_candidate_release(&field);
            return RIN_WSCANF_NOMATCH;
        }
        if (suppress) {
            rin_wscanf_candidate_dealloc(allocated_field.buffer);
            rin_wscanf_candidate_release(&field);
            return RIN_WSCANF_MATCH;
        }
        status = rin_wscanf_store_characters(field.data, index, 1, wide,
                                              arguments, spec,
                                              &allocated_field);
        rin_wscanf_candidate_dealloc(allocated_field.buffer);
        rin_wscanf_candidate_release(&field);
        return status;
    }

    while (index < width) {
        uint32_t value;
        status = rin_wscanf_read(input, &value);
        if (status != RIN_WSCANF_MATCH) {
            rin_wscanf_candidate_dealloc(allocated_field.buffer);
            rin_wscanf_candidate_release(&field);
            return status;
        }
        if (rin_wscanf_candidate_append(&field, value) != 0) {
            rin_wscanf_candidate_dealloc(allocated_field.buffer);
            rin_wscanf_candidate_release(&field);
            return RIN_WSCANF_ERROR;
        }
        ++index;
    }
    if (suppress) {
        rin_wscanf_candidate_dealloc(allocated_field.buffer);
        rin_wscanf_candidate_release(&field);
        return RIN_WSCANF_MATCH;
    }
    status = rin_wscanf_store_characters(field.data, index, 0, wide,
                                          arguments, spec,
                                          &allocated_field);
    rin_wscanf_candidate_dealloc(allocated_field.buffer);
    rin_wscanf_candidate_release(&field);
    return status;
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

/* bound_width once kept a 4096 field-width cap for stream entries, whose
 * collectors read into fixed buffers. B58 lifted the stream cap:
 * collectors now grow a heap candidate, so every caller passes zero and
 * any representable width is accepted on both paths. The parameter is
 * retained so the shared parser signature is untouched. */
static int rin_wscanf_parse_spec(const wchar_t* format, size_t* offset,
                                  RinWscanfSpec* output,
                                  int bound_width) {
    RinWscanfSpec spec = {};
    size_t cursor = *offset;
    size_t probe;
    size_t argument_index = 0u;
    int have_argument_digits = 0;
    int argument_overflow = 0;
    int have_width = 0;
    if (!format || !offset || !output || cursor >= RIN_WSCANF_FORMAT_LIMIT) {
        errno = EINVAL;
        return RIN_WSCANF_ERROR;
    }
    probe = cursor;
    while (probe < RIN_WSCANF_FORMAT_LIMIT &&
           (uint32_t)format[probe] >= (uint32_t)'0' &&
           (uint32_t)format[probe] <= (uint32_t)'9') {
        size_t digit = (size_t)((uint32_t)format[probe] - (uint32_t)'0');
        have_argument_digits = 1;
        /* B47: a huge digit run only fails when it is actually
         * positional; a plain width falls through to the width
         * parser below (string entries accept any width). */
        if (argument_index > (SIZE_MAX - digit) / 10u) {
            argument_overflow = 1;
        } else if (!argument_overflow) {
            argument_index = argument_index * 10u + digit;
        }
        ++probe;
    }
    if (probe == RIN_WSCANF_FORMAT_LIMIT) {
        errno = EOVERFLOW;
        return RIN_WSCANF_ERROR;
    }
    if (have_argument_digits && (uint32_t)format[probe] == (uint32_t)'$') {
        if (argument_overflow || argument_index > 64u) {
            errno = EOVERFLOW;
            return RIN_WSCANF_ERROR;
        }
        if (argument_index == 0u) {
            errno = EINVAL;
            return RIN_WSCANF_ERROR;
        }
        spec.argument_index = argument_index;
        cursor = probe + 1u;
    }
    if (cursor < RIN_WSCANF_FORMAT_LIMIT &&
        (uint32_t)format[cursor] == (uint32_t)'*') {
        spec.suppress = 1;
        ++cursor;
    }
    while (cursor < RIN_WSCANF_FORMAT_LIMIT &&
           (uint32_t)format[cursor] >= (uint32_t)'0' &&
           (uint32_t)format[cursor] <= (uint32_t)'9') {
        unsigned int digit = (unsigned int)((uint32_t)format[cursor] -
                                             (uint32_t)'0');
        unsigned int width_limit;
        /* B58: stream entries take any representable width (growable
         * collectors); the UINT_MAX check below still rejects digit
         * runs no unsigned int can hold. bound_width stays so the
         * shared parser signature is untouched. */
        width_limit = bound_width ? RIN_WSCANF_FIELD_LIMIT : UINT_MAX;
        if (spec.width > (width_limit - digit) / 10u) {
            errno = EOVERFLOW;
            return RIN_WSCANF_ERROR;
        }
        spec.width = spec.width * 10u + digit;
        have_width = 1;
        ++cursor;
    }
    spec.width_present = have_width;
    if (have_width && spec.width == 0u) {
        errno = EINVAL;
        return RIN_WSCANF_ERROR;
    }
    if (cursor < RIN_WSCANF_FORMAT_LIMIT &&
        (uint32_t)format[cursor] == (uint32_t)'m') {
        spec.allocate = 1;
        ++cursor;
    }
    if (cursor >= RIN_WSCANF_FORMAT_LIMIT) {
        errno = EOVERFLOW;
        return RIN_WSCANF_ERROR;
    }
    {
        uint32_t conversion = (uint32_t)format[cursor++];
        if (conversion == (uint32_t)'L') {
            spec.length = RIN_WSCANF_LENGTH_LONG_DOUBLE;
            if (cursor >= RIN_WSCANF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return RIN_WSCANF_ERROR;
            }
            conversion = (uint32_t)format[cursor++];
        } else if (conversion == (uint32_t)'l') {
            spec.length = RIN_WSCANF_LENGTH_LONG;
            if (cursor >= RIN_WSCANF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return RIN_WSCANF_ERROR;
            }
            conversion = (uint32_t)format[cursor++];
            if (conversion == (uint32_t)'l') {
                spec.length = RIN_WSCANF_LENGTH_LONG_LONG;
                if (cursor >= RIN_WSCANF_FORMAT_LIMIT) {
                    errno = EOVERFLOW;
                    return RIN_WSCANF_ERROR;
                }
                conversion = (uint32_t)format[cursor++];
            }
        } else if (conversion == (uint32_t)'h') {
            spec.length = RIN_WSCANF_LENGTH_SHORT;
            if (cursor >= RIN_WSCANF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return RIN_WSCANF_ERROR;
            }
            conversion = (uint32_t)format[cursor++];
            if (conversion == (uint32_t)'h') {
                spec.length = RIN_WSCANF_LENGTH_CHAR;
                if (cursor >= RIN_WSCANF_FORMAT_LIMIT) {
                    errno = EOVERFLOW;
                    return RIN_WSCANF_ERROR;
                }
                conversion = (uint32_t)format[cursor++];
            }
        } else if (conversion == (uint32_t)'z') {
            spec.length = RIN_WSCANF_LENGTH_SIZE;
            if (cursor >= RIN_WSCANF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return RIN_WSCANF_ERROR;
            }
            conversion = (uint32_t)format[cursor++];
        } else if (conversion == (uint32_t)'j') {
            spec.length = RIN_WSCANF_LENGTH_INTMAX;
            if (cursor >= RIN_WSCANF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return RIN_WSCANF_ERROR;
            }
            conversion = (uint32_t)format[cursor++];
        } else if (conversion == (uint32_t)'t') {
            spec.length = RIN_WSCANF_LENGTH_PTRDIFF;
            if (cursor >= RIN_WSCANF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return RIN_WSCANF_ERROR;
            }
            conversion = (uint32_t)format[cursor++];
        } else if (conversion == (uint32_t)'w') {
            size_t width_offset = cursor - 1u;
            int parsed = rin_stdio_parse_width_spec_wide(
                format, &width_offset, RIN_WSCANF_FORMAT_LIMIT,
                &spec.width_bits, &spec.width_fast);
            if (parsed != 1) return RIN_WSCANF_ERROR;
            spec.length = spec.width_fast
                ? RIN_WSCANF_LENGTH_FAST_WIDTH
                : RIN_WSCANF_LENGTH_WIDTH;
            if (width_offset >= RIN_WSCANF_FORMAT_LIMIT) {
                errno = EOVERFLOW;
                return RIN_WSCANF_ERROR;
            }
            conversion = (uint32_t)format[width_offset];
            cursor = width_offset + 1u;
        }
        if (!rin_wchar_scalar_valid(conversion)) {
            errno = EILSEQ;
            return RIN_WSCANF_ERROR;
        }
        spec.conversion = conversion;
    }
    if ((spec.length == RIN_WSCANF_LENGTH_WIDTH ||
         spec.length == RIN_WSCANF_LENGTH_FAST_WIDTH) &&
        spec.conversion != (uint32_t)'b' &&
        spec.conversion != (uint32_t)'B' &&
        spec.conversion != (uint32_t)'d' &&
        spec.conversion != (uint32_t)'i' &&
        spec.conversion != (uint32_t)'o' &&
        spec.conversion != (uint32_t)'u' &&
        spec.conversion != (uint32_t)'x' &&
        spec.conversion != (uint32_t)'X' &&
        spec.conversion != (uint32_t)'n') {
        errno = EINVAL;
        return RIN_WSCANF_ERROR;
    }
    if (spec.length == RIN_WSCANF_LENGTH_LONG_DOUBLE &&
        spec.conversion != (uint32_t)'a' && spec.conversion != (uint32_t)'A' &&
        spec.conversion != (uint32_t)'e' && spec.conversion != (uint32_t)'E' &&
        spec.conversion != (uint32_t)'f' && spec.conversion != (uint32_t)'F' &&
        spec.conversion != (uint32_t)'g' && spec.conversion != (uint32_t)'G') {
        errno = EINVAL;
        return RIN_WSCANF_ERROR;
    }
    switch (spec.conversion) {
    case '%':
        if (spec.suppress || spec.allocate || spec.length != RIN_WSCANF_LENGTH_DEFAULT ||
            spec.width_present || spec.argument_index != 0u) {
            errno = EINVAL;
            return RIN_WSCANF_ERROR;
        }
        break;
    case 'c':
    case 's':
        if (spec.length != RIN_WSCANF_LENGTH_DEFAULT &&
            spec.length != RIN_WSCANF_LENGTH_LONG) {
            errno = EINVAL;
            return RIN_WSCANF_ERROR;
        }
        if (spec.allocate && spec.suppress) {
            errno = EINVAL;
            return RIN_WSCANF_ERROR;
        }
        break;
    case 'C':
    case 'S':
        if (spec.allocate && spec.suppress) {
            errno = EINVAL;
            return RIN_WSCANF_ERROR;
        }
        break;
    case '[':
        if (spec.length != RIN_WSCANF_LENGTH_DEFAULT &&
            spec.length != RIN_WSCANF_LENGTH_LONG) {
            errno = EINVAL;
            return RIN_WSCANF_ERROR;
        }
        if (spec.allocate && spec.suppress) {
            errno = EINVAL;
            return RIN_WSCANF_ERROR;
        }
        break;
    case 'd': case 'i': case 'b': case 'B': case 'u': case 'o': case 'x': case 'X':
        if (spec.length == RIN_WSCANF_LENGTH_LONG_DOUBLE) {
            errno = EINVAL;
            return RIN_WSCANF_ERROR;
        }
        if (spec.allocate) {
            errno = EINVAL;
            return RIN_WSCANF_ERROR;
        }
        break;
    case 'p':
        if (spec.length != RIN_WSCANF_LENGTH_DEFAULT || spec.allocate) {
            errno = EINVAL;
            return RIN_WSCANF_ERROR;
        }
        break;
    case 'n':
        if (spec.suppress || spec.allocate || spec.width_present ||
            spec.length == RIN_WSCANF_LENGTH_LONG_DOUBLE) {
            errno = EINVAL;
            return RIN_WSCANF_ERROR;
        }
        break;
    case 'a': case 'A': case 'e': case 'E': case 'f': case 'F': case 'g': case 'G':
        if (spec.length != RIN_WSCANF_LENGTH_DEFAULT &&
            spec.length != RIN_WSCANF_LENGTH_LONG &&
            spec.length != RIN_WSCANF_LENGTH_LONG_DOUBLE) {
            errno = EINVAL;
            return RIN_WSCANF_ERROR;
        }
        if (spec.allocate) {
            errno = EINVAL;
            return RIN_WSCANF_ERROR;
        }
        break;
    default:
        errno = EINVAL;
        return RIN_WSCANF_ERROR;
    }
    if (spec.allocate && spec.conversion != (uint32_t)'c' &&
        spec.conversion != (uint32_t)'s' &&
        spec.conversion != (uint32_t)'C' &&
        spec.conversion != (uint32_t)'S' &&
        spec.conversion != (uint32_t)'[') {
        errno = EINVAL;
        return RIN_WSCANF_ERROR;
    }
    if (spec.argument_index != 0u && spec.suppress) {
        errno = EINVAL;
        return RIN_WSCANF_ERROR;
    }
    *offset = cursor;
    *output = spec;
    return RIN_WSCANF_MATCH;
}

static enum RinWscanfArgumentKind rin_wscanf_argument_kind(
    const RinWscanfSpec* spec) {
    int is_signed;
    if (!spec || spec->suppress || spec->conversion == (uint32_t)'%')
        return RIN_WSCANF_ARG_NONE;
    if (spec->conversion == (uint32_t)'p')
        return RIN_WSCANF_ARG_VOID_POINTER;
    if (spec->conversion == (uint32_t)'a' ||
        spec->conversion == (uint32_t)'A' ||
        spec->conversion == (uint32_t)'e' ||
        spec->conversion == (uint32_t)'E' ||
        spec->conversion == (uint32_t)'f' ||
        spec->conversion == (uint32_t)'F' ||
        spec->conversion == (uint32_t)'g' ||
        spec->conversion == (uint32_t)'G')
        return spec->length == RIN_WSCANF_LENGTH_LONG
            ? RIN_WSCANF_ARG_DOUBLE_POINTER
            : spec->length == RIN_WSCANF_LENGTH_LONG_DOUBLE
                ? RIN_WSCANF_ARG_LONG_DOUBLE_POINTER
                : RIN_WSCANF_ARG_FLOAT_POINTER;
    if (spec->conversion == (uint32_t)'c' ||
        spec->conversion == (uint32_t)'s' ||
        spec->conversion == (uint32_t)'C' ||
        spec->conversion == (uint32_t)'S' ||
        spec->conversion == (uint32_t)'[') {
        int wide = spec->conversion == (uint32_t)'C' ||
                   spec->conversion == (uint32_t)'S' ||
                   spec->length == RIN_WSCANF_LENGTH_LONG;
        if (spec->allocate)
            return wide ? RIN_WSCANF_ARG_WCHAR_POINTER_POINTER
                        : RIN_WSCANF_ARG_CHAR_POINTER_POINTER;
        return wide ? RIN_WSCANF_ARG_WCHAR_POINTER
                    : RIN_WSCANF_ARG_CHAR_POINTER;
    }
    is_signed = spec->conversion == (uint32_t)'d' ||
                spec->conversion == (uint32_t)'i' ||
                spec->conversion == (uint32_t)'n';
    if (spec->length == RIN_WSCANF_LENGTH_WIDTH ||
        spec->length == RIN_WSCANF_LENGTH_FAST_WIDTH) {
        int fast = spec->length == RIN_WSCANF_LENGTH_FAST_WIDTH;
        if (is_signed) {
            if (spec->width_bits == 8u)
                return fast ? RIN_WSCANF_ARG_FAST_I8_POINTER : RIN_WSCANF_ARG_I8_POINTER;
            if (spec->width_bits == 16u)
                return fast ? RIN_WSCANF_ARG_FAST_I16_POINTER : RIN_WSCANF_ARG_I16_POINTER;
            if (spec->width_bits == 32u)
                return fast ? RIN_WSCANF_ARG_FAST_I32_POINTER : RIN_WSCANF_ARG_I32_POINTER;
            return fast ? RIN_WSCANF_ARG_FAST_I64_POINTER : RIN_WSCANF_ARG_I64_POINTER;
        }
        if (spec->width_bits == 8u)
            return fast ? RIN_WSCANF_ARG_FAST_U8_POINTER : RIN_WSCANF_ARG_U8_POINTER;
        if (spec->width_bits == 16u)
            return fast ? RIN_WSCANF_ARG_FAST_U16_POINTER : RIN_WSCANF_ARG_U16_POINTER;
        if (spec->width_bits == 32u)
            return fast ? RIN_WSCANF_ARG_FAST_U32_POINTER : RIN_WSCANF_ARG_U32_POINTER;
        return fast ? RIN_WSCANF_ARG_FAST_U64_POINTER : RIN_WSCANF_ARG_U64_POINTER;
    }
    if (is_signed) {
        switch (spec->length) {
        case RIN_WSCANF_LENGTH_SHORT: return RIN_WSCANF_ARG_SHORT_POINTER;
        case RIN_WSCANF_LENGTH_CHAR: return RIN_WSCANF_ARG_SCHAR_POINTER;
        case RIN_WSCANF_LENGTH_LONG: return RIN_WSCANF_ARG_LONG_POINTER;
        case RIN_WSCANF_LENGTH_LONG_LONG: return RIN_WSCANF_ARG_LLONG_POINTER;
        case RIN_WSCANF_LENGTH_SIZE:
        case RIN_WSCANF_LENGTH_PTRDIFF: return RIN_WSCANF_ARG_PTRDIFF_POINTER;
        case RIN_WSCANF_LENGTH_INTMAX: return RIN_WSCANF_ARG_INTMAX_POINTER;
        default: return RIN_WSCANF_ARG_INT_POINTER;
        }
    }
    switch (spec->length) {
    case RIN_WSCANF_LENGTH_SHORT: return RIN_WSCANF_ARG_USHORT_POINTER;
    case RIN_WSCANF_LENGTH_CHAR: return RIN_WSCANF_ARG_UCHAR_POINTER;
    case RIN_WSCANF_LENGTH_LONG: return RIN_WSCANF_ARG_ULONG_POINTER;
    case RIN_WSCANF_LENGTH_LONG_LONG: return RIN_WSCANF_ARG_ULLONG_POINTER;
    case RIN_WSCANF_LENGTH_SIZE:
    case RIN_WSCANF_LENGTH_PTRDIFF: return RIN_WSCANF_ARG_SIZE_POINTER;
    case RIN_WSCANF_LENGTH_INTMAX: return RIN_WSCANF_ARG_UINTMAX_POINTER;
    default: return RIN_WSCANF_ARG_UINT_POINTER;
    }
}

static void* rin_wscanf_capture_argument(
    va_list* arguments, enum RinWscanfArgumentKind kind) {
#define RIN_WSCANF_CAPTURE(tag, type) case tag: return (void*)va_arg(*arguments, type*)
    switch (kind) {
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_VOID_POINTER, void*);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_FLOAT_POINTER, float);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_DOUBLE_POINTER, double);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_LONG_DOUBLE_POINTER, long double);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_CHAR_POINTER, char);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_WCHAR_POINTER, wchar_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_CHAR_POINTER_POINTER, char*);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_WCHAR_POINTER_POINTER, wchar_t*);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_INT_POINTER, int);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_SHORT_POINTER, short);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_SCHAR_POINTER, signed char);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_LONG_POINTER, long);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_LLONG_POINTER, long long);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_INTMAX_POINTER, intmax_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_PTRDIFF_POINTER, ptrdiff_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_UINT_POINTER, unsigned int);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_USHORT_POINTER, unsigned short);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_UCHAR_POINTER, unsigned char);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_ULONG_POINTER, unsigned long);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_ULLONG_POINTER, unsigned long long);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_UINTMAX_POINTER, uintmax_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_SIZE_POINTER, size_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_I8_POINTER, int8_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_I16_POINTER, int16_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_I32_POINTER, int32_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_I64_POINTER, int64_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_U8_POINTER, uint8_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_U16_POINTER, uint16_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_U32_POINTER, uint32_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_U64_POINTER, uint64_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_FAST_I8_POINTER, int_fast8_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_FAST_I16_POINTER, int_fast16_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_FAST_I32_POINTER, int_fast32_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_FAST_I64_POINTER, int_fast64_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_FAST_U8_POINTER, uint_fast8_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_FAST_U16_POINTER, uint_fast16_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_FAST_U32_POINTER, uint_fast32_t);
    RIN_WSCANF_CAPTURE(RIN_WSCANF_ARG_FAST_U64_POINTER, uint_fast64_t);
    default: errno = EINVAL; return NULL;
    }
#undef RIN_WSCANF_CAPTURE
}

static int rin_wscanf_prepare_arguments(const wchar_t* format,
                                         va_list* arguments,
                                         RinWscanfArgumentPlan* plan,
                                         int bound_width) {
    size_t offset = 0u;
    int saw_positional = 0;
    int saw_sequential = 0;
    for (size_t slot = 0u; slot < 64u; ++slot) {
        plan->slots[slot].kind = RIN_WSCANF_ARG_NONE;
        plan->slots[slot].value = NULL;
    }
    plan->maximum_index = 0u;
    plan->sequential_count = 0u;
    plan->positional = 0;
    while (offset < RIN_WSCANF_FORMAT_LIMIT) {
        uint32_t value = (uint32_t)format[offset++];
        if (value == 0u) break;
        if (!rin_wchar_scalar_valid(value)) {
            errno = EILSEQ;
            return RIN_WSCANF_ERROR;
        }
        if (value != (uint32_t)'%') continue;
        {
            RinWscanfSpec spec;
            enum RinWscanfArgumentKind kind;
            size_t argument_index;
            if (rin_wscanf_parse_spec(format, &offset, &spec,
                                      bound_width) != RIN_WSCANF_MATCH)
                return RIN_WSCANF_ERROR;
            if (spec.conversion == (uint32_t)'[') {
                RinWscanfScanSet set;
                if (rin_wscanf_scanset_parse(format, &offset, &set) !=
                    RIN_WSCANF_MATCH)
                    return RIN_WSCANF_ERROR;
            }
            kind = rin_wscanf_argument_kind(&spec);
            if (kind == RIN_WSCANF_ARG_NONE) continue;
            argument_index = spec.argument_index;
            if (argument_index != 0u) {
                saw_positional = 1;
            } else {
                saw_sequential = 1;
                argument_index = ++plan->sequential_count;
            }
            if (saw_positional && saw_sequential) {
                errno = EINVAL;
                return RIN_WSCANF_ERROR;
            }
            if (argument_index == 0u || argument_index > 64u) {
                errno = EOVERFLOW;
                return RIN_WSCANF_ERROR;
            }
            if (plan->slots[argument_index - 1u].kind != RIN_WSCANF_ARG_NONE &&
                plan->slots[argument_index - 1u].kind != kind) {
                errno = EINVAL;
                return RIN_WSCANF_ERROR;
            }
            plan->slots[argument_index - 1u].kind = kind;
            if (argument_index > plan->maximum_index)
                plan->maximum_index = argument_index;
        }
    }
    if (offset >= RIN_WSCANF_FORMAT_LIMIT &&
        (uint32_t)format[RIN_WSCANF_FORMAT_LIMIT - 1u] != 0u) {
        errno = EOVERFLOW;
        return RIN_WSCANF_ERROR;
    }
    if (saw_positional) {
        va_list cursor;
        plan->positional = 1;
        for (size_t slot = 0u; slot < plan->maximum_index; ++slot) {
            if (plan->slots[slot].kind == RIN_WSCANF_ARG_NONE) {
                errno = EINVAL;
                return RIN_WSCANF_ERROR;
            }
        }
        va_copy(cursor, *arguments);
        for (size_t slot = 0u; slot < plan->maximum_index; ++slot)
            plan->slots[slot].value = rin_wscanf_capture_argument(
                &cursor, plan->slots[slot].kind);
        va_end(cursor);
    }
    return RIN_WSCANF_MATCH;
}

static void rin_wscanf_bind_argument(RinWscanfSpec* spec,
                                      const RinWscanfArgumentPlan* plan) {
    if (spec && plan && plan->positional && spec->argument_index != 0u) {
        spec->argument_override = plan->slots[spec->argument_index - 1u].value;
        spec->has_argument_override = 1;
    }
}

static void* rin_wscanf_argument(va_list* arguments,
                                  const RinWscanfSpec* spec,
                                  enum RinWscanfArgumentKind kind) {
    if (spec && spec->has_argument_override) return spec->argument_override;
    return rin_wscanf_capture_argument(arguments, kind);
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

/* Exact string extent for %[ (B47): peek member scalars so the heap
 * candidate fits without a fixed bound. */
static size_t rin_wscanf_string_scanset_extent(RinWscanfInput* input,
                                               const RinWscanfScanSet* set,
                                               unsigned int width) {
    RinWscanfString* string_input = (RinWscanfString*)input->context;
    const wchar_t* text = string_input->text + string_input->offset;
    size_t extent = 0u;
    while (extent < (size_t)width && text[extent] != 0 &&
           rin_wscanf_scanset_contains(set, (uint32_t)text[extent]))
        ++extent;
    if (extent > (size_t)UINT_MAX) extent = (size_t)UINT_MAX;
    return extent;
}

static int rin_wscanf_scanset(RinWscanfInput* input,
                               const RinWscanfScanSet* set,
                               unsigned int width, int wide, int suppress,
                               va_list* arguments,
                               const RinWscanfSpec* spec) {
    /* B58: exact-extent reserve on strings (B47 malloc-before-read
     * kept), growable candidate on streams (narrow B56 parity).
     * Stream %m buffers are sized at store time; only the destination
     * slot is validated before reading. Growth failure fails fast with
     * partial scalars consumed. */
    RinWscanfCandidate field;
    RinWscanfAllocatedField allocated_field = {NULL, NULL};
    unsigned int index = 0u;
    int saw_eof = 0;
    rin_wscanf_candidate_init(&field);
    if (input->string_backed) {
        size_t extent = rin_wscanf_string_scanset_extent(input, set,
                                                          width);
        if (rin_wscanf_candidate_reserve(&field, extent) != 0) {
            rin_wscanf_candidate_release(&field);
            return RIN_WSCANF_ERROR;
        }
    }
    if (spec && spec->allocate) {
        enum RinWscanfArgumentKind argument_kind = wide
            ? RIN_WSCANF_ARG_WCHAR_POINTER_POINTER
            : RIN_WSCANF_ARG_CHAR_POINTER_POINTER;
        allocated_field.destination_slot = rin_wscanf_argument(
            arguments, spec, argument_kind);
        if (!allocated_field.destination_slot) {
            rin_wscanf_candidate_release(&field);
            errno = EFAULT;
            return RIN_WSCANF_ERROR;
        }
        if (input->string_backed) {
            size_t capacity =
                rin_wscanf_string_scanset_extent(input, set, width);
            if (rin_wscanf_allocate_field_buffer(capacity, 1, wide,
                                                 &allocated_field) !=
                RIN_WSCANF_MATCH) {
                rin_wscanf_candidate_release(&field);
                return RIN_WSCANF_ERROR;
            }
        } else {
            /* B58: stream %m buffers are sized from the measured field
             * at store time; the destination slot above is still
             * validated before anything is read. */
            allocated_field.buffer = NULL;
        }
    }
    for (;;) {
        uint32_t value;
        int status;
        if (index == width) break;
        status = rin_wscanf_read(input, &value);
        if (status == RIN_WSCANF_EOF) {
            saw_eof = 1;
            break;
        }
        if (status != RIN_WSCANF_MATCH) {
            rin_wscanf_candidate_dealloc(allocated_field.buffer);
            rin_wscanf_candidate_release(&field);
            return status;
        }
        if (!rin_wscanf_scanset_contains(set, value)) {
            if (rin_wscanf_unread(input, value) != RIN_WSCANF_MATCH) {
                rin_wscanf_candidate_dealloc(allocated_field.buffer);
                rin_wscanf_candidate_release(&field);
                return RIN_WSCANF_ERROR;
            }
            break;
        }
        if (rin_wscanf_candidate_append(&field, value) != 0) {
            rin_wscanf_candidate_dealloc(allocated_field.buffer);
            rin_wscanf_candidate_release(&field);
            return RIN_WSCANF_ERROR;
        }
        ++index;
    }
    if (index == 0u) {
        rin_wscanf_candidate_dealloc(allocated_field.buffer);
        rin_wscanf_candidate_release(&field);
        return saw_eof ? RIN_WSCANF_EOF : RIN_WSCANF_NOMATCH;
    }
    if (suppress) {
        rin_wscanf_candidate_dealloc(allocated_field.buffer);
        rin_wscanf_candidate_release(&field);
        return RIN_WSCANF_MATCH;
    }
    {
        int store_status = rin_wscanf_store_characters(field.data, index, 1,
                                                        wide, arguments,
                                                        spec,
                                                        &allocated_field);
        rin_wscanf_candidate_dealloc(allocated_field.buffer);
        rin_wscanf_candidate_release(&field);
        return store_status;
    }
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

/* B58: width-less fields run to the token end on both paths (narrow B56
 * parity). String collectors reserve the measured extent up front;
 * stream collectors grow the heap candidate below. */
static unsigned int rin_wscanf_default_width(RinWscanfInput* input) {
    (void)input;
    return UINT_MAX;
}

static int rin_wscanf_format(RinWscanfInput* input, const wchar_t* format,
                             va_list* arguments) {
    RinWscanfArgumentPlan argument_plan;
    size_t offset = 0u;
    int assignments = 0;
    if (!input || !format) {
        errno = EINVAL;
        return EOF;
    }
    if (rin_wscanf_prepare_arguments(format, arguments, &argument_plan,
                                      0) !=
        RIN_WSCANF_MATCH)
        return EOF;
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
            RinWscanfSpec spec;
            int suppress;
            int produces_assignment;
            int length;
            int width_present;
            unsigned int width_bits;
            int width_fast;
            unsigned int width;
            uint32_t conversion;
            if (rin_wscanf_parse_spec(format, &offset, &spec,
                                      0) !=
                RIN_WSCANF_MATCH)
                return EOF;
            rin_wscanf_bind_argument(&spec, &argument_plan);
            suppress = spec.suppress;
            produces_assignment = spec.conversion != (uint32_t)'%' &&
                                 spec.conversion != (uint32_t)'n';
            length = spec.length;
            width_present = spec.width_present;
            width_bits = spec.width_bits;
            width_fast = spec.width_fast;
            width = spec.width;
            conversion = spec.conversion;
            if (conversion == (uint32_t)'%') {
                status = rin_wscanf_literal(input, (uint32_t)'%');
            } else if (conversion == (uint32_t)'c' ||
                       conversion == (uint32_t)'s' ||
                       conversion == (uint32_t)'C' ||
                       conversion == (uint32_t)'S') {
                /* glibc extension: %C/%S act like %lc/%ls; any length
                 * modifier is ignored. */
                int is_upper = conversion == (uint32_t)'C' ||
                    conversion == (uint32_t)'S';
                int is_string = conversion == (uint32_t)'s' ||
                    conversion == (uint32_t)'S';
                if (!is_upper && length != RIN_WSCANF_LENGTH_DEFAULT &&
                    length != RIN_WSCANF_LENGTH_LONG) {
                    errno = EINVAL;
                    return EOF;
                }
                if (!width_present) {
                    /* B58: width-less strings run to the terminator on
                     * both paths (growable candidate on streams). */
                    width = is_string ?
                        rin_wscanf_default_width(input) : 1u;
                }
                status = rin_wscanf_characters(input, width,
                    is_string,
                    is_upper || length == RIN_WSCANF_LENGTH_LONG,
                    suppress, arguments, &spec);
            } else if (conversion == (uint32_t)'[') {
                RinWscanfScanSet set;
                if (length != RIN_WSCANF_LENGTH_DEFAULT &&
                    length != RIN_WSCANF_LENGTH_LONG) {
                    errno = EINVAL;
                    return EOF;
                }
                if (!width_present) {
                    /* B58: the first non-member is unread and remains
                     * available on both paths now. */
                    width = rin_wscanf_default_width(input);
                }
                status = rin_wscanf_scanset_parse(format, &offset, &set);
                if (status == RIN_WSCANF_MATCH)
                    status = rin_wscanf_scanset(input, &set, width,
                                                length == RIN_WSCANF_LENGTH_LONG,
                                                suppress, arguments, &spec);
            } else if (conversion == (uint32_t)'d' || conversion == (uint32_t)'i' ||
                       conversion == (uint32_t)'b' || conversion == (uint32_t)'B' ||
                       conversion == (uint32_t)'u' ||
                       conversion == (uint32_t)'o' ||
                       conversion == (uint32_t)'x' || conversion == (uint32_t)'X') {
                int negative;
                unsigned long long magnitude;
                if (!width_present) width = rin_wscanf_default_width(input);
                status = rin_wscanf_skip_space(input);
                if (status == RIN_WSCANF_MATCH)
                    status = rin_wscanf_number(input, width, conversion,
                                               &negative, &magnitude);
                if (status == RIN_WSCANF_MATCH &&
                    (length == RIN_WSCANF_LENGTH_WIDTH ||
                     length == RIN_WSCANF_LENGTH_FAST_WIDTH) &&
                    !rin_stdio_width_integer_in_range(
                        width_bits, width_fast,
                        conversion == (uint32_t)'d' ||
                        conversion == (uint32_t)'i',
                        magnitude, negative))
                    status = RIN_WSCANF_ERROR;
                if (status == RIN_WSCANF_MATCH && !suppress) {
                    if (length == RIN_WSCANF_LENGTH_WIDTH ||
                        length == RIN_WSCANF_LENGTH_FAST_WIDTH) {
                        status = rin_stdio_store_width_integer_from(
                            arguments, spec.argument_override,
                            spec.has_argument_override,
                            width_bits, width_fast,
                            (char)conversion, magnitude, negative) == 0
                            ? RIN_WSCANF_MATCH : RIN_WSCANF_ERROR;
                    } else {
                        status = rin_wscanf_store_number(
                            conversion, length, negative, magnitude,
                            arguments, &spec);
                    }
                }
            } else if (conversion == (uint32_t)'n') {
                if (suppress || width_present) {
                    errno = EINVAL;
                    return EOF;
                }
                produces_assignment = 0;
                if (length == RIN_WSCANF_LENGTH_WIDTH ||
                    length == RIN_WSCANF_LENGTH_FAST_WIDTH) {
                    status = rin_stdio_store_width_count_from(
                        arguments, spec.argument_override,
                        spec.has_argument_override,
                        width_bits, width_fast,
                        input->consumed) == 0
                        ? RIN_WSCANF_MATCH : RIN_WSCANF_ERROR;
                } else {
                    status = rin_wscanf_store_count(length,
                                                    input->consumed,
                                                    arguments, &spec);
                }
            } else if (conversion == (uint32_t)'p') {
                int negative;
                unsigned long long magnitude;
                if (length != RIN_WSCANF_LENGTH_DEFAULT) {
                    errno = EINVAL;
                    return EOF;
                }
                if (!width_present) width = rin_wscanf_default_width(input);
                status = rin_wscanf_skip_space(input);
                if (status == RIN_WSCANF_MATCH)
                    status = rin_wscanf_number(input, width, (uint32_t)'x',
                                               &negative, &magnitude);
                if (status == RIN_WSCANF_MATCH && !suppress)
                    status = rin_wscanf_store_pointer(negative, magnitude,
                                                      arguments, &spec);
            } else if (conversion == (uint32_t)'a' || conversion == (uint32_t)'A' ||
                       conversion == (uint32_t)'e' || conversion == (uint32_t)'E' ||
                       conversion == (uint32_t)'f' || conversion == (uint32_t)'F' ||
                       conversion == (uint32_t)'g' || conversion == (uint32_t)'G') {
                if (!width_present) width = rin_wscanf_default_width(input);
                status = rin_wscanf_string_float(width, width_present,
                                                 length, conversion,
                                                 suppress,
                                                 input, arguments, &spec);
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
    /* B47: strings are scanned in place without a fixed bound; keep
     * the up-front scalar validation (fail-fast EILSEQ before any
     * destination is touched). */
    for (index = 0u; ; ++index) {
        uint32_t value = (uint32_t)text[index];
        if (value == 0u) break;
        if (!rin_wchar_scalar_valid(value)) {
            errno = EILSEQ;
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
