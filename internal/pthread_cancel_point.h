/* SPDX-License-Identifier: MIT */
#ifndef RIN_LIBC_INTERNAL_PTHREAD_CANCEL_POINT_H
#define RIN_LIBC_INTERNAL_PTHREAD_CANCEL_POINT_H

/* The syscall-facing headers are also used by programs that do not link the
 * pthread runtime.  Keep their cancellation hook weak so those programs do
 * not acquire a pthread runtime dependency. */
#if defined(__GNUC__) || defined(__clang__)
typedef void (*__rin_pthread_cancel_cleanup_fn)(void* argument);
#ifdef __cplusplus
extern "C" {
#endif
extern void __rin_pthread_cancel_point(void) __attribute__((weak));
extern void __rin_pthread_cancel_point_cleanup(
    __rin_pthread_cancel_cleanup_fn cleanup, void* argument)
    __attribute__((weak));
#ifdef __cplusplus
}
#endif
static inline void __rin_pthread_cancel_point_if_linked(void)
{
    if (__rin_pthread_cancel_point != 0)
        __rin_pthread_cancel_point();
}
static inline void __rin_pthread_cancel_point_cleanup_if_linked(
    __rin_pthread_cancel_cleanup_fn cleanup, void* argument)
{
    if (__rin_pthread_cancel_point_cleanup != 0)
        __rin_pthread_cancel_point_cleanup(cleanup, argument);
}
#else
typedef void (*__rin_pthread_cancel_cleanup_fn)(void* argument);
static inline void __rin_pthread_cancel_point_if_linked(void) { }
static inline void __rin_pthread_cancel_point_cleanup_if_linked(
    __rin_pthread_cancel_cleanup_fn cleanup, void* argument)
{
    (void)cleanup;
    (void)argument;
}
#endif

#endif
