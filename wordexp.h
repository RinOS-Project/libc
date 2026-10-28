/* SPDX-License-Identifier: MIT */
/* POSIX shell word expansion. */
#ifndef _WORDEXP_H
#define _WORDEXP_H

#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t we_wordc;
    char** we_wordv;
    size_t we_offs;
} wordexp_t;

#define WRDE_APPEND  0x01
#define WRDE_DOOFFS  0x02
#define WRDE_NOCMD   0x04
#define WRDE_REUSE   0x08
#define WRDE_SHOWERR 0x10
#define WRDE_UNDEF   0x20

#define WRDE_BADCHAR 1
#define WRDE_BADVAL  2
#define WRDE_CMDSUB  3
#define WRDE_NOSPACE 4
#define WRDE_SYNTAX  5
#define WRDE_NOSYS   6

int wordexp(const char* words, wordexp_t* word_vector, int flags);
void wordfree(wordexp_t* word_vector);

#ifdef __cplusplus
}
#endif

#endif /* _WORDEXP_H */
