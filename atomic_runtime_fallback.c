/* SPDX-License-Identifier: MIT */
#include <rin/atomic_fallback_v1.h>

#include <stddef.h>
#include <stdint.h>

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
