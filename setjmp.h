#define __STDC_VERSION_SETJMP_H__ 202311L


#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && \
    __STDC_HOSTED__ && (defined(__clang__) || defined(__GNUC__))
/* Hosted consumers must share the platform's jmp_buf layout.  The compact
 * Rin declaration below is only a freestanding ABI contract; using it with
 * the MinGW CRT's setjmp/longjmp implementation corrupts the saved context.
 * The include guard lives inside the freestanding branch: defining it
 * first would disable the host header, which shares the guard macro. */
#include_next <setjmp.h>
#else
#ifndef _SETJMP_H
#define _SETJMP_H

#include "signal.h"
#include "stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__x86_64__) || defined(_M_X64)
/* rbx, rbp, r12, r13, r14, r15, rdi, rsi, rsp, rip.  SysV keeps its
 * (caller-saved, ignored) RDI slot so both ABIs share one layout. */
typedef long jmp_buf[10];
#elif defined(__i386__) || defined(_M_IX86)
/* ebx, esi, edi, ebp, esp, eip */
typedef long jmp_buf[6];
#else
#error "RinOS setjmp.h only supports x86_64 and i386"
#endif

/* Rin setjmp/longjmp save the callee-saved integer state plus the stack
 * pointer and return address.  Signal masks are NOT saved (POSIX leaves
 * setjmp mask-saving unspecified; use sigsetjmp to save the mask), and
 * floating-point state is NOT saved (also POSIX-unspecified; ucontext
 * preserves the full context including FP).  naked-in-header follows
 * ucontext.h; a helper call would lose the Win64 caller-live RDI/RSI. */
#if defined(__x86_64__) || defined(_M_X64)
static __attribute__((naked, noinline, returns_twice, unused)) int
setjmp(jmp_buf env __attribute__((unused))) {
#if defined(_WIN64)
    __asm__(
        "movq %rbx, 0(%rcx)\n\t"
        "movq %rbp, 8(%rcx)\n\t"
        "movq %r12, 16(%rcx)\n\t"
        "movq %r13, 24(%rcx)\n\t"
        "movq %r14, 32(%rcx)\n\t"
        "movq %r15, 40(%rcx)\n\t"
        "movq %rdi, 48(%rcx)\n\t"
        "movq %rsi, 56(%rcx)\n\t"
        "leaq 8(%rsp), %rax\n\t"
        "movq %rax, 64(%rcx)\n\t"
        "movq (%rsp), %rax\n\t"
        "movq %rax, 72(%rcx)\n\t"
        "xorl %eax, %eax\n\t"
        "ret\n\t");
#else
    __asm__(
        "movq %rbx, 0(%rdi)\n\t"
        "movq %rbp, 8(%rdi)\n\t"
        "movq %r12, 16(%rdi)\n\t"
        "movq %r13, 24(%rdi)\n\t"
        "movq %r14, 32(%rdi)\n\t"
        "movq %r15, 40(%rdi)\n\t"
        "movq %rdi, 48(%rdi)\n\t"
        "movq %rsi, 56(%rdi)\n\t"
        "leaq 8(%rsp), %rax\n\t"
        "movq %rax, 64(%rdi)\n\t"
        "movq (%rsp), %rax\n\t"
        "movq %rax, 72(%rdi)\n\t"
        "xorl %eax, %eax\n\t"
        "ret\n\t");
#endif
}

static __attribute__((naked, noinline, noreturn, unused)) void
longjmp(jmp_buf env __attribute__((unused)),
        int val __attribute__((unused))) {
#if defined(_WIN64)
    __asm__(
        "movl %edx, %eax\n\t"
        "testl %eax, %eax\n\t"
        "jnz 1f\n\t"
        "movl $1, %eax\n\t"
        "1:\n\t"
        "movq 0(%rcx), %rbx\n\t"
        "movq 8(%rcx), %rbp\n\t"
        "movq 16(%rcx), %r12\n\t"
        "movq 24(%rcx), %r13\n\t"
        "movq 32(%rcx), %r14\n\t"
        "movq 40(%rcx), %r15\n\t"
        "movq 56(%rcx), %rsi\n\t"
        "movq 72(%rcx), %r11\n\t"
        "movq 64(%rcx), %rsp\n\t"
        "movq 48(%rcx), %rdi\n\t"
        "jmp *%r11\n\t");
#else
    __asm__(
        "movl %esi, %eax\n\t"
        "testl %eax, %eax\n\t"
        "jnz 1f\n\t"
        "movl $1, %eax\n\t"
        "1:\n\t"
        "movq 0(%rdi), %rbx\n\t"
        "movq 8(%rdi), %rbp\n\t"
        "movq 16(%rdi), %r12\n\t"
        "movq 24(%rdi), %r13\n\t"
        "movq 32(%rdi), %r14\n\t"
        "movq 40(%rdi), %r15\n\t"
        "movq 56(%rdi), %rsi\n\t"
        "movq 72(%rdi), %rcx\n\t"
        "movq 64(%rdi), %rsp\n\t"
        "movq 48(%rdi), %rdi\n\t"
        "jmp *%rcx\n\t");
#endif
}
#elif defined(__i386__) || defined(_M_IX86)
static __attribute__((naked, noinline, returns_twice, unused)) int
setjmp(jmp_buf env __attribute__((unused))) {
    __asm__(
        "movl 4(%esp), %eax\n\t"
        "movl %ebx, 0(%eax)\n\t"
        "movl %esi, 4(%eax)\n\t"
        "movl %edi, 8(%eax)\n\t"
        "movl %ebp, 12(%eax)\n\t"
        "leal 4(%esp), %ecx\n\t"
        "movl %ecx, 16(%eax)\n\t"
        "movl (%esp), %ecx\n\t"
        "movl %ecx, 20(%eax)\n\t"
        "xorl %eax, %eax\n\t"
        "ret\n\t");
}

static __attribute__((naked, noinline, noreturn, unused)) void
longjmp(jmp_buf env __attribute__((unused)),
        int val __attribute__((unused))) {
    __asm__(
        "movl 8(%esp), %eax\n\t"
        "testl %eax, %eax\n\t"
        "jnz 1f\n\t"
        "movl $1, %eax\n\t"
        "1:\n\t"
        "movl 4(%esp), %ecx\n\t"
        "movl 0(%ecx), %ebx\n\t"
        "movl 4(%ecx), %esi\n\t"
        "movl 8(%ecx), %edi\n\t"
        "movl 12(%ecx), %ebp\n\t"
        "movl 20(%ecx), %edx\n\t"
        "movl 16(%ecx), %esp\n\t"
        "jmp *%edx\n\t");
}
#endif

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
    if (save_mask) {
        /* sigsetjmp() has no error return.  Do not publish a context that
         * claims to save the signal mask when its snapshot actually failed. */
        if (sigprocmask(SIG_SETMASK, NULL, &env[0].__saved_mask) != 0)
            abort();
        env[0].__mask_saved = 1;
    }
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

#endif /* _SETJMP_H */

#endif /* hosted platform setjmp owner */
