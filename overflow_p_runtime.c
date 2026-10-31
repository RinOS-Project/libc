/* SPDX-License-Identifier: MIT */
#include <stdint.h>

typedef struct RinOverflowWide {
    uint64_t low;
    uint64_t high;
} RinOverflowWide;

static RinOverflowWide rin_overflow_wide_from_integer(
    uint64_t value, int is_unsigned)
{
    RinOverflowWide result;
    result.low = value;
    result.high = !is_unsigned && (value >> 63) != 0u ? UINT64_MAX : 0u;
    return result;
}

static RinOverflowWide rin_overflow_wide_add(
    RinOverflowWide left, RinOverflowWide right)
{
    RinOverflowWide result;
    result.low = left.low + right.low;
    result.high = left.high + right.high + (result.low < left.low);
    return result;
}

static RinOverflowWide rin_overflow_wide_subtract(
    RinOverflowWide left, RinOverflowWide right)
{
    RinOverflowWide result;
    result.low = left.low - right.low;
    result.high = left.high - right.high - (left.low < right.low);
    return result;
}

static RinOverflowWide rin_overflow_wide_multiply_unsigned(
    uint64_t left, uint64_t right)
{
    const uint64_t mask = UINT64_C(0xffffffff);
    uint64_t left_low = left & mask;
    uint64_t left_high = left >> 32;
    uint64_t right_low = right & mask;
    uint64_t right_high = right >> 32;
    uint64_t word0 = left_low * right_low;
    uint64_t intermediate = left_high * right_low + (word0 >> 32);
    uint64_t word1 = intermediate & mask;
    uint64_t word2 = intermediate >> 32;
    RinOverflowWide result;

    word1 += left_low * right_high;
    result.high = left_high * right_high + word2 + (word1 >> 32);
    result.low = (word1 << 32) | (word0 & mask);
    return result;
}

static RinOverflowWide rin_overflow_wide_negate(RinOverflowWide value)
{
    RinOverflowWide result;
    result.low = ~value.low + 1u;
    result.high = ~value.high + (result.low == 0u);
    return result;
}

static RinOverflowWide rin_overflow_wide_multiply(
    uint64_t left, int left_unsigned, uint64_t right, int right_unsigned)
{
    int left_negative = !left_unsigned && (left >> 63) != 0u;
    int right_negative = !right_unsigned && (right >> 63) != 0u;
    uint64_t left_magnitude = left_negative ? 0u - left : left;
    uint64_t right_magnitude = right_negative ? 0u - right : right;
    RinOverflowWide result = rin_overflow_wide_multiply_unsigned(
        left_magnitude, right_magnitude);
    if (left_negative != right_negative) {
        result = rin_overflow_wide_negate(result);
    }
    return result;
}

static int rin_overflow_wide_fits(
    RinOverflowWide value, unsigned precision, int result_unsigned)
{
    uint64_t low_mask;
    uint64_t upper_low;
    int negative;
    if (precision == 0u || precision > 64u) return 0;
    if (result_unsigned) {
        return value.high == 0u &&
            (precision == 64u || (value.low >> precision) == 0u);
    }
    negative = (int)((value.low >> (precision - 1u)) & 1u);
    if (precision == 64u) {
        return value.high == (negative ? UINT64_MAX : 0u);
    }
    low_mask = (UINT64_C(1) << precision) - 1u;
    upper_low = value.low & ~low_mask;
    return value.high == (negative ? UINT64_MAX : 0u) &&
        upper_low == (negative ? ~low_mask : 0u);
}

int __rin_builtin_overflow_p_v1(
    int operation, uint64_t left, int left_unsigned,
    uint64_t right, int right_unsigned,
    unsigned result_precision, int result_unsigned)
{
    RinOverflowWide exact;
    if (operation == 0) {
        exact = rin_overflow_wide_add(
            rin_overflow_wide_from_integer(left, left_unsigned),
            rin_overflow_wide_from_integer(right, right_unsigned));
    } else if (operation == 1) {
        exact = rin_overflow_wide_subtract(
            rin_overflow_wide_from_integer(left, left_unsigned),
            rin_overflow_wide_from_integer(right, right_unsigned));
    } else if (operation == 2) {
        exact = rin_overflow_wide_multiply(
            left, left_unsigned, right, right_unsigned);
    } else {
        return 1;
    }
    return !rin_overflow_wide_fits(
        exact, result_precision, result_unsigned);
}
