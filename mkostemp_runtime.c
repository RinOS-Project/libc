/* SPDX-License-Identifier: MIT */
/* Export GNU temporary-file symbols through the shared libc implementation. */

#define mkostemp __rin_mkostemp_inline
#define mkostemps __rin_mkostemps_inline
#include "stdlib.h"
#undef mkostemp
#undef mkostemps

int mkostemp(char* template_name, int flags) {
    return __rin_mkostemp_inline(template_name, flags);
}

int mkostemps(char* template_name, int suffix_length, int flags) {
    return __rin_mkostemps_inline(template_name, suffix_length, flags);
}
