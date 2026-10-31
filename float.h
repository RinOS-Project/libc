/*
 * RinOS libc - float.h
 * 浮動小数点制限定数
 */

#ifndef _FLOAT_H
#define _FLOAT_H
#define __STDC_VERSION_FLOAT_H__ 202311L


#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * 丸めモード
 * ═══════════════════════════════════════════════════════════════*/

#define FLT_ROUNDS 1  /* 最近接偶数丸め */

/* 浮動小数点式の評価方法 */
#define FLT_EVAL_METHOD 0  /* 型通りに評価 */

/* ═══════════════════════════════════════════════════════════════
 * 基数と精度
 * ═══════════════════════════════════════════════════════════════*/

#define FLT_RADIX 2  /* 基数 */

/* RinOS keeps long double distinct in the C type system and rank, while its
 * x86 ABI uses the same binary64 representation and calling convention as
 * double. */

/* 10進精度 */
#define FLT_DIG        6
#define DBL_DIG        15
#define LDBL_DIG       DBL_DIG

/* 2進精度 (仮数部ビット数) */
#define FLT_MANT_DIG   24
#define DBL_MANT_DIG   53
#define LDBL_MANT_DIG  DBL_MANT_DIG

/* ε (1.0と次の表現可能な値の差) */
#define FLT_EPSILON    1.19209290e-07F
#define DBL_EPSILON    2.2204460492503131e-16
#define LDBL_EPSILON   2.2204460492503131e-16L

/* ═══════════════════════════════════════════════════════════════
 * 指数範囲
 * ═══════════════════════════════════════════════════════════════*/

/* 最小指数 (2進) */
#define FLT_MIN_EXP    (-125)
#define DBL_MIN_EXP    (-1021)
#define LDBL_MIN_EXP   DBL_MIN_EXP

/* 最大指数 (2進) */
#define FLT_MAX_EXP    128
#define DBL_MAX_EXP    1024
#define LDBL_MAX_EXP   DBL_MAX_EXP

/* 最小指数 (10進) */
#define FLT_MIN_10_EXP  (-37)
#define DBL_MIN_10_EXP  (-307)
#define LDBL_MIN_10_EXP DBL_MIN_10_EXP

/* 最大指数 (10進) */
#define FLT_MAX_10_EXP  38
#define DBL_MAX_10_EXP  308
#define LDBL_MAX_10_EXP DBL_MAX_10_EXP

/* ═══════════════════════════════════════════════════════════════
 * 最小・最大値
 * ═══════════════════════════════════════════════════════════════*/

/* 最小正規化正数 */
#define FLT_MIN        1.17549435e-38F
#define DBL_MIN        2.2250738585072014e-308
#define LDBL_MIN       2.2250738585072014e-308L

/* 最大有限値 */
#define FLT_MAX        3.40282347e+38F
#define DBL_MAX        1.7976931348623157e+308
#define LDBL_MAX       1.7976931348623157e+308L

/* ═══════════════════════════════════════════════════════════════
 * C11追加定数
 * ═══════════════════════════════════════════════════════════════*/

/* 有効10進桁数 (正確な往復変換) */
#define FLT_DECIMAL_DIG  9
#define DBL_DECIMAL_DIG  17
#define LDBL_DECIMAL_DIG DBL_DECIMAL_DIG
#define DECIMAL_DIG      LDBL_DECIMAL_DIG

/* 最小非正規化正数 */
#define FLT_TRUE_MIN   1.40129846e-45F
#define DBL_TRUE_MIN   4.9406564584124654e-324
#define LDBL_TRUE_MIN  4.9406564584124654e-324L

/* 非正規化数サポート */
#define FLT_HAS_SUBNORM  1
#define DBL_HAS_SUBNORM  1
#define LDBL_HAS_SUBNORM 1

/* C23 normalized maxima: RinOS models IEEE-754 binary32/64, where the
 * normalized maximum equals the maximum. */
#ifndef FLT_NORM_MAX
#define FLT_NORM_MAX FLT_MAX
#endif
#ifndef DBL_NORM_MAX
#define DBL_NORM_MAX DBL_MAX
#endif
#ifndef LDBL_NORM_MAX
#define LDBL_NORM_MAX LDBL_MAX
#endif
/* C23 IEC-60559 format queries: prefer the compiler's answer, else claim
 * the IEEE binary model this header already hardcodes. */
#ifndef FLT_IS_IEC_60559
#ifdef __FLT_IS_IEC_60559__
#define FLT_IS_IEC_60559 __FLT_IS_IEC_60559__
#else
#define FLT_IS_IEC_60559 1
#endif
#endif
#ifndef DBL_IS_IEC_60559
#ifdef __DBL_IS_IEC_60559__
#define DBL_IS_IEC_60559 __DBL_IS_IEC_60559__
#else
#define DBL_IS_IEC_60559 1
#endif
#endif
#ifndef LDBL_IS_IEC_60559
#ifdef __LDBL_IS_IEC_60559__
#define LDBL_IS_IEC_60559 __LDBL_IS_IEC_60559__
#else
#define LDBL_IS_IEC_60559 1
#endif
#endif
/* C23 also specifies INFINITY/NAN in <float.h> (shared with <math.h>). */
#ifndef INFINITY
#if defined(__FLT_HAS_INFINITY__) && !__FLT_HAS_INFINITY__
/* Target without infinity: INFINITY stays undefined. */
#elif defined(__GNUC__) || defined(__clang__)
#define INFINITY (__builtin_inff())
#else
#define INFINITY ((float)(1e+300 * 1e+300))
#endif
#endif
#ifndef NAN
#if defined(__FLT_HAS_QUIET_NAN__) && !__FLT_HAS_QUIET_NAN__
/* Target without quiet NaN: NAN stays undefined. */
#elif defined(__GNUC__) || defined(__clang__)
#define NAN (__builtin_nanf(""))
#else
#define NAN ((float)(INFINITY - INFINITY))
#endif
#endif
#if defined(__GNUC__) || defined(__clang__)
/* C23 signaling NaNs ("if exists": GNU-compatible compilers provide them). */
#ifndef FLT_SNAN
#if !defined(__FLT_HAS_QUIET_NAN__) || __FLT_HAS_QUIET_NAN__
#define FLT_SNAN (__builtin_nansf(""))
#endif
#endif
#ifndef DBL_SNAN
#if !defined(__DBL_HAS_QUIET_NAN__) || __DBL_HAS_QUIET_NAN__
#define DBL_SNAN (__builtin_nans(""))
#endif
#endif
#ifndef LDBL_SNAN
#if !defined(__LDBL_HAS_QUIET_NAN__) || __LDBL_HAS_QUIET_NAN__
#define LDBL_SNAN (__builtin_nansl(""))
#endif
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* _FLOAT_H */
