/* SPDX-License-Identifier: MIT */
/* Process-wide C rand/srand owner.  This deterministic generator is not
 * suitable for keys, nonces, tokens, or any other cryptographic purpose. */

#include "errno.h"

#define RIN_LIBC_RAND_MAX 0x7fffffffU

#ifdef __cplusplus
extern "C" {
#endif

static unsigned int rin_libc_rand_state = 1U;

static unsigned int rin_libc_rand_next_state(unsigned int state)
{
    /* xorshift32 has a full 2^32-1 cycle for every nonzero state. */
    if (state == 0U) state = 0x6D2B79F5U;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static unsigned int rin_libc_rand_output(unsigned int state)
{
    /* Avalanche high state bits into rand()'s exposed low bits. */
    state ^= state >> 16;
    state *= 0x7FEB352DU;
    state ^= state >> 15;
    state *= 0x846CA68BU;
    state ^= state >> 16;
    return state;
}

int rand(void)
{
    unsigned int observed = __atomic_load_n(
        &rin_libc_rand_state, __ATOMIC_RELAXED);
    unsigned int next;

    do {
        next = rin_libc_rand_next_state(observed);
    } while (!__atomic_compare_exchange_n(
        &rin_libc_rand_state, &observed, next, 1,
        __ATOMIC_RELAXED, __ATOMIC_RELAXED));
    return (int)(rin_libc_rand_output(next) & RIN_LIBC_RAND_MAX);
}

void srand(unsigned int seed)
{
    __atomic_store_n(&rin_libc_rand_state, seed, __ATOMIC_RELAXED);
}

int rand_r(unsigned int* seed)
{
    unsigned int next;
    if (!seed) {
        errno = EINVAL;
        return 0;
    }
    next = rin_libc_rand_next_state(*seed);
    *seed = next;
    return (int)(rin_libc_rand_output(next) & RIN_LIBC_RAND_MAX);
}

/* POSIX random(3) uses the same non-cryptographic process-wide generator
 * for this freestanding libc.  Keep the aliases in one state owner so code
 * that mixes rand() and random() observes deterministic, coherent progress. */
long random(void)
{
    return (long)rand();
}

void srandom(unsigned int seed)
{
    srand(seed);
}

#ifdef __cplusplus
}
#endif
