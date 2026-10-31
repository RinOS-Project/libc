/* SPDX-License-Identifier: MIT */
#include <rin/atomic_fallback_v1.h>

#include <stddef.h>
#include <stdint.h>
#include "rin_atomic_wait.h"

#define RIN_ATOMIC_WAIT_BUCKET_COUNT 64u

typedef struct rin_atomic_waiter_v1 {
    const volatile void *object;
    size_t size;
    volatile uint32_t wake_state;
    struct rin_atomic_waiter_v1 *next;
} rin_atomic_waiter_v1;

static volatile uint32_t g_rin_atomic_wait_lock;
static unsigned long long g_rin_atomic_wait_generation;
static rin_atomic_waiter_v1 *g_rin_atomic_wait_buckets[
    RIN_ATOMIC_WAIT_BUCKET_COUNT];

static void rin_atomic_wait_lock_acquire(void)
{
    while (__atomic_exchange_n(&g_rin_atomic_wait_lock, 1u,
                               __ATOMIC_ACQUIRE) != 0u) {
        __asm__ volatile("pause" ::: "memory");
    }
}

static void rin_atomic_wait_lock_release(void)
{
    __atomic_store_n(&g_rin_atomic_wait_lock, 0u, __ATOMIC_RELEASE);
}

static size_t rin_atomic_wait_bucket_index(const volatile void *object,
                                           size_t size)
{
    uintptr_t key = (uintptr_t)object;
    key ^= key >> 11;
    key ^= (uintptr_t)size * (uintptr_t)0x9e3779b1u;
    return (size_t)(key & (RIN_ATOMIC_WAIT_BUCKET_COUNT - 1u));
}

#ifdef RIN_ATOMIC_WAIT_RUNTIME_TEST_HOOKS
extern int rin_atomic_wait_test_host_wait(volatile uint32_t *address,
                                          uint32_t expected,
                                          const struct timespec *timeout);
extern int rin_atomic_wait_test_host_wake_one(volatile uint32_t *address);
extern int rin_atomic_wait_test_host_wake_all(volatile uint32_t *address);
#define RIN_ATOMIC_WAIT_RUNTIME_WAIT(address, expected, timeout) \
    rin_atomic_wait_test_host_wait((address), (expected), (timeout))
#define RIN_ATOMIC_WAIT_RUNTIME_WAKE_ONE(address) \
    rin_atomic_wait_test_host_wake_one((address))
#define RIN_ATOMIC_WAIT_RUNTIME_WAKE_ALL(address) \
    rin_atomic_wait_test_host_wake_all((address))
#else
#define RIN_ATOMIC_WAIT_RUNTIME_WAIT(address, expected, timeout) \
    rin_atomic_wait_u32((address), (expected), (timeout))
#define RIN_ATOMIC_WAIT_RUNTIME_WAKE_ONE(address) \
    rin_atomic_notify_one_u32((address))
#define RIN_ATOMIC_WAIT_RUNTIME_WAKE_ALL(address) \
    rin_atomic_notify_all_u32((address))
#endif

unsigned long long __rin_atomic_wait_prepare_v1(void)
{
    unsigned long long generation;
    rin_atomic_wait_lock_acquire();
    generation = g_rin_atomic_wait_generation;
    rin_atomic_wait_lock_release();
    return generation;
}

void __rin_atomic_wait_park_v1(const volatile void *object, size_t size,
                               unsigned long long generation)
{
    const size_t bucket_index = rin_atomic_wait_bucket_index(object, size);
    rin_atomic_waiter_v1 waiter;

    waiter.object = object;
    waiter.size = size;
    waiter.next = NULL;
    __atomic_store_n(&waiter.wake_state, 0u, __ATOMIC_RELAXED);

    rin_atomic_wait_lock_acquire();
    if (g_rin_atomic_wait_generation != generation) {
        rin_atomic_wait_lock_release();
        return;
    }
    waiter.next = g_rin_atomic_wait_buckets[bucket_index];
    g_rin_atomic_wait_buckets[bucket_index] = &waiter;
    rin_atomic_wait_lock_release();

    while (__atomic_load_n(&waiter.wake_state, __ATOMIC_ACQUIRE) == 0u) {
        struct timespec retry_timeout;
        int wait_result;
        retry_timeout.tv_sec = 0;
        retry_timeout.tv_nsec = 100000000L;
        wait_result = RIN_ATOMIC_WAIT_RUNTIME_WAIT(
            &waiter.wake_state, 0u, &retry_timeout);
#ifdef RIN_ATOMIC_WAIT_RUNTIME_TEST_HOOKS
        (void)wait_result;
#else
        if (wait_result != 0 && errno != ETIMEDOUT) {
            while (__atomic_load_n(&waiter.wake_state,
                                   __ATOMIC_ACQUIRE) == 0u) {
                __asm__ volatile("pause" ::: "memory");
            }
        }
#endif
    }

    rin_atomic_wait_lock_acquire();
    {
        rin_atomic_waiter_v1 **link =
            &g_rin_atomic_wait_buckets[bucket_index];
        while (*link && *link != &waiter)
            link = &(*link)->next;
        if (*link == &waiter)
            *link = waiter.next;
    }
    rin_atomic_wait_lock_release();
}

void __rin_atomic_notify_v1(const volatile void *object, size_t size,
                            int notify_all)
{
    const size_t bucket_index =
        rin_atomic_wait_bucket_index(object, size);
    rin_atomic_waiter_v1 *waiter;

    rin_atomic_wait_lock_acquire();
    ++g_rin_atomic_wait_generation;
    for (waiter = g_rin_atomic_wait_buckets[bucket_index]; waiter;
         waiter = waiter->next) {
        if (waiter->object != object || waiter->size != size ||
            __atomic_load_n(&waiter->wake_state, __ATOMIC_RELAXED) != 0u)
            continue;
        __atomic_store_n(&waiter->wake_state, 1u, __ATOMIC_RELEASE);
        if (notify_all)
            (void)RIN_ATOMIC_WAIT_RUNTIME_WAKE_ALL(&waiter->wake_state);
        else
            (void)RIN_ATOMIC_WAIT_RUNTIME_WAKE_ONE(&waiter->wake_state);
        if (!notify_all)
            break;
    }
    rin_atomic_wait_lock_release();
}

static volatile uint32_t g_rin_atomic_fallback_lock;

static void rin_atomic_fallback_lock_acquire(void)
{
    while (__atomic_exchange_n(&g_rin_atomic_fallback_lock, 1u,
                               __ATOMIC_ACQUIRE) != 0u) {
        __asm__ volatile("pause" ::: "memory");
    }
}

static void rin_atomic_fallback_lock_release(void)
{
    __atomic_store_n(&g_rin_atomic_fallback_lock, 0u, __ATOMIC_RELEASE);
}

static void rin_atomic_fallback_copy_from_volatile(
    unsigned char *destination, const volatile unsigned char *source,
    size_t size)
{
    size_t index;
    if ((uintptr_t)destination < (uintptr_t)source ||
        (uintptr_t)destination - (uintptr_t)source >= size) {
        for (index = 0; index < size; ++index)
            destination[index] = source[index];
    } else {
        for (index = size; index > 0u; --index)
            destination[index - 1u] = source[index - 1u];
    }
}

static void rin_atomic_fallback_copy_to_volatile(
    volatile unsigned char *destination, const unsigned char *source,
    size_t size)
{
    size_t index;
    if ((uintptr_t)destination < (uintptr_t)source ||
        (uintptr_t)destination - (uintptr_t)source >= size) {
        for (index = 0; index < size; ++index)
            destination[index] = source[index];
    } else {
        for (index = size; index > 0u; --index)
            destination[index - 1u] = source[index - 1u];
    }
}

void __rin_atomic_load_v1(const volatile void *object, void *result,
                          size_t size, int memory_order)
{
    (void)memory_order;
    rin_atomic_fallback_lock_acquire();
    rin_atomic_fallback_copy_from_volatile((unsigned char *)result,
        (const volatile unsigned char *)object, size);
    rin_atomic_fallback_lock_release();
}

void __rin_atomic_store_v1(volatile void *object, const void *value,
                           size_t size, int memory_order)
{
    const unsigned char *source = (const unsigned char *)value;
    volatile unsigned char *destination = (volatile unsigned char *)object;
    (void)memory_order;
    rin_atomic_fallback_lock_acquire();
    rin_atomic_fallback_copy_to_volatile(destination, source, size);
    rin_atomic_fallback_lock_release();
}

void __rin_atomic_exchange_v1(volatile void *object, const void *desired,
                              void *result, size_t size, int memory_order)
{
    unsigned char snapshot[size ? size : 1u];
    size_t index;
    const unsigned char *source = (const unsigned char *)desired;
    volatile unsigned char *target = (volatile unsigned char *)object;
    volatile unsigned char *output = (volatile unsigned char *)result;
    (void)memory_order;
    rin_atomic_fallback_lock_acquire();
    for (index = 0; index < size; ++index) snapshot[index] = source[index];
    rin_atomic_fallback_copy_from_volatile(
        (unsigned char *)output, (const volatile unsigned char *)target, size);
    rin_atomic_fallback_copy_to_volatile(target, snapshot, size);
    rin_atomic_fallback_lock_release();
}

int __rin_atomic_compare_exchange_v1(
    volatile void *object, void *expected, const void *desired, size_t size,
    int weak, int success_order, int failure_order)
{
    size_t index;
    volatile unsigned char *target = (volatile unsigned char *)object;
    unsigned char *expected_bytes = (unsigned char *)expected;
    const unsigned char *desired_bytes = (const unsigned char *)desired;
    int equal = 1;
    (void)weak;
    (void)success_order;
    (void)failure_order;
    rin_atomic_fallback_lock_acquire();
    for (index = 0; index < size; ++index) {
        if (target[index] != expected_bytes[index]) {
            equal = 0;
            break;
        }
    }
    if (equal) {
        rin_atomic_fallback_copy_to_volatile(target, desired_bytes, size);
    } else {
        rin_atomic_fallback_copy_from_volatile(
            expected_bytes, (const volatile unsigned char *)target, size);
    }
    rin_atomic_fallback_lock_release();
    return equal;
}
