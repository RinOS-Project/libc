/* SPDX-License-Identifier: MIT */
/* C23 type-specific floating-point formatting functions. */

#include "stdlib.h"
#include "stdio.h"
#include "limits.h"

typedef struct RinStrfromFormat {
    char conversion;
    int has_precision;
    int precision;
} RinStrfromFormat;

static int rin_strfrom_parse_format(const char* input,
                                    RinStrfromFormat* parsed) {
    const unsigned char* cursor;
    unsigned int precision = 0u;
    int has_precision = 0;
    char conversion;

    if (input == NULL || parsed == NULL || input[0] != '%') goto invalid;
    cursor = (const unsigned char*)input + 1u;
    if (*cursor == '.') {
        has_precision = 1;
        ++cursor;
        while (*cursor >= '0' && *cursor <= '9') {
            const unsigned int digit = (unsigned int)(*cursor - '0');
            if (precision > ((unsigned int)INT_MAX - digit) / 10u) {
                errno = EOVERFLOW;
                return -1;
            }
            precision = precision * 10u + digit;
            ++cursor;
        }
    }

    if (*cursor == '\0') goto invalid;
    conversion = (char)*cursor++;
    if (*cursor != '\0' ||
        (conversion != 'a' && conversion != 'A' &&
         conversion != 'e' && conversion != 'E' &&
         conversion != 'f' && conversion != 'F' &&
         conversion != 'g' && conversion != 'G'))
        goto invalid;

    parsed->conversion = conversion;
    parsed->has_precision = has_precision;
    parsed->precision = (int)precision;
    return 0;

invalid:
    errno = EINVAL;
    return -1;
}

static size_t rin_strfrom_append_unsigned(char* output, unsigned int value) {
    char digits[10];
    size_t count = 0u;
    size_t index;
    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    for (index = 0u; index < count; ++index)
        output[index] = digits[count - index - 1u];
    return count;
}

static void rin_strfrom_make_format(char output[16],
                                    const RinStrfromFormat* parsed,
                                    int long_double) {
    size_t offset = 0u;
    unsigned int precision = (unsigned int)parsed->precision;

    output[offset++] = '%';
    if (parsed->has_precision) {
        output[offset++] = '.';
        offset += rin_strfrom_append_unsigned(output + offset, precision);
    }
    if (long_double) output[offset++] = 'L';
    output[offset++] = parsed->conversion;
    output[offset] = '\0';
}

static int rin_strfrom_output(char* restrict output, size_t capacity,
                              const char* restrict format, double value) {
    RinStrfromFormat parsed;
    char normalized[16];
    if ((capacity != 0u && output == NULL) ||
        rin_strfrom_parse_format(format, &parsed) != 0) {
        if (capacity != 0u && output == NULL) errno = EINVAL;
        return -1;
    }
    rin_strfrom_make_format(normalized, &parsed, 0);
    return snprintf(output, capacity, normalized, value);
}

int strfromd(char* restrict s, size_t n, const char* restrict format,
             double fp) {
    return rin_strfrom_output(s, n, format, fp);
}

int strfromf(char* restrict s, size_t n, const char* restrict format,
             float fp) {
    return rin_strfrom_output(s, n, format, (double)fp);
}

int strfroml(char* restrict s, size_t n, const char* restrict format,
             long double fp) {
    RinStrfromFormat parsed;
#if defined(__RINOS__)
    /* RinOS long double shares double's binary64 representation and ABI. */
    if ((n != 0u && s == NULL) ||
        rin_strfrom_parse_format(format, &parsed) != 0) {
        if (n != 0u && s == NULL) errno = EINVAL;
        return -1;
    }
    char normalized[16];
    rin_strfrom_make_format(normalized, &parsed, 0);
    return snprintf(s, n, normalized, (double)fp);
#else
    if ((n != 0u && s == NULL) ||
        rin_strfrom_parse_format(format, &parsed) != 0) {
        if (n != 0u && s == NULL) errno = EINVAL;
        return -1;
    }
    char normalized[16];
    rin_strfrom_make_format(normalized, &parsed, 1);
    return snprintf(s, n, normalized, fp);
#endif
}
