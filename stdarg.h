/*
 * RinOS libc - stdarg.h
 * 可変長引数
 */

#ifndef _STDARG_H
#define _STDARG_H

typedef __builtin_va_list va_list;
typedef __builtin_va_list __gnuc_va_list;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define va_start(ap, ...) __builtin_va_start(ap __VA_OPT__(,) __VA_ARGS__)
#else
#define va_start(ap, last) __builtin_va_start(ap, last)
#endif
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

#endif /* _STDARG_H */
