/* SPDX-License-Identifier: MIT */
/* Export temporary-file symbols through the shared libc implementation. */

#define mkstemp __rin_mkstemp_inline
#define mkstemps __rin_mkstemps_inline
#define mkostemp __rin_mkostemp_inline
#define mkostemps __rin_mkostemps_inline
#define mkdtemp __rin_mkdtemp_inline
#include "stdlib.h"
#undef mkstemp
#undef mkstemps
#undef mkostemp
#undef mkostemps
#undef mkdtemp

#if !defined(RINSDK_BOOTSTRAP_HAS_MKSTEMP)
int mkstemp(char* template_name) {
    return __rin_mkstemp_inline(template_name);
}
#endif

int mkstemps(char* template_name, int suffix_length) {
    return __rin_mkstemps_inline(template_name, suffix_length);
}

int mkostemp(char* template_name, int flags) {
    return __rin_mkostemp_inline(template_name, flags);
}

int mkostemps(char* template_name, int suffix_length, int flags) {
    return __rin_mkostemps_inline(template_name, suffix_length, flags);
}

char* mkdtemp(char* template_name) {
    return __rin_mkdtemp_inline(template_name);
}
