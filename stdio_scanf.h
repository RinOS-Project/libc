/* SPDX-License-Identifier: MIT */
/* Narrow stdio scanning ABI. */

#ifndef _RIN_STDIO_SCANF_ABI_H
#define _RIN_STDIO_SCANF_ABI_H

#include "stdarg.h"

#ifndef _FILE_DECLARED
#define _FILE_DECLARED
struct _FILE;
typedef struct _FILE FILE;
#endif

#ifdef __cplusplus
extern "C" {
#endif

int vsscanf(const char* string, const char* format, va_list ap);
int sscanf(const char* string, const char* format, ...);
int vfscanf(FILE* stream, const char* format, va_list ap);
int vscanf(const char* format, va_list ap);
int fscanf(FILE* stream, const char* format, ...);
int scanf(const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif /* _RIN_STDIO_SCANF_ABI_H */
