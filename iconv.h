/* SPDX-License-Identifier: MIT */
/* POSIX character set conversion interface. */

#ifndef _ICONV_H
#define _ICONV_H

#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The value is an opaque generation-tagged libc descriptor. */
typedef void* iconv_t;

iconv_t iconv_open(const char* tocode, const char* fromcode);
size_t iconv(iconv_t descriptor, char** input, size_t* input_left,
             char** output, size_t* output_left);
int iconv_close(iconv_t descriptor);

#ifdef __cplusplus
}
#endif

#endif /* _ICONV_H */
