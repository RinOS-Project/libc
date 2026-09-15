/*
 * RinOS libc - fenv.h
 * 浮動小数点環境
 */

#ifndef _FENV_H
#define _FENV_H

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * 型定義
 * ═══════════════════════════════════════════════════════════════*/

/* 浮動小数点環境 */
typedef struct {
    unsigned int __control;
    unsigned int __status;
    unsigned int __tag;
    unsigned int __others[4];
} fenv_t;

/* 浮動小数点例外フラグ */
typedef unsigned int fexcept_t;

/* ═══════════════════════════════════════════════════════════════
 * 例外フラグ定数
 * ═══════════════════════════════════════════════════════════════*/

#define FE_INVALID      0x01    /* 無効な操作 */
#define FE_DENORMAL     0x02    /* 非正規化数オペランド */
#define FE_DIVBYZERO    0x04    /* ゼロ除算 */
#define FE_OVERFLOW     0x08    /* オーバーフロー */
#define FE_UNDERFLOW    0x10    /* アンダーフロー */
#define FE_INEXACT      0x20    /* 不正確な結果 */

#define FE_ALL_EXCEPT   (FE_INVALID | FE_DENORMAL | FE_DIVBYZERO | \
                         FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT)

/* ═══════════════════════════════════════════════════════════════
 * 丸めモード定数
 * ═══════════════════════════════════════════════════════════════*/

#define FE_TONEAREST    0x0000  /* 最近接偶数への丸め */
#define FE_DOWNWARD     0x0400  /* 負の無限大への丸め */
#define FE_UPWARD       0x0800  /* 正の無限大への丸め */
#define FE_TOWARDZERO   0x0C00  /* ゼロへの丸め */

#ifndef MIDL_PASS
/* ═══════════════════════════════════════════════════════════════
 * デフォルト環境
 * ═══════════════════════════════════════════════════════════════*/

static const fenv_t __fe_dfl_env = {
    0x037F, 0, 0xFFFF, {0x1F80u, 0, 0, 0}
};
#define FE_DFL_ENV (&__fe_dfl_env)

#if defined(__i386__) || defined(__x86_64__)
typedef struct __attribute__((packed)) {
    unsigned short control;
    unsigned short control_padding;
    unsigned short status;
    unsigned short status_padding;
    unsigned short tag;
    unsigned short tag_padding;
    unsigned int instruction_offset;
    unsigned short instruction_selector;
    unsigned short opcode;
    unsigned int data_offset;
    unsigned short data_selector;
    unsigned short data_padding;
} __rin_x87_environment_t;

#ifdef __cplusplus
static_assert(sizeof(__rin_x87_environment_t) == 28u,
              "x87 environment layout drift");
#else
_Static_assert(sizeof(__rin_x87_environment_t) == 28u,
               "x87 environment layout drift");
#endif

static inline void __rin_x87_getenv(__rin_x87_environment_t* environment) {
    __asm__ volatile ("fnstenv %0" : "=m"(*environment) : : "memory");
}

static inline void __rin_x87_setenv(
    const __rin_x87_environment_t* environment) {
    __asm__ volatile ("fldenv %0" : : "m"(*environment) : "memory");
}

static inline void __rin_x87_update_excepts(unsigned int clear_mask,
                                             unsigned int raise_mask) {
    __rin_x87_environment_t environment;
    __rin_x87_getenv(&environment);
    environment.status = (unsigned short)(
        (environment.status & ~(clear_mask & FE_ALL_EXCEPT)) |
        (raise_mask & FE_ALL_EXCEPT));
    __rin_x87_setenv(&environment);
}

#ifdef __x86_64__
static inline unsigned int __rin_mxcsr_get(void) {
    unsigned int mxcsr;
    __asm__ volatile ("stmxcsr %0" : "=m"(mxcsr) : : "memory");
    return mxcsr;
}

static inline void __rin_mxcsr_set(unsigned int mxcsr) {
    __asm__ volatile ("ldmxcsr %0" : : "m"(mxcsr) : "memory");
}
#endif
#endif

/* ═══════════════════════════════════════════════════════════════
 * 例外処理関数
 * ═══════════════════════════════════════════════════════════════*/

/* 例外フラグをクリア */
static inline int feclearexcept(int excepts) {
    unsigned int requested = (unsigned int)excepts & FE_ALL_EXCEPT;
    if (requested == 0u) return 0;

#if defined(__i386__) || defined(__x86_64__)
    __rin_x87_update_excepts(requested, 0u);
#ifdef __x86_64__
    unsigned int mxcsr = __rin_mxcsr_get();
    mxcsr &= ~requested;
    __rin_mxcsr_set(mxcsr);
#endif
#else
    return -1;
#endif
    return 0;
}

/* 例外フラグを取得 */
static inline int fetestexcept(int excepts) {
#if defined(__i386__) || defined(__x86_64__)
    unsigned short status;
    __asm__ volatile ("fnstsw %0" : "=m"(status));
#ifdef __x86_64__
    unsigned int mxcsr = __rin_mxcsr_get();
    status |= (unsigned short)mxcsr;
#endif
    return (int)status & (excepts & FE_ALL_EXCEPT);
#else
    (void)excepts;
    return 0;
#endif
}

/* 例外フラグを発生させる */
static inline int feraiseexcept(int excepts) {
    unsigned int requested = (unsigned int)excepts & FE_ALL_EXCEPT;
    if (requested == 0u) return 0;

#if defined(__i386__) || defined(__x86_64__)
    __rin_x87_update_excepts(0u, requested);
#ifdef __x86_64__
    __rin_mxcsr_set(__rin_mxcsr_get() | requested);
#endif
    return 0;
#else
    return -1;
#endif
}

/* 例外フラグを保存 */
static inline int fegetexceptflag(fexcept_t* flagp, int excepts) {
    if (!flagp) return -1;
    *flagp = (fexcept_t)fetestexcept(excepts);
    return 0;
}

/* 例外フラグを復元 */
static inline int fesetexceptflag(const fexcept_t* flagp, int excepts) {
    if (!flagp) return -1;
    if (feclearexcept(excepts) != 0) return -1;
    return feraiseexcept((int)(*flagp) & excepts);
}

/* ═══════════════════════════════════════════════════════════════
 * 丸めモード関数
 * ═══════════════════════════════════════════════════════════════*/

/* 丸めモードを取得 */
static inline int fegetround(void) {
#if defined(__i386__) || defined(__x86_64__)
    unsigned short control;
    __asm__ volatile ("fnstcw %0" : "=m"(control));
    return control & 0x0C00;
#else
    return FE_TONEAREST;
#endif
}

/* 丸めモードを設定 */
static inline int fesetround(int round) {
    if (round != FE_TONEAREST && round != FE_DOWNWARD &&
        round != FE_UPWARD && round != FE_TOWARDZERO) {
        return -1;
    }

#if defined(__i386__) || defined(__x86_64__)
    unsigned short control;
    __asm__ volatile ("fnstcw %0" : "=m"(control));
    control = (control & ~0x0C00) | (round & 0x0C00);
    __asm__ volatile ("fldcw %0" : : "m"(control));
#ifdef __x86_64__
    unsigned int mxcsr;
    __asm__ volatile ("stmxcsr %0" : "=m"(mxcsr));
    mxcsr = (mxcsr & ~(3u << 13)) | (((unsigned int)round >> 10) << 13);
    __asm__ volatile ("ldmxcsr %0" : : "m"(mxcsr));
#endif
    return 0;
#else
    return round == FE_TONEAREST ? 0 : -1;
#endif
}

/* ═══════════════════════════════════════════════════════════════
 * 環境関数
 * ═══════════════════════════════════════════════════════════════*/

/* 浮動小数点環境を取得 */
static inline int fegetenv(fenv_t* envp) {
    if (!envp) return -1;

#if defined(__i386__) || defined(__x86_64__)
    __rin_x87_environment_t x87_environment;
    __rin_x87_getenv(&x87_environment);
    *envp = __fe_dfl_env;
    envp->__control = x87_environment.control;
    envp->__status = x87_environment.status;
    envp->__tag = x87_environment.tag;
#ifdef __x86_64__
    envp->__others[0] = __rin_mxcsr_get();
#endif
    __rin_x87_setenv(&x87_environment);
#else
    return -1;
#endif
    return 0;
}

/* 浮動小数点環境を設定 */
static inline int fesetenv(const fenv_t* envp) {
    if (!envp) return -1;

#if defined(__i386__) || defined(__x86_64__)
    __rin_x87_environment_t x87_environment;
    __rin_x87_getenv(&x87_environment);
    x87_environment.control = (unsigned short)envp->__control;
    x87_environment.status = (unsigned short)envp->__status;
    x87_environment.tag = (unsigned short)envp->__tag;
    __rin_x87_setenv(&x87_environment);
#ifdef __x86_64__
    __rin_mxcsr_set(envp->__others[0]);
#endif
#else
    return -1;
#endif
    return 0;
}

/* 浮動小数点環境を保存してデフォルトに設定 */
static inline int feholdexcept(fenv_t* envp) {
    if (!envp) return -1;

#if defined(__i386__) || defined(__x86_64__)
    if (fegetenv(envp) != 0) return -1;
    __rin_x87_environment_t x87_environment;
    __rin_x87_getenv(&x87_environment);
    x87_environment.control |= FE_ALL_EXCEPT;
    x87_environment.status &= (unsigned short)~FE_ALL_EXCEPT;
    __rin_x87_setenv(&x87_environment);
#ifdef __x86_64__
    unsigned int mxcsr = __rin_mxcsr_get();
    mxcsr |= (0x3Fu << 7);
    mxcsr &= ~0x3Fu;
    __rin_mxcsr_set(mxcsr);
#endif
#else
    return -1;
#endif
    return 0;
}

/* 浮動小数点環境を復元し、例外を発生 */
static inline int feupdateenv(const fenv_t* envp) {
    if (!envp) return -1;

#if defined(__i386__) || defined(__x86_64__)
    int status = fetestexcept(FE_ALL_EXCEPT);
    if (fesetenv(envp) != 0) return -1;
    if (status) return feraiseexcept(status);
#else
    return -1;
#endif
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif /* !MIDL_PASS */

#endif /* _FENV_H */
