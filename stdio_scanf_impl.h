/* SPDX-License-Identifier: MIT */
/* Bounded narrow string scanf core.  Included by stdio.h after FILE exists. */

#ifndef _RIN_STDIO_SCANF_IMPL_H
#define _RIN_STDIO_SCANF_IMPL_H

#include "wchar.h"
#include "stdlib.h"
#include "stdio_scanf_float_impl.h"

#define RIN_SSCANF_MAX_CONVERSIONS 64u

typedef char rin_sscanf_ptrdiff_unsigned_width_must_match[
    sizeof(size_t) == sizeof(ptrdiff_t) ? 1 : -1];

static size_t rin_sscanf_mbrtowc32(uint32_t* character, const char* source,
                                   size_t length,
                                   rin_unicode_mbstate_t* state) {
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
    return rin_locale_current_mbrtowc32(
        character, source, length, (struct rin_unicode_mbstate*)state);
#else
    return rin_unicode_mbrtowc32(character, source, length, state);
#endif
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

/* B56: growable stream-scanf candidate. Fields read from streams cannot
 * be measured before they are consumed, so width-omitted (and very wide)
 * conversions accumulate here instead of the legacy 4096-byte stack
 * scratch. Inline storage covers ordinary fields with no heap traffic;
 * larger fields spill to the heap with geometric, overflow-checked
 * growth. Appends always keep room for a trailing NUL, so data[count]
 * is writable after any successful append. The owning frame releases
 * its candidate on every exit; stack cost is input-length independent. */
#define RIN_FSCANF_CANDIDATE_INLINE 256u
struct RinFscanfCandidate {
    char* data;
    size_t count;
    size_t capacity;
    char inline_data[RIN_FSCANF_CANDIDATE_INLINE];
};

#ifdef RIN_SSCANF_TEST_ALLOC_HOOKS
/* Test seam (B56): the unbounded-field test injects allocation failures
 * and checks for leaks through these. Zero cost when the macro is off. */
extern void* (*rin_fscanf_candidate_malloc_hook)(size_t size);
extern void (*rin_fscanf_candidate_free_hook)(void* pointer);
#endif

static void* rin_fscanf_candidate_alloc(size_t size) {
#ifdef RIN_SSCANF_TEST_ALLOC_HOOKS
    if (rin_fscanf_candidate_malloc_hook)
        return rin_fscanf_candidate_malloc_hook(size);
#endif
    return malloc(size);
}

static void rin_fscanf_candidate_dealloc(void* pointer) {
    if (!pointer) return;
#ifdef RIN_SSCANF_TEST_ALLOC_HOOKS
    if (rin_fscanf_candidate_free_hook) {
        rin_fscanf_candidate_free_hook(pointer);
        return;
    }
#endif
    free(pointer);
}

static void rin_fscanf_candidate_init(struct RinFscanfCandidate* cand) {
    cand->data = cand->inline_data;
    cand->count = 0u;
    cand->capacity = RIN_FSCANF_CANDIDATE_INLINE;
}

static void rin_fscanf_candidate_release(struct RinFscanfCandidate* cand) {
    if (cand->data != cand->inline_data) {
        rin_fscanf_candidate_dealloc(cand->data);
        cand->data = cand->inline_data;
    }
    cand->count = 0u;
    cand->capacity = RIN_FSCANF_CANDIDATE_INLINE;
}

/* Appends one byte, growing the heap on demand. Returns 0 on success,
 * -1 with errno == ENOMEM when growth fails (partial bytes stay). */
static int rin_fscanf_candidate_append(struct RinFscanfCandidate* cand,
                                       unsigned char value) {
    if (cand->count + 1u >= cand->capacity) {
        size_t grown;
        char* replacement;
        size_t index;
        if (cand->capacity > (SIZE_MAX - 1u) / 2u) {
            errno = ENOMEM;
            return -1;
        }
        grown = cand->capacity * 2u;
        replacement = (char*)rin_fscanf_candidate_alloc(grown);
        if (!replacement) {
            errno = ENOMEM;
            return -1;
        }
        for (index = 0u; index < cand->count; ++index)
            replacement[index] = cand->data[index];
        if (cand->data != cand->inline_data)
            rin_fscanf_candidate_dealloc(cand->data);
        cand->data = replacement;
        cand->capacity = grown;
    }
    cand->data[cand->count++] = (char)value;
    return 0;
}

/* bound_width once kept a 4096 field-width cap for stream entries, whose
 * collectors read into fixed stack buffers. B56 lifted the stream cap:
 * collectors now grow a heap candidate, so every caller passes zero and
 * any representable width is accepted on both paths. The parameter is
 * retained so the shared parser signature is untouched. */
static int rin_sscanf_parse_spec(const char* format, size_t* offset,
                                 struct RinSscanfSpec* spec,
                                 int bound_width) {
    size_t current = *offset;
    size_t width = 0u;
    size_t scanset_start = 0u;
    int have_width = 0;
    spec->length = RIN_SSCANF_LENGTH_NONE;
    spec->width = 0u;
    spec->width_bits = 0u;
    spec->conversion = '\0';
    spec->has_width = 0;
    spec->suppress = 0;
    spec->width_fast = 0;
    spec->scanset_invert = 0;
    spec->scanset_data = NULL;
    spec->scanset_bytes = 0u;
    spec->argument_index = 0u;
    spec->argument_override = NULL;
    spec->has_argument_override = 0;
    spec->allocate = 0;
    for (size_t index = 0u; index < sizeof(spec->scanset); ++index)
        spec->scanset[index] = 0u;
    if (format[current] == '%') {
        spec->conversion = '%';
        *offset = current + 1u;
        return 0;
    }
    {
        size_t probe = current;
        size_t argument_index = 0u;
        int have_argument_digits = 0;
        int argument_overflow = 0;
        while (probe <= RIN_SSCANF_MAX_INPUT &&
               format[probe] >= '0' && format[probe] <= '9') {
            size_t digit = (size_t)(format[probe] - '0');
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
        if (probe > RIN_SSCANF_MAX_INPUT) {
            errno = EOVERFLOW;
            return -1;
        }
        if (have_argument_digits && format[probe] == '$') {
            if (argument_overflow ||
                argument_index > RIN_SSCANF_MAX_INPUT) {
                errno = EOVERFLOW;
                return -1;
            }
            if (argument_index == 0u ||
                argument_index > RIN_SSCANF_MAX_CONVERSIONS) {
                errno = EINVAL;
                return -1;
            }
            spec->argument_index = argument_index;
            current = probe + 1u;
        }
    }
    if (format[current] == '*') {
        spec->suppress = 1;
        ++current;
    }
    while (format[current] >= '0' && format[current] <= '9') {
        size_t digit = (size_t)(format[current] - '0');
        size_t width_limit;
        have_width = 1;
        /* B56: stream entries take any representable width (growable
         * collectors); the SIZE_MAX check below still rejects digit
         * runs no size_t can hold. bound_width stays so the shared
         * parser signature is untouched. */
        width_limit = bound_width ? RIN_SSCANF_MAX_INPUT : SIZE_MAX;
        if (width > (width_limit - digit) / 10u) {
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
    /* B46: the m assignment-allocation character precedes length
     * modifiers (glibc: %mls works, %lms does not). */
    if (format[current] == 'm') {
        spec->allocate = 1;
        ++current;
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
    } else if (format[current] == 'w') {
        const char* width_format = format + current;
        int parsed = rin_stdio_parse_width_spec(
            &width_format, &spec->width_bits, &spec->width_fast);
        if (parsed != 1) return -1;
        current = (size_t)(width_format - format);
        spec->length = spec->width_fast
            ? RIN_SSCANF_LENGTH_FAST_WIDTH : RIN_SSCANF_LENGTH_WIDTH;
    }
    if (format[current] == '\0') {
        errno = EINVAL;
        return -1;
    }
    spec->conversion = format[current++];
    spec->width = width;
    spec->has_width = have_width;
    if (spec->conversion == 'd' || spec->conversion == 'i' ||
        spec->conversion == 'b' || spec->conversion == 'u' ||
        spec->conversion == 'o' ||
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
        /* `%n` has no input field and cannot be assignment-suppressed. */
        if (spec->suppress || spec->has_width ||
            spec->length == RIN_SSCANF_LENGTH_LONG_DOUBLE) {
            errno = EINVAL;
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
         * buffer size to the caller; fields read to the token end on both
         * paths (B47 strings, B56 streams), so a missing delimiter ends
         * the field at EOF instead of truncating it. Stack stays bounded:
         * the stream candidate spills to the heap past inline storage. */
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
         * the same unbounded default as stream integer/float readers. */
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
    if (spec->allocate && spec->conversion != 'c' &&
        spec->conversion != 's' && spec->conversion != '[') {
        errno = EINVAL;
        return -1;
    }
    if (spec->argument_index != 0u && spec->suppress) {
        errno = EINVAL;
        return -1;
    }
    *offset = current;
    return 0;
}

enum RinSscanfArgumentKind {
    RIN_SSCANF_ARG_NONE = 0,
    RIN_SSCANF_ARG_VOID_POINTER,
    RIN_SSCANF_ARG_FLOAT_POINTER,
    RIN_SSCANF_ARG_DOUBLE_POINTER,
    RIN_SSCANF_ARG_LONG_DOUBLE_POINTER,
    RIN_SSCANF_ARG_CHAR_POINTER,
    RIN_SSCANF_ARG_WCHAR_POINTER,
    RIN_SSCANF_ARG_CHAR_POINTER_POINTER,
    RIN_SSCANF_ARG_WCHAR_POINTER_POINTER,
    RIN_SSCANF_ARG_INT_POINTER,
    RIN_SSCANF_ARG_SHORT_POINTER,
    RIN_SSCANF_ARG_SCHAR_POINTER,
    RIN_SSCANF_ARG_LONG_POINTER,
    RIN_SSCANF_ARG_LLONG_POINTER,
    RIN_SSCANF_ARG_INTMAX_POINTER,
    RIN_SSCANF_ARG_PTRDIFF_POINTER,
    RIN_SSCANF_ARG_UINT_POINTER,
    RIN_SSCANF_ARG_USHORT_POINTER,
    RIN_SSCANF_ARG_UCHAR_POINTER,
    RIN_SSCANF_ARG_ULONG_POINTER,
    RIN_SSCANF_ARG_ULLONG_POINTER,
    RIN_SSCANF_ARG_UINTMAX_POINTER,
    RIN_SSCANF_ARG_SIZE_POINTER,
    RIN_SSCANF_ARG_WIDTH_I8_POINTER,
    RIN_SSCANF_ARG_WIDTH_I16_POINTER,
    RIN_SSCANF_ARG_WIDTH_I32_POINTER,
    RIN_SSCANF_ARG_WIDTH_I64_POINTER,
    RIN_SSCANF_ARG_WIDTH_U8_POINTER,
    RIN_SSCANF_ARG_WIDTH_U16_POINTER,
    RIN_SSCANF_ARG_WIDTH_U32_POINTER,
    RIN_SSCANF_ARG_WIDTH_U64_POINTER,
    RIN_SSCANF_ARG_FAST_I8_POINTER,
    RIN_SSCANF_ARG_FAST_I16_POINTER,
    RIN_SSCANF_ARG_FAST_I32_POINTER,
    RIN_SSCANF_ARG_FAST_I64_POINTER,
    RIN_SSCANF_ARG_FAST_U8_POINTER,
    RIN_SSCANF_ARG_FAST_U16_POINTER,
    RIN_SSCANF_ARG_FAST_U32_POINTER,
    RIN_SSCANF_ARG_FAST_U64_POINTER
};

struct RinSscanfArgumentSlot {
    enum RinSscanfArgumentKind kind;
    void* value;
};

struct RinSscanfArgumentPlan {
    struct RinSscanfArgumentSlot slots[RIN_SSCANF_MAX_CONVERSIONS];
    size_t maximum_index;
    size_t sequential_count;
    int positional;
};

static enum RinSscanfArgumentKind rin_sscanf_argument_kind(
        const struct RinSscanfSpec* spec) {
    int is_signed;
    if (!spec || spec->suppress || spec->conversion == '%')
        return RIN_SSCANF_ARG_NONE;
    if (spec->conversion == 'p') return RIN_SSCANF_ARG_VOID_POINTER;
    if (spec->conversion == 'a' || spec->conversion == 'A' ||
        spec->conversion == 'e' || spec->conversion == 'E' ||
        spec->conversion == 'f' || spec->conversion == 'F' ||
        spec->conversion == 'g' || spec->conversion == 'G')
        return spec->length == RIN_SSCANF_LENGTH_NONE
            ? RIN_SSCANF_ARG_FLOAT_POINTER
            : spec->length == RIN_SSCANF_LENGTH_LONG
                ? RIN_SSCANF_ARG_DOUBLE_POINTER
                : RIN_SSCANF_ARG_LONG_DOUBLE_POINTER;
    if (spec->conversion == 'c' || spec->conversion == 's' ||
        spec->conversion == '[') {
        if (spec->allocate)
            return spec->length == RIN_SSCANF_LENGTH_LONG
                ? RIN_SSCANF_ARG_WCHAR_POINTER_POINTER
                : RIN_SSCANF_ARG_CHAR_POINTER_POINTER;
        return spec->length == RIN_SSCANF_LENGTH_LONG
            ? RIN_SSCANF_ARG_WCHAR_POINTER
            : RIN_SSCANF_ARG_CHAR_POINTER;
    }
    is_signed = spec->conversion == 'd' || spec->conversion == 'i' ||
                spec->conversion == 'n';
    if (spec->length == RIN_SSCANF_LENGTH_WIDTH ||
        spec->length == RIN_SSCANF_LENGTH_FAST_WIDTH) {
        int fast = spec->length == RIN_SSCANF_LENGTH_FAST_WIDTH;
        if (is_signed) {
            if (spec->width_bits == 8u)
                return fast ? RIN_SSCANF_ARG_FAST_I8_POINTER
                            : RIN_SSCANF_ARG_WIDTH_I8_POINTER;
            if (spec->width_bits == 16u)
                return fast ? RIN_SSCANF_ARG_FAST_I16_POINTER
                            : RIN_SSCANF_ARG_WIDTH_I16_POINTER;
            if (spec->width_bits == 32u)
                return fast ? RIN_SSCANF_ARG_FAST_I32_POINTER
                            : RIN_SSCANF_ARG_WIDTH_I32_POINTER;
            if (spec->width_bits == 64u)
                return fast ? RIN_SSCANF_ARG_FAST_I64_POINTER
                            : RIN_SSCANF_ARG_WIDTH_I64_POINTER;
        } else {
            if (spec->width_bits == 8u)
                return fast ? RIN_SSCANF_ARG_FAST_U8_POINTER
                            : RIN_SSCANF_ARG_WIDTH_U8_POINTER;
            if (spec->width_bits == 16u)
                return fast ? RIN_SSCANF_ARG_FAST_U16_POINTER
                            : RIN_SSCANF_ARG_WIDTH_U16_POINTER;
            if (spec->width_bits == 32u)
                return fast ? RIN_SSCANF_ARG_FAST_U32_POINTER
                            : RIN_SSCANF_ARG_WIDTH_U32_POINTER;
            if (spec->width_bits == 64u)
                return fast ? RIN_SSCANF_ARG_FAST_U64_POINTER
                            : RIN_SSCANF_ARG_WIDTH_U64_POINTER;
        }
        return RIN_SSCANF_ARG_NONE;
    }
    if (is_signed) {
        switch (spec->length) {
        case RIN_SSCANF_LENGTH_SHORT: return RIN_SSCANF_ARG_SHORT_POINTER;
        case RIN_SSCANF_LENGTH_CHAR: return RIN_SSCANF_ARG_SCHAR_POINTER;
        case RIN_SSCANF_LENGTH_LONG: return RIN_SSCANF_ARG_LONG_POINTER;
        case RIN_SSCANF_LENGTH_LONG_LONG:
            return RIN_SSCANF_ARG_LLONG_POINTER;
        case RIN_SSCANF_LENGTH_SIZE:
        case RIN_SSCANF_LENGTH_PTRDIFF:
            return RIN_SSCANF_ARG_PTRDIFF_POINTER;
        case RIN_SSCANF_LENGTH_INTMAX:
            return RIN_SSCANF_ARG_INTMAX_POINTER;
        default: return RIN_SSCANF_ARG_INT_POINTER;
        }
    }
    switch (spec->length) {
    case RIN_SSCANF_LENGTH_SHORT: return RIN_SSCANF_ARG_USHORT_POINTER;
    case RIN_SSCANF_LENGTH_CHAR: return RIN_SSCANF_ARG_UCHAR_POINTER;
    case RIN_SSCANF_LENGTH_LONG: return RIN_SSCANF_ARG_ULONG_POINTER;
    case RIN_SSCANF_LENGTH_LONG_LONG:
        return RIN_SSCANF_ARG_ULLONG_POINTER;
    case RIN_SSCANF_LENGTH_SIZE:
    case RIN_SSCANF_LENGTH_PTRDIFF:
        return RIN_SSCANF_ARG_SIZE_POINTER;
    case RIN_SSCANF_LENGTH_INTMAX:
        return RIN_SSCANF_ARG_UINTMAX_POINTER;
    default: return RIN_SSCANF_ARG_UINT_POINTER;
    }
}

static void* rin_sscanf_capture_argument(
        va_list* arguments, enum RinSscanfArgumentKind kind) {
#define RIN_SSCANF_CAPTURE(tag, type) \
    case tag: return (void*)va_arg(*arguments, type*)
    switch (kind) {
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_VOID_POINTER, void*);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_FLOAT_POINTER, float);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_DOUBLE_POINTER, double);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_LONG_DOUBLE_POINTER, long double);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_CHAR_POINTER, char);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_WCHAR_POINTER, wchar_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_CHAR_POINTER_POINTER, char*);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_WCHAR_POINTER_POINTER, wchar_t*);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_INT_POINTER, int);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_SHORT_POINTER, short);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_SCHAR_POINTER, signed char);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_LONG_POINTER, long);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_LLONG_POINTER, long long);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_INTMAX_POINTER, intmax_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_PTRDIFF_POINTER, ptrdiff_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_UINT_POINTER, unsigned int);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_USHORT_POINTER, unsigned short);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_UCHAR_POINTER, unsigned char);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_ULONG_POINTER, unsigned long);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_ULLONG_POINTER, unsigned long long);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_UINTMAX_POINTER, uintmax_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_SIZE_POINTER, size_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_WIDTH_I8_POINTER, int8_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_WIDTH_I16_POINTER, int16_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_WIDTH_I32_POINTER, int32_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_WIDTH_I64_POINTER, int64_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_WIDTH_U8_POINTER, uint8_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_WIDTH_U16_POINTER, uint16_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_WIDTH_U32_POINTER, uint32_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_WIDTH_U64_POINTER, uint64_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_FAST_I8_POINTER, int_fast8_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_FAST_I16_POINTER, int_fast16_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_FAST_I32_POINTER, int_fast32_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_FAST_I64_POINTER, int_fast64_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_FAST_U8_POINTER, uint_fast8_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_FAST_U16_POINTER, uint_fast16_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_FAST_U32_POINTER, uint_fast32_t);
    RIN_SSCANF_CAPTURE(RIN_SSCANF_ARG_FAST_U64_POINTER, uint_fast64_t);
    default: errno = EINVAL; return NULL;
    }
#undef RIN_SSCANF_CAPTURE
}

static void* rin_sscanf_argument_pointer(
        va_list* arguments, const struct RinSscanfSpec* spec,
        enum RinSscanfArgumentKind kind) {
    if (spec && spec->has_argument_override)
        return spec->argument_override;
    return rin_sscanf_capture_argument(arguments, kind);
}

static int rin_sscanf_validate_format(
        const char* format, struct RinSscanfArgumentPlan* plan,
        int bound_width) {
    size_t index = 0u;
    size_t conversions = 0u;
    int saw_positional = 0;
    int saw_sequential = 0;
    if (!format || !plan) {
        errno = EINVAL;
        return -1;
    }
    for (size_t slot = 0u; slot < RIN_SSCANF_MAX_CONVERSIONS; ++slot) {
        plan->slots[slot].kind = RIN_SSCANF_ARG_NONE;
        plan->slots[slot].value = NULL;
    }
    plan->maximum_index = 0u;
    plan->sequential_count = 0u;
    plan->positional = 0;
    for (;;) {
        unsigned char value;
        if (index > RIN_SSCANF_MAX_INPUT) {
            errno = EOVERFLOW;
            return -1;
        }
        value = (unsigned char)format[index];
        if (value == '\0') break;
        if (index == RIN_SSCANF_MAX_INPUT) {
            errno = EOVERFLOW;
            return -1;
        }
        ++index;
        if (value == '%') {
            struct RinSscanfSpec spec;
            enum RinSscanfArgumentKind kind;
            size_t argument_index;
            if (++conversions > RIN_SSCANF_MAX_CONVERSIONS) {
                errno = EOVERFLOW;
                return -1;
            }
            if (rin_sscanf_parse_spec(format, &index, &spec,
                                      bound_width) != 0) return -1;
            kind = rin_sscanf_argument_kind(&spec);
            if (kind == RIN_SSCANF_ARG_NONE) continue;
            argument_index = spec.argument_index;
            if (argument_index != 0u) {
                saw_positional = 1;
                if (argument_index > RIN_SSCANF_MAX_CONVERSIONS) {
                    errno = EOVERFLOW;
                    return -1;
                }
            } else {
                saw_sequential = 1;
                argument_index = ++plan->sequential_count;
            }
            if (saw_positional && saw_sequential) {
                errno = EINVAL;
                return -1;
            }
            if (argument_index > RIN_SSCANF_MAX_CONVERSIONS) {
                errno = EOVERFLOW;
                return -1;
            }
            if (plan->slots[argument_index - 1u].kind !=
                    RIN_SSCANF_ARG_NONE &&
                plan->slots[argument_index - 1u].kind != kind) {
                errno = EINVAL;
                return -1;
            }
            plan->slots[argument_index - 1u].kind = kind;
            if (argument_index > plan->maximum_index)
                plan->maximum_index = argument_index;
        }
    }
    if (saw_positional) {
        plan->positional = 1;
        for (size_t slot = 0u; slot < plan->maximum_index; ++slot) {
            if (plan->slots[slot].kind == RIN_SSCANF_ARG_NONE) {
                errno = EINVAL;
                return -1;
            }
        }
    }
    return 0;
}

static int rin_sscanf_prepare_arguments(
        const char* format, va_list ap,
        struct RinSscanfArgumentPlan* plan, int bound_width) {
    if (rin_sscanf_validate_format(format, plan, bound_width) != 0)
        return -1;
    if (!plan->positional) return 0;
    {
        va_list cursor;
        va_copy(cursor, ap);
        for (size_t slot = 0u; slot < plan->maximum_index; ++slot)
            plan->slots[slot].value = rin_sscanf_capture_argument(
                &cursor, plan->slots[slot].kind);
        va_end(cursor);
    }
    return 0;
}

static int rin_sscanf_capture_positional_arguments(
        va_list ap, struct RinSscanfArgumentPlan* plan) {
    va_list cursor;
    if (!plan || !plan->positional) return 0;
    va_copy(cursor, ap);
    for (size_t slot = 0u; slot < plan->maximum_index; ++slot)
        plan->slots[slot].value = rin_sscanf_capture_argument(
            &cursor, plan->slots[slot].kind);
    va_end(cursor);
    return 0;
}

static void rin_sscanf_bind_argument(
        struct RinSscanfSpec* spec,
        const struct RinSscanfArgumentPlan* plan) {
    if (spec && plan && plan->positional && spec->argument_index != 0u) {
        spec->argument_override =
            plan->slots[spec->argument_index - 1u].value;
        spec->has_argument_override = 1;
    }
}

static int rin_sscanf_parse_integer(
        const char* input, size_t input_size, size_t* offset,
        const struct RinSscanfSpec* spec, uint64_t* magnitude_out,
        int* negative_out, int* input_failure_out,
        int* range_error_out) {
    size_t current = *offset;
    size_t end;
    unsigned int base;
    uint64_t magnitude = 0u;
    int negative = 0;
    int have_digit = 0;
    int prefixed = 0;
    if (range_error_out) *range_error_out = 0;
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
    /* B46: %p takes an optional sign like %x (glibc wraps
     * negatives into the pointer); the clamp below folds it. */
    if (current < end &&
        (input[current] == '+' || input[current] == '-')) {
        negative = input[current] == '-';
        ++current;
    }
    base = spec->conversion == 'b' ? 2u :
           spec->conversion == 'o' ? 8u :
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
            prefixed = 1;
        }
    }
    if ((base == 0u || base == 2u) && current + 2u < end &&
        input[current] == '0' &&
        (input[current + 1u] == 'b' || input[current + 1u] == 'B')) {
        unsigned int prefix_digit;
        if (rin_sscanf_digit((unsigned char)input[current + 2u],
                             &prefix_digit) && prefix_digit < 2u) {
            base = 2u;
            current += 2u;
            prefixed = 1;
        }
    }
    /* B46: a bare 0x/0X/0b/0B prefix without following digits
     * still consumes through the marker with value 0 (glibc:
     * %i/%x/%b/%p on "0x" give r=1, v=0, n=2); the input item
     * ends there. */
    if (!prefixed && current + 1u < end &&
        input[current] == '0' &&
        (((base == 0u || base == 16u) &&
          (input[current + 1u] == 'x' ||
           input[current + 1u] == 'X')) ||
         ((base == 0u || base == 2u) &&
          (input[current + 1u] == 'b' ||
           input[current + 1u] == 'B')))) {
        *offset = current + 2u;
        *magnitude_out = 0u;
        *negative_out = negative;
        return 1;
    }
    if (base == 0u)
        base = current < end && input[current] == '0' ? 8u : 10u;
    while (current < end) {
        unsigned int digit;
        if (!rin_sscanf_digit((unsigned char)input[current], &digit) ||
            digit >= base)
            break;
        /* BATCH41: overflow saturates (strtol rules) instead of failing:
         * keep consuming valid digits so the offset stays exact. */
        if (magnitude > (UINT64_MAX - digit) / base) {
            errno = ERANGE;
            if (range_error_out) *range_error_out = 1;
            magnitude = UINT64_MAX;
        } else {
            magnitude = magnitude * base + digit;
        }
        have_digit = 1;
        ++current;
    }
    if (!have_digit) return 0;
    *offset = current;
    *magnitude_out = magnitude;
    *negative_out = negative;
    return 1;
}

/* BATCH41: strtol-family width clamp (glibc rules). Integer conversions
 * saturate to the strto{l,ll,ul,ull}-width range of their length, set
 * ERANGE, and still assign (the store path narrows to the target, also
 * like glibc). Widths: {none,hh,h,l} use (unsigned) long, {ll,j} use
 * (unsigned) long long, {z,t} use ptrdiff/size_t (which equal long on
 * LP64, match UCRT on LLP64, and match strtol-width on m32). %p and
 * C23 %w keep their strict range failure. */
static int rin_sscanf_clamp_integer_magnitude(
        const struct RinSscanfSpec* spec, uint64_t* magnitude_io,
        int negative, int saturated, int* clamped_out) {
    int is_signed = spec->conversion == 'd' || spec->conversion == 'i';
    uint64_t maximum;
    uint64_t magnitude = *magnitude_io;
    if (clamped_out) *clamped_out = 0;
    if (spec->conversion == 'p') {
        /* B46: %p wraps like an unsigned conversion (glibc:
         * "-1" reads (void *)-1); only a positive magnitude
         * past uintptr_t still fails strictly. */
        if (negative) {
            *magnitude_io = 0u - magnitude;
            return 1;
        }
        if (magnitude > (uint64_t)UINTPTR_MAX) {
            errno = ERANGE;
            return 0;
        }
        return 1;
    }
    if (spec->length == RIN_SSCANF_LENGTH_WIDTH ||
        spec->length == RIN_SSCANF_LENGTH_FAST_WIDTH)
        return rin_stdio_width_integer_in_range(
            spec->width_bits,
            spec->length == RIN_SSCANF_LENGTH_FAST_WIDTH,
            is_signed, magnitude, negative);
    if (is_signed) {
        uint64_t limit;
        if (spec->length == RIN_SSCANF_LENGTH_LONG_LONG)
            maximum = (uint64_t)LLONG_MAX;
        else if (spec->length == RIN_SSCANF_LENGTH_INTMAX)
            maximum = (uint64_t)INT64_MAX;
        else if (spec->length == RIN_SSCANF_LENGTH_SIZE ||
                 spec->length == RIN_SSCANF_LENGTH_PTRDIFF)
            maximum = (uint64_t)PTRDIFF_MAX;
        else
            maximum = (uint64_t)LONG_MAX;
        limit = maximum + (negative ? 1u : 0u);
        /* A saturated parse holds UINT64_MAX, which can also be a
         * legitimate in-range magnitude; the flag disambiguates. */
        if (saturated || magnitude > limit) {
            *magnitude_io = limit;
            if (clamped_out) *clamped_out = 1;
            errno = ERANGE;
        }
    } else {
        if (spec->length == RIN_SSCANF_LENGTH_LONG_LONG)
            maximum = (uint64_t)ULLONG_MAX;
        else if (spec->length == RIN_SSCANF_LENGTH_INTMAX)
            maximum = (uint64_t)UINT64_MAX;
        else if (spec->length == RIN_SSCANF_LENGTH_SIZE ||
                 spec->length == RIN_SSCANF_LENGTH_PTRDIFF)
            maximum = (uint64_t)SIZE_MAX;
        else
            maximum = (uint64_t)ULONG_MAX;
        if (saturated || magnitude > maximum) {
            *magnitude_io = maximum;
            if (clamped_out) *clamped_out = 1;
            errno = ERANGE;
        }
    }
    return 1;
}

/* `va_list` is a pointer type on the Windows x64 ABI.  Receive its address so
 * consuming a conversion advances the caller's list on both pointer and
 * array-based ABI representations. */
static int rin_sscanf_store_integer(va_list* arguments,
                                    const struct RinSscanfSpec* spec,
                                    uint64_t magnitude, int negative,
                                    int range_error) {
    int is_signed = spec->conversion == 'd' || spec->conversion == 'i';
    int clamped = 0;
    if (!rin_sscanf_clamp_integer_magnitude(spec, &magnitude, negative,
                                            range_error, &clamped))
        return -1;
    if (spec->conversion == 'p') {
        void** output = (void**)rin_sscanf_argument_pointer(
            arguments, spec, RIN_SSCANF_ARG_VOID_POINTER);
        if (!output) { errno = EFAULT; return -1; }
        *output = (void*)(uintptr_t)magnitude;
        return 0;
    }
    if (spec->length == RIN_SSCANF_LENGTH_WIDTH ||
        spec->length == RIN_SSCANF_LENGTH_FAST_WIDTH) {
        if (spec->has_argument_override)
            return rin_stdio_store_width_integer_from(
                arguments, spec->argument_override, 1, spec->width_bits,
                spec->length == RIN_SSCANF_LENGTH_FAST_WIDTH,
                spec->conversion, magnitude, negative);
        return rin_stdio_store_width_integer(
            arguments, spec->width_bits,
            spec->length == RIN_SSCANF_LENGTH_FAST_WIDTH,
            spec->conversion, magnitude, negative);
    }
    if (is_signed) {
        int64_t value;
        if (!negative) value = (int64_t)magnitude;
        else if (magnitude == UINT64_C(0x8000000000000000))
            value = INT64_MIN;
        else
            value = -(int64_t)magnitude;
        if (spec->length == RIN_SSCANF_LENGTH_LONG_LONG) {
            long long* output = (long long*)rin_sscanf_argument_pointer(
                arguments, spec, RIN_SSCANF_ARG_LLONG_POINTER);
            if (!output) { errno = EFAULT; return -1; }
            *output = (long long)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_LONG) {
            long* output = (long*)rin_sscanf_argument_pointer(
                arguments, spec, RIN_SSCANF_ARG_LONG_POINTER);
            if (!output) { errno = EFAULT; return -1; }
            *output = (long)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_SIZE) {
            ptrdiff_t* output = (ptrdiff_t*)rin_sscanf_argument_pointer(
                arguments, spec, RIN_SSCANF_ARG_PTRDIFF_POINTER);
            if (!output) { errno = EFAULT; return -1; }
            *output = (ptrdiff_t)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_SHORT) {
            short* output = (short*)rin_sscanf_argument_pointer(
                arguments, spec, RIN_SSCANF_ARG_SHORT_POINTER);
            if (!output) { errno = EFAULT; return -1; }
            *output = (short)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_CHAR) {
            signed char* output = (signed char*)rin_sscanf_argument_pointer(
                arguments, spec, RIN_SSCANF_ARG_SCHAR_POINTER);
            if (!output) { errno = EFAULT; return -1; }
            *output = (signed char)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_INTMAX) {
            intmax_t* output = (intmax_t*)rin_sscanf_argument_pointer(
                arguments, spec, RIN_SSCANF_ARG_INTMAX_POINTER);
            if (!output) { errno = EFAULT; return -1; }
            *output = (intmax_t)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_PTRDIFF) {
            ptrdiff_t* output = (ptrdiff_t*)rin_sscanf_argument_pointer(
                arguments, spec, RIN_SSCANF_ARG_PTRDIFF_POINTER);
            if (!output) { errno = EFAULT; return -1; }
            *output = (ptrdiff_t)value;
        } else {
            int* output = (int*)rin_sscanf_argument_pointer(
                arguments, spec, RIN_SSCANF_ARG_INT_POINTER);
            if (!output) { errno = EFAULT; return -1; }
            *output = (int)value;
        }
    } else {
        /* A clamped magnitude already holds the strtoul result; only
         * wrap an in-range negative (BATCH41). */
        uint64_t value = (negative && !clamped)
            ? UINT64_C(0) - magnitude
            : magnitude;
        if (spec->length == RIN_SSCANF_LENGTH_LONG_LONG) {
            unsigned long long* output =
                (unsigned long long*)rin_sscanf_argument_pointer(
                    arguments, spec, RIN_SSCANF_ARG_ULLONG_POINTER);
            if (!output) { errno = EFAULT; return -1; }
            *output = (unsigned long long)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_LONG) {
            unsigned long* output = (unsigned long*)rin_sscanf_argument_pointer(
                arguments, spec, RIN_SSCANF_ARG_ULONG_POINTER);
            if (!output) { errno = EFAULT; return -1; }
            *output = (unsigned long)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_SIZE) {
            size_t* output = (size_t*)rin_sscanf_argument_pointer(
                arguments, spec, RIN_SSCANF_ARG_SIZE_POINTER);
            if (!output) { errno = EFAULT; return -1; }
            *output = (size_t)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_SHORT) {
            unsigned short* output =
                (unsigned short*)rin_sscanf_argument_pointer(
                    arguments, spec, RIN_SSCANF_ARG_USHORT_POINTER);
            if (!output) { errno = EFAULT; return -1; }
            *output = (unsigned short)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_CHAR) {
            unsigned char* output = (unsigned char*)rin_sscanf_argument_pointer(
                arguments, spec, RIN_SSCANF_ARG_UCHAR_POINTER);
            if (!output) { errno = EFAULT; return -1; }
            *output = (unsigned char)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_INTMAX) {
            uintmax_t* output = (uintmax_t*)rin_sscanf_argument_pointer(
                arguments, spec, RIN_SSCANF_ARG_UINTMAX_POINTER);
            if (!output) { errno = EFAULT; return -1; }
            *output = (uintmax_t)value;
        } else if (spec->length == RIN_SSCANF_LENGTH_PTRDIFF) {
            size_t* output = (size_t*)rin_sscanf_argument_pointer(
                arguments, spec, RIN_SSCANF_ARG_SIZE_POINTER);
            if (!output) { errno = EFAULT; return -1; }
            *output = (size_t)value;
        } else {
            unsigned int* output = (unsigned int*)rin_sscanf_argument_pointer(
                arguments, spec, RIN_SSCANF_ARG_UINT_POINTER);
            if (!output) { errno = EFAULT; return -1; }
            *output = (unsigned int)value;
        }
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
    if (spec->length == RIN_SSCANF_LENGTH_WIDTH ||
        spec->length == RIN_SSCANF_LENGTH_FAST_WIDTH) {
        if (spec->has_argument_override)
            return rin_stdio_store_width_count_from(
                arguments, spec->argument_override, 1, spec->width_bits,
                spec->length == RIN_SSCANF_LENGTH_FAST_WIDTH, count);
        return rin_stdio_store_width_count(
            arguments, spec->width_bits,
            spec->length == RIN_SSCANF_LENGTH_FAST_WIDTH, count);
    }
    if (spec->length == RIN_SSCANF_LENGTH_LONG_LONG) {
        long long* output;
        if (rin_sscanf_count_exceeds_signed_maximum(count,
                                                    (uint64_t)LLONG_MAX)) {
            errno = ERANGE;
            return -1;
        }
        output = (long long*)rin_sscanf_argument_pointer(
            arguments, spec, RIN_SSCANF_ARG_LLONG_POINTER);
        if (!output) { errno = EFAULT; return -1; }
        *output = (long long)count;
    } else if (spec->length == RIN_SSCANF_LENGTH_LONG) {
        long* output;
        if ((uint64_t)count > (uint64_t)LONG_MAX) {
            errno = ERANGE;
            return -1;
        }
        output = (long*)rin_sscanf_argument_pointer(
            arguments, spec, RIN_SSCANF_ARG_LONG_POINTER);
        if (!output) { errno = EFAULT; return -1; }
        *output = (long)count;
    } else if (spec->length == RIN_SSCANF_LENGTH_SIZE) {
        ptrdiff_t* output;
        if ((uint64_t)count > (uint64_t)PTRDIFF_MAX) {
            errno = ERANGE;
            return -1;
        }
        output = (ptrdiff_t*)rin_sscanf_argument_pointer(
            arguments, spec, RIN_SSCANF_ARG_PTRDIFF_POINTER);
        if (!output) { errno = EFAULT; return -1; }
        *output = (ptrdiff_t)count;
    } else if (spec->length == RIN_SSCANF_LENGTH_SHORT) {
        short* output;
        if ((uint64_t)count > (uint64_t)SHRT_MAX) {
            errno = ERANGE;
            return -1;
        }
        output = (short*)rin_sscanf_argument_pointer(
            arguments, spec, RIN_SSCANF_ARG_SHORT_POINTER);
        if (!output) { errno = EFAULT; return -1; }
        *output = (short)count;
    } else if (spec->length == RIN_SSCANF_LENGTH_CHAR) {
        signed char* output;
        if ((uint64_t)count > (uint64_t)SCHAR_MAX) {
            errno = ERANGE;
            return -1;
        }
        output = (signed char*)rin_sscanf_argument_pointer(
            arguments, spec, RIN_SSCANF_ARG_SCHAR_POINTER);
        if (!output) { errno = EFAULT; return -1; }
        *output = (signed char)count;
    } else if (spec->length == RIN_SSCANF_LENGTH_INTMAX) {
        intmax_t* output;
        if (rin_sscanf_count_exceeds_signed_maximum(count,
                                                    (uint64_t)INT64_MAX)) {
            errno = ERANGE;
            return -1;
        }
        output = (intmax_t*)rin_sscanf_argument_pointer(
            arguments, spec, RIN_SSCANF_ARG_INTMAX_POINTER);
        if (!output) { errno = EFAULT; return -1; }
        *output = (intmax_t)count;
    } else if (spec->length == RIN_SSCANF_LENGTH_PTRDIFF) {
        ptrdiff_t* output;
        if ((uint64_t)count > (uint64_t)PTRDIFF_MAX) {
            errno = ERANGE;
            return -1;
        }
        output = (ptrdiff_t*)rin_sscanf_argument_pointer(
            arguments, spec, RIN_SSCANF_ARG_PTRDIFF_POINTER);
        if (!output) { errno = EFAULT; return -1; }
        *output = (ptrdiff_t)count;
    } else {
        int* output;
        if ((uint64_t)count > (uint64_t)INT_MAX) {
            errno = ERANGE;
            return -1;
        }
        output = (int*)rin_sscanf_argument_pointer(
            arguments, spec, RIN_SSCANF_ARG_INT_POINTER);
        if (!output) { errno = EFAULT; return -1; }
        *output = (int)count;
    }
    return 0;
}

/* Decode a narrow input item into a private wide candidate before touching
 * the caller's destination.  Accepting only complete locale sequences avoids
 * byte-casting malformed input into a wchar_t and keeps `%lc`/`%ls`
 * failure-atomic. */
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
        size_t consumed = rin_sscanf_mbrtowc32(
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

static int rin_sscanf_first_multibyte(const char* input, size_t input_size,
                                      size_t* consumed_out,
                                      uint32_t* value_out) {
    rin_unicode_mbstate_t state;
    size_t probe;
    if (!input || !consumed_out || !value_out || input_size == 0u) return 0;
    for (probe = 1u; probe <= input_size && probe <= 4u; ++probe) {
        size_t consumed;
        uint32_t value = 0u;
        state.state = 0u;
        state.codepoint = 0u;
        consumed = rin_sscanf_mbrtowc32(&value, input, probe, &state);
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
        int parsed = rin_sscanf_first_multibyte(
            spec->scanset_data + offset, spec->scanset_bytes - offset,
            &consumed, &member);
        if (parsed <= 0) return -1;
        offset += consumed;
        if (member == (uint32_t)'-' && have_previous &&
            offset < spec->scanset_bytes) {
            uint32_t range_end;
            parsed = rin_sscanf_first_multibyte(
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

/* Locate a wide string-field extent in place (B47: no 4097-byte stack
 * candidate).  Width is a scalar count for `%lc`/`%ls`; without a
 * width the field runs to the first boundary in the whole string. */
static int rin_sscanf_wide_field_extent(
        const char* input, size_t input_size, size_t* input_offset,
        const struct RinSscanfSpec* spec, size_t* begin_out,
        size_t* end_out, size_t* scalar_count_out, int* input_failure_out) {
    size_t current = *input_offset;
    size_t begin;
    size_t scalar_count = 0u;
    size_t scalar_limit = spec->has_width ? spec->width : SIZE_MAX;
    int first_result;
    if (!input || !input_offset || !spec || !begin_out || !end_out ||
        !scalar_count_out || !input_failure_out || scalar_limit == 0u) {
        errno = EINVAL;
        return -1;
    }
    *input_failure_out = 0;
    if (spec->conversion == 's') {
        while (current < input_size &&
               rin_sscanf_space((unsigned char)input[current])) ++current;
    }
    begin = current;
    while (current < input_size && scalar_count < scalar_limit) {
        size_t consumed;
        uint32_t value;
        first_result = rin_sscanf_first_multibyte(
            input + current, input_size - current, &consumed, &value);
        if (first_result <= 0) return -1;
        if (spec->conversion == 's' && rin_sscanf_space(value)) break;
        if (spec->conversion == '[') {
            int matched = rin_sscanf_wide_scanset_contains(spec, value);
            if (matched < 0) return -1;
            if (matched == 0) break;
        }
        current += consumed;
        ++scalar_count;
    }
    if (scalar_count == 0u) {
        if (current == input_size) *input_failure_out = 1;
        return 0;
    }
    *input_offset = current;
    *begin_out = begin;
    *end_out = current;
    *scalar_count_out = scalar_count;
    return 1;
}

static int rin_sscanf_store_wide_field(
        va_list* arguments, const struct RinSscanfSpec* spec,
        const char* candidate, size_t byte_count, int suppress) {
    /* B47: two passes over the caller bytes instead of a 4097-wide
     * stack scratch.  The counting pass validates every scalar up
     * front, so EILSEQ still fails before any destination (or %m
     * allocation) is touched. */
    size_t offset = 0u;
    size_t wide_count = 0u;
    size_t wide_capacity = 0u;
    size_t decoded = 0u;
    int terminate = spec->conversion == 's' || spec->conversion == '[';
    while (offset < byte_count) {
        size_t consumed = 0u;
        uint32_t value = 0u;
        if (rin_sscanf_first_multibyte(candidate + offset,
                                       byte_count - offset, &consumed,
                                       &value) <= 0) return -1;
        offset += consumed;
        ++wide_count;
    }
    if (suppress) return 0;
    wide_capacity = wide_count + (terminate ? 1u : 0u);
    if (spec->allocate) {
        wchar_t* storage;
        wchar_t** output = (wchar_t**)rin_sscanf_argument_pointer(
            arguments, spec, RIN_SSCANF_ARG_WCHAR_POINTER_POINTER);
        if (!output) { errno = EFAULT; return -1; }
        if (wide_capacity > SIZE_MAX / sizeof(wchar_t)) {
            errno = EOVERFLOW;
            return -1;
        }
        if (wide_capacity == 0u) wide_capacity = 1u;
        storage = (wchar_t*)rin_fscanf_candidate_alloc(
            wide_capacity * sizeof(wchar_t));
        if (!storage) { errno = ENOMEM; return -1; }
        if (rin_sscanf_decode_wide_candidate(candidate, byte_count, storage,
                                             wide_count, &decoded) != 0) {
            rin_fscanf_candidate_dealloc(storage);
            return -1;
        }
        if (terminate) storage[wide_count] = L'\0';
        *output = storage;
    } else {
        wchar_t* output = (wchar_t*)rin_sscanf_argument_pointer(
            arguments, spec, RIN_SSCANF_ARG_WCHAR_POINTER);
        if (!output) { errno = EFAULT; return -1; }
        if (rin_sscanf_decode_wide_candidate(candidate, byte_count, output,
                                             wide_count, &decoded) != 0)
            return -1;
        if (terminate) output[wide_count] = L'\0';
    }
    (void)decoded;
    return 0;
}

static int rin_sscanf_store_narrow_field(
        va_list* arguments, const struct RinSscanfSpec* spec,
        const char* candidate, size_t count, int terminate) {
    if (spec->suppress) return 0;
    if (spec->allocate) {
        size_t allocation_count = count + (terminate ? 1u : 0u);
        char* storage;
        char** output = (char**)rin_sscanf_argument_pointer(
            arguments, spec, RIN_SSCANF_ARG_CHAR_POINTER_POINTER);
        if (!output) { errno = EFAULT; return -1; }
        if (allocation_count == 0u) allocation_count = 1u;
        storage = (char*)rin_fscanf_candidate_alloc(allocation_count);
        if (!storage) { errno = ENOMEM; return -1; }
        for (size_t index = 0u; index < count; ++index)
            storage[index] = candidate[index];
        if (terminate) storage[count] = '\0';
        *output = storage;
    } else {
        char* output = (char*)rin_sscanf_argument_pointer(
            arguments, spec, RIN_SSCANF_ARG_CHAR_POINTER);
        if (!output) { errno = EFAULT; return -1; }
        for (size_t index = 0u; index < count; ++index)
            output[index] = candidate[index];
        if (terminate) output[count] = '\0';
    }
    return 0;
}

static int rin_sscanf_input_error_result(int assignments) {
    return assignments == 0 ? EOF : assignments;
}

/* vsscanf - unbounded string input (B47: the caller string is parsed in
 * place; no stack copy, no 4096 fixed rejection).  Format validation
 * happens before any variadic argument is read or caller output is
 * written. */
_RIN_STDIO_API int vsscanf(const char* string, const char* format, va_list ap) {
    const char* input = NULL;
    size_t input_size = 0u;
    size_t input_offset = 0u;
    size_t format_offset = 0u;
    int assignments = 0;
    va_list arguments;
    struct RinSscanfArgumentPlan argument_plan;
    /* GCC knows vsscanf as a builtin with nonnull format; the check
     * below intentionally tolerates a null format defensively. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull-compare"
#endif
    if (!string || !format) {
        errno = EINVAL;
        return EOF;
    }
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
    if (rin_sscanf_prepare_arguments(format, ap, &argument_plan, 0) != 0)
        return EOF;
    while (string[input_size] != '\0')
        ++input_size;
    input = string;
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
            if (rin_sscanf_parse_spec(format, &format_offset, &spec,
                                      0) != 0) {
                va_end(arguments);
                return rin_sscanf_input_error_result(assignments);
            }
            rin_sscanf_bind_argument(&spec, &argument_plan);
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
                size_t begin = 0u;
                size_t end = 0u;
                size_t scalar_count = 0u;
                int input_failure = 0;
                int parsed = rin_sscanf_wide_field_extent(
                    input, input_size, &input_offset, &spec, &begin,
                    &end, &scalar_count, &input_failure);
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
                        &arguments, &spec, input + begin, end - begin,
                        spec.suppress) != 0) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (!spec.suppress) ++assignments;
                continue;
            }
            if (spec.conversion == 'd' || spec.conversion == 'i' ||
                spec.conversion == 'b' || spec.conversion == 'u' ||
                spec.conversion == 'o' ||
                spec.conversion == 'x' || spec.conversion == 'X' ||
                spec.conversion == 'p') {
                uint64_t magnitude;
                int negative;
                int input_failure = 0;
                int range_error = 0;
                int parsed = rin_sscanf_parse_integer(
                    input, input_size, &input_offset, &spec, &magnitude,
                    &negative, &input_failure, &range_error);
                uint64_t clamp_probe = magnitude;
                if (parsed < 0 || (parsed > 0 &&
                    !rin_sscanf_clamp_integer_magnitude(&spec, &clamp_probe,
                                                        negative,
                                                        range_error, NULL))) {
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
                                                 negative, range_error) != 0) {
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
                size_t limit = spec.has_width ? spec.width : SIZE_MAX; /* B47: unbounded string field */
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
                    if (rin_sscanf_store_narrow_field(
                            &arguments, &spec, input + begin, count, 1) != 0) {
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    ++assignments;
                }
                continue;
            }
            if (spec.conversion == 'c') {
                /* BATCH41: short %c input assigns what is there (glibc);
                 * only zero available chars is input failure. */
                size_t count = spec.has_width ? spec.width : 1u;
                size_t available = input_size - input_offset;
                if (available == 0u) {
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (count > available) count = available;
                if (!spec.suppress) {
                    if (rin_sscanf_store_narrow_field(
                            &arguments, &spec, input + input_offset, count,
                            0) != 0) {
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
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
                size_t limit = spec.has_width ? spec.width : SIZE_MAX; /* B47: unbounded string field */
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
                    if (rin_sscanf_store_narrow_field(
                            &arguments, &spec, input + begin, count, 1) != 0) {
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    ++assignments;
                }
            }
        }
    }
    va_end(arguments);
    return assignments;
}

_RIN_STDIO_API int sscanf(const char* string, const char* format, ...) {
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
        const struct RinSscanfSpec* spec, struct RinFscanfCandidate* cand,
        size_t* scalar_count_out, int* input_failure_out) {
    size_t scalar_count = 0u;
    size_t scalar_limit = spec->has_width ? spec->width :
        (spec->conversion == 'c' ? 1u : SIZE_MAX);
    int status;
    if (!input || !spec || !cand ||
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
            /* BATCH41: partial %c assigns what is there (glibc). */
            if ((spec->conversion == 's' || spec->conversion == '[' ||
                 spec->conversion == 'c') &&
                scalar_count != 0u) {
                *input_failure_out = 0;
                break;
            }
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
            size_t scalar_begin = cand->count;
            size_t scalar_byte_count = 0u;
            size_t conversion;
            state.state = 0u;
            state.codepoint = 0u;
            for (;;) {
                /* B56: the candidate grows; no fixed capacity. */
                if (scalar_byte_count >= sizeof(scalar_bytes)) {
                    errno = EILSEQ;
                    return -1;
                }
                scalar_bytes[scalar_byte_count++] = byte;
                if (rin_fscanf_candidate_append(cand, (unsigned char)byte) != 0)
                    return -1;
                conversion = rin_sscanf_mbrtowc32(
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
                    cand->count = scalar_begin;
                    break;
                }
            }
            ++scalar_count;
        }
    }
    if (scalar_count == 0u) return 0;
    /* BATCH41: partial %c/%lc is success (see Fix C above). */
    *scalar_count_out = scalar_count;
    return 1;
}

static int rin_fscanf_stream_try_hex_prefix(
        struct RinFscanfStreamInput* input, size_t used, size_t limit,
        unsigned char* value, int* prefixed_out,
        int* matching_failure_out) {
    unsigned char prefix;
    unsigned char suffix;
    unsigned int digit;
    int status;
    *prefixed_out = 0;
    *matching_failure_out = 0;
    if (*value != '0' || limit - used < 2u) return 0;
    status = rin_fscanf_stream_read(input, &prefix);
    if (status < 0) return -1;
    if (status == 0) return 0;
    if (prefix != 'x' && prefix != 'X')
        return rin_fscanf_stream_unread(input, prefix);
    if (limit - used < 3u) {
        *matching_failure_out = 1;
        return 0;
    }
    status = rin_fscanf_stream_read(input, &suffix);
    if (status > 0 && rin_sscanf_digit(suffix, &digit) && digit < 16u) {
        *value = suffix;
        *prefixed_out = 1;
        return 0;
    }
    if (status < 0) return -1;
    if (status > 0 && rin_fscanf_stream_unread(input, suffix) != 0)
        return -1;
    *matching_failure_out = 1;
    return 0;
}

static int rin_fscanf_stream_try_binary_prefix(
        struct RinFscanfStreamInput* input, size_t used, size_t limit,
        unsigned char* value, int* prefixed_out,
        int* matching_failure_out) {
    unsigned char prefix;
    unsigned char suffix;
    unsigned int digit;
    int status;
    *prefixed_out = 0;
    *matching_failure_out = 0;
    if (*value != '0' || limit - used < 2u) return 0;
    status = rin_fscanf_stream_read(input, &prefix);
    if (status < 0) return -1;
    if (status == 0) return 0;
    if (prefix != 'b' && prefix != 'B')
        return rin_fscanf_stream_unread(input, prefix);
    if (limit - used < 3u) {
        *matching_failure_out = 1;
        return 0;
    }
    status = rin_fscanf_stream_read(input, &suffix);
    if (status > 0 && rin_sscanf_digit(suffix, &digit) && digit < 2u) {
        *value = suffix;
        *prefixed_out = 1;
        return 0;
    }
    if (status < 0) return -1;
    if (status > 0 && rin_fscanf_stream_unread(input, suffix) != 0)
        return -1;
    *matching_failure_out = 1;
    return 0;
}

static int rin_fscanf_stream_parse_integer(
        struct RinFscanfStreamInput* input,
        const struct RinSscanfSpec* spec, uint64_t* magnitude_out,
        int* negative_out, int* input_failure_out, unsigned int base,
        int* range_error_out) {
    unsigned char value = '\0';
    size_t used = 0u;
    size_t limit = spec->has_width ? spec->width : SIZE_MAX;
    uint64_t magnitude = 0u;
    int negative = 0;
    int prefixed = 0;
    int matching_failure = 0;
    int status = rin_fscanf_stream_skip_space(input);

    *input_failure_out = 0;
    if (range_error_out) *range_error_out = 0;
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
        /* B46: %p takes an optional sign like %x. */
        negative = value == '-';
        ++used;
        if (used == limit) return 0;
        status = rin_fscanf_stream_read(input, &value);
        if (status <= 0) {
            /* A consumed sign is a nonempty, non-convertible input item at
             * EOF, so this is matching failure rather than input failure. */
            return status < 0 ? -1 : 0;
        }
    }
    if (base == 0u || base == 16u) {
        if (rin_fscanf_stream_try_hex_prefix(
                input, used, limit, &value, &prefixed,
                &matching_failure) != 0)
            return -1;
        if (matching_failure) {
            /* B46: a bare 0x prefix converts 0 with the marker
             * consumed (glibc); the flag now means "prefix seen
             * without digits", not failure. */
            *magnitude_out = 0u;
            *negative_out = negative;
            return 1;
        }
    }
    if (prefixed) {
        base = 16u;
        used += 2u;
    } else if (base == 0u || base == 2u) {
        if (rin_fscanf_stream_try_binary_prefix(
                input, used, limit, &value, &prefixed,
                &matching_failure) != 0)
            return -1;
        if (matching_failure) {
            /* B46: a bare 0b prefix converts 0 with the marker
             * consumed (glibc); see the hex site above. */
            *magnitude_out = 0u;
            *negative_out = negative;
            return 1;
        }
        if (prefixed) {
            base = 2u;
            used += 2u;
        }
    }
    if (!prefixed && base == 0u) {
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
        /* BATCH41: saturate like the string parser (strtol rules); the
         * loop below keeps consuming digits and unreads the terminator. */
        if (magnitude > (UINT64_MAX - (uint64_t)digit) /
                            (uint64_t)base) {
            errno = ERANGE;
            if (range_error_out) *range_error_out = 1;
            magnitude = UINT64_MAX;
        } else {
            magnitude = magnitude * (uint64_t)base + (uint64_t)digit;
        }
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

static int rin_fscanf_stream_parse_float_candidate(
        struct RinFscanfCandidate* cand, const struct RinSscanfSpec* spec,
        struct RinSscanfFloat* value_out, int* input_failure_out) {
    size_t candidate_offset = 0u;
    int parser_input_failure = 0;
    int parsed;

    *input_failure_out = 0;
    /* B56: every successful append keeps the NUL slot writable, so the
     * terminator below never writes out of bounds. */
    cand->data[cand->count] = '\0';
    parsed = rin_sscanf_parse_float(cand->data, cand->count,
                                    &candidate_offset, spec,
                                    value_out, &parser_input_failure);
    if (parsed == 0 && parser_input_failure) *input_failure_out = 1;
    if (parsed > 0 && candidate_offset != cand->count) {
        errno = EIO;
        return -1;
    }
    return parsed;
}

/* Preserve the longest special-float input item. Partial `inf` words and
 * `infinity` suffixes fail as a whole instead of rolling back to the
 * shorter `inf` conversion; `nan(...)` payloads are never part of the
 * scanf item at all (B46: glibc stops after "nan"). */
static int rin_fscanf_stream_parse_special_float(
        struct RinFscanfStreamInput* input,
        const struct RinSscanfSpec* spec, struct RinFscanfCandidate* cand,
        unsigned char first, struct RinSscanfFloat* value_out,
        int* input_failure_out) {
    const char* base_suffix =
        rin_fscanf_stream_case_equal(first, 'i') ? "nf" : "an";
    size_t limit = spec->has_width ? spec->width : SIZE_MAX;
    size_t suffix_index;
    int status;
    unsigned char value = '\0';

    *input_failure_out = 0;
    if (rin_fscanf_candidate_append(cand, (unsigned char)first) != 0) return -1;
    for (suffix_index = 0u; base_suffix[suffix_index] != '\0';
         ++suffix_index) {
        if (cand->count == limit) return 0;
        status = rin_fscanf_stream_read(input, &value);
        if (status < 0) return -1;
        if (status == 0) {
            /* The initial i/n already began a nonempty input item. */
            return 0;
        }
        if (!rin_fscanf_stream_case_equal(value, base_suffix[suffix_index])) {
            /* B46: a failed inf/nan word keeps every consumed
             * byte (glibc); nothing goes back. */
            return 0;
        }
        if (rin_fscanf_candidate_append(cand, (unsigned char)value) != 0) return -1;
    }

    if (rin_fscanf_stream_case_equal(first, 'n') && cand->count < limit) {
        /* B46: scanf never consumes nan(...) payloads (glibc
         * stops after "nan"); only peek so the terminator goes
         * back. */
        status = rin_fscanf_stream_read(input, &value);
        if (status < 0) return -1;
        if (status > 0 && rin_fscanf_stream_unread(input, value) != 0)
            return -1;
    }

    if (rin_fscanf_stream_case_equal(first, 'i') && cand->count < limit) {
        static const char infinity_suffix[] = "nity";
        status = rin_fscanf_stream_read(input, &value);
        if (status < 0) return -1;
        if (status > 0 && rin_fscanf_stream_case_equal(value, 'i')) {
            if (rin_fscanf_candidate_append(cand, (unsigned char)value) != 0) return -1;
            for (suffix_index = 0u; infinity_suffix[suffix_index] != '\0';
                 ++suffix_index) {
                if (cand->count == limit) return 0;
                status = rin_fscanf_stream_read(input, &value);
                if (status < 0) return -1;
                if (status == 0) return 0;
                if (!rin_fscanf_stream_case_equal(
                        value, infinity_suffix[suffix_index])) {
                    /* B46: a failed infinity tail keeps every
                     * consumed byte (glibc); nothing goes back. */
                    return 0;
                }
                if (rin_fscanf_candidate_append(cand, (unsigned char)value) != 0) return -1;
            }
        } else if (status > 0 && rin_fscanf_stream_unread(input, value) != 0) {
            return -1;
        }
    }

    return rin_fscanf_stream_parse_float_candidate(
        cand, spec, value_out, input_failure_out);
}

/* Direct hexadecimal float lexer.  A rejected `0x` prefix is matching
 * failure with the prefix consumed (B46: glibc); only a width cut right
 * after the x falls back to the decimal zero conversion.  A partial
 * binary exponent still converts the mantissa it hangs off. */
static int rin_fscanf_stream_parse_hex_float(
        struct RinFscanfStreamInput* input,
        const struct RinSscanfSpec* spec, struct RinFscanfCandidate* cand,
        unsigned char prefix_marker, struct RinSscanfFloat* value_out,
        int* input_failure_out) {
    unsigned char value = '\0';
    unsigned char tail[3];
    unsigned char decimal_point =
        (unsigned char)rin_sscanf_decimal_point();
    size_t tail_count = 1u;
    size_t limit = spec->has_width ? spec->width : SIZE_MAX;
    int status;
    int any_digit = 0;
    int seen_point = 0;

    if (rin_fscanf_candidate_append(cand, (unsigned char)'0') != 0) return -1;
    if (rin_fscanf_candidate_append(cand, (unsigned char)prefix_marker) != 0) return -1;
    tail[0] = prefix_marker;
    if (cand->count == limit) goto decimal_zero;
    status = rin_fscanf_stream_read(input, &value);
    if (status < 0) return -1;
    /* B46: end of input after 0x is matching failure with the
     * prefix consumed; only a width cut falls back below. */
    if (status == 0) return 0;
    for (;;) {
        int digit = rin_fp_hex_digit((char)value);
        if (digit >= 0) {
            any_digit = 1;
            if (rin_fscanf_candidate_append(cand, (unsigned char)value) != 0) return -1;
        } else if (value == decimal_point && !seen_point) {
            seen_point = 1;
            if (rin_fscanf_candidate_append(cand, (unsigned char)value) != 0) return -1;
            tail[tail_count++] = value;
        } else {
            break;
        }
        if (cand->count == limit) {
            status = 2;
            break;
        }
        status = rin_fscanf_stream_read(input, &value);
        if (status <= 0) break;
    }
    if (status < 0) return -1;
    if (!any_digit) {
        if (seen_point) {
            /* B46: glibc converts "0x." (point, no hex digits)
             * as 0.0; the point stays consumed, only bytes past
             * it go back. At a width cut (status 2) or EOF the
             * value slot is stale, so only a fresh terminator
             * (status 1) joins the tail. */
            if (status == 1) tail[tail_count++] = value;
            if (rin_fscanf_stream_unread_float_tail(input, tail + 2u,
                                                    tail_count - 2u) != 0)
                return -1;
            value_out->bits = 0u;
            value_out->long_value = 0.0;
            return 1;
        }
        /* B46: matching failure; the 0x stays consumed and only
         * a fresh terminator (status 1, never a stale width-cut
         * or EOF slot) past it goes back. */
        if (status == 1) tail[tail_count++] = value;
        if (rin_fscanf_stream_unread_float_tail(input, tail + 1u,
                                                tail_count - 1u) != 0)
            return -1;
        return 0;
    }
    if (status == 0 || status == 2) goto parse_candidate;
    if (value == 'p' || value == 'P') {
        unsigned char marker = value;
        /* B46: a partial binary exponent still converts the
         * mantissa (glibc); the p stays consumed in the stream
         * while the candidate holds the valid prefix. */
        if (cand->count + 1u >= limit) {
            goto parse_candidate;
        }
        status = rin_fscanf_stream_read(input, &value);
        if (status < 0) return -1;
        if (status == 0) goto parse_candidate;
        if (value >= '0' && value <= '9') {
            if (rin_fscanf_candidate_append(cand, (unsigned char)marker) != 0) return -1;
            if (rin_fscanf_candidate_append(cand, (unsigned char)value) != 0) return -1;
        } else if (value == '+' || value == '-') {
            unsigned char sign = value;
            if (cand->count + 2u >= limit) goto parse_candidate;
            status = rin_fscanf_stream_read(input, &value);
            if (status < 0) return -1;
            if (status == 0) goto parse_candidate;
            if (value < '0' || value > '9') {
                if (rin_fscanf_stream_unread(input, value) != 0) return -1;
                goto parse_candidate;
            }
            if (rin_fscanf_candidate_append(cand, (unsigned char)marker) != 0) return -1;
            if (rin_fscanf_candidate_append(cand, (unsigned char)sign) != 0) return -1;
            if (rin_fscanf_candidate_append(cand, (unsigned char)value) != 0) return -1;
        } else {
            if (rin_fscanf_stream_unread(input, value) != 0) return -1;
            goto parse_candidate;
        }
        while (cand->count < limit) {
            status = rin_fscanf_stream_read(input, &value);
            if (status < 0) return -1;
            if (status == 0) break;
            if (value < '0' || value > '9') {
                if (rin_fscanf_stream_unread(input, value) != 0) return -1;
                break;
            }
            if (rin_fscanf_candidate_append(cand, (unsigned char)value) != 0) return -1;
        }
        goto parse_candidate;
    }
    if (rin_fscanf_stream_unread(input, value) != 0) return -1;

parse_candidate:
    return rin_fscanf_stream_parse_float_candidate(
        cand, spec, value_out, input_failure_out);

decimal_zero:
    --cand->count;
    if (rin_fscanf_stream_unread(input, tail[0]) != 0) return -1;
    return rin_fscanf_stream_parse_float_candidate(
        cand, spec, value_out, input_failure_out);
}

/* Direct float lexer.  A partial exponent still converts the mantissa it
 * hangs off (B46: glibc longest match); the e/E and its sign stay
 * consumed in the stream while the candidate holds the valid prefix. */
static int rin_fscanf_stream_parse_decimal_float_inner(
        struct RinFscanfStreamInput* input,
        const struct RinSscanfSpec* spec, struct RinFscanfCandidate* cand,
        struct RinSscanfFloat* value_out, int* input_failure_out) {
    unsigned char value = '\0';
    unsigned char tail[3];
    unsigned char decimal_point =
        (unsigned char)rin_sscanf_decimal_point();
    size_t limit = spec->has_width ? spec->width : SIZE_MAX;
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
        if (rin_fscanf_candidate_append(cand, (unsigned char)value) != 0) return -1;
        if (cand->count == limit) goto parse_candidate;
        status = rin_fscanf_stream_read(input, &value);
        if (status <= 0) {
            /* B46: a consumed sign is a nonempty, non-convertible
             * input item at EOF, so this is matching failure
             * rather than input failure. */
            return status < 0 ? -1 : 0;
        }
    }
    if (value == '0' && cand->count + 1u < limit) {
        status = rin_fscanf_stream_read(input, &value);
        if (status < 0) return -1;
        if (status > 0 && (value == 'x' || value == 'X'))
            return rin_fscanf_stream_parse_hex_float(
                input, spec, cand, value, value_out,
                input_failure_out);
        if (status > 0 && rin_fscanf_stream_unread(input, value) != 0)
            return -1;
        value = '0';
    }
    if (value == 'i' || value == 'I' || value == 'n' || value == 'N')
        return rin_fscanf_stream_parse_special_float(
            input, spec, cand, value, value_out,
            input_failure_out);
    for (;;) {
        if (value >= '0' && value <= '9') {
            any_digit = 1;
            if (rin_fscanf_candidate_append(cand, (unsigned char)value) != 0) return -1;
        } else if (value == decimal_point && !seen_point) {
            seen_point = 1;
            if (rin_fscanf_candidate_append(cand, (unsigned char)value) != 0) return -1;
        } else {
            break;
        }
        if (cand->count == limit) {
            status = 2;
            break;
        }
        status = rin_fscanf_stream_read(input, &value);
        if (status <= 0) break;
    }
    if (status < 0) return -1;
    if (!any_digit) {
        /* B46: a nonempty item that converts nothing is matching
         * failure, never input failure; the item's bytes stay
         * consumed and only the terminator (freshly read, status
         * == 1) goes back.  At EOF or a width cut (status 0/2)
         * there is no terminator to return. */
        if (status == 1 &&
            rin_fscanf_stream_unread(input, value) != 0)
            return -1;
        return 0;
    }
    if (status == 0 || status == 2) goto parse_candidate;
    if ((value == 'x' || value == 'X') && !seen_point &&
        cand->data[cand->count - 1u] == '0' &&
        (cand->count == 1u || (cand->count == 2u &&
                         (cand->data[0] == '+' || cand->data[0] == '-')))) {
        tail[0] = (unsigned char)cand->data[0];
        if (cand->count == 2u) tail[1] = (unsigned char)cand->data[1];
        tail[cand->count] = value;
        if (rin_fscanf_stream_unread_float_tail(input, tail, cand->count + 1u) != 0)
            return -1;
        return 0;
    }
    if (value == 'e' || value == 'E') {
        unsigned char marker = value;
        /* B46: a partial exponent still converts the mantissa
         * (glibc); the e stays consumed in the stream while the
         * candidate holds the valid prefix. */
        if (cand->count + 1u >= limit) {
            goto parse_candidate;
        }
        status = rin_fscanf_stream_read(input, &value);
        if (status < 0) return -1;
        if (status == 0) goto parse_candidate;
        if (value >= '0' && value <= '9') {
            if (rin_fscanf_candidate_append(cand, (unsigned char)marker) != 0) return -1;
            if (rin_fscanf_candidate_append(cand, (unsigned char)value) != 0) return -1;
        } else if (value == '+' || value == '-') {
            unsigned char sign = value;
            if (cand->count + 2u >= limit) goto parse_candidate;
            status = rin_fscanf_stream_read(input, &value);
            if (status < 0) return -1;
            if (status == 0) goto parse_candidate;
            if (value < '0' || value > '9') {
                if (rin_fscanf_stream_unread(input, value) != 0) return -1;
                goto parse_candidate;
            }
            if (rin_fscanf_candidate_append(cand, (unsigned char)marker) != 0) return -1;
            if (rin_fscanf_candidate_append(cand, (unsigned char)sign) != 0) return -1;
            if (rin_fscanf_candidate_append(cand, (unsigned char)value) != 0) return -1;
        } else {
            if (rin_fscanf_stream_unread(input, value) != 0) return -1;
            goto parse_candidate;
        }
        while (cand->count < limit) {
            status = rin_fscanf_stream_read(input, &value);
            if (status < 0) return -1;
            if (status == 0) break;
            if (value < '0' || value > '9') {
                if (rin_fscanf_stream_unread(input, value) != 0) return -1;
                break;
            }
            if (rin_fscanf_candidate_append(cand, (unsigned char)value) != 0) return -1;
        }
        goto parse_candidate;
    }
    if (rin_fscanf_stream_unread(input, value) != 0) return -1;

parse_candidate:
    return rin_fscanf_stream_parse_float_candidate(
        cand, spec, value_out, input_failure_out);
}

/* The decimal lexer borrows its candidate from this wrapper, so the
 * returns and gotos below share a single heap release. */
static int rin_fscanf_stream_parse_decimal_float(
        struct RinFscanfStreamInput* input,
        const struct RinSscanfSpec* spec, struct RinSscanfFloat* value_out,
        int* input_failure_out) {
    struct RinFscanfCandidate candidate;
    int parsed;
    rin_fscanf_candidate_init(&candidate);
    parsed = rin_fscanf_stream_parse_decimal_float_inner(
        input, spec, &candidate, value_out, input_failure_out);
    rin_fscanf_candidate_release(&candidate);
    return parsed;
}

/* Validate every supported stream conversion before the first read.  Unsupported
 * grammar stays outside the cursor so it cannot consume a partial line. */
static int rin_fscanf_validate_format(
        const char* format, struct RinSscanfArgumentPlan* plan) {
    size_t offset = 0u;
    if (rin_sscanf_validate_format(format, plan, 0) != 0) return -1;
    while (format[offset] != '\0') {
        struct RinSscanfSpec spec;
        if (format[offset++] != '%') continue;
        if (rin_sscanf_parse_spec(format, &offset, &spec, 0) != 0)
            return -1;
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
            spec.conversion == 'b' || spec.conversion == 'u' ||
            spec.conversion == 'o' ||
            spec.conversion == 'x' || spec.conversion == 'X' ||
            spec.conversion == 'p')
            continue;
        errno = ENOSYS;
        return -1;
    }
    return 0;
}

/* Direct byte-stream subset: literals, whitespace, %%, %c, %s, %n,
 * and integer/pointer conversions.  Width-omitted fields are unbounded
 * on every path (B56: growable heap candidates); explicit widths are
 * honored.  A delimiter is put back before caller output is published. */
static inline int _rin_vfscanf_unlocked(FILE* stream, const char* format, va_list ap) {
    struct RinFscanfStreamInput input;
    struct RinSscanfArgumentPlan argument_plan;
    va_list arguments;
    size_t format_offset = 0u;
    int assignments = 0;
    if (!stream || !format) {
        errno = EINVAL;
        return EOF;
    }
    if (rin_fscanf_validate_format(format, &argument_plan) != 0) return EOF;
    if (rin_sscanf_capture_positional_arguments(ap, &argument_plan) != 0)
        return EOF;
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
            ++format_offset;
            if (rin_sscanf_parse_spec(format, &format_offset, &spec,
                                      0) != 0) {
                va_end(arguments);
                return rin_sscanf_input_error_result(assignments);
            }
            rin_sscanf_bind_argument(&spec, &argument_plan);
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
                struct RinFscanfCandidate candidate;
                size_t scalar_count = 0u;
                int input_failure = 0;
                int parsed;
                rin_fscanf_candidate_init(&candidate);
                parsed = rin_fscanf_stream_wide_field(
                    &input, &spec, &candidate, &scalar_count,
                    &input_failure);
                (void)scalar_count;
                if (parsed < 0) {
                    rin_fscanf_candidate_release(&candidate);
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (parsed == 0) {
                    rin_fscanf_candidate_release(&candidate);
                    va_end(arguments);
                    return input_failure ?
                        rin_sscanf_input_error_result(assignments) : assignments;
                }
                if (rin_sscanf_store_wide_field(
                        &arguments, &spec, candidate.data, candidate.count,
                        spec.suppress) != 0) {
                    rin_fscanf_candidate_release(&candidate);
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                rin_fscanf_candidate_release(&candidate);
                if (!spec.suppress) ++assignments;
                continue;
            }
            if (spec.conversion == 'd' || spec.conversion == 'i' ||
                spec.conversion == 'b' || spec.conversion == 'u' ||
                spec.conversion == 'o' ||
                spec.conversion == 'x' || spec.conversion == 'X' ||
                spec.conversion == 'p') {
                uint64_t magnitude = 0u;
                int negative = 0;
                int input_failure = 0;
                int range_error = 0;
                int parsed = rin_fscanf_stream_parse_integer(
                    &input, &spec, &magnitude, &negative, &input_failure,
                    spec.conversion == 'b' ? 2u :
                    spec.conversion == 'o' ? 8u :
                    (spec.conversion == 'x' || spec.conversion == 'X' ||
                     spec.conversion == 'p') ?
                    16u : spec.conversion == 'i' ? 0u : 10u,
                    &range_error);
                uint64_t clamp_probe = magnitude;
                if (parsed < 0 || (parsed > 0 &&
                    !rin_sscanf_clamp_integer_magnitude(&spec, &clamp_probe,
                                                        negative,
                                                        range_error, NULL))) {
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
                                                 negative, range_error) != 0) {
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
                size_t limit = spec.has_width ? spec.width : SIZE_MAX;
                struct RinFscanfCandidate candidate;
                rin_fscanf_candidate_init(&candidate);
                while (candidate.count < limit) {
                    unsigned char value = '\0';
                    status = rin_fscanf_stream_read(&input, &value);
                    if (status < 0) {
                        rin_fscanf_candidate_release(&candidate);
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    if (status == 0) break;
                    if (!rin_sscanf_scanset_contains(&spec, value)) {
                        if (rin_fscanf_stream_unread(&input, value) != 0) {
                            rin_fscanf_candidate_release(&candidate);
                            va_end(arguments);
                            return rin_sscanf_input_error_result(assignments);
                        }
                        break;
                    }
                    if (rin_fscanf_candidate_append(&candidate, value) != 0) {
                        rin_fscanf_candidate_release(&candidate);
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                }
                if (candidate.count == 0u) {
                    rin_fscanf_candidate_release(&candidate);
                    va_end(arguments);
                    return status == 0 ?
                        rin_sscanf_input_error_result(assignments) : assignments;
                }
                if (!spec.suppress) {
                    if (rin_sscanf_store_narrow_field(
                            &arguments, &spec, candidate.data, candidate.count, 1) != 0) {
                        rin_fscanf_candidate_release(&candidate);
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    ++assignments;
                }
                rin_fscanf_candidate_release(&candidate);
                continue;
            }
            if (spec.conversion == 'c') {
                /* BATCH41: partial %c assigns what is there (glibc). */
                size_t want = spec.has_width ? spec.width : 1u;
                struct RinFscanfCandidate candidate;
                rin_fscanf_candidate_init(&candidate);
                for (size_t index = 0u; index < want; ++index) {
                    unsigned char value = '\0';
                    int status = rin_fscanf_stream_read(&input, &value);
                    if (status < 0) {
                        rin_fscanf_candidate_release(&candidate);
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    if (status == 0) break;
                    if (rin_fscanf_candidate_append(&candidate, value) != 0) {
                        rin_fscanf_candidate_release(&candidate);
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                }
                if (candidate.count == 0u) {
                    rin_fscanf_candidate_release(&candidate);
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (!spec.suppress) {
                    if (rin_sscanf_store_narrow_field(
                            &arguments, &spec, candidate.data, candidate.count, 0) != 0) {
                        rin_fscanf_candidate_release(&candidate);
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    ++assignments;
                }
                rin_fscanf_candidate_release(&candidate);
                continue;
            }
            {
                int status = rin_fscanf_stream_skip_space(&input);
                size_t limit = spec.has_width ? spec.width : SIZE_MAX;
                struct RinFscanfCandidate candidate;
                rin_fscanf_candidate_init(&candidate);
                if (status <= 0) {
                    rin_fscanf_candidate_release(&candidate);
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                while (candidate.count < limit) {
                    unsigned char value = '\0';
                    status = rin_fscanf_stream_read(&input, &value);
                    if (status < 0) {
                        rin_fscanf_candidate_release(&candidate);
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    if (status == 0) break;
                    if (rin_sscanf_space(value)) {
                        if (rin_fscanf_stream_unread(&input, value) != 0) {
                            rin_fscanf_candidate_release(&candidate);
                            va_end(arguments);
                            return rin_sscanf_input_error_result(assignments);
                        }
                        break;
                    }
                    if (rin_fscanf_candidate_append(&candidate, value) != 0) {
                        rin_fscanf_candidate_release(&candidate);
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                }
                if (candidate.count == 0u) {
                    rin_fscanf_candidate_release(&candidate);
                    va_end(arguments);
                    return rin_sscanf_input_error_result(assignments);
                }
                if (!spec.suppress) {
                    if (rin_sscanf_store_narrow_field(
                            &arguments, &spec, candidate.data, candidate.count, 1) != 0) {
                        rin_fscanf_candidate_release(&candidate);
                        va_end(arguments);
                        return rin_sscanf_input_error_result(assignments);
                    }
                    ++assignments;
                }
                rin_fscanf_candidate_release(&candidate);
            }
        }
    }
    va_end(arguments);
    return assignments;
}

_RIN_STDIO_API int vfscanf(FILE* stream, const char* format, va_list ap) {
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

_RIN_STDIO_API int vscanf(const char* format, va_list ap) {
    return vfscanf(stdin, format, ap);
}

_RIN_STDIO_API int fscanf(FILE* stream, const char* format, ...) {
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = vfscanf(stream, format, arguments);
    va_end(arguments);
    return result;
}

_RIN_STDIO_API int scanf(const char* format, ...) {
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = vscanf(format, arguments);
    va_end(arguments);
    return result;
}

#endif /* _RIN_STDIO_SCANF_IMPL_H */
