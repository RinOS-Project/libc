/*
 * RinOS libc - tommath.h
 * Compatibility subset for Ladybird LibCrypto BigInt using rinTLS bignum.
 */

#ifndef _TOMMATH_H
#define _TOMMATH_H

#define BN_H_ 1

#include "ctype.h"
#include "limits.h"
#include "math.h"
#include "stdint.h"
#include "stdlib.h"
#include "string.h"

#if defined(__cplusplus) && defined(RINTLS_SKIP_BASIC_TYPEDEFS)
typedef __UINT8_TYPE__ u8;
typedef __UINT16_TYPE__ u16;
typedef __UINT32_TYPE__ u32;
typedef __UINT64_TYPE__ u64;
typedef __INT8_TYPE__ i8;
typedef __INT16_TYPE__ i16;
typedef __INT32_TYPE__ i32;
typedef __INT64_TYPE__ i64;
#endif

#include "../rintls/crypto/bignum.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int mp_err;
typedef uint64_t mp_digit;
typedef int mp_sign;

typedef struct {
    int used;
    int alloc;
    mp_sign sign;
    mp_digit* dp;
} mp_int;

enum {
    MP_OKAY = 0,
    MP_MEM = -2,
    MP_VAL = -3,
    MP_ITER = -4,
    MP_BUF = -5,
};

enum {
    MP_ZPOS = 0,
    MP_NEG = 1,
};

enum {
    MP_LT = -1,
    MP_EQ = 0,
    MP_GT = 1,
};

enum {
    MP_MSB_FIRST = 0,
    MP_LSB_FIRST = 1,
};

enum {
    MP_NATIVE_ENDIAN = 0,
    MP_BIG_ENDIAN = 1,
    MP_LITTLE_ENDIAN = 2,
};

#define MP_DIGIT_BIT ((int)(sizeof(mp_digit) * CHAR_BIT))
#define MP_DEFAULT_DIGITS ((BIGNUM_MAX_LIMBS / 2) + 2)

static inline int _mp_max_int(int a, int b)
{
    return a > b ? a : b;
}

static inline void _mp_zero_storage(mp_int* a)
{
    if (!a->dp || a->alloc <= 0)
        return;
    memset(a->dp, 0, (size_t)a->alloc * sizeof(mp_digit));
    a->used = 0;
    a->sign = MP_ZPOS;
}

static inline void _mp_clamp(mp_int* a)
{
    while (a->used > 0 && a->dp[a->used - 1] == 0)
        --a->used;
    if (a->used == 0)
        a->sign = MP_ZPOS;
}

static inline mp_err _mp_grow(mp_int* a, int min_digits)
{
    mp_digit* new_digits;
    size_t new_size;
    int i;

    if (min_digits < MP_DEFAULT_DIGITS)
        min_digits = MP_DEFAULT_DIGITS;

    if (a->dp && a->alloc >= min_digits)
        return MP_OKAY;

    new_size = (size_t)min_digits * sizeof(mp_digit);
    if (!a->dp) {
        new_digits = (mp_digit*)malloc(new_size);
        if (!new_digits)
            return MP_MEM;
        a->dp = new_digits;
        a->alloc = min_digits;
        _mp_zero_storage(a);
        return MP_OKAY;
    }

    new_digits = (mp_digit*)realloc(a->dp, new_size);
    if (!new_digits)
        return MP_MEM;
    for (i = a->alloc; i < min_digits; ++i)
        new_digits[i] = 0;
    a->dp = new_digits;
    a->alloc = min_digits;
    return MP_OKAY;
}

static inline void _mp_to_bignum(mp_int const* src, bignum_t* dst)
{
    int i;

    bn_init(dst);
    dst->sign = (src->sign == MP_NEG) ? 1 : 0;
    for (i = 0; i < src->used && (i * 2 + 1) < BIGNUM_MAX_LIMBS; ++i) {
        uint64_t digit = src->dp[i];
        dst->limbs[i * 2] = (u32)(digit & 0xffffffffu);
        dst->limbs[i * 2 + 1] = (u32)(digit >> 32);
    }
    dst->used = (rin_size_t)(src->used * 2);
    bn_normalize(dst);
}

static inline mp_err _mp_from_bignum(mp_int* dst, bignum_t const* src)
{
    int needed_digits;
    int i;

    needed_digits = (int)((src->used + 1) / 2);
    if (_mp_grow(dst, needed_digits) != MP_OKAY)
        return MP_MEM;

    _mp_zero_storage(dst);
    dst->sign = src->sign ? MP_NEG : MP_ZPOS;
    dst->used = needed_digits;

    for (i = 0; i < needed_digits; ++i) {
        uint64_t low = (uint64_t)src->limbs[i * 2];
        uint64_t high = 0;
        if ((i * 2 + 1) < (int)src->used)
            high = (uint64_t)src->limbs[i * 2 + 1];
        dst->dp[i] = low | (high << 32);
    }

    _mp_clamp(dst);
    return MP_OKAY;
}

static inline mp_err _mp_from_u64(mp_int* a, uint64_t value)
{
    if (_mp_grow(a, 1) != MP_OKAY)
        return MP_MEM;
    _mp_zero_storage(a);
    if (value != 0) {
        a->dp[0] = value;
        a->used = 1;
    }
    return MP_OKAY;
}

static inline mp_err _mp_from_i64(mp_int* a, int64_t value)
{
    uint64_t magnitude;

    if (value >= 0) {
        a->sign = MP_ZPOS;
        return _mp_from_u64(a, (uint64_t)value);
    }

    magnitude = (uint64_t)(-(value + 1)) + 1;
    if (_mp_from_u64(a, magnitude) != MP_OKAY)
        return MP_MEM;
    if (a->used != 0)
        a->sign = MP_NEG;
    return MP_OKAY;
}

static inline int mp_init(mp_int* a)
{
    if (!a)
        return MP_VAL;
    a->used = 0;
    a->alloc = 0;
    a->sign = MP_ZPOS;
    a->dp = 0;
    return _mp_grow(a, MP_DEFAULT_DIGITS);
}

static inline int mp_init_copy(mp_int* dst, mp_int const* src)
{
    if (!dst || !src)
        return MP_VAL;
    if (mp_init(dst) != MP_OKAY)
        return MP_MEM;
    if (_mp_grow(dst, src->used) != MP_OKAY)
        return MP_MEM;
    _mp_zero_storage(dst);
    dst->used = src->used;
    dst->sign = src->sign;
    if (src->used > 0)
        memcpy(dst->dp, src->dp, (size_t)src->used * sizeof(mp_digit));
    _mp_clamp(dst);
    return MP_OKAY;
}

static inline void mp_clear(mp_int* a)
{
    if (!a)
        return;
    if (a->dp) {
        memset(a->dp, 0, (size_t)a->alloc * sizeof(mp_digit));
        free(a->dp);
    }
    a->dp = 0;
    a->used = 0;
    a->alloc = 0;
    a->sign = MP_ZPOS;
}

static inline int mp_copy(mp_int const* src, mp_int* dst)
{
    if (!src || !dst)
        return MP_VAL;
    if (_mp_grow(dst, src->used) != MP_OKAY)
        return MP_MEM;
    _mp_zero_storage(dst);
    dst->used = src->used;
    dst->sign = src->sign;
    if (src->used > 0)
        memcpy(dst->dp, src->dp, (size_t)src->used * sizeof(mp_digit));
    _mp_clamp(dst);
    return MP_OKAY;
}

static inline void mp_zero(mp_int* a)
{
    if (!a)
        return;
    if (_mp_grow(a, 1) != MP_OKAY)
        return;
    _mp_zero_storage(a);
}

static inline void mp_set_u64(mp_int* a, uint64_t value)
{
    (void)_mp_from_u64(a, value);
}

static inline void mp_set_i64(mp_int* a, int64_t value)
{
    (void)_mp_from_i64(a, value);
}

static inline int mp_set_double(mp_int* a, double value)
{
    if (isnan(value) || isinf(value))
        return MP_VAL;
    return _mp_from_i64(a, (int64_t)value);
}

static inline uint64_t mp_get_u64(mp_int const* a)
{
    bignum_t bn;
    uint64_t result = 0;

    _mp_to_bignum(a, &bn);
    if (bn.used > 0)
        result |= (uint64_t)bn.limbs[0];
    if (bn.used > 1)
        result |= (uint64_t)bn.limbs[1] << 32;
    return result;
}

static inline int64_t mp_get_i64(mp_int const* a)
{
    uint64_t magnitude = mp_get_u64(a);
    if (a->sign == MP_NEG)
        return -(int64_t)magnitude;
    return (int64_t)magnitude;
}

static inline double mp_get_double(mp_int const* a)
{
    double result = 0.0;
    int i;

    for (i = a->used - 1; i >= 0; --i)
        result = result * 18446744073709551616.0 + (double)a->dp[i];
    return a->sign == MP_NEG ? -result : result;
}

static inline int mp_cmp(mp_int const* a, mp_int const* b)
{
    bignum_t a_bn;
    bignum_t b_bn;
    int cmp;

    _mp_to_bignum(a, &a_bn);
    _mp_to_bignum(b, &b_bn);
    cmp = bn_cmp(&a_bn, &b_bn);
    if (cmp < 0)
        return MP_LT;
    if (cmp > 0)
        return MP_GT;
    return MP_EQ;
}

static inline int mp_iszero(mp_int const* a)
{
    return !a || a->used == 0;
}

static inline int mp_isodd(mp_int const* a)
{
    return !mp_iszero(a) && ((a->dp[0] & 1u) != 0);
}

static inline int mp_isneg(mp_int const* a)
{
    return a && a->sign == MP_NEG && !mp_iszero(a);
}

static inline int mp_count_bits(mp_int const* a)
{
    bignum_t bn;
    _mp_to_bignum(a, &bn);
    return (int)bn_bitlen(&bn);
}

static inline int mp_abs(mp_int const* a, mp_int* b)
{
    if (mp_copy(a, b) != MP_OKAY)
        return MP_MEM;
    b->sign = MP_ZPOS;
    return MP_OKAY;
}

static inline int mp_neg(mp_int const* a, mp_int* b)
{
    if (mp_copy(a, b) != MP_OKAY)
        return MP_MEM;
    if (!mp_iszero(b))
        b->sign = (a->sign == MP_NEG) ? MP_ZPOS : MP_NEG;
    return MP_OKAY;
}

static inline int _mp_apply_binary_bignum_op(mp_int const* a, mp_int const* b, mp_int* out, int (*op)(bignum_t*, bignum_t const*, bignum_t const*))
{
    bignum_t a_bn;
    bignum_t b_bn;
    bignum_t out_bn;
    int rc;

    _mp_to_bignum(a, &a_bn);
    _mp_to_bignum(b, &b_bn);
    rc = op(&out_bn, &a_bn, &b_bn);
    if (rc != BIGNUM_OK)
        return MP_VAL;
    return _mp_from_bignum(out, &out_bn);
}

static inline int mp_add(mp_int const* a, mp_int const* b, mp_int* c)
{
    return _mp_apply_binary_bignum_op(a, b, c, bn_add);
}

static inline int mp_sub(mp_int const* a, mp_int const* b, mp_int* c)
{
    return _mp_apply_binary_bignum_op(a, b, c, bn_sub);
}

static inline int mp_mul(mp_int const* a, mp_int const* b, mp_int* c)
{
    return _mp_apply_binary_bignum_op(a, b, c, bn_mul);
}

static inline int mp_div(mp_int const* a, mp_int const* b, mp_int* q, mp_int* r)
{
    bignum_t a_bn;
    bignum_t b_bn;
    bignum_t q_bn;
    bignum_t r_bn;
    int rc;

    _mp_to_bignum(a, &a_bn);
    _mp_to_bignum(b, &b_bn);
    rc = bn_div(q ? &q_bn : 0, r ? &r_bn : 0, &a_bn, &b_bn);
    if (rc != BIGNUM_OK)
        return MP_VAL;
    if (q && _mp_from_bignum(q, &q_bn) != MP_OKAY)
        return MP_MEM;
    if (r && _mp_from_bignum(r, &r_bn) != MP_OKAY)
        return MP_MEM;
    return MP_OKAY;
}

static inline int mp_add_d(mp_int const* a, mp_digit b, mp_int* c)
{
    mp_int temp;
    if (mp_init(&temp) != MP_OKAY)
        return MP_MEM;
    mp_set_u64(&temp, b);
    if (mp_add(a, &temp, c) != MP_OKAY) {
        mp_clear(&temp);
        return MP_VAL;
    }
    mp_clear(&temp);
    return MP_OKAY;
}

static inline int mp_sub_d(mp_int const* a, mp_digit b, mp_int* c)
{
    mp_int temp;
    if (mp_init(&temp) != MP_OKAY)
        return MP_MEM;
    mp_set_u64(&temp, b);
    if (mp_sub(a, &temp, c) != MP_OKAY) {
        mp_clear(&temp);
        return MP_VAL;
    }
    mp_clear(&temp);
    return MP_OKAY;
}

static inline int mp_mul_2d(mp_int const* a, int b, mp_int* c)
{
    bignum_t a_bn;
    bignum_t c_bn;
    int rc;

    if (b < 0)
        return MP_VAL;
    _mp_to_bignum(a, &a_bn);
    rc = bn_lshift(&c_bn, &a_bn, (rin_size_t)b);
    if (rc != BIGNUM_OK)
        return MP_VAL;
    return _mp_from_bignum(c, &c_bn);
}

static inline int mp_div_2d(mp_int const* a, int b, mp_int* q, mp_int* r)
{
    bignum_t a_bn;
    bignum_t q_bn;
    bignum_t r_bn;
    bignum_t mod_bn;
    int rc;

    if (b < 0)
        return MP_VAL;
    _mp_to_bignum(a, &a_bn);
    if (q) {
        rc = bn_rshift(&q_bn, &a_bn, (rin_size_t)b);
        if (rc != BIGNUM_OK)
            return MP_VAL;
        if (_mp_from_bignum(q, &q_bn) != MP_OKAY)
            return MP_MEM;
    }
    if (r) {
        bn_init(&mod_bn);
        if (b == 0) {
            bn_init(&r_bn);
        } else {
            bn_set_u32(&mod_bn, 1);
            rc = BIGNUM_OK;
            if (bn_lshift(&mod_bn, &mod_bn, (rin_size_t)b) != BIGNUM_OK)
                return MP_VAL;
            if (bn_mod(&r_bn, &a_bn, &mod_bn) != BIGNUM_OK)
                return MP_VAL;
        }
        if (_mp_from_bignum(r, &r_bn) != MP_OKAY)
            return MP_MEM;
    }
    return MP_OKAY;
}

static inline int mp_mod_2d(mp_int const* a, int b, mp_int* c)
{
    return mp_div_2d(a, b, 0, c);
}

static inline int mp_2expt(mp_int* a, int b)
{
    bignum_t one;
    bignum_t out_bn;

    if (b < 0)
        return MP_VAL;
    bn_set_u32(&one, 1);
    if (bn_lshift(&out_bn, &one, (rin_size_t)b) != BIGNUM_OK)
        return MP_VAL;
    return _mp_from_bignum(a, &out_bn);
}

static inline int mp_expt_u32(mp_int const* a, uint32_t exponent, mp_int* c)
{
    bignum_t base;
    bignum_t result;
    bignum_t temp;
    uint32_t e = exponent;

    _mp_to_bignum(a, &base);
    bn_set_u32(&result, 1);

    while (e != 0) {
        if (e & 1u) {
            if (bn_mul(&temp, &result, &base) != BIGNUM_OK)
                return MP_VAL;
            bn_copy(&result, &temp);
        }
        e >>= 1;
        if (e != 0) {
            if (bn_mul(&temp, &base, &base) != BIGNUM_OK)
                return MP_VAL;
            bn_copy(&base, &temp);
        }
    }

    return _mp_from_bignum(c, &result);
}

static inline int mp_exptmod(mp_int const* base, mp_int const* exponent, mp_int const* modulus, mp_int* out)
{
    bignum_t base_bn;
    bignum_t exp_bn;
    bignum_t mod_bn;
    bignum_t out_bn;

    _mp_to_bignum(base, &base_bn);
    _mp_to_bignum(exponent, &exp_bn);
    _mp_to_bignum(modulus, &mod_bn);
    if (bn_mod_exp(&out_bn, &base_bn, &exp_bn, &mod_bn) != BIGNUM_OK)
        return MP_VAL;
    return _mp_from_bignum(out, &out_bn);
}

static inline int mp_gcd(mp_int const* a, mp_int const* b, mp_int* c)
{
    bignum_t a_bn;
    bignum_t b_bn;
    bignum_t c_bn;

    _mp_to_bignum(a, &a_bn);
    _mp_to_bignum(b, &b_bn);
    if (bn_gcd(&c_bn, &a_bn, &b_bn) != BIGNUM_OK)
        return MP_VAL;
    return _mp_from_bignum(c, &c_bn);
}

static inline int mp_lcm(mp_int const* a, mp_int const* b, mp_int* c)
{
    mp_int gcd;
    mp_int quotient;
    mp_int product;
    int rc;

    if (mp_init(&gcd) != MP_OKAY || mp_init(&quotient) != MP_OKAY || mp_init(&product) != MP_OKAY) {
        mp_clear(&gcd);
        mp_clear(&quotient);
        mp_clear(&product);
        return MP_MEM;
    }

    rc = mp_gcd(a, b, &gcd);
    if (rc == MP_OKAY)
        rc = mp_div(a, &gcd, &quotient, 0);
    if (rc == MP_OKAY)
        rc = mp_mul(&quotient, b, &product);
    if (rc == MP_OKAY) {
        product.sign = MP_ZPOS;
        rc = mp_copy(&product, c);
    }

    mp_clear(&gcd);
    mp_clear(&quotient);
    mp_clear(&product);
    return rc;
}

static inline void _mp_to_twos_complement(mp_int const* src, mp_digit* dst, int width)
{
    int i;
    mp_digit carry;

    for (i = 0; i < width; ++i)
        dst[i] = (i < src->used) ? src->dp[i] : 0;

    if (!mp_isneg(src))
        return;

    for (i = 0; i < width; ++i)
        dst[i] = ~dst[i];

    carry = 1;
    for (i = 0; i < width; ++i) {
        mp_digit next = dst[i] + carry;
        carry = (next < dst[i]) ? 1 : 0;
        dst[i] = next;
        if (!carry)
            break;
    }
}

static inline int _mp_from_twos_complement(mp_digit const* src, int width, mp_int* dst)
{
    int negative;
    int i;

    if (_mp_grow(dst, width) != MP_OKAY)
        return MP_MEM;
    _mp_zero_storage(dst);

    negative = (int)((src[width - 1] >> (MP_DIGIT_BIT - 1)) & 1u);
    if (!negative) {
        dst->used = width;
        for (i = 0; i < width; ++i)
            dst->dp[i] = src[i];
        dst->sign = MP_ZPOS;
        _mp_clamp(dst);
        return MP_OKAY;
    }

    dst->used = width;
    for (i = 0; i < width; ++i)
        dst->dp[i] = ~src[i];
    dst->sign = MP_NEG;

    {
        mp_digit carry = 1;
        for (i = 0; i < width; ++i) {
            mp_digit next = dst->dp[i] + carry;
            carry = (next < dst->dp[i]) ? 1 : 0;
            dst->dp[i] = next;
            if (!carry)
                break;
        }
    }

    _mp_clamp(dst);
    return MP_OKAY;
}

static inline int _mp_bitwise_binary(mp_int const* a, mp_int const* b, mp_int* out, int op)
{
    int width = _mp_max_int(a->used, b->used) + 1;
    mp_digit lhs[MP_DEFAULT_DIGITS];
    mp_digit rhs[MP_DEFAULT_DIGITS];
    mp_digit result[MP_DEFAULT_DIGITS];
    int i;

    if (width > MP_DEFAULT_DIGITS)
        return MP_MEM;

    _mp_to_twos_complement(a, lhs, width);
    _mp_to_twos_complement(b, rhs, width);

    for (i = 0; i < width; ++i) {
        if (op == 0)
            result[i] = lhs[i] & rhs[i];
        else if (op == 1)
            result[i] = lhs[i] | rhs[i];
        else
            result[i] = lhs[i] ^ rhs[i];
    }

    return _mp_from_twos_complement(result, width, out);
}

static inline int mp_and(mp_int const* a, mp_int const* b, mp_int* c)
{
    return _mp_bitwise_binary(a, b, c, 0);
}

static inline int mp_or(mp_int const* a, mp_int const* b, mp_int* c)
{
    return _mp_bitwise_binary(a, b, c, 1);
}

static inline int mp_xor(mp_int const* a, mp_int const* b, mp_int* c)
{
    return _mp_bitwise_binary(a, b, c, 2);
}

static inline int mp_complement(mp_int const* a, mp_int* b)
{
    mp_int temp;
    int rc;

    if (mp_init(&temp) != MP_OKAY)
        return MP_MEM;
    rc = mp_neg(a, &temp);
    if (rc == MP_OKAY)
        rc = mp_sub_d(&temp, 1, b);
    mp_clear(&temp);
    return rc;
}

static inline size_t mp_ubin_size(mp_int const* a)
{
    int bits = mp_count_bits(a);
    return bits == 0 ? 0 : (size_t)((bits + 7) / 8);
}

static inline int mp_to_ubin(mp_int const* a, unsigned char* buf, size_t maxlen, size_t* written)
{
    bignum_t bn;
    size_t size;

    _mp_to_bignum(a, &bn);
    size = mp_ubin_size(a);
    if (written)
        *written = size;
    if (size > maxlen)
        return MP_BUF;
    if (size == 0)
        return MP_OKAY;
    return bn_to_bytes(&bn, buf, size) == BIGNUM_OK ? MP_OKAY : MP_VAL;
}

static inline int mp_from_ubin(mp_int* a, unsigned char const* buf, size_t len)
{
    bignum_t bn;
    if (bn_from_bytes(&bn, buf, len) != BIGNUM_OK)
        return MP_VAL;
    bn.sign = 0;
    return _mp_from_bignum(a, &bn);
}

static inline size_t mp_sbin_size(mp_int const* a)
{
    size_t abs_size;

    if (mp_iszero(a))
        return 1;

    abs_size = mp_ubin_size(a);
    if (!mp_isneg(a)) {
        if (a->used > 0 && (a->dp[a->used - 1] >> (((abs_size - 1) % sizeof(mp_digit)) * 8 + 7)) != 0)
            return abs_size + 1;
        {
            unsigned char first_byte = 0;
            unsigned char buffer[MP_DEFAULT_DIGITS * sizeof(mp_digit)];
            size_t written = 0;
            if (mp_to_ubin(a, buffer, sizeof(buffer), &written) == MP_OKAY && written > 0)
                first_byte = buffer[0];
            return (first_byte & 0x80u) ? (abs_size + 1) : abs_size;
        }
    }

    return abs_size;
}

static inline int mp_to_sbin(mp_int const* a, unsigned char* buf, size_t maxlen, size_t* written)
{
    size_t size;

    if (mp_iszero(a)) {
        if (maxlen < 1)
            return MP_BUF;
        buf[0] = 0;
        if (written)
            *written = 1;
        return MP_OKAY;
    }

    if (!mp_isneg(a)) {
        unsigned char temp[MP_DEFAULT_DIGITS * sizeof(mp_digit) + 1];
        size_t unsigned_written = 0;
        if (mp_to_ubin(a, temp + 1, sizeof(temp) - 1, &unsigned_written) != MP_OKAY)
            return MP_VAL;
        if (temp[1] & 0x80u) {
            temp[0] = 0;
            size = unsigned_written + 1;
            if (size > maxlen)
                return MP_BUF;
            memcpy(buf, temp, size);
        } else {
            size = unsigned_written;
            if (size > maxlen)
                return MP_BUF;
            memcpy(buf, temp + 1, size);
        }
        if (written)
            *written = size;
        return MP_OKAY;
    }

    {
        mp_int magnitude;
        unsigned char temp[MP_DEFAULT_DIGITS * sizeof(mp_digit) + 2];
        size_t mag_written = 0;
        size_t width;
        size_t i;
        int carry;
        int rc;

        if (mp_init(&magnitude) != MP_OKAY)
            return MP_MEM;
        rc = mp_abs(a, &magnitude);
        if (rc != MP_OKAY) {
            mp_clear(&magnitude);
            return rc;
        }
        rc = mp_to_ubin(&magnitude, temp + 1, sizeof(temp) - 1, &mag_written);
        mp_clear(&magnitude);
        if (rc != MP_OKAY)
            return rc;

        width = mag_written;
        if (width == 0)
            width = 1;

        while (1) {
            memset(temp, 0, sizeof(temp));
            if (mag_written > 0)
                memcpy(temp + (width - mag_written), temp + 1, mag_written);

            for (i = 0; i < width; ++i)
                temp[i] = (unsigned char)~temp[i];
            carry = 1;
            for (i = width; i > 0; --i) {
                unsigned int value = (unsigned int)temp[i - 1] + (unsigned int)carry;
                temp[i - 1] = (unsigned char)value;
                carry = (value >> 8) & 1u;
                if (!carry)
                    break;
            }
            if (temp[0] & 0x80u)
                break;
            ++width;
        }

        if (width > maxlen)
            return MP_BUF;
        memcpy(buf, temp, width);
        if (written)
            *written = width;
    }

    return MP_OKAY;
}

static inline int mp_from_sbin(mp_int* a, unsigned char const* buf, size_t len)
{
    unsigned char temp[MP_DEFAULT_DIGITS * sizeof(mp_digit) + 2];
    size_t i;
    int carry;
    int rc;

    if (len == 0)
        return mp_init(a);

    if (!(buf[0] & 0x80u)) {
        rc = mp_from_ubin(a, buf, len);
        if (rc == MP_OKAY)
            a->sign = MP_ZPOS;
        return rc;
    }

    if (len > sizeof(temp))
        return MP_BUF;
    memcpy(temp, buf, len);
    for (i = 0; i < len; ++i)
        temp[i] = (unsigned char)~temp[i];
    carry = 1;
    for (i = len; i > 0; --i) {
        unsigned int value = (unsigned int)temp[i - 1] + (unsigned int)carry;
        temp[i - 1] = (unsigned char)value;
        carry = (value >> 8) & 1u;
        if (!carry)
            break;
    }

    rc = mp_from_ubin(a, temp, len);
    if (rc == MP_OKAY && !mp_iszero(a))
        a->sign = MP_NEG;
    return rc;
}

static inline int _mp_digit_from_char(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'z')
        return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'Z')
        return 10 + (ch - 'A');
    return -1;
}

static inline int mp_read_radix(mp_int* a, char const* str, int radix)
{
    mp_int result;
    int negative = 0;
    int rc;

    if (!str || radix < 2 || radix > 36)
        return MP_VAL;

    if (mp_init(&result) != MP_OKAY)
        return MP_MEM;

    if (*str == '-') {
        negative = 1;
        ++str;
    } else if (*str == '+') {
        ++str;
    }

    while (*str) {
        int digit;
        mp_int temp;
        if (*str == '_') {
            ++str;
            continue;
        }
        digit = _mp_digit_from_char(*str);
        if (digit < 0 || digit >= radix) {
            mp_clear(&result);
            return MP_VAL;
        }
        if (mp_init(&temp) != MP_OKAY) {
            mp_clear(&result);
            return MP_MEM;
        }
        rc = mp_mul_2d(&result, 0, &temp);
        if (rc == MP_OKAY) {
            bignum_t temp_bn;
            bignum_t radix_bn;
            bignum_t result_bn;
            bignum_t digit_bn;
            _mp_to_bignum(&result, &result_bn);
            bn_set_u32(&radix_bn, (u32)radix);
            bn_set_u32(&digit_bn, (u32)digit);
            rc = bn_mul(&temp_bn, &result_bn, &radix_bn);
            if (rc == BIGNUM_OK)
                rc = bn_add(&result_bn, &temp_bn, &digit_bn);
            if (rc == BIGNUM_OK)
                rc = _mp_from_bignum(&temp, &result_bn);
            else
                rc = MP_VAL;
        }
        if (rc == MP_OKAY)
            rc = mp_copy(&temp, &result);
        mp_clear(&temp);
        if (rc != MP_OKAY) {
            mp_clear(&result);
            return rc;
        }
        ++str;
    }

    if (negative && !mp_iszero(&result))
        result.sign = MP_NEG;

    rc = mp_copy(&result, a);
    mp_clear(&result);
    return rc;
}

static inline int _mp_radix_digit_count(mp_int const* a, int radix)
{
    bignum_t value;
    bignum_t divisor;
    bignum_t quotient;
    bignum_t remainder;
    int count = 0;

    _mp_to_bignum(a, &value);
    value.sign = 0;
    bn_set_u32(&divisor, (u32)radix);

    if (bn_is_zero(&value))
        return 1;

    while (!bn_is_zero(&value)) {
        if (bn_div(&quotient, &remainder, &value, &divisor) != BIGNUM_OK)
            return 0;
        bn_copy(&value, &quotient);
        ++count;
    }

    return count;
}

static inline int mp_radix_size(mp_int const* a, int radix, int* size)
{
    int digits;

    if (!size || radix < 2 || radix > 36)
        return MP_VAL;

    digits = _mp_radix_digit_count(a, radix);
    if (digits <= 0)
        return MP_VAL;
    if (mp_isneg(a))
        ++digits;
    *size = digits + 1;
    return MP_OKAY;
}

static inline int mp_to_radix(mp_int const* a, char* str, size_t maxlen, size_t* written, int radix)
{
    static char const digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    bignum_t value;
    bignum_t divisor;
    bignum_t quotient;
    bignum_t remainder;
    char buffer[2048];
    size_t index = 0;
    size_t out_index = 0;

    if (!str || radix < 2 || radix > 36)
        return MP_VAL;

    _mp_to_bignum(a, &value);
    value.sign = 0;
    bn_set_u32(&divisor, (u32)radix);

    if (bn_is_zero(&value))
        buffer[index++] = '0';
    else {
        while (!bn_is_zero(&value)) {
            if (bn_div(&quotient, &remainder, &value, &divisor) != BIGNUM_OK)
                return MP_VAL;
            buffer[index++] = digits[remainder.used ? remainder.limbs[0] : 0];
            bn_copy(&value, &quotient);
        }
    }

    if (mp_isneg(a))
        buffer[index++] = '-';

    if (index + 1 > maxlen)
        return MP_BUF;

    while (index > 0)
        str[out_index++] = buffer[--index];
    str[out_index++] = '\0';
    if (written)
        *written = out_index;
    return MP_OKAY;
}

static inline size_t mp_pack_count(mp_int const* a, size_t nails, size_t size)
{
    if (nails != 0)
        return 0;
    if (size == sizeof(uint32_t)) {
        bignum_t bn;
        _mp_to_bignum(a, &bn);
        return bn.used;
    }
    if (size == sizeof(mp_digit))
        return (size_t)a->used;
    return 0;
}

static inline mp_err _mp_read_chunk(unsigned char const* input, size_t size, int endian, uint64_t* out)
{
    size_t i;
    uint64_t value = 0;

    if (endian == MP_NATIVE_ENDIAN || endian == MP_LITTLE_ENDIAN) {
        for (i = 0; i < size; ++i)
            value |= ((uint64_t)input[i]) << (i * 8);
    } else if (endian == MP_BIG_ENDIAN) {
        for (i = 0; i < size; ++i)
            value = (value << 8) | input[i];
    } else {
        return MP_VAL;
    }
    *out = value;
    return MP_OKAY;
}

static inline void _mp_write_chunk(unsigned char* output, size_t size, int endian, uint64_t value)
{
    size_t i;
    if (endian == MP_NATIVE_ENDIAN || endian == MP_LITTLE_ENDIAN) {
        for (i = 0; i < size; ++i) {
            output[i] = (unsigned char)(value & 0xffu);
            value >>= 8;
        }
    } else {
        for (i = size; i > 0; --i) {
            output[i - 1] = (unsigned char)(value & 0xffu);
            value >>= 8;
        }
    }
}

static inline int mp_unpack(mp_int* rop, size_t count, int order, size_t size, int endian, size_t nails, void const* op)
{
    bignum_t bn;
    size_t i;
    unsigned char const* input = (unsigned char const*)op;

    if (!rop || !op || nails != 0)
        return MP_VAL;

    bn_init(&bn);
    if (order == MP_LSB_FIRST && size == sizeof(uint32_t) && count <= BIGNUM_MAX_LIMBS) {
        for (i = 0; i < count; ++i) {
            uint64_t value = 0;
            if (_mp_read_chunk(input + (i * size), size, endian, &value) != MP_OKAY)
                return MP_VAL;
            bn.limbs[i] = (u32)value;
        }
        bn.used = (rin_size_t)count;
        bn_normalize(&bn);
        return _mp_from_bignum(rop, &bn);
    }

    return MP_VAL;
}

static inline int mp_pack(void* rop, size_t maxcount, size_t* written, int order, size_t size, int endian, size_t nails, mp_int const* op)
{
    bignum_t bn;
    size_t i;
    unsigned char* output = (unsigned char*)rop;

    if (!rop || !op || nails != 0)
        return MP_VAL;

    _mp_to_bignum(op, &bn);
    if (order == MP_LSB_FIRST && size == sizeof(uint32_t)) {
        size_t count = bn.used;
        if (count > maxcount)
            return MP_BUF;
        for (i = 0; i < count; ++i)
            _mp_write_chunk(output + (i * size), size, endian, bn.limbs[i]);
        if (written)
            *written = count;
        return MP_OKAY;
    }

    return MP_VAL;
}

#ifdef __cplusplus
}
#endif

#endif /* _TOMMATH_H */
