/*
 * RinOS libc - float.h
 * 浮動小数点制限定数
 */

#ifndef _FLOAT_H
#define _FLOAT_H

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

/* 10進精度 */
#define FLT_DIG        6
#define DBL_DIG        15
#define LDBL_DIG       18

/* 2進精度 (仮数部ビット数) */
#define FLT_MANT_DIG   24
#define DBL_MANT_DIG   53
#define LDBL_MANT_DIG  64

/* ε (1.0と次の表現可能な値の差) */
#define FLT_EPSILON    1.19209290e-07F
#define DBL_EPSILON    2.2204460492503131e-16
#define LDBL_EPSILON   1.08420217248550443401e-19L

/* ═══════════════════════════════════════════════════════════════
 * 指数範囲
 * ═══════════════════════════════════════════════════════════════*/

/* 最小指数 (2進) */
#define FLT_MIN_EXP    (-125)
#define DBL_MIN_EXP    (-1021)
#define LDBL_MIN_EXP   (-16381)

/* 最大指数 (2進) */
#define FLT_MAX_EXP    128
#define DBL_MAX_EXP    1024
#define LDBL_MAX_EXP   16384

/* 最小指数 (10進) */
#define FLT_MIN_10_EXP  (-37)
#define DBL_MIN_10_EXP  (-307)
#define LDBL_MIN_10_EXP (-4931)

/* 最大指数 (10進) */
#define FLT_MAX_10_EXP  38
#define DBL_MAX_10_EXP  308
#define LDBL_MAX_10_EXP 4932

/* ═══════════════════════════════════════════════════════════════
 * 最小・最大値
 * ═══════════════════════════════════════════════════════════════*/

/* 最小正規化正数 */
#define FLT_MIN        1.17549435e-38F
#define DBL_MIN        2.2250738585072014e-308
#define LDBL_MIN       3.36210314311209350626e-4932L

/* 最大有限値 */
#define FLT_MAX        3.40282347e+38F
#define DBL_MAX        1.7976931348623157e+308
#define LDBL_MAX       1.18973149535723176502e+4932L

/* ═══════════════════════════════════════════════════════════════
 * C11追加定数
 * ═══════════════════════════════════════════════════════════════*/

/* 有効10進桁数 (正確な往復変換) */
#define FLT_DECIMAL_DIG  9
#define DBL_DECIMAL_DIG  17
#define LDBL_DECIMAL_DIG 21
#define DECIMAL_DIG      LDBL_DECIMAL_DIG

/* 最小非正規化正数 */
#define FLT_TRUE_MIN   1.40129846e-45F
#define DBL_TRUE_MIN   4.9406564584124654e-324
#define LDBL_TRUE_MIN  3.64519953188247460253e-4951L

/* 非正規化数サポート */
#define FLT_HAS_SUBNORM  1
#define DBL_HAS_SUBNORM  1
#define LDBL_HAS_SUBNORM 1

#ifdef __cplusplus
}
#endif

#endif /* _FLOAT_H */
