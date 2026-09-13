/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - stdint.h
 * 固定幅整数型
 */

#ifndef _STDINT_H
#define _STDINT_H

#if !defined(__CLANG_STDINT_H)

/* MSVC has no GNU/Clang fixed-width type macros. */
#if defined(_MSC_VER)
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned __int64   uint64_t;
typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef __int64            int64_t;
#else
/* 符号なし整数型 */
typedef __UINT8_TYPE__  uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
/* Use the compiler's fixed-width type even for freestanding builds.  This
 * keeps this public header compatible when a kernel translation unit has
 * already included the toolchain's stdint.h. */
typedef __UINT64_TYPE__ uint64_t;

/* 符号付き整数型 */
typedef __INT8_TYPE__  int8_t;
typedef __INT16_TYPE__ int16_t;
typedef __INT32_TYPE__ int32_t;
typedef __INT64_TYPE__ int64_t;
#endif

/* 最小幅整数型 */
typedef int8_t   int_least8_t;
typedef int16_t  int_least16_t;
typedef int32_t  int_least32_t;
typedef int64_t  int_least64_t;
typedef uint8_t  uint_least8_t;
typedef uint16_t uint_least16_t;
typedef uint32_t uint_least32_t;
typedef uint64_t uint_least64_t;

/* 最速整数型 */
/* Match the active compiler ABI rather than assuming that a 32-bit integer is
 * the fastest type for every minimum width. */
/* MSVC's LLP64 ABI maps each fast alias to the corresponding exact type. */
#if defined(_MSC_VER)
typedef int8_t           int_fast8_t;
typedef int16_t          int_fast16_t;
typedef int32_t          int_fast32_t;
typedef int64_t          int_fast64_t;
typedef uint8_t          uint_fast8_t;
typedef uint16_t         uint_fast16_t;
typedef uint32_t         uint_fast32_t;
typedef uint64_t         uint_fast64_t;
#else
typedef __INT_FAST8_TYPE__   int_fast8_t;
typedef __INT_FAST16_TYPE__  int_fast16_t;
typedef __INT_FAST32_TYPE__  int_fast32_t;
typedef __INT_FAST64_TYPE__  int_fast64_t;
typedef __UINT_FAST8_TYPE__  uint_fast8_t;
typedef __UINT_FAST16_TYPE__ uint_fast16_t;
typedef __UINT_FAST32_TYPE__ uint_fast32_t;
typedef __UINT_FAST64_TYPE__ uint_fast64_t;
#endif

/* Use compiler ABI types rather than inferring LP64 from the CPU macro.
 * This is also correct for LLP64 build hosts where pointers are 64-bit but
 * long remains 32-bit. */
/* Use the actual LLP64 target width rather than a host long. */
#if defined(_MSC_VER)
#if defined(_WIN64)
typedef __int64          intptr_t;
typedef unsigned __int64 uintptr_t;
#else
typedef int              intptr_t;
typedef unsigned int     uintptr_t;
#endif
#else
typedef __INTPTR_TYPE__  intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;
#endif
#ifndef SIZE_MAX
#if defined(_MSC_VER)
#if defined(_WIN64)
#define SIZE_MAX    UINT64_MAX
#else
#define SIZE_MAX    UINT32_MAX
#endif
#else
#define SIZE_MAX    __SIZE_MAX__
#endif
#endif
#ifndef INTPTR_MIN
#if defined(_MSC_VER)
#if defined(_WIN64)
#define INTPTR_MIN  INT64_MIN
#define INTPTR_MAX  INT64_MAX
#define UINTPTR_MAX UINT64_MAX
#define PTRDIFF_MIN INT64_MIN
#define PTRDIFF_MAX INT64_MAX
#else
#define INTPTR_MIN  INT32_MIN
#define INTPTR_MAX  INT32_MAX
#define UINTPTR_MAX UINT32_MAX
#define PTRDIFF_MIN INT32_MIN
#define PTRDIFF_MAX INT32_MAX
#endif
#else
#define INTPTR_MIN  (-__INTPTR_MAX__ - 1)
#define INTPTR_MAX  __INTPTR_MAX__
#define UINTPTR_MAX __UINTPTR_MAX__
#define PTRDIFF_MIN (-__PTRDIFF_MAX__ - 1)
#define PTRDIFF_MAX __PTRDIFF_MAX__
#endif
#endif

/* 最大幅整数型 */
typedef int64_t  intmax_t;
typedef uint64_t uintmax_t;

/* 定数マクロ */
#ifndef INT8_MIN
#define INT8_MIN   (-128)
#endif
#ifndef INT8_MAX
#define INT8_MAX   127
#endif
#ifndef UINT8_MAX
#define UINT8_MAX  255
#endif

#ifndef INT16_MIN
#define INT16_MIN  (-32768)
#endif
#ifndef INT16_MAX
#define INT16_MAX  32767
#endif
#ifndef UINT16_MAX
#define UINT16_MAX 65535
#endif

#ifndef INT32_MIN
#define INT32_MIN  (-2147483647-1)
#endif
#ifndef INT32_MAX
#define INT32_MAX  2147483647
#endif
#ifndef UINT32_MAX
#define UINT32_MAX 4294967295U
#endif

#ifndef INT64_MIN
#define INT64_MIN  (-9223372036854775807LL-1)
#endif
#ifndef INT64_MAX
#define INT64_MAX  9223372036854775807LL
#endif
#ifndef UINT64_MAX
#define UINT64_MAX 18446744073709551615ULL
#endif

/* The least-width aliases are the exact-width aliases in the Rin ABI. */
#ifndef INT_LEAST8_MIN
#define INT_LEAST8_MIN INT8_MIN
#endif
#ifndef INT_LEAST8_MAX
#define INT_LEAST8_MAX INT8_MAX
#endif
#ifndef UINT_LEAST8_MAX
#define UINT_LEAST8_MAX UINT8_MAX
#endif
#ifndef INT_LEAST16_MIN
#define INT_LEAST16_MIN INT16_MIN
#endif
#ifndef INT_LEAST16_MAX
#define INT_LEAST16_MAX INT16_MAX
#endif
#ifndef UINT_LEAST16_MAX
#define UINT_LEAST16_MAX UINT16_MAX
#endif
#ifndef INT_LEAST32_MIN
#define INT_LEAST32_MIN INT32_MIN
#endif
#ifndef INT_LEAST32_MAX
#define INT_LEAST32_MAX INT32_MAX
#endif
#ifndef UINT_LEAST32_MAX
#define UINT_LEAST32_MAX UINT32_MAX
#endif
#ifndef INT_LEAST64_MIN
#define INT_LEAST64_MIN INT64_MIN
#endif
#ifndef INT_LEAST64_MAX
#define INT_LEAST64_MAX INT64_MAX
#endif
#ifndef UINT_LEAST64_MAX
#define UINT_LEAST64_MAX UINT64_MAX
#endif

/* Keep the fast-width limits coupled to the compiler-selected ABI types. */
#if defined(_MSC_VER)
#define INT_FAST8_MIN INT8_MIN
#define INT_FAST8_MAX INT8_MAX
#define UINT_FAST8_MAX UINT8_MAX
#define INT_FAST16_MIN INT16_MIN
#define INT_FAST16_MAX INT16_MAX
#define UINT_FAST16_MAX UINT16_MAX
#define INT_FAST32_MIN INT32_MIN
#define INT_FAST32_MAX INT32_MAX
#define UINT_FAST32_MAX UINT32_MAX
#define INT_FAST64_MIN INT64_MIN
#define INT_FAST64_MAX INT64_MAX
#define UINT_FAST64_MAX UINT64_MAX
#else
#ifndef INT_FAST8_MIN
#define INT_FAST8_MIN (-__INT_FAST8_MAX__ - 1)
#endif
#ifndef INT_FAST8_MAX
#define INT_FAST8_MAX __INT_FAST8_MAX__
#endif
#ifndef UINT_FAST8_MAX
#define UINT_FAST8_MAX __UINT_FAST8_MAX__
#endif
#ifndef INT_FAST16_MIN
#define INT_FAST16_MIN (-__INT_FAST16_MAX__ - 1)
#endif
#ifndef INT_FAST16_MAX
#define INT_FAST16_MAX __INT_FAST16_MAX__
#endif
#ifndef UINT_FAST16_MAX
#define UINT_FAST16_MAX __UINT_FAST16_MAX__
#endif
#ifndef INT_FAST32_MIN
#define INT_FAST32_MIN (-__INT_FAST32_MAX__ - 1)
#endif
#ifndef INT_FAST32_MAX
#define INT_FAST32_MAX __INT_FAST32_MAX__
#endif
#ifndef UINT_FAST32_MAX
#define UINT_FAST32_MAX __UINT_FAST32_MAX__
#endif
#ifndef INT_FAST64_MIN
#define INT_FAST64_MIN (-__INT_FAST64_MAX__ - 1)
#endif
#ifndef INT_FAST64_MAX
#define INT_FAST64_MAX __INT_FAST64_MAX__
#endif
#ifndef UINT_FAST64_MAX
#define UINT_FAST64_MAX __UINT_FAST64_MAX__
#endif
#endif

#ifndef INTMAX_MIN
#define INTMAX_MIN INT64_MIN
#endif
#ifndef INTMAX_MAX
#define INTMAX_MAX INT64_MAX
#endif
#ifndef UINTMAX_MAX
#define UINTMAX_MAX UINT64_MAX
#endif

/* Integer constant macros */
#ifndef INT8_C
#if defined(_MSC_VER)
#define INT8_C(x) x
#else
#define INT8_C(x)   x
#endif
#endif
#ifndef INT16_C
#if defined(_MSC_VER)
#define INT16_C(x) x
#else
#define INT16_C(x)  x
#endif
#endif
#ifndef INT32_C
#if defined(_MSC_VER)
#define INT32_C(x) x
#else
#define INT32_C(x)  x
#endif
#endif
#ifndef INT64_C
#if defined(_MSC_VER)
#define INT64_C(x)  x##i64
#else
#define INT64_C(x)  x##LL
#endif
#endif

#ifndef UINT8_C
#if defined(_MSC_VER)
#define UINT8_C(x) x
#else
#define UINT8_C(x)  x
#endif
#endif
#ifndef UINT16_C
#if defined(_MSC_VER)
#define UINT16_C(x) x
#else
#define UINT16_C(x) x
#endif
#endif
#ifndef UINT32_C
#if defined(_MSC_VER)
#define UINT32_C(x) x##U
#else
#define UINT32_C(x) x##U
#endif
#endif
#ifndef UINT64_C
#if defined(_MSC_VER)
#define UINT64_C(x) x##ui64
#else
#define UINT64_C(x) x##ULL
#endif
#endif

#ifndef INTMAX_C
#if defined(_MSC_VER)
#define INTMAX_C(x)  INT64_C(x)
#else
#define INTMAX_C(x)  INT64_C(x)
#endif
#endif
#ifndef UINTMAX_C
#if defined(_MSC_VER)
#define UINTMAX_C(x) UINT64_C(x)
#else
#define UINTMAX_C(x) UINT64_C(x)
#endif
#endif

#endif /* !__CLANG_STDINT_H */

#endif /* _STDINT_H */
