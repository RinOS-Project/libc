/*
 * RinOS libc - ucontext.h
 * User context handling
 */

#ifndef _UCONTEXT_H
#define _UCONTEXT_H

#include "stddef.h"
#include "stdint.h"
#include "errno.h"
#include "stdarg.h"
#include "sys/syscall.h"

#ifdef __cplusplus
#define RIN_UCONTEXT_STATIC_ASSERT(condition, message) static_assert(condition, message)
#else
#define RIN_UCONTEXT_STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * Register structure (gregset)
 * ═══════════════════════════════════════════════════════════════*/

#if defined(__x86_64__) || defined(_M_X64)
/* x86-64 register indices */
#define NGREG   23

enum {
    REG_R8 = 0,
    REG_R9,
    REG_R10,
    REG_R11,
    REG_R12,
    REG_R13,
    REG_R14,
    REG_R15,
    REG_RDI,
    REG_RSI,
    REG_RBP,
    REG_RBX,
    REG_RDX,
    REG_RAX,
    REG_RCX,
    REG_RSP,
    REG_RIP,
    REG_EFL,
    REG_CSGSFS,
    REG_ERR,
    REG_TRAPNO,
    REG_OLDMASK,
    REG_CR2
};

typedef long long greg_t;
typedef greg_t gregset_t[NGREG];

#else
/* x86 (32-bit) register indices */
#define NGREG   19

enum {
    REG_GS = 0,
    REG_FS,
    REG_ES,
    REG_DS,
    REG_EDI,
    REG_ESI,
    REG_EBP,
    REG_ESP,
    REG_EBX,
    REG_EDX,
    REG_ECX,
    REG_EAX,
    REG_TRAPNO,
    REG_ERR,
    REG_EIP,
    REG_CS,
    REG_EFL,
    REG_UESP,
    REG_SS
};

typedef int greg_t;
typedef greg_t gregset_t[NGREG];

#endif /* __x86_64__ */

/* ═══════════════════════════════════════════════════════════════
 * Floating-point register structure
 * ═══════════════════════════════════════════════════════════════*/

struct _libc_fpreg {
    unsigned short significand[4];
    unsigned short exponent;
};

/* The context switcher owns the complete legacy x87/SSE register image.
 * Keep the storage fixed and 16-byte aligned so FXSAVE/FXRSTOR can be used
 * on both x86 targets.  The first 108 bytes have the architectural FXSAVE
 * header; callers must not infer a host libc fpstate layout from this private
 * type. */
#define RIN_UCONTEXT_FXSAVE_BYTES 512u
struct _libc_fpstate {
    unsigned char fxsave[RIN_UCONTEXT_FXSAVE_BYTES];
} __attribute__((aligned(16)));

typedef struct _libc_fpstate* fpregset_t;

RIN_UCONTEXT_STATIC_ASSERT(sizeof(struct _libc_fpstate) ==
                               RIN_UCONTEXT_FXSAVE_BYTES,
                           "ucontext FXSAVE storage size changed");

/* ═══════════════════════════════════════════════════════════════
 * Machine context structure
 * ═══════════════════════════════════════════════════════════════*/

typedef struct {
    gregset_t gregs;
    fpregset_t fpregs;
    unsigned long oldmask;
    unsigned long cr2;
} mcontext_t;

/* ═══════════════════════════════════════════════════════════════
 * Stack structure (must be defined before ucontext)
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _STACK_T_DEFINED
#define _STACK_T_DEFINED
typedef struct {
    void*  ss_sp;
    int    ss_flags;
    size_t ss_size;
} stack_t;
#endif

/* ═══════════════════════════════════════════════════════════════
 * Signal set type (must be defined before ucontext)
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _SIGSET_T_DEFINED
#define _SIGSET_T_DEFINED
typedef unsigned long sigset_t;
#endif

/* ═══════════════════════════════════════════════════════════════
 * User context structure
 * Note: Use struct __ucontext to match signal.h forward declaration
 * ═══════════════════════════════════════════════════════════════*/

struct __ucontext {
    unsigned long      uc_flags;
    struct __ucontext* uc_link;
    stack_t            uc_stack;
    mcontext_t         uc_mcontext;
    sigset_t           uc_sigmask;
    struct _libc_fpstate __fpregs_mem;
};

#ifndef _UCONTEXT_T_DEFINED
#define _UCONTEXT_T_DEFINED
typedef struct __ucontext ucontext_t;
#endif

/* ═══════════════════════════════════════════════════════════════
 * Context manipulation functions
 * ═══════════════════════════════════════════════════════════════
 *
 * The AMD64 and i386 implementations are deliberately small userspace
 * context switchers.  They record a call continuation and integer register
 * state.  FP/SIMD state is saved with the architectural FXSAVE format; signal
 * masks are intentionally still process-local and contexts remain same-thread.
 */

#define RIN_UCONTEXT_FLAG_TAG          0x52494e00UL
#define RIN_UCONTEXT_FLAG_TAG_MASK     0xffffff00UL
#define RIN_UCONTEXT_FLAG_READY        0x00000001UL
#define RIN_UCONTEXT_FLAG_MADE         0x00000002UL
#define RIN_UCONTEXT_FLAG_RESUMED      0x00000004UL
#define RIN_UCONTEXT_MAX_ARGS          16
#define RIN_UCONTEXT_RETURNS_TWICE __attribute__((returns_twice))
#define RIN_UCONTEXT_NOINLINE          __attribute__((noinline))
#define RIN_UCONTEXT_UNUSED            __attribute__((unused))

#if defined(__x86_64__) || defined(_M_X64)

#define RIN_UCONTEXT_RFLAGS_MASK       0x0000000000000cd5ULL
#define RIN_UCONTEXT_RFLAGS_REQUIRED   0x0000000000000202ULL

/* This layout is shared with the scheduler's signal-frame ABI.  Keep the
 * naked assembly below tied to explicit assertions instead of silently
 * accepting a future ABI drift. */
RIN_UCONTEXT_STATIC_ASSERT(offsetof(ucontext_t, uc_mcontext.gregs[REG_R8]) == 40u,
               "ucontext AMD64 R8 offset changed");
RIN_UCONTEXT_STATIC_ASSERT(offsetof(ucontext_t, uc_mcontext.gregs[REG_RSP]) == 160u,
               "ucontext AMD64 RSP offset changed");
RIN_UCONTEXT_STATIC_ASSERT(offsetof(ucontext_t, uc_mcontext.gregs[REG_RIP]) == 168u,
               "ucontext AMD64 RIP offset changed");
RIN_UCONTEXT_STATIC_ASSERT(offsetof(ucontext_t, uc_mcontext.gregs[REG_EFL]) == 176u,
               "ucontext AMD64 RFLAGS offset changed");
RIN_UCONTEXT_STATIC_ASSERT(offsetof(ucontext_t, uc_mcontext.fpregs) == 224u,
               "ucontext AMD64 FP pointer offset changed");

static inline int rin_ucontext_amd64_canonical(uintptr_t value) {
    uint64_t upper = (uint64_t)value >> 48;
    return upper == 0u || upper == 0xffffu;
}

static inline int rin_ucontext_amd64_ready(const ucontext_t* context) {
    uintptr_t rip;
    uintptr_t rsp;

    if (!context ||
        (context->uc_flags & RIN_UCONTEXT_FLAG_TAG_MASK) !=
            RIN_UCONTEXT_FLAG_TAG ||
        (context->uc_flags & RIN_UCONTEXT_FLAG_READY) == 0u) {
        return 0;
    }
    rip = (uintptr_t)context->uc_mcontext.gregs[REG_RIP];
    rsp = (uintptr_t)context->uc_mcontext.gregs[REG_RSP];
    return rip != 0u && rsp != 0u && (rsp & 7u) == 0u &&
           rin_ucontext_amd64_canonical(rip) &&
           rin_ucontext_amd64_canonical(rsp) &&
           context->uc_mcontext.fpregs != NULL &&
           (((uintptr_t)context->uc_mcontext.fpregs & 15u) == 0u);
}

/* The wrapper around this naked function validates the pointer first.  On a
 * later setcontext(), returning here runs the wrapper epilogue exactly as a
 * normal getcontext() call would. */
static __attribute__((naked, noinline)) int
rin_ucontext_amd64_capture(ucontext_t* context __attribute__((unused))) {
#if defined(_WIN64)
    __asm__(
        /* Win64 passes the context pointer in RCX; preserve the incoming
         * register values before reusing RDI as the local context pointer. */
        "movq %rax, 144(%rcx)\n\t"
        "movq %rdi, 104(%rcx)\n\t"
        "movq %rcx, 152(%rcx)\n\t"
        "movq %rcx, %rdi\n\t"
        "movq 224(%rdi), %rax\n\t"
        "fxsave (%rax)\n\t"
        "movq %r8, 40(%rdi)\n\t"
        "movq %r9, 48(%rdi)\n\t"
        "movq %r10, 56(%rdi)\n\t"
        "movq %r11, 64(%rdi)\n\t"
        "movq %r12, 72(%rdi)\n\t"
        "movq %r13, 80(%rdi)\n\t"
        "movq %r14, 88(%rdi)\n\t"
        "movq %r15, 96(%rdi)\n\t"
        "movq %rsi, 112(%rdi)\n\t"
        "movq %rbp, 120(%rdi)\n\t"
        "movq %rbx, 128(%rdi)\n\t"
        "movq %rdx, 136(%rdi)\n\t"
        "movq %rcx, 152(%rdi)\n\t"
        "leaq 8(%rsp), %rax\n\t"
        "movq %rax, 160(%rdi)\n\t"
        "movq (%rsp), %rax\n\t"
        "movq %rax, 168(%rdi)\n\t"
        "movq $0x202, 176(%rdi)\n\t"
        "movq 104(%rdi), %rdi\n\t"
        "xorl %eax, %eax\n\t"
        "ret\n\t");
#else
    __asm__(
        "movq %rax, 144(%rdi)\n\t"
        "movq 224(%rdi), %rax\n\t"
        "fxsave (%rax)\n\t"
        "movq %r8, 40(%rdi)\n\t"
        "movq %r9, 48(%rdi)\n\t"
        "movq %r10, 56(%rdi)\n\t"
        "movq %r11, 64(%rdi)\n\t"
        "movq %r12, 72(%rdi)\n\t"
        "movq %r13, 80(%rdi)\n\t"
        "movq %r14, 88(%rdi)\n\t"
        "movq %r15, 96(%rdi)\n\t"
        "movq %rdi, 104(%rdi)\n\t"
        "movq %rsi, 112(%rdi)\n\t"
        "movq %rbp, 120(%rdi)\n\t"
        "movq %rbx, 128(%rdi)\n\t"
        "movq %rdx, 136(%rdi)\n\t"
        "movq %rcx, 152(%rdi)\n\t"
        "leaq 8(%rsp), %rax\n\t"
        "movq %rax, 160(%rdi)\n\t"
        "movq (%rsp), %rax\n\t"
        "movq %rax, 168(%rdi)\n\t"
        "movq $0x202, 176(%rdi)\n\t"
        "xorl %eax, %eax\n\t"
        "ret\n\t");
#endif
}

/* setcontext() has validated the tag and the control-flow pair before this
 * function runs.  Store the continuation below the target RSP, restore all
 * integer registers, then return into the saved instruction pointer. */
static __attribute__((naked, noinline, noreturn)) void
rin_ucontext_amd64_restore(const ucontext_t* context __attribute__((unused))) {
#if defined(_WIN64)
    __asm__(
        "movq %rcx, %rdi\n\t"
        "movq 224(%rdi), %rax\n\t"
        "fxrstor (%rax)\n\t"
        "movq 160(%rdi), %r11\n\t"
        "subq $8, %r11\n\t"
        "movq 168(%rdi), %rax\n\t"
        "movq %rax, (%r11)\n\t"
        "movq 176(%rdi), %rax\n\t"
        "andq $0xcd5, %rax\n\t"
        "orq $0x202, %rax\n\t"
        "pushq %rax\n\t"
        "popfq\n\t"
        "movq 40(%rdi), %r8\n\t"
        "movq 48(%rdi), %r9\n\t"
        "movq 72(%rdi), %r12\n\t"
        "movq 80(%rdi), %r13\n\t"
        "movq 88(%rdi), %r14\n\t"
        "movq 96(%rdi), %r15\n\t"
        "movq 112(%rdi), %rsi\n\t"
        "movq 120(%rdi), %rbp\n\t"
        "movq 128(%rdi), %rbx\n\t"
        "movq 136(%rdi), %rdx\n\t"
        "movq 152(%rdi), %rcx\n\t"
        "movq 56(%rdi), %r10\n\t"
        "movq %r11, %rsp\n\t"
        "movq 64(%rdi), %r11\n\t"
        "movq 104(%rdi), %rdi\n\t"
        "xorl %eax, %eax\n\t"
        "ret\n\t");
#else
    __asm__(
        "movq 224(%rdi), %rax\n\t"
        "fxrstor (%rax)\n\t"
        "movq 160(%rdi), %r11\n\t"
        "subq $8, %r11\n\t"
        "movq 168(%rdi), %rax\n\t"
        "movq %rax, (%r11)\n\t"
        "movq 176(%rdi), %rax\n\t"
        "andq $0xcd5, %rax\n\t"
        "orq $0x202, %rax\n\t"
        "pushq %rax\n\t"
        "popfq\n\t"
        "movq 40(%rdi), %r8\n\t"
        "movq 48(%rdi), %r9\n\t"
        "movq 72(%rdi), %r12\n\t"
        "movq 80(%rdi), %r13\n\t"
        "movq 88(%rdi), %r14\n\t"
        "movq 96(%rdi), %r15\n\t"
        "movq 112(%rdi), %rsi\n\t"
        "movq 120(%rdi), %rbp\n\t"
        "movq 128(%rdi), %rbx\n\t"
        "movq 136(%rdi), %rdx\n\t"
        "movq 152(%rdi), %rcx\n\t"
        "movq 56(%rdi), %r10\n\t"
        "movq %r11, %rsp\n\t"
        "movq 64(%rdi), %r11\n\t"
        "movq 104(%rdi), %rdi\n\t"
        "xorl %eax, %eax\n\t"
        "ret\n\t");
#endif
}

static RIN_UCONTEXT_NOINLINE RIN_UCONTEXT_RETURNS_TWICE int
getcontext(ucontext_t* context) {
    if (!context) {
        errno = EFAULT;
        return -1;
    }

    /* Do this before capture: a resumed context returns after capture and
     * retains the swapcontext() resume marker until that wrapper consumes it. */
    context->uc_flags = RIN_UCONTEXT_FLAG_TAG | RIN_UCONTEXT_FLAG_READY;
    context->uc_mcontext.fpregs = &context->__fpregs_mem;
    context->uc_mcontext.oldmask = 0u;
    context->uc_mcontext.cr2 = 0u;
    context->uc_sigmask = 0u;
    return rin_ucontext_amd64_capture(context);
}

/* ASan cannot infer an arbitrary user-stack switch from a naked RET. Keep the
 * transfer boundary itself uninstrumented; callers and entered contexts stay
 * instrumented and are covered by the behavioral test. */
static inline __attribute__((no_sanitize("address"))) int
setcontext(const ucontext_t* context) {
    if (!context) {
        errno = EFAULT;
        return -1;
    }
    if (!rin_ucontext_amd64_ready(context)) {
        errno = EINVAL;
        return -1;
    }
    rin_ucontext_amd64_restore(context);
    __builtin_unreachable();
}

static __attribute__((noreturn, used)) void rin_ucontext_finish(void) {
    ucontext_t* link;

    __asm__ volatile("movq %%r12, %0" : "=r"(link));
    if (link) (void)setcontext(link);
    (void)_syscall1((uintptr_t)SYS_THREAD_EXIT, (uintptr_t)0);
    __builtin_trap();
}

/* A makecontext target returns with RSP aligned for an ordinary call.  This
 * naked shim creates that call frame before entering C code. */
static __attribute__((naked, noinline, noreturn)) void
rin_ucontext_return_trampoline(void) {
    __asm__("call rin_ucontext_finish\n\tud2\n\t");
}

static inline void makecontext(ucontext_t* context, void (*function)(void),
                               int argc, ...) {
    uintptr_t arguments[RIN_UCONTEXT_MAX_ARGS] = {0u};
    uintptr_t base;
    uintptr_t top;
    uintptr_t entry_stack;
    size_t stack_argument_count;
    size_t stack_words;
    size_t stack_bytes;
    va_list ap;
    int index;

    if (!context || !function) {
        errno = EFAULT;
        return;
    }
    if (!rin_ucontext_amd64_ready(context) || argc < 0) {
        errno = EINVAL;
        return;
    }
    if (argc > RIN_UCONTEXT_MAX_ARGS) {
        errno = ENOSYS;
        return;
    }
    base = (uintptr_t)context->uc_stack.ss_sp;
    if (base == 0u || !rin_ucontext_amd64_canonical(base) ||
        context->uc_stack.ss_size < 16u ||
        context->uc_stack.ss_size > UINTPTR_MAX - base) {
        errno = EINVAL;
        return;
    }
    top = (base + context->uc_stack.ss_size) & ~(uintptr_t)0x0fu;
 #if defined(_WIN64)
    stack_argument_count = argc > 4 ? (size_t)(argc - 4) : 0u;
 #else
    stack_argument_count = argc > 6 ? (size_t)(argc - 6) : 0u;
 #endif
    /* The function-entry stack owns its return trampoline followed by any
     * seventh-and-later SysV integer arguments.  An odd number of stack
     * arguments needs one trailing word so RSP is 16-byte aligned before the
     * synthetic CALL/RET handoff.  Keep one word below it for restore's
     * temporary saved RIP. */
 #if defined(_WIN64)
    /* Win64 reserves four home slots below the first stack argument and
     * enters a function with RSP congruent to eight modulo sixteen. */
    stack_words = 5u + stack_argument_count;
 #else
    stack_words = 1u + stack_argument_count +
                  (stack_argument_count & 1u);
 #endif
    stack_bytes = stack_words * sizeof(uintptr_t);
    if (top < base || top - base < stack_bytes + sizeof(uintptr_t) ||
        !rin_ucontext_amd64_canonical(top - stack_bytes) ||
        !rin_ucontext_amd64_canonical(top - stack_bytes -
                                      sizeof(uintptr_t))) {
        errno = EINVAL;
        return;
    }

    va_start(ap, argc);
    for (index = 0; index < argc; ++index)
        arguments[index] = va_arg(ap, uintptr_t);
    va_end(ap);

    /* setcontext places RIP at RSP - 8 and RETs to it.  The resulting
     * function-entry RSP holds the return trampoline; seventh and later
     * arguments follow at the normal AMD64 ABI stack offsets. */
 #if defined(_WIN64)
    entry_stack = top - stack_bytes;
    entry_stack = (entry_stack & ~(uintptr_t)0x0fu) | (uintptr_t)0x08u;
    if (entry_stack > top - stack_bytes) {
        if (entry_stack < base + 16u) {
            errno = EINVAL;
            return;
        }
        entry_stack -= 16u;
    }
 #else
    entry_stack = top - stack_bytes;
 #endif
    *(uintptr_t*)entry_stack = (uintptr_t)rin_ucontext_return_trampoline;
 #if defined(_WIN64)
    for (index = 4; index < argc; ++index) {
        *(uintptr_t*)(entry_stack + 5u * sizeof(uintptr_t) +
                      (size_t)(index - 4) * sizeof(uintptr_t)) =
            arguments[index];
    }
 #else
    for (index = 6; index < argc; ++index) {
        *(uintptr_t*)(entry_stack + sizeof(uintptr_t) +
                      (size_t)(index - 6) * sizeof(uintptr_t)) =
            arguments[index];
    }
 #endif
    for (index = 0; index < NGREG; ++index)
        context->uc_mcontext.gregs[index] = 0;
    context->uc_mcontext.gregs[REG_RIP] = (greg_t)(uintptr_t)function;
    context->uc_mcontext.gregs[REG_RSP] = (greg_t)entry_stack;
    context->uc_mcontext.gregs[REG_EFL] =
        (greg_t)RIN_UCONTEXT_RFLAGS_REQUIRED;
 #if defined(_WIN64)
    context->uc_mcontext.gregs[REG_RCX] = (greg_t)arguments[0];
    context->uc_mcontext.gregs[REG_RDX] = (greg_t)arguments[1];
    context->uc_mcontext.gregs[REG_R8] = (greg_t)arguments[2];
    context->uc_mcontext.gregs[REG_R9] = (greg_t)arguments[3];
 #else
    context->uc_mcontext.gregs[REG_RDI] = (greg_t)arguments[0];
    context->uc_mcontext.gregs[REG_RSI] = (greg_t)arguments[1];
    context->uc_mcontext.gregs[REG_RDX] = (greg_t)arguments[2];
    context->uc_mcontext.gregs[REG_RCX] = (greg_t)arguments[3];
    context->uc_mcontext.gregs[REG_R8] = (greg_t)arguments[4];
    context->uc_mcontext.gregs[REG_R9] = (greg_t)arguments[5];
 #endif
    context->uc_mcontext.gregs[REG_R12] = (greg_t)(uintptr_t)context->uc_link;
    context->uc_flags = RIN_UCONTEXT_FLAG_TAG | RIN_UCONTEXT_FLAG_READY |
                        RIN_UCONTEXT_FLAG_MADE;
}

static RIN_UCONTEXT_NOINLINE RIN_UCONTEXT_RETURNS_TWICE RIN_UCONTEXT_UNUSED int
swapcontext(ucontext_t* old_context, const ucontext_t* new_context) {
    if (!old_context || !new_context) {
        errno = EFAULT;
        return -1;
    }
    if (old_context == new_context ||
        !rin_ucontext_amd64_ready(new_context)) {
        errno = EINVAL;
        return -1;
    }
    if (getcontext(old_context) != 0)
        return -1;
    if ((old_context->uc_flags & RIN_UCONTEXT_FLAG_RESUMED) != 0u) {
        old_context->uc_flags &= ~RIN_UCONTEXT_FLAG_RESUMED;
        return 0;
    }
    old_context->uc_flags |= RIN_UCONTEXT_FLAG_RESUMED;
    return setcontext(new_context);
}

#elif defined(__i386__) || defined(_M_IX86)

#define RIN_UCONTEXT_EFLAGS_MASK       0x00000cd5UL
#define RIN_UCONTEXT_EFLAGS_REQUIRED   0x00000202UL

/* Pin the naked i386 assembly to the public mcontext control-flow offsets. */
RIN_UCONTEXT_STATIC_ASSERT(offsetof(ucontext_t, uc_mcontext.gregs[REG_EDI]) == 36u,
               "ucontext i386 EDI offset changed");
RIN_UCONTEXT_STATIC_ASSERT(offsetof(ucontext_t, uc_mcontext.gregs[REG_ESP]) == 48u,
               "ucontext i386 ESP offset changed");
RIN_UCONTEXT_STATIC_ASSERT(offsetof(ucontext_t, uc_mcontext.gregs[REG_EIP]) == 76u,
               "ucontext i386 EIP offset changed");
RIN_UCONTEXT_STATIC_ASSERT(offsetof(ucontext_t, uc_mcontext.gregs[REG_EFL]) == 84u,
               "ucontext i386 EFLAGS offset changed");
RIN_UCONTEXT_STATIC_ASSERT(offsetof(ucontext_t, uc_mcontext.fpregs) == 96u,
               "ucontext i386 FP pointer offset changed");

static inline int rin_ucontext_i386_ready(const ucontext_t* context) {
    uintptr_t eip;
    uintptr_t esp;

    if (!context ||
        (context->uc_flags & RIN_UCONTEXT_FLAG_TAG_MASK) !=
            RIN_UCONTEXT_FLAG_TAG ||
        (context->uc_flags & RIN_UCONTEXT_FLAG_READY) == 0u) {
        return 0;
    }
    eip = (uintptr_t)(unsigned int)context->uc_mcontext.gregs[REG_EIP];
    esp = (uintptr_t)(unsigned int)context->uc_mcontext.gregs[REG_ESP];
    return eip != 0u && esp != 0u && (esp & 3u) == 0u &&
           context->uc_mcontext.fpregs != NULL &&
           (((uintptr_t)context->uc_mcontext.fpregs & 15u) == 0u);
}

/* Capture the normal continuation after this helper returns.  EAX/EDX are
 * preserved on the helper stack long enough to record them; restore always
 * returns zero as required by getcontext(). */
static __attribute__((naked, noinline)) int
rin_ucontext_i386_capture(ucontext_t* context __attribute__((unused))) {
    __asm__(
        "pushl %eax\n\t"
        "pushl %edx\n\t"
        "movl 12(%esp), %eax\n\t"
        "movl 96(%eax), %edx\n\t"
        "fxsave (%edx)\n\t"
        "movl $0, 20(%eax)\n\t"
        "movl $0, 24(%eax)\n\t"
        "movl $0, 28(%eax)\n\t"
        "movl $0, 32(%eax)\n\t"
        "movl %edi, 36(%eax)\n\t"
        "movl %esi, 40(%eax)\n\t"
        "movl %ebp, 44(%eax)\n\t"
        "leal 12(%esp), %edx\n\t"
        "movl %edx, 48(%eax)\n\t"
        "movl %ebx, 52(%eax)\n\t"
        "movl 0(%esp), %edx\n\t"
        "movl %edx, 56(%eax)\n\t"
        "movl %ecx, 60(%eax)\n\t"
        "movl 4(%esp), %edx\n\t"
        "movl %edx, 64(%eax)\n\t"
        "movl $0, 68(%eax)\n\t"
        "movl $0, 72(%eax)\n\t"
        "movl 8(%esp), %edx\n\t"
        "movl %edx, 76(%eax)\n\t"
        "movl $0, 80(%eax)\n\t"
        "pushfl\n\t"
        "popl %edx\n\t"
        "movl %edx, 84(%eax)\n\t"
        "leal 12(%esp), %edx\n\t"
        "movl %edx, 88(%eax)\n\t"
        "movl $0, 92(%eax)\n\t"
        "addl $8, %esp\n\t"
        "xorl %eax, %eax\n\t"
        "ret\n\t");
}

/* The target control-flow pair is validated by setcontext(). Place its EIP
 * below the saved ESP, restore the preserved integer state, and RET into the
 * continuation or makecontext entry. */
static __attribute__((naked, noinline, noreturn)) void
rin_ucontext_i386_restore(const ucontext_t* context __attribute__((unused))) {
    __asm__(
        "movl 4(%esp), %eax\n\t"
        "movl 96(%eax), %ecx\n\t"
        "fxrstor (%ecx)\n\t"
        "movl 48(%eax), %edx\n\t"
        "subl $4, %edx\n\t"
        "movl 76(%eax), %ecx\n\t"
        "movl %ecx, (%edx)\n\t"
        "movl 84(%eax), %ecx\n\t"
        "andl $0xcd5, %ecx\n\t"
        "orl $0x202, %ecx\n\t"
        "pushl %ecx\n\t"
        "popfl\n\t"
        "movl %edx, %esp\n\t"
        "movl 36(%eax), %edi\n\t"
        "movl 40(%eax), %esi\n\t"
        "movl 44(%eax), %ebp\n\t"
        "movl 52(%eax), %ebx\n\t"
        "movl 56(%eax), %edx\n\t"
        "movl 60(%eax), %ecx\n\t"
        "xorl %eax, %eax\n\t"
        "ret\n\t");
}

static RIN_UCONTEXT_NOINLINE RIN_UCONTEXT_RETURNS_TWICE int
getcontext(ucontext_t* context) {
    if (!context) {
        errno = EFAULT;
        return -1;
    }

    context->uc_flags = RIN_UCONTEXT_FLAG_TAG | RIN_UCONTEXT_FLAG_READY;
    context->uc_mcontext.fpregs = &context->__fpregs_mem;
    context->uc_mcontext.oldmask = 0u;
    context->uc_mcontext.cr2 = 0u;
    context->uc_sigmask = 0u;
    return rin_ucontext_i386_capture(context);
}

static inline __attribute__((no_sanitize("address"))) int
setcontext(const ucontext_t* context) {
    if (!context) {
        errno = EFAULT;
        return -1;
    }
    if (!rin_ucontext_i386_ready(context)) {
        errno = EINVAL;
        return -1;
    }
    rin_ucontext_i386_restore(context);
    __builtin_unreachable();
}

/* ESI is callee-saved by the i386 ABI and carries uc_link for a prepared
 * context. A returning entry therefore reaches its link without a global
 * context table or cross-thread state. */
static __attribute__((noreturn, noinline, used)) void
rin_ucontext_i386_finish(void) {
    ucontext_t* link;

    __asm__ volatile("movl %%esi, %0" : "=r"(link));
    if (link) (void)setcontext(link);
    (void)_syscall1((uintptr_t)SYS_THREAD_EXIT, (uintptr_t)0);
    __builtin_trap();
}

static __attribute__((naked, noinline, noreturn)) void
rin_ucontext_i386_return_trampoline(void) {
    __asm__("call rin_ucontext_i386_finish\n\tud2\n\t");
}

static inline void makecontext(ucontext_t* context, void (*function)(void),
                               int argc, ...) {
    uintptr_t arguments[RIN_UCONTEXT_MAX_ARGS] = {0u};
    uintptr_t base;
    uintptr_t top;
    uintptr_t candidate;
    uintptr_t entry_stack;
    size_t stack_words;
    size_t stack_bytes;
    va_list ap;
    int index;

    if (!context || !function) {
        errno = EFAULT;
        return;
    }
    if (!rin_ucontext_i386_ready(context) || argc < 0) {
        errno = EINVAL;
        return;
    }
    if (argc > RIN_UCONTEXT_MAX_ARGS) {
        errno = ENOSYS;
        return;
    }
    base = (uintptr_t)context->uc_stack.ss_sp;
    if (base == 0u || context->uc_stack.ss_size < 16u ||
        context->uc_stack.ss_size > UINTPTR_MAX - base) {
        errno = EINVAL;
        return;
    }
    top = (base + context->uc_stack.ss_size) & ~(uintptr_t)0x0fu;
    stack_words = 1u + (size_t)argc;
    stack_bytes = stack_words * sizeof(uintptr_t);
    if (top < base || top - base < stack_bytes + sizeof(uintptr_t)) {
        errno = EINVAL;
        return;
    }

    /* i386 cdecl enters a function with the return address at ESP and its
     * first argument at ESP+4. Keep ESP mod 16 equal to 12 at entry, leaving
     * one word below it for restore's temporary continuation. */
    candidate = top - stack_bytes;
    entry_stack = (candidate & ~(uintptr_t)0x0fu) | (uintptr_t)0x0cu;
    if (entry_stack > candidate) {
        if (entry_stack < 16u) {
            errno = EINVAL;
            return;
        }
        entry_stack -= 16u;
    }
    if (entry_stack < base || entry_stack - base < sizeof(uintptr_t)) {
        errno = EINVAL;
        return;
    }

    va_start(ap, argc);
    for (index = 0; index < argc; ++index)
        arguments[index] = va_arg(ap, uintptr_t);
    va_end(ap);

    *(uintptr_t*)entry_stack =
        (uintptr_t)rin_ucontext_i386_return_trampoline;
    for (index = 0; index < argc; ++index) {
        *(uintptr_t*)(entry_stack + sizeof(uintptr_t) +
                      (size_t)index * sizeof(uintptr_t)) = arguments[index];
    }
    for (index = 0; index < NGREG; ++index)
        context->uc_mcontext.gregs[index] = 0;
    context->uc_mcontext.gregs[REG_EIP] = (greg_t)(uintptr_t)function;
    context->uc_mcontext.gregs[REG_ESP] = (greg_t)entry_stack;
    context->uc_mcontext.gregs[REG_UESP] = (greg_t)entry_stack;
    context->uc_mcontext.gregs[REG_EFL] =
        (greg_t)RIN_UCONTEXT_EFLAGS_REQUIRED;
    context->uc_mcontext.gregs[REG_ESI] =
        (greg_t)(uintptr_t)context->uc_link;
    context->uc_flags = RIN_UCONTEXT_FLAG_TAG | RIN_UCONTEXT_FLAG_READY |
                        RIN_UCONTEXT_FLAG_MADE;
}

static RIN_UCONTEXT_NOINLINE RIN_UCONTEXT_RETURNS_TWICE RIN_UCONTEXT_UNUSED int
swapcontext(ucontext_t* old_context, const ucontext_t* new_context) {
    if (!old_context || !new_context) {
        errno = EFAULT;
        return -1;
    }
    if (old_context == new_context ||
        !rin_ucontext_i386_ready(new_context)) {
        errno = EINVAL;
        return -1;
    }
    if (getcontext(old_context) != 0)
        return -1;
    if ((old_context->uc_flags & RIN_UCONTEXT_FLAG_RESUMED) != 0u) {
        old_context->uc_flags &= ~RIN_UCONTEXT_FLAG_RESUMED;
        return 0;
    }
    old_context->uc_flags |= RIN_UCONTEXT_FLAG_RESUMED;
    return setcontext(new_context);
}

#else

/* Non-x86 targets have no matching user-mode register restore owner yet.
 * Keep those backends explicitly unavailable until an owner exists. */
static inline int getcontext(ucontext_t* context) {
    (void)context;
    errno = ENOSYS;
    return -1;
}

static inline int setcontext(const ucontext_t* context) {
    (void)context;
    errno = ENOSYS;
    return -1;
}

static inline void makecontext(ucontext_t* context, void (*function)(void),
                               int argc, ...) {
    (void)context;
    (void)function;
    (void)argc;
    errno = ENOSYS;
}

static inline int swapcontext(ucontext_t* old_context,
                              const ucontext_t* new_context) {
    (void)old_context;
    (void)new_context;
    errno = ENOSYS;
    return -1;
}

#endif

#ifdef __cplusplus
}
#endif

#undef RIN_UCONTEXT_STATIC_ASSERT
#undef RIN_UCONTEXT_RETURNS_TWICE
#undef RIN_UCONTEXT_NOINLINE

#endif /* _UCONTEXT_H */
