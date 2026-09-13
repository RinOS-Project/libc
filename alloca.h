/*
 * RinOS libc - alloca.h
 * スタック上の動的メモリ割り当て
 */

#ifndef _ALLOCA_H
#define _ALLOCA_H

#include "stddef.h"

/* GCC/Clang built-in alloca */
#define alloca(size) __builtin_alloca(size)

#endif /* _ALLOCA_H */
