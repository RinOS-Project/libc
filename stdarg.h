/*
 * RinOS libc - stdarg.h
 * 可変長引数
 */

#ifndef _STDARG_H
#define _STDARG_H
#if !defined(__cplusplus) && defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 202311L
#ifndef __STDC_VERSION_STDARG_H__
#define __STDC_VERSION_STDARG_H__ 202311L
#endif
#endif

typedef __builtin_va_list va_list;
typedef __builtin_va_list __gnuc_va_list;

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

#endif /* _STDARG_H */
