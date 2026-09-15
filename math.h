/*
 * RinOS libc - math.h
 * 数学関数 (簡易実装)
 */

#ifndef _MATH_H
#define _MATH_H

#include "stddef.h"
#include "float.h"
#include "limits.h"
#include "fenv.h"

#if FLT_EVAL_METHOD == 0
typedef float  float_t;
typedef double double_t;
#elif FLT_EVAL_METHOD == 1
typedef double float_t;
typedef double double_t;
#elif FLT_EVAL_METHOD == 2
typedef long double float_t;
typedef long double double_t;
#else
#error Unknown FLT_EVAL_METHOD
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_E
#define M_E 2.71828182845904523536
#endif

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

#ifndef M_PI_4
#define M_PI_4 0.78539816339744830962
#endif

#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif

#ifndef M_LN10
#define M_LN10 2.30258509299404568402
#endif

#ifndef M_SQRT1_2
#define M_SQRT1_2 0.70710678118654752440
#endif

#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

/* 分類マクロ定数 */
#define FP_NAN       0
#define FP_INFINITE  1
#define FP_ZERO      2
#define FP_SUBNORMAL 3
#define FP_NORMAL    4

#ifndef MIDL_PASS
/* IEEE 754特殊値 - ビット操作による完全実装 */

/* GCC/Clang built-in を使用して定数式として定義 */
#if defined(__GNUC__) || defined(__clang__)
#define HUGE_VAL    __builtin_huge_val()
#define HUGE_VALF   __builtin_huge_valf()
#define HUGE_VALL   __builtin_huge_vall()
#define INFINITY    __builtin_inff()
#define NAN         __builtin_nanf("")
#else
/* Fallback: inline関数版 */
static inline double _libc_make_huge_val(void) {
    union { double d; unsigned long long i; } u;
    u.i = 0x7FF0000000000000ULL;  /* +Infinity */
    return u.d;
}
static inline float _libc_make_huge_valf(void) {
    union { float f; unsigned int i; } u;
    u.i = 0x7F800000U;  /* +Infinity */
    return u.f;
}
static inline float _libc_make_nanf(void) {
    union { float f; unsigned int i; } u;
    u.i = 0x7FC00000U;  /* Quiet NaN */
    return u.f;
}

#define HUGE_VAL    (_libc_make_huge_val())
#define HUGE_VALF   (_libc_make_huge_valf())
#define HUGE_VALL   ((long double)_libc_make_huge_val())
#define INFINITY    (_libc_make_huge_valf())
#define NAN         (_libc_make_nanf())
#endif

/* C99 constructors.  The payload is intentionally ignored: the
 * freestanding runtime has no payload ABI, while callers still require a
 * quiet NaN rather than a synthetic finite fallback.  Compiler builtins
 * materialize the IEEE-754 constants without a libm dependency. */
static inline double nan(const char* tagp) {
    (void)tagp;
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_nan("");
#else
    union { double d; unsigned long long i; } u;
    u.i = 0x7FF8000000000000ULL;
    return u.d;
#endif
}

static inline float nanf(const char* tagp) {
    (void)tagp;
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_nanf("");
#else
    union { float f; unsigned int i; } u;
    u.i = 0x7FC00000U;
    return u.f;
#endif
}

/* ═══════════════════════════════════════════════════════════════
 * 浮動小数点分類関数 - IEEE 754 完全実装
 * C++モードでは cmath.h が std:: 版を提供するため、Cのみで定義
 * ═══════════════════════════════════════════════════════════════ */

#ifndef __cplusplus

/* isnan - NaN判定 (C用) */
static inline int _libc_isnan_f(float x) {
    union { float f; unsigned int i; } u;
    u.f = x;
    unsigned int exp = (u.i >> 23) & 0xFF;
    unsigned int frac = u.i & 0x7FFFFF;
    return (exp == 0xFF) && (frac != 0);
}
static inline int _libc_isnan_d(double x) {
    union { double d; unsigned long long i; } u;
    u.d = x;
    unsigned long long exp = (u.i >> 52) & 0x7FF;
    unsigned long long frac = u.i & 0xFFFFFFFFFFFFFULL;
    return (exp == 0x7FF) && (frac != 0);
}
#define isnan(x) _Generic((x), \
    float: _libc_isnan_f, \
    double: _libc_isnan_d, \
    default: _libc_isnan_d)(x)

/* isinf - 無限大判定 (C用) */
static inline int _libc_isinf_f(float x) {
    union { float f; unsigned int i; } u;
    u.f = x;
    unsigned int exp = (u.i >> 23) & 0xFF;
    unsigned int frac = u.i & 0x7FFFFF;
    return (exp == 0xFF) && (frac == 0);
}
static inline int _libc_isinf_d(double x) {
    union { double d; unsigned long long i; } u;
    u.d = x;
    unsigned long long exp = (u.i >> 52) & 0x7FF;
    unsigned long long frac = u.i & 0xFFFFFFFFFFFFFULL;
    return (exp == 0x7FF) && (frac == 0);
}
#define isinf(x) _Generic((x), \
    float: _libc_isinf_f, \
    double: _libc_isinf_d, \
    default: _libc_isinf_d)(x)

/* isfinite - 有限数判定 (C用) */
static inline int _libc_isfinite_f(float x) {
    union { float f; unsigned int i; } u;
    u.f = x;
    unsigned int exp = (u.i >> 23) & 0xFF;
    return exp != 0xFF;
}
static inline int _libc_isfinite_d(double x) {
    union { double d; unsigned long long i; } u;
    u.d = x;
    unsigned long long exp = (u.i >> 52) & 0x7FF;
    return exp != 0x7FF;
}
#define isfinite(x) _Generic((x), \
    float: _libc_isfinite_f, \
    double: _libc_isfinite_d, \
    default: _libc_isfinite_d)(x)

/* isnormal - 正規化数判定 (C用) */
static inline int _libc_isnormal_f(float x) {
    union { float f; unsigned int i; } u;
    u.f = x;
    unsigned int exp = (u.i >> 23) & 0xFF;
    return (exp != 0) && (exp != 0xFF);
}
static inline int _libc_isnormal_d(double x) {
    union { double d; unsigned long long i; } u;
    u.d = x;
    unsigned long long exp = (u.i >> 52) & 0x7FF;
    return (exp != 0) && (exp != 0x7FF);
}
#define isnormal(x) _Generic((x), \
    float: _libc_isnormal_f, \
    double: _libc_isnormal_d, \
    default: _libc_isnormal_d)(x)

/* signbit - 符号ビット判定 (C用) */
static inline int _libc_signbit_f(float x) {
    union { float f; unsigned int i; } u;
    u.f = x;
    return (u.i >> 31) != 0;
}
static inline int _libc_signbit_d(double x) {
    union { double d; unsigned long long i; } u;
    u.d = x;
    return (u.i >> 63) != 0;
}
#define signbit(x) _Generic((x), \
    float: _libc_signbit_f, \
    double: _libc_signbit_d, \
    default: _libc_signbit_d)(x)

/* fpclassify - 浮動小数点数分類 (C用) */
static inline int _libc_fpclassify_f(float x) {
    union { float f; unsigned int i; } u;
    u.f = x;
    unsigned int exp = (u.i >> 23) & 0xFF;
    unsigned int frac = u.i & 0x7FFFFF;
    if (exp == 0xFF) {
        return (frac == 0) ? FP_INFINITE : FP_NAN;
    } else if (exp == 0) {
        return (frac == 0) ? FP_ZERO : FP_SUBNORMAL;
    }
    return FP_NORMAL;
}
static inline int _libc_fpclassify_d(double x) {
    union { double d; unsigned long long i; } u;
    u.d = x;
    unsigned long long exp = (u.i >> 52) & 0x7FF;
    unsigned long long frac = u.i & 0xFFFFFFFFFFFFFULL;
    if (exp == 0x7FF) {
        return (frac == 0) ? FP_INFINITE : FP_NAN;
    } else if (exp == 0) {
        return (frac == 0) ? FP_ZERO : FP_SUBNORMAL;
    }
    return FP_NORMAL;
}
#define fpclassify(x) _Generic((x), \
    float: _libc_fpclassify_f, \
    double: _libc_fpclassify_d, \
    default: _libc_fpclassify_d)(x)

#endif /* !__cplusplus */

#ifdef __cplusplus
static inline int signbit(float x) {
    union {
        float f;
        unsigned int i;
    } u;
    u.f = x;
    return (u.i >> 31) != 0;
}

static inline int signbit(double x) {
    union {
        double d;
        unsigned long long i;
    } u;
    u.d = x;
    return (u.i >> 63) != 0;
}

static inline double nextafter(double from, double to) {
    union {
        double d;
        unsigned long long i;
    } u;

    if (from == to)
        return to;
    /* IEEE nextafter propagates a NaN operand.  The old implementation
     * treated a NaN payload as an ordered integer encoding and could return
     * an unrelated finite value (or a signaling NaN) after incrementing the
     * bit pattern.  Test both operands before the zero/encoding paths. */
    if (from != from || to != to)
        return from + to;
    if (from == 0.0) {
        u.i = (to > 0.0) ? 1ULL : (1ULL << 63) | 1ULL;
        return u.d;
    }

    u.d = from;
    if ((to > from) == (from > 0.0))
        ++u.i;
    else
        --u.i;
    return u.d;
}

static inline float nextafterf(float from, float to) {
    union {
        float f;
        unsigned int i;
    } u;

    if (from == to)
        return to;
    if (from != from || to != to)
        return from + to;
    if (from == 0.0f) {
        u.i = (to > 0.0f) ? 1U : (1U << 31) | 1U;
        return u.f;
    }

    u.f = from;
    if ((to > from) == (from > 0.0f))
        ++u.i;
    else
        --u.i;
    return u.f;
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations for helpers referenced before their definitions. */
static inline double trunc(double x);
static inline double exp(double x);
static inline double log(double x);
static inline double frexp(double x, int* exp);
static inline double ldexp(double x, int exp);
static inline double atan(double x);

/* ═══════════════════════════════════════════════════════════════
 * 基本数学関数 (Taylor展開による近似)
 * ═══════════════════════════════════════════════════════════════ */

/* fabs - 絶対値 */
static inline double fabs(double x) {
    union { double value; unsigned long long bits; } u;
    u.value = x;
    u.bits &= ~(1ULL << 63);
    return u.value;
}

static inline float fabsf(float x) {
    union { float value; unsigned int bits; } u;
    u.value = x;
    u.bits &= ~(1U << 31);
    return u.value;
}

/* fmod - 浮動小数点剰余 */
static inline double fmod(double x, double y) {
    if (__builtin_isnan(x) || __builtin_isnan(y) || y == 0.0 ||
        __builtin_isinf(x)) return NAN;
    if (__builtin_isinf(y) || x == 0.0) return x;

    int ex = 0;
    int ey = 0;
    double ax = fabs(x);
    double ay = fabs(y);
    double mx = frexp(ax, &ex);
    double my = frexp(ay, &ey);
    if (ex < ey || (ex == ey && mx < my)) return x;

    /* Mantissa long division keeps every intermediate below one, so the
     * quotient cannot overflow even for DBL_MAX divided by a subnormal. */
    double remainder = mx;
    if (remainder >= my) remainder -= my;
    int shift = ex - ey;
    for (int i = 0; i < 4096 && i < shift; ++i) {
        remainder *= 2.0;
        if (remainder >= my) remainder -= my;
    }
    double result = ldexp(remainder, ey);
    return __builtin_signbit(x) ? -result : result;
}

static inline float fmodf(float x, float y) {
    return (float)fmod((double)x, (double)y);
}

/* sin - 正弦 (Taylor展開) */
static inline double sin(double x) {
    if (__builtin_isnan(x) || __builtin_isinf(x)) return NAN;
    /* Bounded remainder reduction avoids one-subtraction-per-period work. */
    x = fmod(x, 2 * M_PI);

    /* Taylor展開: sin(x) = x - x^3/3! + x^5/5! - x^7/7! + ... */
    double term = x;
    double sum = x;
    double x2 = x * x;

    for (int i = 1; i <= 10; i++) {
        term *= -x2 / ((2*i) * (2*i + 1));
        sum += term;
    }
    return sum;
}

static inline float sinf(float x) {
    return (float)sin((double)x);
}

/* cos - 余弦 (Taylor展開) */
static inline double cos(double x) {
    if (__builtin_isnan(x) || __builtin_isinf(x)) return NAN;
    x = fmod(x, 2 * M_PI);

    /* Taylor展開: cos(x) = 1 - x^2/2! + x^4/4! - x^6/6! + ... */
    double term = 1.0;
    double sum = 1.0;
    double x2 = x * x;

    for (int i = 1; i <= 10; i++) {
        term *= -x2 / ((2*i - 1) * (2*i));
        sum += term;
    }
    return sum;
}

static inline float cosf(float x) {
    return (float)cos((double)x);
}

/* sincos - calculate sine and cosine in one ABI-compatible call. */
static inline void sincos(double x, double* sinp, double* cosp) {
    if (sinp != NULL) *sinp = sin(x);
    if (cosp != NULL) *cosp = cos(x);
}

static inline void sincosf(float x, float* sinp, float* cosp) {
    if (sinp != NULL) *sinp = sinf(x);
    if (cosp != NULL) *cosp = cosf(x);
}

/* tan - 正接 */
static inline double tan(double x) {
    if (__builtin_isnan(x) || __builtin_isinf(x)) return NAN;
    if (x == 0.0) return x;
    double s = sin(x);
    double c = cos(x);
    /* At an exactly represented pole, preserve the sign of sin instead of
     * silently claiming that tan is zero.  The finite reduced path below
     * still returns the quotient for near-pole arguments. */
    if (c == 0.0) return __builtin_copysign(HUGE_VAL, s);
    return s / c;
}

static inline float tanf(float x) {
    return (float)tan((double)x);
}

/* sqrt - 平方根 (Newton法) */
static inline double sqrt(double x) {
    if (__builtin_isnan(x)) return x;
    if (x < 0) return NAN;
    if (__builtin_isinf(x) || x == 0) return x;

    int exponent = 0;
    double mantissa = frexp(x, &exponent);
    if (exponent % 2 != 0) {
        mantissa *= 2.0;
        --exponent;
    }

    /* The normalized mantissa is in [1, 2), so this fixed guess converges
     * quadratically instead of spending all iterations halving DBL_MAX. */
    double guess = 0.8;
    for (int i = 0; i < 12; i++) {
        guess = (guess + mantissa / guess) / 2.0;
    }
    return ldexp(guess, exponent / 2);
}

static inline float sqrtf(float x) {
    return (float)sqrt((double)x);
}

/* pow - べき乗 (整数指数のbinary exponentiationと正底の実数指数) */
static inline double pow(double base, double exponent) {
    if (exponent == 0.0 || base == 1.0) return 1.0;
    if (__builtin_isnan(base) || __builtin_isnan(exponent)) return NAN;

    int neg_exp = exponent < 0.0;
    double magnitude = exponent < 0.0 ? -exponent : exponent;
    int negative_base = __builtin_signbit(base);
    int integral_exp = __builtin_isinf(magnitude) ||
                       trunc(magnitude) == magnitude;
    int odd_exp = integral_exp && !__builtin_isinf(magnitude) &&
                  magnitude < 9007199254740992.0 &&
                  trunc(magnitude / 2.0) * 2.0 != magnitude;

    if (!integral_exp && base < 0.0) return NAN;
    if (__builtin_isinf(exponent)) {
        double abs_base = fabs(base);
        if (abs_base == 1.0) return 1.0;
        int grows = abs_base > 1.0;
        if (exponent < 0.0) grows = !grows;
        return grows ? INFINITY : 0.0;
    }
    if (__builtin_isinf(base)) {
        double result = neg_exp ? 0.0 : INFINITY;
        return (negative_base && odd_exp) ? -result : result;
    }
    if (base == 0.0) {
        double result = neg_exp ? INFINITY : 0.0;
        return (negative_base && odd_exp) ? -result : result;
    }

    if (!integral_exp) return exp(log(base) * exponent);

    /* Integer exponentiation without narrowing the exponent to int. */
    double factor = base;
    double result = 1.0;
    while (magnitude >= 1.0) {
        double half = trunc(magnitude / 2.0);
        if (magnitude - half * 2.0 >= 1.0) result *= factor;
        factor *= factor;
        magnitude = half;
    }
    return neg_exp ? 1.0 / result : result;
}

static inline float powf(float base, float exp) {
    return (float)pow((double)base, (double)exp);
}

/* floor - 切り捨て */
static inline double floor(double x) {
    return __builtin_floor(x);
}

static inline float floorf(float x) {
    return __builtin_floorf(x);
}

static inline long double floorl(long double x) {
    return __builtin_floorl(x);
}

/* ceil - 切り上げ */
static inline double ceil(double x) {
    return __builtin_ceil(x);
}

static inline float ceilf(float x) {
    return __builtin_ceilf(x);
}

static inline long double ceill(long double x) {
    return __builtin_ceill(x);
}

/* round - 四捨五入 */
static inline unsigned long long rin_math_double_fraction_mask(
    unsigned int fractional_bits) {
    static const unsigned long long masks[53] = {
        0x0000000000000000ULL, 0x0000000000000001ULL,
        0x0000000000000003ULL, 0x0000000000000007ULL,
        0x000000000000000FULL, 0x000000000000001FULL,
        0x000000000000003FULL, 0x000000000000007FULL,
        0x00000000000000FFULL, 0x00000000000001FFULL,
        0x00000000000003FFULL, 0x00000000000007FFULL,
        0x0000000000000FFFULL, 0x0000000000001FFFULL,
        0x0000000000003FFFULL, 0x0000000000007FFFULL,
        0x000000000000FFFFULL, 0x000000000001FFFFULL,
        0x000000000003FFFFULL, 0x000000000007FFFFULL,
        0x00000000000FFFFFULL, 0x00000000001FFFFFULL,
        0x00000000003FFFFFULL, 0x00000000007FFFFFULL,
        0x0000000000FFFFFFULL, 0x0000000001FFFFFFULL,
        0x0000000003FFFFFFULL, 0x0000000007FFFFFFULL,
        0x000000000FFFFFFFULL, 0x000000001FFFFFFFULL,
        0x000000003FFFFFFFULL, 0x000000007FFFFFFFULL,
        0x00000000FFFFFFFFULL, 0x00000001FFFFFFFFULL,
        0x00000003FFFFFFFFULL, 0x00000007FFFFFFFFULL,
        0x0000000FFFFFFFFFULL, 0x0000001FFFFFFFFFULL,
        0x0000003FFFFFFFFFULL, 0x0000007FFFFFFFFFULL,
        0x000000FFFFFFFFFFULL, 0x000001FFFFFFFFFFULL,
        0x000003FFFFFFFFFFULL, 0x000007FFFFFFFFFFULL,
        0x00000FFFFFFFFFFFULL, 0x00001FFFFFFFFFFFULL,
        0x00003FFFFFFFFFFFULL, 0x00007FFFFFFFFFFFULL,
        0x0000FFFFFFFFFFFFULL, 0x0001FFFFFFFFFFFFULL,
        0x0003FFFFFFFFFFFFULL, 0x0007FFFFFFFFFFFFULL,
        0x000FFFFFFFFFFFFFULL
    };
    return masks[fractional_bits];
}

static inline double round(double x) {
    union {
        double value;
        unsigned long long bits;
    } u;
    unsigned int exponent;
    unsigned int fractional_bits;

    u.value = x;
    exponent = (unsigned int)((u.bits >> 52) & 0x7FFULL);
    if (exponent >= 0x433U) {
        return x;
    }
    if (exponent < 0x3FEU) {
        u.bits &= 0x8000000000000000ULL;
        return u.bits != 0 ? -0.0 : 0.0;
    }
    if (exponent == 0x3FEU) {
        if (x >= 0.5) {
            return 1.0;
        }
        if (x <= -0.5) {
            return -1.0;
        }
        u.bits &= 0x8000000000000000ULL;
        return u.bits != 0 ? -0.0 : 0.0;
    }

    fractional_bits = 0x433U - exponent;
    u.bits &= ~rin_math_double_fraction_mask(fractional_bits);
    {
        double truncated = u.value;
        double difference = x - truncated;
        if (difference >= 0.5) {
            return truncated + 1.0;
        }
        if (difference <= -0.5) {
            return truncated - 1.0;
        }
        return truncated;
    }
}

static inline float roundf(float x) {
    union {
        float value;
        unsigned int bits;
    } u;
    unsigned int exponent;
    unsigned int fractional_bits;

    u.value = x;
    exponent = (u.bits >> 23) & 0xFFU;
    if (exponent >= 0x96U) {
        return x;
    }
    if (exponent < 0x7EU) {
        u.bits &= 1U << 31;
        return u.bits != 0 ? -0.0f : 0.0f;
    }
    if (exponent == 0x7EU) {
        if (x >= 0.5f) {
            return 1.0f;
        }
        if (x <= -0.5f) {
            return -1.0f;
        }
        u.bits &= 1U << 31;
        return u.bits != 0 ? -0.0f : 0.0f;
    }

    fractional_bits = 0x96U - exponent;
    u.bits &= ~((1U << fractional_bits) - 1U);
    {
        float truncated = u.value;
        float difference = x - truncated;
        if (difference >= 0.5f) {
            return truncated + 1.0f;
        }
        if (difference <= -0.5f) {
            return truncated - 1.0f;
        }
        return truncated;
    }
}

static inline long double roundl(long double x) {
    return __builtin_roundl(x);
}

static inline long __rin_math_lround_to_long(double rounded) {
    if (__builtin_isnan(rounded)) return 0L;
    if (__builtin_isinf(rounded))
        return __builtin_signbit(rounded) ? LONG_MIN : LONG_MAX;
#if __LONG_MAX__ > 2147483647L
    /* No binary64 value exists between the largest representable integer
     * below 2^63 and 2^63 itself, so the half-open check is exact. */
    if (rounded >= 0x1p63) return LONG_MAX;
    if (rounded < -0x1p63) return LONG_MIN;
#else
    if (rounded > (double)LONG_MAX) return LONG_MAX;
    if (rounded < (double)LONG_MIN) return LONG_MIN;
#endif
    return (long)rounded;
}

static inline long long __rin_math_lround_to_ll(double rounded) {
    if (__builtin_isnan(rounded)) return 0LL;
    if (__builtin_isinf(rounded))
        return __builtin_signbit(rounded) ? LLONG_MIN : LLONG_MAX;
    if (rounded >= 0x1p63) return LLONG_MAX;
    if (rounded < -0x1p63) return LLONG_MIN;
    return (long long)rounded;
}

static inline long lround(double x) {
    return __rin_math_lround_to_long(round(x));
}

static inline long lroundf(float x) {
    return __rin_math_lround_to_long((double)roundf(x));
}

static inline long long llround(double x) {
    return __rin_math_lround_to_ll(round(x));
}

static inline long long llroundf(float x) {
    return __rin_math_lround_to_ll((double)roundf(x));
}

/* trunc - 0方向への切り捨て */
static inline double trunc(double x) {
    union {
        double value;
        unsigned long long bits;
    } u;
    unsigned int exponent;
    unsigned int fractional_bits;

    u.value = x;
    exponent = (unsigned int)((u.bits >> 52) & 0x7FFULL);
    if (exponent >= 0x433U) {
        return x;
    }
    if (exponent < 0x3FFU) {
        u.bits &= 0x8000000000000000ULL;
        return u.value;
    }

    fractional_bits = 0x433U - exponent;
    u.bits &= ~rin_math_double_fraction_mask(fractional_bits);
    return u.value;
}

static inline float truncf(float x) {
    union {
        float value;
        unsigned int bits;
    } u;
    unsigned int exponent;
    unsigned int fractional_bits;

    u.value = x;
    exponent = (u.bits >> 23) & 0xFFU;
    if (exponent >= 0x96U) {
        return x;
    }
    if (exponent < 0x7FU) {
        u.bits &= 1U << 31;
        return u.value;
    }

    fractional_bits = 0x96U - exponent;
    u.bits &= ~((1U << fractional_bits) - 1U);
    return u.value;
}

static inline long double truncl(long double x) {
    return __builtin_truncl(x);
}

/* ═══════════════════════════════════════════════════════════════
 * 指数・対数関数
 * ═══════════════════════════════════════════════════════════════ */

/* exp - 指数関数 (Taylor展開) */
static inline double exp(double x) {
    if (__builtin_isnan(x)) return x;
    if (__builtin_isinf(x)) return x > 0.0 ? INFINITY : 0.0;
    /* Binary64 overflow/underflow boundaries. */
    if (x > 709.782712893384) return HUGE_VAL;
    if (x < -745.133219101941) return 0.0;

    /* Reduce to a small interval around zero, then rescale by 2^k. */
    int k = (int)(x / M_LN2 + (x >= 0.0 ? 0.5 : -0.5));
    double reduced = x - k * M_LN2;

    /* Taylor expansion on |reduced| <= ln(2)/2, where cancellation is
     * bounded and a fixed iteration count is sufficient for binary64. */
    double sum = 1.0;
    double term = 1.0;
    for (int i = 1; i <= 24; i++) {
        term *= reduced / i;
        sum += term;
        if (fabs(term) < 1e-15) break;
    }
    return ldexp(sum, k);
}

static inline float expf(float x) {
    return (float)exp((double)x);
}

/* log - 自然対数 (Newton法) */
static inline double log(double x) {
    if (__builtin_isnan(x)) return x;
    if (x == 0.0) return -HUGE_VAL;
    if (x < 0.0) return NAN;
    if (__builtin_isinf(x)) return x;
    if (x == 1.0) return 0.0;

    /* x = m * 2^e として分解 */
    double m = x;
    int e = 0;
    while (m >= 2.0) { m /= 2.0; e++; }
    while (m < 1.0) { m *= 2.0; e--; }

    /* atanh series converges quickly for z=(m-1)/(m+1) in [0,1/3). */
    double z = (m - 1.0) / (m + 1.0);
    double z2 = z * z;
    double term = z;
    double sum = 0.0;
    for (int i = 1; i <= 39; i += 2) {
        sum += term / i;
        term *= z2;
        if (fabs(term / (i + 2)) < 1e-17) break;
    }

    /* log(x) = log(m) + e * log(2) */
    return 2.0 * sum + e * 0.693147180559945309417;
}

static inline float logf(float x) {
    return (float)log((double)x);
}

/* log10 - 常用対数 */
static inline double log10(double x) {
    return log(x) / 2.302585092994045684018;  /* log(10) */
}

static inline float log10f(float x) {
    return (float)log10((double)x);
}

/* log2 - 2を底とする対数 */
static inline double log2(double x) {
    return log(x) / 0.693147180559945309417;  /* log(2) */
}

static inline float log2f(float x) {
    return (float)log2((double)x);
}

/* log1p - log(1 + x) より精度の高い実装 */
static inline double log1p(double x) {
    if (__builtin_isnan(x)) return x;
    if (x == -1.0) return -HUGE_VAL;
    if (x < -1.0) return NAN;
    if (__builtin_isinf(x)) return x;
    if (x == 0.0) return x;
    /* Below half an ulp at one, the quadratic correction cannot affect a
     * binary64 result; returning x also avoids underflow in x/(2+x) for
     * true-subnormal inputs. */
    if (fabs(x) < 1.1102230246251565e-16) return x;
    /* For small arguments, form y=x/(2+x) and sum the atanh series.
     * This avoids the 1+x rounding that made the old x-only shortcut lose
     * the quadratic term (and gives useful precision down to subnormals). */
    if (fabs(x) < 0.5) {
        double y = x / (2.0 + x);
        double y2 = y * y;
        double term = y;
        double sum = 0.0;
        for (int i = 1; i <= 39; i += 2) {
            sum += term / i;
            term *= y2;
            if (fabs(term / (i + 2)) < 1e-18) break;
        }
        return 2.0 * sum;
    }
    return log(1.0 + x);
}

static inline float log1pf(float x) {
    return (float)log1p((double)x);
}

/* expm1 - exp(x) - 1 より精度の高い実装 */
static inline double expm1(double x) {
    if (__builtin_isnan(x)) return x;
    if (x == 0.0) return x;
    /* Direct exp(x)-1 loses the x²/2 term when exp rounds to one.  The
     * convergent Taylor sum below is accurate on the small interval where
     * that cancellation matters, including signed subnormal inputs. */
    if (fabs(x) < 0.5) {
        double sum = x;
        double term = x;
        for (int i = 2; i <= 40; i++) {
            term *= x / i;
            sum += term;
            if (fabs(term) < 1e-18) break;
        }
        return sum;
    }
    return exp(x) - 1.0;
}

static inline float expm1f(float x) {
    return (float)expm1((double)x);
}

/* ═══════════════════════════════════════════════════════════════
 * 逆三角関数
 * ═══════════════════════════════════════════════════════════════ */

/* asin - 逆正弦 (Taylor展開) */
static inline double asin(double x) {
    if (__builtin_isnan(x)) return x;
    if (x < -1.0 || x > 1.0) return NAN;
    if (x == 1.0) return M_PI / 2.0;
    if (x == -1.0) return -M_PI / 2.0;

    /* Range reduction through atan keeps convergence uniform near |x|=1. */
    return atan(x / sqrt(1.0 - x * x));
}

static inline float asinf(float x) {
    return (float)asin((double)x);
}

/* acos - 逆余弦 */
static inline double acos(double x) {
    return M_PI / 2.0 - asin(x);
}

static inline float acosf(float x) {
    return (float)acos((double)x);
}

/* atan - 逆正接 (Taylor展開) */
static inline double atan(double x) {
    if (__builtin_isnan(x)) return x;
    if (__builtin_isinf(x)) return __builtin_copysign(M_PI / 2.0, x);
    if (x == 0.0) return x;
    int neg = __builtin_signbit(x);
    if (neg) x = -x;
    int invert = 0;
    if (x > 1.0) {
        invert = 1;
        x = 1.0 / x;
    }
    double offset = 0.0;
    if (x > 0.4142135623730950) {
        offset = M_PI / 4.0;
        x = (x - 1.0) / (x + 1.0);
    }

    /* Taylor展開: the reduction above keeps |x| <= tan(π/8). */
    double sum = 0.0;
    double term = x;
    double x2 = x * x;
    for (int i = 0; i <= 30; i++) {
        sum += term / (2*i + 1);
        term *= -x2;
        if (fabs(term / (2*i + 3)) < 1e-15) break;
    }

    sum += offset;
    if (invert) sum = M_PI / 2.0 - sum;
    if (neg) sum = -sum;
    return sum;
}

static inline float atanf(float x) {
    return (float)atan((double)x);
}

/* atan2 - 2引数逆正接 */
static inline double atan2(double y, double x) {
    if (__builtin_isnan(x) || __builtin_isnan(y)) return NAN;
    if (__builtin_isinf(y)) {
        if (__builtin_isinf(x))
            return __builtin_copysign(
                x < 0 ? 3.0 * M_PI / 4.0 : M_PI / 4.0, y);
        return __builtin_copysign(M_PI / 2.0, y);
    }
    if (__builtin_isinf(x))
        return x < 0 ? __builtin_copysign(M_PI, y)
                     : __builtin_copysign(0.0, y);
    if (y == 0.0) {
        if (x < 0.0) return __builtin_copysign(M_PI, y);
        return y;
    }
    if (x == 0.0) return __builtin_copysign(M_PI / 2.0, y);
    if (x > 0) return atan(y / x);
    if (x < 0 && y >= 0) return atan(y / x) + M_PI;
    if (x < 0 && y < 0) return atan(y / x) - M_PI;
    return NAN;
}

static inline float atan2f(float y, float x) {
    return (float)atan2((double)y, (double)x);
}

/* ═══════════════════════════════════════════════════════════════
 * 双曲線関数
 * ═══════════════════════════════════════════════════════════════ */

/* sinh - 双曲線正弦 */
static inline double sinh(double x) {
    if (__builtin_isnan(x) || __builtin_isinf(x)) return x;
    if (fabs(x) < 1e-10) return x;
    if (fabs(x) < 0.5) {
        /* e^x-e^-x = expm1(x)-expm1(-x), avoiding cancellation in the
         * direct reciprocal form around zero. */
        return (expm1(x) - expm1(-x)) * 0.5;
    }
    double ex = exp(x);
    return (ex - 1.0 / ex) / 2.0;
}

static inline float sinhf(float x) {
    return (float)sinh((double)x);
}

/* cosh - 双曲線余弦 */
static inline double cosh(double x) {
    if (__builtin_isnan(x)) return x;
    if (__builtin_isinf(x)) return INFINITY;
    if (fabs(x) < 0.5) {
        /* cosh(x) = 1 + (expm1(x)+expm1(-x))/2. */
        return 1.0 + (expm1(x) + expm1(-x)) * 0.5;
    }
    double ex = exp(x);
    return (ex + 1.0 / ex) / 2.0;
}

static inline float coshf(float x) {
    return (float)cosh((double)x);
}

/* tanh - 双曲線正接 */
static inline double tanh(double x) {
    if (__builtin_isnan(x)) return x;
    if (__builtin_isinf(x)) return x > 0 ? 1.0 : -1.0;
    if (fabs(x) < 1e-10) return x;
    if (fabs(x) < 0.5) {
        /* tanh(x) = expm1(2x)/(expm1(2x)+2), stable for small x. */
        double numerator = expm1(2.0 * x);
        return numerator / (numerator + 2.0);
    }
    if (x > 20.0) return 1.0;
    if (x < -20.0) return -1.0;
    double ex = exp(2.0 * x);
    return (ex - 1.0) / (ex + 1.0);
}

static inline float tanhf(float x) {
    return (float)tanh((double)x);
}

/* asinh - 逆双曲線正弦 */
static inline double asinh(double x) {
    if (__builtin_isnan(x) || __builtin_isinf(x)) return x;
    if (fabs(x) < 1e-10) return x;
    int negative = __builtin_signbit(x);
    double magnitude = fabs(x);
    double result;
    if (magnitude > 1e10) {
        result = log(magnitude) + M_LN2;
    } else {
        result = log(magnitude + sqrt(magnitude * magnitude + 1.0));
    }
    return negative ? -result : result;
}

static inline float asinhf(float x) {
    return (float)asinh((double)x);
}

/* acosh - 逆双曲線余弦 */
static inline double acosh(double x) {
    if (__builtin_isnan(x)) return x;
    if (__builtin_isinf(x)) return x;
    if (x < 1.0) return NAN;
    if (x == 1.0) return 0.0;
    if (x > 1e10) return log(x) + M_LN2;
    return log(x + sqrt((x - 1.0) * (x + 1.0)));
}

static inline float acoshf(float x) {
    return (float)acosh((double)x);
}

/* atanh - 逆双曲線正接 */
static inline double atanh(double x) {
    if (__builtin_isnan(x)) return x;
    if (x == 1.0) return INFINITY;
    if (x == -1.0) return -INFINITY;
    if (x < -1.0 || x > 1.0) return NAN;
    if (fabs(x) < 1e-10) return x;
    if (fabs(x) < 0.5) {
        /* atanh(x) = (log1p(x)-log1p(-x))/2; keeping the two log1p terms
         * separate avoids rounding 1+x and 1-x before taking a logarithm. */
        return 0.5 * (log1p(x) - log1p(-x));
    }
    return 0.5 * log((1.0 + x) / (1.0 - x));
}

static inline float atanhf(float x) {
    return (float)atanh((double)x);
}

/* ═══════════════════════════════════════════════════════════════
 * その他の数学関数
 * ═══════════════════════════════════════════════════════════════ */

/* cbrt - 立方根 (Newton法) */
static inline double cbrt(double x) {
    if (__builtin_isnan(x) || __builtin_isinf(x) || x == 0.0) return x;
    int neg = __builtin_signbit(x);
    double magnitude = fabs(x);
    int exponent = 0;
    double mantissa = frexp(magnitude, &exponent);
    int remainder = exponent % 3;
    if (remainder < 0) remainder += 3;
    if (remainder == 1) {
        mantissa *= 2.0;
        --exponent;
    } else if (remainder == 2) {
        mantissa *= 4.0;
        exponent -= 2;
    }

    /* The normalized mantissa is in [0.5, 4), avoiding overflow in the
     * Newton quotient while making convergence independent of input scale. */
    double guess = 1.0;
    for (int i = 0; i < 12; i++) {
        guess = (2.0 * guess + mantissa / (guess * guess)) / 3.0;
    }
    double result = ldexp(guess, exponent / 3);
    return neg ? -result : result;
}

static inline float cbrtf(float x) {
    return (float)cbrt((double)x);
}

/* hypot - ユークリッド距離 */
static inline double hypot(double x, double y) {
    /* IEEE/C: infinity dominates NaN, while finite operands are scaled
     * before squaring so a large finite component does not overflow early. */
    if (__builtin_isinf(x) || __builtin_isinf(y)) return INFINITY;
    if (__builtin_isnan(x) || __builtin_isnan(y)) return NAN;

    double ax = fabs(x);
    double ay = fabs(y);
    if (ax < ay) {
        double swap = ax;
        ax = ay;
        ay = swap;
    }
    if (ax == 0.0) return 0.0;
    double ratio = ay / ax;
    return ax * sqrt(1.0 + ratio * ratio);
}

static inline float hypotf(float x, float y) {
    if (__builtin_isinf(x) || __builtin_isinf(y)) return __builtin_inff();
    if (__builtin_isnan(x) || __builtin_isnan(y)) return __builtin_nanf("");

    float ax = fabsf(x);
    float ay = fabsf(y);
    if (ax < ay) {
        float swap = ax;
        ax = ay;
        ay = swap;
    }
    if (ax == 0.0f) return 0.0f;
    float ratio = ay / ax;
    return ax * sqrtf(1.0f + ratio * ratio);
}

/* modf - 整数部と小数部に分離 */
static inline double modf(double x, double* iptr) {
    double i = trunc(x);
    if (iptr) *iptr = i;
    if (__builtin_isnan(x)) return x;
    if (__builtin_isinf(x) || x == 0.0)
        return __builtin_copysign(0.0, x);
    if (x == i) return __builtin_copysign(0.0, x);
    return x - i;
}

static inline float modff(float x, float* iptr) {
    float i = truncf(x);
    if (iptr) *iptr = i;
    if (__builtin_isnan(x)) return x;
    if (__builtin_isinf(x) || x == 0.0f)
        return __builtin_copysignf(0.0f, x);
    if (x == i) return __builtin_copysignf(0.0f, x);
    return x - i;
}

/* copysign - 符号をコピー */
static inline double copysign(double x, double y) {
    union { double value; unsigned long long bits; } magnitude, sign;
    magnitude.value = x;
    sign.value = y;
    magnitude.bits = (magnitude.bits & ~(1ULL << 63)) |
                     (sign.bits & (1ULL << 63));
    return magnitude.value;
}

static inline float copysignf(float x, float y) {
    union { float value; unsigned int bits; } magnitude, sign;
    magnitude.value = x;
    sign.value = y;
    magnitude.bits = (magnitude.bits & ~(1U << 31)) |
                     (sign.bits & (1U << 31));
    return magnitude.value;
}

/* frexp - 仮数と指数に分解 */
static inline double frexp(double x, int* exp) {
    if (__builtin_isnan(x) || __builtin_isinf(x)) {
        if (exp) *exp = 0;
        return x;
    }
    if (x == 0.0) { if (exp) *exp = 0; return x; }

    int e = 0;
    double m = fabs(x);
    while (m >= 1.0) { m /= 2.0; e++; }
    while (m < 0.5) { m *= 2.0; e--; }

    if (exp) *exp = e;
    return x < 0 ? -m : m;
}

static inline float frexpf(float x, int* exp) {
    return (float)frexp((double)x, exp);
}

/* ilogbf - unbiased binary exponent for float values.  Keep this local to
 * the freestanding libc so callers such as CoreCLR do not acquire a host
 * libm dependency. */
static inline int ilogbf(float x) {
    union { float value; unsigned int bits; } u;
    u.value = x;
    unsigned int exponent = (u.bits >> 23) & 0xFFU;
    unsigned int fraction = u.bits & 0x7FFFFFU;

    if (exponent == 0) {
        if (fraction == 0) return INT_MIN;

        int highest_bit = 0;
        while ((fraction >>= 1) != 0) highest_bit++;
        return highest_bit - 149;
    }

    if (exponent == 0xFFU) return INT_MAX;
    return (int)exponent - 127;
}

/* ldexp - 仮数と指数から合成 */
static inline double ldexp(double x, int exp) {
    if (x == 0.0 || __builtin_isnan(x) || __builtin_isinf(x)) return x;
    /* A binary64 value is fully scaled after 2,048 binary steps.  Clamp
     * untrusted public exponents so INT_MIN/INT_MAX cannot turn this helper
     * into a multi-billion-iteration denial of service. */
    if (exp > 2048) exp = 2048;
    else if (exp < -2048) exp = -2048;
    while (exp > 0) { x *= 2.0; exp--; }
    while (exp < 0) { x /= 2.0; exp++; }
    return x;
}

static inline float ldexpf(float x, int exp) {
    return (float)ldexp((double)x, exp);
}

/* scalbn - 2のべき乗をかける */
static inline double scalbn(double x, int n) {
    return ldexp(x, n);
}

static inline float scalbnf(float x, int n) {
    return ldexpf(x, n);
}

/* rint - 最近接整数への丸め（現在のモードに従う） */
static inline double __rin_math_round_even(double x) {
    if (__builtin_isnan(x) || __builtin_isinf(x) || x == 0.0) return x;
    double truncated = trunc(x);
    double fraction = x - truncated;
    double magnitude = fabs(fraction);
    if (magnitude < 0.5) return truncated;
    if (magnitude > 0.5)
        return truncated + __builtin_copysign(1.0, x);
    /* A half-way value chooses the even integer.  Values with no
     * fractional bits have already returned above, so this remains bounded
     * and does not narrow the integer part. */
    if (fmod(fabs(truncated), 2.0) == 0.0) return truncated;
    return truncated + __builtin_copysign(1.0, x);
}

static inline double __rin_math_round_mode(double x) {
    double truncated;
    switch (fegetround()) {
    case FE_DOWNWARD:
        truncated = trunc(x);
        return x < truncated ? truncated - 1.0 : truncated;
    case FE_UPWARD:
        truncated = trunc(x);
        return x > truncated ? truncated + 1.0 : truncated;
    case FE_TOWARDZERO:
        return trunc(x);
    case FE_TONEAREST:
    default:
        return __rin_math_round_even(x);
    }
}

static inline double rint(double x) {
    return __rin_math_round_mode(x);
}

static inline float rintf(float x) {
    return (float)__rin_math_round_mode((double)x);
}

static inline long double rintl(long double x) {
    return __builtin_rintl(x);
}

/* lrint - 四捨五入して整数へ */
static inline long lrint(double x) {
    return __rin_math_lround_to_long(__rin_math_round_mode(x));
}

static inline long lrintf(float x) {
    return __rin_math_lround_to_long(__rin_math_round_mode((double)x));
}

/* llrint - 四捨五入してlong longへ */
static inline long long llrint(double x) {
    return __rin_math_lround_to_ll(__rin_math_round_mode(x));
}

/* nearbyint - 現在の丸めモードで整数へ（FE例外なし） */
static inline double nearbyint(double x) {
    return rint(x);
}

static inline float nearbyintf(float x) {
    return rintf(x);
}

static inline long double nearbyintl(long double x) {
    return rintl(x);
}

/* remainder - IEEE剰余 */
static inline double remainder(double x, double y) {
    double rem = fmod(x, y);
    if (__builtin_isnan(rem) || rem == 0.0 || __builtin_isinf(y)) return rem;

    double half = 0.5 * fabs(y);
    double magnitude = fabs(rem);
    int adjust = magnitude > half;
    if (magnitude == half) {
        /* IEEE remainder chooses the even nearest quotient on a tie. */
        double quotient = x / y;
        if (__builtin_isfinite(quotient)) {
            double absolute = fabs(quotient);
            double lower = trunc(absolute);
            double fraction = absolute - lower;
            if (fraction == 0.5)
                adjust = trunc(lower / 2.0) * 2.0 != lower;
        }
    }
    if (!adjust) return rem;
    return __builtin_signbit(x) ? rem + fabs(y) : rem - fabs(y);
}

static inline float remainderf(float x, float y) {
    return (float)remainder((double)x, (double)y);
}

/* fmin/fmax */
static inline int __rin_math_isnan_d(double x) {
    union { double value; unsigned long long bits; } encoded;
    encoded.value = x;
    return ((encoded.bits >> 52) & 0x7FFu) == 0x7FFu &&
           (encoded.bits & 0xFFFFFFFFFFFFFULL) != 0;
}

static inline int __rin_math_isnan_f(float x) {
    union { float value; unsigned int bits; } encoded;
    encoded.value = x;
    return ((encoded.bits >> 23) & 0xFFu) == 0xFFu &&
           (encoded.bits & 0x7FFFFFu) != 0;
}

static inline int __rin_math_signbit_d(double x) {
    union { double value; unsigned long long bits; } encoded;
    encoded.value = x;
    return (int)(encoded.bits >> 63);
}

static inline int __rin_math_signbit_f(float x) {
    union { float value; unsigned int bits; } encoded;
    encoded.value = x;
    return (int)(encoded.bits >> 31);
}

static inline double fmin(double x, double y) {
    if (__rin_math_isnan_d(x)) return y;
    if (__rin_math_isnan_d(y)) return x;
    if (x == y) {
        if (x == 0.0)
            return (__rin_math_signbit_d(x) || __rin_math_signbit_d(y))
                ? -0.0 : 0.0;
        return x;
    }
    return x < y ? x : y;
}

static inline double fmax(double x, double y) {
    if (__rin_math_isnan_d(x)) return y;
    if (__rin_math_isnan_d(y)) return x;
    if (x == y) {
        if (x == 0.0)
            return (__rin_math_signbit_d(x) && __rin_math_signbit_d(y))
                ? -0.0 : 0.0;
        return x;
    }
    return x > y ? x : y;
}

static inline float fminf(float x, float y) {
    if (__rin_math_isnan_f(x)) return y;
    if (__rin_math_isnan_f(y)) return x;
    if (x == y) {
        if (x == 0.0f)
            return (__rin_math_signbit_f(x) || __rin_math_signbit_f(y))
                ? -0.0f : 0.0f;
        return x;
    }
    return x < y ? x : y;
}

static inline float fmaxf(float x, float y) {
    if (__rin_math_isnan_f(x)) return y;
    if (__rin_math_isnan_f(y)) return x;
    if (x == y) {
        if (x == 0.0f)
            return (__rin_math_signbit_f(x) && __rin_math_signbit_f(y))
                ? -0.0f : 0.0f;
        return x;
    }
    return x > y ? x : y;
}

/* fdim - 正の差 */
static inline double fdim(double x, double y) {
    if (__builtin_isnan(x) || __builtin_isnan(y)) return NAN;
    return x > y ? x - y : 0.0;
}

static inline float fdimf(float x, float y) {
    if (__builtin_isnan(x) || __builtin_isnan(y)) return __builtin_nanf("");
    return x > y ? x - y : 0.0f;
}

/* fma - 融合積和演算 */
static inline double __rin_fma_finite(double x, double y, double z) {
    /* Dekker product splitting recovers the rounded-away product tail for
     * ordinary finite operands without requiring an external libm symbol. */
    double product = x * y;
    if (!__builtin_isfinite(product) || fabs(x) > 1e150 || fabs(y) > 1e150)
        return product + z;
    const double splitter = 134217729.0; /* 2^27 + 1 */
    double cx = splitter * x;
    double cy = splitter * y;
    double hx = cx - (cx - x);
    double hy = cy - (cy - y);
    double tx = x - hx;
    double ty = y - hy;
    double product_error = ((hx * hy - product) + hx * ty + tx * hy) + tx * ty;
    double sum = product + z;
    double sum_error = (product - sum) + z;
    return sum + (product_error + sum_error);
}

static inline double fma(double x, double y, double z) {
    return __rin_fma_finite(x, y, z);
}

static inline float fmaf(float x, float y, float z) {
    return (float)__rin_fma_finite((double)x, (double)y, (double)z);
}

/* exp2 - 2^x */
static inline double exp2(double x) {
    return pow(2.0, x);
}

static inline float exp2f(float x) {
    return powf(2.0f, x);
}

/* 浮動小数点分類関数はマクロとして定義済み (isnan, isinf, isfinite等) */

/* ═══════════════════════════════════════════════════════════════
 * 文字列から浮動小数点への変換
 * ═══════════════════════════════════════════════════════════════ */

/* Preserve this header's C-compatible atof extension when freestanding
 * <cstdlib> has placed the implementation in namespace std. */
#if !defined(_ATOF_DEFINED) && defined(__cplusplus) && \
    defined(RINCXX_CSTDLIB_H) && !defined(RINCXX_CSTDLIB_HOSTED_C)
#define _ATOF_DEFINED
static inline double atof(const char* s) {
    return std::atof(s);
}
#elif !defined(_ATOF_DEFINED) && \
      !(defined(__cplusplus) && \
        (defined(RINCXX_CSTDLIB_H) || defined(_INC_STDLIB)))
#define _ATOF_DEFINED
static inline double atof(const char* s) {
    double result = 0.0;
    double frac = 0.0;
    double frac_div = 1.0;
    int neg = 0;
    int in_frac = 0;
    int exp = 0;
    int exp_neg = 0;

    /* 空白スキップ */
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;

    /* 符号 */
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;

    /* 整数部と小数部 */
    while (*s) {
        if (*s >= '0' && *s <= '9') {
            if (in_frac) {
                frac = frac * 10.0 + (*s - '0');
                frac_div *= 10.0;
            } else {
                result = result * 10.0 + (*s - '0');
            }
        } else if (*s == '.' && !in_frac) {
            in_frac = 1;
        } else if (*s == 'e' || *s == 'E') {
            s++;
            if (*s == '-') { exp_neg = 1; s++; }
            else if (*s == '+') s++;
            while (*s >= '0' && *s <= '9') {
                exp = exp * 10 + (*s - '0');
                s++;
            }
            break;
        } else {
            break;
        }
        s++;
    }

    result += frac / frac_div;
    if (neg) result = -result;

    /* 指数部適用 */
    if (exp > 0) {
        double mult = 1.0;
        while (exp-- > 0) mult *= 10.0;
        if (exp_neg) result /= mult;
        else result *= mult;
    }

    return result;
}
#endif /* _ATOF_DEFINED */

#ifdef __cplusplus
}
#endif
#endif /* !MIDL_PASS */

#if defined(__cplusplus) && !defined(TARGET_RINOS) && \
    !defined(RIN_TARGET_RINOS)

static inline int isnan(float x) {
    union { float f; unsigned int i; } u;
    u.f = x;
    unsigned int exp = (u.i >> 23) & 0xFF;
    unsigned int frac = u.i & 0x7FFFFF;
    return (exp == 0xFF) && (frac != 0);
}

static inline int isnan(double x) {
    union { double d; unsigned long long i; } u;
    u.d = x;
    unsigned long long exp = (u.i >> 52) & 0x7FF;
    unsigned long long frac = u.i & 0xFFFFFFFFFFFFFULL;
    return (exp == 0x7FF) && (frac != 0);
}

static inline int isnan(long double x) {
    return isnan((double)x);
}

static inline int isinf(float x) {
    union { float f; unsigned int i; } u;
    u.f = x;
    unsigned int exp = (u.i >> 23) & 0xFF;
    unsigned int frac = u.i & 0x7FFFFF;
    return (exp == 0xFF) && (frac == 0);
}

static inline int isinf(double x) {
    union { double d; unsigned long long i; } u;
    u.d = x;
    unsigned long long exp = (u.i >> 52) & 0x7FF;
    unsigned long long frac = u.i & 0xFFFFFFFFFFFFFULL;
    return (exp == 0x7FF) && (frac == 0);
}

static inline int isinf(long double x) {
    return isinf((double)x);
}

static inline int isfinite(float x) {
    union { float f; unsigned int i; } u;
    u.f = x;
    return ((u.i >> 23) & 0xFF) != 0xFF;
}

static inline int isfinite(double x) {
    union { double d; unsigned long long i; } u;
    u.d = x;
    return ((u.i >> 52) & 0x7FF) != 0x7FF;
}

static inline int isfinite(long double x) {
    return isfinite((double)x);
}
#endif

#endif /* _MATH_H */
