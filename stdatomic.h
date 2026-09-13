/*
 * RinOS libc - stdatomic.h
 * C11原子操作 (x86_64実装)
 */

#ifndef _STDATOMIC_H
#define _STDATOMIC_H

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"
#include "rin_atomic_wait.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * メモリオーダー
 * ═══════════════════════════════════════════════════════════════*/

typedef enum {
    memory_order_relaxed = 0,
    memory_order_consume = 1,
    memory_order_acquire = 2,
    memory_order_release = 3,
    memory_order_acq_rel = 4,
    memory_order_seq_cst = 5
} memory_order;

/* ═══════════════════════════════════════════════════════════════
 * アトミック型定義
 * ═══════════════════════════════════════════════════════════════*/

typedef _Atomic _Bool              atomic_bool;
typedef _Atomic char               atomic_char;
typedef _Atomic signed char        atomic_schar;
typedef _Atomic unsigned char      atomic_uchar;
typedef _Atomic short              atomic_short;
typedef _Atomic unsigned short     atomic_ushort;
typedef _Atomic int                atomic_int;
typedef _Atomic unsigned int       atomic_uint;
typedef _Atomic long               atomic_long;
typedef _Atomic unsigned long      atomic_ulong;
typedef _Atomic long long          atomic_llong;
typedef _Atomic unsigned long long atomic_ullong;
typedef _Atomic intptr_t           atomic_intptr_t;
typedef _Atomic uintptr_t          atomic_uintptr_t;
typedef _Atomic size_t             atomic_size_t;
typedef _Atomic ptrdiff_t          atomic_ptrdiff_t;
typedef _Atomic intmax_t           atomic_intmax_t;
typedef _Atomic uintmax_t          atomic_uintmax_t;

typedef _Atomic int_fast8_t        atomic_int_fast8_t;
typedef _Atomic uint_fast8_t       atomic_uint_fast8_t;
typedef _Atomic int_fast16_t       atomic_int_fast16_t;
typedef _Atomic uint_fast16_t      atomic_uint_fast16_t;
typedef _Atomic int_fast32_t       atomic_int_fast32_t;
typedef _Atomic uint_fast32_t      atomic_uint_fast32_t;
typedef _Atomic int_fast64_t       atomic_int_fast64_t;
typedef _Atomic uint_fast64_t      atomic_uint_fast64_t;

typedef _Atomic int_least8_t       atomic_int_least8_t;
typedef _Atomic uint_least8_t      atomic_uint_least8_t;
typedef _Atomic int_least16_t      atomic_int_least16_t;
typedef _Atomic uint_least16_t     atomic_uint_least16_t;
typedef _Atomic int_least32_t      atomic_int_least32_t;
typedef _Atomic uint_least32_t     atomic_uint_least32_t;
typedef _Atomic int_least64_t      atomic_int_least64_t;
typedef _Atomic uint_least64_t     atomic_uint_least64_t;

/* atomic_flag (最も基本的なアトミック型) */
typedef struct {
    atomic_bool value;
} atomic_flag;

#define ATOMIC_FLAG_INIT { 0 }
#define ATOMIC_VAR_INIT(value) (value)

/* ═══════════════════════════════════════════════════════════════
 * アトミック操作 (GCC built-in使用)
 * ═══════════════════════════════════════════════════════════════*/

#define __RIN_ATOMIC_VALUE_TYPE(obj) __typeof__((*(obj)) + 0)
#define __RIN_ATOMIC_VALUE_PTR(obj) ((__RIN_ATOMIC_VALUE_TYPE(obj)*)(obj))

#define atomic_init(obj, value) \
    __atomic_store_n(__RIN_ATOMIC_VALUE_PTR(obj), value, __ATOMIC_RELAXED)

#define atomic_store(obj, value) \
    __atomic_store_n(__RIN_ATOMIC_VALUE_PTR(obj), value, __ATOMIC_SEQ_CST)

#define atomic_store_explicit(obj, value, order) \
    __atomic_store_n(__RIN_ATOMIC_VALUE_PTR(obj), value, order)

#define atomic_load(obj) \
    __atomic_load_n(__RIN_ATOMIC_VALUE_PTR(obj), __ATOMIC_SEQ_CST)

#define atomic_load_explicit(obj, order) \
    __atomic_load_n(__RIN_ATOMIC_VALUE_PTR(obj), order)

#define atomic_exchange(obj, desired) \
    __atomic_exchange_n(__RIN_ATOMIC_VALUE_PTR(obj), desired, __ATOMIC_SEQ_CST)

#define atomic_exchange_explicit(obj, desired, order) \
    __atomic_exchange_n(__RIN_ATOMIC_VALUE_PTR(obj), desired, order)

#define atomic_compare_exchange_strong(obj, expected, desired) \
    __atomic_compare_exchange_n(__RIN_ATOMIC_VALUE_PTR(obj), expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)

#define atomic_compare_exchange_strong_explicit(obj, expected, desired, succ, fail) \
    __atomic_compare_exchange_n(__RIN_ATOMIC_VALUE_PTR(obj), expected, desired, 0, succ, fail)

#define atomic_compare_exchange_weak(obj, expected, desired) \
    __atomic_compare_exchange_n(__RIN_ATOMIC_VALUE_PTR(obj), expected, desired, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)

#define atomic_compare_exchange_weak_explicit(obj, expected, desired, succ, fail) \
    __atomic_compare_exchange_n(__RIN_ATOMIC_VALUE_PTR(obj), expected, desired, 1, succ, fail)

#define atomic_fetch_add(obj, arg) \
    __atomic_fetch_add(__RIN_ATOMIC_VALUE_PTR(obj), arg, __ATOMIC_SEQ_CST)

#define atomic_fetch_add_explicit(obj, arg, order) \
    __atomic_fetch_add(__RIN_ATOMIC_VALUE_PTR(obj), arg, order)

#define atomic_fetch_sub(obj, arg) \
    __atomic_fetch_sub(__RIN_ATOMIC_VALUE_PTR(obj), arg, __ATOMIC_SEQ_CST)

#define atomic_fetch_sub_explicit(obj, arg, order) \
    __atomic_fetch_sub(__RIN_ATOMIC_VALUE_PTR(obj), arg, order)

#define atomic_fetch_and(obj, arg) \
    __atomic_fetch_and(__RIN_ATOMIC_VALUE_PTR(obj), arg, __ATOMIC_SEQ_CST)

#define atomic_fetch_and_explicit(obj, arg, order) \
    __atomic_fetch_and(__RIN_ATOMIC_VALUE_PTR(obj), arg, order)

#define atomic_fetch_or(obj, arg) \
    __atomic_fetch_or(__RIN_ATOMIC_VALUE_PTR(obj), arg, __ATOMIC_SEQ_CST)

#define atomic_fetch_or_explicit(obj, arg, order) \
    __atomic_fetch_or(__RIN_ATOMIC_VALUE_PTR(obj), arg, order)

#define atomic_fetch_xor(obj, arg) \
    __atomic_fetch_xor(__RIN_ATOMIC_VALUE_PTR(obj), arg, __ATOMIC_SEQ_CST)

#define atomic_fetch_xor_explicit(obj, arg, order) \
    __atomic_fetch_xor(__RIN_ATOMIC_VALUE_PTR(obj), arg, order)

/* ═══════════════════════════════════════════════════════════════
 * atomic_flag操作
 * ═══════════════════════════════════════════════════════════════*/

static inline _Bool atomic_flag_test_and_set(volatile atomic_flag* flag) {
    return __atomic_test_and_set(&flag->value, __ATOMIC_SEQ_CST);
}

static inline _Bool atomic_flag_test_and_set_explicit(volatile atomic_flag* flag, memory_order order) {
    return __atomic_test_and_set(&flag->value, order);
}

static inline void atomic_flag_clear(volatile atomic_flag* flag) {
    __atomic_clear(&flag->value, __ATOMIC_SEQ_CST);
}

static inline void atomic_flag_clear_explicit(volatile atomic_flag* flag, memory_order order) {
    __atomic_clear(&flag->value, order);
}

/* ═══════════════════════════════════════════════════════════════
 * メモリフェンス
 * ═══════════════════════════════════════════════════════════════*/

#define atomic_thread_fence(order) __atomic_thread_fence(order)
#define atomic_signal_fence(order) __atomic_signal_fence(order)

/* ═══════════════════════════════════════════════════════════════
 * ロックフリー判定
 * ═══════════════════════════════════════════════════════════════*/

#define ATOMIC_BOOL_LOCK_FREE       __GCC_ATOMIC_BOOL_LOCK_FREE
#define ATOMIC_CHAR_LOCK_FREE       __GCC_ATOMIC_CHAR_LOCK_FREE
#define ATOMIC_CHAR16_T_LOCK_FREE   __GCC_ATOMIC_CHAR16_T_LOCK_FREE
#define ATOMIC_CHAR32_T_LOCK_FREE   __GCC_ATOMIC_CHAR32_T_LOCK_FREE
#define ATOMIC_WCHAR_T_LOCK_FREE    __GCC_ATOMIC_WCHAR_T_LOCK_FREE
#define ATOMIC_SHORT_LOCK_FREE      __GCC_ATOMIC_SHORT_LOCK_FREE
#define ATOMIC_INT_LOCK_FREE        __GCC_ATOMIC_INT_LOCK_FREE
#define ATOMIC_LONG_LOCK_FREE       __GCC_ATOMIC_LONG_LOCK_FREE
#define ATOMIC_LLONG_LOCK_FREE      __GCC_ATOMIC_LLONG_LOCK_FREE
#define ATOMIC_POINTER_LOCK_FREE    __GCC_ATOMIC_POINTER_LOCK_FREE

#define atomic_is_lock_free(obj) __atomic_is_lock_free(sizeof(*(obj)), obj)

#ifdef __cplusplus
}
#endif

#endif /* _STDATOMIC_H */
