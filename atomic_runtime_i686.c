/* SPDX-License-Identifier: MIT */
/*
 * Clang lowers under-aligned 64-bit atomic builtins on i686 to these libatomic
 * entry points.  Keep the fallback in the freestanding libc runtime so IA-32
 * applications do not acquire a host libatomic dependency.
 */

typedef unsigned int rin_atomic_u32;
typedef unsigned long long rin_atomic_u64;

static volatile rin_atomic_u32 g_rin_atomic_u64_lock;

static void rin_atomic_u64_lock_acquire(void)
{
    while (__atomic_exchange_n(&g_rin_atomic_u64_lock, 1u, __ATOMIC_ACQUIRE) !=
           0u)
        __asm__ volatile("pause" ::: "memory");
}

static void rin_atomic_u64_lock_release(void)
{
    __atomic_store_n(&g_rin_atomic_u64_lock, 0u, __ATOMIC_RELEASE);
}

/* The global lock gives the fallback a single modification order; acquire /
 * release is intentionally stronger than each caller's requested order. */
rin_atomic_u64 rin_atomic_load_8_runtime(const volatile void* address,
                                         int memory_order)
    __asm__("__atomic_load_8");
rin_atomic_u64 rin_atomic_load_8_runtime(const volatile void* address,
                                         int memory_order)
{
    rin_atomic_u64 value;
    (void)memory_order;
    rin_atomic_u64_lock_acquire();
    value = *(const volatile rin_atomic_u64*)address;
    rin_atomic_u64_lock_release();
    return value;
}

void rin_atomic_store_8_runtime(volatile void* address, rin_atomic_u64 value,
                                int memory_order)
    __asm__("__atomic_store_8");
void rin_atomic_store_8_runtime(volatile void* address, rin_atomic_u64 value,
                                int memory_order)
{
    (void)memory_order;
    rin_atomic_u64_lock_acquire();
    *(volatile rin_atomic_u64*)address = value;
    rin_atomic_u64_lock_release();
}

rin_atomic_u64 rin_atomic_exchange_8_runtime(volatile void* address,
                                             rin_atomic_u64 value,
                                             int memory_order)
    __asm__("__atomic_exchange_8");
rin_atomic_u64 rin_atomic_exchange_8_runtime(volatile void* address,
                                             rin_atomic_u64 value,
                                             int memory_order)
{
    rin_atomic_u64 previous;
    (void)memory_order;
    rin_atomic_u64_lock_acquire();
    previous = *(volatile rin_atomic_u64*)address;
    *(volatile rin_atomic_u64*)address = value;
    rin_atomic_u64_lock_release();
    return previous;
}

rin_atomic_u64 rin_atomic_fetch_add_8_runtime(volatile void* address,
                                              rin_atomic_u64 value,
                                              int memory_order)
    __asm__("__atomic_fetch_add_8");
rin_atomic_u64 rin_atomic_fetch_add_8_runtime(volatile void* address,
                                              rin_atomic_u64 value,
                                              int memory_order)
{
    rin_atomic_u64 previous;
    (void)memory_order;
    rin_atomic_u64_lock_acquire();
    previous = *(volatile rin_atomic_u64*)address;
    *(volatile rin_atomic_u64*)address = previous + value;
    rin_atomic_u64_lock_release();
    return previous;
}

rin_atomic_u64 rin_atomic_fetch_sub_8_runtime(volatile void* address,
                                              rin_atomic_u64 value,
                                              int memory_order)
    __asm__("__atomic_fetch_sub_8");
rin_atomic_u64 rin_atomic_fetch_sub_8_runtime(volatile void* address,
                                              rin_atomic_u64 value,
                                              int memory_order)
{
    rin_atomic_u64 previous;
    (void)memory_order;
    rin_atomic_u64_lock_acquire();
    previous = *(volatile rin_atomic_u64*)address;
    *(volatile rin_atomic_u64*)address = previous - value;
    rin_atomic_u64_lock_release();
    return previous;
}

rin_atomic_u64 rin_atomic_fetch_and_8_runtime(volatile void* address,
                                              rin_atomic_u64 value,
                                              int memory_order)
    __asm__("__atomic_fetch_and_8");
rin_atomic_u64 rin_atomic_fetch_and_8_runtime(volatile void* address,
                                              rin_atomic_u64 value,
                                              int memory_order)
{
    rin_atomic_u64 previous;
    (void)memory_order;
    rin_atomic_u64_lock_acquire();
    previous = *(volatile rin_atomic_u64*)address;
    *(volatile rin_atomic_u64*)address = previous & value;
    rin_atomic_u64_lock_release();
    return previous;
}

rin_atomic_u64 rin_atomic_fetch_or_8_runtime(volatile void* address,
                                             rin_atomic_u64 value,
                                             int memory_order)
    __asm__("__atomic_fetch_or_8");
rin_atomic_u64 rin_atomic_fetch_or_8_runtime(volatile void* address,
                                             rin_atomic_u64 value,
                                             int memory_order)
{
    rin_atomic_u64 previous;
    (void)memory_order;
    rin_atomic_u64_lock_acquire();
    previous = *(volatile rin_atomic_u64*)address;
    *(volatile rin_atomic_u64*)address = previous | value;
    rin_atomic_u64_lock_release();
    return previous;
}

rin_atomic_u64 rin_atomic_fetch_xor_8_runtime(volatile void* address,
                                              rin_atomic_u64 value,
                                              int memory_order)
    __asm__("__atomic_fetch_xor_8");
rin_atomic_u64 rin_atomic_fetch_xor_8_runtime(volatile void* address,
                                              rin_atomic_u64 value,
                                              int memory_order)
{
    rin_atomic_u64 previous;
    (void)memory_order;
    rin_atomic_u64_lock_acquire();
    previous = *(volatile rin_atomic_u64*)address;
    *(volatile rin_atomic_u64*)address = previous ^ value;
    rin_atomic_u64_lock_release();
    return previous;
}

/* Clang's i686 outlined builtin ABI omits the weak flag when the lowering is
 * statically strong.  The C identifier avoids GCC's differently-shaped
 * builtin declaration while retaining Clang's required object symbol. */
_Bool rin_atomic_compare_exchange_8_runtime(volatile void* address,
                                            void* expected_address,
                                            rin_atomic_u64 desired,
                                            int success_order,
                                            int failure_order)
    __asm__("__atomic_compare_exchange_8");
_Bool rin_atomic_compare_exchange_8_runtime(volatile void* address,
                                            void* expected_address,
                                            rin_atomic_u64 desired,
                                            int success_order,
                                            int failure_order)
{
    rin_atomic_u64 current;
    rin_atomic_u64 expected;
    (void)success_order;
    (void)failure_order;
    rin_atomic_u64_lock_acquire();
    current = *(volatile rin_atomic_u64*)address;
    expected = *(rin_atomic_u64*)expected_address;
    if (current == expected) {
        *(volatile rin_atomic_u64*)address = desired;
        rin_atomic_u64_lock_release();
        return 1;
    }
    *(rin_atomic_u64*)expected_address = current;
    rin_atomic_u64_lock_release();
    return 0;
}
