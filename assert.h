/* SPDX-License-Identifier: MIT */
#ifndef _ASSERT_H
#define _ASSERT_H

#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && \
    __STDC_HOSTED__ && (defined(__clang__) || defined(__GNUC__))
/* Hosted tests and tools must use the host C runtime's assertion owner.  The
 * RinOS implementation below terminates through rin_log/rin_exit, neither of
 * which is linked into ordinary host-only unit executables. */
#include_next <assert.h>
#else

#ifdef NDEBUG
    #define assert(expr) ((void)0)
#else
#ifdef __cplusplus
extern "C" {
#endif
    extern void __assert_fail(const char* expr, const char* file, int line);
#ifdef __cplusplus
}
#endif
    #define assert(expr) \
        ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__))
#endif

/* Static assert - C11 uses _Static_assert, C++11+ has static_assert as keyword */
#ifndef __cplusplus
    #define static_assert _Static_assert
#endif

#endif /* hosted C assertion owner */

#endif // _ASSERT_H
