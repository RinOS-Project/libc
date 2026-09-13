/* SPDX-License-Identifier: MIT */
#ifndef RIN_LIBC_INTERNAL_INTEGER_PARSE_H
#define RIN_LIBC_INTERNAL_INTEGER_PARSE_H

/* Dependency-free integer tokenization shared by libc and the legacy kernel
 * compatibility unit.  Policy such as errno and the destination type remains
 * with each public wrapper. */
typedef struct rin_integer_parse_result {
    const char* end;
    unsigned long long magnitude;
    int negative;
    int any;
    int overflow;
    int invalid_input;
    int invalid_base;
} rin_integer_parse_result;

static inline int rin_integer_parse_space(char character)
{
    return character == ' ' || character == '\t' || character == '\n' ||
           character == '\r' || character == '\f' || character == '\v';
}

static inline int rin_integer_parse_digit(char character)
{
    if (character >= '0' && character <= '9')
        return character - '0';
    if (character >= 'a' && character <= 'z')
        return character - 'a' + 10;
    if (character >= 'A' && character <= 'Z')
        return character - 'A' + 10;
    return -1;
}

static inline rin_integer_parse_result rin_integer_parse(
    const char* input, int base, unsigned long long positive_limit,
    unsigned long long negative_limit)
{
    rin_integer_parse_result result;
    const char* cursor;
    unsigned long long limit;
    unsigned long long cutoff;
    unsigned int cutlim;

    result.end = input;
    result.magnitude = 0;
    result.negative = 0;
    result.any = 0;
    result.overflow = 0;
    result.invalid_input = input == (const char*)0;
    result.invalid_base = base != 0 && (base < 2 || base > 36);
    if (result.invalid_input || result.invalid_base)
        return result;

    cursor = input;
    while (rin_integer_parse_space(*cursor))
        ++cursor;
    if (*cursor == '-' || *cursor == '+') {
        result.negative = *cursor == '-';
        ++cursor;
    }

    if (base == 0) {
        if (cursor[0] == '0') {
            base = 8;
            if (cursor[1] == 'x' || cursor[1] == 'X') {
                int prefix_digit = rin_integer_parse_digit(cursor[2]);
                if (prefix_digit >= 0 && prefix_digit < 16) {
                    base = 16;
                    cursor += 2;
                }
            }
        } else {
            base = 10;
        }
    } else if (base == 16 && cursor[0] == '0' &&
               (cursor[1] == 'x' || cursor[1] == 'X')) {
        int prefix_digit = rin_integer_parse_digit(cursor[2]);
        if (prefix_digit >= 0 && prefix_digit < 16)
            cursor += 2;
    }

    limit = result.negative ? negative_limit : positive_limit;
    cutoff = limit / (unsigned int)base;
    cutlim = (unsigned int)(limit % (unsigned int)base);
    for (;;) {
        int digit = rin_integer_parse_digit(*cursor);
        if (digit < 0 || digit >= base)
            break;
        result.any = 1;
        if (!result.overflow) {
            if (result.magnitude > cutoff ||
                (result.magnitude == cutoff &&
                 (unsigned int)digit > cutlim)) {
                result.overflow = 1;
                result.magnitude = limit;
            } else {
                result.magnitude = result.magnitude * (unsigned int)base +
                                   (unsigned int)digit;
            }
        }
        ++cursor;
    }

    result.end = result.any ? cursor : input;
    return result;
}

#endif /* RIN_LIBC_INTERNAL_INTEGER_PARSE_H */
