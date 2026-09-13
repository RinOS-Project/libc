/*
 * RinOS libc - stdbool.h
 * ブール型
 */

#ifndef _STDBOOL_H
#define _STDBOOL_H

#ifdef __cplusplus
/* C++ではboolは組み込み型 */
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
