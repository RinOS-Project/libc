/* SPDX-License-Identifier: MIT */
#ifndef RIN_FLOAT_PARSE_H
#define RIN_FLOAT_PARSE_H

/*
 * Allocation-free classic-C-locale parser shared by libc headers and the
 * freestanding kernel runtime.  The public wrappers own errno; this core only
 * reports invalid input, range direction, and subnormal results.
 *
 * Decimal conversion compares the complete retained decimal integer against
 * adjacent IEEE-754 binary values.  That avoids host floating arithmetic and
 * double rounding while keeping the result independent of the active FPU
 * precision mode.
 */

#if defined(RIN_FP_PARSE_BINARY80) && defined(__LDBL_MANT_DIG__) && \
    defined(__LDBL_MAX_EXP__) && __LDBL_MANT_DIG__ == 64 && \
    __LDBL_MAX_EXP__ == 16384 && defined(__BYTE_ORDER__) && \
    __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define RIN_FP_HAS_BINARY80 1
#define RIN_FP_BIG_LIMBS 1216u
#else
#define RIN_FP_BIG_LIMBS 320u
#endif
#define RIN_FP_DECIMAL_DIGITS 1100u

typedef __UINT16_TYPE__ rin_fp_u16;
typedef __UINT32_TYPE__ rin_fp_u32;
typedef __UINT64_TYPE__ rin_fp_u64;

typedef struct rin_fp_big_uint {
    rin_fp_u32 limb[RIN_FP_BIG_LIMBS];
    unsigned size;
} rin_fp_big_uint;

typedef struct rin_float_parse_result {
    const char* end;
    rin_fp_u64 bits;
    int converted;
    int invalid_input;
    int range_direction;
    int subnormal;
} rin_float_parse_result;

#if defined(RIN_FP_HAS_BINARY80)
typedef struct rin_long_double_parse_result {
    const char* end;
    long double value;
    int converted;
    int invalid_input;
    int range_direction;
    int subnormal;
} rin_long_double_parse_result;
#endif

typedef struct rin_fp_traits {
    unsigned fraction_bits;
    int exponent_bias;
    int minimum_normal;
    int minimum_subnormal;
    int maximum_exponent;
    rin_fp_u64 sign;
    rin_fp_u64 infinity;
    rin_fp_u64 nan;
    rin_fp_u64 maximum_finite;
} rin_fp_traits;

static inline rin_fp_traits rin_fp_binary32_traits(void)
{
    rin_fp_traits traits;
    traits.fraction_bits = 23u;
    traits.exponent_bias = 127;
    traits.minimum_normal = -126;
    traits.minimum_subnormal = -149;
    traits.maximum_exponent = 127;
    traits.sign = 0x80000000u;
    traits.infinity = 0x7f800000u;
    traits.nan = 0x7fc00001u;
    traits.maximum_finite = 0x7f7fffffu;
    return traits;
}

static inline rin_fp_traits rin_fp_binary64_traits(void)
{
    rin_fp_traits traits;
    traits.fraction_bits = 52u;
    traits.exponent_bias = 1023;
    traits.minimum_normal = -1022;
    traits.minimum_subnormal = -1074;
    traits.maximum_exponent = 1023;
    traits.sign = ((rin_fp_u64)1u) << 63u;
    traits.infinity = ((rin_fp_u64)0x7ffu) << 52u;
    traits.nan = (((rin_fp_u64)0x7ffu) << 52u) |
                 (((rin_fp_u64)1u) << 51u) | 1u;
    traits.maximum_finite = (rin_fp_u64)0x7fefffffffffffffULL;
    return traits;
}

static inline void rin_fp_big_zero(rin_fp_big_uint* value)
{
    value->size = 0u;
}

static inline void rin_fp_big_set(rin_fp_big_uint* value, rin_fp_u64 source)
{
    rin_fp_big_zero(value);
    if (source == 0u) return;
    value->limb[0] = (rin_fp_u32)source;
    value->limb[1] = (rin_fp_u32)(source >> 32u);
    value->size = value->limb[1] == 0u ? 1u : 2u;
}

static inline int rin_fp_big_compare(const rin_fp_big_uint* left,
                                     const rin_fp_big_uint* right)
{
    unsigned index;
    if (left->size != right->size)
        return left->size < right->size ? -1 : 1;
    for (index = left->size; index != 0u; --index) {
        if (left->limb[index - 1u] != right->limb[index - 1u]) {
            return left->limb[index - 1u] < right->limb[index - 1u]
                       ? -1 : 1;
        }
    }
    return 0;
}

static inline int rin_fp_big_shift_left(rin_fp_big_uint* value,
                                        unsigned bits)
{
    unsigned words;
    unsigned shift;
    unsigned extra;
    unsigned index;
    rin_fp_u32 carry;

    if (value->size == 0u || bits == 0u) return 1;
    words = bits / 32u;
    shift = bits % 32u;
    extra = shift != 0u &&
            (value->limb[value->size - 1u] >> (32u - shift)) != 0u;
    if (value->size + words + extra > RIN_FP_BIG_LIMBS) return 0;
    for (index = value->size; index != 0u; --index)
        value->limb[index - 1u + words] = value->limb[index - 1u];
    for (index = 0u; index < words; ++index) value->limb[index] = 0u;
    value->size += words;
    if (shift == 0u) return 1;

    carry = 0u;
    for (index = words; index < value->size; ++index) {
        rin_fp_u64 next = ((rin_fp_u64)value->limb[index] << shift) | carry;
        value->limb[index] = (rin_fp_u32)next;
        carry = (rin_fp_u32)(next >> 32u);
    }
    if (carry != 0u) value->limb[value->size++] = carry;
    return 1;
}

static inline int rin_fp_big_multiply_small(rin_fp_big_uint* value,
                                            rin_fp_u32 multiplier)
{
    rin_fp_u64 carry = 0u;
    unsigned index;
    if (value->size == 0u || multiplier == 1u) return 1;
    if (multiplier == 0u) {
        rin_fp_big_zero(value);
        return 1;
    }
    for (index = 0u; index < value->size; ++index) {
        rin_fp_u64 product = (rin_fp_u64)value->limb[index] * multiplier +
                             carry;
        value->limb[index] = (rin_fp_u32)product;
        carry = product >> 32u;
    }
    if (carry != 0u) {
        if (value->size == RIN_FP_BIG_LIMBS) return 0;
        value->limb[value->size++] = (rin_fp_u32)carry;
    }
    return 1;
}

static inline int rin_fp_big_add_small(rin_fp_big_uint* value,
                                       rin_fp_u32 addend)
{
    rin_fp_u64 carry = addend;
    unsigned index = 0u;
    if (addend == 0u) return 1;
    if (value->size == 0u) {
        value->limb[0] = addend;
        value->size = 1u;
        return 1;
    }
    while (carry != 0u && index < value->size) {
        rin_fp_u64 sum = (rin_fp_u64)value->limb[index] + carry;
        value->limb[index++] = (rin_fp_u32)sum;
        carry = sum >> 32u;
    }
    if (carry != 0u) {
        if (value->size == RIN_FP_BIG_LIMBS) return 0;
        value->limb[value->size++] = (rin_fp_u32)carry;
    }
    return 1;
}

static inline int rin_fp_big_add(rin_fp_big_uint* value,
                                 const rin_fp_big_uint* addend)
{
    unsigned limit = value->size > addend->size
                         ? value->size : addend->size;
    rin_fp_u64 carry = 0u;
    unsigned index;
    if (limit > RIN_FP_BIG_LIMBS) return 0;
    for (index = 0u; index < limit; ++index) {
        rin_fp_u64 sum = carry;
        if (index < value->size) sum += value->limb[index];
        if (index < addend->size) sum += addend->limb[index];
        value->limb[index] = (rin_fp_u32)sum;
        carry = sum >> 32u;
    }
    value->size = limit;
    if (carry != 0u) {
        if (value->size == RIN_FP_BIG_LIMBS) return 0;
        value->limb[value->size++] = (rin_fp_u32)carry;
    }
    return 1;
}

static inline int rin_fp_big_multiply_power5(rin_fp_big_uint* value,
                                             unsigned exponent)
{
    unsigned index;
    for (index = 0u; index < exponent; ++index) {
        if (!rin_fp_big_multiply_small(value, 5u)) return 0;
    }
    return 1;
}

static inline int rin_fp_ascii_space(char value)
{
    return value == ' ' || value == '\f' || value == '\n' ||
           value == '\r' || value == '\t' || value == '\v';
}

static inline int rin_fp_ascii_case_equal(char value, char lower)
{
    return value == lower || value == (char)(lower - ('a' - 'A'));
}

static inline int rin_fp_match_word(const char* first, const char* word,
                                    const char** end)
{
    const char* cursor = first;
    while (*word != '\0') {
        if (*cursor == '\0' || !rin_fp_ascii_case_equal(*cursor, *word++))
            return 0;
        ++cursor;
    }
    *end = cursor;
    return 1;
}

static inline int rin_fp_hex_digit(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static inline void rin_fp_finite_components(rin_fp_u64 bits,
                                            const rin_fp_traits* traits,
                                            rin_fp_u64* significand,
                                            int* exponent)
{
    rin_fp_u64 fraction_mask =
        (((rin_fp_u64)1u) << traits->fraction_bits) - 1u;
    rin_fp_u64 fraction = bits & fraction_mask;
    unsigned exponent_width = traits->fraction_bits == 23u ? 8u : 11u;
    rin_fp_u64 encoded_exponent =
        (bits >> traits->fraction_bits) &
        ((((rin_fp_u64)1u) << exponent_width) - 1u);
    if (encoded_exponent == 0u) {
        *significand = fraction;
        *exponent = traits->minimum_subnormal;
    } else {
        *significand = (((rin_fp_u64)1u) << traits->fraction_bits) |
                       fraction;
        *exponent = (int)encoded_exponent - traits->exponent_bias -
                    (int)traits->fraction_bits;
    }
}

static inline int rin_fp_compare_decimal_big_binary(
    const rin_fp_big_uint* decimal, int decimal_exponent, int sticky,
    rin_fp_big_uint right, int binary_exponent, int* okay)
{
    rin_fp_big_uint left = *decimal;
    int left_exponent = 0;
    int right_exponent = binary_exponent;
    int comparison;

    if (right.size == 0u) return decimal->size == 0u ? 0 : 1;
    if (decimal_exponent >= 0) {
        *okay = rin_fp_big_multiply_power5(
            &left, (unsigned)decimal_exponent);
        left_exponent = decimal_exponent;
    } else {
        unsigned power = (unsigned)-decimal_exponent;
        *okay = rin_fp_big_multiply_power5(&right, power);
        right_exponent += (int)power;
    }
    if (!*okay) return 0;
    if (left_exponent > right_exponent) {
        *okay = rin_fp_big_shift_left(
            &left, (unsigned)(left_exponent - right_exponent));
    } else if (right_exponent > left_exponent) {
        *okay = rin_fp_big_shift_left(
            &right, (unsigned)(right_exponent - left_exponent));
    }
    if (!*okay) return 0;
    comparison = rin_fp_big_compare(&left, &right);
    return comparison == 0 && sticky ? 1 : comparison;
}

static inline int rin_fp_compare_decimal_binary(
    const rin_fp_big_uint* decimal, int decimal_exponent, int sticky,
    rin_fp_u64 binary_significand, int binary_exponent, int* okay)
{
    rin_fp_big_uint right;
    rin_fp_big_set(&right, binary_significand);
    return rin_fp_compare_decimal_big_binary(
        decimal, decimal_exponent, sticky, right, binary_exponent, okay);
}

#if defined(RIN_FP_HAS_BINARY80)
static inline long double rin_fp_binary80_value(rin_fp_u64 significand,
                                                rin_fp_u16 encoded_exponent,
                                                int negative)
{
    union {
        long double value;
        unsigned char bytes[sizeof(long double)];
    } converted = {0};
    rin_fp_u16 exponent_sign;
    unsigned index;
    for (index = 0u; index < 8u; ++index)
        converted.bytes[index] =
            (unsigned char)(significand >> (index * 8u));
    exponent_sign = (rin_fp_u16)(encoded_exponent |
                    (negative ? 0x8000u : 0u));
    converted.bytes[8] = (unsigned char)exponent_sign;
    converted.bytes[9] = (unsigned char)(exponent_sign >> 8u);
    return converted.value;
}

static inline int rin_fp_compare_decimal_midpoint(
    const rin_fp_big_uint* decimal, int decimal_exponent, int sticky,
    rin_fp_u64 lower_significand, int lower_exponent,
    rin_fp_u64 upper_significand, int upper_exponent, int* okay)
{
    int common_exponent = lower_exponent < upper_exponent
                              ? lower_exponent : upper_exponent;
    rin_fp_big_uint boundary;
    rin_fp_big_uint upper;
    rin_fp_big_set(&boundary, lower_significand);
    rin_fp_big_set(&upper, upper_significand);
    *okay = rin_fp_big_shift_left(
                 &boundary, (unsigned)(lower_exponent - common_exponent)) &&
             rin_fp_big_shift_left(
                 &upper, (unsigned)(upper_exponent - common_exponent)) &&
             rin_fp_big_add(&boundary, &upper);
    if (!*okay) return 0;
    return rin_fp_compare_decimal_big_binary(
        decimal, decimal_exponent, sticky, boundary, common_exponent - 1,
        okay);
}
#endif

static inline int rin_fp_decimal_to_bits(const rin_fp_big_uint* decimal,
                                         int decimal_exponent, int sticky,
                                         int negative,
                                         const rin_fp_traits* traits,
                                         rin_fp_u64* output)
{
    rin_fp_u64 low = 0u;
    rin_fp_u64 high = traits->maximum_finite;
    int okay = 1;
    rin_fp_u64 lower_significand;
    int lower_exponent;
    int exact;
    rin_fp_u64 selected;

    while (low < high) {
        rin_fp_u64 distance = high - low;
        rin_fp_u64 middle = low + distance / 2u + (distance & 1u);
        rin_fp_u64 significand;
        int exponent;
        int comparison;
        rin_fp_finite_components(middle, traits, &significand, &exponent);
        comparison = rin_fp_compare_decimal_binary(
            decimal, decimal_exponent, sticky, significand, exponent, &okay);
        if (!okay) return 0;
        if (comparison >= 0) low = middle;
        else high = middle - 1u;
    }

    rin_fp_finite_components(low, traits, &lower_significand,
                             &lower_exponent);
    exact = rin_fp_compare_decimal_binary(
        decimal, decimal_exponent, sticky, lower_significand,
        lower_exponent, &okay);
    if (!okay) return 0;
    selected = low;
    if (exact != 0) {
        rin_fp_u64 boundary_significand;
        int boundary_exponent;
        int boundary;
        if (low == traits->maximum_finite) {
            boundary_significand = lower_significand * 2u + 1u;
            boundary_exponent = lower_exponent - 1;
            boundary = rin_fp_compare_decimal_binary(
                decimal, decimal_exponent, sticky, boundary_significand,
                boundary_exponent, &okay);
            if (!okay || boundary >= 0) return 0;
        } else {
            rin_fp_u64 upper_significand;
            int upper_exponent;
            int common_exponent;
            rin_fp_u64 lower_scaled;
            rin_fp_u64 upper_scaled;
            rin_fp_finite_components(low + 1u, traits, &upper_significand,
                                     &upper_exponent);
            common_exponent = lower_exponent < upper_exponent
                                  ? lower_exponent : upper_exponent;
            lower_scaled = lower_significand <<
                (unsigned)(lower_exponent - common_exponent);
            upper_scaled = upper_significand <<
                (unsigned)(upper_exponent - common_exponent);
            boundary_significand = lower_scaled + upper_scaled;
            boundary_exponent = common_exponent - 1;
            boundary = rin_fp_compare_decimal_binary(
                decimal, decimal_exponent, sticky, boundary_significand,
                boundary_exponent, &okay);
            if (!okay) return 0;
            if (boundary > 0 || (boundary == 0 && (low & 1u) != 0u))
                selected = low + 1u;
        }
    }
    if (selected == 0u) return 0;
    if (negative) selected |= traits->sign;
    *output = selected;
    return 1;
}

#if defined(RIN_FP_HAS_BINARY80)
static inline int rin_fp_decimal_to_binary80(
    const rin_fp_big_uint* decimal, int decimal_exponent, int sticky,
    int negative, long double* output)
{
    int okay = 1;
    rin_fp_u16 selected_exponent = 0u;
    int minimum_comparison = rin_fp_compare_decimal_binary(
        decimal, decimal_exponent, sticky, ((rin_fp_u64)1u) << 63u,
        -16445, &okay);
    rin_fp_u64 low;
    rin_fp_u64 high;
    int candidate_exponent;
    int exact;
    rin_fp_u16 result_exponent;
    rin_fp_u64 result_significand;

    if (!okay) return 0;
    if (minimum_comparison >= 0) {
        unsigned low_exponent = 1u;
        unsigned high_exponent = 0x7ffeu;
        while (low_exponent < high_exponent) {
            unsigned distance = high_exponent - low_exponent;
            unsigned middle = low_exponent + distance / 2u +
                              (distance & 1u);
            int exponent = (int)middle - 16383 - 63;
            int comparison = rin_fp_compare_decimal_binary(
                decimal, decimal_exponent, sticky,
                ((rin_fp_u64)1u) << 63u, exponent, &okay);
            if (!okay) return 0;
            if (comparison >= 0) low_exponent = middle;
            else high_exponent = middle - 1u;
        }
        selected_exponent = (rin_fp_u16)low_exponent;
    }

    low = selected_exponent == 0u ? 0u : ((rin_fp_u64)1u) << 63u;
    high = selected_exponent == 0u
               ? (((rin_fp_u64)1u) << 63u) - 1u
               : ~(rin_fp_u64)0u;
    candidate_exponent = selected_exponent == 0u
                             ? -16445
                             : (int)selected_exponent - 16383 - 63;
    while (low < high) {
        rin_fp_u64 distance = high - low;
        rin_fp_u64 middle = low + distance / 2u + (distance & 1u);
        int comparison = rin_fp_compare_decimal_binary(
            decimal, decimal_exponent, sticky, middle,
            candidate_exponent, &okay);
        if (!okay) return 0;
        if (comparison >= 0) low = middle;
        else high = middle - 1u;
    }

    exact = rin_fp_compare_decimal_binary(
        decimal, decimal_exponent, sticky, low, candidate_exponent, &okay);
    if (!okay) return 0;
    result_exponent = selected_exponent;
    result_significand = low;
    if (exact != 0) {
        rin_fp_u16 upper_exponent = selected_exponent;
        rin_fp_u64 upper_significand;
        int upper_binary_exponent;
        int boundary;
        if (selected_exponent == 0u &&
            low == (((rin_fp_u64)1u) << 63u) - 1u) {
            upper_significand = ((rin_fp_u64)1u) << 63u;
            upper_exponent = 1u;
        } else if (low != ~(rin_fp_u64)0u) {
            upper_significand = low + 1u;
        } else {
            upper_significand = ((rin_fp_u64)1u) << 63u;
            ++upper_exponent;
        }
        upper_binary_exponent = upper_exponent == 0u
                                    ? -16445
                                    : (int)upper_exponent - 16383 - 63;
        boundary = rin_fp_compare_decimal_midpoint(
            decimal, decimal_exponent, sticky, low, candidate_exponent,
            upper_significand, upper_binary_exponent, &okay);
        if (!okay) return 0;
        if (boundary > 0 || (boundary == 0 && (low & 1u) != 0u)) {
            result_exponent = upper_exponent;
            result_significand = upper_significand;
        }
    }
    if (result_significand == 0u || result_exponent == 0x7fffu) return 0;
    *output = rin_fp_binary80_value(result_significand, result_exponent,
                                    negative);
    return 1;
}
#endif

static inline int rin_fp_round_right(rin_fp_u64 source, unsigned shift,
                                     int sticky, rin_fp_u64* rounded)
{
    rin_fp_u64 quotient;
    rin_fp_u64 remainder;
    rin_fp_u64 halfway;
    if (shift == 0u) {
        *rounded = source;
        return 1;
    }
    if (shift > 64u) {
        *rounded = 0u;
        return 1;
    }
    if (shift == 64u) {
        halfway = ((rin_fp_u64)1u) << 63u;
        *rounded = source > halfway || (source == halfway && sticky)
                       ? 1u : 0u;
        return 1;
    }
    quotient = source >> shift;
    remainder = source & ((((rin_fp_u64)1u) << shift) - 1u);
    halfway = ((rin_fp_u64)1u) << (shift - 1u);
    if (remainder > halfway ||
        (remainder == halfway && (sticky || (quotient & 1u) != 0u)))
        ++quotient;
    *rounded = quotient;
    return 1;
}

static inline int rin_fp_binary_to_bits(rin_fp_u64 significand, int scale,
                                        int round_bit, int sticky,
                                        int negative,
                                        const rin_fp_traits* traits,
                                        rin_fp_u64* output)
{
    unsigned highest;
    long long exponent_wide;
    rin_fp_u64 rounded = 0u;
    if (significand == 0u) {
        *output = negative ? traits->sign : 0u;
        return 1;
    }
    highest = 63u - (unsigned)__builtin_clzll(significand);
    exponent_wide = (long long)scale + highest;
    if (exponent_wide > traits->maximum_exponent) return 0;
    if (exponent_wide >= traits->minimum_normal) {
        int shift = (int)highest - (int)traits->fraction_bits;
        rin_fp_u64 precision_limit;
        rin_fp_u64 fraction_mask;
        rin_fp_u64 exponent_bits;
        rin_fp_u64 bits;
        if (shift > 0) {
            rin_fp_round_right(significand, (unsigned)shift,
                               round_bit || sticky, &rounded);
        } else {
            rounded = significand << (unsigned)-shift;
        }
        precision_limit = ((rin_fp_u64)1u) <<
                          (traits->fraction_bits + 1u);
        if (rounded == precision_limit) {
            rounded >>= 1u;
            if (++exponent_wide > traits->maximum_exponent) return 0;
        }
        exponent_bits = (rin_fp_u64)(exponent_wide +
                                     traits->exponent_bias);
        fraction_mask = (((rin_fp_u64)1u) << traits->fraction_bits) - 1u;
        bits = rounded & fraction_mask;
        bits |= exponent_bits << traits->fraction_bits;
        if (negative) bits |= traits->sign;
        *output = bits;
        return 1;
    }
    {
        long long shift_wide = (long long)traits->minimum_subnormal - scale;
        rin_fp_u64 normal_threshold;
        if (shift_wide <= 0) {
            rounded = significand << (unsigned)-shift_wide;
        } else {
            rin_fp_round_right(significand,
                               shift_wide > 65 ? 65u : (unsigned)shift_wide,
                               round_bit || sticky, &rounded);
        }
        if (rounded == 0u) return 0;
        normal_threshold = ((rin_fp_u64)1u) << traits->fraction_bits;
        *output = rounded >= normal_threshold ? normal_threshold : rounded;
        if (negative) *output |= traits->sign;
        return 1;
    }
}

#if defined(RIN_FP_HAS_BINARY80)
static inline int rin_fp_binary_to_binary80(
    rin_fp_u64 significand, int scale, int round_bit, int sticky,
    int negative, long double* output)
{
    unsigned highest;
    long long exponent_wide;
    rin_fp_u64 rounded = 0u;
    if (significand == 0u) {
        *output = rin_fp_binary80_value(0u, 0u, negative);
        return 1;
    }
    highest = 63u - (unsigned)__builtin_clzll(significand);
    exponent_wide = (long long)scale + highest;
    if (exponent_wide > 16383) return 0;
    if (exponent_wide >= -16382) {
        int shift = (int)highest - 63;
        rin_fp_u16 encoded_exponent;
        if (shift > 0) {
            rin_fp_round_right(significand, (unsigned)shift,
                               round_bit || sticky, &rounded);
        } else {
            rounded = significand << (unsigned)-shift;
            if (shift == 0 && round_bit &&
                (sticky || (rounded & 1u) != 0u)) {
                if (rounded == ~(rin_fp_u64)0u) {
                    rounded = ((rin_fp_u64)1u) << 63u;
                    if (++exponent_wide > 16383) return 0;
                } else {
                    ++rounded;
                }
            }
        }
        encoded_exponent = (rin_fp_u16)(exponent_wide + 16383);
        *output = rin_fp_binary80_value(rounded, encoded_exponent, negative);
        return 1;
    }
    {
        long long shift_wide = -16445LL - (long long)scale;
        if (shift_wide <= 0) {
            rounded = significand << (unsigned)-shift_wide;
        } else {
            rin_fp_round_right(significand,
                               shift_wide > 65 ? 65u : (unsigned)shift_wide,
                               round_bit || sticky, &rounded);
        }
        if (rounded == 0u) return 0;
        if (rounded >= (((rin_fp_u64)1u) << 63u)) {
            *output = rin_fp_binary80_value(
                ((rin_fp_u64)1u) << 63u, 1u, negative);
        } else {
            *output = rin_fp_binary80_value(rounded, 0u, negative);
        }
        return 1;
    }
}
#endif

static inline rin_float_parse_result rin_fp_empty_result(const char* string)
{
    rin_float_parse_result result;
    result.end = string;
    result.bits = 0u;
    result.converted = 0;
    result.invalid_input = string == (const char*)0;
    result.range_direction = 0;
    result.subnormal = 0;
    return result;
}

static inline rin_float_parse_result rin_fp_parse_decimal_magnitude(
    const char* first, int negative, const rin_fp_traits* traits)
{
    rin_float_parse_result result = rin_fp_empty_result(first);
    rin_fp_big_uint decimal;
    const char* cursor = first;
    int any_digit = 0;
    int significant = 0;
    int sticky = 0;
    unsigned kept = 0u;
    long long significant_count = 0;
    long long fractional_digits = 0;
    int point = 0;
    long long explicit_exponent = 0;
    long long dropped;
    long long decimal_exponent;
    long long order;

    rin_fp_big_zero(&decimal);
    for (;;) {
        unsigned digit;
        if (*cursor == '.' && !point) {
            point = 1;
            ++cursor;
            continue;
        }
        if (*cursor < '0' || *cursor > '9') break;
        any_digit = 1;
        if (point && fractional_digits < 1000000LL) ++fractional_digits;
        digit = (unsigned)(*cursor - '0');
        ++cursor;
        if (!significant && digit == 0u) continue;
        significant = 1;
        if (significant_count < 1000000LL) ++significant_count;
        if (kept < RIN_FP_DECIMAL_DIGITS) {
            if (!rin_fp_big_multiply_small(&decimal, 10u) ||
                !rin_fp_big_add_small(&decimal, (rin_fp_u32)digit)) {
                result.converted = 1;
                result.end = cursor;
                result.range_direction = 1;
                result.bits = traits->infinity |
                              (negative ? traits->sign : 0u);
                return result;
            }
            ++kept;
        } else if (digit != 0u) {
            sticky = 1;
        }
    }
    if (!any_digit) return result;

    if (*cursor == 'e' || *cursor == 'E') {
        const char* marker = cursor;
        const char* scan = cursor + 1;
        int exponent_negative = 0;
        const char* exponent_digits;
        if (*scan == '+' || *scan == '-') {
            exponent_negative = *scan == '-';
            ++scan;
        }
        exponent_digits = scan;
        while (*scan >= '0' && *scan <= '9') {
            if (explicit_exponent < 100000LL)
                explicit_exponent = explicit_exponent * 10 + (*scan - '0');
            ++scan;
        }
        if (scan != exponent_digits) {
            cursor = scan;
            if (exponent_negative) explicit_exponent = -explicit_exponent;
        } else {
            cursor = marker;
        }
    }

    result.converted = 1;
    result.end = cursor;
    if (!significant) {
        result.bits = negative ? traits->sign : 0u;
        return result;
    }
    dropped = significant_count - (long long)kept;
    decimal_exponent = explicit_exponent - fractional_digits + dropped;
    order = significant_count + explicit_exponent - fractional_digits - 1;
    if (order > 400LL || order < -400LL ||
        decimal_exponent > 2000LL || decimal_exponent < -2000LL ||
        !rin_fp_decimal_to_bits(&decimal, (int)decimal_exponent, sticky,
                                negative, traits, &result.bits)) {
        result.range_direction = order < 0 ? -1 : 1;
        result.bits = result.range_direction > 0
                          ? traits->infinity |
                                (negative ? traits->sign : 0u)
                          : (negative ? traits->sign : 0u);
        return result;
    }
    result.subnormal = (result.bits & traits->infinity) == 0u &&
                       (result.bits & (traits->infinity - 1u)) != 0u;
    return result;
}

static inline rin_float_parse_result rin_fp_parse_hex_magnitude(
    const char* first, int negative, const rin_fp_traits* traits)
{
    rin_float_parse_result result = rin_fp_empty_result(first);
    const char* cursor = first;
    rin_fp_u64 significand = 0u;
    unsigned kept_bits = 0u;
    long long total_bits = 0;
    long long fractional_nibbles = 0;
    int any_digit = 0;
    int seen_point = 0;
    int seen_nonzero = 0;
    int round_bit = 0;
    int sticky = 0;
    int tail_started = 0;
    long long exponent = 0;

    for (;;) {
        int digit;
        unsigned digit_bits;
        unsigned available;
        if (*cursor == '.' && !seen_point) {
            seen_point = 1;
            ++cursor;
            continue;
        }
        digit = rin_fp_hex_digit(*cursor);
        if (digit < 0) break;
        any_digit = 1;
        if (seen_point && fractional_nibbles < 1000000LL)
            ++fractional_nibbles;
        ++cursor;
        if (!seen_nonzero && digit == 0) continue;
        digit_bits = 4u;
        if (!seen_nonzero) {
            seen_nonzero = 1;
            digit_bits = digit >= 8 ? 4u : digit >= 4 ? 3u :
                         digit >= 2 ? 2u : 1u;
        }
        if (total_bits < 1000000LL) total_bits += digit_bits;
        available = 64u - kept_bits;
        if (digit_bits <= available) {
            significand = (significand << digit_bits) | (unsigned)digit;
            kept_bits += digit_bits;
        } else if (available != 0u) {
            unsigned discarded = digit_bits - available;
            unsigned tail;
            significand = (significand << available) |
                          ((unsigned)digit >> discarded);
            kept_bits = 64u;
            tail = (unsigned)digit & ((1u << discarded) - 1u);
            round_bit = (tail & (1u << (discarded - 1u))) != 0u;
            sticky = (tail & ((1u << (discarded - 1u)) - 1u)) != 0u;
            tail_started = 1;
        } else if (!tail_started) {
            round_bit = ((unsigned)digit & 8u) != 0u;
            sticky = ((unsigned)digit & 7u) != 0u;
            tail_started = 1;
        } else if (digit != 0) {
            sticky = 1;
        }
    }
    if (!any_digit) return result;

    if (*cursor == 'p' || *cursor == 'P') {
        const char* marker = cursor;
        const char* scan = cursor + 1;
        int exponent_negative = 0;
        const char* exponent_digits;
        if (*scan == '+' || *scan == '-') {
            exponent_negative = *scan == '-';
            ++scan;
        }
        exponent_digits = scan;
        while (*scan >= '0' && *scan <= '9') {
            if (exponent < 100000LL)
                exponent = exponent * 10 + (*scan - '0');
            ++scan;
        }
        if (scan != exponent_digits) {
            cursor = scan;
            if (exponent_negative) exponent = -exponent;
        } else {
            cursor = marker;
        }
    }

    result.converted = 1;
    result.end = cursor;
    if (!seen_nonzero) {
        result.bits = negative ? traits->sign : 0u;
        return result;
    }
    {
        long long dropped_bits = total_bits - (long long)kept_bits;
        long long scale_wide = exponent - fractional_nibbles * 4LL +
                               dropped_bits;
        if (scale_wide < -100000LL || scale_wide > 100000LL ||
            !rin_fp_binary_to_bits(significand, (int)scale_wide, round_bit,
                                   sticky, negative, traits, &result.bits)) {
            result.range_direction = scale_wide < 0 ? -1 : 1;
            result.bits = result.range_direction > 0
                              ? traits->infinity |
                                    (negative ? traits->sign : 0u)
                              : (negative ? traits->sign : 0u);
            return result;
        }
    }
    result.subnormal = (result.bits & traits->infinity) == 0u &&
                       (result.bits & (traits->infinity - 1u)) != 0u;
    return result;
}

#if defined(RIN_FP_HAS_BINARY80)
static inline rin_long_double_parse_result rin_fp_empty_binary80_result(
    const char* string)
{
    rin_long_double_parse_result result;
    result.end = string;
    result.value = rin_fp_binary80_value(0u, 0u, 0);
    result.converted = 0;
    result.invalid_input = string == (const char*)0;
    result.range_direction = 0;
    result.subnormal = 0;
    return result;
}

static inline rin_long_double_parse_result
rin_fp_parse_binary80_decimal_magnitude(const char* first, int negative)
{
    rin_long_double_parse_result result =
        rin_fp_empty_binary80_result(first);
    rin_fp_big_uint decimal;
    const char* cursor = first;
    int any_digit = 0;
    int significant = 0;
    int sticky = 0;
    unsigned kept = 0u;
    long long significant_count = 0;
    long long fractional_digits = 0;
    int point = 0;
    long long explicit_exponent = 0;
    long long dropped;
    long long decimal_exponent;
    long long order;

    rin_fp_big_zero(&decimal);
    for (;;) {
        unsigned digit;
        if (*cursor == '.' && !point) {
            point = 1;
            ++cursor;
            continue;
        }
        if (*cursor < '0' || *cursor > '9') break;
        any_digit = 1;
        if (point && fractional_digits < 1000000LL) ++fractional_digits;
        digit = (unsigned)(*cursor - '0');
        ++cursor;
        if (!significant && digit == 0u) continue;
        significant = 1;
        if (significant_count < 1000000LL) ++significant_count;
        if (kept < RIN_FP_DECIMAL_DIGITS) {
            if (!rin_fp_big_multiply_small(&decimal, 10u) ||
                !rin_fp_big_add_small(&decimal, (rin_fp_u32)digit)) {
                result.converted = 1;
                result.end = cursor;
                result.range_direction = 1;
                result.value = rin_fp_binary80_value(
                    ((rin_fp_u64)1u) << 63u, 0x7fffu, negative);
                return result;
            }
            ++kept;
        } else if (digit != 0u) {
            sticky = 1;
        }
    }
    if (!any_digit) return result;

    if (*cursor == 'e' || *cursor == 'E') {
        const char* marker = cursor;
        const char* scan = cursor + 1;
        int exponent_negative = 0;
        const char* exponent_digits;
        if (*scan == '+' || *scan == '-') {
            exponent_negative = *scan == '-';
            ++scan;
        }
        exponent_digits = scan;
        while (*scan >= '0' && *scan <= '9') {
            if (explicit_exponent < 100000LL)
                explicit_exponent = explicit_exponent * 10 + (*scan - '0');
            ++scan;
        }
        if (scan != exponent_digits) {
            cursor = scan;
            if (exponent_negative) explicit_exponent = -explicit_exponent;
        } else {
            cursor = marker;
        }
    }

    result.converted = 1;
    result.end = cursor;
    if (!significant) {
        result.value = rin_fp_binary80_value(0u, 0u, negative);
        return result;
    }
    dropped = significant_count - (long long)kept;
    decimal_exponent = explicit_exponent - fractional_digits + dropped;
    order = significant_count + explicit_exponent - fractional_digits - 1;
    if (order > 5000LL || order < -5000LL ||
        decimal_exponent > 5100LL || decimal_exponent < -6200LL ||
        !rin_fp_decimal_to_binary80(&decimal, (int)decimal_exponent, sticky,
                                    negative, &result.value)) {
        result.range_direction = order < 0 ? -1 : 1;
        result.value = result.range_direction > 0
                           ? rin_fp_binary80_value(
                                 ((rin_fp_u64)1u) << 63u, 0x7fffu,
                                 negative)
                           : rin_fp_binary80_value(0u, 0u, negative);
        return result;
    }
    {
        union {
            long double value;
            unsigned char bytes[sizeof(long double)];
        } raw = {0};
        rin_fp_u16 exponent;
        raw.value = result.value;
        exponent = (rin_fp_u16)raw.bytes[8] |
                   (rin_fp_u16)((rin_fp_u16)raw.bytes[9] << 8u);
        result.subnormal = (exponent & 0x7fffu) == 0u;
    }
    return result;
}

static inline rin_long_double_parse_result
rin_fp_parse_binary80_hex_magnitude(const char* first, int negative)
{
    rin_long_double_parse_result result =
        rin_fp_empty_binary80_result(first);
    const char* cursor = first;
    rin_fp_u64 significand = 0u;
    unsigned kept_bits = 0u;
    long long total_bits = 0;
    long long fractional_nibbles = 0;
    int any_digit = 0;
    int seen_point = 0;
    int seen_nonzero = 0;
    int round_bit = 0;
    int sticky = 0;
    int tail_started = 0;
    long long exponent = 0;

    for (;;) {
        int digit;
        unsigned digit_bits;
        unsigned available;
        if (*cursor == '.' && !seen_point) {
            seen_point = 1;
            ++cursor;
            continue;
        }
        digit = rin_fp_hex_digit(*cursor);
        if (digit < 0) break;
        any_digit = 1;
        if (seen_point && fractional_nibbles < 1000000LL)
            ++fractional_nibbles;
        ++cursor;
        if (!seen_nonzero && digit == 0) continue;
        digit_bits = 4u;
        if (!seen_nonzero) {
            seen_nonzero = 1;
            digit_bits = digit >= 8 ? 4u : digit >= 4 ? 3u :
                         digit >= 2 ? 2u : 1u;
        }
        if (total_bits < 1000000LL) total_bits += digit_bits;
        available = 64u - kept_bits;
        if (digit_bits <= available) {
            significand = (significand << digit_bits) | (unsigned)digit;
            kept_bits += digit_bits;
        } else if (available != 0u) {
            unsigned discarded = digit_bits - available;
            unsigned tail;
            significand = (significand << available) |
                          ((unsigned)digit >> discarded);
            kept_bits = 64u;
            tail = (unsigned)digit & ((1u << discarded) - 1u);
            round_bit = (tail & (1u << (discarded - 1u))) != 0u;
            sticky = (tail & ((1u << (discarded - 1u)) - 1u)) != 0u;
            tail_started = 1;
        } else if (!tail_started) {
            round_bit = ((unsigned)digit & 8u) != 0u;
            sticky = ((unsigned)digit & 7u) != 0u;
            tail_started = 1;
        } else if (digit != 0) {
            sticky = 1;
        }
    }
    if (!any_digit) return result;

    if (*cursor == 'p' || *cursor == 'P') {
        const char* marker = cursor;
        const char* scan = cursor + 1;
        int exponent_negative = 0;
        const char* exponent_digits;
        if (*scan == '+' || *scan == '-') {
            exponent_negative = *scan == '-';
            ++scan;
        }
        exponent_digits = scan;
        while (*scan >= '0' && *scan <= '9') {
            if (exponent < 100000LL)
                exponent = exponent * 10 + (*scan - '0');
            ++scan;
        }
        if (scan != exponent_digits) {
            cursor = scan;
            if (exponent_negative) exponent = -exponent;
        } else {
            cursor = marker;
        }
    }

    result.converted = 1;
    result.end = cursor;
    if (!seen_nonzero) {
        result.value = rin_fp_binary80_value(0u, 0u, negative);
        return result;
    }
    {
        long long dropped_bits = total_bits - (long long)kept_bits;
        long long scale_wide = exponent - fractional_nibbles * 4LL +
                               dropped_bits;
        if (scale_wide < -100000LL || scale_wide > 100000LL ||
            !rin_fp_binary_to_binary80(
                significand, (int)scale_wide, round_bit, sticky, negative,
                &result.value)) {
            result.range_direction = scale_wide < 0 ? -1 : 1;
            result.value = result.range_direction > 0
                               ? rin_fp_binary80_value(
                                     ((rin_fp_u64)1u) << 63u, 0x7fffu,
                                     negative)
                               : rin_fp_binary80_value(0u, 0u, negative);
            return result;
        }
    }
    {
        union {
            long double value;
            unsigned char bytes[sizeof(long double)];
        } raw = {0};
        rin_fp_u16 encoded_exponent;
        raw.value = result.value;
        encoded_exponent = (rin_fp_u16)raw.bytes[8] |
                           (rin_fp_u16)((rin_fp_u16)raw.bytes[9] << 8u);
        result.subnormal = (encoded_exponent & 0x7fffu) == 0u;
    }
    return result;
}

static inline rin_long_double_parse_result rin_float_parse_binary80(
    const char* string)
{
    rin_long_double_parse_result result =
        rin_fp_empty_binary80_result(string);
    const char* original = string;
    const char* cursor;
    const char* special_end;
    int negative = 0;
    int hexadecimal = 0;

    if (!string) return result;
    cursor = string;
    while (rin_fp_ascii_space(*cursor)) ++cursor;
    if (*cursor == '-' || *cursor == '+') {
        negative = *cursor == '-';
        ++cursor;
    }
    if (rin_fp_match_word(cursor, "inf", &special_end)) {
        const char* infinity_end;
        if (rin_fp_match_word(special_end, "inity", &infinity_end))
            special_end = infinity_end;
        result.end = special_end;
        result.value = rin_fp_binary80_value(
            ((rin_fp_u64)1u) << 63u, 0x7fffu, negative);
        result.converted = 1;
        return result;
    }
    if (rin_fp_match_word(cursor, "nan", &special_end)) {
        if (*special_end == '(') {
            const char* scan = special_end + 1;
            while ((*scan >= '0' && *scan <= '9') ||
                   (*scan >= 'a' && *scan <= 'z') ||
                   (*scan >= 'A' && *scan <= 'Z') || *scan == '_')
                ++scan;
            if (*scan == ')') special_end = scan + 1;
        }
        result.end = special_end;
        result.value = rin_fp_binary80_value(
            0xc000000000000001ULL, 0x7fffu, negative);
        result.converted = 1;
        return result;
    }
    if (cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X')) {
        int first_digit = rin_fp_hex_digit(cursor[2]);
        int point_digit = cursor[2] == '.'
                              ? rin_fp_hex_digit(cursor[3]) : -1;
        hexadecimal = first_digit >= 0 || point_digit >= 0;
    }
    result = hexadecimal
                 ? rin_fp_parse_binary80_hex_magnitude(cursor + 2, negative)
                 : rin_fp_parse_binary80_decimal_magnitude(cursor, negative);
    if (!result.converted) {
        result.end = original;
        result.value = rin_fp_binary80_value(0u, 0u, 0);
    }
    return result;
}
#endif

static inline rin_float_parse_result rin_fp_parse_binary(
    const char* string, const rin_fp_traits* traits)
{
    rin_float_parse_result result = rin_fp_empty_result(string);
    const char* original = string;
    const char* cursor;
    const char* special_end;
    int negative = 0;
    int hexadecimal = 0;

    if (!string) return result;
    cursor = string;
    while (rin_fp_ascii_space(*cursor)) ++cursor;
    if (*cursor == '-' || *cursor == '+') {
        negative = *cursor == '-';
        ++cursor;
    }

    if (rin_fp_match_word(cursor, "inf", &special_end)) {
        const char* infinity_end;
        if (rin_fp_match_word(special_end, "inity", &infinity_end))
            special_end = infinity_end;
        result.end = special_end;
        result.bits = traits->infinity | (negative ? traits->sign : 0u);
        result.converted = 1;
        return result;
    }
    if (rin_fp_match_word(cursor, "nan", &special_end)) {
        if (*special_end == '(') {
            const char* scan = special_end + 1;
            while ((*scan >= '0' && *scan <= '9') ||
                   (*scan >= 'a' && *scan <= 'z') ||
                   (*scan >= 'A' && *scan <= 'Z') || *scan == '_')
                ++scan;
            if (*scan == ')') special_end = scan + 1;
        }
        result.end = special_end;
        result.bits = traits->nan | (negative ? traits->sign : 0u);
        result.converted = 1;
        return result;
    }

    if (cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X')) {
        int first_digit = rin_fp_hex_digit(cursor[2]);
        int point_digit = cursor[2] == '.'
                              ? rin_fp_hex_digit(cursor[3]) : -1;
        hexadecimal = first_digit >= 0 || point_digit >= 0;
    }
    result = hexadecimal
                 ? rin_fp_parse_hex_magnitude(cursor + 2, negative, traits)
                 : rin_fp_parse_decimal_magnitude(cursor, negative, traits);
    if (!result.converted) {
        result.end = original;
        result.bits = 0u;
    }
    return result;
}

static inline rin_float_parse_result rin_float_parse_binary32(
    const char* string)
{
    rin_fp_traits traits = rin_fp_binary32_traits();
    return rin_fp_parse_binary(string, &traits);
}

static inline rin_float_parse_result rin_float_parse_binary64(
    const char* string)
{
    rin_fp_traits traits = rin_fp_binary64_traits();
    return rin_fp_parse_binary(string, &traits);
}

static inline float rin_float_parse_binary32_value(rin_fp_u64 bits)
{
    union {
        rin_fp_u32 bits;
        float value;
    } converted;
    converted.bits = (rin_fp_u32)bits;
    return converted.value;
}

static inline double rin_float_parse_binary64_value(rin_fp_u64 bits)
{
    union {
        rin_fp_u64 bits;
        double value;
    } converted;
    converted.bits = bits;
    return converted.value;
}

#endif /* RIN_FLOAT_PARSE_H */
