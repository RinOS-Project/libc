#ifndef _SETJMP_H
#define _SETJMP_H

#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && \
    __STDC_HOSTED__ && (defined(__clang__) || defined(__GNUC__))
/* Hosted consumers must share the platform's jmp_buf layout.  The compact
 * Rin declaration below is only a freestanding ABI contract; using it with
 * the MinGW CRT's setjmp/longjmp implementation corrupts the saved context. */
#include_next <setjmp.h>
#else

#include "signal.h"
#include "stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__x86_64__) || defined(_M_X64)
/* rbx, rbp, r12, r13, r14, r15, rsp, rip */
typedef long jmp_buf[8];
#elif defined(__i386__) || defined(_M_IX86)
/* ebx, esi, edi, ebp, esp, eip */
typedef long jmp_buf[6];
#else
#error "RinOS setjmp.h only supports x86_64 and i386"
#endif

// setjmp: save context, return 0
// Returns non-zero when returning from longjmp
int setjmp(jmp_buf env) __attribute__((returns_twice));

// longjmp: restore context, make setjmp return val (or 1 if val is 0)
void longjmp(jmp_buf env, int val) __attribute__((noreturn));

// Aliases
#define _setjmp setjmp
#define _longjmp longjmp

typedef struct {
    jmp_buf __context;
    sigset_t __saved_mask;
    int __mask_saved;
} __rin_sigjmp_context;
typedef __rin_sigjmp_context sigjmp_buf[1];

static inline void __rin_sigjmp_prepare(sigjmp_buf env, int save_mask) {
    env[0].__mask_saved = 0;
    if (save_mask &&
        sigprocmask(SIG_SETMASK, NULL, &env[0].__saved_mask) == 0)
        env[0].__mask_saved = 1;
}

static inline void __rin_sigjmp_restore(sigjmp_buf env) {
    if (env[0].__mask_saved &&
        sigprocmask(SIG_SETMASK, &env[0].__saved_mask, NULL) < 0)
        abort();
}

/* Keep the setjmp call in the caller's frame: saving it in a helper would
 * leave a context whose stack frame has already returned. */
#define sigsetjmp(env, savesigs) \
    (__rin_sigjmp_prepare((env), (savesigs)), setjmp((env)[0].__context))
#define siglongjmp(env, val) \
    do { \
        __rin_sigjmp_restore((env)); \
        longjmp((env)[0].__context, (val)); \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* hosted platform setjmp owner */

#endif // _SETJMP_H
