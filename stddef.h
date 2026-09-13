/*
 * RinOS libc - stddef.h
 * 基本型定義
 */

#ifndef _STDDEF_H
#define _STDDEF_H

/* Use compiler built-in types for portability.  MSVC does not expose the
 * GNU/Clang spelling, so retain the actual target-width ABI explicitly. */
#if defined(_MSC_VER)
#if defined(_WIN64)
typedef unsigned __int64 size_t;
typedef __int64          ptrdiff_t;
#else
typedef unsigned int size_t;
typedef int          ptrdiff_t;
#endif
#else
typedef __SIZE_TYPE__    size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#endif

/* wchar_t is a built-in type in C++, only define in C */
#if !defined(__cplusplus) && !defined(_WCHAR_T_DEFINED)
#define _WCHAR_T_DEFINED
#if defined(_MSC_VER)
typedef unsigned short    wchar_t;
#elif defined(__WCHAR_TYPE__)
/* Hosted CRTs (notably MinGW) use the compiler's ABI-selected width.  Keep
 * that spelling and advertise the same guard so a later CRT include does not
 * redeclare wchar_t with a conflicting underlying type. */
typedef __WCHAR_TYPE__    wchar_t;
#else
typedef int              wchar_t;
#endif
#endif

#ifndef NULL
  #ifdef __cplusplus
    #if __cplusplus >= 201103L
      #define NULL nullptr
    #else
      #define NULL 0
    #endif
  #else
    #define NULL ((void*)0)
  #endif
#endif

/* offsetof - 構造体メンバのオフセットを取得
 *
 * 注意: C++の定数式(constexpr/template)でoffsetofを使用するには
 * コンパイラの__builtin_offsetofが必要です。これはGCC, Clang, MSVC等
 * 全ての主要C++コンパイラでサポートされており、標準ライブラリの
 * 実装でも使用されています（glibc, musl, libcxx等）。MSVC の C
 * frontend はこの spelling を公開しないため、そこでだけ compiler-
 * recognized な C の conventional form を使います。
 *
 * ポインタ演算による実装 ((size_t)&((type*)0)->member) は
 * 実行時には動作しますが、C++の定数式では使用できません。
 */
#if defined(_MSC_VER) && !defined(__cplusplus)
#define offsetof(type, member) ((size_t)&(((type*)0)->member))
#elif !defined(offsetof)
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif

/* max_align_t - type with maximum alignment requirement
 * RinOS独自定義 */
#if defined(_MSC_VER)
typedef struct {
    long long __ll;
    long double __ld;
} max_align_t;
#elif !defined(__CLANG_MAX_ALIGN_T_DEFINED) && !defined(_GCC_MAX_ALIGN_T)
typedef struct {
    long long __ll __attribute__((__aligned__(__alignof__(long long))));
    long double __ld __attribute__((__aligned__(__alignof__(long double))));
} max_align_t;
#define __CLANG_MAX_ALIGN_T_DEFINED
#define _GCC_MAX_ALIGN_T
#endif

#endif /* _STDDEF_H */
