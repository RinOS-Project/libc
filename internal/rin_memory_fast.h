/* SPDX-License-Identifier: MIT */
#ifndef RIN_LIBC_INTERNAL_MEMORY_FAST_H
#define RIN_LIBC_INTERNAL_MEMORY_FAST_H

/*
 * Freestanding memory primitives shared by the kernel and application
 * runtime.  Kernel builds deliberately stay scalar/REP-only because kernel
 * code is compiled with -mno-sse/-mno-sse2 and must not borrow user SIMD
 * state.  x86_64 userspace may select AVX2 after checking both CPUID and
 * XCR0; ERMS/FSRM select REP MOVSB for the sizes where it is strongest.
 */

typedef __SIZE_TYPE__ rin_memory_size_t;
typedef __UINTPTR_TYPE__ rin_memory_uintptr_t;

#define RIN_MEMORY_X86_ERMS  (1u << 0)
#define RIN_MEMORY_X86_FSRM  (1u << 1)
#define RIN_MEMORY_X86_AVX2  (1u << 2)
#define RIN_MEMORY_X86_READY (1u << 31)

static inline unsigned long long rin_memory_load_u64(const void* pointer) {
    unsigned long long value;
#if defined(__x86_64__)
    __asm__ volatile("movq (%1), %0"
                     : "=r"(value)
                     : "r"(pointer)
                     : "memory");
#else
    const unsigned char* bytes = (const unsigned char*)pointer;
    value = 0;
    for (unsigned index = 0; index < 8u; index++) {
        value |= (unsigned long long)bytes[index] << (index * 8u);
    }
#endif
    return value;
}

static inline void rin_memory_store_u64(void* pointer,
                                        unsigned long long value) {
#if defined(__x86_64__)
    __asm__ volatile("movq %1, (%0)"
                     :
                     : "r"(pointer), "r"(value)
                     : "memory");
#else
    unsigned char* bytes = (unsigned char*)pointer;
    for (unsigned index = 0; index < 8u; index++) {
        bytes[index] = (unsigned char)(value >> (index * 8u));
    }
#endif
}

#if defined(__i386__)
static inline unsigned rin_memory_load_u32(const void* pointer) {
    unsigned value;
    __asm__ volatile("movl (%1), %0"
                     : "=r"(value)
                     : "r"(pointer)
                     : "memory");
    return value;
}

static inline void rin_memory_store_u32(void* pointer, unsigned value) {
    __asm__ volatile("movl %1, (%0)"
                     :
                     : "r"(pointer), "r"(value)
                     : "memory");
}

static inline void rin_memory_copy_u32(unsigned char** destination,
                                       const unsigned char** source,
                                       rin_memory_size_t* size) {
    unsigned char* d = *destination;
    const unsigned char* s = *source;
    rin_memory_size_t n = *size;
    while (n >= 4u) {
        rin_memory_store_u32(d, rin_memory_load_u32(s));
        d += 4u;
        s += 4u;
        n -= 4u;
    }
    *destination = d;
    *source = s;
    *size = n;
}
#endif

static inline unsigned rin_memory_x86_features(void) {
#if defined(__x86_64__)
    static unsigned cached;
    unsigned features = __atomic_load_n(&cached, __ATOMIC_RELAXED);
    unsigned maximum_leaf;
    unsigned eax;
    unsigned ebx;
    unsigned ecx;
    unsigned edx;

    if ((features & RIN_MEMORY_X86_READY) != 0u) return features;

    __asm__ volatile("cpuid"
                     : "=a"(maximum_leaf), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(0u), "c"(0u));
    features = 0u;
    if (maximum_leaf >= 7u) {
        unsigned leaf7_ebx;
        unsigned leaf7_edx;
        __asm__ volatile("cpuid"
                         : "=a"(eax), "=b"(leaf7_ebx), "=c"(ecx),
                           "=d"(leaf7_edx)
                         : "a"(7u), "c"(0u));
        if ((leaf7_ebx & (1u << 9)) != 0u) features |= RIN_MEMORY_X86_ERMS;
        if ((leaf7_edx & (1u << 4)) != 0u) features |= RIN_MEMORY_X86_FSRM;

        if ((leaf7_ebx & (1u << 5)) != 0u) {
#if defined(RIN_USERSPACE)
            unsigned leaf1_ecx;
            unsigned xcr0_low;
            unsigned xcr0_high;
            __asm__ volatile("cpuid"
                             : "=a"(eax), "=b"(ebx), "=c"(leaf1_ecx),
                               "=d"(edx)
                             : "a"(1u), "c"(0u));
            if ((leaf1_ecx & (1u << 27)) != 0u &&
                (leaf1_ecx & (1u << 28)) != 0u) {
                __asm__ volatile("xgetbv"
                                 : "=a"(xcr0_low), "=d"(xcr0_high)
                                 : "c"(0u));
                (void)xcr0_high;
                if ((xcr0_low & 0x6u) == 0x6u) {
                    features |= RIN_MEMORY_X86_AVX2;
                }
            }
#endif
        }
    }
    features |= RIN_MEMORY_X86_READY;
    __atomic_store_n(&cached, features, __ATOMIC_RELAXED);
    return features;
#else
    return RIN_MEMORY_X86_READY;
#endif
}

static inline void rin_memory_copy_u64(unsigned char** destination,
                                       const unsigned char** source,
                                       rin_memory_size_t* size) {
    unsigned char* d = *destination;
    const unsigned char* s = *source;
    rin_memory_size_t n = *size;

    while (n >= 32u) {
        rin_memory_store_u64(d + 0u, rin_memory_load_u64(s + 0u));
        rin_memory_store_u64(d + 8u, rin_memory_load_u64(s + 8u));
        rin_memory_store_u64(d + 16u, rin_memory_load_u64(s + 16u));
        rin_memory_store_u64(d + 24u, rin_memory_load_u64(s + 24u));
        d += 32u;
        s += 32u;
        n -= 32u;
    }
    while (n >= 8u) {
        rin_memory_store_u64(d, rin_memory_load_u64(s));
        d += 8u;
        s += 8u;
        n -= 8u;
    }
    *destination = d;
    *source = s;
    *size = n;
}

static inline void* rin_memory_copy_fast(void* destination,
                                         const void* source,
                                         rin_memory_size_t size) {
    unsigned char* d = (unsigned char*)destination;
    const unsigned char* s = (const unsigned char*)source;

#if defined(__x86_64__)
    unsigned features = rin_memory_x86_features();
    if ((size >= 256u && (features & RIN_MEMORY_X86_ERMS) != 0u) ||
        (size >= 64u && (features & RIN_MEMORY_X86_FSRM) != 0u)) {
        __asm__ volatile("cld; rep movsb"
                         : "+D"(d), "+S"(s), "+c"(size)
                         :
                         : "memory", "cc");
        return destination;
    }
#if defined(RIN_USERSPACE)
    if (size >= 128u && (features & RIN_MEMORY_X86_AVX2) != 0u) {
        while (size >= 128u) {
            __asm__ volatile(
                "vmovdqu 0(%1), %%ymm0\n\t"
                "vmovdqu 32(%1), %%ymm1\n\t"
                "vmovdqu 64(%1), %%ymm2\n\t"
                "vmovdqu 96(%1), %%ymm3\n\t"
                "vmovdqu %%ymm0, 0(%0)\n\t"
                "vmovdqu %%ymm1, 32(%0)\n\t"
                "vmovdqu %%ymm2, 64(%0)\n\t"
                "vmovdqu %%ymm3, 96(%0)"
                :
                : "r"(d), "r"(s)
                : "ymm0", "ymm1", "ymm2", "ymm3", "memory");
            d += 128u;
            s += 128u;
            size -= 128u;
        }
        __asm__ volatile("vzeroupper" ::: "memory");
    }
    while (size >= 64u) {
        /* SSE2 is architectural on x86_64.  The userspace context switcher
         * saves XMM state, while this branch is never compiled into kernel
         * callers. */
        __asm__ volatile(
            "movdqu 0(%1), %%xmm0\n\t"
            "movdqu 16(%1), %%xmm1\n\t"
            "movdqu 32(%1), %%xmm2\n\t"
            "movdqu 48(%1), %%xmm3\n\t"
            "movdqu %%xmm0, 0(%0)\n\t"
            "movdqu %%xmm1, 16(%0)\n\t"
            "movdqu %%xmm2, 32(%0)\n\t"
            "movdqu %%xmm3, 48(%0)"
            :
            : "r"(d), "r"(s)
            : "xmm0", "xmm1", "xmm2", "xmm3", "memory");
        d += 64u;
        s += 64u;
        size -= 64u;
    }
#endif
#elif defined(__i386__)
    if (size >= 32u) {
        rin_memory_size_t words = size >> 2;
        rin_memory_size_t tail = size & 3u;
        __asm__ volatile("cld; rep movsl"
                         : "+D"(d), "+S"(s), "+c"(words)
                         :
                         : "memory", "cc");
        size = tail;
    }
#endif

#if defined(__x86_64__)
    rin_memory_copy_u64(&d, &s, &size);
#elif defined(__i386__)
    rin_memory_copy_u32(&d, &s, &size);
#endif
    while (size-- != 0u) *d++ = *s++;
    return destination;
}

static inline void* rin_memory_move_fast(void* destination,
                                         const void* source,
                                         rin_memory_size_t size) {
    unsigned char* d = (unsigned char*)destination;
    const unsigned char* s = (const unsigned char*)source;
    rin_memory_uintptr_t d_address = (rin_memory_uintptr_t)d;
    rin_memory_uintptr_t s_address = (rin_memory_uintptr_t)s;

    if (size == 0u || d == s) return destination;
    if (d_address < s_address || d_address - s_address >= size) {
        return rin_memory_copy_fast(destination, source, size);
    }

    d += size;
    s += size;
#if defined(__x86_64__)
    while (size >= 32u) {
        d -= 32u;
        s -= 32u;
        rin_memory_store_u64(d + 24u, rin_memory_load_u64(s + 24u));
        rin_memory_store_u64(d + 16u, rin_memory_load_u64(s + 16u));
        rin_memory_store_u64(d + 8u, rin_memory_load_u64(s + 8u));
        rin_memory_store_u64(d + 0u, rin_memory_load_u64(s + 0u));
        size -= 32u;
    }
    while (size >= 8u) {
        d -= 8u;
        s -= 8u;
        rin_memory_store_u64(d, rin_memory_load_u64(s));
        size -= 8u;
    }
#elif defined(__i386__)
    while (size >= 4u) {
        d -= 4u;
        s -= 4u;
        rin_memory_store_u32(d, rin_memory_load_u32(s));
        size -= 4u;
    }
#endif
    while (size-- != 0u) *--d = *--s;
    return destination;
}

static inline void* rin_memory_set_fast(void* destination, int value,
                                        rin_memory_size_t size) {
    unsigned char* d = (unsigned char*)destination;

#if defined(__x86_64__)
    if (size >= 64u) {
        unsigned features = rin_memory_x86_features();
        if ((features & (RIN_MEMORY_X86_ERMS | RIN_MEMORY_X86_FSRM)) != 0u) {
            __asm__ volatile("cld; rep stosb"
                             : "+D"(d), "+c"(size)
                             : "a"((unsigned char)value)
                             : "memory", "cc");
            return destination;
        }
    }
    {
        unsigned long long pattern = (unsigned char)value;
        pattern *= 0x0101010101010101ULL;
        while (size >= 32u) {
            rin_memory_store_u64(d + 0u, pattern);
            rin_memory_store_u64(d + 8u, pattern);
            rin_memory_store_u64(d + 16u, pattern);
            rin_memory_store_u64(d + 24u, pattern);
            d += 32u;
            size -= 32u;
        }
        while (size >= 8u) {
            rin_memory_store_u64(d, pattern);
            d += 8u;
            size -= 8u;
        }
    }
#elif defined(__i386__)
    if (size >= 32u) {
        unsigned word = (unsigned char)value;
        rin_memory_size_t words;
        rin_memory_size_t tail;
        word *= 0x01010101u;
        words = size >> 2;
        tail = size & 3u;
        __asm__ volatile("cld; rep stosl"
                         : "+D"(d), "+c"(words)
                         : "a"(word)
                         : "memory", "cc");
        size = tail;
    }
    while (size >= 4u) {
        unsigned word = (unsigned char)value;
        word *= 0x01010101u;
        rin_memory_store_u32(d, word);
        d += 4u;
        size -= 4u;
    }
#endif
    while (size-- != 0u) *d++ = (unsigned char)value;
    return destination;
}

#endif
