/*
 * RinOS libc - stdbool.h
 * ブール型
 */

#ifndef _STDBOOL_H
#define _STDBOOL_H

#ifdef __cplusplus
/* C++ではboolは組み込み型 */
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ > 201710L && \
    ((defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 13) || \
     (defined(__clang__) && __clang_major__ >= 16) || \
     (!defined(__GNUC__) && !defined(__clang__) && !defined(_MSC_VER)))
/* C23ではbool/true/falseはキーワード。互換マクロのみ定義する */
#ifndef __bool_true_false_are_defined
#define __bool_true_false_are_defined 1
#endif
#else
/* Cでは_Boolを使用 */
#ifndef __bool_true_false_are_defined
typedef unsigned char bool;
#define true  1
#define false 0
#define __bool_true_false_are_defined 1
#endif
#endif

#endif /* _STDBOOL_H */
