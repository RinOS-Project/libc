/* SPDX-License-Identifier: MIT */
/* Process-global narrow stdio owner for each user process runtime. */

#define RIN_STDIO_OWNER_BUILD 1
#define _STDIO_GLOBAL_IMPL 1
#include "stdio_impl.h"

uint32_t _rin_stdio_tmpnam_next_sequence(void) {
    static volatile uint32_t sequence;
    const uint32_t limit = (uint32_t)TMP_MAX;
    uint32_t current = __atomic_load_n(&sequence, __ATOMIC_RELAXED);
    for (;;) {
        if (current >= limit) return UINT32_MAX;
        if (__atomic_compare_exchange_n(&sequence, &current, current + 1u, 0,
                                        __ATOMIC_RELAXED,
                                        __ATOMIC_RELAXED))
            return current;
    }
}
