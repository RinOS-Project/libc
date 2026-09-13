/* SPDX-License-Identifier: MIT */

#include "time.h"
#include "wchar.h"

typedef struct {
    wchar_t* output;
    size_t capacity;
    size_t length;
    int error;
#if defined(RIN_USERSPACE)
    locale_t locale;
#endif
} RinWcsftimeWriter;

static void rin_wcsftime_scalar(RinWcsftimeWriter* writer, uint32_t scalar) {
    size_t units = 1u;
    if (writer->error != 0) return;
    if (!rin_unicode_is_valid_scalar(scalar)) {
        writer->error = EILSEQ;
        return;
    }
    if (sizeof(wchar_t) == 2u && scalar > 0xffffu) units = 2u;
    if (writer->length > SIZE_MAX - units) {
        writer->error = EOVERFLOW;
        return;
    }
    if (writer->output) {
        if (writer->length > writer->capacity ||
            units > writer->capacity - writer->length) {
            writer->error = EOVERFLOW;
            return;
        }
        if (units == 2u) {
            uint32_t value = scalar - 0x10000u;
            writer->output[writer->length] =
                (wchar_t)(0xd800u + (value >> 10u));
            writer->output[writer->length + 1u] =
                (wchar_t)(0xdc00u + (value & 0x3ffu));
        } else {
            writer->output[writer->length] = (wchar_t)scalar;
        }
    }
    writer->length += units;
}

static void rin_wcsftime_utf8(RinWcsftimeWriter* writer, const char* text,
                              size_t length) {
    size_t offset = 0u;
    while (writer->error == 0 && offset < length) {
        uint32_t scalar = 0u;
        size_t consumed = 0u;
        if (rin_unicode_decode_utf8(text + offset, length - offset, &scalar,
                                    &consumed) != RIN_UNICODE_OK ||
            consumed == 0u) {
            writer->error = EILSEQ;
            return;
        }
        rin_wcsftime_scalar(writer, scalar);
        offset += consumed;
    }
}

static int rin_wcsftime_conversion_supported(uint32_t conversion) {
    switch (conversion) {
        case 'a': case 'A': case 'b': case 'B': case 'c': case 'C':
        case 'd': case 'D': case 'e': case 'F': case 'g': case 'G':
        case 'h': case 'H': case 'I': case 'j': case 'm': case 'M':
        case 'n': case 'p': case 'r': case 'R': case 'S': case 't':
        case 'T': case 'u': case 'U': case 'V': case 'w': case 'W':
        case 'x': case 'X': case 'y': case 'Y': case 'z': case 'Z':
        case '%':
            return 1;
        default:
            return 0;
    }
}

static int rin_wcsftime_tm_valid(const struct tm* value) {
    if (!value) return 0;
    if (value->tm_sec < 0 || value->tm_sec > 60 ||
        value->tm_min < 0 || value->tm_min > 59 ||
        value->tm_hour < 0 || value->tm_hour > 23 ||
        value->tm_mday < 1 || value->tm_mday > 31 ||
        value->tm_mon < 0 || value->tm_mon > 11 ||
        value->tm_year < -1900 || value->tm_year > 8099 ||
        value->tm_wday < 0 || value->tm_wday > 6 ||
        value->tm_yday < 0 || value->tm_yday > 365) {
        return 0;
    }
    return 1;
}

/* Decode one format character without treating a UTF-16 surrogate pair as
 * two independent (and invalid) Unicode scalars on Windows. */
static int rin_wcsftime_next_scalar(const wchar_t** format,
                                    uint32_t* scalar) {
    uint32_t first;
    if (!format || !*format || !scalar || **format == L'\0') return 0;
    first = (uint32_t)*(*format)++;
    if (sizeof(wchar_t) == 2u) {
        if (first >= 0xd800u && first <= 0xdbffu) {
            uint32_t second;
            if (**format == L'\0') return 0;
            second = (uint32_t)*(*format)++;
            if (second < 0xdc00u || second > 0xdfffu) return 0;
            first = 0x10000u + ((first - 0xd800u) << 10u) +
                   (second - 0xdc00u);
        } else if (first >= 0xdc00u && first <= 0xdfffu) {
            return 0;
        }
    }
    if (!rin_unicode_is_valid_scalar(first)) return 0;
    *scalar = first;
    return 1;
}

static int rin_wcsftime_format(RinWcsftimeWriter* writer,
                               const wchar_t* format,
                               const struct tm* value
#if defined(RIN_USERSPACE)
                               , locale_t locale
#endif
                               ) {
    while (writer->error == 0 && *format != L'\0') {
        uint32_t scalar;
        if (!rin_wcsftime_next_scalar(&format, &scalar)) {
            writer->error = EILSEQ;
            break;
        }
        if (scalar != (uint32_t)'%') {
            rin_wcsftime_scalar(writer, scalar);
            continue;
        }

        {
            uint32_t modifier = 0u;
            char directive[4];
            size_t directive_length = 0u;
            char expanded[256];
            size_t expanded_length;
            int expansion_errno;

            scalar = (uint32_t)*format++;
            if (scalar == (uint32_t)'E' || scalar == (uint32_t)'O') {
                modifier = scalar;
                scalar = (uint32_t)*format++;
            }
            if (!rin_wcsftime_conversion_supported(scalar)) {
                writer->error = EINVAL;
                break;
            }

            directive[directive_length++] = '%';
            if (modifier != 0u) directive[directive_length++] = (char)modifier;
            directive[directive_length++] = (char)scalar;
            directive[directive_length] = '\0';
#if defined(RIN_USERSPACE)
            expanded_length = strftime_l(expanded, sizeof(expanded),
                                         directive, value, locale);
#else
            expanded_length = strftime(expanded, sizeof(expanded),
                                       directive, value);
#endif
            expansion_errno = errno;
            if (expanded_length == 0u ||
                expanded_length == sizeof(expanded) - 1u) {
                writer->error = expanded_length == sizeof(expanded) - 1u
                                    ? EOVERFLOW
                                    : (expansion_errno == ERANGE
                                           ? EOVERFLOW
                                           : expansion_errno == EILSEQ
                                                 ? EILSEQ
                                                 : EINVAL);
                break;
            }
            rin_wcsftime_utf8(writer, expanded, expanded_length);
        }
    }
    return writer->error == 0;
}

static size_t rin_wcsftime_run(wchar_t* destination, size_t maximum,
                               const wchar_t* format,
                               const struct tm* value
#if defined(RIN_USERSPACE)
                               , locale_t locale
#endif
                               ) {
    RinWcsftimeWriter sizing = {NULL, 0u, 0u, 0
#if defined(RIN_USERSPACE)
                                 , 0
#endif
    };
    RinWcsftimeWriter output;
    int saved_errno = errno;

    if (!destination || !format || !rin_wcsftime_tm_valid(value)) {
        errno = EINVAL;
        return 0u;
    }
#if defined(RIN_USERSPACE)
    sizing.locale = locale;
#endif
    if (!rin_wcsftime_format(&sizing, format, value
#if defined(RIN_USERSPACE)
                             , locale
#endif
                             )) {
        errno = sizing.error;
        return 0u;
    }
    if (maximum == 0u || sizing.length >= maximum) {
        errno = ERANGE;
        return 0u;
    }

    output.output = destination;
    output.capacity = sizing.length;
    output.length = 0u;
    output.error = 0;
#if defined(RIN_USERSPACE)
    output.locale = locale;
#endif
    if (!rin_wcsftime_format(&output, format, value
#if defined(RIN_USERSPACE)
                             , locale
#endif
                             ) ||
        output.length != sizing.length) {
        destination[0] = L'\0';
        errno = output.error != 0 ? output.error : EIO;
        return 0u;
    }
    destination[output.length] = L'\0';
    errno = saved_errno;
    return output.length;
}

size_t wcsftime(wchar_t* destination, size_t maximum,
                const wchar_t* format, const struct tm* value) {
#if defined(RIN_USERSPACE)
    return rin_wcsftime_run(destination, maximum, format, value,
                            LC_GLOBAL_LOCALE);
#else
    return rin_wcsftime_run(destination, maximum, format, value);
#endif
}

#if defined(RIN_USERSPACE)
size_t wcsftime_l(wchar_t* destination, size_t maximum,
                  const wchar_t* format, const struct tm* value,
                  locale_t locale) {
    if (!locale) {
        errno = EINVAL;
        return 0u;
    }
    return rin_wcsftime_run(destination, maximum, format, value, locale);
}
#endif
