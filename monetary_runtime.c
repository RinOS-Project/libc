/* SPDX-License-Identifier: MIT */
#include "monetary.h"

#include "errno.h"
#include "limits.h"
#include "stdarg.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

typedef struct RinMonetaryBuffer {
    char* data;
    size_t length;
    size_t capacity;
    size_t limit;
} RinMonetaryBuffer;

typedef struct RinMonetarySpec {
    char fill;
    size_t width;
    size_t left_precision;
    size_t right_precision;
    int has_left_precision;
    int has_right_precision;
    int no_grouping;
    int suppress_symbol;
    int left_justify;
    int parentheses;
    int show_positive_sign;
    int conversion;
} RinMonetarySpec;

static void rin_monetary_buffer_destroy(RinMonetaryBuffer* buffer) {
    if (!buffer) return;
    free(buffer->data);
    buffer->data = NULL;
    buffer->length = 0u;
    buffer->capacity = 0u;
}

static int rin_monetary_buffer_reserve(RinMonetaryBuffer* buffer,
                                       size_t extra) {
    size_t required;
    size_t capacity;
    char* grown;
    if (extra > buffer->limit - buffer->length) {
        errno = E2BIG;
        return 0;
    }
    required = buffer->length + extra + 1u;
    if (required <= buffer->capacity) return 1;
    capacity = buffer->capacity ? buffer->capacity : 32u;
    while (capacity < required) {
        if (capacity > ((size_t)-1) / 2u) {
            capacity = required;
            break;
        }
        capacity *= 2u;
    }
    grown = (char*)realloc(buffer->data, capacity);
    if (!grown) {
        errno = ENOMEM;
        return 0;
    }
    buffer->data = grown;
    buffer->capacity = capacity;
    return 1;
}

static int rin_monetary_append(RinMonetaryBuffer* buffer,
                               const char* text, size_t length) {
    if (length == 0u) return 1;
    if (!text || !rin_monetary_buffer_reserve(buffer, length)) return 0;
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return 1;
}

static size_t rin_monetary_strlen(const char* text) {
    size_t length = 0u;
    if (!text) return 0u;
    while (text[length] != '\0') ++length;
    return length;
}

static int rin_monetary_repeat(RinMonetaryBuffer* buffer, char value,
                               size_t count) {
    size_t index;
    if (count == 0u) return 1;
    if (!rin_monetary_buffer_reserve(buffer, count)) return 0;
    for (index = 0u; index < count; ++index)
        buffer->data[buffer->length + index] = value;
    buffer->length += count;
    buffer->data[buffer->length] = '\0';
    return 1;
}

static int rin_monetary_parse_number(const char** cursor, size_t* value) {
    const char* text = *cursor;
    size_t parsed = 0u;
    if (*text < '0' || *text > '9') return 0;
    do {
        unsigned int digit = (unsigned int)(*text - '0');
        if (parsed > (((size_t)-1) - digit) / 10u) {
            errno = EOVERFLOW;
            return -1;
        }
        parsed = parsed * 10u + digit;
        ++text;
    } while (*text >= '0' && *text <= '9');
    *cursor = text;
    *value = parsed;
    return 1;
}

static int rin_monetary_parse_spec(const char** cursor,
                                   RinMonetarySpec* spec) {
    const char* text = *cursor;
    int sign_style = 0;
    int parsed;
    memset(spec, 0, sizeof(*spec));
    spec->fill = ' ';
    while (*text != '\0') {
        switch (*text) {
            case '=':
                ++text;
                if (*text == '\0') goto invalid;
                spec->fill = *text++;
                continue;
            case '^': spec->no_grouping = 1; ++text; continue;
            case '+':
                if (sign_style == 2) goto invalid;
                sign_style = 1;
                ++text;
                continue;
            case '(':
                if (sign_style == 1) goto invalid;
                sign_style = 2;
                spec->parentheses = 1;
                ++text;
                continue;
            case '!': spec->suppress_symbol = 1; ++text; continue;
            case '-': spec->left_justify = 1; ++text; continue;
            default: break;
        }
        break;
    }
    spec->show_positive_sign = sign_style == 1;
    parsed = rin_monetary_parse_number(&text, &spec->width);
    if (parsed < 0) return 0;
    if (*text == '#') {
        ++text;
        parsed = rin_monetary_parse_number(&text, &spec->left_precision);
        if (parsed < 0) return 0;
        if (parsed == 0) goto invalid;
        spec->has_left_precision = 1;
    }
    if (*text == '.') {
        ++text;
        parsed = rin_monetary_parse_number(&text, &spec->right_precision);
        if (parsed < 0) return 0;
        if (parsed == 0) goto invalid;
        spec->has_right_precision = 1;
    }
    if (*text != 'n' && *text != 'i' && *text != '%') goto invalid;
    spec->conversion = *text++;
    if (spec->conversion == '%' && text != *cursor + 2) goto invalid;
    *cursor = text;
    return 1;

invalid:
    errno = EINVAL;
    return 0;
}

static int rin_monetary_append_grouped_integer(
    RinMonetaryBuffer* output, const char* digits, size_t digit_count,
    const char* grouping, const char* separator, int enabled) {
    size_t grouping_index = 0u;
    size_t remaining;
    unsigned int group = 0u;
    size_t index;
    unsigned char* break_before = NULL;
    if (enabled && grouping && separator && separator[0] != '\0') {
        group = (unsigned char)grouping[0];
        if (group != 0u && group != (unsigned char)CHAR_MAX &&
            digit_count > group) {
            break_before = (unsigned char*)calloc(digit_count, 1u);
            if (!break_before) {
                errno = ENOMEM;
                return 0;
            }
            remaining = digit_count;
            while (group != 0u && group != (unsigned char)CHAR_MAX &&
                   remaining > group) {
                remaining -= group;
                break_before[remaining] = 1u;
                if ((unsigned char)grouping[grouping_index + 1u] != 0u) {
                    ++grouping_index;
                    group = (unsigned char)grouping[grouping_index];
                }
            }
        }
    }
    for (index = 0u; index < digit_count; ++index) {
        if (break_before && break_before[index]) {
            if (!rin_monetary_append(output, separator,
                                     rin_monetary_strlen(separator))) {
                free(break_before);
                return 0;
            }
        }
        if (!rin_monetary_append(output, digits + index, 1u)) {
            free(break_before);
            return 0;
        }
    }
    free(break_before);
    return 1;
}

static int rin_monetary_decimal_digits(double amount, size_t precision,
                                      const char* numeric_point,
                                      size_t output_limit, char** digits_out,
                                      size_t* integer_count_out,
                                      size_t* fraction_count_out,
                                      int* negative_out) {
    int needed;
    int written;
    size_t format_length = 0u;
    char format[32];
    char precision_digits[24];
    size_t precision_count = 0u;
    size_t value = precision;
    size_t index;
    size_t number_length;
    char* number;
    const char* start;
    const char* point;
    size_t point_length = rin_monetary_strlen(numeric_point);

    if (!numeric_point || point_length == 0u) {
        numeric_point = ".";
        point_length = 1u;
    }
    if (precision > output_limit) {
        errno = E2BIG;
        return 0;
    }
    do {
        precision_digits[precision_count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u && precision_count < sizeof(precision_digits));
    if (value != 0u) {
        errno = EOVERFLOW;
        return 0;
    }
    format[format_length++] = '%';
    format[format_length++] = '.';
    for (index = precision_count; index != 0u; --index)
        format[format_length++] = precision_digits[index - 1u];
    format[format_length++] = 'f';
    format[format_length] = '\0';

    needed = snprintf(NULL, 0u, format, amount);
    if (needed < 0) {
        errno = EINVAL;
        return 0;
    }
    if ((size_t)needed > output_limit) {
        errno = E2BIG;
        return 0;
    }
    number = (char*)malloc((size_t)needed + 1u);
    if (!number) {
        errno = ENOMEM;
        return 0;
    }
    written = snprintf(number, (size_t)needed + 1u, format, amount);
    if (written != needed) {
        free(number);
        errno = EINVAL;
        return 0;
    }
    number_length = (size_t)needed;
    start = number;
    *negative_out = number_length != 0u && number[0] == '-';
    if (*negative_out) ++start;
    point = NULL;
    if (precision != 0u) {
        for (index = 0u; index + point_length <=
             number_length - (size_t)(start - number); ++index) {
            size_t match = 0u;
            while (match < point_length && start[index + match] ==
                   numeric_point[match])
                ++match;
            if (match == point_length) {
                point = start + index;
                break;
            }
        }
        if (!point) {
            free(number);
            errno = EINVAL;
            return 0;
        }
        *integer_count_out = (size_t)(point - start);
        *fraction_count_out = (size_t)(number + number_length -
                                       (point + point_length));
        memmove(number, start, *integer_count_out);
        memmove(number + *integer_count_out, point + point_length,
                *fraction_count_out);
    } else {
        *integer_count_out = number_length - (size_t)(start - number);
        *fraction_count_out = 0u;
    }
    if (*integer_count_out == 0u) {
        free(number);
        errno = EINVAL;
        return 0;
    }
    /* Keep the integer and fractional digits contiguous for the formatter. */
    if (precision == 0u && start != number)
        memmove(number, start, *integer_count_out);
    number[*integer_count_out + *fraction_count_out] = '\0';
    *digits_out = number;
    return 1;
}

static int rin_monetary_make_amount(
    RinMonetaryBuffer* amount, const RinMonetarySpec* spec, double value,
    size_t precision, const struct lconv* locale_data,
    const char* monetary_point, const char* monetary_separator,
    const char* numeric_point, int* negative_out) {
    char* digits = NULL;
    size_t integer_count = 0u;
    size_t fraction_count = 0u;
    size_t fill_count = 0u;
    size_t amount_limit = amount->limit;
    RinMonetaryBuffer integer = {0};
    const char* grouping = locale_data->mon_grouping;
    unsigned char group_enabled;

    if (!rin_monetary_decimal_digits(value, precision, numeric_point,
                                     amount_limit, &digits, &integer_count,
                                     &fraction_count, negative_out))
        return 0;
    if (spec->has_left_precision) {
        if (integer_count > spec->left_precision) {
            free(digits);
            return 2; /* POSIX: ignore a conversion that exceeds #n. */
        }
        fill_count = spec->left_precision - integer_count;
    }
    integer.limit = amount_limit;
    if (!rin_monetary_repeat(&integer, spec->fill, fill_count)) goto fail;
    group_enabled = spec->no_grouping ? 0u : 1u;
    if (!rin_monetary_append_grouped_integer(
            &integer, digits, integer_count, grouping, monetary_separator,
            group_enabled))
        goto fail;
    if (!rin_monetary_append(amount, integer.data ? integer.data : "",
                             integer.length))
        goto fail;
    if (precision != 0u &&
        (!rin_monetary_append(amount, monetary_point,
                              rin_monetary_strlen(monetary_point)) ||
         !rin_monetary_append(amount,
                              digits + integer_count,
                              fraction_count)))
        goto fail;
    free(digits);
    rin_monetary_buffer_destroy(&integer);
    return 1;

fail:
    free(digits);
    rin_monetary_buffer_destroy(&integer);
    return 0;
}

static int rin_monetary_make_conversion(
    RinMonetaryBuffer* converted, const RinMonetarySpec* spec, double value,
    const struct lconv* data) {
    RinMonetaryBuffer amount = {0};
    RinMonetaryBuffer base = {0};
    const char* symbol;
    const char* separator;
    const char* decimal_point;
    const char* numeric_point;
    const char* sign;
    const char* other_sign;
    unsigned int symbol_precedes;
    unsigned int separator_by_space;
    unsigned int sign_position;
    size_t precision;
    size_t sign_width;
    int negative = 0;
    int monetary_result;

    symbol = spec->conversion == 'i' ? data->int_curr_symbol
                                     : data->currency_symbol;
    separator = data->mon_thousands_sep ? data->mon_thousands_sep : "";
    decimal_point = data->mon_decimal_point;
    if (!decimal_point || decimal_point[0] == '\0')
        decimal_point = ".";
    /* RinOS printf emits an ASCII radix point. Normalize that result into
     * the LC_MONETARY radix, which can differ from LC_NUMERIC. */
    numeric_point = ".";

    if (spec->conversion == 'i') {
        precision = (unsigned char)data->int_frac_digits;
    } else {
        precision = (unsigned char)data->frac_digits;
    }
    if (spec->conversion == 'i' &&
        precision == (unsigned char)CHAR_MAX)
        precision = (unsigned char)data->frac_digits;
    if (precision == (unsigned char)CHAR_MAX) precision = 2u;
    if (spec->has_right_precision) precision = spec->right_precision;
    if (precision > (size_t)INT_MAX) {
        errno = EOVERFLOW;
        return 0;
    }
    if (symbol && spec->suppress_symbol) symbol = "";
    if (!symbol) symbol = "";
    amount.limit = converted->limit;
    monetary_result = rin_monetary_make_amount(
        &amount, spec, value, precision, data, decimal_point, separator,
        numeric_point, &negative);
    if (monetary_result == 0) goto fail;
    if (monetary_result == 2) {
        rin_monetary_buffer_destroy(&amount);
        return 2;
    }
    if (spec->conversion == 'i') {
        sign = negative ? data->negative_sign : data->positive_sign;
        other_sign = negative ? data->positive_sign : data->negative_sign;
        symbol_precedes = negative
            ? (unsigned char)data->int_n_cs_precedes
            : (unsigned char)data->int_p_cs_precedes;
        separator_by_space = negative
            ? (unsigned char)data->int_n_sep_by_space
            : (unsigned char)data->int_p_sep_by_space;
        sign_position = negative ? (unsigned char)data->int_n_sign_posn
                                 : (unsigned char)data->int_p_sign_posn;
        if (symbol_precedes > 1u)
            symbol_precedes = negative ? (unsigned char)data->n_cs_precedes
                                       : (unsigned char)data->p_cs_precedes;
        if (separator_by_space > 1u)
            separator_by_space = negative ? (unsigned char)data->n_sep_by_space
                                          : (unsigned char)data->p_sep_by_space;
        if (sign_position > 4u)
            sign_position = negative ? (unsigned char)data->n_sign_posn
                                     : (unsigned char)data->p_sign_posn;
    } else {
        sign = negative ? data->negative_sign : data->positive_sign;
        other_sign = negative ? data->positive_sign : data->negative_sign;
        symbol_precedes = negative ? (unsigned char)data->n_cs_precedes
                                   : (unsigned char)data->p_cs_precedes;
        separator_by_space = negative ? (unsigned char)data->n_sep_by_space
                                      : (unsigned char)data->p_sep_by_space;
        sign_position = negative ? (unsigned char)data->n_sign_posn
                                 : (unsigned char)data->p_sign_posn;
    }
    if (!sign) sign = "";
    if (!other_sign) other_sign = "";
    if (!negative && !spec->show_positive_sign) sign = "";
    if (symbol_precedes > 1u) symbol_precedes = 1u;
    if (separator_by_space > 1u) separator_by_space = 0u;
    if (symbol[0] == '\0') separator_by_space = 0u;
    if (sign_position > 4u) sign_position = 1u;
    sign_width = rin_monetary_strlen(sign);
    if (rin_monetary_strlen(other_sign) > sign_width)
        sign_width = rin_monetary_strlen(other_sign);

    base.limit = converted->limit;
    if (symbol_precedes) {
        if (!rin_monetary_append(&base, symbol, rin_monetary_strlen(symbol)) ||
            (separator_by_space &&
             !rin_monetary_append(&base, " ", 1u)) ||
            !rin_monetary_append(&base, amount.data ? amount.data : "",
                                 amount.length))
            goto fail;
    } else {
        if (!rin_monetary_append(&base, amount.data ? amount.data : "",
                                 amount.length) ||
            (separator_by_space &&
             !rin_monetary_append(&base, " ", 1u)) ||
            !rin_monetary_append(&base, symbol, rin_monetary_strlen(symbol)))
            goto fail;
    }

    if (spec->parentheses || (negative && sign_position == 0u)) {
        if (negative) {
            if (!rin_monetary_append(converted, "(", 1u) ||
                !rin_monetary_append(converted, base.data ? base.data : "",
                                     base.length) ||
                !rin_monetary_append(converted, ")", 1u))
                goto fail;
        } else {
            if (spec->has_left_precision &&
                !rin_monetary_repeat(converted, ' ', 1u))
                goto fail;
            if (!rin_monetary_append(converted, base.data ? base.data : "",
                                     base.length))
                goto fail;
            if (spec->has_left_precision &&
                !rin_monetary_repeat(converted, ' ', 1u))
                goto fail;
        }
    } else if (sign_position == 1u) {
        size_t sign_length = rin_monetary_strlen(sign);
        if (!rin_monetary_append(converted, sign, sign_length) ||
            (spec->has_left_precision && sign_length < sign_width &&
             !rin_monetary_repeat(converted, ' ', sign_width - sign_length)) ||
            !rin_monetary_append(converted, base.data ? base.data : "",
                                 base.length))
            goto fail;
    } else if (sign_position == 2u) {
        size_t sign_length = rin_monetary_strlen(sign);
        if (!rin_monetary_append(converted, base.data ? base.data : "",
                                 base.length) ||
            (spec->has_left_precision && sign_length < sign_width &&
             !rin_monetary_repeat(converted, ' ', sign_width - sign_length)) ||
            !rin_monetary_append(converted, sign, sign_length))
            goto fail;
    } else if (sign_position == 3u || sign_position == 4u) {
        int before_symbol = sign_position == 3u;
        size_t sign_length = rin_monetary_strlen(sign);
        if (symbol[0] == '\0') {
            if (before_symbol) {
                if (!rin_monetary_append(converted, sign, sign_length) ||
                    (spec->has_left_precision && sign_length < sign_width &&
                     !rin_monetary_repeat(converted, ' ', sign_width-sign_length)) ||
                    !rin_monetary_append(converted,
                        amount.data ? amount.data : "", amount.length))
                    goto fail;
            } else {
                if (!rin_monetary_append(converted,
                        amount.data ? amount.data : "", amount.length) ||
                    (spec->has_left_precision && sign_length < sign_width &&
                     !rin_monetary_repeat(converted, ' ', sign_width-sign_length)) ||
                    !rin_monetary_append(converted, sign, sign_length))
                    goto fail;
            }
        } else if (symbol_precedes) {
            if (!before_symbol &&
                !rin_monetary_append(converted, symbol,
                                     rin_monetary_strlen(symbol)))
                goto fail;
            if (before_symbol &&
                !rin_monetary_append(converted, sign, sign_length))
                goto fail;
            if (!before_symbol &&
                !rin_monetary_append(converted, sign, sign_length))
                goto fail;
            if (spec->has_left_precision && sign_length < sign_width &&
                !rin_monetary_repeat(converted, ' ', sign_width-sign_length))
                goto fail;
            if (before_symbol &&
                !rin_monetary_append(converted, symbol,
                                     rin_monetary_strlen(symbol)))
                goto fail;
            if (separator_by_space &&
                !rin_monetary_append(converted, " ", 1u))
                goto fail;
            if (!rin_monetary_append(converted, amount.data ? amount.data : "",
                                     amount.length))
                goto fail;
        } else {
            if (!rin_monetary_append(converted, amount.data ? amount.data : "",
                                     amount.length) ||
                (separator_by_space &&
                 !rin_monetary_append(converted, " ", 1u)))
                goto fail;
            if (before_symbol &&
                !rin_monetary_append(converted, sign, sign_length))
                goto fail;
            if (!before_symbol &&
                !rin_monetary_append(converted, symbol,
                                     rin_monetary_strlen(symbol)))
                goto fail;
            if (spec->has_left_precision && sign_length < sign_width &&
                !rin_monetary_repeat(converted, ' ', sign_width-sign_length))
                goto fail;
            if (before_symbol &&
                !rin_monetary_append(converted, symbol,
                                     rin_monetary_strlen(symbol)))
                goto fail;
            if (!before_symbol &&
                !rin_monetary_append(converted, sign, sign_length))
                goto fail;
        }
    } else {
        if (!rin_monetary_append(converted, base.data ? base.data : "",
                                 base.length))
            goto fail;
    }

    rin_monetary_buffer_destroy(&amount);
    rin_monetary_buffer_destroy(&base);
    return 1;

fail:
    rin_monetary_buffer_destroy(&amount);
    rin_monetary_buffer_destroy(&base);
    return 0;
}

static ssize_t rin_strfmon_v(char* output, size_t maxsize, locale_t locale,
                             int change_locale, const char* format,
                             va_list arguments) {
    RinMonetaryBuffer result = {0};
    const struct lconv* monetary_data;
    locale_t previous = NULL;
    int changed = 0;
    const char* cursor;
    ssize_t return_value = -1;
    int saved_errno = 0;

    if (!output || !format || maxsize == 0u || maxsize > (size_t)LONG_MAX) {
        errno = output && format ? E2BIG : EINVAL;
        return -1;
    }
    output[0] = '\0';
    if (change_locale) {
        if (!locale || locale == LC_GLOBAL_LOCALE) {
            errno = EINVAL;
            return -1;
        }
        previous = uselocale(locale);
        if (!previous) return -1;
        changed = 1;
    }
    monetary_data = rin_localeconv_for_locale(locale);
    if (!monetary_data) {
        errno = EINVAL;
        goto done;
    }
    result.limit = maxsize - 1u;
    cursor = format;
    while (*cursor != '\0') {
        if (*cursor != '%') {
            if (!rin_monetary_append(&result, cursor, 1u)) goto done;
            ++cursor;
            continue;
        }
        {
            const char* conversion_start = cursor;
            RinMonetarySpec spec;
            RinMonetaryBuffer converted = {0};
            int conversion_result;
            ++cursor;
            if (*cursor == '\0') {
                errno = EINVAL;
                goto done;
            }
            if (*cursor == '%') {
                if (!rin_monetary_append(&result, "%", 1u)) goto done;
                cursor++;
                continue;
            }
            if (!rin_monetary_parse_spec(&cursor, &spec)) goto done;
            if (spec.conversion == '%') {
                errno = EINVAL;
                goto done;
            }
            converted.limit = result.limit - result.length;
            conversion_result = rin_monetary_make_conversion(
                &converted, &spec, va_arg(arguments, double), monetary_data);
            if (conversion_result == 0) {
                rin_monetary_buffer_destroy(&converted);
                goto done;
            }
            if (conversion_result == 2) {
                rin_monetary_buffer_destroy(&converted);
                continue;
            }
            if (spec.width > result.limit - result.length) {
                rin_monetary_buffer_destroy(&converted);
                errno = E2BIG;
                goto done;
            }
            if (converted.length < spec.width && !spec.left_justify) {
                if (!rin_monetary_repeat(&result, ' ',
                                         spec.width - converted.length) ||
                    !rin_monetary_append(&result,
                        converted.data ? converted.data : "", converted.length)) {
                    rin_monetary_buffer_destroy(&converted);
                    goto done;
                }
            } else if (!rin_monetary_append(&result,
                        converted.data ? converted.data : "", converted.length)) {
                rin_monetary_buffer_destroy(&converted);
                goto done;
            }
            if (converted.length < spec.width && spec.left_justify &&
                !rin_monetary_repeat(&result, ' ',
                                     spec.width - converted.length)) {
                rin_monetary_buffer_destroy(&converted);
                goto done;
            }
            rin_monetary_buffer_destroy(&converted);
            if (cursor <= conversion_start) {
                errno = EINVAL;
                goto done;
            }
        }
    }
    if (result.length + 1u > maxsize) {
        errno = E2BIG;
        goto done;
    }
    if (result.length != 0u) memcpy(output, result.data, result.length);
    output[result.length] = '\0';
    return_value = (ssize_t)result.length;

done:
    saved_errno = errno;
    if (changed) {
        if (!uselocale(previous)) {
            return_value = -1;
            saved_errno = errno;
        }
    }
    rin_monetary_buffer_destroy(&result);
    if (return_value < 0) {
        output[0] = '\0';
        errno = saved_errno;
    }
    return return_value;
}

ssize_t strfmon(char* output, size_t maxsize, const char* format, ...) {
    va_list arguments;
    ssize_t result;
    locale_t locale = uselocale(NULL);
    if (!locale) return -1;
    va_start(arguments, format);
    result = rin_strfmon_v(output, maxsize, locale, 0, format, arguments);
    va_end(arguments);
    return result;
}

ssize_t strfmon_l(char* output, size_t maxsize, locale_t locale,
                  const char* format, ...) {
    va_list arguments;
    ssize_t result;
    va_start(arguments, format);
    result = rin_strfmon_v(output, maxsize, locale, 1, format, arguments);
    va_end(arguments);
    return result;
}
