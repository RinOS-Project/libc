/*
 * RinOS libc - sys/ucontext.h
 * User context handling (sys/ wrapper)
 */

#if (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__
#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
#include_next <sys/ucontext.h>
#endif
#else
#ifndef _SYS_UCONTEXT_H
#define _SYS_UCONTEXT_H

#include "../ucontext.h"

#endif /* _SYS_UCONTEXT_H */
#endif /* hosted CRT provider vs. RinOS target implementation */
